# Verification: Linux map-text gate (full UI stack + mapText)

Date: 2026-09-05 (UTC)
Build: `build-linux/libeu4dll_linux.so` (map-text work, pre-push)

## Offline (read-only, real EU4 1.37.5 ELF)

All nine map sites preflight uniquely with exact expected bytes:

- AddNameArea ToUpper call `0x1b5dcd4`, spacing `0x1b5de2a`, glyph count
  `0x1b5e213`; AddNudgedNames glyph count `0x1b5fe41`
- CurveText drawing `0x1b5f463`, length calls `0x1b5f327/+11`, loop-init
  site located (unpatched by design)
- FillVertexBuffer preprocessing `0x2055562`, drawing `0x2055ac1`

CString slots resolve: AppendChar `0x254c1aa`, AppendString `0x254c19c`,
GetSize `0x254c3ee`, Index `0x254c110`, MutableIndex `0x254c10a`.

Deterministic tests: 20/20 pass (incl. `eu4dll.target.linux_map_text`
with per-cluster batches and escape-walk logic).

## Live gate

Bootstrap: base 4 + layout 6 + mainText 3 + tooltip 3 + localization 1 +
mapText 9 (four clusters, sequential with per-cluster logging), supported
ELF accepted, no patch/memory failures.

- Operated ~5 min session (zoom/pan/map-modes/names/rebuilds/time);
  reaped cleanly by the 300 s timeout (settings rewrite, zero dumps,
  zero popups).
- User confirmation: map/province/country/curved names display normally.

## Design record

CurveText skip is derived per-call (structural walk from the CString base
at `[rbp-0x148]`/data@+0 over `r14d` glyphs) — no process-global, no TLS,
no extra hook. The loop-init site stays unpatched original code.
