# Port Linux text layout hooks

## Dependency
`09-05-linux-port-foundation`.

## Requirements
- Re-extract every Linux pattern, symbol bound, expected span, continuation, register, stack offset, and layout constant from `src/platform/linux/text_layout_probe.cpp`; compare semantics with canonical `src/textLayout.cpp`.
- Keep naked/register-specific hooks in `linux_x86_64`; reuse portable text policy.
- Use `PatchDescription`/`PatchBatch`, not the old scanner.
- Give every naked hook an explicit SysV entry/clobber/preserve/continuation contract.
- Keep textLayout separately gateable.
- Real-game gate with base+textLayout only: menus/campaign load, Latin/CJK strings, wrapping/size-sensitive UI, time advance, short soak.
- Stop on instability before mainText. Add tests only for non-game-observable failure states.
