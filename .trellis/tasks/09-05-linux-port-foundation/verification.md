# Verification: Linux port foundation + base vertical slice

Date: 2026-09-05 (UTC)
Host: CachyOS Linux, x86-64, GCC 16.2.1, CMake 3.27+ / Ninja, `RelWithDebInfo`
Source baseline: `eu4dll_mac/main` canonical tree; legacy
`name8102/eu4dll_linux@linux-port` (`102241a`) used only as a binary-facts
and technique reference. Nothing was merged from it.

## Commands run (clean tree)

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

Result: configure succeeds on Linux with no Apple-only fatal error;
**15/15 tests pass**, including all pre-existing portable/runtime suites.

## What was verified locally (no EU4 required)

- Build graph: `features` + `runtime` configure and build on Linux; macOS
  frameworks/targets/tests/tooling are `APPLE`-guarded; Linux produces the
  preloadable `libeu4dll_linux.so` (`ELF 64-bit LSB shared object, x86-64`,
  `ldd` clean, only system undefined symbols).
- ELF memory: `LinuxProcessMemory` enumerates main-executable PT_LOAD regions
  via `dl_iterate_phdr()`; executable/read-only/writable purposes are exposed
  separately; `dlsym(RTLD_DEFAULT)` resolution failure names the symbol;
  `mprotect()` writes are page-aligned, restore original permissions (double
  write succeeds), and flush the instruction cache.
- Multi-region search: match in second region succeeds; a pattern straddling
  the unmapped gap does NOT match; duplicates across regions report
  `ambiguous` with aggregate `matchCount == 2`; single-region behavior
  unchanged (existing `patch_runtime` tests still pass).
- Identity: manifest schema is now v2 with explicit `macho-uuid` /
  `file-sha256` kinds (v1 manifests still parse and normalize). Mach-O UUID
  round trip preserved; SHA-256 round trip works; kind mismatch, value
  mismatch, version mismatch, and malformed identities all fail closed. Full
  `EU4 v1.37.5.0 Inca` text matches the short `1.37.5` manifest form.
- Branch reachability: directly reachable targets allocate nothing;
  out-of-range without an allocator fails with a diagnostic; the 14-byte
  `FF 25 + target64` stub decodes to the final address; allocation/protect
  failures release the page with zero leaks. Live `LinuxNearAllocator` test
  allocates a rel32-reachable trampoline near the caller and unmaps it on
  release.
- Atomic batches: success publishes per-patch continuations; expected-byte
  mismatch writes nothing; overlapping writes rejected before commit;
  second-write failure restores the first write; rollback-write failure
  surfaces a distinct `rollback` diagnostic; failed commits publish nothing.
- Linux target: `linux_x86_64` owns the supported SHA-256
  (`af115d3b…a7ce3`), `EU4 v1.37.5.0 Inca`, the three symbols, all four
  patterns/bounds/expected bytes, `0x86ac0` / `0x6ac` facts, and the naked-hook
  ABI contract (`ABI_NOTES.md`). Fixture tests prove site location, expected
  bytes, mutations, and `site + 8` continuation without EU4. Unknown binaries
  (bad ELF magic) fail closed with zero mutation.
- Preload host gate: `LD_PRELOAD=libeu4dll_linux.so` into a small test host
  fails closed on SHA-256 mismatch with an actionable diagnostic, performs
  zero mutation, and the host still exits with its own code (`42`). With
  `EU4DLL_ALLOW_UNSUPPORTED_ELF=1` the override prints an unmistakable
  NOT-supported warning, then still fails closed on the next fact (ELF header
  on a non-EU4 host, version text after the PIE-base fix) — never silently
  promoting an unknown binary.
- macOS preservation: shared-interface changes were applied to macOS adapters
  in the same series (`MainModuleRegions` overrides, `SetUuid`). macOS
  compile was **not** available on this Linux host, so macOS build/test
  behavior still requires a macOS confirmation run. No macOS behavior was
  intentionally changed: single-region adapters return their legacy region,
  and the manifest v1 path is backward compatible.

