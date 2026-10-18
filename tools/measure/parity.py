"""Function parity between your copy of the original game and the rebuilt RPX.

    python tools/measure/parity.py --original <update dump>/code/Minecraft.Client.rpx
    python tools/measure/parity.py --original ORIGINAL.rpx --rebuilt build/Minecraft.Client.rpx \
        --map build/Minecraft.Client.map --report build/parity.tsv

Every function of the original executable is paired by name with the function of the rebuilt one and
the two are compared on facts a different compiler leaves alone:
  * the functions it calls;
  * how many calls it makes through a pointer (virtual calls, callbacks);
  * the floating-point constants it loads and what each is used for (an operand of which operation, a
    value stored, an argument, a comparison), with a constant that sits next to one the original loads
    counted as misread;
  * the 32-bit constants it builds;
  * the string literals it uses (other than those it hands to the standard library or an assert,
    which each library treats its own way);
  * the constant integer arguments of the calls both functions make, in the callee's argument
    registers;
  * how many conditional branches it has, leaving out the ones a compiler adds by itself (a
    constructor's allocation test, the test of what operator new returned, a switch's range check).
A function is clean when all of these agree. Code the two compilers bring themselves (the standard
library, boost, the compiler and C runtime) is not compared. Constructors, destructors and thunks are
reported apart, with the virtual function tables the constructors install, since their shape is set by
the C++ ABI as much as by the source.

The rebuilt RPX is compiled by a different compiler than the original, which inlines different
functions. A call the original makes that the rebuilt function lacks still counts as present when the
callee could have been inlined into it (defined in the same compile unit, or only inline in a header)
and the rebuilt body holds that callee's calls and constants; a call the rebuilt function makes that
the original inlined counts when the original body holds that callee's calls and constants, which then
count as the rebuilt function's. Everything is read from the two executables' symbols, relocations and
machine code, and from the rebuilt executable's link map.

A function of the original with no copy in the rebuilt executable is looked up in the source tree:
when the source defines it, the rebuilt compiler inlined it everywhere it is used, and it is reported as
inlined and not compared; otherwise it is missing.

This is a check of calls, constants and branch counts, not a proof of behaviour: field offsets, integer
arguments and the order of operations are not compared, so a clean function can still behave
differently. A function with none of these facts (a small accessor, for one) is reported as trivial and
left out of the percentages.
"""
import argparse
import collections
import csv
import heapq
import math
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ghsnames  # noqa: E402
import ppcelf  # noqa: E402
import sourceindex  # noqa: E402

ROOT = HERE.parents[1]
INSTRUMENTATION = {'PIXBeginNamedEvent', 'PIXEndNamedEvent', 'PIXSetMarker', 'MemSect'}
# Operations each compiler implements its own way: allocation and array helpers, block copies it may
# expand inline, and its arithmetic helpers.
RUNTIME = {'__nw', '__dl', '__nwa', '__dla', '__vec_new', '__vec_delete', '__ptmf_scall', '__construct_new_array',
           '__destroy_new_array', '__destroy_arr', '__construct_array', '__pure_virtual_called',
           '__register_global_object', '__throw', '__rethrow', '__end__catch', '__unexpected', '__terminate',
           '__dynamic_cast', '__ptr_to_member', '__cvt_fp2unsigned', '__div2i', '__div2u', '__mod2i', '__mod2u',
           '__shl2i', '__shr2i', '__shr2u', '__cvt_sll_flt', '__cvt_dbl_usll', '__cvt_ull_dbl', '__cvt_sll_dbl',
           '__cvt_dbl_ull', 'operator new', 'operator delete', 'operator new[]', 'operator delete[]',
           'memcpy', 'memmove', 'memset', 'memcmp', 'bzero', '__gh_memclr32', 'fabs', 'fabsf',
           '__ashldi3', '__ashrdi3', '__lshrdi3', '__divdi3', '__udivdi3', '__moddi3', '__umoddi3', '__muldi3',
           '__fixdfdi', '__fixsfdi', '__fixunsdfdi', '__fixunssfdi', '__floatdidf', '__floatdisf',
           '__floatundidf', '__floatundisf'}
MATH = {'sqrt', 'floor', 'ceil', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'atan2', 'pow', 'exp', 'log',
        'log10', 'fmod', 'sinh', 'cosh', 'tanh', 'round', 'trunc', 'modf', 'frexp', 'ldexp'}
NEW_OPERATORS = {'__nw__FUi', '__nwa__FUi', '_Znwj', '_Znaj'}
MAP_LINE = re.compile(r'^\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\d+ ( *)(\S.*)$')
LIBRARY_ARCHIVE = re.compile(r'(?:^|/)lib[\w+.-]*\.a\(')
ITANIUM_CTOR = re.compile(r'^_ZN.*C[123]E')
AREAS = [('Game', 'game/'), ('Wii U platform layer', 'platform/'), ('Mii faces (FFL)', 'nn/ffl/'),
         ('NEX online library', 'nn/nex/'), ('Pia online library', 'nn/pia/'), ('Other Nintendo SDK', 'nn/'),
         ('Sound and middleware', 'vendor/'), ('Cafe SDK runtime', 'cafe/')]
ABI_AREA = 'C++ ABI (ctors, dtors, thunks)'
THIN_SIZE = 128
THIN_BRANCHES = 8
INLINE_DEPTH = 3
ROLE_WINDOW = 16
STORES = (36, 37, 38, 39, 44, 45, 47, 52, 53, 54, 55)
OPTIONS = {
    'inline': True,           # inlining-aware call comparison
    'branch_tolerance': -1.0,  # rebuilt branch count within this fraction of the original's (negative: off)
    'roles': True,            # compare float constants by what they are used for
    'float': 'single',        # 'single': a float and a double of the same single value are one constant
    'float_tolerance': 0.0,   # relative difference under which two float constants are one
    'abi_apart': True,        # report constructors, destructors and thunks apart
    'extra_constants': True,  # a float constant only the rebuilt function loads makes it differ
    'strings': True,          # compare the string literals a function uses
    'arguments': True,        # compare the constant integer arguments of calls both functions make
}


# ---------------------------------------------------------------- reading one function

def s16(value):
    return value - 0x10000 if value & 0x8000 else value


class Function(object):
    """The comparable facts of one function's machine code."""
    __slots__ = ('name', 'size', 'callees', 'indirect', 'branches', 'generated', 'constants', 'neighbours',
                 'immediates', 'roles', 'vtables', 'strings', 'arguments')

    def __init__(self, name, size):
        self.name = name
        self.size = size
        self.callees = set()
        self.indirect = 0
        self.branches = 0
        self.generated = 0
        self.constants = set()
        self.neighbours = set()
        self.immediates = set()
        self.roles = set()
        self.vtables = []
        self.strings = set()
        self.arguments = set()

    @property
    def source_branches(self):
        return self.branches - self.generated


