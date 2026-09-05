# Linux localized search ABI, EU4 1.37.5

Process comparison at 0x1cad08f redirects to the portable SearchEngine.
Entries have stride 0x90: original text +0x30, cleaned +0x50, local +0x70.
The unmodified query pointer survives at original rsp+0x38; the hook must not
use the game's byte-wise lowercase/cleaned copy, which corrupts escape payloads.
Matched entries receive the portable distance and the native matched flag;
unmatched entries resume the original comparison.

Eight pushed GPRs preserve call alignment. The site has no live XMM state;
original comparison flags are rebuilt on fallback. Continuation pointers are
retained on unconfirmed rollback. Dictionary and SearchEngine initialization
is deferred until the first search because cpp-pinyin globals are not ready
during the preload constructor. A mutex serializes worker use of its cache.

The real ELF probe invokes actual Process with Chinese, full pinyin, initials,
uppercase and mixed ASCII/CJK queries. Portable regressions cover escape
payload letters that would be damaged by case folding before decoding.
