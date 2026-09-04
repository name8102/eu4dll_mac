# Design: Linux runtime foundation and base vertical slice

## Architectural direction

The refactored `eu4dll_mac/main` tree remains canonical. The legacy
`eu4dll_linux/linux-port` tree is treated as a verified Linux prototype and
binary-research archive. We migrate facts and proven platform techniques into
existing abstractions; we do not merge the old source architecture.

The target architecture is:

```text
portable feature policy
        |
        v
runtime/patch ---------------- runtime/manifest
        |                            |
        |                            v
        |                     image identity
        v
platform/{macos,linux} <---- target/{macos_x86_64,linux_x86_64}
        |
        v
bootstrap/{macos,linux}
```

## 1. Build selection

Top-level CMake should always make portable `features` and `runtime` available.
Platform/target/bootstrap targets are selected conditionally:

```text
APPLE
  platform/macos
  targets/eu4_1_37_5/macos_x86_64
  bootstrap/macos
  macOS install/tooling

Linux
  platform/linux
  targets/eu4_1_37_5/linux_x86_64
  bootstrap/linux
```

Tests follow the same rule: portable/runtime tests always build; platform and
real-target tests are conditional.

## 2. Memory regions

The current single `MainModule()` region fits the Mach-O `__TEXT` model but is
not a safe model for ELF with multiple PT_LOAD regions and possible holes.

Prefer a minimal shared extension such as:

```cpp
enum class RegionPurpose {
    ExecutableSearch,
    ReadOnlySearch,
    Writable,
};

class Memory {
public:
    virtual std::vector<MemoryRegion> MainModuleRegions(
        RegionPurpose purpose, std::string &error) const = 0;
    ...
};
```

An equivalent design is acceptable if it preserves these invariants:

- scanners never read fabricated unmapped gaps;
- uniqueness is evaluated across the aggregate candidate regions;
- macOS can still expose one executable region without behavioral regression;
- symbol-scoped searches remain a bounded region rooted at the resolved symbol.

Linux region discovery uses `dl_iterate_phdr()` and PT_LOAD flags. Linux writes
use page-aligned `mprotect()` with original permission restoration.

## 3. Executable identity

Replace manifest-level hard coding of a 16-byte UUID with an identity value that
states its kind explicitly. Suggested shape:

```cpp
enum class ImageIdentityKind : std::uint8_t {
    MachOUuid,
    FileSha256,
};

struct ImageIdentity {
    ImageIdentityKind kind;
    std::vector<std::uint8_t> value;
};
```

Serialization must be versioned or otherwise backward-safe. If changing schema
version is required, update tests and tooling deliberately rather than silently
reinterpreting old bytes.

macOS continues to validate its Mach-O UUID. Linux validates the known file
SHA-256 plus version text. Identity policy belongs to the target/profile; file
hashing and ELF discovery belong to the Linux platform/bootstrap side.

## 4. Branch reachability

The shared runtime currently understands x86-64 relative E8/E9 mutations but a
Linux preload library may live beyond signed rel32 range from the main ELF.

Introduce a small resolver boundary rather than putting Linux allocation logic
inside feature code. One acceptable separation is:

```cpp
class ExecutableCodeAllocator {
public:
    virtual std::optional<Address> AllocateNear(
        Address anchor, std::size_t size, std::string &error) = 0;
    virtual void Release(Address address, std::size_t size) = 0;
};

class BranchTargetResolver {
public:
    virtual ResolvedBranch Resolve(
        Address instruction, Address requestedTarget,
        BranchKind kind, std::string &error) = 0;
};
```

The exact API may differ. Required behavior:

- if requested target is directly rel32 reachable, no trampoline is allocated;
- otherwise Linux attempts a nearby executable stub;
- the stub performs an x86-64 absolute indirect transfer to the final target;
- staged resources can be released when preflight/commit fails;
- successful resources remain alive for the lifetime of installed hooks.

The legacy implementation's `FF 25 00 00 00 00 + <target64>` stub is a proven
reference implementation.

## 5. Atomic feature installation

Do not simply call `PatchRuntime::Install()` four times for Linux base and accept
partial success.

Add a shared patch batch that separates:

