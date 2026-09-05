# Port Linux text input and IME

## Dependency
`09-05-linux-localization-utf8`.

## Requirements
- Compare canonical `src/input.cpp` / `src/features/text_input` with `input_ime_probe.cpp`.
- Identify the actual Linux/SDL event seam used by EU4; platform adapter emits normalized composition/cursor/backspace/commit events.
- Game ABI/register facts stay in linux_x86_64 target code, not native event code.
- Verify composition, candidate commit, cursor movement, deletion, ASCII/CJK mixing, focus changes, cancellation and repeated editable-control lifecycle.
- Clipboard remains disabled until its own task.
