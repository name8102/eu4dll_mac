# Linux `localization_utf8` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

One call redirect inside `LocalizeYmlAddKey`: the UTF-8 value-conversion
call becomes the portable localization conversion. No naked hooks, no
pinyin/search facts here.

## Site

- Symbol: `_Z17LocalizeYmlAddKeyRK11CUTF8StringS1_iiPv` (bound `0x135`).
- Pattern: `BE A0 83 43 03 E8 80 66 1E 00`, mutation offset `+5`.
- Expected: `E8 80 66 1E 00` (5-byte relative CALL).
- Replacement: 5-byte relative CALL to `ConvertUtf8Localization`
  (via the shared branch resolver; a near trampoline stages only if the
  shared object ever maps outside rel32).

## Callee contract

`void ConvertUtf8Localization(const char *utf8_in, char *out_buffer)` is a
regular C++ function (System V: `rdi` = input, `rsi` = output), reached by
`call` — no naked prologue, no stack surgery:

- Delegates to
  `localization_loading::ConvertUtf8ForEu4(input, output,
  kLegacyOutputCapacity + 1)` — the exact function the canonical macOS
  adapter calls, so Linux and macOS share conversion semantics.
- Null-tolerant on both pointers (portable layer no-ops), matching the
  legacy early return.
- Output is bounded (`32767` chars + NUL) and always NUL-terminated when
  `out_buffer` is non-null.
- Deliberate deviation from the legacy prototype: the old 4-byte (emoji)
  path wrote four `'?'` with no bound check (latent 3-byte overrun past
  `MAX_OUT`); the portable layer bounds every unit. Output bytes are
  otherwise identical, including truncation points.

## Atomicity

Single-patch `PatchBatch` group: expected bytes verify before any write;
failure mutates nothing.
