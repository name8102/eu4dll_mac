# Verification: Linux localization gate (full stack)

Date: 2026-09-05 (UTC)
Build: `build-linux/libeu4dll_linux.so` (localization work + crash fix, pre-push)

## Offline (read-only, real EU4 1.37.5 ELF)

`LocalizeYmlAddKey` (`0x23671a1`, bound `0x135`): value-conversion pattern
unique, call site `0x236721c` holds `E8 80 66 1E 00` exactly.

Deterministic tests: 19/19 pass (incl. `eu4dll.target.linux_localization`;
converter canonical encoding + round-trip pinned).

## Crash found and fixed during this gate (BLOCKER, now resolved)

Two deterministic SIGSEGVs (dumps `eu4_20260905_115049/115238`, since
cleared) with identical stacks:

```text
GetActualRequiredSize +0x20e/+0x227  (continuation load / rbp deref)
  <- AddNameArea <- GenerateNames <- InitMap <- boot map generation
```

- Disassembly proves the faulting loads consume the layout ARS hook's
  outputs (`mov rbp,[rcx+rax*8+0x100]` at the continuation, then
  `movswl 0xc(%rbp)`).
- Bisection: full stack minus localization boots clean (no dump, past the
  crash point); with localization it crashed twice at ~45s. Trigger =
  newly-converted content reaching an unguarded decoder: a marker at a
  string tail makes `movzx reg, word [rdx+1]` read past into NUL-adjacent
  bytes, yielding a wild index. (The Width hook already had a length
  guard; the other decoders did not — a legacy asymmetry.)
- Fix: every decoder arm in layout/main-text/tooltip now NUL-checks both
  payload bytes before the word read, falling back to the plain-byte path
  (stock-equivalent). Documented in all three ABI_NOTES.md. Well-formed
  input never touches the new branches.
- Re-verification: full 17-patch stack passes the old crash point, alive
  at ~2 min with active event processing, zero crash dumps.

## Live gate status

- Install evidence (17 patches, accepted target, no failures): verified.
- Crash-point regression: fixed and verified (no dumps where two
  deterministic dumps occurred before).
- Visual/operation confirmation after the guard fix: PENDING user check
  (Chinese display, events/tooltips/UI, save/load paths, time advance).
