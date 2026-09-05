# Port and harden Linux map text

## Dependency
`09-05-linux-localization-utf8` plus a stable UI-text soak.

## Requirements
- Read `MAP_TEXT_DEBUG_SUMMARY.md` and current `map_text_probe.cpp` before coding.
- Port verified FillVertexBuffer/AddNameArea/AddNudgedNames/CurveText facts one cluster at a time behind a mapText gate.
- Do not copy `g_linuxCurveTextSkippedByteCount` as process-global mutable state. Prefer per-call/stack derivation or another demonstrably reentrant design.
- Audit every naked hook that calls C/C++ for SysV stack alignment, caller-saved GPR/XMM, flags and red-zone assumptions.
- Do not revive the reverted CString::GetSize allocation experiment or allow allocation length to disagree with vertex writing.
- Real-game gate: zoom/pan, map modes, country/area/province and curved names, repeated rebuilds, time advance, save load, extended soak before text3D.
- On crash, capture IP/backtrace and active hook cluster; preserve cluster-level gating while diagnosing.
