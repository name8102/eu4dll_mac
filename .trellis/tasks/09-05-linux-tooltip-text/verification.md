# Verification: Linux tooltip gate (base+layout+mainText+tooltip)

Date: 2026-09-05 (UTC)
Build: `build-linux/libeu4dll_linux.so` (tooltip work, pre-push)

## Offline (read-only, real EU4 1.37.5 ELF)

All three RenderToTexture sites preflight uniquely with exact bytes
(symbol `0x204e886`, bound `0x311c`):

- preprocessing `0x204ef51`, wrapping `0x204f0f6`, drawing `0x2051085`

`CString::operator+=(char)` resolves at `0x254c1aa`.

Deterministic tests: 18/18 pass (incl. `eu4dll.target.linux_tooltip`).

## Live gate

Bootstrap: base 4/4 + layout 6/6 + mainText 3/3 + tooltip 3/3 committed
atomically (16 patches), supported ELF accepted, no patch/memory failures.

User confirmation: panels and button text display normally; no crash.
Chinese rendering now covers main UI, events, tooltips, and buttons.

## Standing process change

From this gate on: present the verification checklist BEFORE launching
the game, not after it has booted. Only the latest-group effect is
evaluated (no AB comparisons).
