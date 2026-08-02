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
cargo build --manifest-path tools/domes-cli/Cargo.toml
```

## Change Rules

1. Edit `.proto` and `.options` files, never generated files.
2. Regenerate every committed consumer.
3. Review generated diffs for changed IDs, field bounds, and removed messages.
4. Build the firmware, CLI, and Flutter app where affected.
5. Run host protocol tests and exercise a real transport for wire-level changes.

The ordinary ESP-IDF build compiles committed nanopb output; it does not regenerate it.

OTA transfer frames and the internal ESP-NOW peer protocol are bounded fixed-binary exceptions.
They are not defined in this directory and must not be used as a precedent for new host protocols.