def float_value(raw):
    if raw is None:
        return None
    value = struct.unpack('>f' if len(raw) == 4 else '>d', raw)[0]
    return value if math.isfinite(value) else None


def plausible(value):
    """A constant worth comparing: zero is left out, since a compiler can make it without a load."""
    return value is not None and 1e-12 < abs(value) < 1e12


def key_of(value):
    """A constant as compared: by magnitude, since one compiler adds a negative constant where the
    other subtracts the positive one, and at single precision (the default) a constant one compiler
    keeps as a double and the other as a float is the same constant."""
    value = abs(value)
    if OPTIONS['float'] == 'single' and value < 3.4e38:
        return float(struct.unpack('>f', struct.pack('>f', value))[0])
    return value


def same_float(a, b):
    tolerance = OPTIONS['float_tolerance']
    if a == b:
        return True
    return tolerance > 0 and abs(a - b) <= tolerance * max(abs(a), abs(b))


def float_in(value, values):
    if value in values:
        return True
    return OPTIONS['float_tolerance'] > 0 and any(same_float(value, v) for v in values)


def float_role(words, i, reg):
    """What a loaded float register is used for first, following moves, negations and roundings: a
    multiplication, an addition or subtraction, a division (and on which side), a comparison, a store,
    a call argument, a selection or a conversion. None when that is not seen."""
    for k in range(i + 1, min(i + 1 + ROLE_WINDOW, len(words))):
        n = words[k]
        op = n >> 26
        d, a, b, c = (n >> 21) & 31, (n >> 16) & 31, (n >> 11) & 31, (n >> 6) & 31
        if op in (52, 53, 54, 55) and d == reg:
            return 'store'
        if op in (59, 63):
            xo5, xo10 = (n >> 1) & 0x1f, (n >> 1) & 0x3ff
            if op == 63 and xo10 in (0, 32):
                if reg in (a, b):
                    return 'compare'
                continue
            if op == 63 and xo10 in (72, 40, 12, 264, 136):
                if reg == b:
                    reg = d
                    continue
            elif op == 63 and xo10 in (14, 15):
                if reg == b:
                    return 'convert'
            elif xo5 == 18:
                if reg in (a, b):
                    return 'div.left' if reg == a else 'div.right'
            elif xo5 in (20, 21):
                if reg in (a, b):
                    return 'addsub'
            elif xo5 == 25:
                if reg in (a, c):
                    return 'mul'
            elif xo5 in (28, 29, 30, 31):
                if reg in (a, c):
                    return 'mul'
                if reg == b:
                    return 'addsub'
            elif xo5 == 23 and reg in (a, b, c):
                return 'select'
            if d == reg:
                return None
        if op in (16, 18, 19):
            if op == 18 and n & 1 or op == 19 and (n >> 1) & 0x3ff == 528 and n & 1:
                return 'argument' if 1 <= reg <= 8 else None
            return None
        if op in (48, 49, 50, 51) and d == reg:
            return None
    return None


# A string handed to the standard library becomes a string object: one library takes the literal's
# address, the other may copy its characters in as constants. Such strings are not compared.
STRING_SINK = re.compile(r'assert|basic_string|Q2_3std|^_ZN?K?S[tsab]|^_ZSt|^wcs|^str', re.I)
PRINTABLE = set(range(0x20, 0x7f)) | {0x09, 0x0a, 0x0d}


def string_at(data, offset):
    """The string literal at offset: a two-byte wide string (UTF-16 big-endian) or a narrow one, at
    least two characters long and printable; None otherwise."""
    if offset < 0 or offset >= len(data):
        return None
    wide = []
    k = offset
    while k + 1 < len(data) and len(wide) < 512:
        unit = (data[k] << 8) | data[k + 1]
        if unit == 0:
            break
        if unit >= 0x80 and unit < 0xa0 or unit < 0x20 and unit not in (0x09, 0x0a, 0x0d) or unit in (0xfffe, 0xffff):
            wide = None
            break
        wide.append(unit)
        k += 2
    if wide and len(wide) >= 2 and k + 1 < len(data) and all(u < 0x2000 for u in wide) \
            and 4 * sum(u < 0x80 for u in wide) >= 3 * len(wide):
        return 'L' + ''.join(chr(u) for u in wide)
    end = data.find(b'\0', offset, offset + 1024)
    if end - offset >= 2 and all(b in PRINTABLE for b in data[offset:end]):
        return data[offset:end].decode('ascii')
    return None


X_WRITES_RA = {28, 60, 124, 284, 316, 412, 444, 476, 24, 536, 792, 824, 26, 922, 954}
X_NO_WRITE = {0, 32, 151, 215, 407, 535, 599, 661, 663, 725, 727, 983, 918, 662, 438, 150, 4, 86, 54, 278, 246, 1014,
              470, 758, 982, 306, 370, 566, 598, 854, 512, 467, 144, 146, 210, 242}
X_STORE_UPDATE = {183, 247, 439, 695, 759}
X_LOAD_UPDATE = {55, 119, 311, 375}
X_FLOAT_LOAD_UPDATE = {567, 631}
VOLATILE = (0,) + tuple(range(3, 13))
MERGED_CANDIDATES = 4
BLOCK_VISITS = 32


def written_gprs(w):
    """The general registers an instruction writes."""
    op = w >> 26
    rd, ra = (w >> 21) & 31, (w >> 16) & 31
    if op in (7, 8, 12, 13, 14, 15, 32, 34, 40, 42):
        return (rd,)
    if op in (33, 35, 41, 43):
        return (rd, ra)
    if op in (20, 21, 23, 24, 25, 26, 27, 28, 29):
        return (ra,)
    if op in (37, 39, 45, 49, 51, 53, 55, 57, 61):
        return (ra,)
    if op == 46:
        return tuple(range(rd, 32))
    if op == 4:
        return (ra,) if (w >> 1) & 0x3f in (38, 39) else ()
    if op == 31:
        xo = (w >> 1) & 0x3ff
        if xo in X_WRITES_RA or xo in X_STORE_UPDATE or xo in X_FLOAT_LOAD_UPDATE:
            return (ra,)
        if xo in X_NO_WRITE:
            return ()
        if xo in X_LOAD_UPDATE:
            return (rd, ra)
        return (rd,)
    return ()


def is_call(w):
    """Whether an instruction is a call: a branch that sets the link register."""
    op = w >> 26
    return bool(w & 1) and (op in (16, 18) or op == 19 and (w >> 1) & 0x3ff in (16, 528))


