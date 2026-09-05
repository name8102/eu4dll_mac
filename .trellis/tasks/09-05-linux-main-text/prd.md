# Port Linux main text rendering

## Dependency
`09-05-linux-text-layout`.

## Requirements
- Extract Linux main-text sites/ABI facts from `main_text_probe.cpp`; compare with canonical `src/mainText.cpp`.
- Implement a linux_x86_64 adapter through the shared runtime/trampoline policy; reuse escaped/glyph policy.
- Keep mainText separately gateable before tooltip.
- Real-game gate: main menu, event text, province/country panels, long/multiline CJK strings, campaign load/start, time advance and short soak.
- Any crash blocks tooltip migration.
