# DOMES CLI - AI Development Guide

Instructions for AI assistants working on the domes-cli codebase.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.rs                                  │
│  - Clap argument parsing                                        │
│  - Transport selection (serial vs TCP)                          │
│  - Command dispatch                                             │
└──────────────────────────┬──────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                 ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   commands/     │ │   transport/    │ │   protocol/     │
│                 │ │                 │ │                 │
│ feature.rs      │ │ mod.rs (trait)  │ │ mod.rs          │
│ rgb.rs          │ │ serial.rs       │ │ - ConfigMsgType │
│ wifi.rs         │ │ tcp.rs          │ │ - serialize_*   │
│ ota.rs          │ │ frame.rs        │ │ - parse_*       │
└────────┬────────┘ └────────┬────────┘ └────────┬────────┘
         │                   │                   │
         └───────────────────┼───────────────────┘
                             ▼
                    ┌─────────────────┐
                    │    proto.rs     │
                    │                 │
                    │ Generated types │
                    │ + CLI helpers   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │   build.rs      │
                    │                 │
                    │ prost codegen   │
                    │ from .proto     │
                    └─────────────────┘
```

## Critical Rules

### 1. Protocol Buffers are the Source of Truth

**NEVER hand-roll protocol types.** All message types come from:
- Source: `firmware/common/proto/config.proto`
- Generated: `target/*/build/domes-cli-*/out/domes.config.rs`

```rust
// WRONG - Don't define enums manually
pub enum Feature {
    LedEffects = 1,
    Ble = 2,
}

// RIGHT - Use generated types
use crate::proto::config::Feature;
```

### 2. Message Type Ranges

Frame-level message types (NOT protobuf, just byte identifiers):

| Range | Purpose |
|-------|---------|
| `0x01-0x05` | OTA protocol |
| `0x10-0x1F` | Trace protocol |
| `0x20-0x2F` | Feature control |
| `0x30-0x3F` | RGB pattern control |

When adding new commands, pick the next available range.

### 3. Adding a New Command - Complete Checklist

1. **Update proto file** (`firmware/common/proto/config.proto`):
   ```protobuf
   enum NewThing { ... }
   message SetNewThingRequest { ... }
   message SetNewThingResponse { ... }
   ```

2. **Regenerate nanopb** (for firmware):
   ```bash
   cd firmware/common
   python3 ../third_party/nanopb/generator/nanopb_generator.py proto/config.proto
   ```

3. **Add message types** to `src/protocol/mod.rs`:
   ```rust
   pub enum ConfigMsgType {
       // ... existing ...
       SetNewThingReq = 0x40,
       SetNewThingRsp = 0x41,
   }
   ```

4. **Add serialization** to `src/protocol/mod.rs`:
   ```rust
   pub fn serialize_set_new_thing(...) -> Vec<u8> { ... }
   pub fn parse_set_new_thing_response(...) -> Result<...> { ... }
   ```

5. **Create command module** `src/commands/newthing.rs`:
   ```rust
   pub fn newthing_set(transport: &mut dyn Transport, ...) -> Result<...> { ... }
   ```

6. **Export from** `src/commands/mod.rs`:
   ```rust
   pub mod newthing;
   pub use newthing::newthing_set;
   ```

7. **Add CLI subcommand** to `src/main.rs`:
   ```rust
   enum Commands {
       NewThing { #[command(subcommand)] action: NewThingAction },
   }

   enum NewThingAction { ... }
   ```

8. **Add proto extensions** to `src/proto.rs` (for cli_name, FromStr, etc.)

## Code Patterns

### Transport Abstraction

Commands receive `&mut dyn Transport`, allowing serial or TCP:

```rust
pub fn my_command(transport: &mut dyn Transport) -> Result<Response> {
    let payload = serialize_request(...);
    let frame = transport.send_command(MsgType::MyReq as u8, &payload)?;

    if frame.msg_type != MsgType::MyRsp as u8 {
        anyhow::bail!("Unexpected response type");
    }

    parse_response(&frame.payload)
}
```

### Frame Encoding

```rust
// Frame format: [0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
// - Start: 2 bytes (0xAA, 0x55)
// - Length: 2 bytes LE (type + payload size)
// - Type: 1 byte
// - Payload: 0-1024 bytes
// - CRC32: 4 bytes LE (over type + payload)
```

### Protobuf Serialization

```rust
use prost::Message;

// Encoding
let req = SetFeatureRequest { feature: f as i32, enabled: true };
let payload = req.encode_to_vec();

// Decoding
let resp = SetFeatureResponse::decode(&payload[..])?;
```

### CLI Argument Parsing

Uses Clap derive macros:

```rust
#[derive(Subcommand)]
enum MyAction {
    /// Help text shown in --help
    DoThing {
        /// Argument description
        #[arg(short, long)]
        value: Option<String>,

        #[arg(short, long, default_value = "50")]
        speed: u32,
    },
}
```

### proto.rs Extensions

Add user-friendly methods to generated types:

```rust
impl Feature {
    pub fn cli_name(&self) -> &'static str { ... }
    pub fn from_cli_name(s: &str) -> Option<Feature> { ... }
}

impl std::str::FromStr for Feature { ... }
impl std::fmt::Display for Feature { ... }
```

## Common Pitfalls

### 1. Forgetting to Update TryFrom

When adding message types to `ConfigMsgType`, also update:
```rust
impl TryFrom<u8> for ConfigMsgType {
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            // ADD NEW TYPES HERE
        }
    }
}
```

### 2. Wrong Response Type Check

Always verify the response type matches expected:
```rust
if frame.msg_type != ConfigMsgType::ExpectedRsp as u8 {
    anyhow::bail!("Unexpected response");
}
```

### 3. Forgetting cargo build After Proto Changes

Prost generates code at build time. After changing `.proto`:
```bash
cargo build  # Regenerates Rust types
```

### 4. Status Byte in Responses

Some responses have a status byte prefix before protobuf:
```rust
// Format: [status_byte][protobuf_data]
let status = payload[0];
let proto_data = &payload[1..];
```

Check if firmware sends status byte for your message type.

### 5. Using anyhow vs thiserror

- `anyhow::Result` for command functions (user-facing errors)
- `thiserror` for protocol errors (typed, matchable)

```rust
// In protocol/mod.rs
#[derive(Debug, Error)]
pub enum ProtocolError {
    #[error("Unknown feature ID: {0}")]
    UnknownFeature(i32),
}

