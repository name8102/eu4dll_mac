# Legacy Linux port migration research

## Source

- Repository: `name8102/eu4dll_linux`
- Branch: `linux-port`
- Reference commit: `102241a9116ce521170c75ce80e303698c4c427f`
- Target: Steam native Linux x86-64 EU4 `1.37.5.0 Inca`

This source is a working Linux prototype and binary-research archive. It is not
the architectural baseline for the new port.

## Proven platform behavior

The legacy port already demonstrated:

- Linux shared-library build and `LD_PRELOAD` loading;
- main ELF discovery with `dl_iterate_phdr()`;
- PT_LOAD segment classification;
- exact file SHA-256 validation plus version-string validation;
- `process_vm_readv`/`/proc/self/mem` read fallback;
- page-permission-aware `mprotect()` writes and instruction-cache flushing;
- unique pattern matching over executable regions;
- `dlsym(RTLD_DEFAULT, ...)` symbol anchors;
- near x86-64 trampolines for rel32-out-of-range hook targets;
- module-level patch transactions with pre-commit expected-byte checks and
  rollback;
- a real Linux base module that installed without an immediate crash;
- subsequent calibrated text-layout/main-text/tooltip/map hooks, which are
  intentionally outside the current task.

## Supported ELF identity

Known supported SHA-256:

```text
af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3
```

Expected version string:

```text
EU4 v1.37.5.0 Inca
```

Legacy development override:

```text
EU4DLL_ALLOW_UNSUPPORTED_ELF=1
```

The new implementation may preserve an explicit development override, but must
remain fail-closed by default and must not conflate an override with actual
support.

## Base module facts

### Constants

```text
expanded bitmap-font size = 0x86ac0
extended glyph index shift = 0x6ac
Linux glyph table offset observed elsewhere in the port = 0x100
```

The Linux object layout differs from macOS and must be represented by the Linux
target rather than shared constants.

### Patch 1: font allocation

Symbol:

```text
_ZN12CEU3Graphics16ReadGameSpecificER7CReaderiRP13C2dObjectTypeRP10CPdx3DTypeRP11CBitmapFont
```

Legacy symbol-local search size: `0xaf`.

Pattern:

```text
4D 89 CC BF 60 35 00 00 E8 ? ? ? ?
```

Patch offset: `+8`.

Expected bytes in the calibrated binary:

```text
E8 A4 02 11 FF
```

Replacement behavior: redirect allocation to a proxy that allocates and zeroes
`0x86ac0` bytes.

### Patch 2: ParseFontFile character limit

Symbol:

```text
_ZN11CBitmapFont13ParseFontFileEv
```

Search size: `0xa20`.

Pattern:

```text
41 81 FD FF 00 00 00 0F 87 ? ? ? ?
```

Patch offset: `+4`.

Expected byte `00` -> replacement `FF`.

### Patch 3: ParseFontFile character index hook

Same symbol/search bound.

Pattern:

```text
44 89 E9 48 8B 44 24 08 48 83 BC C8 00 01 00 00 00
```

Expected overwritten bytes:

```text
44 89 E9 48 8B 44 24 08
```

Continuation: patch site `+8`.

Legacy naked hook semantics:

```asm
cmp r13d, 256
jb 1f
add r13d, 0x6ac
1:
mov ecx, r13d
mov rax, [rsp + 0x8]
jmp continuation
```

This is Linux target ABI data and must be revalidated/documented at migration.

### Patch 4: texture size limit

Symbol:

```text
_ZN15CTextureHandler11LoadTextureERK7CStringRiRK20SLoadTextureSettingsi
```

Search size: `0x5e6`.

Pattern:

```text
81 FB 00 00 00 01 72 19
```

Patch offset: `+5`.

Expected byte `01` -> replacement `04`, raising the legacy limit from 16 MiB to
64 MiB.

## Near trampoline reference

The legacy allocator scans page-aligned candidate addresses within signed rel32
range around the patch site using `mmap(..., MAP_FIXED_NOREPLACE, ...)`.

The generated absolute transfer stub is 14 bytes:

```text
FF 25 00 00 00 00 <8-byte absolute target>
```

The page is written RW, instruction cache is cleared, then permissions are
changed to RX. Allocations are tracked and can be released on failed staging.

This is a proven Linux mechanism, but the new port should expose it through a
small allocator/branch-resolver boundary rather than copying the old global
trampoline manager unchanged.

## Patch transaction reference

The legacy `PatchTransaction`:

- rejects empty writes;
- verifies expected and replacement sizes;
- rejects overlapping writes;
- snapshots original bytes;
- rechecks originals immediately before each commit write;
- rolls back already-applied writes in reverse order after failure.

The new port should preserve these semantics while integrating them with the
shared `PatchRuntime` instead of retaining a parallel Linux patch engine.

## Important non-migration items

Do not copy these legacy architectural patterns into the new canonical tree:

- `memoryHelper_linux.cpp` as a second scanner/mutation runtime;
- every `*_probe.cpp` owning its own scanner, transaction, trampoline, and
  installer policy;
- one large `library.cpp` with platform and feature environment-variable
  switchboards;
- Linux implementation details placed in portable feature source.

## Later stability investigation context

The legacy port can display Chinese but has been reported to crash after running
for some time with broader feature sets enabled. Higher-risk rendering code is
therefore deliberately excluded from this foundation task.

One known risk in legacy `map_text_probe.cpp` is shared mutable hook state such
as `g_linuxCurveTextSkippedByteCount`, which assumes a particular execution
shape across hook invocations. The map-text debug history also records a prior
`CurveText` experiment that changed logical length/allocation behavior and
caused a stack overflow/SIGSEGV before being reverted.

These findings do not prove the current delayed-crash root cause. They justify
establishing a stable base-only baseline before migrating text/map hooks.

## Migration rule

For each legacy file, extract one of four things only:

1. platform mechanism -> implement behind the new platform/runtime interface;
2. target binary fact -> move to `linux_x86_64` target data;
3. hook ABI code -> move to target-specific source with an explicit ABI
   contract;
4. portable behavior -> use the already-refactored feature implementation.

Anything that does not fit one of those categories should not be copied by
default.