def falls_through(w):
    """Whether the next instruction can run after this one without a branch to it."""
    op = w >> 26
    if w & 1:
        return True
    if op == 18:
        return False
    if op == 16 or op == 19 and (w >> 1) & 0x3ff in (16, 528):
        return (w >> 21) & 0x14 != 0x14
    return True


def local_branch(w, i, count, relocated):
    """The index of the instruction a direct branch at index i goes to within its function, or None
    for a call, a branch through a relocation or to an absolute address, and one that leaves."""
    op = w >> 26
    if relocated or w & 3 or op not in (16, 18):
        return None
    if op == 18:
        disp = w & 0x03fffffc
        if disp & 0x02000000:
            disp -= 0x04000000
    else:
        disp = w & 0xfffc
        if disp & 0x8000:
            disp -= 0x10000
    target = i + disp // 4
    return target if 0 <= target < count else None


def merged(states):
    """The register facts of several (addresses, code, high) states meeting at one instruction: a data
    register every state defines holds any of their addresses (up to MERGED_CANDIDATES of them), a
    code register only the function all of them agree on, and a register the high half of an address
    only when all of them agree on it."""
    addresses, code, high = states[0]
    rest = states[1:]
    joined = {}
    for reg, candidates in addresses.items():
        if all(reg in s[0] for s in rest):
            union = list(candidates)
            for s in rest:
                union.extend(c for c in s[0][reg] if c not in union)
            if len(union) <= MERGED_CANDIDATES:
                joined[reg] = tuple(sorted(union, key=lambda c: (c[0].index, c[1])))
    return (joined, {reg: v for reg, v in code.items() if all(s[1].get(reg) == v for s in rest)},
            {reg: v for reg, v in high.items() if all(s[2].get(reg) == v for s in rest)})


