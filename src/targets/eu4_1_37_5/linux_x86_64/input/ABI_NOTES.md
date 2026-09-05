# Linux input and clipboard ABI, EU4 1.37.5

Ten input patches install as one batch: Begin/EndTextInputMode, two PollEvent
calls, UTF-8 commit dispatch, four CTextBuffer edit function entries and Write.
Three independent clipboard calls cover Paste, Copy and Cut. All symbols,
original bytes and continuation offsets are checked before installation.

- CSdlEvents text-input flag is +0x38. CRect<int> contains x/y/width/height,
  confirmed by Contains adding the last two values to the first two.
- SDL_TEXTINPUT/SDL_TEXTEDITING are 0x303/0x302, text at +12 with 32-byte capacity.
  Poll consumes pre-edit and composition navigation; focus loss cancels state.
- Commit site 0x1ff52d7: UTF-8 at rsp+0x3c, timestamp at rsp+0x34,
  handler in r13, keyboard in r15, continuation 0x1ff5601. Calls actual game
  CTextInputEvent/CInputEvent constructors and the original dispatch slots.
- CTextBuffer full string is +0x30, selection scratch +0x70, byte limit +0xe0.
  The four virtual slots are +0xd8/+0xe8/+0x140/+0x148 from the address point.
  CEditBox installs its own CTextBuffer address point (CEditBox vtable +0x2b0),
  so editing only the base-class vtable misses actual widgets. Hooks replace
  shared method entries and replay complete six-byte prologues (seven for
  EnterBackspace), covering inherited and direct calls. Original backspace recursively calls virtual MoveLeft,
  so a thread-local byte-operation guard preserves the native inner behavior.
  Han deletion selects the entire three-byte character then invokes native
  selected deletion once. Repeating native deletion per byte exposes partial
  escapes to text-change observers; an actual vtable notification regression
  fails on that implementation and passes on the atomic edit.
- Write replays its eight-byte prologue through a continuation. A scoped commit
  groups the game's byte events into one complete Write. Capacity stays in
  bytes and truncation happens only at escaped-text boundaries.
- Clipboard Get uses a hidden string-result pointer; Set receives a nontrivial
  by-value CString by invisible reference. Conversion uses independent copies.

Slots are retained if rollback cannot be confirmed. The adapter and dictionary
objects avoid dynamic initialization during the preload constructor.

Tests execute actual CTextBuffer constructors, Write, edits, selections, clipboard
and event constructors in the real ELF, including inherited CEditBox edit
entries. The base-vtable-only implementation fails that derived-widget
regression. The input adapter separately verifies
lifecycle, pre-edit, focus and consumed keys. Physical IME requires a functional
SDL backend; this game's built-in X11 text-input start/stop functions are empty.

SDL contracts: [text input](https://wiki.libsdl.org/SDL2/SDL_TextInputEvent),
[editing](https://wiki.libsdl.org/SDL2/SDL_TextEditingEvent),
[rectangle](https://wiki.libsdl.org/SDL2/SDL_SetTextInputRect).
