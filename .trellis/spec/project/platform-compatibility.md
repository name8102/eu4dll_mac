# Platform Compatibility

## Supported game

- EU4 1.37.x is the supported game series.
- Do not build a general multi-version compatibility system.
- Validate executable capabilities at startup: Mach-O format, x86-64
  architecture, EU4 1.37 major/minor version, all patch-site contracts, and all
  required symbols must pass before mutation.
- EU4 targets are x86-64. Do not introduce profiles for other architectures.
- Storefront or publisher labels do not define a binary profile. GOG, Steam,
  and other distribution channels must not appear in profile IDs, directory
  names, validation checks, or support claims when their executable bytes are
  identical.

Example profile names:

    macos-x86_64
    windows-x86_64
    linux-x86_64

Target validation must use executable facts such as Mach-O/PE/ELF format,
x86-64 architecture, supported major/minor version text, stable byte
fingerprints, and required symbols. Patch/build/codename text and distribution
labels are not substitutes for capability preflight.

## Hook ABI

- Naked assembly and register adaptation are target-specific.
- Windows x64 and System V x86-64 calling conventions must not share hook
  assembly merely to reduce file count.
- Shared C++ behavior should sit behind thin target hook adapters.
- Every hook change records the matched bytes, overwritten length, return or
  bypass address calculation, and required symbol/object-layout facts.

## Input methods

Each operating system has a separate input-method adapter.

    native input method
        → platform IME adapter
        → normalized composition events
        → portable text-input module
        → EU4 target hook adapter
        → CTextBuffer

Platform adapters own native event capture, pre-edit text, candidate state,
commit, and cancellation. The portable text-input module owns escaped-text
conversion, logical cursor movement, deletion, and composition semantics.
Target hook adapters own EU4 addresses, ABI, object layout, and control flow.

Normalized behavior should cover composition start/update/commit/cancel,
committed UTF-8 text, selection, cursor movement, and backspace.

## Scenario: macOS deployment target and executable evidence

### 1. Scope / Trigger

- Apply when the Xcode SDK, CMake runner, supported EU4 executable, or release
  packaging changes. An unset deployment target inherits the build host and can
  silently produce a dylib that older EU4 hosts cannot load.

### 2. Signatures

- Configure: `cmake -S . -B <build> -DCMAKE_BUILD_TYPE=Release`
- Contract variable: `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`
- Read-only target probe: `eu4dll_real_binary_probe /path/to/eu4`

### 3. Contracts

- `libeu4dll_mac.dylib` and `insert_dylib` are Mach-O x86-64 only.
- Both artifacts declare `LC_BUILD_VERSION minos 11.0`; they must not inherit
  the current SDK default.
- The 11.0 baseline is declared by both locally verified EU4 1.37.4 and 1.37.5
  executables and enforced by linker checks.
- A binary outside EU4 1.37.x is negative rejection evidence only.

### 4. Validation & Error Matrix

- Deployment target empty -> packaging failure; set it explicitly.
- Artifact `minos` differs from 11.0 -> build verification failure.
- Probe reports a version outside 1.37.x -> record rejection, not support.
- Synthetic fixture passes -> patch-contract evidence only.
- Storefront differs but executable bytes match -> same target profile.

### 5. Good/Base/Bad Cases

- Good: an EU4 1.37.x executable passes all 55 patch sites and 16 symbols, then
  passes a launched integration run.
- Base: clean build, tests, x86-64, minos 11.0, wrong-series rejection, and
  read-only capability preflight pass; live runtime behavior remains open.
- Bad: use the host SDK default or accept a version marker without full
  capability preflight.

### 6. Tests Required

- Assert `file`/`lipo` report x86-64 for both artifacts.
- Assert `vtool -show-build` reports `minos 11.0`.
- Run the read-only probe on any available executable and preserve its exact
  version/result.
- Run the full registry probe on each available legal EU4 1.37.x binary and
  preserve the result; never commit those binaries.

### 7. Wrong vs Correct

#### Wrong

```cmake
# Unset: current SDK may raise minos without review.
set(CMAKE_OSX_ARCHITECTURES "x86_64")
```

#### Correct

```cmake
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build for x86_64 only" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Explicit macOS baseline" FORCE)
```

## Scenario: unsigned macOS game installation

### 1. Scope / Trigger

- Apply when changing `install.sh`, executable injection, backup placement, or
  release installation instructions for the verified GOG macOS App.

### 2. Signatures

- Install entry point: `EU4DLL_APP_PATH=/path/to/eu4.app install.sh`
- Explicit test overrides may provide `EU4DLL_CODESIGN` and `EU4DLL_XATTR`, but
  the installer contract is that neither command is invoked.

### 3. Contracts

- The verified EU4 1.37.4 GOG App and several bundled frameworks are unsigned.
- Install and uninstall preserve that signing state.
- Inject into a temporary executable, publish it atomically, and keep the
  original executable backup without signing either file.
- Release artifact signing in CI is a separate packaging concern; it does not
  authorize modifying the installed game's signatures or xattrs.

### 4. Validation & Error Matrix

- Installer invokes `codesign` or `xattr` -> integration-test failure.
- Manifest preflight fails -> zero App mutation.
- Injection/publish fails -> restore the original executable and owned files.
- Runtime rejects the manifest -> zero patch mutation and reinstall guidance.

### 5. Good/Base/Bad Cases

- Good: unsigned App receives the injected executable, dylib, manifest, and
  dictionary; launcher starts it without signing changes.
- Base: fake-App install/uninstall passes and command logs contain no signing or
  xattr calls.
- Bad: `codesign --deep` the App, sign the in-bundle backup, or clear xattrs
  across the bundle.

### 6. Tests Required

- Run `bash -n install.sh tests/install_fake_app.sh`.
- Assert strict `codesign`/`xattr` stubs receive zero calls during install,
  repair, update, and uninstall.
- Perform an explicitly authorized real-App smoke test and record whether the
  launcher and game reach the main menu; never automate destructive testing.

### 7. Wrong vs Correct

#### Wrong

```sh
codesign --force --deep --sign - "$APP_PATH"
```

#### Correct

```sh
# Preserve the game's existing unsigned state.
mv -f "$EXEC_PATH.eu4dll-tmp" "$EXEC_PATH"
```