class Reader(object):
    """Reads Function facts from one executable."""

    def __init__(self, elf, neighbours):
        self.elf = elf
        self.want_neighbours = neighbours
        self.at = {}
        for symbol, section, offset in elf.functions():
            current = self.at.get((section.index, offset))
            if current is None or (current.bind == ppcelf.STB_LOCAL and symbol.bind != ppcelf.STB_LOCAL):
                self.at[(section.index, offset)] = symbol

    def function_at(self, section, offset):
        symbol = self.at.get((section.index, offset))
        return symbol.name if symbol is not None else None

    def data_address(self, symbol_index, addend):
        """(section, offset) of data a relocation names, or (None, None) for code."""
        target = self.elf.target(symbol_index, addend)
        if target is None:
            return None, None
        section, offset = target
        if section.executable or section.kind == ppcelf.SHT_NOBITS:
            return None, None
        return section, offset

    def add_constant(self, out, section, offset, width, words, i, reg):
        value = float_value(self.elf.read(section, offset, width))
        if plausible(value):
            key = key_of(value)
            out.constants.add(key)
            role = float_role(words, i, reg)
            if role is not None:
                out.roles.add((key, role))
        if self.want_neighbours:
            for delta in (-8, -4, 4, 8):
                for size in (4, 8):
                    near = float_value(self.elf.read(section, offset + delta, size))
                    if plausible(near):
                        out.neighbours.add(key_of(near))

    def vtable_slots(self, section, offset):
        """The functions of a virtual function table at (section, offset), or None when no table is
        there. Entries are 8 bytes, the function in the second word; the first entry holds the type
        information."""
        relocs = self.elf.relocs(section.index)
        first = relocs.get(offset + 4)
        if first is None:
            return None
        slots = []
        k = 1
        while True:
            if relocs.get(offset + 8 * k) is not None:
                break
            entry = relocs.get(offset + 8 * k + 4)
            if entry is None:
                break
            target = self.elf.target(entry[1], entry[2])
            if target is None or not target[0].executable:
                break
            slots.append(self.function_at(*target) or '?')
            k += 1
        return slots or None

    def read(self, symbol, section, offset):
        out = Function(symbol.name, symbol.size)
        data = self.elf.read(section, offset, symbol.size)
        if data is None:
            return out
        count = len(data) // 4
        words = struct.unpack('>%dI' % count, data[:count * 4])
        relocs = self.elf.relocs(section.index)
        constructor = symbol.name.startswith('__ct__') or ITANIUM_CTOR.match(symbol.name) is not None
        entries = self.block_entries(words, offset, relocs)
        addresses = {}
        code = {}
        high = {}
        ctr = None
        conditional = []
        new_calls = []
        switches = []
        first_compare_r3 = None
        pending = []
        known = {}
        for i, w in enumerate(words):
            here = offset + 4 * i
            if i in entries:
                addresses, code, high = (dict(part) for part in entries[i])
            op = w >> 26
            rd, ra = (w >> 21) & 31, (w >> 16) & 31
            reloc = relocs.get(here + 2) if op != 18 else relocs.get(here)
            if first_compare_r3 is None and i < 12 and (op in (10, 11) and ra == 3
                                                         or op == 31 and (w >> 1) & 0x3ff == 444 and w & 1
                                                         and rd == 3):
                first_compare_r3 = i
            if op == 31 and (w >> 1) & 0x3ff == 467 and (w >> 11) & 0x3ff == 0x120:
                ctr = code.get(rd)
            if op == 31 and (w >> 1) & 0x3ff == 444 and rd == ((w >> 11) & 31) and rd in known:
                known[ra] = known[rd]
            elif op == 14 and ra == 0 and reloc is None:
                known[rd] = s16(w & 0xffff) & 0xffffffff
            elif op == 24 and reloc is None and rd in known and rd == ra:
                known[ra] = known[rd] | (w & 0xffff)
            elif op == 15 and ra == 0 and reloc is None:
                known[rd] = (w & 0xffff) << 16
            else:
                for reg in written_gprs(w):
                    known.pop(reg, None)
            if is_call(w):
                target = None
                if op == 18:
                    target = self.call_target(w, here, section, relocs)
                elif op == 19 and (w >> 1) & 0x3ff == 528 and (rd & 0x14) == 0x14 and ctr is not None:
                    target = self.function_at(*ctr)
                if target:
                    for reg in range(3, 11):
                        if reg in known:
                            out.arguments.add((target, reg - 3, known[reg]))
                for reg in VOLATILE:
                    known.pop(reg, None)
                if op != 19 or (w >> 1) & 0x3ff != 528:
                    ctr = None
            if op == 18:
                name = self.branch(out, w, here, offset, symbol.size, section, relocs)
                if name in NEW_OPERATORS and w & 1:
                    new_calls.append(i)
                if name is not None:
                    if not STRING_SINK.search(name):
                        out.strings.update(pending)
                    pending = []
            elif op == 16:
                if (rd & 0x14) != 0x14:
                    out.branches += 1
                    conditional.append(i)
            elif op == 19:
                xo = (w >> 1) & 0x3ff
                if xo in (16, 528) and (rd & 0x14) != 0x14:
                    out.branches += 1
                    conditional.append(i)
                if xo == 528 and (rd & 0x14) == 0x14:
                    if ctr is not None:
                        name = self.function_at(*ctr)
                        if name:
                            out.callees.add(name)
                            if name in NEW_OPERATORS and w & 1:
                                new_calls.append(i)
                            if not STRING_SINK.search(name):
                                out.strings.update(pending)
                            pending = []
                    elif not w & 1 and self.switch(words, i, section, offset, symbol.size, relocs):
                        switches.append(i)
                    else:
                        out.indirect += 1
                    ctr = None
            elif op in (48, 49, 50, 51):
                width = 4 if op < 50 else 8
                if reloc is not None and reloc[0] == ppcelf.R_PPC_ADDR16_LO:
                    section_, at = self.data_address(reloc[1], reloc[2])
                    if section_ is not None and self.based(op, ra, reloc, high):
                        self.add_constant(out, section_, at, width, words, i, rd)
                elif reloc is None and ra in addresses:
                    for section_, at in addresses[ra]:
                        self.add_constant(out, section_, at + s16(w & 0xffff), width, words, i, rd)
            elif op == 36 and rd in addresses:
                for section_, at in addresses[rd]:
                    slots = self.vtable_slots(section_, at)
                    if slots is not None:
                        out.vtables.append(slots)
            elif op == 15 and ra == 0 and reloc is None:
                value = self.built(words, i, relocs, offset)
                if value >= 0x10000:
                    out.immediates.add(value)
            formed = self.transfer(w, here, relocs, addresses, code, high)
            for section_, at in addresses.get(formed, ()):
                text = string_at(section_.data, at)
                if text is not None:
                    pending.append(text)
        out.strings.update(pending)
        out.generated = self.generated(conditional, new_calls, switches, constructor, first_compare_r3)
        return out

    def high_half(self, reloc):
        """The high half (as addis adds it) of the address a relocation names, or None."""
        target = self.elf.target(reloc[1], reloc[2])
        if target is None:
            return None
        address = target[0].address + target[1]
        return ((address + 0x8000) >> 16) & 0xffff

    def based(self, op, ra, reloc, high):
        """Whether an instruction with a low-half relocation forms that address itself: its base is
        zero (li, a load from an absolute address) or a register holding the address's high half
        from a lis. An addis that adds a high half to an index makes a table address instead."""
        if ra == 0 and op not in (12, 13):
            return True
        return ra in high and high[ra] == self.high_half(reloc)

    def transfer(self, w, here, relocs, addresses, code, high):
        """Applies one instruction to the registers known to hold the address of data (addresses: each
        register's candidate (section, offset) pairs), of a function (code) or the high half of an
        address from a lis (high). Returns the register an instruction that forms a data address
        (li, addi or addic with a relocation, addi to a known address) set, or None."""
        op = w >> 26
        rd, ra = (w >> 21) & 31, (w >> 16) & 31
        reloc = relocs.get(here + 2) if op != 18 else relocs.get(here)
        defined = formed = raised = None
        if op == 15 and ra == 0 and reloc is not None and reloc[0] == ppcelf.R_PPC_ADDR16_HA:
            half = self.high_half(reloc)
            if half is not None:
                high[rd] = half
                raised = rd
        elif op in (12, 13, 14) and reloc is not None and reloc[0] == ppcelf.R_PPC_ADDR16_LO \
                and self.based(op, ra, reloc, high):
            section_, at = self.data_address(reloc[1], reloc[2])
            target = self.elf.target(reloc[1], reloc[2])
            addresses.pop(rd, None)
            code.pop(rd, None)
            defined = rd
            if section_ is not None:
                addresses[rd] = ((section_, at),)
                formed = rd
            elif target is not None and target[0].executable:
                code[rd] = target
        elif op == 15 and ra != 0 and reloc is not None and reloc[0] == ppcelf.R_PPC_ADDR16_HA:
            section_, at = self.data_address(reloc[1], reloc[2])
            target = self.elf.target(reloc[1], reloc[2])
            if section_ is not None and (section_, at) in addresses.get(ra, ()):
                code.pop(rd, None)
                addresses[rd] = ((section_, at),)
                defined = rd
            elif target is not None and code.get(ra) == target:
                addresses.pop(rd, None)
                code[rd] = target
                defined = rd
        elif op == 14 and reloc is None and ra != 0 and ra in addresses:
            code.pop(rd, None)
            addresses[rd] = tuple((section_, at + s16(w & 0xffff)) for section_, at in addresses[ra])
            defined = formed = rd
        elif op == 31 and (w >> 1) & 0x3ff == 444 and rd == (w >> 11) & 31:
            if rd in addresses:
                code.pop(ra, None)
                addresses[ra] = addresses[rd]
                defined = ra
            elif rd in code:
                addresses.pop(ra, None)
                code[ra] = code[rd]
                defined = ra
            if rd in high:
                high[ra] = high[rd]
                raised = ra
        if is_call(w):
            for reg in VOLATILE:
                addresses.pop(reg, None)
                code.pop(reg, None)
                high.pop(reg, None)
        for reg in written_gprs(w):
            if reg != defined:
                addresses.pop(reg, None)
                code.pop(reg, None)
            if reg != raised:
                high.pop(reg, None)
        return formed

    def block_entries(self, words, offset, relocs):
        """The (addresses, code, high) state at the start of each basic block of a function, found by
        applying transfer along its direct branches and fall-throughs until nothing changes. Where
        several paths meet, a register keeps what every one of them agrees it may hold. A block that no
        direct branch or fall-through reaches (the target of a jump through a table) starts with the
        state of the block before it."""
        count = len(words)
        jumps = {}
        leaders = {0}
        for i, w in enumerate(words):
            target = local_branch(w, i, count, relocs.get(offset + 4 * i) is not None)
            if target is not None:
                jumps[i] = target
                leaders.add(target)
            if w >> 26 in (16, 18, 19) and not is_call(w) and i + 1 < count:
                leaders.add(i + 1)
        starts = sorted(leaders)
        ends = dict(zip(starts, starts[1:] + [count]))
        preds = collections.defaultdict(list)
        for start in starts:
            last = ends[start] - 1
            if last in jumps:
                preds[jumps[last]].append(start)
            if ends[start] < count and falls_through(words[last]):
                preds[ends[start]].append(start)
        for before, start in zip(starts, starts[1:]):
            if not preds[start]:
                preds[start].append(before)
        succs = collections.defaultdict(list)
        for start, sources in preds.items():
            for source in sources:
                succs[source].append(start)
        entries = {}
        exits = {}
        work = [0]
        queued = {0}
        visits = BLOCK_VISITS * len(starts)
        while work and visits > 0:
            visits -= 1
            start = heapq.heappop(work)
            queued.discard(start)
            states = [exits[p] for p in preds[start] if p in exits]
            if start == 0:
                states.append(({}, {}, {}))
            entry = merged(states)
            if start in exits and entries.get(start) == entry:
                continue
            entries[start] = entry
            addresses, code, high = (dict(part) for part in entry)
            for k in range(start, ends[start]):
                self.transfer(words[k], offset + 4 * k, relocs, addresses, code, high)
            if exits.get(start) != (addresses, code, high):
                exits[start] = (addresses, code, high)
                for succ in succs[start]:
                    if succ not in queued:
                        queued.add(succ)
                        heapq.heappush(work, succ)
        return entries

    @staticmethod
    def generated(conditional, new_calls, switches, constructor, first_compare_r3):
        """Conditional branches a compiler adds by itself: a constructor's test of a null this, the test
        of what operator new returned, a switch's range check."""
        count = 0
        for b in conditional:
            if constructor and first_compare_r3 is not None and b - first_compare_r3 <= 3 and b < 16:
                count += 1
            elif any(0 < b - n <= 4 for n in new_calls):
                count += 1
            elif any(0 < s - b <= 8 for s in switches):
                count += 1
        return count

    @staticmethod
    def built(words, i, relocs, offset):
        """The 32-bit value a lis builds with the ori/addi that completes it, if one does."""
        w = words[i]
        reg = (w >> 21) & 31
        value = (w & 0xffff) << 16
        for k in range(i + 1, min(i + 9, len(words))):
            n = words[k]
            op, rd, ra = n >> 26, (n >> 21) & 31, (n >> 16) & 31
            if relocs.get(offset + 4 * k + 2) is not None:
                if rd == reg and op not in STORES:
                    break
                continue
            if op == 24 and rd == reg:
                return (value | (n & 0xffff)) & 0xffffffff
            if op == 14 and ra == reg:
                return (value + s16(n & 0xffff)) & 0xffffffff
            if op in (16, 18, 19):
                break
            if reg in written_gprs(n):
                break
        return value

    def switch(self, words, i, section, offset, size, relocs):
        """Whether a bctr dispatches a jump table: the count register comes from a table load, or
        is an address inside the function itself (a table of branches an indexed addis and its addi
        select)."""
        for k in range(i - 1, max(-1, i - 9), -1):
            w = words[k]
            if w >> 26 == 31 and (w >> 1) & 0x3ff == 467 and ((w >> 11) & 0x3ff) == 0x120:
                reg = (w >> 21) & 31
                for j in range(k - 1, max(-1, k - 7), -1):
                    v = words[j]
                    if v >> 26 == 31 and (v >> 1) & 0x3ff in (23, 266) and (v >> 21) & 31 == reg:
                        return True
                    if v >> 26 == 14 and (v >> 21) & 31 == reg:
                        reloc = relocs.get(offset + 4 * j + 2)
                        if reloc is None or reloc[0] != ppcelf.R_PPC_ADDR16_LO:
                            return False
                        target = self.elf.target(reloc[1], reloc[2])
                        return target is not None and target[0].index == section.index \
                            and offset <= target[1] < offset + size
                return False
        return False

    def call_target(self, w, here, section, relocs):
        """The name of the function a bl calls, or None."""
        reloc = relocs.get(here)
        if reloc is not None and reloc[0] == ppcelf.R_PPC_REL24:
            symbol = self.elf.symbols[reloc[1]] if reloc[1] < len(self.elf.symbols) else None
            if symbol is not None and symbol.kind == ppcelf.STT_SECTION:
                target = self.elf.target(reloc[1], reloc[2])
                return self.function_at(*target) if target else None
            return symbol.name if symbol is not None else None
        if w & 2:
            return None
        disp = w & 0x03fffffc
        if disp & 0x02000000:
            disp -= 0x04000000
        return self.function_at(section, here + disp)

    def branch(self, out, w, here, start, size, section, relocs):
        """Records the function a b/bl reaches and returns its name."""
        reloc = relocs.get(here)
        name = None
        if reloc is not None and reloc[0] == ppcelf.R_PPC_REL24:
            symbol = self.elf.symbols[reloc[1]] if reloc[1] < len(self.elf.symbols) else None
            if symbol is not None and symbol.kind == ppcelf.STT_SECTION:
                target = self.elf.target(reloc[1], reloc[2])
                name = self.function_at(*target) if target else None
            elif symbol is not None:
                name = symbol.name
        else:
            if w & 2:
                return None
            disp = w & 0x03fffffc
            if disp & 0x02000000:
                disp -= 0x04000000
            dest = here + disp
            if start <= dest < start + size and not (w & 1):
                return None
            name = self.function_at(section, dest)
        if name:
            out.callees.add(name)
        return name


