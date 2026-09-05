# FS1 feedback interface software evidence

Status: implemented and software-verified; awaiting independent review
Current phase: publication
Specification revision: `93b8e7d3d95001290e2cebde7851f9b686f1d921`
Repository state: PR 145, corrected implementation commit
`19379f6681336fcaeb6bf6b787a2e4238fc96eab`, required base
`d9f84e4eca153d1f637b869681eae6e04a6adac6`
Last updated: 2026-08-21; issue 138 and its resumable PR 145 are the sole live owners of this slice

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
- [x] Committed, pushed, opened PR 145 against `main`, and reconciled its exact head.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | `tools/generate_protocols.sh --check all` | passed; full-gate protocol log below |
| Automated | CMake configure/build and `ctest --output-on-failure` | passed; 328 tests |
| Automated | Cargo fmt, strict Clippy, locked debug/release builds, all targets/features | passed; 105 unit and 13 integration tests |
| Automated | locked Flutter restore, fatal analysis, full tests, Linux release build | passed; 179 tests and release bundle |
| Automated | `python3 tools/docs/check_markdown_links.py` | passed; 108 files and 448 links |
| Automated | full `scripts/verify.sh` with ESP-IDF 5.4.4 and isolated SDKCONFIG | passed; 6 of 6 software checks |
| Accepted command | immutable reviewed-head device command | unavailable to implementation worker; not attempted |
| Physical confirmation | direct board observation and measurement | unverified; deferred to separate verifier |

The prior reviewed head `fa616e9afa765e0f90a509e920fa48837027f915` passed exact-head Software
CI run [32548799633](https://github.com/pcesar22/domes/actions/runs/32548799633), including the
macOS iOS no-codesign build. That CI result is historical evidence only and does not verify corrected
implementation commit `19379f6681336fcaeb6bf6b787a2e4238fc96eab`. Fresh local rework evidence is
retained privately; the final pushed-head CI result is recorded in
PR 145 because a commit cannot contain the identifier of its own future CI
run. The rework environment uses Rust 1.92.0, Flutter 3.44.8/Dart 3.12.2, Dart `protoc_plugin`
25.0.0, and ESP-IDF 5.4.4. The Linux worker cannot execute a macOS iOS build; only the historical
exact-head result above is recorded as passed.

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
scripts/verify.sh
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

| Requirement | Registered Pod 1 | Registered Pod 2 |
| --- | --- | --- |
| Observed LED, touch, IMU, haptic, and audio behavior | Unverified | Unverified |
| Audio loudness/quality and haptic detectability/quality | Unverified | Unverified |
| Idle, radio, LED, audio, haptic, and combined current/transients | Unverified | Unverified |
| Power behavior and rails | Unverified | Unverified |
| Physical timing and latency | Unverified | Unverified |
| RF behavior | Unverified | Unverified |
| Exact populated-part confirmation | Unverified | Unverified |

## Decisions, discoveries, and deviations

- Issue 138 is open for rework and PR 145 is its sole resumable pull request; no other issue, pull
  request, controller workspace, or tracked plan owns this interface slice. Other FS issues own
  distinct scoring, QEMU-link, and fault-replay work.
- The six feedback request/response IDs occupy reserved gaps `0x2C`-`0x2F` and `0x4A`-`0x4B`
  inside the established `0x20`-`0x4F` config-command range; `0x50` remains the unsolicited touch
  notification.
- Message acceptance means queued audio or driver-accepted haptic triggering, never sensed output.
- The Linux release build passed. The corrected head's iOS no-codesign build was not run locally
  because this worker is not on macOS; it is not recorded as passed.
- The full software gate reports hardware `NOT_ASSESSED`; no hardware operation or transport was
  searched for or invoked.
- Final pre-publication reconciliation found issue 138 open, PR 145 as the only PR owning this
  slice, base branch `main`, and no competing controller workspace. Corrected implementation commit
  `19379f6681336fcaeb6bf6b787a2e4238fc96eab` descends from required base
  `d9f84e4eca153d1f637b869681eae6e04a6adac6` through reconciliation commit
  `67684fd18b32f9ff05bc7a990f29a6f8226b1a08`.

## Resume checkpoint

Implementation, publication, and the complete software gate are complete. PR 145 is ready for
independent review. Leave approval, merge, and all physical evidence to their authorized stages.
