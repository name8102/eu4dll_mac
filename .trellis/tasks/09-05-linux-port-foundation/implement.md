# Implementation plan

## Phase 0: establish baseline

- Record the current `main` commit and verify the existing macOS build/test path
  before changing shared interfaces.
- Read the task PRD/design and project architecture/platform/quality specs.
- Use `name8102/eu4dll_linux@102241a9116ce521170c75ce80e303698c4c427f`
  only as a Linux reference implementation and binary-facts source.
- Do not copy its `memoryHelper_linux.cpp` or `*_probe.cpp` framework wholesale.

Review gate: no production code changes until the existing platform split and
runtime contracts are understood well enough to map every Linux addition to
`runtime`, `platform/linux`, `targets/.../linux_x86_64`, or `bootstrap/linux`.

## Phase 1: make the portable graph build on Linux

- Remove/replace the top-level non-Apple fatal configuration path.
- Keep `features` and `runtime` unconditional where possible.
- Guard macOS-only frameworks, targets, tests, installer tooling, and
  `CMAKE_OSX_*` settings under `APPLE`.
- Add Linux platform/target/bootstrap subdirectories conditionally, initially
  with only enough source to build a no-hook preload scaffold.
- Split tests so portable/runtime tests do not require macOS targets to exist.

Validation:

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

On macOS, rerun the repository's existing configure/build/test path.

Rollback point: if shared CMake changes cannot keep macOS intact, stop and
reduce the scope of the build-graph refactor before proceeding.

## Phase 2: multi-region Memory contract + LinuxProcessMemory

- Extend the shared `patch::Memory` abstraction so main-module scanning can use
  multiple safe regions rather than one fabricated contiguous range.
- Update `ByteBufferMemory` and `MachProcessMemory` to the new contract.
- Update `PatchRuntime::Locate()` to search all relevant regions and aggregate
  the match count globally.
- Preserve symbol-scoped bounded search behavior.
- Implement `LinuxProcessMemory`:
  - discover main ELF PT_LOAD regions with `dl_iterate_phdr()`;
  - expose executable/read-only/writable regions as required by the contract;
  - resolve symbols with `dlsym(RTLD_DEFAULT, ...)`;
  - write code with page-aligned `mprotect()` and permission restoration;
  - flush instruction cache after mutation.

Tests:

- one match in one of several regions -> success;
- no matches across all regions -> not found;
- two matches in different regions -> ambiguous when uniqueness required;
- no reads across unmapped/fake gaps;
- Linux memory write restores page permissions;
- symbol resolution failure is actionable.

Review gate: do not begin target hooks until the shared scanner can operate on
ELF regions without introducing a second Linux scanner.

## Phase 3: generalize image identity

- Replace manifest hard-coded 16-byte UUID identity with typed image identity.
- Preserve Mach-O UUID behavior and existing macOS validation.
- Add Linux SHA-256 identity support.
- Update manifest serialization/parser, validation, tests, and `manifest_tool`
  if required.
- If binary format changes, bump/version the schema explicitly.
- Put the supported Linux SHA-256 and version string into Linux target/profile
  data rather than a platform-global constant.

Tests:

- Mach-O UUID manifest round-trip remains valid;
- SHA-256 identity round-trip works;
- identity-kind mismatch fails;
- identity-value mismatch fails;
- version mismatch fails;
- malformed serialized identity fails closed.

## Phase 4: branch resolver and Linux near trampolines

- Add the smallest shared branch-resolution extension required to handle a hook
  destination outside signed rel32 range.
- Implement the Linux near executable allocator using `mmap()` near the patch
  site and an absolute x86-64 indirect transfer stub.
- Reuse the legacy `FF 25 00 00 00 00 + target64` technique unless a tested
  equivalent is demonstrably safer.
- Ensure direct reachable branches allocate no trampoline.
- Ensure failed staging releases any allocated trampoline.
- Keep successful trampoline pages alive for installed hook lifetime.

