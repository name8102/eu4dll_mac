# Linux date, name and save-tooltip follow-up

Calibrated against EU4 1.37.5 ELF SHA-256
`af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3`.
Addresses below are evidence, not hard-coded destinations. Symbol-scoped unique
patterns and original bytes are checked before each batch; all sites overwrite
one five-byte CALL and return to site + 5 through the ordinary return address.

| Caller/site | Original bytes | Adaptation |
| --- | --- | --- |
| RefreshSpeedControlsWindow +0x4f (`0x1f4c707`) | `E8 C2 C6 09 00` | Gregorian GetString: RDI is unconstructed result storage, RSI date, RDX format. Forward with shared year/month/day format; preserve RAX result pointer. |
| GetFullName +0x11f (`0x1688b15`) | `E8 6A 36 EC 00` | RDI constructed given name, RSI temporary space+dynasty, RBX monarch. Read culture pointer at RBX+0x58; tail jump to helper; return RAX=RDI. Caller still destroys temporary. |
| GetNewRepublicName +0x1bb (`0xffc15d`) | `E8 0A 00 55 01` | Explicit surname append. |
| GetNewRepublicName +0x203 (`0xffc1a5`) | `E8 C2 FF 54 01` | Surname-table append. |
| GetNewRepublicName +0x25e (`0xffc200`) | `E8 67 FF 54 01` | Random dynasty append. |
| UpdateHeaderInfo +0x939 (`0x1d2cd8d`) | `E8 DA F3 81 00` | RDI color-prefix string, RSI stored filename at item+0x268; append display copy. Return value unused. |
| GetCurrentTooltip +0x4c6 (`0x1c5bbe8`) | `E8 7F 05 8F 00` | RDI color-prefix string, RSI local filename produced by GetContinueSave at +0x227; append display copy. Return value unused. |

Republic adapters use the native saved culture at caller RSP+8, hence RSP+0x10
on hook entry after CALL. RDI already ends with the native separator; strip only
that final space for surname-first cultures. Western branches append unchanged.
Naked adapters tail jump without altering stack alignment or callee-saved
registers. Other branches (interregnum, no dynasty, names without surname) remain
native.

Object facts confirmed from constructors/accessors:

- InitReligionAndCulture at `0x168d8a0`: monarch culture output address +0x58.
- CCulture ctor `0xf4e12a`: input culture tag copied to +0x40, localized name
  constructed at +0x60, group pointer stored at +0x80.
- CCultureGroup ctor `0xf4d35e`: input group tag copied to +0x38, localized name
  constructed at +0x58. Do not use macOS's common tag offset for both classes.
- CDLCManager::EnableMod at `0x1fded7a`: CPdxArray at +0x88; FindIndex confirms
  pointer +8 and signed count +0x14, with 32-byte CString elements. Therefore
  manager +0x90/+0x9c provides enabled mod filenames. Read the resolved singleton
  at name-call time, after mod loading, instead of caching during preload.
- Prefix literal `0x265b54a` is `A7 59 00` (yellow). Only the filename is
  UTF-8-converted; the prefix and persistent disk string must remain unchanged.

Verification: deterministic preflight accepts unique sites and rejects missing,
duplicate and unresolved contracts without writes. Real ELF probes exercise all
installed CALL adapters, including mode switching and temporary ownership. The
replacement main lacks a localization database, so date testing temporarily
stubs only GetMonthString; native Gregorian formatting and topbar adapter run.
That is ABI/format evidence, not a claim of final in-game font layout.
