# Protocol Schemas And Generated Bindings

`config.proto` and `trace.proto` are the source of truth for config and trace messages. Nanopb C
bindings and Flutter Dart bindings are committed; Rust prost bindings are generated in Cargo's build
directory.

## Generate All Committed Bindings

From the repository root:

```bash
dart pub global activate protoc_plugin 25.0.0
tools/generate_protocols.sh
```

The script uses the checked-in nanopb generator, generates both `config` and `trace` C bindings, and
generates the Flutter app's `config` bindings. Use its check mode in local verification and CI:

```bash
tools/generate_protocols.sh --check
```

Generate or check a single consumer with `nanopb` or `dart` as the final argument. Build the Rust
consumer separately:

```bash
cargo build --locked --manifest-path tools/domes-cli/Cargo.toml
```

## Change Rules

1. Edit `.proto` and `.options` files, never generated files.
2. Regenerate every committed consumer.
3. Review generated diffs for changed IDs, field bounds, and removed messages.
4. Build the firmware, CLI, and Flutter app where affected.
5. Run host protocol tests and exercise a real transport for wire-level changes.

The ordinary ESP-IDF build compiles committed nanopb output; it does not regenerate it.

## Message-Type Ranges

| Range | Ownership |
| --- | --- |
| `0x01-0x05` | Legacy fixed-binary OTA transfer frames; not defined in these schemas |
| `0x10-0x1B` | Trace requests, responses, data, and metadata from `trace.proto` |
| `0x20-0x4F` | Config, feature, system, and diagnostic command requests/responses from `config.proto`, with reserved gaps |
| `0x50` | Unsolicited device-originated `TouchEventNotification` from `config.proto` |

`0x50` is not part of the command request range. Its payload is a bare protobuf and therefore has
no command-status byte.

## Response Envelope

The outer frame payload is not always a bare protobuf. Most config command responses use:

```text
[Status:u8][Protobuf response]
```

List and diagnostic responses that do not return command status, plus unsolicited notifications,
contain the protobuf directly. The firmware sender and each host decoder must agree on the envelope
for that message. The status byte is an established config-protocol wrapper, not a protobuf field
and not a precedent for new manually defined message families.

OTA transfer frames and the internal ESP-NOW peer protocol are bounded fixed-binary exceptions.
They are not defined in this directory and must not be used as a precedent for new host protocols.
