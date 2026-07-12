# Design

## Structure

Keep `install.sh` as the user entry point, but organize it around pure discovery
and validation helpers followed by explicit mutation phases. Allow tests to
override app/tool/artifact paths and command implementations through documented
environment variables or a sourced library mode.

## Transaction

1. Discover and validate all inputs.
2. Generate the manifest into a temporary directory outside the app.
3. Prepare a transaction directory and preserve the original executable.
4. Apply injection, dylib/resource/manifest copies and configuration without
   changing the unsigned App's signing or xattr state.
5. Atomically publish final files.
6. On failure, roll back only files changed by the transaction.

Uninstall uses recorded paths and removes only eu4dll-owned artifacts before
restoring the verified original executable.

## Testing

Build a fake `.app` tree under a temporary directory. Stub `insert_dylib`, the
manifest tool, signing, and privilege escalation while retaining their argument
and failure behavior. Tests assert file hashes, call ordering, rollback, and
diagnostic phase names.
