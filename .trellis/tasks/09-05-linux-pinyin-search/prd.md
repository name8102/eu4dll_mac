# Port Linux pinyin search and East Asian naming behavior

## Dependency
`09-05-linux-localization-utf8`.

## Requirements
- Compare `pinyin_search_probe.cpp` with canonical pinyinHelper/localization behavior.
- Reuse cpp-pinyin and the same packaged dictionaries; no Linux fork of resources.
- Port only target-specific search/name hooks.
- Validate Han queries, pinyin queries, mixed Latin/CJK, ambiguous syllables, empty/long input and repeated searches during a running campaign.
- Do not couple implementation to IME; both should compose through normal input/search paths when enabled.
