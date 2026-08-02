# DOMES Execution Plans

Use an execution plan for work that is likely to outlive one context window or that crosses a
boundary where an incomplete change can look correct. A plan is required for:

- firmware plus CLI or Flutter changes, including protobuf generation;
- protocol migrations, architecture refactors, OTA or rollback, and multi-device behavior;
- hardware bring-up or verification with physical gates; and
- long-running release, debugging, or recovery work that another agent may need to resume.

Small, single-component changes with an obvious verification path do not need a plan. GitHub issues
remain the backlog; a plan records only the current execution state and is removed or reduced to a
final outcome after the work is integrated.

## Working Contract

Create one plan for the active task at the location named by the user or, by default,
`docs/plans/<short-name>.md`. Keep it self-contained enough that another agent can continue using the
plan and repository state without replaying chat or raw logs.

- Write observable outcomes, not activity such as "work on firmware."
- Link to authoritative repository files; do not copy architecture or runbooks into the plan.
- Record contracts that must remain true and every generated consumer affected by a schema change.
- Mark one current phase and make stage dependencies explicit.
- Update the checkpoint after each stage, meaningful discovery, failed assumption, or scope change.
- Summarize evidence with the command, result, and relevant artifact; do not paste full logs.
- Separate automated checks, an accepted device command, and physical confirmation. None implies
  either of the others.
- If a toolchain, transport, device, second pod, or observer is unavailable, name the exact untested
  behavior and the command or observation still required. Unavailable is not passed.
- Record decisions and deviations where they happen, including why the plan changed.

## Template

```markdown
# <Outcome-oriented title>

Status: active | blocked | complete
Current phase: <one phase>
Repository state: <branch/worktree and last relevant commit; list intentional dirty files>
Last updated: <date and concise checkpoint>

## Objective and observable outcome
<What changes and how a person or test can observe success.>

## Authorities and contracts
- Authority: `<path>` - <what it owns>
- Preserve: <wire, API, lifecycle, safety, or user-visible behavior>

## Affected components and generated consumers
| Component | Files or generated output | Required change |
| --- | --- | --- |

## Stages and dependencies
- [x] <completed stage and evidence pointer>
- [ ] **Current:** <next bounded stage>
- [ ] <later stage; depends on ...>

## Verification
| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `<exact command>` | <pending, passed, or failed with cause> |
| Accepted command | `<transport command and response>` | pending; does not prove physical effect |
| Physical confirmation | <observable device behavior and setup> | pending/unavailable: <exact gap> |

## Decisions, discoveries, and deviations
- <decision or discovery> - <reason and consequence>

## Resume checkpoint
<What is complete, what is intentionally dirty, the next command or edit, and the first authority
to reread. Include blockers and remaining unverified behavior.>
```

## Representative Example: Runtime Brightness Command

This is an illustrative cross-component plan, not an active feature claim.

Status: active
Current phase: schema and compatibility review
Repository state: `codex/feat/runtime-brightness`; no implementation commit; intended dirty file
`firmware/common/proto/config.proto`
Last updated: 2026-08-02; request/response ownership mapped, message IDs not yet assigned

### Objective and observable outcome

A host can request a bounded runtime brightness value through serial or BLE; firmware acknowledges
the request, applies it, and both the CLI and Flutter report the applied value. Automated encoding
tests, an accepted response, and visible brightness are recorded as three separate results.

### Authorities and contracts

- Authority: `firmware/common/proto/config.proto` and `config.options` - message schema and bounds.
- Authority: `firmware/domes/main/config/configCommandHandler.hpp` - firmware response envelope.
- Preserve: `[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]` framing and the existing one-byte status
  envelope for status-bearing config responses.
- Preserve: generated nanopb, Rust prost, and Dart types; do not create handwritten duplicates.

### Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Schema | `firmware/common/proto/config.proto`, `config.options` | Add bounded request and response |
| Firmware | config handler and generated nanopb | Decode, validate, apply, respond |
| Rust CLI | `tools/domes-cli/build.rs`, command tests | Use prost type on every supported transport |
| Flutter | generated Dart protocol and provider tests | Send request and reject stale responses |

### Stages and dependencies

- [x] Mapped schema authority, consumers, and response envelope.
- [ ] **Current:** Choose unused message IDs and review compatibility before editing the schema.
- [ ] Generate all consumers with `tools/generate_protocols.sh`; depends on approved schema.
- [ ] Implement firmware, CLI, and Flutter paths; depends on clean generated output.
- [ ] Run software checks, then serial and BLE device verification.

### Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `tools/generate_protocols.sh --check` plus firmware, Cargo, and Flutter suites from `docs/TESTING.md` | pending |
| Accepted command | CLI set/get over serial and BLE returns the applied protobuf value | pending; not physical proof |
| Physical confirmation | Observer compares minimum, midpoint, and maximum LED brightness on one pod | unavailable until a pod and observer are assigned |

### Decisions, discoveries, and deviations

- Brightness remains a config response with a status envelope; using a bare protobuf response would
  create a new exception without a compatibility need.
- WiFi is not part of the first hardware claim because a clean default build lacks auto-connect and
  credentials; software transport-independence tests still cover the shared command layer.

### Resume checkpoint

No implementation is complete. Re-read the message-type declarations in
`configCommandHandler.hpp`, confirm unused IDs across firmware/Rust/Dart, then edit the `.proto`
first. The exact remaining physical gap is serial and BLE application plus observed LED brightness;
an accepted response alone must remain labeled incomplete.
