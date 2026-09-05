# Complete Linux feature migration

## Goal

Finish native Linux x86-64 support in the canonical `name8102/eu4dll_mac` tree after the Linux runtime/base foundation. `name8102/eu4dll_linux@linux-port` is evidence for Linux symbols, patterns, ABI state, working behavior, and regressions only; do not merge its framework.

## Prerequisite

`09-05-linux-port-foundation` must remain stable. If base/runtime becomes unstable, stop later migration and repair that layer first.

## Dependency graph

```text
linux-port-foundation
        |
        v
text-layout -> main-text -> tooltip-text -> localization-utf8
                                      |          |          |
                                      |          |          +-> pinyin-search
                                      |          +-> input-ime -> clipboard-paste
                                      |                         +-> save-filename
                                      +-> map-text -> text3d

all children -> parity-integration
```

Tree position is not a scheduler; each child records its actual prerequisites.

## Global rules

- Linux ABI/layout facts live under `src/targets/eu4_1_37_5/linux_x86_64/`; native OS/event mechanisms under `src/platform/linux/`; reusable policy stays shared.
- Use `PatchDescription`, `PatchBatch`, and shared branch resolution. Do not resurrect legacy scanner/transaction/probe frameworks.
- Every naked hook documents entry registers/stack state, clobbers/preservation, replayed instructions, and continuation.
- Naked hooks that call C/C++ require a System V AMD64 audit: 16-byte call alignment, caller-saved GPR/XMM, flags, and red-zone assumptions.
- Keep feature groups independently gateable until final integration.
- Unknown target facts fail closed. Never copy a macOS object offset merely because both targets are x86-64.
- Tests are not a coverage target. Add deterministic tests only for states normal real-game exercising cannot reliably reveal.
- Each child ends with a real-game gate before dependent work is considered ready.

## High-risk map-text rule

`mapText` is isolated because the legacy implementation had historical CurveText allocation/length corruption and mutable skipped-byte state. Do not start text3D until map-text soak is stable.

## Completion

The migration is complete only after `linux-parity-integration` accounts for every canonical feature and every legacy Linux probe, the intended Linux feature set runs through the canonical architecture, and the full hook set survives an extended real-game soak and normal shutdown.
