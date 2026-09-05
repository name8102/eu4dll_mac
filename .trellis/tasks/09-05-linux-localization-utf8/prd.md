# Port Linux UTF-8 localization path

## Dependency
`09-05-linux-tooltip-text`.

## Requirements
- Compare `localization_utf8_probe.cpp`, canonical `src/localization.cpp`, and portable localization features.
- Port only Linux symbols/patterns/ABI bridges needed for UTF-8 data to reach the portable layer.
- Fail closed on target mismatch; keep pinyin/search disabled.
- Real-game gate: Chinese localization in events/tooltips/UI, load/save paths that touch localization, and time advance.
