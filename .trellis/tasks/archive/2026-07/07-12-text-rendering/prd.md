# Migrate text rendering hooks

## Goal

Move screen, texture, layout, map, and 3D text paths onto the patch runtime and
shared escaped-text behavior while preserving target-specific ABI code.

## Requirements

- Migrate each rendering path as an independently verified increment.
- Remove duplicated escaped-character policy where portable behavior is safe.
- Keep unavoidable register and calling-convention adaptation in target hooks.
- Preserve current installation order and visible rendering behavior.

## Acceptance Criteria

- [ ] Screen and texture text use the new modules.
- [ ] Layout and measurement paths use the new modules.
- [ ] Map and 3D text paths use the new modules.
- [ ] Every migrated hook installs through the patch runtime.
- [ ] Remaining assembly duplication is documented as target-specific.
- [ ] The macOS library builds after every rendering increment.

## Dependencies

Requires 07-12-patch-runtime, 07-12-macos-target-profile, and
07-12-escaped-text.

