# Deepen patch installation runtime

## Goal

Concentrate patch location, validation, mutation, continuation calculations,
optimization, and diagnostics in one testable runtime.

## Requirements

- Preserve current Mach process-memory behavior through a macOS adapter.
- Add deterministic byte-buffer memory support for tests.
- Represent installation failures as structured results.
- Migrate representative patches before migrating the remaining installers.
- Keep feature policy and hook ABI code outside the patch runtime.

## Acceptance Criteria

- [ ] The live and byte-buffer adapters exercise the same patch runtime.
- [ ] Pattern uniqueness and expected original bytes can be verified.
- [ ] JMP, CALL, and raw-byte mutations have deterministic tests.
- [ ] Representative existing patches install through the new runtime.
- [ ] Diagnostics identify the feature, target, match, and failed operation.

## Dependencies

Requires 07-12-baseline-build-skeleton.

