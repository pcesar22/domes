# FS3 contract compatibility gate

## Revision-bound verdict

The FS3 peer/drill software baseline at specification revision
`6f197670a49bc8b83753d1dfab0dd1f789b5f4db` is **accepted as internally compatible**.
The retained rerun executed the already-committed runner at descendant revision
`1d24d66018d010cc9618dab42cb8c85d85f6b331`, which descends from required base revision
`be347355d3747b849b0521e40c539aae88d33614`, on 2026-08-22 UTC. Before running any consumer
check, it compared every covered working-tree source to its pinned Git object and found no drift.
Protocol generation, 48 focused firmware/simulator tests, 3 Rust CLI contract tests, 3 Dart
contract tests, 13 mobile scoring/result tests, 22 scoring validation tests, and 2 retained
negative-path checks passed. The later artifact-only commit retains this exact run without changing
the executed runner.

This historical verdict accepts only the pinned software compatibility baseline.
Current convergence and acceptance work is tracked in issues #154 and #155.
PR #107 is excluded as authority. The FS-WP-003A physical exit remains unverified.

## Pinned source map

| Contract surface | Pinned source and generated artifacts | Consumer or validation reference |
| --- | --- | --- |
| Protobuf authority | `firmware/common/proto/peer_drill.proto`, `peer_drill.options` | `tools/generate_protocols.sh --check all` |
| Firmware nanopb | `firmware/common/proto/peer_drill.pb.c`, `peer_drill.pb.h` | `firmware/test_app/main/test_esp_now_protocol.cpp` generated-encoding and fail-closed tests |
| Rust prost | `tools/domes-cli/build.rs`, `tools/domes-cli/src/proto.rs`; ephemeral `Cargo OUT_DIR/domes.peer.rs` | `tools/domes-cli/src/protocol/peer_contract.rs` generated-variant, malformed-input, role, and state tests |
| Generated Dart | `ios/domes_app/lib/data/proto/generated/peer_drill.pb.dart`, `.pbenum.dart`, `.pbjson.dart` | `ios/domes_app/lib/data/protocol/peer_contract.dart` and `ios/domes_app/test/data/protocol/peer_contract_test.dart` |
| ESP-NOW version 1 | `firmware/domes/main/services/espNowProtocol.hpp` | Exact 11-20 byte sizes, generated payload mapping, canonical decode, semantic validation, and lossless legacy mapping in `test_esp_now_protocol.cpp` |
| Production round tokens | `firmware/domes/main/services/roundTokenSequence.hpp`, `espNowService.hpp`, `espNowService.cpp` | Seed-plus-one/zero-skip test in `firmware/test_app/main/test_platform_inputs.cpp`; stale-token and round-message tests in `test_multi_pod_sim.cpp` |
| Simulator tokens | `firmware/test_app/sim/drillOrchestrator.hpp` | `firmware/test_app/main/test_sim_drill.cpp` and `test_multi_pod_sim.cpp` preserve, match, reject, and score round tokens |
| Mobile runtime/results | `ios/domes_app/lib/application/providers/drill_provider.dart`, `ios/domes_app/lib/domain/models/drill_result.dart` | `ios/domes_app/test/domain/models/drill_result_test.dart` exercises production result scoring; mobile results intentionally do not retain round tokens |
| Retained scoring | `tools/scoring_validation/fixtures/fixed_two_pod_v1.json`, `campaign.py`, `test_campaign.py` | `tools/scoring_validation/artifacts/verdict.json` retains status `diverged`: scoring fields match, while absent mobile round tokens are explicit divergences |

The runner regenerates the pinned/current object map and generated prost SHA-256.
The historical raw execution log is no longer part of the source snapshot.

## Executed checks

Portable reproduction command:

```bash
mkdir -p .artifacts
tools/scoring_validation/run_fs3_contract_gate.sh 2>&1 | \
  tee .artifacts/fs3-contract-gate.log
```

Use the pinned Flutter SDK and a writable pub cache. The runner records
the exact child commands, tool versions, output, per-command zero exit status, generated prost
digest, start/end timestamps, and final verdict. Key tool versions were Git 2.53.0, Python 3.14.3,
CMake 3.21.1, GCC 15.2.1, protoc 33.1, Cargo/Rust 1.92.0, Flutter 3.44.8, and Dart 3.12.2.

The historical run reported the following result; regenerate it for a current-revision claim:

```text
GATE_VERDICT=ACCEPTED_SOFTWARE_COMPATIBILITY
PHYSICAL_EVIDENCE=UNVERIFIED
```

## Fail-closed and evidence boundary

`run_fs3_contract_gate.sh` exits nonzero before accepting the baseline if the pinned commit is
missing or is not an ancestor, any mapped source differs from its pinned Git object, generated
nanopb or Dart bindings drift, generated prost is absent, the version-1 radio mapping or
RoundTokenSequence tests fail, any firmware/simulator/Rust/Dart consumer rejects the baseline, or
the scoring fixture and fail-closed validation checks fail. Ordinary command failures are recorded
by the error trap. Deliberate source-drift and missing-artifact branches use a shared failure path
that records the exact source or artifact, check, exit status, and source line before terminating.
Focused negative-path tests exercise both retained records and prove that neither output contains
an accepted verdict. A failure is a stop condition and does not authorize dependent work.

Any later change to `peer_drill.proto`, nanopb/prost/Dart generation or artifacts, the ESP-NOW
version-1 mapping, production token sequencing, mapped consumer/runtime code, focused contract
tests, simulator token behavior, or retained scoring validation invalidates this verdict and
requires a fresh gate rerun and human review.

No hardware operation occurred. Physical BLE, ESP-NOW radio behavior, touch, device timing,
synchronized-clock behavior, and wall-clock equivalence are all **unverified**. An automated test
or accepted device command cannot convert any of those evidence classes into a pass; this worker
also performed no accepted device command.
