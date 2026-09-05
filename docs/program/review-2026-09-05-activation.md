# Three-lane activation evidence review

Reviewed 2026-09-05 00:50 UTC. Local management-source baseline 8d7b5dbee0c061152d87381ab563c41ae349d00f
plus the explicit activation changes. Runtime workers and the retained NFF build pin main
3b62a6c82160d0271b276bb42894e3d3bb69761e. This is not acceptance of all newer-main behavior.

## Authority and observed execution

Paulo explicitly approved the proposed three-lane operating agreement. GitHub issues #197 and #198
carry bounded main-pinned, software-only review contracts; #199 is coordinator-managed hardware
definition. All three were created only after live backlog and PR inspection. The first two ranked
first in the controller's live eligibility output. Controller validation passed on clean ministrom.
No controller was running at preflight. Codex authentication and tool availability were confirmed
through the normal login environment, without installing packages or modifying services.

A finite `run --execute --limit 2` cycle started in `domes-coordinated` at 00:44:46 UTC. At 00:45 UTC,
the controller and separate issue-197/issue-198 Codex worker processes were directly observed.
FS-WP-004A is therefore Active, not Complete. The new NFF-MEM-001 software-candidate step is Active,
not a physical readiness pass. HW-WP-002 is Active because its initial inventory/coverage record was
actually produced; its remaining qualified review, quotes, instruments and safe-bench exit is open.

The 30-minute `coordinate-domes-delivery` thread heartbeat was created ACTIVE. The existing
two-hour `refresh-domes-product-status` publisher remains the sole scheduled publisher, with its
original owner-only and no-device/no-ministrom/no-GitHub-write boundaries. Outbound Slack DM was
previously sent successfully; inbound reply handling remains untested. Execution observations are
dated snapshots, not a claim of a permanent live process or uninterrupted sleeping-host access.

## Device evidence incorporated, without advancing physical milestones

The retained September 5 report and artifacts establish a fresh ESP-IDF v5.4.4 build, successful
standard programming on both registered NFFs and 6/6 repeated final identity responses. They also
establish a real failure: both full readiness checks fail, solely Heap, 9/10 self-tests, 0/2 ready.
The untouched second board exhibited the same condition before programming. The 16 KiB health
floor and 30 KiB self-test/OTA floor are different and both remain unchanged.

Retained stable samples report 23,331 B free internal heap, 18,259 B historical minimum and 7,680 B
largest contiguous block. The free deficit to 30 KiB is 7,389 B. Read-only source/ELF inspection
maps the physical profile and candidate allocation sites: a 32,000 B startup audio tone workspace,
the default 96 KiB trace ring and a 4,096 B memory-profiler task. This is a quantified investigation
starting point, not proven runtime attribution or a selected repair. Candidate semantics, DMA,
allocator capabilities, ISR safety and error behavior require implementation-level verification.

The corresponding source paths are unchanged between the local and retained runtime revisions;
other QEMU/ESP-NOW changes are not imported or accepted by this audit. Build success cannot prove
recovered headroom. NFF1 additionally waits for LAB0 and a reviewed repair candidate, then requires
its own exact two-board physical readiness/recovery evidence. No new board access, reflash, OTA,
RF/peripheral/power measurement or physical gate transition occurred during activation.

## Setup and specification gaps

The initial setup record differentiates observed two-board UART access from unknown instrument
possession, calibration, phone/Mac inventory, safe powering and qualified electrical ownership.
It maps baseline, peripheral, envelope, phone, shared-contract, six-node and architecture/parts
evidence needs. It invents no prices, calibration, equipment or electrical limits.

HW-WP-002 names NFF1–NFF5 but the reviewed graph has NFF1–NFF4 then HR0. The discrepancy is now
explicit rather than silently creating NFF5. Current quotes and a qualified safety/uncertainty plan
remain future work. The current two NFFs do not establish six-node physical evidence.

## Execution-control limitations

The configured four-role ceiling conflicts with old three-slot prose; the prose is corrected,
without changing the controller implementation. Manual review-only tickets do not receive the
automated path's atomic PR-cap reservation or remote-artifact attestation. Non-autopilot cycles
also do not perform autonomous CI reconciliation. The coordinator must independently verify each
PR/head/base/diff and required checks, reserve/count the six-PR budget and inspect the complete
eligible queue before another finite cycle. This is recorded in the operating agreement; it is not
represented as a controller-enforced guarantee. Human review/merge remains mandatory.

The initial account snapshot was 4% consumed / 96% remaining in the main reported window. This is
shared allowance, not per-package tokens. The coordinator checks all applicable windows and stops
new unattended dispatch at the 20% reserve; actual per-package usage is unavailable until supplied.

## Source receipt and verdict

The pre-review deterministic check correctly refused the new NFF/activation plans and a previously
committed bring-up-checklist reference change. That checklist change replaces a removed skill link
with docs/TESTING.md; inspection found no relaxed ESP-NOW lifecycle or acceptance condition.
All newly watched files in this activation are intentional reviewed plan/setup records. Unrelated
user edits to .codex/PLATFORM.md and tools/agent_control/test_control.py remain preserved and are
not implementation acceptance evidence or part of the activation commit.

Verdict remains P1 / Pre-EVT / Amber, G1 Hold; HR0–HR2 and later physical/product releases do not
advance. App, NFF repair and setup statuses reflect execution only. Stale forecasts, old trace
lineage, predictive qualification and different management/runtime baselines remain explicit.
Record this receipt only after validating executive/graph agreement and source inventory; tests,
lint, typecheck and production build are still required before private publication.
