# Changelog

## v0.1.0-preview.2

- Unify macOS and Linux installation as `bash install.sh [GAME_PATH]`, including
  interactive path entry and source-checkout builds.
- Preflight the Linux launcher and complete dictionary before mutation; stage
  payloads and restore the previous installation after publication failure.
- Complete the multi-platform README, installation/recovery instructions,
  supported-version boundaries and issue-report guidance.
- Package both platforms with the same resource checklist, project/third-party
  notices, dictionary licenses, changelog and source/build metadata.
- Preserve the gameplay patch behavior already accepted in preview.1.

## v0.1.0-preview.1

- Integrate native Linux Chinese rendering, IME editing, clipboard adapters,
  pinyin search, local save filenames, date and East Asian name formatting.
- Fix single-character map labels, two-character spacing and atomic deletion.
- Add macOS/Linux cloud builds and tests; fix the macOS probe's Memory::Write
  interface after the first cross-platform CI run.

Extended campaign testing remains pending; these releases are prereleases.