1. locate/resolve;
2. validate expected bytes and mutations;
3. stage branch resources/payloads;
4. snapshot originals;
5. commit all writes;
6. roll back on partial failure;
7. publish continuations/resources only after success.

The batch implementation should reuse existing patch description semantics and
diagnostics rather than create a second scanner or mutation engine.

## 6. Linux target ownership

Create:

```text
src/targets/eu4_1_37_5/linux_x86_64/
  CMakeLists.txt
  target_facts.h
  profile.h/.cpp
  hook_symbols.h/.cpp        # if useful
  base/
    base_patch.h/.cpp
    base_contract.*          # optional, matching current target conventions
    ABI_NOTES.md
```

Linux target code owns:

- supported SHA-256 and version string;
- mangled game symbols;
- local pattern windows;
- expected original bytes;
- Linux-only object/layout constants;
- hook-site register/stack contracts;
- continuation and bypass offsets.

Portable feature code must not include these headers.

## 7. Base target facts to migrate

From the legacy Linux port, preserve and revalidate:

```text
expanded font allocation: 0x86ac0
character index shift:     0x6ac
```

Calibrated sites:

### ReadGameSpecific allocation call

Symbol:
`_ZN12CEU3Graphics16ReadGameSpecificER7CReaderiRP13C2dObjectTypeRP10CPdx3DTypeRP11CBitmapFont`

Search bound observed in legacy port: `0xaf`.
Pattern:
`4D 89 CC BF 60 35 00 00 E8 ? ? ? ?`
Mutation offset: `+8`.
Legacy expected call bytes:
`E8 A4 02 11 FF`.

### ParseFontFile character limit

Symbol:
`_ZN11CBitmapFont13ParseFontFileEv`

Search bound: `0xa20`.
Pattern:
`41 81 FD FF 00 00 00 0F 87 ? ? ? ?`
Mutation offset: `+4`.
Legacy expected byte: `00`; replacement: `FF`.

### ParseFontFile character index

Same symbol and search bound.
Pattern:
`44 89 E9 48 8B 44 24 08 48 83 BC C8 00 01 00 00 00`
Expected overwritten bytes:
`44 89 E9 48 8B 44 24 08`.
Continuation: site `+8`.

The naked hook must document its Linux ABI contract. Legacy behavior:

```asm
cmp r13d, 256
jb  normal
add r13d, 0x6ac
normal:
mov ecx, r13d
mov rax, [rsp + 0x8]
jmp continuation
```

### LoadTexture size limit

Symbol:
`_ZN15CTextureHandler11LoadTextureERK7CStringRiRK20SLoadTextureSettingsi`

Search bound: `0x5e6`.
Pattern:
`81 FB 00 00 00 01 72 19`
Mutation offset: `+5`.
Legacy expected byte: `01`; replacement: `04`.

These values are migration inputs, not unquestionable truth. The new target must
preflight them against the exact supported ELF before mutation.

## 8. Bootstrap

Linux bootstrap should be a separate source target and should not turn the
existing macOS entry point into an expanding `#ifdef` block.

Initial flow:

```text
constructor / preload entry
    -> discover main ELF
    -> identify exact supported target
    -> create LinuxProcessMemory + live PatchRuntime
    -> preflight base batch
    -> commit base batch
    -> emit consolidated diagnostics
```

Use `LD_PRELOAD` for the initial integration path. A launcher script may be
added if useful, but ELF binary rewriting is outside this task.

## 9. Delayed-crash containment

Do not migrate map/text3D/input hooks in this task. The purpose of the base-only
vertical slice is to establish a clean stability baseline before higher-risk ABI
hooks are introduced.

All future naked hooks should document:

- live input registers/stack values;
- clobbered GPR/flags/XMM state;
- preserved state;
- overwritten original instructions;
- continuation/bypass addresses;
- any external calls and required stack alignment.

## 10. Compatibility and rollback

Shared interface changes must be implemented with macOS updates in the same
commit/series so main never intentionally loses macOS support.

If a proposed abstraction substantially destabilizes existing macOS behavior,
prefer the smallest compatible extension that correctly supports Linux rather
than a second project-wide refactor.
