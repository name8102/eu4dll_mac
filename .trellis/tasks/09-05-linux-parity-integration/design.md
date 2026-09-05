# Linux integration decisions

- Use canonical root targets and shared portable features. The untracked legacy
  prototype remains reference material and is not linked into the installed DSO.
- Keep ELF identity and exact site-byte checks. New input, clipboard, search and
  save adapters own their Linux System V ABI facts; portable code owns encoding
  and search policy.
- Use SDL's dynamic API override for the calibrated game's nonfunctional built-in
  X11 input-method start/stop implementation. The launcher selects system SDL2
  with X11 and preserves explicit environment overrides. Tested local runtime is
  sdl2-compat 2.32.70; no desktop Fcitx configuration is modified.
- UTF-8 commit is coalesced at Write. Native byte-size limits remain, with complete
  character boundaries. Atomic selected deletion avoids partial-text observer
  notifications. Edit hooks cover shared method entries because CEditBox installs
  an inherited vtable distinct from CTextBuffer's standalone table.
- CurveText's three counts serve short-label orientation, loop bound/denominator,
  and interpolation special case. The last must count glyphs to avoid single-Han
  NaN vertices. AddNameArea must zero-extend its uint8_t helper result from AL;
  EAX's upper bits are unspecified and can carry a previous string pointer.
- Keep manual-save disk/path objects UTF-8. Convert display copies at list,
  selection and confirmation boundaries instead of re-encoding load paths.
- Initialize cpp-pinyin lazily after preload construction; serialize its cache
  across native search workers. Decode query escapes before ASCII case folding.
- Install with dated backups and atomic file replacement. Test probe and curve
  trace DSOs remain test-only and are not shipped by the installer.

See target ABI_NOTES and docs/LINUX_ADAPTATION.zh-CN.md for site contracts,
commands and the distinction between implemented behavior and remaining parity.


## Display follow-up

A separate Linux display-formatting target delegates to existing portable date
and East Asian name policies. Linux culture/group offsets are independently
calibrated; enabled mod filenames are read from the live manager at call time.
Save header/continue additions reuse AppendDisplayCopy after the native color
prefix. See display_formatting/ABI_NOTES.md for all 7 exact CALL contracts and
fixture boundaries. Default launcher enables the display-formatting gate after
localization. Existing input, search and map code is unchanged in this follow-up.
