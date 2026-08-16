# Portable Peer And Drill Contract

FS-WP-003A replaces the simulator-owned message hierarchy with one generated protobuf contract in
`firmware/common/proto/peer_drill.proto`. Firmware nanopb, Rust prost, and Flutter Dart outputs are
generated from that schema.

## Compatibility Boundary

The deployed ESP-NOW version-1 bytes remain unchanged: the first byte retains the existing message
IDs and every fixed message remains 11-20 bytes. Firmware decodes that compatibility layout into a
generated `PeerMessage` before dispatch. Native functional peers, the CLI, and Flutter consume the
protobuf encoding directly. Any future radio wire migration requires a new contract version and
retained compatibility evidence.

The contract owns master/slave roles, lifecycle states, round tokens, and all discovery, control,
simulation-injection, touch, and timeout variants. The old `SimMessage` variant hierarchy and its
simulator-only `PLAY_SOUND` message no longer exist; the functional bus carries the nanopb-generated
`PeerMessage` losslessly.

## Fail-Closed Rules

Firmware, Rust, and Dart consumers reject empty, malformed, truncated, oversized, unknown,
role-invalid, state-invalid, and semantically invalid inputs. Round tokens are nonzero, timeouts are
bounded to 60 seconds, feedback modes are bounded to the production bitmask, colors are bytes, and
touch pads are 0-3.

## Evidence Boundary

Software checks exercise all generated variants and retained ESP-NOW wire sizes. They do not prove
radio compatibility or physical behavior. FS-WP-003A remains acceptance-pending until the exact
reviewed commit passes required Software CI and the separate verification worker retains the
specified two-board discovery, complementary-role, bidirectional benchmark, and traced-drill
evidence.
