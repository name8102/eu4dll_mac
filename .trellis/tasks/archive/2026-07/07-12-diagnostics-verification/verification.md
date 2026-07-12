# Verification

## Result

The diagnostics and compatibility work satisfies the child-task acceptance
criteria. The supported public capability target is
`eu4-1.37-macos-x86_64`: EU4 is x86-64, and publisher/storefront labels are not
binary-profile inputs. Both locally installed EU4 1.37.4.0 and 1.37.5.0
executables pass the same canonical 55-site patch registry and 16 required
symbols without mutation.

## Implementation evidence

- Startup validation checks Mach-O, x86-64, EU4 1.37 major/minor, all 55 patch
  contracts, and all 16 symbols before the first mutation.
- Startup failures use one actionable report with structured feature, target,
  operation, match, mutation, and failure information. Required-symbol failures
  are accumulated; manifest site validation is fail-fast and reports the first
  invalid site before any mutation.
- Target hook symbols and continuation storage are localized under the macOS
  x86-64 target adapter.
- Obsolete `global.h`, `memoryHelper.*`, `strConvert.*`, `library.h`, and the
  dead `hello()` export are removed.
- Legacy base/input installers use `PatchRuntime`; overwritten spans include
  full relative CALL/JMP bytes through masked expected-byte contracts.
- The public compatibility registry contains 55 stable IDs:
  base 5, input 5, localization 8, save 9, rendering 28.
- The historical internal directory/namespace `eu4_1_37_5` remains unchanged;
  it is not the public support-policy identifier.

## Automated verification

All builds used the locally cached pinned cpp-pinyin 1.0.2 source. Final clean
build directories were `/private/tmp/eu4dll-release-reviewed`,
`/private/tmp/eu4dll-strict-reviewed`, and
`/private/tmp/eu4dll-sanitize-reviewed`; older workspace build directories are
not the evidence for the counts below.

| Check | Result |
|---|---|
| Clean Release configure/build | passed |
| Release CTest | 16/16 passed |
| C++ `-Wall -Wextra -Werror` build | passed; pinned third-party warnings remain non-fatal |
| Strict CTest | 16/16 passed |
| ASan + UBSan CTest | clean rebuild, 16/16 passed |
| `bash -n install.sh` | passed |
| GitHub workflow YAML parse | passed |
| `git diff --check HEAD` | passed |
| Dead export/name scan | no `hello`, `ExecutionTracker`, `FuncGuard`, `memoryHelper`, or `strConvert` export/reference |

The strict build exposed missing explicit initialization for the new
`ExpectedBytes::mask` field in base/input callers and patch-runtime tests. Those
initializers were corrected and the complete strict suite then passed.

## Real executable matrix

Read-only command:

```sh
eu4dll_real_binary_probe /path/to/eu4
```

| Executable | Format / arch | minos / SDK | Patch sites | Symbols | Result |
|---|---|---|---:|---:|---|
| EU4 1.37.4.0 Inca | Mach-O x86-64 | 11.0 / 11.3 | 55/55 | 16/16 | passed |
| EU4 1.37.5.0 Inca | Mach-O x86-64 | 11.0 / 11.3 | 55/55 | 16/16 | passed |

Observed reports:

```text
target=eu4-1.37-macos-x86_64 check=capability-preflight.complete version=EU4 v1.37.4.0 Inca sites=55 symbols=16
target=eu4-1.37-macos-x86_64 check=capability-preflight.complete version=EU4 v1.37.5.0 Inca sites=55 symbols=16
```

File-probe benchmark, one warmup plus ten measured runs:

| Version | min | median | p95 / max |
|---|---:|---:|---:|
| 1.37.4 | 1258.036 ms | 1267.196 ms | 1277.677 ms |
| 1.37.5 | 1268.612 ms | 1292.393 ms | 1335.107 ms |

These timings measure complete file probing, not separately instrumented
in-process startup preflight.

## Artifact and resource verification

- `libeu4dll_mac.dylib`: Mach-O 64-bit shared library, x86-64 only,
  `LC_BUILD_VERSION minos 11.0`.
- `insert_dylib`: Mach-O 64-bit executable, x86-64 only,
  `LC_BUILD_VERSION minos 11.0`.
- The dylib links CoreFoundation, libc++, and libSystem; no unexpected local
  dylib dependency is present.
- `chinese_dict` is copied beside the dylib with 12 files in the Mandarin and
  Cantonese dictionaries, total size 648K.

## Child acceptance matrix

| Criterion | Evidence |
|---|---|
| One actionable consolidated startup diagnostic | startup diagnostics tests and formatted aggregate report |
| Replaced tracking/installation paths removed | dead-name scan and deletion list above |
| No unrelated global policy header | `src/global.h` deleted; target facts live under `src/targets` |
| Portable and fixture tests pass | Release and strict CTest 16/16 |
| Clean macOS configure/build | clean Release build passed |
| Pinyin resources present | 12 files, 648K beside artifact |
| No unsupported Windows/Linux claims | docs describe extension points, not runtime support |
| Remaining runtime gaps recorded | section below |
| Parent acceptance reconciled | matrix below |

## Parent acceptance reconciliation

| Parent criterion | Status |
|---|---|
| Feature/runtime/platform/target/bootstrap locations reflected in CMake | satisfied |
| macOS x86-64 EU4 1.37.x dylib builds | satisfied |
| Startup target validation and actionable failures | satisfied |
| Deterministic patch fixtures without launching EU4 | satisfied |
| Escaped-text focused tests | satisfied |
| Localization and name-ordering focused tests | satisfied |
| Normalized macOS composition adapter | satisfied |
| Windows/Linux extension points without support claims | satisfied |
| Target hooks own ABI/layout rather than portable policy | satisfied within documented unavoidable naked-hook boundaries |
| Portable features exclude platform/game-layout headers | satisfied by build-graph tests and source audit |
| Consolidated installation failures | satisfied |
| Build/test commands documented and passing | satisfied |
| Remaining platform verification gaps listed | satisfied below |

## Remaining runtime evidence gaps

The real-binary matrix proves file format, architecture, version series,
pattern uniqueness, expected overwritten bytes, and symbol availability. It
does not prove behavior after code injection. A launched in-process run remains
required to verify:

- all rendering paths visually and their live continuation/control flow;
- CJK IME start/update/commit/cancel, selection, cursor, and backspace behavior;
- save-name conversion and UI/disk ownership behavior;
- localization loading, pinyin search, and East Asian name ordering;
- consolidated user-visible diagnostics on an intentionally incompatible or
  modified executable.
- Manifest validation currently reports the first invalid patch site rather
  than enumerating every invalid site. This preserves zero-mutation safety and
  one actionable report, but a future diagnostic enhancement could collect all
  invalid site records in a single pass.
