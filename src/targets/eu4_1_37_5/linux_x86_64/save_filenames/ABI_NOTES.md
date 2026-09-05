# Linux save filenames ABI, EU4 1.37.5

Eight symbol-scoped five-byte CALL redirects keep path storage in UTF-8:

- SaveGame 0x1cdcbfb converts the edited escaped name to UTF-8 at the native
  RemoveSpecialCharacters call. That native helper transliterates high bytes;
  it is not replaced globally.
- SaveGameSelect 0x1cdd249 converts the selected path stem for the edit box.
- CLocalSavegameItem 0x1d2c383 and 0x1d2c3cb pass converted copies to
  CText::ChangeString, leaving stored filename/path objects untouched.
- ConfirmSave 0x1d2a8b4, ConfirmLoadSave 0x1d2b11d,
  ConfirmLocalDeleteInGame 0x1d2b55e and ConfirmLocalDeleteGameSetup 0x1d2b7b9
  convert only the FILE replacement passed to PdxLocalize. The hidden return
  pointer precedes key, token and replacement reference in the System V ABI.

All original callees and site bytes are validated. Each wrapper copies display
text before conversion. The real ELF probe calls installed SaveGame and
SaveGameSelect targets and checks a Chinese filename round trip. User testing
confirms manual Chinese-name saves; cloud/autosave/continue-hover paths remain
outside that evidence.