def read_all(elf, neighbours):
    reader = Reader(elf, neighbours)
    functions = {}
    for symbol, section, offset in elf.functions():
        if symbol.name not in functions or symbol.bind != ppcelf.STB_LOCAL:
            functions[symbol.name] = reader.read(symbol, section, offset)
    return functions


# ---------------------------------------------------------------- the rebuilt executable's units

def read_map(path):
    """{symbol name: input file} from an lld map file."""
    units = {}
    current = None
    with open(path, encoding='utf-8', errors='replace') as handle:
        for line in handle:
            m = MAP_LINE.match(line)
            if not m:
                continue
            depth, text = len(m.group(3)), m.group(4)
            if depth >= 16:
                if current is not None:
                    units.setdefault(text.strip(), current)
            elif depth >= 8:
                current = text.split(':(')[0]
    return units


def area_of(unit):
    if unit is None:
        return 'No counterpart'
    rel = unit.replace('\\', '/')
    if '/obj/' in rel:
        rel = rel.split('/obj/', 1)[1]
    for name, prefix in AREAS:
        if rel.startswith(prefix):
            return name
    return 'Other'


def abi_function(name):
    """Constructors, destructors and thunks: code whose shape the C++ ABI sets as much as the source."""
    if name.startswith('__ghs_thunk__'):
        return True
    key = ghsnames.ghs_key(name)
    return key is not None and (key[1] in ('{ctor}', '{dtor}') or key[1].startswith('__ghs_thunk__'))


