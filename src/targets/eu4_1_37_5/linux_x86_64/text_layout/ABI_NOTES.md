# Linux `text_layout` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

Five glyph-decoder hooks plus the `GetActualRequiredSize` wrapping gate.
The legacy `loopTail` / `ellipsisTruncation` probes stay **disabled**
(probe-only, never installed). No mainText/mapText/text3D facts here.

## Shared vs target-owned policy

- Escape markers (`0x10`–`0x13`) and shifts (`-0x0E` / `+0x900` / `+0x8F2`)
  reuse the portable `escaped_text` module constants. The decode semantics
  were verified byte-identical against real game log data.
- Linux target facts: glyph-table base `0x100`, extended-index shift
  `0x6AC`, the five symbols, search bounds, patterns, expected spans,
  overwrite widths, and every continuation/bypass offset below.
- Linux values are NOT the macOS values (`0xE8` table base, `0x6B0` shift).

## Common decoder shape

Each hook replaces a `movzx reg, [ptr]` + glyph-table-load sequence:

```asm
<save scratch>
<ptr -> rdx, marker byte -> index reg>
if marker == 0x10..0x13: index = word[rdx+1] (+/- shift)
<site length-counter advance>
if index >= 256: index += 0x6AC
<replay overwritten table load>
jmp continuation
```

No calls, no XMM use. Pushes are balanced on every path (including the
width bypass), so stack alignment at the continuation is unchanged.

## Site contracts

### 1. `GetHeightOfString` glyph lookup

- Symbol: `_ZNK11CBitmapFont17GetHeightOfStringERK7CStringiiRK8CVector2IiEb`
  (bound `0x3f4`); pattern `0F B6 00 48 8B 84 C3 00 01 00 00 48 85 C0`;
  overwrite 11, continuation site `+11` (`return`).
- Entry: `rax` = string byte pointer (overwritten `movzx eax, [rax]`).
- Hook: `push rdx`; `rdx = rax`; decode into `eax` with counter
  `add ebp, 2`; `pop rdx`; replay `mov rax, [rbx+rax*8+0x100]`.
- Clobbers: `rax`, `ebp` (+2, intended length counter), flags.
  Preserves: `rdx` (push/pop), `rbx`.

### 2. `GetWidthOfString` glyph lookup

- Symbol: `_ZN11CBitmapFont16GetWidthOfStringEPKcib` (bound `0x257`);
  pattern `48 8B AC F7 00 01 00 00 48 85 ED`; overwrite 8,
  continuation `+8` (`return`), bypass `+0x187` (`bypass`).
- Entry: `esi` = current index, `edx` = length bound, string at `[rbx+r15]`
  (the overwritten 8 bytes are `mov rbp, [rdi+rsi*8+0x100]`).
- Hook: `push rdx; push rcx`; `ecx = edx`; `rdx = rbx+r15`; decode into
  `esi` with counter `add r14d, 2` plus `cmp r14d, ecx; jge bypass`
  (prevents over-reading two bytes past the end when the string ends with
  an escape marker, which used to misplace `0x0A`); `pop rcx; pop rdx`;
  replay `mov rbp, [rdi+rsi*8+0x100]`; `jmp return`.
- Bypass path: `pop rcx; pop rdx; jmp bypass`.
- Clobbers: `esi` (intended), `r14d` (+2), `rbp` (replayed), flags.
  Preserves: `rdx`, `rcx` (push/pop), `rbx`, `rdi`, `r15`.

### 3. `GetActualRequiredSize` glyph lookup

- Symbol: `_ZNK11CBitmapFont21GetActualRequiredSizeERK7CStringiiR8CVector2IjERS3_IiEb`
  (bound `0x618`); pattern
  `0F B6 00 48 8B 4C 24 28 48 8B AC C1 00 01 00 00 48 85 ED`;
  expected first 8 bytes, overwrite 8, continuation `+8`.
- Entry: `rax` = string byte pointer.
- Hook: `push rdx`; `rdx = rax`; decode into `eax` with counter
  `add r13d, 2`; `pop rdx`; replay `mov rcx, [rsp+0x28]`; `jmp return`.
- Clobbers: `rax`, `r13d`, flags. Preserves: `rdx`.

### 4. `GetRequiredSize` glyph lookup

- Symbol: `_ZNK11CBitmapFont15GetRequiredSizeERK7CStringRS0_iiR8CVector2IjEb`
  (bound `0x7bc`); pattern `0F B6 00 49 8B AC C5 00 01 00 00 48 85 ED`;
  overwrite 11, continuation `+11`.
- Entry: `rax` = string byte pointer.
- Hook: `push rdx`; `rdx = rax`; decode into `eax` with counter
  `add dword ptr [rsp+0xc], 2` (address accounts for the pushed `rdx`;
  calibrated live); `pop rdx`; replay `mov rbp, [r13+rax*8+0x100]`.
- Clobbers: `rax`, stack counter slot (+2, intended), flags.
  Preserves: `rdx`, `r13`.

### 5. `GetActualRealRequiredSizeActually` glyph lookup

- Symbol: `_ZNK11CBitmapFont33GetActualRealRequiredSizeActuallyERK7CStringRS0_iiR8CVector2IjEbbbPiPb`
  (bound `0xcce`); pattern `0F B6 00 49 8B AC C7 00 01 00 00 48 85 ED`;
  overwrite 11, continuation `+11`.
- Same shape as site 4 with counter `add dword ptr [rsp+0x8], 2` and
  replay `mov rbp, [r15+rax*8+0x100]`.
- Clobbers: `rax`, stack counter slot (+2, intended), flags.
  Preserves: `rdx`, `r15`.

### 6. `GetActualRequiredSize` wrapping gate (raw, no hook)

- Same symbol/bound as site 3; pattern
  `0F BF 45 06 0F 57 C9 … 0F 2E 54 24 34`; expected `0F BF 45 06`
  → replacement `EB 15 90 90` (short jump forcing the wrap branch,
  fixing extra blank lines in popups).

## Atomicity

All six install as one `PatchBatch` group after every return/bypass slot
is published. Jump payloads are 5-byte `E9 rel32`; bytes after the jump up
to the calibrated overwrite width are unreachable dead bytes, never
fall-through code. Failed installs release staged trampolines, clear
published slots, and mutate nothing.

## Truncated-escape guard (crash fix)

Every decoder arm checks both payload bytes for NUL before the word read:

```asm
cmp byte ptr [rdx + 1], 0
je 7f            ; truncated escape -> plain-byte fallback
cmp byte ptr [rdx + 2], 0
je 7f
movzx reg, word ptr [rdx + 1]
```

Rationale: a marker at the end of a string would otherwise feed NUL-adjacent
bytes into the index, producing a wild glyph-table load downstream
(SIGSEGV observed in `GetActualRequiredSize` via map-name generation once
localization started feeding newly-converted content). The fallback treats
the marker as a plain byte, matching stock handling; well-formed input never
touches these branches.
