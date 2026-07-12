# Development Workflow

## Purpose

Trellis preserves project decisions and long-running task context. The model is
expected to reason and implement directly; the workflow should add memory and
verification, not ceremony.

## Core principles

1. Read the relevant project spec before changing code.
2. Keep portable behavior separate from platform and EU4 hook adapters.
3. Record only durable requirements, decisions, and discoveries.
4. Verify in proportion to risk; do not claim success without evidence.

## When to create a task

Create a Trellis task when work is likely to:

- span multiple modules or development sessions;
- add or change a platform adapter, hook ABI, patch plan, or executable profile;
- require design decisions that future work must remember;
- contain several independently verifiable deliverables.

Do not create a task for explanations, code reading, tiny edits, formatting, or
single-file mechanical changes. Do not ask the user about task creation for
these small turns.

If task creation would materially expand the requested scope, ask first.
Otherwise, a request to use the Trellis workflow is standing permission to
create an appropriately scoped task.

## Task commands

    python3 ./.trellis/scripts/task.py create "<title>" [--slug <name>]
    python3 ./.trellis/scripts/task.py start <name>
    python3 ./.trellis/scripts/task.py current --source
    python3 ./.trellis/scripts/task.py finish
    python3 ./.trellis/scripts/task.py archive <name>
    python3 ./.trellis/scripts/task.py list [--mine]

## Planning artifacts

- prd.md: required for every task; keep requirements, constraints, and
  acceptance criteria concise.
- design.md: add for platform/ABI changes, new seams, risky refactors, or
  meaningful tradeoffs.
- implement.md: add only when ordering, migration, or rollback details matter.
- research/: add only when external documentation or experiments affect the
  decision.
- implement.jsonl and check.jsonl: optional; Codex inline mode does not require
  them.

## Phase index

    Plan → Execute → Finish

### No active task

[workflow-state:no_task]
No active task. Handle small work inline.
Create a concise Trellis task only for multi-module, cross-platform, risky, or
multi-session work. Do not add workflow ceremony to simple turns.
[/workflow-state:no_task]

### Phase 1: Plan

1. Create or select the task.
2. Write a concise prd.md.
3. Add design.md or implement.md only when their information is necessary.
4. Read the relevant files under .trellis/spec/project/.
5. Start the task when the acceptance criteria are clear.

[workflow-state:planning]
Stay in planning until requirements and acceptance criteria are clear.
Keep artifacts concise. Add design or execution detail only when risk requires
it. Do not implement before the task is started.
[/workflow-state:planning]

[workflow-state:planning-inline]
Stay in planning until requirements and acceptance criteria are clear.
Codex works inline: skip sub-agent manifests and mandatory dispatch. Read the
relevant project specs, then start the task.
[/workflow-state:planning-inline]

### Phase 2: Execute

1. Implement directly in the main session.
2. Keep changes within the active task's scope.
3. Run focused checks after meaningful increments.
4. Update planning artifacts only when the implementation changes the decision.

[workflow-state:in_progress]
Implement the active task directly unless independent parallel work clearly
earns its coordination cost. Read the PRD and relevant project specs, keep the
change scoped, and gather verification evidence before finishing.
[/workflow-state:in_progress]

[workflow-state:in_progress-inline]
Implement directly in the main Codex session. Do not require implement/check
sub-agents. Read the PRD and relevant project specs, run focused validation,
then update durable project knowledge only if something new was learned.
[/workflow-state:in_progress-inline]

### Phase 3: Finish

1. Run the relevant build, tests, or static checks.
2. Review the diff for scope and platform leakage.
3. Update .trellis/spec/project/ only for durable new rules or decisions.
4. Summarize results and remaining risks.
5. Commit only when the user asks.
6. Archive the task when its acceptance criteria are met.

[workflow-state:completed]
The task is complete. Report verification evidence and remaining risks.
Archive the task. Do not create a commit unless the user requested one.
[/workflow-state:completed]

## Verification expectations

- Portable behavior: unit tests where practical.
- Patch installation: byte-buffer or binary-fixture verification.
- Platform adapters: compile on the target platform plus focused integration
  checks.
- Hook changes: verify pattern uniqueness, overwritten bytes, return addresses,
  and ABI assumptions.
- Documentation-only changes: check links, paths, and generated configuration.

## Durable knowledge

Write to .trellis/spec/project/ when a fact should constrain future work, such
as a supported target, ABI rule, module seam, or validation requirement.
Do not journal routine actions or restate information already visible in code.

