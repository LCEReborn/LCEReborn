# Measuring

How close is the rebuilt game to the original? `tools/measure/parity.py`
answers that function by function, from two files you have: the original
`Minecraft.Client.rpx` in your update 688 dump, and the one you built.

```
python tools/build.py
python tools/measure/parity.py --original <update v688 dump>/code/Minecraft.Client.rpx
```

It reads `build/Minecraft.Client.rpx`, its link map
`build/Minecraft.Client.map` and the `source/` tree by default (`--rebuilt`,
`--map` and `--source` change them), runs in about a minute, and
`--report FILE` writes one row per function with the reasons for each
verdict. It needs Python 3.8 or newer and a C++ demangler: `llvm-cxxfilt`
or `c++filt` if your machine has one, otherwise the `c++filt` inside the
Docker image `tools/build.py` builds.

## What it compares

Both executables keep their function names, relocations and machine code.
The tool pairs every function of the original with the function of the same
name in the rebuilt one and compares the facts a different compiler leaves
alone:

- the functions it calls;
- how many calls it makes through a pointer (virtual calls, callbacks);
- the floating-point constants it loads, and what each one is used for (an
  operand of a multiplication, an addition or subtraction, a division, a
  comparison, a store, a call argument); a constant next to the one the
  original loads counts as misread;
- the 32-bit constants it builds;
- the string literals it uses;
- the constant integer arguments of the calls both functions make;
- whether its body is far thinner than the original's.

A function is **clean** when all of these agree, and **differs** otherwise;
the report says why. Constructors, destructors and thunks are counted
separately, since the C++ ABI shapes them as much as the source does, and
the virtual function tables the constructors install are compared slot by
slot.

The rebuilt RPX is compiled by RedHills, the original by the compiler the
game shipped with, and the two inline different functions. The tool allows
for that: a call the original makes that the rebuilt function lacks still
counts when the callee could have been inlined there and the rebuilt body
holds what the callee does, and the other way round. A function of the
original with no copy in the rebuilt executable is looked up in `source/`:
when the source defines it, RedHills inlined it wherever it is used, and it
is reported as **inlined**; otherwise it is **missing**. Functions with
nothing to compare (a small accessor, for one) are reported as
**trivial**. Neither inlined nor trivial functions count in the
percentages. Code each compiler brings itself, the C++ standard library,
boost, rapidxml and the C runtime, is not compared.

## What the number means

- **It is a check of calls, constants and strings, not of behaviour.**
  Field offsets, the order of operations and branch conditions are not
  compared, so a clean function can still behave differently from the
  original.
- **It understates.** The two compilers sometimes produce different facts
  from the same source (one folds a constant the other loads, one keeps a
  call the other inlines in a way the tool cannot follow), so some correct
  functions are reported as differing.
- **It measures the build you run.** The numbers below come from the
  public build of the commit named with them; your own build of that commit
  gives the same result.

## Current result

Measured on 18 October 2026 with the public build of the commit that last
updated these numbers, against update 688's `Minecraft.Client.rpx`.

Of the original's 40,032 functions outside the standard library and the
runtimes, 23,851 were compared; 6,378 were inlined by RedHills and 9,803
had nothing to compare.

| Result | Functions | Share of compared |
| --- | ---: | ---: |
| Clean | 18,463 | 77.4% |
| Differs | 4,832 | 20.3% |
| Missing | 556 | 2.3% |

| Area | Clean | Differs | Clean share |
| --- | ---: | ---: | ---: |
| Game | 14,713 | 3,470 | 80.9% |
| Wii U platform layer | 551 | 287 | 65.8% |
| Mii faces (FFL) | 228 | 81 | 73.8% |
| NEX online library | 970 | 407 | 70.4% |
| Pia online library | 1,598 | 454 | 77.9% |
| Other Nintendo SDK | 43 | 5 | 89.6% |
| Sound and middleware | 340 | 115 | 74.7% |
| Cafe SDK runtime | 19 | 10 | 65.5% |
| Constructors, destructors and thunks | 4,138 | 709 | 84.6% |

The 556 missing functions are ones the tool could not find a definition of
in `source/`; its search reads source text, so some of them are defined in a
form it does not recognise.

Of the 2,464 virtual function tables the original's constructors install,
2,441 are identical in the rebuilt executable, and 102,993 of their 103,024
slots match. Of the 23 that differ, 15 differ in one way: the original's
compiler gives an override with a covariant return type
(`CompoundTag::copy`, for one) a second slot of its own, and RedHills does
not yet. 7 are tables the tool pairs with the wrong rebuilt table (the
`IdMapper` base table of a `MappedRegistry` against the derived
`SimpleRegistry` one), and one has trailing slots the rebuilt table lacks.
29 tables of abstract base classes are never installed by the rebuilt
executable, whose compiler drops a store the derived constructor
overwrites, and are not compared.