Tests:

- direct E8/E9 relative mutation;
- forced out-of-range target -> near trampoline;
- generated stub reaches the requested final address;
- allocation failure returns a diagnostic without mutation;
- staged trampoline cleanup on later failure.

Review gate: no Linux naked hook installation until this path is covered by
unit tests.

## Phase 5: platform-independent patch batch

- Add a shared transaction/batch layer around existing patch descriptions and
  mutation generation.
- Preflight every patch before any write.
- Snapshot originals.
- Reject overlapping staged writes.
- Commit in a deterministic order.
- On partial write failure, restore all already-written bytes in reverse order.
- Treat staged branch/trampoline resources as part of the transaction lifetime.
- Return consolidated diagnostics identifying the feature/site that failed and
  whether rollback succeeded.

Tests:

- complete success;
- expected-byte mismatch -> zero writes;
- overlapping mutations -> rejected before commit;
- failure after first write -> first write restored;
- rollback-write failure -> explicitly visible diagnostic;
- continuations/resources are not published on failed commit.

## Phase 6: Linux target profile

Create `src/targets/eu4_1_37_5/linux_x86_64/` with a structure matching current
target conventions rather than the legacy probe layout.

Add:

- target id/diagnostic id;
- architecture and game version;
- supported ELF SHA-256;
- required symbols for this base-only slice;
- the four base patch descriptions/facts;
- ABI notes for the naked ParseFontFile hook;
- target preflight/profile tests.

Migration source values are documented in `design.md` and
`research/legacy-linux-port.md`; verify them against the supported ELF instead
of blindly copying them.

## Phase 7: port Linux base as the first vertical slice

Implement only:

1. bitmap-font allocation replacement (`0x86ac0` allocation);
2. ParseFontFile character-limit extension;
3. ParseFontFile character-index naked hook (`+0x6ac` for extended glyphs);
4. LoadTexture size-limit extension to the legacy-proven value.

Requirements:

- use the shared `PatchRuntime`/batch path;
- use Linux target facts, not `#ifdef __linux__` inside macOS target files;
- resolve hook continuation through installation results/target contract rather
  than a new ad hoc scanner;
- install all four as one atomic base feature group;
- document the naked hook ABI contract in the Linux target directory.

Fixture tests must validate site location, expected bytes, mutations, and
continuation addresses without launching EU4.

## Phase 8: Linux bootstrap/preload

- Add a Linux-specific bootstrap target.
- Initial entry may use a shared-library constructor for `LD_PRELOAD`.
- Flow: discover ELF -> validate identity/version -> create live runtime ->
  preflight base batch -> commit -> report consolidated status.
- Unknown ELF: refuse to mutate.
- Do not enable any later text/input/localization module.
- Add/update a simple launch helper if needed for repeatable manual validation.

Validation with a non-EU4 test host:

- preload library successfully;
- host mismatch fails harmlessly;
- no unexpected unresolved symbols/dependencies.

## Phase 9: real EU4 validation and stability gate

On the exact supported Steam native Linux 1.37.5 ELF:

- verify logged SHA-256 and version;
- verify every base site preflights uniquely;
- verify the four mutations commit atomically;
- reach main menu;
- start/load a campaign;
- open multiple UI panels;
- advance game time;
- check logs for write/protection/branch errors;
- run a base-only soak long enough to distinguish an immediate hook error from
  a basic runtime-stability problem.

Record results in a task-local verification note before marking the task
complete.

If base-only crashes, do not proceed to textLayout. Capture the crash signal,
instruction pointer/backtrace, active patch set, and relevant disassembly, then
root-cause the base/runtime layer first.

## Explicitly deferred next task

After this task passes the stability gate, create a separate Linux text-display
task in this order:

```text
textLayout -> mainText -> tooltip -> localization UTF-8
```

`mapText`, `text3D`, input/IME, pinyin, and save filenames remain later tasks.
