# Linux `base` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

Only the four `base` sites from the foundation task. No textLayout, mainText,
mapText, text3D, input, pinyin, or save-file facts belong here.

## Constants

- Expanded bitmap-font allocation: `0x86ac0` (replaces the legacy `0x3560`
  `mov edi` immediate in `ReadGameSpecific`).
- Extended character-index shift: `0x6ac` (added to the glyph index for
  indices `>= 256`).
- Linux glyph-table/object layout is **not** the macOS layout. The macOS
  `0xE8` table offset / `0x86AC8` size must not be reused here.

## Site 1: `CEU3Graphics::ReadGameSpecific` allocation call

- Symbol: `_ZN12CEU3Graphics16ReadGameSpecificER7CReaderiRP13C2dObjectTypeRP10CPdx3DTypeRP11CBitmapFont`
- Symbol-local search bound: `0xaf`.
- Pattern: `4D 89 CC BF 60 35 00 00 E8 ? ? ? ?`, mutation offset `+8`.
- Expected call bytes: `E8 A4 02 11 FF` (5-byte relative CALL).
- Replacement: 5-byte relative CALL to `BitmapFontOperatorNewProxy`, which
  allocates and zeroes `0x86ac0` bytes. Reachability goes through the shared
  branch resolver; a near trampoline is staged only when rel32 is insufficient.

## Site 2: `CBitmapFont::ParseFontFile` character limit

- Symbol: `_ZN11CBitmapFont13ParseFontFileEv`, search bound `0xa20`.
- Pattern: `41 81 FD FF 00 00 00 0F 87 ? ? ? ?`, mutation offset `+4`.
- Expected byte `0x00` -> replacement `0xFF`.

## Site 3: `CBitmapFont::ParseFontFile` character-index hook (naked)

- Symbol and bound: same as site 2.
- Pattern: `44 89 E9 48 8B 44 24 08 48 83 BC C8 00 01 00 00 00`.
- Overwritten bytes (8): `44 89 E9 48 8B 44 24 08`.
- Continuation: site `+8`.
- Replacement: 5-byte relative JMP to `NakedParseFontFileCharacterIndex`
  (plus 3 NOP padding to cover 8 bytes), resolved through the shared batch
  with a near trampoline when needed.

### Live register/stack contract (System V x86-64, verified in legacy port)

At the hook site:

- `r13d` holds the glyph/character index under test.
- `[rsp+0x8]` holds the 8-byte value reloaded by the overwritten
  `mov rax, [rsp+0x8]`.
- The overwritten `mov ecx, r13d` (`44 89 E9`) feeds the downstream glyph-table
  lookup; the hook must replay it after applying the shift.

Hook semantics:

```asm
cmp r13d, 256
jb  normal
add r13d, 0x6ac
normal:
mov ecx, r13d
mov rax, [rsp+0x8]
jmp qword ptr [rip + g_linuxParseFontFileReturnAddress]
```

Clobbered state: EFLAGS (cmp/add), `ecx`, `rax` (both replay the overwritten
loads, so no caller-visible change beyond the intended shift).
Preserved state: `r13` except the intended `+0x6ac` for extended glyphs,
`rsp`, callee-saved registers, XMM state. The hook makes no calls, so stack
alignment is unchanged. The continuation is resolved from the batch install
result (`site + 8`) and published to `g_linuxParseFontFileReturnAddress`
before commit; failed installs clear the slot.

## Site 4: `CTextureHandler::LoadTexture` size limit

- Symbol: `_ZN15CTextureHandler11LoadTextureERK7CStringRiRK20SLoadTextureSettingsi`,
  search bound `0x5e6`.
- Pattern: `81 FB 00 00 00 01 72 19`, mutation offset `+5`.
- Expected byte `0x01` -> replacement `0x04` (16 MiB -> 64 MiB).

## Atomicity

All four install as one `PatchBatch` group: preflight (locate, expected
bytes, overlap, reachability) passes before any write; commit snapshots
originals, writes deterministically, and rolls back already-applied bytes in
reverse order on failure. Staged trampolines are released on failure and kept
alive on success.
