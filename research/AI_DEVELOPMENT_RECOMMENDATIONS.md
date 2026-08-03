# AI Development Workflow Decision Record

> **Document status: Retired decision record.** This file records the outcome of an early Claude Code
> workflow assessment. Its generated instruction examples, setup commands, tool inventory, and
> implementation checklist were removed because they no longer describe the repository.

## Outcome

The useful result of the assessment was to make agent behavior follow the same project authorities
as human development rather than creating a parallel AI-specific architecture.

The repository adopted these decisions:

- Repository-wide and scoped engineering rules live in `AGENTS.md` files.
- Reusable firmware, debugging, and GitHub workflows live in project-local Codex skills.
- Claude files are compatibility redirects instead of independent technical specifications.
- [`docs/README.md`](../docs/README.md) assigns one owner for delivery status, architecture, protocol,
  hardware mapping, and verification.
- [`scripts/verify.sh`](../scripts/verify.sh) and Software CI provide the aggregate software check.
- Hardware-facing claims require the device evidence defined in
  [`docs/TESTING.md`](../docs/TESTING.md).
- Coding-agent evaluation is isolated from ordinary pull-request CI and documented in
  [`tools/agent_eval/README.md`](../tools/agent_eval/README.md).

## Current Authorities

| Subject | Authority |
| --- | --- |
| Repository instructions | [`../AGENTS.md`](../AGENTS.md) |
| Documentation ownership | [`../docs/README.md`](../docs/README.md) |
| Verification matrix | [`../docs/TESTING.md`](../docs/TESTING.md) |
| Firmware workflows | [`../.codex/skills/domes-esp32-firmware/SKILL.md`](../.codex/skills/domes-esp32-firmware/SKILL.md) |
| ESP32 debugging | [`../.codex/skills/domes-debug-esp32/SKILL.md`](../.codex/skills/domes-debug-esp32/SKILL.md) |
| GitHub workflow | [`../.codex/skills/domes-github-workflow/SKILL.md`](../.codex/skills/domes-github-workflow/SKILL.md) |
| Platform constraints | [`../.codex/PLATFORM.md`](../.codex/PLATFORM.md) |
| Agent evaluation | [`../tools/agent_eval/README.md`](../tools/agent_eval/README.md) |

## Retired Assumptions

The original assessment predated the current repository structure. It treated generated Claude
instructions and slash commands as primary workflow sources, proposed obsolete build and host-test
commands, assumed missing automation that now exists, and copied architecture facts into agent
prompts. Those approaches were retired because they drift independently and can support false
completion claims.

Agent-specific files may explain how to execute a workflow, but they must link to source code,
schemas, the milestone ledger, and the verification matrix for project facts. A model response,
evaluation score, unit test, or successful build is never a substitute for required hardware
verification.

## Historical Provenance

Git history retains the original recommendations and checklist. Keeping this concise record at the
same path preserves inbound links without presenting obsolete commands as current guidance.
