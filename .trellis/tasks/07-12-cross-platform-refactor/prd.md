# Refactor project for multi-platform architecture

## Goal

Refactor the current macOS implementation into a testable multi-platform
architecture while preserving the existing EU4 1.37.x macOS behavior.

The finished structure separates portable feature behavior, patch installation,
operating-system adapters, EU4 target profiles, and target-specific hook ABI
code. It must support adding Windows and Linux implementations without copying
the complete macOS source tree.

## Requirements

### Supported game

- EU4 1.37.x is the supported game series.
- The macOS x86-64 target remains the first fully supported target.
- Startup validates all patch sites and required symbols before accepting a
  1.37.x build.
- EU4 is x86-64; storefront or publisher does not define a different binary
  profile when the executable is identical.
- Do not introduce a general multi-version compatibility system.

### Architecture

- Portable feature behavior is independent of Mach, Win32, ELF, native input
  methods, naked assembly, and EU4 object layouts.
- A shared patch runtime owns pattern matching, original-byte validation, memory
  mutation, hook payload creation, diagnostics, and installation results.
- Operating-system adapters own process memory, image/symbol lookup, resources,
  user diagnostics, and native input-method integration.
- EU4 target adapters own patterns, offsets, object layouts, calling-convention
  details, and control-flow redirection.
- Build configuration selects adapters; shared source must not accumulate
  platform conditionals.
- Global mutable hook state is reduced and localized to target hook
  implementations where it cannot yet be removed.

### Feature modules

- Escaped text owns encoding conversion, malformed-input handling, character
  traversal, cursor movement, deletion, and glyph-code semantics.
- Text rendering covers screen text, texture text, map text, 3D text, and text
  layout without duplicating escaped-character policy.
- Text input uses normalized composition events and a separate input-method
  adapter for each operating system.
- Save filenames reuse the escaped-text module rather than exporting several
  overlapping conversion entry points.
- Localized search owns normalization, pinyin and initials generation, caching,
  exact/fuzzy matching, and ranking.
- East Asian name ordering owns culture classification, surname order, and
  separator policy.

### Migration

- Migrate one vertical feature at a time.
- Keep the macOS dynamic library buildable after each phase.
- Preserve current behavior unless a change is explicitly documented and
  covered by acceptance criteria.
- Avoid a single repository-wide rewrite that cannot be reviewed or rolled
  back incrementally.

## Non-goals

- Supporting EU4 versions outside the 1.37.x series.
- Claiming Windows or Linux runtime compatibility without their executable
  fixtures, build environment, and platform integration verification.
- Rewriting every naked hook into portable code when ABI-specific assembly is
  still required.
- Changing localization features or user-visible behavior solely for style.

## Acceptance Criteria

- [ ] The project has clear feature, runtime, platform, target, and bootstrap
      module locations reflected in CMake targets.
- [ ] The macOS x86-64 EU4 1.37.x dynamic library builds after the
      migration.
- [ ] Startup performs exact target validation and reports actionable failures.
- [ ] Patch installation is exercised through deterministic memory or binary
      fixtures without launching EU4.
- [ ] Escaped-text round trips, malformed sequences, cursor movement, and output
      limits have focused tests.
- [ ] Localized search and East Asian name-ordering policy have focused tests.
- [ ] macOS input is expressed through normalized composition events and a
      macOS-specific input-method adapter.
- [ ] Windows and Linux have documented adapter/target extension points without
      placeholder implementations being reported as supported.
- [ ] Target hook code contains ABI and layout facts but not portable feature
      policy.
- [ ] No shared feature module includes platform-native or game-layout headers.
- [ ] Existing feature installation failures remain visible in a consolidated
      diagnostic result.
- [ ] Relevant build and test commands are documented and pass on available
      platforms.
- [ ] Remaining platform-specific validation gaps are explicitly listed at task
      completion.

## Constraints

- Preserve unrelated user changes throughout the refactor.
- Remain on C++17 unless a separate decision changes the language level.
- Network-dependent build steps must not be the only way to run portable tests.
- Platform-specific source may differ when calling conventions or input-method
  behavior genuinely differ.
