# Port Linux tooltip and button text

## Dependency
`09-05-linux-main-text`.

## Requirements
- Migrate calibrated Linux tooltip/button patterns, expected bytes, continuations and stack/register contracts from `tooltip_text_probe.cpp`.
- Reuse shared escaped-text/text-rendering helpers.
- Keep a separate feature gate.
- Real-game gate: dense tooltip hovering, nested panels, CJK buttons, event options, map-mode UI, time advance, repeated open/close cycles.
- Only add tests for invisible ABI/transaction failure modes.
