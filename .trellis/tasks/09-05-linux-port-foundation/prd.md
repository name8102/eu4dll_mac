# Establish Linux runtime foundation and port base hooks

## Goal

Make the refactored repository a real Linux-capable codebase without merging the
legacy Linux source tree into it. The existing `eu4dll_mac/main` architecture is
the source of truth. The legacy `name8102/eu4dll_linux` `linux-port` branch is a
validated source of Linux binary facts, ABI observations, platform techniques,
and regression behavior.

This task ends when native Linux x86-64 can build the portable runtime, validate
the supported EU4 1.37.5 ELF, preload a Linux shared library, and install the
four calibrated `base` patches through the refactored runtime architecture.

## Source Baseline

- Canonical repository: `name8102/eu4dll_mac`, base branch `main`.
- Legacy Linux research/prototype: `name8102/eu4dll_linux`, branch `linux-port`,
  commit `102241a9116ce521170c75ce80e303698c4c427f`.
- Supported Linux game target for this task: Steam native Linux x86-64
  `EU4 v1.37.5.0 Inca`.
- Known supported ELF SHA-256 from the legacy port:
  `af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3`.

## Requirements

### 1. Build graph

- `features` and `runtime` must configure and build on Linux.
- macOS-only settings, frameworks, bootstrap sources, target sources, tests, and
  install tooling must be guarded by platform selection rather than causing a
  non-Apple configure failure.
- Linux must produce a preloadable shared library target linked with required
  platform libraries such as `dl`.
- Existing macOS targets and tests must remain buildable.

### 2. Linux process memory and ELF regions

- Add a Linux implementation of the shared `patch::Memory` contract.
- Discover the main executable using ELF runtime metadata, preferably
  `dl_iterate_phdr()`.
- Do not model a multi-segment ELF image as one fabricated readable contiguous
  range when unmapped gaps may exist.
- Extend the shared memory/search abstraction as needed so pattern matching can
  safely scan multiple executable regions and still enforce global unique-match
  semantics.
- Resolve exported game symbols through `dlsym(RTLD_DEFAULT, ...)` where valid.
- Code writes must page-align `mprotect()` changes, preserve original page
  permissions, restore them after writes, and flush the instruction cache.

### 3. Platform-independent executable identity

- Remove the assumption that every manifest identity is a 16-byte Mach-O UUID.
- Represent loaded-image identity with an explicit identity kind and value.
- Preserve Mach-O UUID validation for macOS.
- Support Linux file SHA-256 identity for the initial Linux target.
- The Linux target must also validate the expected game version text.
- Unknown or mismatched ELF binaries must fail closed by default.
- Any development-only override must be explicit and produce an unmistakable
  diagnostic; it must not silently convert an unknown binary into a supported
  target.

### 4. rel32 reachability and near trampolines

- Shared patch installation must support x86-64 hook targets that are outside a
  direct signed rel32 range.
- Do not embed Linux `mmap()` policy directly into portable feature code.
- Introduce a narrow allocator/resolver boundary so a platform implementation
  can allocate executable code near a patch site.
- Linux may reuse the proven legacy technique: reserve a page within ±2 GiB,
  place an absolute indirect jump stub there, then redirect the game site to
  the reachable trampoline.
- Trampoline lifetime must be explicit; failed staging/installation must not
  leak or retain unusable trampolines.

### 5. Atomic patch groups

- A feature consisting of multiple dependent writes must not remain partially
  installed after a failure.
- Add a platform-independent batch/transaction mechanism around the shared
  runtime.
- Before commit, validate all target sites and expected bytes.
- Reject overlapping writes unless an explicit future contract permits them.
- If commit fails after one or more writes, restore all already-applied original
  bytes and report rollback failures distinctly.
- Trampolines and other staged resources must follow the same success/failure
  lifetime semantics.

### 6. Linux EU4 1.37.5 target profile

- Create `src/targets/eu4_1_37_5/linux_x86_64/` rather than adding Linux
  conditionals to macOS target files.
- Store Linux-specific symbols, patterns, expected bytes, object-layout facts,
  continuation offsets, and ABI notes in the Linux target.
- The target profile must own the supported ELF identity and game-version
  contract.
- Do not reuse macOS object offsets merely because both targets use x86-64.

### 7. Base vertical slice

Port only the four already-calibrated Linux `base` sites from the legacy port:

