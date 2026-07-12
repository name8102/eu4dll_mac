# Implementation Plan

## Child task map

| Order | Child task | Depends on |
|---|---|---|
| 1 | 07-12-baseline-build-skeleton | none |
| 2 | 07-12-patch-runtime | baseline-build-skeleton |
| 3 | 07-12-macos-target-profile | patch-runtime |
| 4 | 07-12-escaped-text | baseline-build-skeleton |
| 5 | 07-12-text-rendering | patch-runtime, macos-target-profile, escaped-text |
| 6 | 07-12-text-input-ime | macos-target-profile, escaped-text |
| 7 | 07-12-localization-features | patch-runtime, macos-target-profile, escaped-text |
| 8 | 07-12-diagnostics-verification | all previous children |

Each child must be planned, implemented, verified, and archived independently.
The parent remains the integration view and is complete only after every child
acceptance criterion and the parent PRD are satisfied.

## Phase 0 — Establish the baseline

- [ ] Record the current macOS configure and build commands.
- [ ] Build the unmodified library when dependencies are available.
- [ ] Capture exported symbols, generated artifacts, and current startup order.
- [ ] Add characterization fixtures for critical escaped-text behavior.
- [ ] Record known runtime-only checks that cannot be automated locally.

Gate: the current behavior and build limitations are documented before files
move.

## Phase 1 — Create the build and directory skeleton

- [ ] Introduce feature, runtime, platform, target, bootstrap, and test targets.
- [ ] Move files only where the move is behavior-neutral.
- [ ] Keep the original final dynamic-library target name during migration.
- [ ] Ensure the macOS target links the same required libraries and resources.

Gate: the macOS dynamic library still builds with no intended behavior change.

## Phase 2 — Deepen patch installation

- [ ] Define structured patch descriptions and installation results.
- [ ] Add deterministic byte-buffer memory support for tests.
- [ ] Adapt Mach memory access to the runtime seam.
- [ ] Concentrate scanning, original-byte verification, mutation, continuation
      calculations, diagnostics, and optimization ordering.
- [ ] Migrate a small representative patch before migrating the remaining
      installers.

Gate: representative live and fixture installations use the same runtime
behavior.

## Phase 3 — Add the fixed macOS EU4 target

- [ ] Move macOS x86-64 patterns, offsets, symbols, and object-layout facts
      into the historical `eu4_1_37_5` target directory.
- [ ] Replace version-only startup checks with full capability validation.
- [ ] Keep target facts close to the hook adapters that consume them.
- [ ] Verify pattern uniqueness and expected original bytes with available
      binary fixtures.

Gate: unsupported targets fail before mutation and diagnostics identify why.

## Phase 4 — Deepen escaped text

- [ ] Replace shared temporary return storage with explicit ownership.
- [ ] Consolidate conversion, traversal, cursor, deletion, and glyph semantics.
- [ ] Add round-trip, malformed-input, Windows-1252 mapping, capacity, and cursor
      tests.
- [ ] Migrate save filename, input, localization, and pinyin callers.
- [ ] Keep assembly address exports only where target hooks require them.

Gate: all portable escaped-text behavior is covered through one module.

## Phase 5 — Migrate text rendering

- [ ] Migrate screen and texture rendering.
- [ ] Migrate layout and measurement hooks.
- [ ] Migrate map and 3D text hooks.
- [ ] Remove duplicated escaped-character policy from target hook code where
      calling portable behavior is safe.
- [ ] Document unavoidable target-specific assembly duplication.

Gate: each rendering path installs through the patch runtime and uses the shared
escaped-text policy.

## Phase 6 — Migrate text input and macOS input methods

- [ ] Define normalized composition events.
- [ ] Separate portable editing behavior from EU4 text-buffer hooks.
- [ ] Implement the macOS input-method adapter around the current event path.
- [ ] Preserve multi-byte backspace and cursor behavior.
- [ ] Add adapter conformance and portable editing tests.
- [ ] Document Windows and Linux adapter requirements without claiming support.

Gate: macOS input works through the normalized path and portable editing tests
pass.

## Phase 7 — Migrate remaining features

- [ ] Migrate save filename hooks.
- [ ] Deepen localized search and isolate cpp-pinyin inside its implementation.
- [ ] Deepen East Asian name ordering and remove duplicated mutation policy.
- [ ] Separate date formatting and UTF-8 localization loading from unrelated
      localization features.
- [ ] Migrate base/font initialization patches.

Gate: the former broad localization and base modules no longer mix unrelated
feature policy.

## Phase 8 — Consolidate diagnostics and global state

- [ ] Replace ad-hoc tracking macros where structured patch results suffice.
- [ ] Localize continuation addresses and function pointers to target hook
      implementations.
- [ ] Preserve one consolidated startup failure report.
- [ ] Remove dead exports, unused helpers, and replaced installation paths.

Gate: global headers no longer act as an implicit dependency hub for feature
policy.

## Phase 9 — Final verification

- [ ] Run all portable and fixture tests.
- [ ] Configure and build the macOS dynamic library from a clean directory.
- [ ] Verify copied pinyin dictionary resources.
- [ ] Review every platform-native include and every game-layout dependency.
- [ ] Confirm no Windows or Linux support is claimed without platform evidence.
- [ ] Update project specs with durable discoveries.
- [ ] Record remaining runtime checks and platform validation gaps.

Gate: all PRD acceptance criteria are either demonstrated or explicitly marked
as requiring unavailable platform/runtime evidence.
