# Consolidate diagnostics and complete verification

## Goal

Remove obsolete installation paths and global state, complete integration
verification, and document remaining platform evidence gaps.

## Requirements

- Consolidate startup installation results and failure reporting.
- Localize continuation addresses and function pointers to target hooks.
- Remove dead exports, unused helpers, and replaced code.
- Run portable, fixture, resource-copy, clean-build, and macOS integration
  verification.
- Audit platform-native includes and EU4 layout dependencies.

## Acceptance Criteria

- [ ] Startup produces one actionable consolidated diagnostic result.
- [ ] Replaced tracking and installation paths are removed.
- [ ] Global headers no longer carry unrelated feature policy.
- [ ] Portable and fixture tests pass.
- [ ] A clean macOS configure/build succeeds.
- [ ] Pinyin dictionary resources are present in the artifact.
- [ ] Unsupported Windows/Linux claims are absent.
- [ ] Remaining runtime or unavailable-platform checks are explicitly recorded.
- [ ] Parent-task acceptance criteria are reviewed and reconciled.

## Dependencies

Requires all other child tasks.

