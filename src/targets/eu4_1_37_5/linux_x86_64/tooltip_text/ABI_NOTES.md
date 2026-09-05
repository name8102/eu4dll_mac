# Linux `tooltip_text` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

Three `CBitmapFont::RenderToTexture` hooks (preprocessing, wrapping check,
drawing lookup). Tooltip/button rendering path; separate from mainText so a
hot tooltip crash bisects to this group. No map/input facts here.

## Shared vs target-owned policy

- Escape markers/shifts reuse the portable `escaped_text` constants;
  extended-index shift is the Linux `0x6AC` fact.
- `CString::operator+=(char)` (`_ZN7CStringpLEc`) is resolved through
  `Memory::ResolveSymbol` at install (fail-closed) and stored in an
  indirect slot; the hook calls it via `call qword ptr [rip + slot]`.
- Glyph-table base `0x100` (Linux, not macOS `0xE8`).

## Shared hook state

`g_linuxRenderToTextureCurrentCharacter` (u32): preprocessing publishes,
wrapping reads within one character iteration — same assumption shape as
the mainText slot (no re-entrant RenderToTexture between publish and
check). Documented here rather than hidden.

## System V AMD64 call audit (preprocessing hook only)

The preprocessing hook is the only naked hook in the Linux port so far
that calls a C++ function. Audit:

- Entry subtitle: the hook is entered by `jmp` (not `call`), so `rsp`
  keeps whatever alignment the game had at the patch site.
- `push rax; push rdx` subtracts 16, **preserving** the incoming
  `rsp % 16`. The two `call`s therefore observe the same alignment the
  game site had. Calibrated live across tooltip paths; any future
  realignment fault would manifest at the `call`, not silently.
- `rdi`/`rsi` are rebuilt before each call (`lea rdi, [rsp+0x68]` points
  at the game's live `CString` precisely because the `+0x68` accounts for
  the two pushes; `esi` is reloaded from `[rdx+1]`/`[rdx+2]` with `rdx`
  restored from its stack slot between the calls).
- Caller-saved state across the calls: `rax`/`rdx` are explicitly pushed
  and popped; `rcx/rsi/rdi/r8-r11` are either rebuilt or dead at that
  point (`r14d` counter update happens after the pops; `eax` index math
  happens after the pops). Flags are re-tested after the pops
  (`cmp eax, 256`).
- No XMM values are live across the calls (integer/pointer loop only).
- No red-zone reliance: all scratch addressing is `rsp`-relative after
  the pushes, and the 128-byte red zone below the entry `rsp` is never
  named.

## Site contracts

### 1. Preprocessing lookup

- Symbol: `_ZN11CBitmapFont15RenderToTexture…` (bound `0x311c`); pattern
  `0F B6 00 48 8B 4C 24 38 48 8B AC C1 00 01 00 00 48 85 ED`;
  expected first 8 bytes; overwrite 8, continuation `+16` (`return` only).
- Entry: `rax` = string byte pointer (overwritten `movzx eax, [rax]`).
- Hook: `rdx = rax`; decode into `eax`; stage raw bytes 1..2 into the
  live `CString` at `[rsp+0x68]` via two `CString::AppendChar` calls;
  `r14d += 2`; `eax >= 256` gains `+0x6AC`; publish current character;
  replay `mov rcx, [rsp+0x38]; mov rbp, [rcx+rax*8+0x100]`.
- Escape (`5:`) and plain (`7:`) paths both rejoin at `return`.
- Clobbers: `eax`/`edx` (intended), `r14d` (+2), `rbp` (replayed),
  flags, plus call-clobbered GPRs rebuilt as audited above.
  Preserves: `rax`/`rdx` (push/pop), `r14` (except counter), `rbx`.

### 2. Wrapping check

- Same symbol/bound; pattern `66 83 7D 06 00 74 0D 40 8A AC 24 00 29 00 00`;
  expected first 5; overwrite 5, continuation `+5`, bypass `+0x14`.
- Hook: current-character `> 0xFF` (CJK) → `bypass`; else replay
  `cmp word ptr [rbp+0x6], 0` and take `return`. No pushes, no calls.
- Clobbers: flags only.

### 3. Drawing lookup

- Same symbol/bound; pattern
  `0F B6 00 4D 8B 9C C5 00 01 00 00 4D 85 DB`; expected first 11;
  overwrite 11, continuation `+11` (`return` only).
- Entry: `rax` = string byte pointer (overwritten `movzx eax, [rax]`).
- Hook: `rdx = rax`; decode into `eax`; `r14d += 2`;
  `eax >= 256` gains `+0x6AC`; replay
  `mov r11, [r13+rax*8+0x100]`. Both paths rejoin at `return`.
- Clobbers: `eax`/`edx` (intended), `r14d` (+2), `r11` (replayed),
  flags. Preserves: `rax`, `r13`.

## Atomicity

All three install as one `PatchBatch` group after the callee address and
every slot are published. Jump payloads are 5-byte `E9 rel32` (dead bytes
after the jump are unreachable). Failed installs release staged
trampolines, clear published slots (including callee + current character),
and mutate nothing.
