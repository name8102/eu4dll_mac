# Generate install-time patch manifest

## Goal

Perform expensive EU4 1.37.x compatibility scanning once when the dylib is
installed, then install hooks at game startup from a cached storefront-neutral
RVA manifest with only cheap identity and original-byte validation.

## Requirements

- Add an install-time preflight executable that scans the selected Mach-O
  x86-64 EU4 executable using the same canonical patch descriptors as runtime.
- Accept compatible EU4 1.37.x executables by capability; do not key the
  manifest or profile to Steam, GOG, publisher, or an exact patch allowlist.
- Require every active patch pattern to be unique and every overwritten byte,
  width, continuation, bypass, optimizer, and required symbol contract to be
  resolved before producing a manifest.
- Store image-relative virtual addresses/RVAs, never process absolute addresses.
- Bind the manifest to the Mach-O LC_UUID, parsed 1.37.x version, architecture,
  schema version, and canonical descriptor-set version.
- Write the manifest atomically into the installed app resources only after
  the full preflight succeeds.
- At game startup, load the manifest, validate UUID/schema/version and the short
  expected-byte span at every cached site, then apply patches using the current
  ASLR slide without repeating pattern scans.
- Missing, stale, malformed, or mismatched manifests must cause zero mutation
  and one actionable instruction to rerun the installer.
- A game update must invalidate the old manifest safely.
- Keep a development-only explicit fallback if needed; release startup must not
  silently return to expensive scanning.

## Acceptance Criteria

- [ ] The installer produces a manifest for both available compatible 1.37.4
      and 1.37.5 x86-64 executables without storefront-specific profile logic.
- [ ] Manifest generation fails atomically on ambiguous/missing patterns,
      original-byte mismatch, missing symbols, or unsupported architecture.
- [ ] The manifest contains no absolute process addresses and survives ASLR.
- [ ] Runtime startup performs no full-image pattern scan when a valid manifest
      is present.
- [ ] UUID, schema, descriptor version, or expected-byte mismatch yields zero
      mutation and a reinstall diagnostic.
- [ ] All active hook sites are applied from cached RVAs using the canonical
      descriptor source.
- [ ] Install-time scan and startup validation benchmarks are recorded; startup
      validation is materially cheaper than full scanning.
- [ ] Manifest parser/writer, Mach-O mapping, corruption, stale-update, ASLR,
      and two-phase no-mutation behavior have deterministic tests.
- [ ] `install.sh` invokes preflight before copying the final manifest and
      remains distribution-channel neutral.

## Constraints

- Do not copy or commit proprietary EU4 executables or byte dumps beyond the
  minimal contract fixtures already allowed by the project.
- Do not weaken short expected-byte validation at runtime merely to reduce
  startup cost.
- Preserve the existing dylib filename and installation layout unless the
  design documents an intentional compatible addition.
