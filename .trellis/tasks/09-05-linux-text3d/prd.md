# Port Linux 3D text rendering

## Dependency
`09-05-linux-map-text` with its soak gate passed.

## Requirements
- Extract Linux text3D target facts from `text3d_probe.cpp`; compare canonical `src/text3D.cpp` semantics.
- Implement through linux_x86_64 adapters/shared runtime; audit naked call sites for full SysV call ABI correctness.
- Real-game gate: all known 3D text surfaces, map interaction, campaign load, time advance, and combined mapText+text3D soak.
- Do not fold unrelated map fixes into this task.
