# Portable peer/drill semantic and legacy-byte contract

Status: active
Current phase: first-slice implementation and local validation complete; independent publication
review pending
Repository state: `codex/feat/portable-peer-drill-contract` from accepted `origin/main`
`ead26725804feea5e37e453b26f5c4115fab304a`
Last updated: 2026-08-07; first portable contract slice implemented and locally validated

## Objective and observable outcome

Advance the first FS-WP-003A slice by defining the ten current peer messages once as the generated
`domes.peer_drill` oneof, adding a production-owned codec that maps that semantic type to and from
the exact current Legacy-V1 ESP-NOW bytes, and making the native functional simulator exercise the
generated semantic message through those production bytes. Exact-byte fixtures and negative tests
must reject malformed inputs without changing the live firmware transport or packet path.

This slice does not complete FS-WP-003A. Live `EspNowService` adoption, app and CLI operational
convergence, and rolling two-board compatibility remain separate work after independent review.

## Authorities and contracts

- Authority: `firmware/common/proto/peer_drill.proto` - portable peer/drill semantic types.
- Authority: `firmware/common/protocol/peerDrillCodec.*` - explicit Legacy-V1 compatibility
  boundary.
- Compatibility baseline: `firmware/domes/main/services/espNowProtocol.hpp` - the ten live packet
  IDs, exact sizes, and field order that this slice must preserve without modifying its callers.
- Authority: `research/architecture/13-deterministic-virtual-platform.md` - production-codec and
  simulator-convergence intent and later target backplane dependency.
- Preserve: ESP-NOW carries only the current packed Legacy-V1 bytes, never protobuf wire bytes.
- Preserve: destination addressing, RSSI, simulator sequence/fault metadata, delivery scheduling,
  and replay state remain outside the generated semantic oneof.
- Preserve: transmission behavior, callbacks, pending-frame capacity, role election, shared host
  frame, and response-status envelopes remain unchanged.
- Exclude: `PlaySoundCommand` is simulator-only and is not a production peer variant.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Schema | `firmware/common/proto/peer_drill.proto`, `.options` | Ten bounded variants, fixed32 round tokens, exact MAC contract |
| Nanopb | `peer_drill.pb.c`, `peer_drill.pb.h` | Commit generated C representation used by production codec |
| Compatibility codec | `firmware/common/protocol/peerDrillCodec.*` | Validate and encode/decode exact Legacy-V1 bytes |
| Native simulator | `firmware/test_app/sim/` | Replace handwritten semantic variant with generated oneof and production bytes |
| Rust | `tools/domes-cli/build.rs`, `src/proto.rs` | Generate prost module and run semantic fixture tests |
| Dart | `ios/domes_app/lib/data/proto/generated/`, focused test | Generate Dart type and run oneof/fixture tests |
| Generation | `tools/generate_protocols.sh`, proto README | Generate and drift-check every committed consumer |

## Stages and dependencies

- [x] Reconciled exact `origin/main`, clean branch, PR #105 boundary, program ledger, architecture,
  all ten live packet layouts, simulator divergence, and generated consumers.
- [x] Completed pre-edit read-only repository exploration and protocol design routing.
- [x] Define and generate the bounded semantic oneof, then implement the production
  Legacy-V1 validation/codec boundary with exact-byte and negative tests.
- [x] Migrate the native functional simulator to generated semantics and production codec bytes;
  keep delivery/replay metadata outside the oneof.
- [x] Run nanopb drift, host simulator/codec, prost, Dart, and isolated ESP-IDF v5.4.4 checks.
- [x] Run post-diff protocol and firmware reviews and self-review; both specialist reviews report
  no remaining findings after the simulator sender/source identity check was added.
- [x] Prepare the reviewed first-slice candidate for one local commit with reproducible evidence.
- [ ] **Current:** Complete independent publication review; this plan remains active because the
  package is not complete.
- [ ] Later: adopt the codec in live firmware and converge operational CLI/app behavior; depends on
  independent review of this slice.
- [ ] Later: prove rolling compatibility on two boards; depends on separately authorized hardware
  access and retained identity-bound evidence.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Generated drift | `tools/generate_protocols.sh --check all` with pinned local Dart tools | passed; nanopb, prost inputs, and Dart generated outputs clean |
| Exact bytes and native simulator | fresh CMake build, full CTest, then focused post-review regression | passed; final 301/301 full suite and 44/44 focused codec/simulator/drill tests |
| Rust generated type | Cargo fmt, strict clippy, unit and integration tests with repository toolchain | passed; 100 unit and 10 integration tests |
| Dart generated type | focused generated oneof/fixed32 fixture test | passed 2/2 with installed Flutter 3.38.9; repository-pinned Flutter 3.44.8 unavailable locally |
| Firmware integration | fresh isolated ESP-IDF v5.4.4 ESP32-S3 build and isolated `SDKCONFIG` | passed; `domes.bin` built with 25% of the smallest app partition free |
| Physical confirmation | rolling two-board Legacy-V1 compatibility | not run; hardware access prohibited in this cycle |

## Decisions, discoveries, and deviations

- The semantic envelope carries protocol version, exact six-byte sender MAC, truncated 32-bit
  sender timestamp, and one generated oneof. The version is semantic compatibility metadata; it is
  not inserted into the unchanged Legacy-V1 bytes.
- Four round-scoped variants use non-zero fixed 32-bit tokens: arm, simulated touch, touch event,
  and timeout event.
- Feedback values are the existing bounded bitmask values 0 through 3; pad indices are 0 through
  3; protobuf RGB channels must fit one legacy byte.
- Decoder errors distinguish malformed input, unknown legacy type, unsupported semantic version,
  exact-length mismatch, invalid enum, out-of-range channel/pad, and zero round token.
- The simulator may retain routing, virtual time, sequence, delivery action/delay, and replay cursor
  in its transport wrapper. Its semantic payload and replay identity use the generated oneof and
  actual production Legacy-V1 bytes.
- The simulator transport boundary also requires its source-pod metadata to match the generated
  message's exact sender MAC, mirroring live firmware's fail-closed source validation.

## Resume checkpoint

This reviewed first slice is ready for a local-only commit and external publication review. Resume
FS-WP-003A with live `EspNowService` codec adoption and operational CLI/app convergence only after
that review. Keep Legacy-V1 radio bytes stable, preserve rolling compatibility, and treat the
separately authorized two-board campaign as the eventual physical exit; neither attached pod was
accessed by this slice.
