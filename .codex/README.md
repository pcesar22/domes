# DOMES Codex Configuration

This directory contains repository-scoped Codex defaults, narrow specialist agents, and the
structured contracts used by the external deterministic agent control plane. Codex loads
the project layer only after the checkout is trusted. Review these files before trusting a fork or
unfamiliar branch; an untrusted checkout ignores project-local `.codex/` configuration.

The primary agent is pinned to `gpt-5.6-sol` at medium reasoning. Generic child agents default to
`gpt-5.6-terra` at medium reasoning; named roles below override that fallback. Configuration selects
models and caps concurrently open subagent threads at two. It does not perform semantic routing or
change approval policy, network access, or the primary sandbox.

Interactive primary sessions route specialists automatically when a trigger below applies and
delegation materially reduces uncertainty or risk. It remains the sole writer and owns scope,
decisions, synthesis, and verification.

Autonomous issue execution is a separate workflow. `WORKFLOW.md` and
`docs/agent-system/README.md` define disposable requirements-steward, planner, worker, judge, and
verification roles. Their prompts and machine-readable result schemas live under
`.codex/orchestration/`. The scheduler receives final structured results, never raw transcripts.

## Specialist Roles

| Agent | Model | Purpose |
| --- | --- | --- |
| `firmware_reviewer` | Sol, high | Review ISR, FreeRTOS, memory, initialization, and hardware risks |
| `protocol_reviewer` | Sol, high | Review protobuf, framing, envelopes, generation, and compatibility |
| `repo_explorer` | Terra, medium | Map a bounded area and return concise file and symbol evidence |
| `test_triage` | Terra, medium | Isolate the first failure and smallest confirming rerun |
| `hardware_verifier` | Sol, high | Gather device evidence from checked-in runbooks when explicitly assigned |

## Automatic Routing

| Trigger | Route and timing |
| --- | --- |
| Unfamiliar or cross-component scope | `repo_explorer` before planning or editing |
| Protobuf, framing, transport, or cross-language contract | `protocol_reviewer` during design and again on the resulting diff |
| ESP-IDF, ISR, FreeRTOS, memory, or initialization behavior | `firmware_reviewer` after implementation and before acceptance |
| Test or CI failure | `test_triage` before choosing a repair |
| Physical-device evidence or hardware-readiness claim | `hardware_verifier` before accepting the evidence |
| Small, isolated change with an obvious verification path | No specialist; primary handles it directly |

Every specialist is read-only. A role never grants permission to access hardware, publish GitHub
state, change files, or bypass an approval; the parent session and repository instructions still
govern those actions. Reuse a relevant thread, run at most two specialists concurrently, and do not
delegate trivial work. Terra is limited to bounded exploration and triage; priority selection,
quality-critical review, and final judgment stay on Sol.

Validate syntax and the active project default from a trusted checkout with:

```bash
python -m unittest discover -s tools/agent_eval -p 'test_*.py'
codex --strict-config doctor --json --summary
```

The unit tests assert the checked-in model, effort, routing fallbacks, concurrency bound, and named
roles. Doctor must report `config.load` as `ok`; this checks parser compatibility, not semantic
routing. Representative read-only cases for every role live in `tools/agent_eval/cases.json`.

Reference: [Codex configuration and project trust](https://learn.chatgpt.com/docs/config-file/config-reference).
