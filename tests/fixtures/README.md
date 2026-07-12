# Binary fixture status

No redistributable EU4 executable fixture is present in this repository.
`eu4dll_macos_target_profile_tests` therefore uses a synthetic Mach-O byte
buffer to verify target-check ordering, unique pattern handling, original-byte
validation, symbol failures, actionable check names, and the guarantee that
validation performs no mutation.

The synthetic fixture alone is not evidence that the selected patterns are
unique in a real EU4 1.37.x macOS executable. Real-binary probing is required
to confirm every patch contract and required symbol.

`eu4dll_real_binary_probe /path/to/eu4` performs a read-only check of Mach-O
format, x86-64 architecture, EU4 1.37 major/minor version, all 55 patch
contracts, and all 16 required symbols without adding the executable to the
repository. On the current host, EU4 1.37.4.0 and 1.37.5.0 both pass the same
canonical capability registry. This does not by itself validate live hook ABI,
continuations, IME behavior, or rendering inside a launched process.
