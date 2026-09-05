# Port Linux clipboard paste

## Dependency
`09-05-linux-input-ime`.

## Requirements
- Platform code obtains/normalizes clipboard text; reuse portable insertion/edit policy.
- Port any game-side paste interception facts from `clipboard_paste_probe.cpp` into the target adapter.
- Handle UTF-8/CJK, multiline and unsupported controls according to existing policy.
- Real-game gate: paste into all relevant controls, mix paste with IME composition/deletion, change focus, and verify no duplicate insertion.
