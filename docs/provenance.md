# Provenance

## Where this source comes from

This repository is a matching decompilation of the Wii U build of the
title: source written by reading the original executable's compiled code
and refined until it compiles to the same code.

The Wii U build ships unstripped. Its symbol table names every function and
every class, its run-time type information records the class hierarchy, and
its constructors and string tables record class layouts and the text the
game uses. That metadata gives the source its shape: names, signatures and
layouts. The bodies of the functions come from the compiled code itself,
read as disassembly and as intermediate listings generated from it, which
show what each function does.

All of the source is written by AI agents (see `overview.md`). Each class
was reconstructed as a unit by a worker agent reading the evidence for that
class, including its compiled code, deciding what the original source must
have looked like, and writing it in the style described in
`conventions.md`. Each function is then compiled and compared with the
original (see `measuring.md`), and the difference is worked down until the
two match. The Xbox 360 TU75 build of the same codebase served as the
behavioural reference when the Wii U evidence alone left a question open.

The source is therefore derived from the original executable. What it is
not is tool output: no file is emitted by a decompiler or produced by
transforming the binary, and every file is written by hand in the
project's own style.

## What this repository deliberately excludes

- The game binaries, in any form.
- Game assets: textures, models, sounds, fonts, UI movies, language files,
  world templates and save data.
- Symbol tables, disassembly, decompiler output, intermediate "lowered" or
  "semantic" source, and any other file generated from the binary.
- Compiler and linker metadata recovered from the build.
- The tooling that consumed those inputs.

The reconstruction workflow that produced this tree, and the evidence it
read, lived in a separate working repository. Only the resulting source,
and the tooling needed to build it from your own copy of the game, was
carried across. No file under `source/` is the output of a decompiler or
other tool run on the binary; every file is authored text, written with
reference to the original's compiled code as described above.

## Rights

The game, its code and its content belong to their owners. Because the
reconstructed source is derived from the original executable, the licence
in `LICENSE` covers what this project contributes of its own - the build,
packaging and measuring tooling under `tools/` and the project's changes to
other projects' code - and grants no rights to Minecraft: Wii U Edition
itself. Build it only from your own copy of the game.

## Third-party code

`source/vendor/` reconstructs the internals of libraries the title links
against, because those libraries were built into the executable with their
names intact and game code calls into them. They are kept separate from the
game so that the boundary is explicit. If a maintained upstream for any of
them becomes available, the reconstructed copy should be replaced by that
upstream rather than maintained here.

The Mii face library is the one case where such an upstream exists:
aboood40091's FFL decompilation. Its code is never copied into this
repository. It is referenced as the `source/nn/ffl` submodule, a fork of
that project that carries this reconstruction's changes as separate
commits, so upstream's authorship stays with upstream.

## Contributions

Contributions must follow the same rule: source written by hand from
understanding, which may be gained by reading the original's compiled code,
but never pasted from a decompiler, and never accompanied by binaries or
assets.
A pull request that adds anything from the excluded list will be closed.
