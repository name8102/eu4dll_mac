# Quality and Verification

## General

- Preserve unrelated user changes.
- Prefer focused changes over repository-wide rewrites.
- Search for all uses before changing patterns, offsets, escape constants, or
  object-layout facts.
- Keep failure reporting actionable: feature, target profile, match result, and
  failed operation.

## Required verification

- Portable feature behavior: focused unit tests.
- Escaped text: round trips, malformed/truncated sequences, cursor movement, and
  output limits.
- Localized search: Chinese names, initials, polyphonic forms, ranking, and
  cache behavior.
- Name ordering: table-driven culture, mod mode, surname, given name, and
  separator cases.
- Patch runtime: deterministic byte-buffer tests before live-process testing.
- Target profiles: binary fixtures verify pattern uniqueness and expected
  original bytes.
- Platform input: adapter conformance tests plus one real integration path per
  operating system.

## Completion evidence

Report the commands run and their outcome. If a target platform was not
available, state what was verified locally and what still requires that
platform. Never treat compilation on one operating system as proof that another
platform adapter works.

## Scenario: startup installation diagnostics

### 1. Scope / Trigger

- Apply to every startup target check, symbol lookup, patch installation, and
  target-hook contract failure.

### 2. Signatures

- Record detailed failures with `StartupDiagnostics::Record(PatchDiagnostic)`.
- Use `ResolveLiveSymbol(feature, target, symbol)` for macOS symbol lookup.
- Use `InstallGuard(feature, target)` only as a fallback for installers that
  return without a detailed diagnostic.

### 3. Contracts

- Startup resets one collector, validates the target before mutation, records
  every failed operation, and shows at most one final actionable report.
- Each entry contains feature, target, operation, match status/count when
  applicable, and a concrete message.
- The recovery text is storefront-neutral.

### 4. Validation & Error Matrix

- Detailed patch failure -> record it; guard must not add a duplicate fallback.
- Missing symbol -> `resolve-symbol` entry with the symbol name.
- Installer fails silently -> one `install-feature` fallback entry.
- Unsupported target -> report immediately before patch installation.

### 5. Good/Base/Bad Cases

- Good: no failures and no alert.
- Base: several failures produce one report with numbered actionable entries.
- Bad: stderr-only failure, per-feature popup, tracking macro, or copied status
  map disconnected from the structured patch result.

### 6. Tests Required

- Detailed failure suppresses the guard fallback.
- Unmarked guard produces exactly one fallback.
- Successful guard produces no failure.
- Formatted report includes feature, operation, and neutral recovery guidance.

### 7. Wrong vs Correct

#### Wrong

```cpp
TRACK_FUNCTION();
printf("pattern failed");
```

#### Correct

```cpp
diagnostics.Record(result.diagnostic);
// Bootstrap formats the collector once after all installers return.
```
