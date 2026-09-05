# Legacy Linux migration map

Reference prototype: `name8102/eu4dll_linux`, branch `linux-port`, commit `102241a9116ce521170c75ce80e303698c4c427f`.

| Capability | Legacy Linux evidence | Canonical reference |
| --- | --- | --- |
| base | `src/platform/linux/base_probe.cpp` | already migrated in foundation |
| text layout | `text_layout_probe.cpp` | `src/textLayout.cpp` |
| main text | `main_text_probe.cpp` | `src/mainText.cpp` |
| tooltip/button | `tooltip_text_probe.cpp` | `src/tooltipAndButtonText.cpp` |
| UTF-8 localization | `localization_utf8_probe.cpp` | `src/localization.cpp` + portable features |
| map text | `map_text_probe.cpp`, `docs/MAP_TEXT_DEBUG_SUMMARY.md` | `src/mapText.cpp` |
| text3D | `text3d_probe.cpp` | `src/text3D.cpp` |
| input/IME | `input_ime_probe.cpp` | `src/input.cpp`, `src/features/text_input/` |
| clipboard | `clipboard_paste_probe.cpp` | input/text-input pipeline |
| pinyin/search | `pinyin_search_probe.cpp` | `src/pinyinHelper.cpp`, localization |
| save filename | no complete dedicated probe assumed | `src/saveFileName.cpp`; research Linux facts |

Known map-text risk: the prototype had a reverted CurveText change where allocation length and vertex-writing interpretation diverged, causing corruption, and later used mutable skipped-byte tracking. Do not reproduce cross-call global state without proving reentrancy/thread safety.

Foundation contracts already established: segmented ELF search, exact SHA-256/version validation, truthful `WriteResult`, verified atomic rollback, fail-safe trampoline lifetime, true rel32 near allocation, and Linux target facts separate from macOS facts.
