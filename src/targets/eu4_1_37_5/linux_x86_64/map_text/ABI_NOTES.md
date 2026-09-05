# Linux `map_text` ABI notes (EU4 1.37.5 linux_x86_64)

## Scope and risk posture

Highest-risk migration stage: map labels, curved country names, and the
`FillVertexBuffer` vertex path. Four clusters install sequentially behind
one gate so a fault always bisects to one cluster:

1. `fill-vertex-buffer` — preprocessing + drawing decoders.
2. `add-name-area` — spacing hook + final-byte append + escape-preserving ToUpper + glyph count.
3. `add-nudged-names` — glyph count.
4. `curve-text` — drawing decoder + three GetSize redirects.

Preflight validates every cluster before any commit. Commits run cluster
by cluster and stop at the first failure (earlier clusters stay installed
by design — bisection over all-or-nothing). Each cluster batch is itself
atomic with verified rollback.

Explicitly NOT ported: the reverted `CString::GetSize` allocation
experiment (shrinking the vertex allocation while vertex writing kept the
original layout faulted the stack in the legacy tree). The surviving
GetSize→glyph-count redirects in CurveText govern short-label orientation,
loop bounds and the interpolation special case. Vertex allocation happens
earlier in AddNameArea's drawable-glyph loop; none of these three calls
allocates the vertex buffer.

## CurveText skip state: stateless derivation (no counter at all)

The legacy `g_linuxCurveTextSkippedByteCount` process-global is gone and
was NOT replaced with thread-local storage either: the drawing hook
derives the skip on every invocation, so there is no shared state of any
kind to interleave, nest, or leak.

Per-call facts at the drawing site (all verified by disassembly):

- `rax` = current byte pointer = string data + `r14d` (the loop head
  re-runs `operator[](CString, r14d)` every iteration through the
  `0x1b5fbc1 -> 0x1b5f454` back edge; the legacy `rax += skipped`
  correction working live is the independent proof of this shape).
- `r14d` = glyph index (zeroed once by the original `xor r14d` which
  stays unpatched; 32-bit traffic keeps high bits zero).
- `CString*` is saved by the loop head to `[rbp-0x148]` (re-stored every
  iteration, never clobbered before the drawing site); `CString` data
  lives at object `+0` (proven by `GetSize: mov 0x8(%rdi),%eax` and
  `GetDouble: mov (%rdi),%rdi; jmp atof`).

The hook walks at most `r14d` glyphs from the base with the same
NUL-guarded escape rule as the pure `AdvanceLogicalChar` helper,
accumulates `+2` per escape, and applies `rax += skipped`. Malformed
tails degrade to plain counting (bounded, stock-equivalent); well-formed
input reproduces the legacy incremental counter exactly. Cost is O(n^2)
per label worst case, negligible for map-label lengths.

Preservation: five balanced pushes (`rax/rcx/rdx/rsi/rdi`, no calls so
alignment is free); `r14d` read-only; `rbp` stable; only `rax`/flags
(dead at the site) change. `rdx` reuse follows the legacy precedent
(dead at entry, clobbered freely).

Consequence: the loop-init site (`xor r14d; xor ebx`) needs NO hook —
the original instructions stay untouched, which is strictly less risk
than patching them.

## System V audits for hooks that call C/C++

### AddNameArea spacing hook

Calls `CString::operator+=(const char*)` once on the truncation path.
No pushes: `rdi`/`rsi` rebuilt (`[rbp-0x238]` destination,
`[rbp-0x88]` 3-byte scratch holding the current char/escape), incoming
`rsp % 16` preserved to the call. Clobbered GPRs are dead at the `final`
continuation (calibrated live). The escape-payload read is pre-guarded by
the game's own length check (`lea rax,[r12+2]; cmp rax,r15; jae`), so no
additional NUL guard is needed here — the one hook that already bounds
correctly. Slots `[rbp-0x88]`/`[rbp-0x87]` are the game's own char buffer
(the overwritten bytes wrote them), not foreign scratch.

### FillVertexBuffer preprocessing hook

Calls `CString::operator+=(char)` twice. `push rax; push rdx` (−16,
alignment-preserving); `rdi`/`rsi` rebuilt per call with `rdx` restored
from its stack slot between calls; pops restore; `r14d += 2` after the
pops. Same audited shape as the tooltip preprocessing hook.

### C++ bridges (called via redirected CALLs, standard ABI)

- `ToUpperPreservingEscapes(void*)`: same register convention as the
  replaced `CString::ToUpper` (`rdi` = text). The following game
  instruction is `xor eax,eax`, proving the return value is ignored —
  hence `void`. Skips two payload bytes after `0x10..0x13`, uppercases
  the rest with `std::toupper` on unsigned values (no negative-char UB).
- `CurveTextGetGlyphCount(const void*)`: same signature as
  `CString::GetSize` (`rdi` in, count in `eax`). All three CurveText call
  sites redirect to it. The bridge uses general registers only because
  the interpolation call has live XMM state.
- Callee/slot addresses resolve through `Memory::ResolveSymbol` at
  install (fail-closed); the five CString slots clear on any failure.

## Site contracts

### Cluster 1: FillVertexBuffer (`_ZNK11CBitmapFont16FillVertexBuffer…`, 0xb00)

