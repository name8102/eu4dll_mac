# Design

## Current shape

The current source groups code mainly by visible feature, but each feature also
contains pattern scanning, raw address arithmetic, memory mutation, diagnostics,
EU4 object-layout knowledge, and target-specific assembly.

Important policy is distributed:

- escaped-text rules appear in conversion code, input handling, and rendering
  assembly;
- patch installation mechanics repeat across more than fifty installers;
- pinyin search policy is split between localization and PinyinHelper;
- East Asian name rules share a large localization module with unrelated
  features;
- input combines game hooks, text-editing behavior, and platform integration.

## Target shape

    feature modules
        escaped text
        rendering semantics
        text input
        save filenames
        localized search
        East Asian names
             |
             v
    shared runtime
        patch installation
        diagnostics
        normalized events
             |
             v
    selected adapters
        platform adapter
        EU4 1.37.x target adapter
             |
             v
    platform dynamic library

## Proposed source layout

    src/
      features/
        escaped_text/
        text_rendering/
        text_input/
        save_filenames/
        localized_search/
        east_asian_names/
      runtime/
        patch/
        diagnostics/
      platform/
        macos/
        windows/
        linux/
      targets/
        eu4_1_37_5/
          macos_x86_64/
          windows_x86_64/
          linux_x86_64/
      bootstrap/
        macos/
        windows/
        linux/
    tests/
      features/
      runtime/
      fixtures/

Only the macOS target must contain a complete implementation during this
refactor. Other target directories are created only when real implementation or
documentation belongs there; empty support claims are forbidden.

## Patch runtime

The patch runtime concentrates the full installation lifecycle:

1. identify the selected target;
2. locate a pattern or symbol;
3. verify uniqueness and expected original bytes;
4. calculate the mutation and continuation addresses;
5. apply the mutation through process-memory access;
6. optimize target hook jumps where applicable;
7. return structured diagnostic results.

Mach process memory remains the live macOS adapter. A deterministic byte-buffer
adapter provides the second real implementation used by tests.

Target profiles hold the fixed capability contracts shared by supported EU4
1.37.x binaries. They do not select by patch/build/codename or storefront.

## Feature and hook separation

Portable modules operate on owned values or explicit views. They do not inspect
registers, stack frames, or raw EU4 object offsets.

Target hook adapters:

- extract values from the target ABI and EU4 objects;
- call portable behavior when policy is needed;
- write back results;
- continue at a profile-defined address.

Naked assembly remains target-specific. Similar-looking Windows and System V
assembly is not merged unless it is genuinely identical and verified.

## Escaped text

Escaped text becomes the shared definition of:

- UTF-8 and EU4 escaped-text conversion;
- escape-marker recognition and validation;
- logical character traversal;
- logical cursor movement and deletion;
- glyph-code mapping used by rendering;
- ownership, capacity, and malformed-input behavior.

Rendering and input adapters may retain ABI-specific implementations, but the
encoding policy must have one test surface.

## Input methods

The input path is:

    native input method
        → platform input-method adapter
        → normalized composition event
        → portable text-input behavior
        → EU4 target hook adapter
        → game text buffer

Normalized events cover composition start, update, commit, cancel, selection,
cursor movement, and backspace. Native window handles, SDL-native objects, and
operating-system input contexts remain inside platform adapters.

## Build organization

CMake exposes independent targets for:

- portable feature code;
- patch runtime;
- platform adapter;
- selected EU4 target;
- final dynamic library;
- portable tests and fixture tests.

Platform and target selection happens in the build graph. Shared implementation
files are not selected by widespread preprocessor branches.

## Migration and rollback

Migration proceeds as vertical slices. Each phase:

- introduces or deepens one module;
- moves its macOS callers;
- adds focused verification;
- removes replaced code only after the new path passes;
- leaves the library buildable.

If a phase fails, revert only that slice. Do not keep two active policy
implementations indefinitely.

## Risks

- Naked hooks may depend on undocumented compiler or register behavior.
- Refactoring global continuation addresses can change assembly symbol linkage.
- Build isolation may expose missing includes previously supplied transitively.
- Existing behavior has little automated coverage, so characterization tests
  must precede risky movement.
- Windows and Linux input behavior cannot be inferred solely from macOS.
