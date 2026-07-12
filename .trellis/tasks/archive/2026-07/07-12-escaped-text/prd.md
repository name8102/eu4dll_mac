# Deepen escaped text module

## Goal

Make escaped text the single owner of encoding, traversal, cursor, deletion, and
glyph-code semantics used across the project.

## Requirements

- Replace shared temporary return storage with explicit ownership.
- Consolidate UTF-8, Windows-1252, and EU4 escaped-text conversion.
- Define malformed/truncated sequence and capacity behavior.
- Provide logical character traversal, cursor movement, deletion, and glyph
  mapping.
- Migrate callers without changing visible behavior.

## Acceptance Criteria

- [ ] Conversion round trips have focused tests.
- [ ] Malformed, truncated, capacity, and special-character cases are tested.
- [ ] Multi-byte cursor movement and deletion are tested.
- [ ] Input, save, localization, pinyin, and rendering callers use the module.
- [ ] Overlapping conversion entry points and shared temporary state are removed.

## Dependencies

Requires 07-12-baseline-build-skeleton.

