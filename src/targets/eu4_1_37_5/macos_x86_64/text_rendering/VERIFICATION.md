# Rendering target verification

The canonical inventory contains **28 mutation sites**:

- 23 JMP sites that enter target-specific naked hooks;
- 3 CALL sites, including both `CurveText_4` calls as separate mutations;
- 2 raw-byte layout mutations.

The 23 JMP hooks plus the uppercase CALL proxy account for 24 naked symbols.

`rendering_contract_test.cpp` verifies that every inventory entry supplies one
fact ID and pattern, stable expected original bytes, mutation kind/offset/width,
CALL width, continuation or bypass offsets, and the optimizer flag. Runtime
installers consume that same inventory through `PatchId`; the test does not
copy installer facts.

Run `verify_naked_hook_equivalence.sh` from any directory to compile the five
rendering sources at `HEAD` and in the working tree for x86-64, extract every
naked rendering hook/proxy symbol, and compare its raw machine code plus Mach-O
relocation targets. This is refactor evidence for unchanged register,
calling-convention, external call, and return/bypass-jump bindings. It is not
evidence that the hooks execute correctly inside EU4.

## Unavailable validation

No redistributable EU4 executable fixture is present in this repository.
Read-only probes of locally installed EU4 1.37.4.0 and 1.37.5.0 macOS x86-64
executables both pass the canonical 55-site/16-symbol capability registry.
That establishes real-binary pattern and expected-byte evidence, but not live
hook ABI, rendering behavior, or continuation execution. Synthetic descriptor
tests must not be interpreted as launched-process evidence.

The project now builds the dylib and `insert_dylib` with `minos 11.0` instead
of inheriting the host SDK default (`26.0`). Version 11.0 matches the adjacent
locally installed EU4 1.37.4 and 1.37.5 executables and is enforced by the
linker.