## Follow-up review fixes (before textLayout)

A post-commit review of `0f873de` found four failure-path issues in
`PatchBatch` / `Memory::Write` / trampoline lifetime. All fixed and
re-verified; no general test expansion was requested or added beyond one
targeted fault-injection regression test.

- **P0 — no munmap of possibly-referenced trampolines.** `Commit()` now
  tracks per-entry `mutationApplied` / `rollbackConfirmed` (read-back
  verified) and releases a trampoline only when its site never mutated or
  its restore is confirmed. Unconfirmed restores intentionally leak the
  page to process exit and the diagnostic names the retained trampolines.
- **P0 — `Write(false)` ≠ "nothing written".** `Memory::Write` now
  returns `WriteResult{bytesWritten, protectionRestored, error}` on all
  platforms (Linux/macOS/byte-buffer/file adapters updated). Rollback
  membership follows `bytesWritten`, and restores are read-back confirmed.
- **P1 — true process-lifetime allocator.** The bootstrap trampoline
  allocator is now heap-allocated and intentionally leaked, so C++ static
  destruction at exit can no longer unmap trampolines while EU4 global
  destructors in other DSOs may still call patched functions.
- **P2 — true ±2 GiB search.** The near allocator now parses
  `/proc/self/maps` for unmapped gaps inside anchor ±2 GiB (nearest
  first, 64-attempt budget) instead of the ±256 MiB truncated scan.
- Regression test: `TestUnconfirmedRollbackRetainsTrampoline` (poisoned
  write + dropped rollback ⇒ Rollback op, trampoline NOT released,
  payload honestly still visible) plus the mirror
  `TestConfirmedRollbackReleasesTrampoline` (clean failure ⇒ released, no
  safe-path leak). These fault paths are unreachable in normal soak
  testing.

Re-verification after the fixes: clean rebuild, **15/15 tests pass**,
preload smoke unchanged, and a second live EU4 run committed **4/4 base
patches with 2 trampolines via the new maps-based allocator**, reaching
`Running application` again with no patch/memory failures.

## Stability-gate progress (user-confirmed 2026-09-05)

- [x] Supported ELF SHA-256 + `EU4 v1.37.5.0 Inca` validated on the real binary
- [x] All four base sites preflight uniquely (offline scan, exact bytes)
- [x] Four mutations commit atomically on the real binary (2 trampolines)
- [x] Game opens normally with base-only build (user-confirmed)
- [x] Save/campaign loads into a running session (user-confirmed;
      `game.log` shows ingame lobby arrival and event-option selection)
- [ ] Explicit multi-panel + time-advance session and a timed base-only
      soak remain open before textLayout migration begins

The task's stability gate was not executed here because the supported Steam
native Linux EU4 1.37.5 ELF is not present on this machine:

- [ ] Log SHA-256 + version on the exact supported ELF
- [ ] All four base sites preflight uniquely on the real binary
- [ ] Four mutations commit atomically on the real binary
- [ ] Reach main menu; start/load a campaign; open several UI panels
- [ ] Advance game time; check logs for patch/memory failures
- [ ] Base-only soak to separate immediate hook errors from delayed crashes

Repeatable entry point for that session:

```bash
EU4DLL_GAME=/path/to/eu4 ./tool/linux_preload_run.sh
```

## Remaining stability gap before text rendering

Per the task design, `textLayout → mainText → tooltip → localization UTF-8`
must wait until the base-only gate above passes. If base-only crashes, stop
the migration and root-cause the base/runtime layer first (capture signal,
IP/backtrace, active patch set, disassembly). The legacy delayed-crash
context (map-text shared mutable hook state, reverted CurveText length
experiment) is intentionally not migrated and remains a later investigation.
