# 04 - Communication Decisions

> **Document status: Design proposal, partially implemented.** This record preserves the useful
> communication boundaries from the original proposal without duplicating live packet IDs, packed
> structures, GATT UUIDs, or transport commands. Use the authorities below for current behavior.

## Original Scope

The proposal explored pod discovery, host-to-pod control, pod-to-pod game traffic, BLE topology,
radio coexistence, and future clock synchronization. Parts of that direction were implemented, but
the original protocol tables and class sketches did not become the production contract.

## Current Authorities

| Concern | Authority |
| --- | --- |
| As-built protocol and transport boundaries | [`../SOFTWARE_ARCHITECTURE.md`](../SOFTWARE_ARCHITECTURE.md) |
| Host config and trace schemas | [`../../firmware/common/proto/`](../../firmware/common/proto/) |
| Shared host frame codec | [`../../firmware/common/protocol/frameCodec.hpp`](../../firmware/common/protocol/frameCodec.hpp) |
| OTA transfer contract | [`../../firmware/common/protocol/otaProtocol.hpp`](../../firmware/common/protocol/otaProtocol.hpp) and its Rust/Dart consumers |
| Internal ESP-NOW contract | [`../../firmware/domes/main/services/espNowProtocol.hpp`](../../firmware/domes/main/services/espNowProtocol.hpp) |
| Firmware transports | [`../../firmware/domes/main/transport/`](../../firmware/domes/main/transport/) |
| Operator-facing commands | [`../../tools/domes-cli/README.md`](../../tools/domes-cli/README.md) and `domes-cli --help` |
| Verification requirements | [`../../docs/TESTING.md`](../../docs/TESTING.md) |

## As-Built Boundaries

DOMES currently has three deliberately separate wire families:

1. Host config messages plus trace control and metadata use the shared framed transport and protobuf
   schemas. Compact recorder events remain a bounded internal representation carried by the trace
   protocol. UART, BLE, and build-gated TCP config consumers must agree with the paired firmware
   handler on each response envelope.
2. Serial and BLE OTA use a bounded fixed-binary transfer contract inside the shared outer frame.
   The TCP config server does not route raw image transfer.
3. ESP-NOW discovery and game traffic use an internal packed protocol owned by the firmware and
   mirrored by host simulation. It is not a host config protocol.

The active NFF board keeps framed UART traffic on the CP2102N-backed UART and console logs on native
USB Serial/JTAG. BLE carries framed config, trace control/dump, and OTA. Optional WiFi builds expose
config and a separate live trace endpoint; availability of a WiFi feature does not imply raw TCP OTA
support.

## Protocol Messages

This record intentionally does not reproduce message values. Host config and trace values come from
the protobuf schemas, OTA transfer values come from `otaProtocol.hpp` and its consumers, and peer
game values come from `espNowProtocol.hpp`. Those owners, not the original proposal, define the live
contract.

## Decisions Retained

- Keep host control, raw-image transfer, and peer-game traffic as explicit protocol families rather
  than treating every byte stream as one command bus.
- Keep radio callbacks bounded and move parsing or application work into task context.
- Validate message length and source identity before dispatching peer traffic.
- Correlate a game event with the active round so a delayed event cannot complete a later round.
- Derive current two-pod roles deterministically instead of persisting a preferred master.
- Treat connection, peer discovery, packet exchange, and physical game behavior as separate
  verification claims.
- Keep WiFi/BLE coexistence and initialization order explicit in the firmware composition root.

## Targets Not Implemented As Proposed

- A phone-selected ESP-NOW master and general multi-pod drill interpreter.
- A shared pod clock with verified cross-device timing correlation.
- BLE mesh or a BLE-to-ESP-NOW command proxy matching the original sketches.
- The proposal's generic sound, haptic, reboot, and peer-relayed OTA command set.
- Raw image upload through the TCP config server.

These remain product or research inputs, not reserved wire values. Any future host-facing message
must begin in the relevant protobuf schema. Any change to an existing fixed-binary exception must
update every consumer and its compatibility tests together.

## Historical Material Removed

The original packet-number tables, packed C++ examples, proposed UUIDs, latency assertions, and
step-by-step build commands were removed because they diverged from the implementation. Git history
retains them for design archaeology without presenting them as a second live protocol specification.
