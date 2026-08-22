# FS1 feedback interface software evidence

Status: implemented and software-verified; awaiting independent review
Current phase: publication
Repository state: `codex/issue-138` at required base `498ae0203dc8b7048682fbff718a0629243a98a8`
Last updated: 2026-08-21; issue 138 is the sole live owner and no resumable PR owns this slice

## Objective and observable outcome

Provide protobuf-owned audio software-gain get/set and bounded embedded-beep/fixed-haptic probe
commands through firmware, CLI, Flutter, and host simulation. Automated evidence may establish
serialization, routing, persistence, rejection, and lifecycle semantics only; it cannot establish a
physical feedback result.

## Authorities and contracts

- Authority: `firmware/common/proto/config.proto` owns message types and payloads.
- Authority: `firmware/domes/main/config/configCommandHandler.*` owns status-bearing responses.
- Preserve: `[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]` and `[Status:u8][protobuf]` responses.
- Preserve: `infra::config_key::kVolume` as the stored 0-100 software-gain key.
- Preserve: only the embedded `beep` asset and fixed `HapticEffect::kSharpClick100` are exercisable.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Schema | `firmware/common/proto/config.proto` and generated nanopb/prost/Dart | Add bounded contracts |
| Firmware | config handler, audio service/driver, transport wiring | Restore, persist, serialize, route |
| Host simulation | `firmware/test_app/**` | Cover bounds, persistence, failure, feature state, concurrency |
| CLI | `tools/domes-cli/**` | Deterministic volume and feedback commands |
| Flutter | `ios/domes_app/**` | Repository operations and stale-safe pod-detail state/control |

## Stages and dependencies

- [x] Reconciled issue 138, open PRs, controller worktrees, and tracked plans; no collision found.
- [x] Added and generated the shared schema, firmware integration, and focused host tests.
- [x] Implemented and tested the CLI and Flutter consumers.
- [x] Ran the complete required software gate and retained exact artifacts.
- [ ] Commit, push, open the one review-ready PR, and reconcile its exact head.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `tools/generate_protocols.sh --check all` | passed; full-gate protocol log below |
| Automated | CMake configure/build and `ctest --output-on-failure` | passed; 324 tests |
| Automated | Cargo fmt, strict Clippy, locked debug/release builds, all targets/features | passed; 105 unit and 13 integration tests |
| Automated | locked Flutter restore, fatal analysis, full tests, Linux release build | passed; 177 tests and release bundle |
| Automated | `python3 tools/docs/check_markdown_links.py` | passed; 108 files and 448 links |
| Automated | full `scripts/verify.sh` with ESP-IDF 5.4.4 and isolated SDKCONFIG | passed; 6 of 6 software checks |
| Accepted command | immutable reviewed-head device command | unavailable to implementation worker; not attempted |
| Physical confirmation | direct board observation and measurement | unverified; deferred to separate verifier |

The complete full-gate log is `/tmp/domes-issue138-verify/passing-complete.log`; its JSON summary is
`/tmp/domes-issue138-verify/passing-summary.json`, and retained check logs/build artifacts are under
`/tmp/domes-issue138-verify/passing-artifacts/verify-20260822T031757Z-12`. The gate used Rust 1.92.0,
Flutter 3.44.8/Dart 3.12.2, Dart `protoc_plugin` 25.0.0, and ESP-IDF 5.4.4. Because the managed
worker mounts `.codex/**` read-only while the repository's all-files EOF hook opens existing files
for update, the exact candidate source was copied to a temporary writable verification checkout;
`origin/main` there was pinned to `498ae0203dc8b7048682fbff718a0629243a98a8`, the existing
read-only `.codex/**` EOF state was normalized, and no candidate source change was imported back.

Exact component commands executed before or within the passing gate:

```text
tools/generate_protocols.sh --check all
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
cargo fmt --all -- --check
cargo clippy --locked --all-targets --all-features -- -D warnings
cargo build --locked
cargo build --locked --release
cargo test --locked --all-targets --all-features
flutter pub get --enforce-lockfile
flutter analyze --fatal-infos --fatal-warnings
flutter test
flutter build linux --release
python3 tools/docs/check_markdown_links.py
scripts/verify.sh --json-summary /tmp/domes-issue138-verify/passing-summary.json \
  --keep-artifacts /tmp/domes-issue138-verify/passing-artifacts
```

## FS1 software-path map

| Peripheral | Firmware path | CLI path | App path | Simulator/focused tests | Physical status |
| --- | --- | --- | --- | --- | --- |
| LED | `services/ledService.hpp` | `commands/led.rs` | pod repository/pod detail | LED protocol and QEMU adapter tests | Unverified |
| Touch | `services/touchService.hpp` | `commands/touch.rs` | touch-event repository stream | touch protocol and platform-input tests | Unverified |
| IMU | `services/imuService.hpp` | `commands/imu.rs` | feature/repository paths | IMU triage and QEMU adapter tests | Unverified |
| Haptic | `interfaces/iHapticDriver.hpp` | bounded feedback command | bounded repository operation | feedback controller and QEMU adapter tests | Unverified |
| Audio | `services/audioService.hpp` | volume and bounded feedback commands | device-owned volume state/control | feedback controller/audio tests | Unverified |

## Unresolved physical evidence matrix

All entries below remain **Unverified** for both serialized NFF boards. No software result or
accepted command upgrades any entry.

| Requirement | Board 1 | Board 2 |
| --- | --- | --- |
| Observed LED, touch, IMU, haptic, and audio behavior | Unverified | Unverified |
| Audio loudness/quality and haptic detectability/quality | Unverified | Unverified |
| Idle, radio, LED, audio, haptic, and combined current/transients | Unverified | Unverified |
| Power behavior and rails | Unverified | Unverified |
| Physical timing and latency | Unverified | Unverified |
| RF behavior | Unverified | Unverified |
| Exact populated-part confirmation | Unverified | Unverified |

## Decisions, discoveries, and deviations

- Issue 138 is open with `agent:running`; no open pull request or controller workspace duplicates
  this interface slice. Other FS issues own distinct scoring, QEMU-link, and fault-replay work.
- Message acceptance means queued audio or driver-accepted haptic triggering, never sensed output.
- The Linux release build passed. An iOS no-codesign build was not run because this worker is not
  on macOS; it is not recorded as passed.
- The full software gate reports hardware `NOT_ASSESSED`; no hardware operation or transport was
  searched for or invoked.

## Resume checkpoint

Implementation and the complete software gate are complete. Publish this exact candidate on the
single issue-138 branch, record the implementation commit and PR below, and leave review and all
physical evidence to the independent stages.