// In commands/*.rs
pub fn my_cmd() -> anyhow::Result<Response> {
    // anyhow for context
    let result = parse_thing().context("Failed to parse response")?;
}
```

### 6. Timeout Handling

Default receive timeout is 2000ms. OTA commands need longer:
```rust
transport.receive_frame(10000)  // 10 second timeout for OTA
```

## Testing

### Manual Testing

```bash
# Build
cargo build --release

# Test with device
./target/release/domes-cli --port /dev/ttyACM0 feature list
./target/release/domes-cli --port /dev/ttyACM0 rgb rainbow
```

### Unit Tests

```bash
cargo test
```

Tests are in `tests/` directory using `assert_cmd` for CLI testing.

## File Reference

| File | Purpose |
|------|---------|
| `main.rs` | Entry point, Clap setup, command dispatch |
| `commands/*.rs` | Command implementations (one per feature area) |
| `transport/mod.rs` | `Transport` trait definition |
| `transport/serial.rs` | USB serial implementation |
| `transport/tcp.rs` | TCP/WiFi implementation |
| `transport/frame.rs` | Frame encode/decode, CRC32 |
| `protocol/mod.rs` | Message types, serialize/parse functions |
| `proto.rs` | Generated type extensions (cli_name, FromStr) |
| `build.rs` | Prost code generation from .proto |

## Dependencies

| Crate | Purpose |
|-------|---------|
| `clap` | CLI argument parsing |
| `serialport` | USB serial communication |
| `prost` | Protobuf runtime |
| `prost-build` | Protobuf code generation |
| `crc32fast` | CRC32 calculation |
| `anyhow` | Error handling |
| `thiserror` | Error type definitions |
| `sha2` | SHA256 for OTA verification |

## See Also

- `firmware/common/proto/config.proto` - Protocol definitions (source of truth)
- `firmware/domes/main/config/configProtocol.hpp` - Firmware message types
- `firmware/domes/main/config/configCommandHandler.cpp` - Firmware handlers