1. `CEU3Graphics::ReadGameSpecific` bitmap-font allocation call.
2. `CBitmapFont::ParseFontFile` character-limit byte.
3. `CBitmapFont::ParseFontFile` character-index hook.
4. `CTextureHandler::LoadTexture` texture-size-limit byte.

Known Linux target facts to preserve and verify:

- expanded bitmap-font allocation size: `0x86ac0`;
- extended character-index shift: `0x6ac`;
- Linux glyph-table/object layout is not the macOS layout;
- `ParseFontFile` hook ABI uses the Linux-calibrated register/stack contract,
  including the observed `r13d` index and `[rsp+0x8]` continuation context.

All four base modifications must install as one atomic feature group.

### 8. Linux bootstrap

- Add a Linux-specific bootstrap/loading path; do not make macOS `library.cpp`
  a large `#ifdef` switchboard.
- Initial supported loading method may be `LD_PRELOAD`.
- Startup order must be: discover host -> validate exact target -> preflight all
  base sites/resources -> atomically install base -> emit consolidated result.
- No later rendering/input/localization features are enabled in this task.

### 9. Tests and diagnostics

- Portable runtime tests must run on Linux without requiring EU4.
- Add deterministic tests for multi-region search, unique-match aggregation,
  identity serialization/validation, permission-safe memory writes where
  practical, direct rel32 vs trampoline resolution, transaction success,
  rollback, and overlapping-write rejection.
- Add Linux target tests using byte-buffer or fixture data for the four base
  patch descriptions.
- Preserve existing macOS manifest/runtime/target tests and update them for any
  shared interface changes.
- Real-EU4 validation is an integration gate, not the only verification method.

## Non-goals

- Git-merging the `eu4dll_linux` source tree into this repository.
- Porting `textLayout`, `mainText`, tooltip text, localization UTF-8, map text,
  text3D, IME/input, clipboard paste, pinyin search, or save-file hooks.
- Fixing the legacy delayed-crash issue in map rendering during this task.
- General support for arbitrary EU4 versions or arbitrary Linux distributions.
- A generic multi-ISA patch engine; Linux x86-64 is sufficient here.
- Rewriting every existing macOS naked hook.

## Acceptance Criteria

- [ ] CMake configures on Linux without an Apple-only fatal error.
- [ ] Portable feature/runtime tests build and pass on Linux.
- [ ] Existing macOS build/test behavior remains supported after shared API
      changes.
- [ ] Linux process memory enumerates the main executable's PT_LOAD regions and
      exposes executable search regions safely.
- [ ] Pattern matching across multiple regions still rejects zero and multiple
      total matches when uniqueness is required.
- [ ] Manifest/image validation supports Mach-O UUID and Linux SHA-256 identity
      without pretending one is the other.
- [ ] The supported Linux ELF SHA-256 and `EU4 v1.37.5.0 Inca` contract are
      represented in `linux_x86_64` target data.
- [ ] Unknown Linux ELF binaries fail closed before mutation.
- [ ] Out-of-range x86-64 jump/call targets can be staged through a tested near
      trampoline mechanism.
- [ ] Multi-write patch batches validate before commit and roll back on failure.
- [ ] The Linux `base` target owns its four symbols/patterns/expected bytes and
      ABI/layout facts.
- [ ] All four Linux base modifications preflight before any are committed.
- [ ] The four Linux base modifications install as one atomic feature group.
- [ ] A Linux preloadable shared library can be loaded into a small test host.
- [ ] On the supported real EU4 1.37.5 ELF, the Linux bootstrap validates the
      target and installs only `base` without an immediate crash.
- [ ] The task documents the exact real-game validation performed and any
      remaining stability gap before the next text-rendering task begins.

## Stability Gate

A successful launch is necessary but not sufficient. Before closing this task,
run a real-game base-only smoke/soak session that includes at minimum:

- reaching the main menu;
- loading or starting a campaign;
- opening several UI panels;
- advancing game time;
- observing logs for patch or memory failures.

If a crash occurs with only `base` enabled, stop the migration sequence and
root-cause it before porting text-layout or rendering hooks.

## Constraints

- Remain on C++17 unless separately approved.
- Preserve unrelated user changes.
- Keep platform-native headers out of shared feature modules.
- Keep Linux ABI/layout facts out of portable feature policy.
- Prefer small, reviewable commits with tests at each infrastructure boundary.
- Preserve fail-closed validation and actionable diagnostics throughout.
