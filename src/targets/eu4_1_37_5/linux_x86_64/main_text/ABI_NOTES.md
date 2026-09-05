# Linux `main_text` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

Three `CBitmapFont::RenderToScreen` hooks (preprocessing, wrapping check,
drawing lookup). No tooltip/map/input facts here. Depends on the verified
base + textLayout stack.

## Shared vs target-owned policy

- Escape markers/shifts reuse the portable `escaped_text` constants;
  the extended-index shift is the Linux `0x6AC` fact (not macOS `0x6B0`).
- Staging buffer `0x3345191` is an absolute game-data address. The target
  is a non-PIE executable, so the address is stable; inside our shared
  object it is only ever used as `[reg + disp32]`, which stays
  position-independent.
- Drawing base is `[rbx + 0x333D450]` (object-relative, not absolute).

## Shared hook state

`g_linuxRenderToScreenCurrentCharacter` (u32) is written by the
preprocessing hook and read by the wrapping hook within one character
iteration. Assumption: the wrapping check for character N always runs
after the preprocessing publish for character N on the same thread, with
no re-entrant RenderToScreen between them. This matches the calibrated
legacy behavior; any future re-entrancy finding must revisit this slot
(the mapText skipped-byte state is the known-bad shape of the same idea
and must not be copied).

## Site contracts

### 1. Preprocessing lookup

- Symbol: `_ZN11CBitmapFont14RenderToScreen…` (bound `0x218b`); pattern
  `41 0F B6 0C 2E 80 BC 24 18 22 00 00 00 74 4F`; expected first 5 bytes;
  overwrite 5, continuation `+5` (`return`), bypass `+0x5e` (`bypass`).
- Entry: `r14` = string base, `rbp` = index, `ebx` = output slot
  (overwritten `movzx ecx, [r14+rbp]`… precisely `41 0F B6 0C 2E`).
- Hook: `rdx = r14+rbp`; decode marker byte into `ecx`; then
  `push rax; movsxd rax, ebx; dx = [r14+rbp+1]; [rax+0x3345191] = dx;
  pop rax` (stages the raw two bytes for the drawing loop);
  `r13d += 2; r12d += 2` (source/output counters); `ecx >= 256` gains
  `+0x6AC`; publish `ecx` to the current-character slot.
- Escape path → `bypass`; plain path (`7:`) → `return`.
- Clobbers: `ecx`/`dx` (intended), `r13d`/`r12d` (+2), flags.
  Preserves: `rax` (push/pop), `rbx`, `r14`, `rbp`.

### 2. Wrapping check

- Same symbol/bound; pattern
  `66 83 7D 06 00 0F 84 ? ? ? ? 80 3D ? ? ? ? 00`; expected first 5;
  overwrite 5, continuation `+5`, bypass `+0x191`.
- Hook: if current-character `> 0xFF` (CJK) → `bypass` (skip the
  Latin-only break check); else replay `cmp word ptr [rbp+0x6], 0` and
  take `return`. No pushes, no calls.
- Clobbers: flags only.

### 3. Drawing lookup

- Same symbol/bound; pattern
  `0F B6 83 50 D4 33 03 80 BC 24 18 22 00 00 00 0F 84`; expected first 7;
  overwrite 7, continuation `+7`, bypass `+0x1AA`.
- Entry: draw object in `rbx`, index in `r15` (overwritten
  `movzx eax, [rbx+0x333D450]`).
- Hook: `rdx = rbx+0x333D450`; decode into `eax`; `r15d += 2`;
  `eax >= 256` gains `+0x6AC`; escape path → `bypass`, plain path → `return`.
- Clobbers: `eax`/`edx` (intended), `r15d` (+2), flags.
  Preserves: `rbx`.

## Atomicity

All three install as one `PatchBatch` group after all six slots are
published. Jump payloads are 5-byte `E9 rel32` (drawing site: 5 + 2 dead
bytes); failed installs release staged trampolines, clear published slots
(including the current-character slot), and mutate nothing.
