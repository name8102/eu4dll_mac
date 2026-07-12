# macOS build and runtime baseline

Captured on 2026-07-12 before the modular build-skeleton change, at commit
`735cab01c1b80246fb533941ec7baddc3ebac350`.

## Configure and build

Environment:

- macOS host on arm64, cross-building the configured x86-64 target;
- CMake 4.1.2;
- AppleClang 21.0.0;
- C++17 with position-independent code;
- cpp-pinyin 1.0.2 fetched by CMake and built as a static library.

Commands:

```sh
cmake -S . -B <build-dir> -DCMAKE_BUILD_TYPE=Release
cmake --build <build-dir> --parallel 4
```

The first configure attempt could not resolve GitHub in the restricted build
environment. Re-running with network access fetched the pinned cpp-pinyin
source and the unmodified build then completed successfully.

## Generated artifacts and dependencies

- Final library: `libeu4dll_mac.dylib`.
- Architecture: Mach-O 64-bit dynamically linked shared library, x86-64.
- Install name: `@rpath/libeu4dll_mac.dylib`.
- Direct dynamic dependencies observed with `otool -L`:
  - CoreFoundation;
  - libc++;
  - libSystem.
- Release baseline size: 265,880 bytes. Size is diagnostic, not a compatibility
  contract.
- Externally defined symbols: 291. The SHA-256 of sorted `nm -gjU` output was
  `a21721ed76ef2313baf3ae50cdf2e8619c715d7dd15f560e46abade4964d78ac`.
- The `insert_dylib` tool also built successfully at 23,296 bytes.

## Resource copy and installation

The final library's post-build command copies cpp-pinyin's `res/dict` tree to
`<target-directory>/chinese_dict`. The baseline copy contained twelve files and
used approximately 648 KiB.

The installer expects `libeu4dll_mac.dylib`, `insert_dylib`, and optionally
`chinese_dict` beside `install.sh`. It:

1. copies the dynamic library to `eu4.app/Contents/Frameworks`;
2. copies the dictionary to `eu4.app/Contents/Resources` when selected;
3. injects `@executable_path/../Frameworks/libeu4dll_mac.dylib` into the EU4
   executable;
4. updates the app configuration and re-signs the app.

## Startup order

The dynamic-library constructor remains the startup entry point. Its baseline
sequence is:

1. scan the main image for the `EU4 v1` text;
2. stop when the host is not EU4;
3. accept strings beginning with `EU4 v1.37` and report other versions;
4. install `base`;
5. install `textLayout`;
6. install `mainText`;
7. install `tooltipAndButtonText`;
8. install `mapText`;
9. install `text3D`;
10. install `saveFileName`;
11. install `input`;
12. install `localization`;
13. consolidate tracked patch and symbol-resolution failures into the existing
    CoreFoundation alert.

The broad version-prefix behavior is recorded, not corrected by this task.
Exact target validation belongs to the later macOS target-profile task.

## Known limitations

- No EU4 executable fixture is present, so constructor execution, live pattern
  matches, memory mutation, hook ABI, and in-game behavior cannot be automated
  locally.
- There are no pre-existing unit or fixture tests for feature behavior.
- The current source mixes feature policy, patch mechanics, Mach APIs, and EU4
  layout/ABI facts. This task creates build boundaries only; later child tasks
  perform the behavior-covered migrations.
- Only the macOS x86-64 EU4 1.37.5 target is in scope. Storefront does not define
  a different binary profile. This baseline makes no
  Windows or Linux support claim.

## Post-skeleton preservation check

After introducing the module targets, a fresh Release configuration reused the
already fetched cpp-pinyin 1.0.2 source and completed without network access.
The following targets were independently visible in the generated build graph:

- `eu4dll_portable_features`;
- `eu4dll_runtime`;
- `eu4dll_platform_macos`;
- `eu4dll_target_eu4_1_37_5_macos_x86_64`;
- `eu4dll_bootstrap_macos`;
- `eu4dll_mac`;
- `eu4dll_build_skeleton_tests` and `eu4dll_tests`.

Preservation evidence:

- the final filename, x86-64 Mach-O architecture, install name, and dynamic
  dependency list matched the baseline;
- the sorted exported-symbol hash and count matched exactly (291 symbols and
  the SHA-256 recorded above);
- the post-build dictionary copy still contained twelve files;
- the unchanged `library.cpp` constructor preserves startup and diagnostics
  ordering;
- the final dylib was 265,888 bytes, eight bytes larger than the diagnostic
  baseline size, with no exported-symbol or linked-dependency difference;
- the build-skeleton checks passed. Configure-time assertions verify the module
  targets, selected object libraries, and final link dependencies; the CTest
  build-graph check rebuilds the final library and test executable. These are
  not a substitute for the later behavior and fixture tests.
