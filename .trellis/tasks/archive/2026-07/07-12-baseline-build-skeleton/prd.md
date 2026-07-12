# Establish baseline and modular build skeleton

## Goal

Capture current behavior and introduce the target directory/CMake structure
without intentionally changing the macOS dynamic library.

## Requirements

- Record the current configure, build, artifact, resource-copy, and startup
  behavior.
- Introduce build targets for portable features, runtime, macOS platform code,
  the macOS EU4 target, bootstrap, and tests.
- Preserve the existing final library name and linked dependencies.
- Keep behavior-changing code in later child tasks.

## Acceptance Criteria

- [ ] A baseline build result and known limitations are recorded.
- [ ] The modular source/build skeleton exists.
- [ ] The macOS library links the same required dependencies and resources.
- [ ] The dynamic library builds after behavior-neutral moves.
- [ ] No platform support beyond macOS is claimed.

## Dependencies

None. This is the first child task.

