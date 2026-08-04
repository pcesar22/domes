# Hardware-Driving Requirements And Interface Baseline

Status: active
Current phase: candidate and local verification complete; publication pending
Repository state: `codex/docs/ps-wp-002-requirements` in `.worktrees/ps-wp-002`, stacked on PR 92
Last updated: 2026-08-04

## Objective and observable outcome

Convert the PS-WP-001 workflow and current/target architecture into one traceable G1 candidate.
Success is observable when stable product/system requirement and interface IDs expose their source,
classification, allocation, verification, environment, owner, state, bounded closure evidence,
fallback, and invalidation rule without freezing a technical solution or inventing product evidence.

## Authorities and contracts

- Product/workflow: `research/PRODUCT_DEFINITION.md`.
- Candidate allocation and interfaces: `research/G1_REQUIREMENTS_AND_INTERFACES.md`.
- Current implementation: code/protobuf plus `research/SOFTWARE_ARCHITECTURE.md`.
- Target inputs: `research/SYSTEM_ARCHITECTURE.md` and `research/ID_REQUIREMENTS.md`.
- Hardware definition: `hardware/NEXT_ITERATION_REQUEST.md`.
- Verification and status: `docs/TESTING.md` and `PROGRAM_STATUS.md`.
- Issue: 93; dependency: PR 92.
- Stop: no runtime/protocol change, architecture/part freeze, spend, schematic/layout release,
  fabrication, compliance/product claim, merge, release, or next package.

## Affected components

| Component | Required change |
| --- | --- |
| G1 candidate | Add stable requirements, interfaces, traceability, conflicts, and closure ledger |
| Product authority | Link the candidate without promoting it to accepted requirements |
| Documentation navigation | Identify the candidate and preserve source ownership |
| Program control | Record active execution, evidence boundary, and reserved next work |
| Execution record | Link issue, PR, commands, results, uncertainty, and stop boundary |

No generated output or runtime consumer changes.

## Stages and dependencies

- [x] Reconcile PR 92, issue 93, program status, product/system/ID inputs, current implementation,
  hardware request, and verification authority.
- [x] Complete a read-only cross-component source and contradiction map.
- [x] Write the controlled requirement, interface, traceability, conflict, and closure candidate.
- [x] Complete independent semantic review and resolve findings.
- [x] Run documentation and repository verification.
- [ ] Commit, push, open one stacked PR, require green CI, and record final evidence on issue 93.

## Verification

| Evidence level | Command or observation | Status |
| --- | --- | --- |
| Semantic | Two independent reviews against issue 93, product authority, G1, and HW-WP-001 | Passed after resolving all findings |
| Automated | `python3 tools/docs/check_markdown_links.py` | Passed: 87 files, 424 relative links |
| Automated | Pre-commit plus 51 agent/instruction tests and 40 CI contract tests | Passed |
| Automated | `scripts/verify.sh --component docs` | All available checks passed; wrapper stopped only because this host lacks `shellcheck`, so CI remains required |
| CI | Stacked pull-request Software CI and aggregate gate | Pending |
| Hardware | Not applicable; this package changes no behavior and claims no new device evidence | Not required |

## Decisions and boundaries

- One combined G1 candidate prevents separate requirement and interface documents from drifting.
- Current wire, pin, partition, and command values are linked to implementation authorities rather
  than copied into a new specification.
- Numeric target values remain candidates. Missing evidence is represented by a bounded closure
  record whose fallback can preserve analysis but cannot manufacture a gate pass.
- The candidate remains topology-neutral where FS3/HW1 must select radio, charge, power, identity,
  and manufacturing implementations.

## Resume checkpoint

Review `research/G1_REQUIREMENTS_AND_INTERFACES.md` against issue 93 and the G1 evidence table. Fix
semantic findings first, then run all documentation checks. Publish exactly one PR with base
`codex/docs/ps-wp-001-product-brief`, monitor its aggregate CI, update status/evidence, and stop.
