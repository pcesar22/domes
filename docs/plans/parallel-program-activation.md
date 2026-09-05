# Activate three coordinated DOMES delivery lanes

Status: active
Current phase: coordinated execution active; validate and publish the owner-only status snapshot
Repository state: codex/feat/development-dashboard at 8d7b5db; runtime source pinned to main
3b62a6c82160d0271b276bb42894e3d3bb69761e. Preserve user changes in .codex/PLATFORM.md and
tools/agent_control/test_control.py. The prior NFF report is intentional evidence for this package.
Last updated: 2026-09-05 UTC.

## Objective and authorization

The user approved the proposed operating ethos: one coordinator, three independently advancing
lanes, bounded capacity, independent review, human merge, explicit hardware authority, private
Sites status, Slack decisions and scheduled coordination. This activates execution and reporting;
it does not authorize spending, fabrication, releases, weakened acceptance or host-service changes.

## Authorities and contracts

PROGRAM_STATUS.md and docs/program/milestones.json own milestone truth; WORKFLOW.md and
docs/agent-system/README.md own controller acceptance; docs/TESTING.md owns proof. The approved
operating agreement is docs/agent-system/OPERATING_MODEL.md. GitHub holds package contracts and
review state. New worker tickets pin current main, not the unmerged dashboard branch.

## Stages

- [x] Inspect local changes and live GitHub backlog; preserve unrelated work.
- [x] Confirm clean ministrom at current main, no active controller, valid controller contracts.
- [x] Resolve Codex/Flutter through the normal remote login environment; no host modifications.
- [x] Register app #197, memory-repair #198 and coordinator-managed hardware-definition #199.
- [x] Start a bounded controller cycle; both isolated worker processes directly observed.
- [x] Complete the initial hardware inventory/coverage record and evidence reconciliation.
- [x] Activate coordinate-domes-delivery every 30 minutes; retain the existing sole Site publisher.
- [ ] Validate and publish the updated owner-only dashboard; send Slack activation/steer.
- [ ] Commit/push scoped activation work and open a human-review PR; no merge.

## Execution boundaries

First worker cycle is software-only: app virtual lab and NFF memory repair candidate. Neither has
hardware operations. The controller retains its ministrom host pin, exclusive lock, isolated
workspaces and independent judgment. A finite cycle allows account-budget and lane-priority checks
before later dispatch; a heartbeat must never start a duplicate process. The existing controller
has no issue-ID filter, so the coordinator reviews every candidate it could dispatch.

## Verification and resume

Controller validation and live read-only queue passed. Historical eligible rework is #166/#193;
other tickets mostly wait on real dependencies. Initial open PR count is three (#190/#191/#194).
NFF programming/communication evidence is passed; both devices' full readiness remains failed.
Current resumable runtime/publication and notification outcomes are recorded in
docs/program/coordination-checkpoint.md. That checkpoint is deliberately separate from the reviewed
Site snapshot so routine polling/notification bookkeeping does not masquerade as new product evidence.
Manual review-only handoffs require coordinator verification of PR identity, required CI and PR
capacity; the controller's automated-path guarantees must not be claimed for this finite mode.
