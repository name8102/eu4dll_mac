# Create exact macOS EU4 1.37.5 target profile

## Goal

Concentrate macOS x86-64 EU4 1.37.5 executable facts and reject unsupported
targets before any process mutation.

## Requirements

- Move patterns, offsets, symbols, object layouts, and continuation facts into
  the fixed target area.
- Replace the broad 1.37 prefix check with exact target validation.
- Do not use storefront or publisher labels as target-profile dimensions; they
  do not change the EU4 binary.
- Keep facts close to the hook adapters that consume them.
- Do not introduce general game-version selection.

## Acceptance Criteria

- [ ] Supported startup selects only macOS x86-64 EU4 1.37.5.
- [ ] Unsupported targets fail before patch installation.
- [ ] Available binary fixtures verify pattern uniqueness and original bytes.
- [ ] Failure output explains which target check failed.
- [ ] Windows and Linux remain documented future targets, not supported targets.

## Dependencies

Requires 07-12-patch-runtime.
