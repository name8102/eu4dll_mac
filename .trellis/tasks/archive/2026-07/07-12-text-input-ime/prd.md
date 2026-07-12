# Separate text input and macOS IME adapter

## Goal

Separate portable editing behavior, macOS input-method integration, and EU4
text-buffer hooks through normalized composition events.

## Requirements

- Define composition start, update, commit, cancel, selection, cursor, and
  backspace events.
- Move escaped-text editing behavior into portable code.
- Keep native event capture and input-method state in the macOS adapter.
- Keep EU4 addresses, object layout, registers, and control flow in target hooks.
- Document Windows and Linux adapter requirements without placeholder support.

## Acceptance Criteria

- [ ] Portable editing and composition behavior has focused tests.
- [ ] macOS input flows through normalized composition events.
- [ ] Multi-byte backspace and cursor behavior are preserved.
- [ ] Native input objects do not leak into portable feature modules.
- [ ] A platform adapter conformance test shape is documented.
- [ ] macOS integration verification results are recorded.

## Dependencies

Requires 07-12-macos-target-profile and 07-12-escaped-text.

