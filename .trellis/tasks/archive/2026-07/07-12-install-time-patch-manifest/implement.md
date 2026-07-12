# Implementation Plan

1. Define and test the manifest schema and deterministic serializer/parser.
2. Extract reusable read-only Mach-O file mapping, UUID, segment, symbol, and
   RVA translation support.
3. Build the install-time scanner from canonical patch descriptors.
4. Verify full two-version generation and failure atomicity.
5. Add runtime manifest loading, identity checks, expected-byte preflight, and
   cached-RVA apply with ASLR slide.
6. Integrate the tool and atomic manifest placement into `install.sh`.
7. Remove release live-scan fallback after parity tests pass.
8. Benchmark install-time scanning and startup validation.
9. Run clean build, sanitizer, corruption/stale-update tests, and artifact audit.
