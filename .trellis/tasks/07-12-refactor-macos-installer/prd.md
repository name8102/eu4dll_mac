# Refactor and test the macOS installer

## Goal

Refactor `install.sh` into a readable, testable, and fail-safe macOS install /
uninstall workflow while preserving the current user-facing behavior and the
install-time patch-manifest contract.

## Requirements

- Separate discovery, validation, privilege-requiring mutations, manifest
  generation, injection, resource copy, rollback, and uninstall into
  small shell functions with explicit inputs and return codes.
- Keep all distribution channels neutral; discover the selected EU4 app by
  path/capability rather than Steam/GOG-specific logic.
- Run the manifest tool before any game-directory mutation and install its
  output atomically only after successful validation.
- Make repeated install, update, repair, and uninstall idempotent.
- Preserve or restore the original executable safely; never overwrite the only
  known-good backup.
- On failure after mutation begins, report the failed phase and perform a
  bounded rollback where safe.
- Avoid unnecessary `sudo`; group privileged operations and quote every path.
- Keep multilingual messages consistent without duplicating control flow.
- Do not codesign or clear xattrs on the game App, executable, backup, existing
  frameworks, or installed dylib. The verified GOG 1.37.4 App is unsigned and
  bundle signing incorrectly treats the in-bundle backup as nested code.
- Add a fake `.app` integration harness that exercises install/update/uninstall,
  missing tools, stale manifest, spaces in paths, permission failures, and
  rollback without modifying a real game installation.

## Acceptance Criteria

- [ ] Shell control flow is decomposed into focused functions and passes
      `bash -n` plus the repository's shell lint when available.
- [ ] The fake-app harness validates first install, reinstall, update, repair,
      uninstall, rollback, and paths containing spaces.
- [ ] Manifest preflight always precedes executable injection/copy/signing.
- [ ] Failed preflight leaves the fake app byte-for-byte unchanged.
- [ ] Partial mutation failures restore a usable executable and do not publish
      a stale manifest.
- [ ] Install/uninstall are idempotent and preserve unrelated app resources.
- [ ] Release artifact layout and multilingual prompts remain compatible.
- [ ] Installer tests prove that neither `codesign` nor `xattr` is invoked.
- [ ] No test writes to a real EU4 app or requires network access.

## Constraints

- Do not modify or test destructively against `/Applications` or the Steam game
  directory.
- Do not duplicate manifest compatibility logic in shell; invoke the manifest
  tool as the source of truth.
