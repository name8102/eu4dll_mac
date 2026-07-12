# Architecture

## Direction

Organize the multi-platform fork around portable feature modules, platform
adapters, fixed EU4 target profiles, and a shared patch-installation runtime.

    portable feature behavior
            ↓
    patch runtime and normalized events
            ↓
    platform adapter + EU4 target hook adapter
            ↓
    EU4 1.37.x process

## Module placement

- features/: portable text encoding, search, name ordering, and text-editing
  behavior.
- runtime/: pattern scanning, patch validation, memory mutation, diagnostics,
  and normalized runtime events.
- platform/<os>/: operating-system process memory, image/symbol lookup,
  resource discovery, diagnostics, and input-method integration.
- targets/eu4_1_37_5/<platform-arch>/: executable fingerprints,
  patterns, offsets, object layout facts, and hook ABI adapters.
- bootstrap/: dynamic-library entry for each operating system.

## Dependency rules

- Portable feature modules must not include Mach, Win32, ELF, SDL-native, or
  game-layout headers.
- Platform adapters must not contain localization, search-ranking, name-order,
  or escaped-text policy.
- Target hook adapters translate registers and EU4 objects into portable values;
  they do not own feature policy.
- Avoid platform conditionals spread through shared code. Select adapters in the
  build graph.
- Add a seam only when at least two real adapters exist or when a live adapter
  and a deterministic test adapter are both used.

## Existing code

The current source is intentionally not reorganized during Trellis bootstrap.
Future refactors should migrate one vertical feature at a time and keep the
library buildable after each step.
