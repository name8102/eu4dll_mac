# Input-method adapter requirements

The macOS adapter consumes the current SDL text-editing and text-input capture
path. It owns whether a native composition is active and copies native buffers
into normalized, owned UTF-8 event values. SDL/native objects must not cross
into `features/text_input`.

A platform conformance suite should feed start/update/commit/cancel,
selection, logical cursor, and backspace events through an adapter and verify
the same normalized event sequence and owned lifetimes.

Windows requires a real adapter around the selected windowing path and TSF or
IMM composition lifecycle, including UTF-16 conversion, candidate/pre-edit
state, selection, commit, cancellation, and focus loss. Linux requires a real
adapter for the selected SDL/IBus/Fcitx path with equivalent lifecycle and
UTF-8 ownership. Neither adapter exists or is supported by this repository.

The repository verifies the fixed-capacity 0x302 snapshot, transition-only
drain sequence, owned payload lifetime, normalized portable application, target
byte-injection ordering, and logical cursor/backspace call counts with
deterministic tests. The production 0x302 hook only captures POD state; owned
events are created at a C++ safe boundary.

Runtime verification still required in a real macOS x86-64 EU4 1.37.x
process: start a CJK composition, observe successive pre-edit updates, commit
multiple characters into mixed ASCII/CJK text and an active selection, cancel
a composition, then move and backspace across committed multi-byte characters.
EU4 1.37.4.0 and 1.37.5.0 both pass the same 55-site/16-symbol read-only
capability preflight. That validates their patch contracts but not the live IME
path, so unit, sanitizer, binary-probe, and dylib build success are not a
live-integration claim. Publisher/storefront labels do not change the binary.