# ---------------------------------------------------------------- comparison

def near_float(a, b):
    if a == b or a == 0 or b == 0:
        return False
    return abs(a - b) <= 1e-6 * max(abs(a), abs(b)) * 16


def argument_registers(name):
    """The argument registers (0 = r3) that carry a function's integer parameters, from its Green
    Hills name: `this` first for a member, nothing for a float, an aligned pair for a 64-bit integer.
    None when the parameters are not known."""
    scope, method, sig = ghsnames.demangle(name)
    if scope is None:
        key = ghsnames.ghs_key(name)
        if key is None or key[2] is None:
            return None
        params, member = key[2], False
    else:
        if not sig.startswith('('):
            return None
        params = [ghsnames.norm_type(p) for p in ghsnames.split_top(sig[1:sig.rfind(')')], ',') if p.strip()]
        member = '[static]' not in sig
    slots = []
    reg = 1 if member else 0
    if member:
        slots.append(0)
    for param in params:
        if param in ('float', 'double', 'constfloat', 'constdouble'):
            continue
        if param in ('longlong', 'unsignedlonglong', 'constlonglong', 'constunsignedlonglong'):
            reg += reg % 2
            slots.extend((reg, reg + 1))
            reg += 2
        elif param == '...':
            return None
        else:
            slots.append(reg)
            reg += 1
    return set(s for s in slots if s < 8)


def base_name(name):
    key = ghsnames.ghs_key(name)
    return key[1] if key is not None and key[0] is None else name


def callee_set(callees, library, rename=None):
    """The calls worth comparing, in original names: instrumentation, library code and compiler
    runtime operations left out, and a float math function named as its double version."""
    out = set()
    for callee in callees:
        name = (rename(callee) or callee) if rename else callee
        base = base_name(name)
        if name in INSTRUMENTATION or base in RUNTIME or library(callee):
            continue
        if base.endswith('f') and base[:-1] in MATH:
            name = base[:-1]
        out.add(name)
    return out


class Facts(object):
    """The facts of one function, grown by the functions folded into it."""

    def __init__(self, function, callees):
        self.callees = set(callees)
        self.constants = set(function.constants)
        self.immediates = set(function.immediates)
        self.roles = set(function.roles)
        self.strings = set(function.strings)
        self.branches = function.source_branches
        self.indirect = function.indirect

    def add(self, function, callees):
        self.callees |= callees
        self.constants |= function.constants
        self.immediates |= function.immediates
        self.roles |= function.roles
        self.strings |= function.strings
        self.branches += function.source_branches
        self.indirect += function.indirect


def contains(outer_callees, outer, inner_callees, inner):
    """Whether a function's facts include another's: what a body that inlined it shows."""
    return inner_callees <= outer_callees and inner.constants <= outer.constants and inner.immediates <= outer.immediates


def library_counts(function, rebuilt, library, depth=0):
    """(indirect calls, branches) of the library functions a rebuilt function calls, which the
    original may have inlined: a shared_ptr release, a container's growth."""
    indirect = branches = 0
    if depth >= INLINE_DEPTH:
        return 0, 0
    for callee in function.callees:
        if not library(callee):
            continue
        inner = rebuilt.get(callee)
        if inner is None:
            continue
        deeper = library_counts(inner, rebuilt, library, depth + 1)
        indirect += inner.indirect + deeper[0]
        branches += inner.source_branches + deeper[1]
    return indirect, branches


def comparable(function, callees):
    """Whether a function has any fact to compare."""
    return bool(callees or function.constants or function.immediates or function.indirect or function.strings
                or function.branches >= THIN_BRANCHES)


