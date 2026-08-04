# DOMES Agent Evaluation

This suite captures whether a coding agent can find DOMES's authoritative files, explain protocol
and hardware contracts, select adequate verification, and support claims with repository evidence.
It provides an auditable repository-understanding and change-planning baseline; it does not replace
an independent semantic audit, component tests, firmware builds, or physical device validation.

## Safety Model

The runner reads the selected revision and every recursive gitlink directly from local Git object
databases, then creates a disposable single-commit repository. It never invokes
`git submodule update`, `git clone`, `git fetch`, checkout filters, or worktree hooks. Every recorded
submodule must already have an initialized local worktree under the primary checkout, and every
recorded commit and blob must already be present locally. Lazy object fetching and replace objects
are disabled, so missing state fails closed before a source snapshot is created.
`tools/agent_eval/` and the original Git history are excluded, so hidden reference paths are
unavailable from the evaluated repository or its Git objects. The checked-in baseline cases use the
default-deny read-only profile described below and prohibit GitHub writes, host configuration
changes, physical device access, and claims of hardware execution.

Before the first case, the runner copies only `auth.json` into a mode-0700, per-run `CODEX_HOME` and
runs a no-model containment probe. The probe verifies that model-generated commands can read the
sanitized checkout, can use only their isolated home and temporary directories for scratch writes,
and cannot read the original repository, original authentication, isolated authentication, or
response schema. It also loads the exact prompt-free `codex exec --strict-config` command to catch
unsupported flags or configuration without inference. The runner fails closed when file-backed
Codex authentication, Linux user/PID namespaces, Bubblewrap, or either probe is unavailable.

Each case uses a default-deny Codex permission profile instead of the legacy `read-only` and
`workspace-write` modes, which permit broad host reads. The profile exposes minimal system runtime
paths, the Codex installation, and exact case-local paths; direct network access is disabled. Codex
receives no proxy, credential, DBus, SSH, or user-session environment, no user config or exec rules,
and an explicit allowlisted shell environment. An outer read-only Bubblewrap mount and PID namespace
prevent host writes and kill every descendant when the Codex process exits, including processes that
change session or process group. All subprocess waits are bounded.

The runner hashes the checkout before execution and detects tracked, untracked, and ignored writes
after every prepared case, including nonzero exits, timeouts, and malformed responses. Cleanup and
write-audit failures are retained in the result.

Run live evaluations only against revisions whose project instructions and repository contents you
trust. The sandbox and prompt reduce risk, but they are not a security boundary for evaluating an
untrusted pull request with access to model authentication. Live evaluations therefore do not run
in pull-request CI.

Workspace-write cases are not supported because a path-only change list is not enough to audit
generated patches after the temporary checkout is deleted. A future write-enabled evaluator must
retain a bounded content or patch manifest before adding such cases. This evaluator must never
publish issues, pull requests, labels, releases, or device mutations.

## Validate The Suite

```bash
python3 tools/agent_eval/agent_eval.py validate
python3 -m unittest discover -s tools/agent_eval -p 'test_*.py' -v
```

Initialize submodules explicitly in the primary checkout before a live run. This setup command may
use the network; snapshot preparation never performs a Git network operation:

```bash
git submodule update --init --recursive
```

A run fails when the selected revision's recorded submodule worktree, commit, tree, or blob is not
already available locally. For a historical target, prepare all required objects explicitly before
starting the evaluator.

Live runs currently require Linux, Bubblewrap, unprivileged user namespaces, and file-backed Codex
authentication at `$CODEX_HOME/auth.json` (or `$HOME/.codex/auth.json`). Tests skip only the optional
installed-Codex containment integration when those executables are absent; a live run never falls
back to weaker containment.

## Capture A Run

Commit every repository change before capturing a run. The runner has no dirty-checkout override:
the exact harness source, schema, cases, target revision, sanitized checkout, and submodule commits
must be reproducible. A custom `--cases` file must also be a tracked file inside this repository.
Definitions are parsed from the same captured bytes that are hashed and copied into the immutable
run. Use an output path outside the repository for exploratory runs, or commit a previous result
before starting the next run.

```bash
python3 tools/agent_eval/agent_eval.py run \
  --run-id baseline-sol-medium \
  --model gpt-5.6-sol \
  --effort medium \
  --output tools/agent_eval/results/baseline-sol-medium.json

python3 tools/agent_eval/agent_eval.py report \
  --input tools/agent_eval/results/baseline-sol-medium.json \
  --output tools/agent_eval/results/baseline-sol-medium.md
```

Use `--case <id>` repeatedly for a focused run. The runner records the exact Git revision, model,
reasoning effort, complete case contracts, harness/schema/case digests, environment metadata,
containment settings and preflight, submodule revisions, sanitized-checkout digest, per-case duration,
optional token usage exposed by Codex, the structured response, evidence coverage, cleanup contract,
and unexpected checkout changes. The response schema is copied once into a read-only per-run snapshot
before any case starts.

`run` exits nonzero when a case errors, omits criterion evidence or reference paths, reports the
wrong hardware requirement, or writes in the read-only checkout. Every case reports only whether
physical hardware is required for full validation; hardware execution is prohibited by the harness
and recorded separately in run metadata. Exit zero means only that the result is structurally ready
for an independent LLM semantic audit. It never means the response is correct or approved.

## Comparing Changes

Change one variable at a time:

1. Capture the current repository baseline with Sol at medium reasoning.
2. Test Sol at high reasoning only on cases whose difficulty may justify it.
3. Apply one instruction, tool, or configuration change.
4. Rerun the same case definition with the original model and effort.
5. Have an independent LLM apply the case criteria to evidence correctness, coverage, unsupported
   claims, and the content and spirit of the repository contract; use `Meets intent`, `Needs
   revision`, or `Not verifiable`, with reasons.

Do not make coverage easier by weakening a criterion or reference path. Update a case only when the
repository contract or the evaluation itself was wrong, and retain the previous result for
comparison.

## Case Contract

[`cases.json`](cases.json) contains deterministic prompts, hidden reference paths, neutral evidence
criteria, read-only sandbox mode, hardware requirement, and cleanup contract. Criterion ids and
questions are shown to the evaluated agent, but reference paths and previous results are not present
in its sanitized checkout. Codex must return [`response.schema.json`](response.schema.json) with one
evidence entry per criterion. Reported paths must already exist in the sanitized checkout;
prospective outputs belong in response prose. `hardware_requirement: required` means physical
evidence remains necessary for full validation; it never means the harness ran that evidence.
`not_required` means the scoped assessment can be completed without physical execution.

Automated checks validate only structural evidence coverage, repository path existence, the
hardware-requirement contract, and checkout cleanliness. They do not search for required or
forbidden keywords and do not assign a correctness score. The generated report includes the agent
summary, claims, invariants, verification plan, criterion evidence, token usage, and containment
metadata so an independent LLM can audit correctness before comparing runs or accepting a result.

Cases cover cross-language protobuf changes, response envelopes, ISR safety, CLI commands, Flutter
BLE lifecycle, documentation authority, autonomous priority selection, OTA rollback claims,
multi-pod port routing, scoped docs, unsafe firmware review, WiFi boundaries, and release
verification.

## Legacy Results

`results/pre-optimization-sol-medium-2026-08-02.json` is a schema-v1 result produced by the
predecessor harness at its recorded revision. It is retained as historical evidence, but the current
schema-v3 runner cannot parse or compare it and its score is not a current-harness baseline. Do not
rewrite its revision or infer missing containment and evidence metadata. Capture a new run with the
current harness before making before-and-after comparisons.
