# FS4 terminal software acceptance

## Claim and stop boundary

`tools/simulation/fs4_terminal_acceptance.py` reproduces one canonical verdict from immutable,
hash-bound software inputs. A pass means only that the retained FS4 software and simulation
evidence met this contract on one exact integrated commit. Physical validation remains unverified;
four additional alpha nodes are unavailable. The verdict makes no physical six-node timing,
synchronized-clock, radio-frequency, peripheral-actuation, hardware-equivalence, physical fault,
physical recovery, physical soak, or predictive-trust claim.

The runner stops without a verdict when upstream accepted evidence is absent. At specification
revision `0f1659c6a32288fa3478969586e54a81599c4453`, issues #154, #155, and #174-#176 remain open and
their accepted artifacts are not integrated on `main`. This is the expected current result, not a
software pass.

## Ownership and duplication audit

The canonical input maps these areas to their existing owners and implementation paths:

| Area | Tracker owner |
| --- | --- |
| Six-node orchestration | #174 |
| Mobile control and accepted result semantics | #154, #155, #174 |
| Canonical fleet diagnostics | #175 |
| Lifecycle recovery | #178, #183, #184 |
| Command and stream fault handling | #178, #183, #184 |
| Six-identity bounded recovery soak | #179 |
| Prequalification bundling | #176 |

The terminal runner consumes those records; it does not reimplement them. Issue #116 is neither
resumed nor replaced. Issue #143 and issues #161-#171 remain excluded scheduler, QEMU,
concurrency, calibration, or predictive work.

## Pinned source inventory

The implementation inventory below is pinned to specification revision
`0f1659c6a32288fa3478969586e54a81599c4453`. It is the source baseline the terminal input must
supersede with accepted, integrated outputs; it is not evidence that the open prerequisite issues
have completed.

| Scope | Pinned implementation and test paths |
| --- | --- |
| Firmware test simulation | `tools/simulation/deterministic_peer_backplane.py`, `tools/simulation/deterministic_peer_campaign.py`, and their focused tests |
| Mobile providers and lifecycle recovery | `ios/domes_app/lib/application/providers/drill_provider.dart`, `multi_pod_provider.dart`, `pod_connection_provider.dart`, and their provider tests |
| Command and stream recovery soak | `ios/domes_app/test/application/providers/fs4_command_recovery_soak_test.dart`, `tools/simulation/fs4_command_recovery_soak.py`, and its focused test |
| CLI readiness behavior | `tools/domes-cli/src/main.rs`, `tools/domes-cli/src/device.rs`, and the CLI tests under `tools/domes-cli/tests/` |
| FS3 scoring validation | `tools/scoring_validation/campaign.py`, `run_fs3_contract_gate.sh`, and their focused tests |
| Existing qualification bundling | `tools/simulation/vc_prequalification.py`, `tools/simulation/qualification/`, and `tools/simulation/test_vc_prequalification.py` |

The controller-captured ownership snapshot on 2026-08-23 recorded #154, #155, and #174-#176 as
open; #178, #179, #183, and #184 as closed; #116 as open and explicitly excluded; and issues #143
and #161-#171 as excluded scheduler, QEMU, concurrency, calibration, or predictive scopes. The final
input still has to bind each consumed artifact to its producing tracker, source commit, tool,
lockfile, and SHA-256 identity. A passing verdict emits an empty
`uncovered_software_behavior` list alongside the complete canonical coverage records.

## Retained input contract

The input is canonical JSON beside all referenced files. It binds the specification revision,
tested Git SHA, exactly six unique ordered target identities, accepted prerequisite source commits,
tracker identities, producing tool identities, lockfiles, artifacts, execution tool versions, raw
logs, lifecycle and fault stages, per-target and per-stage totals, terminal states, and all invariant
counters with SHA-256. Execution-tool version strings carry a checked SHA-256 identity. Each
prerequisite source commit must be an ancestor of the tested commit. Each prerequisite artifact
must repeat its accepted result, source, producing-tool, and lockfile identities.

The execution inventory contains these lifecycle and fault stages in order:

1. `prepare_mode`, `prepare_clear`, `arm_target`, and `hit_clear`
2. `miss_feedback`, `miss_clear`, `cleanup_clear`, and `cleanup_mode`
3. `command_failure`, `stream_failure`, `disconnect`, and `reconnect`

At least 1,000 total cycles are required, with cycles rotating deterministically across all six
targets and every declared stage. Stale mutations, leaked subscriptions, quarantined-generation
reuse, lost or duplicate results, cleanup-order violations, unhealthy-peer mutations, and
unexplained runtime divergences must all remain zero. Every target must finish disconnected.

## Reproduction

Keep input files and raw command logs in the controller's private evidence state. The commands below
assume the controller materializes that state at an ignored `.tmp/fs4-terminal/` path; confirm the
path is locally excluded before running them.
After the upstream work is accepted and integrated, run the focused Flutter lifecycle/recovery
tests, canonical six-target diagnostics, deterministic six-node mobile qualification, and
six-identity recovery soak using their owner-provided commands. Record their immutable outputs in
the input manifest, then run the terminal command twice:

```sh
python3 tools/simulation/fs4_terminal_acceptance.py \
  .tmp/fs4-terminal/input.json \
  --expected-git-sha "$(git rev-parse HEAD)" \
  --output .tmp/fs4-terminal/verdict-1.json \
  >.tmp/fs4-terminal/reproduction-1.log
python3 tools/simulation/fs4_terminal_acceptance.py \
  .tmp/fs4-terminal/input.json \
  --expected-git-sha "$(git rev-parse HEAD)" \
  --output .tmp/fs4-terminal/verdict-2.json \
  >.tmp/fs4-terminal/reproduction-2.log
cmp .tmp/fs4-terminal/verdict-1.json .tmp/fs4-terminal/verdict-2.json
python3 tools/simulation/fs4_terminal_acceptance.py \
  .tmp/fs4-terminal/input.json \
  --expected-git-sha "$(git rev-parse HEAD)" \
  --verify .tmp/fs4-terminal/verdict-1.json
```

The two canonical verdict files and their embedded digest must be byte-identical. Generated inputs,
logs, and verdicts are execution evidence and remain outside Git.
