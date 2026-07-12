# Implementation Plan

1. Characterize current install, update, already-injected, and uninstall flows.
2. Extract common path discovery, messaging, validation, and command helpers.
3. Introduce an explicit transaction with preflight-before-mutation ordering.
4. Make backup, publish, rollback, and uninstall idempotent.
5. Add the fake-app harness and failure injection stubs.
6. Verify artifact compatibility, shell syntax/lint, and non-destructive tests.
