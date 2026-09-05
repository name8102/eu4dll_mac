# Verification: Linux text-layout gate (base+textLayout)

Date: 2026-09-05 (UTC)
Build: `build-linux/libeu4dll_linux.so` (text-layout work, pre-push)

## Offline (read-only, real EU4 1.37.5 ELF)

All six layout sites preflight uniquely with exact expected bytes:

- GetHeightOfString `0x2053d53`, GetWidthOfString `0x2054027`
- GetActualRequiredSize `0x2056198`, GetRequiredSize `0x2056872`
- GetActualRealRequiredSizeActually `0x20570b6`
- Wrapping gate `0x2056386`

Deterministic tests: 16/16 pass (incl. `eu4dll.target.linux_text_layout`).

## Live gate

- A (base-only): 4/4 committed, `layout=disabled`, clean normal exit, no
  patch/memory failures, no segfault, no crash dump.
- B (base+textLayout): `layout=batch committed 6 patch(es)` (five glyph
  decoders + wrapping gate), game alive and operable.
- User confirmation: **no crash, display consistent between A and B**.
  Chinese still garbled on screen in both groups, as expected (rendering
  hooks belong to main-text, not this task).

## Standing process change

From this task on: no more AB comparisons. Validation looks only at the
latest group (B) effect: boots, no crash, no new visual/layout anomalies
vs. the previous stage.
