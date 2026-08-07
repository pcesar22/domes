# Portable peer/drill semantic and legacy-byte contract

Status: active
Current phase: first-slice implementation, review repairs, and final independent review complete;
publication and exact-head CI pending
Repository state: `codex/feat/portable-peer-drill-contract` from accepted `origin/main`
`ead26725804feea5e37e453b26f5c4115fab304a`
Last updated: 2026-08-07; first portable contract slice implemented, repaired, and locally validated

## Objective and observable outcome

Advance the first FS-WP-003A slice by defining the ten current peer messages once as the generated
`domes.peer_drill` oneof, adding a production-owned codec that maps that semantic type to and from
the exact current Legacy-V1 ESP-NOW bytes, defining sender roles once in the same schema, and making
the native functional simulator exercise the generated semantic message through those production
bytes. Exact-byte fixtures and per-language validators must reject malformed inputs and invalid
sender direction without changing the live firmware transport or packet path.

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
| Schema | `firmware/common/proto/peer_drill.proto`, `.options` | Ten variants whose generated tags own the Legacy type IDs, generated roles, fixed32 tokens, exact MAC contract |
| Nanopb | `peer_drill.pb.c`, `peer_drill.pb.h` | Commit bounded generated C storage used by the production codec |
| Compatibility codec | `firmware/common/protocol/peerDrillCodec.*` | Validate bounds and role direction; encode/decode exact Legacy-V1 bytes |
| Native simulator | `firmware/test_app/sim/` | Use generated semantics, production bytes, authoritative registered roles, fail-closed errors, and scoped handlers |
| Rust | `tools/domes-cli/build.rs`, `src/proto.rs` | Generate prost types and enforce the same bounds/role contract in a Rust validator |
| Dart | generated bindings, `peer_drill_validator.dart`, focused test | Generate Dart types and enforce the same bounds/role contract in a Dart validator |
| Generation | `tools/generate_protocols.sh`, proto README | Exhaustively guard schema IDs/roles, then generate and drift-check every committed consumer |

## Stages and dependencies

- [x] Reconciled exact `origin/main`, clean branch, PR #105 boundary, program ledger, architecture,
  all ten live packet layouts, simulator divergence, and generated consumers.
- [x] Completed pre-edit read-only repository exploration and protocol design routing.
- [x] Define and generate the semantic oneof and role enum, then implement the bounded production
  Legacy-V1 validation/codec boundary with exact-byte, malformed-input, and role-matrix tests.
- [x] Migrate the native functional simulator to generated semantics and production codec bytes;
  keep delivery/replay metadata outside the oneof.
- [x] Run nanopb drift, host simulator/codec, prost, Dart, and isolated ESP-IDF v5.4.4 checks.
- [x] Repair independent review findings: generated type-ID ownership, portable validation and
  role semantics, authoritative simulator sender identity, fail-closed drill results, and scoped
  callback teardown for delayed deliveries.
- [x] Prepare the first-slice candidate as an initial mission commit plus a review-repair commit
  with reproducible local evidence.
- [x] Complete final independent protocol and firmware publication review with no remaining
  confirmed defects or publication blockers.
- [ ] **Current:** Publish the review-ready branch and pass exact-head CI; this plan remains active
  because FS-WP-003A is not complete.
- [ ] Later: adopt the codec in live firmware and converge operational CLI/app behavior; depends on
  independent review of this slice.
- [ ] Later: prove rolling compatibility on two boards; depends on separately authorized hardware
  access and retained identity-bound evidence.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Generated drift | `tools/generate_protocols.sh --check all` | passed; exhaustive descriptor guard plus nanopb and Dart generated-output drift checks are clean |
| Exact bytes and native simulator | fresh CMake build, full CTest, normal trace generation, and focused ASan/UBSan teardown regressions | passed; 310/310 full suite, valid 116-event trace, and 3/3 sanitizer regressions |
| Rust generated type and validator | Cargo fmt, strict clippy, unit and integration tests with repository toolchain | passed; 102 unit and 10 integration tests |
| Dart generated type and validator | focused protocol test plus fatal-info/fatal-warning analysis | passed 4/4 and analysis clean with installed Flutter 3.38.9; a full-suite attempt stalled after 53 tests and was stopped, so pinned Flutter 3.44.8 CI remains authoritative |
| Firmware integration | fresh isolated ESP-IDF v5.4.4 ESP32-S3 build and isolated `SDKCONFIG` | passed; `domes.bin` built with 25% of the smallest app partition free |
| Changed-scope repository gate | `scripts/verify.sh --quick --changed ead2672` | protocol, 310 host, and CLI checks passed; host tooling could not start because pre-commit 4.6.1 is absent, Flutter rejected local 3.38.9 instead of pinned 3.44.8, and firmware was intentionally covered by the separate isolated build |
| Physical confirmation | rolling two-board Legacy-V1 compatibility | not run; hardware access prohibited in this cycle |

## Decisions, discoveries, and deviations

- The semantic envelope carries protocol version, exact six-byte sender MAC, one fixed32 timestamp,
  and one generated oneof. The timestamp is local sender time except that PONG echoes PING time;
  the semantic metadata is not inserted into the unchanged Legacy-V1 bytes.
- Four round-scoped variants use non-zero fixed 32-bit tokens: arm, simulated touch, touch event,
  and timeout event.
- Feedback values are the existing bounded bitmask values 0 through 3; pad indices are 0 through
  3; protobuf RGB channels must fit one legacy byte.
- Nanopb provides bounded storage; explicit C++, Rust, and Dart validators distinguish or reject
  malformed input, unknown payload, unsupported semantic version, bad MAC length, invalid enum,
  out-of-range channel/pad, zero round token, and invalid sender role.
- The simulator may retain routing, virtual time, sequence, delivery action/delay, and replay cursor
  in its transport wrapper. Its semantic payload and replay identity use the generated oneof and
  actual production Legacy-V1 bytes.
- The simulator transport boundary derives role from the registered source pod, rejects unknown or
  conflicting roles, requires source metadata to match the generated sender MAC, propagates codec
  failures to the drill result/trace-generator exit, and restores temporary handlers before stack
  receivers leave scope.

## Resume checkpoint

This independently reviewed first slice is ready for publication. Resume FS-WP-003A with live
`EspNowService` codec adoption and operational CLI/app convergence only after this branch passes
exact-head CI and review. Keep Legacy-V1 radio bytes stable, preserve rolling compatibility, and
treat the separately authorized two-board campaign as the eventual physical exit; neither attached
pod was accessed by this slice.
