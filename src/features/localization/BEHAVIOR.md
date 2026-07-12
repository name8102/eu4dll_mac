# Localization feature behavior

This migration intentionally preserves the visible EU4 1.37.x behavior:

- save names are UTF-8 on disk and EU4 escaped text in UI strings;
- display-only save-name adapters return or construct caller-owned copies;
  they never mutate the UTF-8 filename source and use no shared temporary;
- `UpdateHeaderInfo` leaves its live `"\xA7Y"` tooltip CString intact and
  replaces only the original `operator+=` call, appending a function-local
  converted copy before returning to the instruction after that call;
- ASCII search is case-normalized, exact matches rank `-3`, prefix matches
  `-2`, early/middle matches `-1` or `0`, and suffix matches `1`;
- Chinese search accepts full pinyin, initials, and every polyphonic form
  returned by cpp-pinyin, with generated forms cached per original name;
- vanilla and Bimillennium Universalis culture tables retain their previous
  membership; Chinese names have no separator, while other surname-first
  East Asian names use one space;
- localization input is converted from UTF-8 to EU4 escaped text at load time;
- the top-bar date byte format remains year, month, day.

Executable patterns, offsets, object layouts, and naked hook ABI remain in the
fixed `eu4_1_37_5/macos_x86_64` target adapter. Distribution/publisher labels
do not participate in the target identity because EU4 has one x86-64 binary
profile per distinct executable.

Patch contracts require a unique match, record the overwritten length, and
verify every overwritten original byte. The canonical 55-site registry and 16
required symbols pass read-only preflight on legally installed EU4 1.37.4.0 and
1.37.5.0 macOS x86-64 executables. Automated tests additionally cover
uniqueness, expected-byte rejection, mutation failure, and continuation
contracts with deterministic byte buffers. Live localization behavior and hook
ABI still require launched-process verification.
