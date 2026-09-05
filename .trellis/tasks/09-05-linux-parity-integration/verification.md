# Integration verification, 2026-09-05

Target is Linux EU4 1.37.5 ELF SHA-256
`af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3`.

## Programmatic evidence

- Canonical Ninja RelWithDebInfo build succeeds; 23/23 CTests pass.
- A separate `-Wall -Wextra -Werror` build succeeds; its 23 CTests and full
  system-SDL real ELF probe pass too. Pinned cpp-pinyin warnings are nonfatal
  only within that third-party target.
- `scripts/linux/probe.sh <eu4.orig>` executes the game's constructors and actual
  installed machine code in a short replacement main. Full probe passes with
  built-in SDL 2.0.4 and system SDL 2.32.70 via SDL_DYNAMIC_API.
- Old map path reports 1/1/3 for a single Han label; corrected path reports 1/1/1
  and retains finite interpolation. ASCII, mixed and multi-Han counts agree.
- The actual AddNameArea spacing loop runs under a bounded probe frame with
  deliberately dirty scratch. It failed before zero-extending the AL helper
  result and passes for 黑羊, 白羊, 明, A明, 明A and A明B after correction.
- Real CTextBuffer constructors, event constructors and Write verify multi-Han
  commits and character-boundary truncation at a two-byte native limit.
- A native text-change callback regression rejects per-byte partial deletion;
  atomic selected deletion passes. A second regression uses the actual inherited
  CEditBox edit entries: the base-vtable-only implementation fails on 奥地利,
  while shared-entry hooks pass backspace, left and Delete.
- Native Copy/Cut/Paste pass a mixed Chinese/ASCII selection through an isolated
  SDL dummy clipboard. Desktop clipboard and user files are not touched.
- Actual search Process ranks Chinese, full pinyin, initials, uppercase, partial
  Han and mixed ASCII/Han queries correctly. Portable tests include damaged
  cleaned-name strings and letter-valued escape payloads.
- Installed SaveGame and SaveGameSelect conversion calls round-trip a Chinese
  filename. The installer verifies and records the installed library digest.

## User evidence and remaining gates

User confirmed single-character labels, pinyin search, manual Chinese-name saves,
and Chinese input/display after switching SDL backend. User found remaining
multi-press backspace and tight two-character spacing; the corresponding actual
ELF regressions and fixes above were added. The user subsequently confirmed both final keyboard deletion and map spacing
work correctly after restarting the final revision. Full clipboard/control matrix, save restart/reload, cloud/autosave,
East Asian naming/date layout and long campaign soak are not covered by these
claims. Keep the broader parity task open until its acceptance gates are met.

## Installed revision

Final normal-build library SHA-256: `58591d4c0602c2f7d02bf8da331f326ac7c6cdc947e939ce67fc0e734431a2e3`.
The installation manifest and rollback copies are under game directory
`eu4dll-backups/20260905-150413-313327/`.
The user exited the older process. A fresh game process loaded this revision,
all feature batches and the system SDL/Fcitx backend successfully. The user confirmed final keyboard deletion and map spacing are correct.

## Date/name/save-tooltip follow-up

User requested implementation of these remaining gaps. Added 5 display-formatting
CALL redirects and 2 save filename display-copy redirects. Ordinary and strict
builds pass 23 CTests, including no-write rejection for missing/duplicate sites
and a missing required symbol. Real ELF adapters pass on built-in SDL 2.0.4 and
system SDL 2.32.70. Installed launcher + installed library + system SDL also pass
the full probe (`/tmp/eu4-display-installed-probe.log`).

Date test executes native Gregorian formatting with a temporary month lookup
stub: replacement main does not initialize the localization database. The initial
unstubbed attempt faulted in LocalizeString; it was a fixture initialization gap.
Native SetDay/SetMonth are zero-based and format spaces are ignored; corrected
fixture verifies year marker, localized month and day marker ordering.

Name adapters cover Han, Japanese, English, Bai, Zhuang and a Bimillennium-only
culture group with the mode both disabled and enabled. Probe exercises monarch
RBX culture extraction, republic saved-stack culture extraction at all three
sites, return pointer and caller ownership. Header and continue tooltip tests
verify color-prefix bytes and immutable UTF-8 filenames. This is programmatic
coverage; all three new visible features still await user in-game confirmation.

Current installed normal library SHA-256:
`433dbc54e83fb8e4fd2231e3120493d530281cf9750367b6ab6818d5f8602edd`.
Rollback directory: `eu4dll-backups/20260905-153710-253354/`.
The previous game process was already absent at final check; next normal launch
will load this revision. No campaign was opened or user save modified by probes.

## User acceptance of display follow-up

User subsequently confirmed all three new visible features work normally:
topbar date, East Asian name order and save header/continue filename display.
This closes their pending in-game visual checks for the installed revision.
Long campaign soak, full clipboard/control matrix, cloud/autosave and all mod
combinations remain outside this acceptance evidence.

## Implementation closeout

2026-09-05: user accepted all implemented features and requested commit/push.
Repository renamed to `name8102/eu4dll_multiplatform`. This is the functional
implementation checkpoint; the user plans extended gameplay testing in a few
days. The broader stability task remains open until that evidence is available.
No scheduled reminder or automatic game run is part of this handoff.

When reporting a later regression, retain the installed library digest, game
version, enabled mods, action/screen and relevant startup/crash logs. Save/load,
time advance, map-mode changes and normal exit are useful real-play coverage.

## Cloud build/release preparation

Added macOS 15 Intel and Ubuntu 24.04 Linux build/test/package jobs using pinned
checkout/upload action revisions. Linux warnings are fatal for project targets.
Standalone Linux bundle installation passed in an isolated temporary game
directory; only its eu4.orig was a read-only link to the verified game ELF.
The local final source rebuild passes 23 CTests and the system-SDL actual ELF
probe. Its library digest is `4095febe93d9d82096cac0e846eef8bfc54ae0ac3508958312c2ba223dc0c776`;
the already user-accepted installed library remains the earlier recorded digest.
Cloud results and release URL will be recorded after the first run completes.
