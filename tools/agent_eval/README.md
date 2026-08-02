# DOMES Agent Evaluation

This suite measures whether a coding agent can find DOMES's authoritative files, preserve protocol
and hardware contracts, select adequate verification, and avoid unsupported completion claims. It
is a repository-understanding and change-planning baseline, not a substitute for component tests,
firmware builds, or physical device validation.

## Safety

Every case runs in a disposable detached Git worktree. Checked-in baseline cases use a read-only
sandbox and prohibit GitHub writes, host configuration, device access, and hardware claims. Future
write cases require `--allow-write-cases` and still run only in a temporary worktree.

## Validate

```bash
python3 tools/agent_eval/agent_eval.py validate
python3 -m unittest discover -s tools/agent_eval -p 'test_*.py' -v
```

## Capture A Baseline

```bash
python3 tools/agent_eval/agent_eval.py run \
  --run-id pre-optimization-sol-medium \
  --model gpt-5.6-sol \
  --effort medium \
  --allow-failures \
  --output tools/agent_eval/results/pre-optimization-sol-medium.json

python3 tools/agent_eval/agent_eval.py report \
  --input tools/agent_eval/results/pre-optimization-sol-medium.json \
  --output tools/agent_eval/results/pre-optimization-sol-medium.md

python3 tools/agent_eval/agent_eval.py compare \
  --input tools/agent_eval/results/pre-optimization-sol-medium.json \
  --input tools/agent_eval/results/post-optimization-sol-medium.json \
  --output tools/agent_eval/results/comparison.md
```

The runner records the exact revision, case digest, model, reasoning effort, environment, duration,
attempt and retry counts, token usage when exposed by Codex, response, scoring criteria, and
checkout changes. It atomically checkpoints after every case. Resume an interrupted run with the
same command plus `--resume`.

Use `--case <id>` repeatedly for focused runs. A dirty harness requires `--allow-dirty` because the
evaluated detached worktree always uses the selected committed revision.

## Compare Changes

Change one variable at a time: capture Sol/medium, test higher effort only on difficult cases, apply
one instruction or tooling change, and rerun the same cases with the original model settings. Do
not improve scores by weakening criteria.

[`cases.json`](cases.json) contains prompts, required files and terms, forbidden claims, hardware
gates, and cleanup contracts. [`response.schema.json`](response.schema.json) defines the structured
model output. Default evaluations never publish GitHub state, releases, or device mutations.
