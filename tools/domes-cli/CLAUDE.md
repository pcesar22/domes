# DOMES CLI - Claude Code Guidelines

## Project Overview

Rust CLI tool for communicating with DOMES firmware over USB serial or WiFi TCP.

## Architecture

```
src/
├── main.rs           # CLI argument parsing (clap)
├── proto.rs          # Generated protobuf modules
├── protocol/
│   └── mod.rs        # Frame encoding, protobuf helpers
├── transport/
│   └── mod.rs        # Serial and TCP transports
└── commands/
    ├── mod.rs        # Command exports
    ├── feature.rs    # Feature list/enable/disable
    ├── wifi.rs       # WiFi enable/disable/status
    ├── ota.rs        # OTA firmware updates
    └── trace.rs      # Performance tracing
```

## Protocol

All protocol definitions come from `firmware/common/proto/*.proto` files. The CLI uses prost to generate Rust types at build time.

**NEVER hand-roll protocol types. Use the generated prost types.**

### Frame Format

```
[0xAA 0x55][Len:u16le][MsgType:u8][Payload][CRC32:u32le]
```

### Message Type Ranges

| Range | Protocol |
|-------|----------|
| 0x01-0x05 | OTA |
| 0x10-0x17 | Trace |
| 0x20-0x25 | Config |

## Adding New Commands

1. Add message types to the appropriate `.proto` file
2. Rebuild to generate prost types: `cargo build`
3. Create command file in `src/commands/`
4. Add subcommand to `main.rs`
5. Export from `src/commands/mod.rs`

## Testing

```bash
# Unit tests
cargo test

# Test against device (serial)
cargo run -- --port /dev/ttyACM0 feature list

# Test against device (WiFi)
cargo run -- --wifi 192.168.1.100:5000 feature list
```

## Common Issues

### "Connection refused" on WiFi
- Ensure device is on same network
- Check TCP server is running (port 5000)
- From WSL2, use Windows Python - WSL2 can't reach WiFi devices directly

### "Permission denied" on serial
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Protobuf changes not reflected
```bash
cargo clean && cargo build
```
