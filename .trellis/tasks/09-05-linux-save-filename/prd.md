# Port Linux save-file name handling

## Dependencies
`09-05-linux-input-ime` and `09-05-linux-localization-utf8`.

## Requirements
- Map canonical `src/saveFileName.cpp` into portable policy vs target ABI concerns first.
- Research equivalent Linux EU4 1.37.5 call sites/symbols and record exact patterns, expected bytes and ABI contracts; the final legacy probe list does not provide a complete dedicated save-filename module.
- Reuse input/localization encoding helpers where appropriate; feature remains gated/fail-closed until facts are verified.
- Real-game gate: create ASCII/CJK/mixed-name saves, overwrite, relevant autosave interaction, restart/reload and verify files remain accessible.
- If Linux does not need a macOS-specific workaround, document an explicit no-op/parity decision rather than forcing a hook.
