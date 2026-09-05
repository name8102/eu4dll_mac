# Finalize Linux feature parity and stability

## Dependencies
All Linux migration children plus the stable foundation.

## Requirements
- Build a parity matrix: canonical root/features vs linux_x86_64 implementation vs legacy prototype; account for every difference.
- Ensure bootstrap ordering/gates support binary-search diagnosis; no feature silently enables a dependent layer.
- Audit process-lifetime trampoline/resource ownership and normal shutdown with the full hook set.
- Long real-game soak: menu, campaign load/save, UI/tooltips/events, localization, map names/curves, text3D, input/IME, clipboard, pinyin search, time advance, map-mode churn and normal exit.
- Investigate any delayed crash before declaring parity, especially legacy map-text regression history.
- Produce final migration documentation. Remove obsolete prototype-only scaffolding only after canonical behavior is equivalent and verified.
- Tests only for failures ordinary game testing cannot reliably expose.
