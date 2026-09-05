# Verification: Linux main-text gate (base+layout+mainText)

Date: 2026-09-05 (UTC)
Build: `build-linux/libeu4dll_linux.so` (main-text work, pre-push)

## Offline (read-only, real EU4 1.37.5 ELF)

All three RenderToScreen sites preflight uniquely with exact bytes
(symbol `0x20519f8`, bound `0x218b`):

- preprocessing `0x2052126`, wrapping `0x2052239`, drawing `0x2052ed5`

Deterministic tests: 17/17 pass (incl. `eu4dll.target.linux_main_text`).

## Live gate

Bootstrap: base 4/4 + layout 6/6 + mainText 3/3 committed atomically,
supported ELF accepted, no patch/memory failures.

User confirmation: game enters normally, no crash, **Chinese partially
renders**. The rendered parts are main UI/event text (this task's
RenderToScreen scope); some buttons remain garbled, which is expected:
button/tooltip text belongs to the tooltip-text task, not this one.

Per standing process: only the latest-group (B) effect was evaluated.
