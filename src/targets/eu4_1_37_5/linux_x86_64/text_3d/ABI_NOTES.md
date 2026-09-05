# Linux `text_3d` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope

Two `CBitmapFont::Render3d` hooks (preprocessing + drawing) for 3D unit
labels and floating text. Kept separate from mapText so a 3D-text fault
attributes to this group, per the migration plan.

## Shared vs target-owned policy

- Escape markers/shifts reuse the portable `escaped_text` constants;
  extended-index shift is the Linux `0x6AC` fact; table base `0x100`.
- `CString::operator+=(char)` resolves through `Memory::ResolveSymbol`
  at install (fail-closed).

## Site contracts

### 1. Preprocessing lookup

- Symbol: `_ZN11CBitmapFont8Render3d…` (bound `0x1000`); pattern
  `0F B6 00 49 8B 9C C4 00 01 00 00 48 85 DB`; expected 11 bytes;
  overwrite 11, continuation `+11` (`return` only).
- Entry: `rax` = string byte pointer.
- Hook: `rdx = rax`; guarded decode into `eax`; stage raw payload bytes
  via two `CString::AppendChar` calls into `[rsp+0x68]` (same audited
  push/call shape as the tooltip/FillVertexBuffer preprocessing hooks:
  `push rax; push rdx`, per-call `rdi`/`rsi` rebuild, pops restore);
  `r15d += 2`; `+0x6AC` rule; replay
  `mov rbx, [r12+rax*8+0x100]`.
- Escape (`5:`) and plain (`7:`) paths both rejoin at `return`.
- Clobbers: `eax`/`edx` (intended), `r15d` (+2), `rbx` (replayed),
  flags, plus call-clobbered GPRs rebuilt as audited.
  Preserves: `rax`/`rdx` (push/pop), `r12`.

### 2. Drawing lookup

- Same symbol/bound; pattern
  `0F B6 00 49 8B 84 C4 00 01 00 00 48 85 C0`; expected 11 bytes;
  overwrite 11, continuation `+11` (`return` only).
- Hook: `rdx = rax`; guarded decode into `eax`; `r15d += 2`;
  `+0x6AC` rule; replay `mov rax, [r12+rax*8+0x100]`.
- Clobbers: `eax`/`edx` (intended), `r15d` (+2), `rax` (replayed),
  flags. Preserves: `r12`.

## Truncated-escape guards

Both decoder arms NUL-check payload bytes before the word read (same
rule as all other Linux decoder hooks after the GetActualRequiredSize
crash fix).

## Atomicity

Both install as one `PatchBatch` group after the callee and both slots
publish. Jump payloads are 5-byte `E9 rel32` (dead bytes unreachable).
Failed installs release staged trampolines, clear all slots, and mutate
nothing.