- Preprocessing: pattern `0F B6 00 48 8B 4C 24 68 4C 8B BC C1 …`;
  expected first 16 bytes; overwrite 16, continuation `+16`.
  `rdx = rax`; decode into `eax` (guarded); stage raw payload bytes via
  two AppendChar calls into `[rsp+0x48]`; `r14d += 2`; `+0x6AC` rule;
  replay `mov rcx,[rsp+0x68]; mov r15,[rcx+rax*8+0x100]`.
- Drawing: pattern `0F B6 00 49 8B 84 C4 00 01 00 00 48 85 C0`;
  expected 11 bytes; overwrite 11, continuation `+11`. `add ebx, 2`
  counter; replay `mov rax,[r12+rax*8+0x100]`.

### Cluster 2: AddNameArea (`_ZN18CGenerateNamesWork11AddNameArea…`, 0x1250)

- Spacing: pattern `43 8A 44 25 00 88 85 78 FF FF FF 4C 89 F7 …`;
  expected 11 bytes; overwrite 11, continuation `+11`, final `+0x5c`
  (truncation path). Entry: `r13` = string base, `r12` = byte index,
  `r15` = LAST VALID BYTE INDEX (length-1, not length). Plain byte →
  store + `return`; escape disposition via `ClassifySpacingEscape`
  (unit-tested truth table): `r12+2<r15` copies payload and continues
  (`r12 += 2`), `r12+2==r15` copies the complete final payload then
  takes `final`, `r12+2>r15` skips the payload read (truncated) and
  takes `final` with the marker only. The old `jae`-only check dropped
  the last CJK character of map names; the three-way form preserves the
  legacy end-of-loop semantics. The helper is `general-regs-only` (no
  XMM, verified in disassembly); `r12/r13/r15/rbp` survive its call
  (callee-saved), `rdi/rsi` are saved/restored around it.
  Its return type is uint8_t: consume `AL` with `movzx ecx,al`, never
  the unspecified upper bits of EAX. The old EAX read could mistake the
  final-character result and continue beyond the label. An actual ELF
  spacing-loop harness reproduces this failure and validates the fix.
  The four-byte scratch is cleared on each iteration. The native final
  ASCII append at 0x1b5de81 uses a one-byte copy so the previous Han
  payload cannot remain attached to the final ASCII character.
- ToUpper call: pattern `E8 ? ? ? ? 31 C0 4C 8D 85 E8 FD FF FF`;
  expected `E8 25 F2 9E 00`; 5-byte CALL redirect (exact width).
- Glyph count: pattern `0F B6 00 49 8B 84 C4 00 01 00 00 48 85 C0`;
  expected 11 bytes; overwrite 11, continuation `+11`
  (`add ebx, 2`; replay `mov rax,[r12+rax*8+0x100]`).

### Cluster 3: AddNudgedNames (`_ZN22CCountryNameCollection14AddNudgedNames…`, 0x580)

- Same 11-byte glyph pattern/continuation as cluster 2's counter,
  scoped to its own symbol. One jump, one continuation.

### Cluster 4: CurveText (AddNameArea symbol, 0x2400 window; no dynsym)

- Drawing: pattern `0F B6 00 4D 8B 2C C4 4D 85 ED`; expected 7 bytes;
  overwrite 7, continuation `+7`. Derive skipped bytes by walking `r14d`
  logical characters from the base; advance `rax`, decode into `eax`
  (guarded), apply the `+0x6AC` rule and replay `mov r13,[r12+rax*8]`.
- Length calls: window pattern
  `E8 ? ? ? ? 41 89 C7 4C 89 E7 E8 ? ? ? ? 48 89 85 30 FF FF FF`;
  first call at match (`E8 C2 D0 9E 00`), second at match `+11`
  (`E8 B7 D0 9E 00`); both 5-byte CALL redirects to the glyph-count
  bridge. The first result (`r15d`) selects short-label orientation;
  the second determines loop bounds and the `count-1` denominator.
- Third count call at 0x1b5f54d: `E8 9C CE 9E 00`, followed by
  `F3 0F 10 35 ... 83 F8 02 7C 10`. Redirected to the same bridge.
  This call selects the single-character interpolation special case.
  Leaving it byte-based makes a single Han character take `0/(1-1)`,
  yielding NaN vertices and an invisible label. The real ELF probe shows
  old counts 1/1/3 and corrected counts 1/1/1.
- Loop initialization stays unpatched; there is no shared skipped counter.

## Slot lifetime vs unconfirmed rollback

Hook continuation/callee slots follow the same fail-safe model as
trampolines (`patch::MustRetainSlots`): a failed install clears slots
only when its `BatchResult::rollbackState` is `NotNeeded` or `Complete`.
On `Unconfirmed`, slots stay published because game code may still be
inside a hook whose trampoline was intentionally retained.

Per-cluster split: each cluster clears only its own slots on failure
(`ClearFillSlots` etc.). Shared CString callee slots resolve once in
`InstallMapText` and clear only when zero clusters installed; a later
cluster failure never clears an installed earlier cluster's slots.

## Truncated-escape guards

All decoder arms (including CurveText drawing) NUL-check both payload
bytes before the word read, falling back to the plain-byte path — same
rule as the layout/mainText/tooltip hooks after the GetActualRequiredSize
crash fix. The spacing hook needs none (game length check precedes its
payload read).
