# Rendering hook ABI notes

The repeated escaped-glyph decode sequences in `mainText.cpp`,
`tooltipAndButtonText.cpp`, `textLayout.cpp`, `mapText.cpp`, and `text3D.cpp`
are intentionally retained in the macOS x86-64 target hooks.

Each naked hook enters at a different point in an EU4 function and owns a
different live-register and stack-frame contract. Calling a normal C++ decoder
there would require saving caller- and callee-saved registers, preserving SIMD
state and stack alignment, and reconstructing the overwritten instruction
sequence. That would change the target ABI and is not safe without executable
fixtures and in-process validation.

The hooks no longer own independent constants: marker bytes, marker shifts,
and the glyph offset come from `features/escaped_text`. C++ rendering paths,
including map logical-length measurement, call `features/text_rendering`, which
delegates traversal to the shared escaped-text module. The remaining assembly
is therefore register/calling-convention adaptation, not a second portable
policy surface.

The complete rendering installation contract is a 28-site inventory in
`rendering_contract.cpp`. See `VERIFICATION.md` for contract coverage, naked
hook machine-code equivalence, and the unavailable real-binary checks.
