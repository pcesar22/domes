# DOMES Codex Configuration

This directory contains repository-scoped Codex defaults and narrow specialist agents. Codex loads
the project layer only after the checkout is trusted. Review these files before trusting a fork or
unfamiliar branch; an untrusted checkout ignores project-local `.codex/` configuration.

The primary agent is pinned to `gpt-5.6-sol` at medium reasoning. It remains responsible for scope,
implementation decisions, final synthesis, and verification. Specialist agents are opt-in: use them
only when a user or applicable project or skill instruction explicitly requests delegation. The
configuration caps concurrently open subagent threads at two and does not change approval policy,
network access, or the primary sandbox.

| Agent | Model | Purpose |
| --- | --- | --- |
| `firmware_reviewer` | Sol, high | Review ISR, FreeRTOS, memory, initialization, and hardware risks |
| `protocol_reviewer` | Sol, high | Review protobuf, framing, envelopes, generation, and compatibility |
| `repo_explorer` | Terra, medium | Map a bounded area and return concise file and symbol evidence |
| `test_triage` | Terra, medium | Isolate the first failure and smallest confirming rerun |
| `hardware_verifier` | Sol, high | Gather device evidence from checked-in runbooks when explicitly assigned |

Every specialist is read-only. A role never grants permission to access hardware, publish GitHub
state, change files, or bypass an approval; the parent session and repository instructions still
govern those actions. Terra is limited to bounded, read-heavy support work. Quality-critical review
and the primary agent's final judgment stay on Sol.

Validate syntax and the active project default from a trusted checkout with:

```bash
python -m unittest discover -s tools/agent_eval -p 'test_*.py'
codex exec --ephemeral --strict-config --sandbox read-only \
  "Report the active model and reasoning effort, then stop without changing files."
```

The non-interactive run should identify `gpt-5.6-sol` and medium reasoning in its session metadata.
Representative read-only cases for every role live in `tools/agent_eval/cases.json`.

Reference: [Codex configuration and project trust](https://learn.chatgpt.com/docs/config-file/config-reference).