class Comparison(object):
    """The two executables read and paired."""

    def __init__(self, original_path, rebuilt_path, map_path, progress=None):
        say = progress or (lambda text: None)
        say('reading the original ...')
        self.original_elf = ppcelf.load(original_path)
        self.original = read_all(self.original_elf, neighbours=True)
        say('reading the rebuilt executable ...')
        self.rebuilt_elf = ppcelf.load(rebuilt_path)
        self.rebuilt = read_all(self.rebuilt_elf, neighbours=False)
        self.units = read_map(map_path)
        self.names = ghsnames.NameMap(self.original)
        self.names.prime(set(self.rebuilt) | {c for f in self.rebuilt.values() for c in f.callees})
        self.pairs = {}
        self.strong_units = collections.defaultdict(set)
        self.provided = set()
        weak = {s.name for s in self.rebuilt_elf.symbols if s.kind == ppcelf.STT_FUNC and s.bind == ppcelf.STB_WEAK}
        for name in self.rebuilt:
            mapped = self.names(name)
            unit = self.units.get(name)
            if mapped is None:
                continue
            if unit is not None and LIBRARY_ARCHIVE.search(unit.replace('\\', '/')):
                self.provided.add(mapped)
                continue
            if ghsnames.CLONE.search(name):
                continue
            self.pairs.setdefault(mapped, name)
            if name not in weak:
                self.strong_units[mapped].add(unit)
        self.original_callees = {n: callee_set(f.callees, ghsnames.ghs_library) for n, f in self.original.items()}
        self.argument_slots = {}

    def in_scope(self, name):
        return not ghsnames.ghs_library(name) and name not in self.provided

    def inlined_in(self, outer_callees, outer, callee, depth=0):
        """Whether the original body holds callee's facts as if it had inlined it, with callee's own
        callees either called by that body or inlined into it in turn."""
        theirs = self.original.get(callee)
        if theirs is None:
            return True
        if not (theirs.constants <= outer.constants and theirs.immediates <= outer.immediates
                and theirs.strings <= outer.strings):
            return False
        for inner in self.original_callees.get(callee, ()):
            if inner in outer_callees or inner == callee:
                continue
            if depth + 1 >= INLINE_DEPTH or not self.inlined_in(outer_callees, outer, inner, depth + 1):
                return False
        return True

    def compare(self, name, ours_name=None):
        """(clean, reasons) for an original function against a rebuilt function (its pair by default)."""
        orig = self.original[name]
        ours_name = ours_name or self.pairs[name]
        ours = self.rebuilt[ours_name]
        names = self.names
        orig_callees = self.original_callees[name]
        facts = Facts(ours, ())
        extra = []
        pending = [(callee, 0) for callee in ours.callees]
        expanded = set()
        while pending:
            callee, depth = pending.pop()
            mapped = callee_set([callee], names.library, names)
            if not mapped:
                continue
            callee_name = next(iter(mapped))
            if depth == 0:
                facts.callees.add(callee_name)
            if callee_name in orig_callees or not OPTIONS['inline'] or callee in expanded:
                if depth > 0:
                    facts.callees.add(callee_name)
                continue
            if not self.inlined_in(orig_callees, orig, callee_name):
                if depth == 0:
                    extra.append(callee_name)
                continue
            expanded.add(callee)
            inner = self.rebuilt.get(callee)
            if inner is not None:
                facts.add(inner, set())
                if depth + 1 < INLINE_DEPTH:
                    pending.extend((c, depth + 1) for c in inner.callees)
        if not OPTIONS['inline']:
            extra = sorted(facts.callees - orig_callees)
        library_indirect, library_branches = library_counts(ours, self.rebuilt, names.library)
        our_unit = self.units.get(ours_name)

        def inlinable(callee):
            units = self.strong_units.get(callee)
            return not units or our_unit in units

        def present(callee, depth):
            if callee in facts.callees:
                return True
            theirs = self.original.get(callee)
            if not OPTIONS['inline'] or theirs is None or depth >= INLINE_DEPTH or not inlinable(callee):
                return False
            if not (all(float_in(v, facts.constants) for v in theirs.constants)
                    and theirs.immediates <= facts.immediates):
                return False
            return all(present(c, depth + 1) for c in self.original_callees.get(callee, ()) if c != callee)

        reasons = []
        missing = sorted(c for c in orig_callees if not present(c, 0))
        if missing:
            reasons.append('missing calls: ' + '; '.join(missing[:6]) + (' ...' if len(missing) > 6 else ''))
        if extra:
            reasons.append('extra calls: ' + '; '.join(sorted(extra)[:6]) + (' ...' if len(extra) > 6 else ''))
        unmatched = sorted(v for v in facts.constants if not float_in(v, orig.constants))
        lost = sorted(v for v in orig.constants if not float_in(v, facts.constants))
        misreads = sorted({v for v in unmatched if v in orig.neighbours or any(near_float(v, w) for w in lost)})
        if misreads:
            reasons.append('misread constants: ' + ', '.join('%g' % v for v in misreads[:6]))
        if lost:
            reasons.append('constants not loaded: ' + ', '.join('%g' % v for v in lost[:6]))
        others = [v for v in unmatched if v not in misreads]
        if others and OPTIONS['extra_constants']:
            reasons.append('other constants: ' + ', '.join('%g' % v for v in others[:6]))
        if OPTIONS['roles'] and not lost:
            ours_roles = {(v, role) for v, role in facts.roles}
            wrong = sorted((v, role) for v, role in orig.roles
                           if not any(role == r and same_float(v, w) for w, r in ours_roles))
            if wrong:
                reasons.append('constants used differently: ' + ', '.join('%g as %s' % x for x in wrong[:6]))
        gap = orig.indirect - facts.indirect - library_indirect
        if gap >= 3 and gap * 3 >= orig.indirect:
            reasons.append('indirect calls %d of %d' % (facts.indirect + library_indirect, orig.indirect))
        if OPTIONS['strings']:
            lost_strings = sorted(orig.strings - facts.strings)
            if lost_strings:
                reasons.append('strings not used: ' + ', '.join(repr(t)[:40] for t in lost_strings[:4]))
        if OPTIONS['arguments']:
            ours_arguments = set()
            for callee, index, value in ours.arguments:
                mapped = callee_set([callee], names.library, names)
                if mapped:
                    ours_arguments.add((next(iter(mapped)), index, value))
            both = orig_callees & facts.callees
            wrong_arguments = []
            for callee, index, value in sorted(orig.arguments):
                if callee not in both or (callee, index, value) in ours_arguments:
                    continue
                registers = self.argument_slots.get(callee)
                if registers is None:
                    registers = self.argument_slots[callee] = argument_registers(callee) or set()
                key = ghsnames.ghs_key(callee)
                if index not in registers or index == 0 and key is not None and key[1] == '{ctor}':
                    continue
                wrong_arguments.append((callee, index, value))
            if wrong_arguments:
                reasons.append('call arguments: ' + ', '.join('%s#%d=%d' % (base_name(c)[:30], i, v)
                                                              for c, i, v in wrong_arguments[:4]))
        absent = sorted(orig.immediates - facts.immediates)
        if absent:
            reasons.append('missing immediates: ' + ', '.join('0x%08x' % v for v in absent[:6]))
        theirs_branches = orig.source_branches
        facts.branches += library_branches
        tolerance = OPTIONS['branch_tolerance']
        if tolerance >= 0 and abs(facts.branches - theirs_branches) > max(2, tolerance * theirs_branches):
            reasons.append('branches %d of %d' % (facts.branches, theirs_branches))
        elif orig.size >= THIN_SIZE and orig.branches >= THIN_BRANCHES and facts.branches * 3 < theirs_branches:
            reasons.append('thin: %d branches of %d' % (facts.branches, theirs_branches))
        return not reasons, reasons

    def slot_name(self, name, rebuilt):
        """A table slot as compared: a thunk by the function it adjusts for (its offset follows the
        class layout, which the two builds' different standard libraries change), a rebuilt name in
        its original spelling."""
        if rebuilt:
            thunk = ghsnames.THUNK.match(name)
            if thunk:
                target = '_Z' + name[thunk.end():]
                self.names.prime([target])
                return 'thunk to ' + (self.names(target) or target)
            return self.names(name) or name
        m = re.match(r'^__ghs_thunk__0x[0-9a-f]+__(.*)$', name)
        return 'thunk to ' + m.group(1) if m else name

    @staticmethod
    def table_class(slots):
        """The class a table belongs to, from its destructor slot, and whether it is a secondary
        table (its destructor reached through a thunk); None for a table without a destructor."""
        for slot in slots:
            thunk = slot.startswith('thunk to ')
            key = ghsnames.ghs_key(slot[len('thunk to '):] if thunk else slot)
            if key is not None and key[1] == '{dtor}':
                return key[0], thunk
        return None

    def vtables(self):
        """[(original constructor, original slots, best rebuilt slots)] for every virtual function
        table the original's constructors install. A table is matched to the rebuilt table of the
        same class (the class of its destructor slot) that shares the most slots; a table without a
        destructor to the best table its rebuilt constructor installs."""
        rebuilt_by_class = collections.defaultdict(list)
        rebuilt_seen = set()
        for name, function in self.rebuilt.items():
            for table in function.vtables:
                slots = tuple(self.slot_name(slot, True) for slot in table)
                if slots not in rebuilt_seen:
                    rebuilt_seen.add(slots)
                    rebuilt_by_class[self.table_class(slots)].append(list(slots))
        seen = set()
        out = []
        for name, orig in sorted(self.original.items()):
            key = ghsnames.ghs_key(name)
            if not orig.vtables or key is None or key[1] != '{ctor}' or name not in self.pairs \
                    or not self.in_scope(name):
                continue
            ours = self.rebuilt[self.pairs[name]]
            for table in orig.vtables:
                slots = [self.slot_name(slot, False) for slot in table]
                if tuple(slots) in seen:
                    continue
                seen.add(tuple(slots))
                owner = self.table_class(slots)
                if owner is not None:
                    candidates = rebuilt_by_class.get(owner, [])
                else:
                    candidates = [[self.slot_name(slot, True) for slot in t] for t in ours.vtables]
                best = max(candidates, default=[],
                           key=lambda t: (len(set(t) & set(slots)), sum(a == b for a, b in zip(t, slots))))
                out.append((name, slots, best))
        return out

    def vtable_summary(self):
        """(identical, differing, not installed, slots matching, slots compared). A base class table the
        rebuilt executable never installs (its compiler drops a store the derived constructor
        overwrites) is not compared: no rebuilt table of that class, or only a derived one."""
        identical = differing = absent = slot_match = slot_total = 0
        for _, slots, best in self.vtables():
            if not best or len(best) > len(slots) and self.table_class(slots) is not None:
                absent += 1
                continue
            slot_total += len(slots)
            slot_match += sum(a == b for a, b in zip(slots, best))
            if best == slots:
                identical += 1
            else:
                differing += 1
        return identical, differing, absent, slot_match, slot_total


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--original', type=Path, required=True,
                        help="your update 688 dump's code/Minecraft.Client.rpx")
    parser.add_argument('--rebuilt', type=Path, default=ROOT / 'build/Minecraft.Client.rpx')
    parser.add_argument('--map', type=Path, help='the link map (default: next to --rebuilt)')
    parser.add_argument('--report', type=Path, help='write one row per function to this TSV file')
    parser.add_argument('--source', type=Path, default=ROOT / 'source',
                        help='the source tree, to tell an inlined function from a missing one')
    parser.add_argument('--float', choices=('single', 'double'), default=OPTIONS['float'],
                        help='compare float constants at single precision (default) or as stored')
    parser.add_argument('--float-tolerance', type=float, default=OPTIONS['float_tolerance'],
                        help='relative difference under which two float constants are one (default exact)')
    parser.add_argument('--branch-tolerance', type=float, default=OPTIONS['branch_tolerance'],
                        help='allowed relative difference in branch count (negative: only flag thin bodies)')
    parser.add_argument('--no-roles', action='store_true', help='compare float constants by value only')
    parser.add_argument('--no-inline', action='store_true', help='compare calls without inlining awareness')
    parser.add_argument('--abi-together', action='store_true',
                        help='count constructors, destructors and thunks with the other functions')
    args = parser.parse_args()
    OPTIONS.update({'float': args.float, 'float_tolerance': args.float_tolerance,
                    'branch_tolerance': args.branch_tolerance, 'roles': not args.no_roles,
                    'inline': not args.no_inline, 'abi_apart': not args.abi_together})
    map_path = args.map or args.rebuilt.with_suffix('.map')
    for path in (args.original, args.rebuilt, map_path):
        if not path.is_file():
            sys.exit('not found: %s' % path)
    comparison = Comparison(args.original, args.rebuilt, map_path, progress=lambda t: print(t, flush=True))
    print('indexing the source ...', flush=True)
    index = sourceindex.SourceIndex(args.source) if args.source.is_dir() else None

    counts = collections.Counter()
    by_area = collections.defaultdict(collections.Counter)
    rows = []
    for name in sorted(comparison.original):
        if not comparison.in_scope(name):
            continue
        orig = comparison.original[name]
        ours_name = comparison.pairs.get(name)
        abi = OPTIONS['abi_apart'] and abi_function(name)
        if ours_name is None:
            key = ghsnames.ghs_key(name)
            if key is None:
                continue
            if index is not None and index.defines(key[0], key[1]):
                state, area = 'inlined', 'Inlined (not compared)'
                reasons = ['no copy in the rebuilt executable; the source defines it, so it was inlined']
            else:
                state, area = 'missing', 'Missing'
                reasons = ['not in the rebuilt executable and not defined in the source']
        else:
            clean, reasons = comparison.compare(name)
            state = 'differs'
            if clean:
                state = 'clean' if comparable(orig, comparison.original_callees[name]) else 'trivial'
            area = area_of(comparison.units.get(ours_name))
        if abi:
            area = ABI_AREA
        else:
            counts[state] += 1
        by_area[area][state] += 1
        rows.append((name, ours_name or '', area, state, orig.size, ' | '.join(reasons)))

    total = sum(counts.values())
    judged = total - counts['trivial'] - counts['inlined']
    print()
    print('Functions of the original: %d; compared: %d' % (total, judged))
    for state in ('clean', 'differs', 'missing'):
        print('  %-8s %6d  %5.1f%%' % (state, counts[state], 100.0 * counts[state] / max(judged, 1)))
    print('  inlined  %6d  (no separate copy in the rebuilt executable; not in the percentages)' % counts['inlined'])
    print('  trivial  %6d  (nothing to compare; not in the percentages)' % counts['trivial'])
    print()
    print('%-32s %8s %8s %8s %8s %8s' % ('Area', 'Clean', 'Differs', 'Missing', 'Trivial', 'Clean %'))
    for area, _ in AREAS + [('Other', None), ('Missing', None), (ABI_AREA, None)]:
        c = by_area.get(area)
        if not c:
            continue
        judged_area = sum(c.values()) - c['trivial'] - c['inlined']
        print('%-32s %8d %8d %8d %8d %7.1f%%' % (area, c['clean'], c['differs'], c['missing'], c['trivial'],
                                                100.0 * c['clean'] / max(judged_area, 1)))
    identical, differing, absent, slot_match, slot_total = comparison.vtable_summary()
    compared = identical + differing
    print()
    print('Virtual function tables the original constructors install: %d of %d identical (%.1f%%), %d of %d '
          'slots (%.2f%%); %d base class tables the rebuilt executable never installs, not compared'
          % (identical, compared, 100.0 * identical / max(compared, 1), slot_match, slot_total,
             100.0 * slot_match / max(slot_total, 1), absent))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with open(args.report, 'w', newline='', encoding='utf-8') as handle:
            writer = csv.writer(handle, delimiter='\t')
            writer.writerow(['original', 'rebuilt', 'area', 'state', 'bytes', 'reasons'])
            writer.writerows(rows)
        print('\nper-function report: %s' % args.report)
    return 0


if __name__ == '__main__':
    sys.exit(main())
