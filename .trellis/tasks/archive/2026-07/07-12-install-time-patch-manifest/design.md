# Design

## Two-phase installation

1. `eu4dll_manifest_tool` opens the selected EU4 Mach-O file read-only.
2. It parses segments, LC_UUID, architecture, and the 1.37.x version marker.
3. It resolves every canonical patch descriptor and required symbol.
4. It emits a temporary manifest only when the complete set passes.
5. `install.sh` atomically moves the completed manifest into the app resources.

No game executable bytes are changed by manifest generation.

## Manifest contract

Use a versioned deterministic format. Each entry contains a stable patch ID,
image-relative RVA, mutation/expected-byte offsets, expected bytes, overwrite
width, continuation/bypass RVAs where applicable, and optimizer policy. The
header contains schema version, descriptor-set version, Mach-O UUID,
architecture, parsed EU4 1.37.x version, and entry count.

The format must reject unknown required fields, duplicates, integer overflow,
out-of-segment RVAs, and inconsistent lengths. Absolute ASLR addresses and
storefront identifiers are forbidden.

## Runtime flow

The dylib reads the manifest before any mutation, validates header identity,
maps each RVA using the loaded image slide, and compares every short expected
byte span. Only after the complete validation succeeds does it apply mutations.
The apply phase reuses validated addresses and does no pattern scan.

## Failure model

Any manifest or byte mismatch returns one structured startup diagnostic with
the failed patch ID/check and a clear instruction to rerun the installer. There
is no partial apply. A release build does not silently fall back to live scans.

## Compatibility

Compatibility is capability-based within EU4 1.37.x. Steam/GOG integration
symbols may differ, but only canonical patch sites and required game symbols
affect the manifest. Both real local executables are verification inputs, not
redistributable fixtures or hard-coded allowlists.
