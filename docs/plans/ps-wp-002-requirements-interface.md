# Hardware-Driving Requirements And Interface Baseline

Status: complete
Current phase: review-ready pull request boundary
Repository state: `codex/docs/ps-wp-002-requirements` in `.worktrees/ps-wp-002`; stacked PR 94;
content commit `011b949` passed all required Software CI checks; final status-only commit requires the
same gate
Last updated: 2026-08-04; PS-WP-002 complete at its review/CI/status stop boundary

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
- [x] Commit, push, open one stacked PR, pass required CI on the content commit, and prepare final
  status/evidence closeout.

## Verification

| Evidence level | Command or observation | Status |
| --- | --- | --- |
| Semantic | Two independent reviews against issue 93, product authority, G1, and HW-WP-001 | Passed after resolving all findings |
| Automated | `python3 tools/docs/check_markdown_links.py` | Passed: 87 files, 424 relative links |
| Automated | Pre-commit plus 51 agent/instruction tests and 40 CI contract tests | Passed |
| Automated | `scripts/verify.sh --component docs` | All available checks passed; wrapper stopped only because this host lacks `shellcheck`, so CI remains required |
| CI | PR 94 Software CI, run 30892936374 | Content commit passed all seven checks including the aggregate `CI Gate`; final status-only head rerun required |
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

PS-WP-002 is complete at the bounded review-package stop condition. PR 94 contains the controlled
candidate, navigation, traceability, and program controls; two independent reviews found no remaining
actionable issue. Content commit `011b949` passed every required Software CI check, including the
CI-provided shell checks. Push this final status-only commit and require the same aggregate gate to
remain green. Then update issue 93 with the exact evidence and stop. Do not begin VC-WP-001 without a
new continuation directive, and do not merge, release, spend, fabricate, or claim product behavior.
