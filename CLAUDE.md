# Claude Code Documentation

This project was created and is maintained with assistance from Claude Code, Anthropic's official CLI for Claude.

## MANDATORY: Verification After Implementation

**IMPORTANT**: After ANY code implementation, you MUST verify your changes work END-TO-END. Unit tests alone are NOT sufficient. Do NOT consider a task complete until the feature actually works on hardware.

### Verification Levels (ALL required for new features)

#### Level 1: Unit Tests
```bash
cd firmware/test_app && mkdir -p build && cd build && cmake .. && make && ctest --output-on-failure
```

#### Level 2: Build ALL affected components
```bash
# Firmware
cd firmware/domes && . ~/esp/esp-idf/export.sh && idf.py build

# Host tools (if modified)
cd tools/domes-cli && cargo build
```

#### Level 3: End-to-End Verification (REQUIRED for new features)
```bash
# Flash firmware to device
/flash

# Test the actual feature works by:
# 1. Running the host tool against the device
# 2. Checking serial output/logs for expected behavior
# 3. Verifying the feature does what it's supposed to do
```

### Feature-Specific Verification

| Feature Type | Verification Method |
|--------------|---------------------|
| Protocol/transport | Flash firmware, run host tool, verify communication works |
| Config/runtime | Flash firmware, use CLI to change settings, verify settings applied |
| LED/display | Flash firmware, visually confirm behavior |
| Sensors/input | Flash firmware, trigger input, verify response in logs |
| WiFi transport | Flash, connect to same network, run `domes-cli --wifi <IP>:5000 feature list` |
| Serial transport | Flash, run `domes-cli --port /dev/ttyACM0 feature list` |
| BLE transport | Flash, run `domes-cli --ble "DOMES-Pod" feature list` (requires native Linux) |
| OTA updates | Flash, run `domes-cli ota flash`, verify device reboots and responds |

### Available Skills & Commands

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `/flash` | Build, flash, monitor | **ALWAYS** after firmware changes |
| `/lint-fw` | Check coding standards | Before committing firmware code |
| `/size` | Analyze binary size | After adding new features |
| `/debug-esp32` | GDB debugging | When investigating runtime issues |

### Test Files

- **Firmware unit tests**: `firmware/test_app/main/test_*.cpp`
- **Protocol tests**: `test_frame_codec.cpp`, `test_ota_protocol.cpp`, `test_config_protocol.cpp`
- **Feature tests**: `test_feature_manager.cpp`
- **CLI integration tests**: Use `domes-cli` commands against device (serial, WiFi, or BLE)

**DO NOT** mark a task as complete if:
- Build fails
- Unit tests fail
- Firmware doesn't flash successfully
- **You haven't verified the feature works on actual hardware**
- Host tool doesn't communicate with device (for host<->device features)

---

## CRITICAL: Protocol Buffers (nanopb/prost) - READ THIS FIRST

**THIS IS NON-NEGOTIABLE. DO NOT SKIP THIS SECTION.**

All message serialization between firmware and host tools MUST use Protocol Buffers:
- **Firmware**: Uses **nanopb** (C library for embedded)
- **Host CLI (Rust)**: Uses **prost** (Rust protobuf library)
- **Single source of truth**: `firmware/common/proto/*.proto` files

### THE RULE

```
┌─────────────────────────────────────────────────────────────────┐
│  NEVER HAND-ROLL PROTOCOL DEFINITIONS.                         │
│  NEVER DUPLICATE ENUMS OR MESSAGE TYPES IN CODE.               │
│  ALL DEFINITIONS COME FROM .proto FILES.                       │
│  IF YOU CREATE A NEW MESSAGE TYPE, ADD IT TO THE .proto FILE.  │
└─────────────────────────────────────────────────────────────────┘
```

### Proto Files Location

```
firmware/common/proto/
├── config.proto       # Runtime config protocol (features, settings)
├── config.options     # nanopb options (max sizes, etc.)
└── (future protos)    # OTA, trace, etc.
```

### How It Works

```
config.proto (SOURCE OF TRUTH)
       │
       ├──► nanopb generator ──► config.pb.h / config.pb.c (firmware)
       │
       └──► prost build.rs ──► config.rs (CLI, generated at build time)
```

### Before Adding ANY Protocol Code

1. **CHECK** `firmware/common/proto/` for existing definitions
2. **ADD** new messages/enums to the `.proto` file
3. **GENERATE** code using nanopb (firmware) and prost (CLI)
4. **NEVER** manually define enums like `Feature`, `Status`, etc. in code

### Example: WRONG vs RIGHT

**WRONG** (what a dumbass would do):
```rust
// DO NOT DO THIS - hand-rolling protocol definitions
pub enum Feature {
    LedEffects = 1,
    Wifi = 3,  // Duplicating proto definitions!
}
```

**RIGHT** (use generated code):
```rust
// In build.rs: prost generates from config.proto
// In code: use the generated types
use crate::proto::config::{Feature, SetFeatureRequest};
```

### Key Files

| Component | File | Purpose |
|-----------|------|---------|
| Proto definitions | `firmware/common/proto/config.proto` | **THE SOURCE OF TRUTH** |
| nanopb options | `firmware/common/proto/config.options` | Size limits for embedded |
| CLI build script | `tools/domes-cli/build.rs` | Runs prost to generate Rust |
| Generated Rust | `tools/domes-cli/src/proto/` | **Generated, do not edit** |

### Adding a New Feature

1. Edit `firmware/common/proto/config.proto`:
   ```protobuf
   enum Feature {
       FEATURE_MY_NEW_THING = 8;  // Add here
   }
   ```

2. Regenerate firmware code (nanopb)
3. Rebuild CLI (prost runs automatically via build.rs)
4. Both sides now have the new feature ID

**DO NOT** add the feature to Rust code manually. It will be generated.

---

## About Claude Code

Claude Code is an interactive CLI tool that helps with software engineering tasks, including:
- Code generation and refactoring
- Bug fixing and debugging
- Documentation creation
- Test development
- Code review and optimization

## Working with Claude

When working on this project with Claude Code:

1. **Clear Communication**: Provide specific, detailed requirements for the best results
2. **Iterative Development**: Claude can help break down complex tasks into manageable steps
3. **Code Review**: Always review Claude's suggestions before committing changes
4. **Context Awareness**: Claude understands the project structure and can maintain consistency
5. **Always Verify**: Run tests after every implementation - no exceptions

## Project Sessions

Claude Code sessions are tracked to maintain context and ensure continuity across development tasks.

### Current Branch
This documentation was created on branch: `claude/create-documentation-files-gJwGB`

## Available Skills

The following skills are available in `.claude/skills/`:

### debug-esp32 (GDB Debugging)
**Trigger**: Use when user asks to "debug", "gdb", "breakpoint", "step through", "backtrace", "info stack", "inspect variable", or investigate crashes.

Provides ESP32-S3 GDB debugging via MCP tools. Key features:
- Start/stop debug sessions with OpenOCD + GDB
- Set breakpoints at functions or file:line
- Step through code (step into, step over, finish)
- Inspect variables, memory, registers
- View FreeRTOS tasks
- Full call stack (backtrace)

**IMPORTANT**: Always read `.claude/skills/debug-esp32/SKILL.md` before debugging to follow the correct workflow (async continue, reset halt, etc.)

### esp32-firmware
**Trigger**: Use when user asks to "build", "flash", "monitor", or work with ESP-IDF.

Provides ESP32 firmware build/flash/monitor commands.

### github-workflow
**Trigger**: Use for GitHub Actions and CI/CD tasks.

## Runtime Configuration Protocol

The firmware supports runtime feature toggles via a binary protocol over USB serial, WiFi TCP, or Bluetooth Low Energy (BLE).

**IMPORTANT**: See "CRITICAL: Protocol Buffers" section above. All message types are defined in `firmware/common/proto/config.proto`.

### Protocol Overview

- **Frame format**: `[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]`
- **Payload**: Protobuf-encoded messages (nanopb on firmware, prost on CLI)
- **Transports**: USB-CDC (serial), TCP port 5000 (WiFi), BLE GATT (notifications)

### Available Features (defined in config.proto)

| ID | Feature | Description |
|----|---------|-------------|
| 1 | FEATURE_LED_EFFECTS | LED animations and effects |
| 2 | FEATURE_BLE_ADVERTISING | Bluetooth Low Energy advertising |
| 3 | FEATURE_WIFI | WiFi connectivity |
| 4 | FEATURE_ESP_NOW | ESP-NOW peer-to-peer |
| 5 | FEATURE_TOUCH | Touch sensing |
| 6 | FEATURE_HAPTIC | Haptic feedback |
| 7 | FEATURE_AUDIO | Audio output |

**To add a new feature**: Edit `firmware/common/proto/config.proto`, NOT code files.

### Testing Config Protocol

```bash
# Test over USB serial
domes-cli --port /dev/ttyACM0 feature list
domes-cli --port /dev/ttyACM0 wifi status

# Test over WiFi (requires same network)
domes-cli --wifi <ESP32_IP>:5000 feature list

# Test over BLE (requires native Linux, not WSL2)
domes-cli --scan-ble                           # Discover devices
domes-cli --ble "DOMES-Pod" feature list       # Connect by name
domes-cli --ble "94:A9:90:0A:EA:52" led get    # Connect by MAC
```

### Key Files

| File | Purpose |
|------|---------|
| `firmware/common/proto/config.proto` | **SOURCE OF TRUTH** for protocol definitions |
| `firmware/common/proto/config.options` | nanopb size constraints |
| `tools/domes-cli/build.rs` | prost code generation for CLI |
| `firmware/domes/main/config/configCommandHandler.hpp` | Command handler (shared by serial/TCP/BLE) |
| `firmware/domes/main/config/featureManager.hpp` | Feature state management |
| `firmware/domes/main/transport/bleOtaService.hpp` | BLE GATT service (NimBLE) |
| `tools/domes-cli/src/transport/ble.rs` | CLI BLE transport (btleplug) |
| `tools/domes-cli/` | CLI tool for testing and OTA |

---

## OTA Firmware Updates

The `domes-cli` tool is the **only supported method** for OTA firmware updates. It supports both wired (serial) and wireless (WiFi/TCP) transports.

### Quick Usage

```bash
# Build the CLI (if not already built)
cd tools/domes-cli && cargo build --release

# Serial OTA (wired)
domes-cli --port /dev/ttyACM0 ota flash firmware/domes/build/domes.bin --version v1.0.0

# WiFi OTA (wireless) - requires device on same network
domes-cli --wifi 192.168.1.100:5000 ota flash firmware/domes/build/domes.bin
```

### OTA Protocol Flow

```
Host → Device: OTA_BEGIN (size, sha256, version)
Device → Host: OTA_ACK (status=OK, nextOffset=0)

Host → Device: OTA_DATA (offset=0, data[0..1015])
Device → Host: OTA_ACK (status=OK, nextOffset=1016)
... repeat for all 1016-byte chunks ...

Host → Device: OTA_END
Device → Host: OTA_ACK (status=OK) → device reboots
```

### OTA Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| OTA_BEGIN | 0x01 | Host → Device | Start transfer with size, SHA256, version |
| OTA_DATA | 0x02 | Host → Device | Firmware chunk (max 1016 bytes) |
| OTA_END | 0x03 | Host → Device | Transfer complete, verify and reboot |
| OTA_ACK | 0x04 | Device → Host | Acknowledge with status and next offset |
| OTA_ABORT | 0x05 | Either | Abort transfer with reason |

### Verifying OTA Success

After OTA completes, the device reboots. Verify it's running:

```bash
# Wait for reboot, then check features
domes-cli --port /dev/ttyACM0 feature list
```

### Key Files

| File | Purpose |
|------|---------|
| `tools/domes-cli/src/commands/ota.rs` | CLI OTA implementation |
| `firmware/common/protocol/otaProtocol.hpp` | OTA message definitions |
| `firmware/domes/main/transport/serialOtaReceiver.hpp` | Device-side OTA handler |
| `firmware/domes/main/services/otaManager.hpp` | HTTPS/GitHub OTA (for auto-updates) |

---

## Performance Tracing

The firmware includes a lightweight tracing framework for performance profiling. Traces are captured to a ring buffer and exported via USB serial in Chrome JSON format for Perfetto visualization.

### Quick Usage

```bash
# Dump traces
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json -n tools/trace/trace_names.json

# Visualize at https://ui.perfetto.dev
```

### Adding Trace Points

```cpp
#include "trace/traceApi.hpp"

void myFunction() {
    // RAII scope trace - automatically records begin/end
    TRACE_SCOPE(TRACE_ID("MyModule.Function"), domes::trace::TraceCategory::kGame);

    // Point-in-time event
    TRACE_INSTANT(TRACE_ID("MyModule.Event"), domes::trace::TraceCategory::kGame);

    // Counter value
    TRACE_COUNTER(TRACE_ID("MyModule.Value"), someValue, domes::trace::TraceCategory::kGame);
}
```

### Key Files

| File | Purpose |
|------|---------|
| `firmware/domes/main/trace/traceApi.hpp` | User-facing macros |
| `firmware/domes/main/trace/traceRecorder.hpp` | Singleton recorder |
| `tools/trace/trace_dump.py` | Host dump/convert tool |
| `tools/trace/trace_names.json` | Span ID → name mappings |
| `research/architecture/07-debugging.md` | Full documentation |

## Platform Requirements

**READ FIRST: `.claude/PLATFORM.md`**

- Host machine for BLE testing: **Native Linux only** (Arch, Ubuntu, etc.)
- Do NOT use WSL2 for BLE - USB passthrough is broken
- Do NOT use Raspberry Pi - BCM Bluetooth has firmware bugs
- Recommended BLE adapter: Intel AX200/AX210 or Realtek RTL8761B

### WSL2 WiFi Testing Limitation

**CRITICAL**: WSL2 cannot reach devices on WiFi networks directly due to NAT isolation. When testing WiFi features (TCP config server, HTTP OTA, etc.):

1. **Problem**: WSL2 is on a virtual subnet (172.29.x.x) isolated from WiFi (192.168.x.x)
2. **Solution**: Connect Windows WiFi to the same network as the ESP32

```bash
# Create WiFi profile (one-time setup)
cat > /tmp/wifi_profile.xml << 'EOF'
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
    <name>NETWORK_NAME</name>
    <SSIDConfig><SSID><name>NETWORK_NAME</name></SSID></SSIDConfig>
    <connectionType>ESS</connectionType>
    <connectionMode>manual</connectionMode>
    <MSM><security>
        <authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption>
        <sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>PASSWORD</keyMaterial></sharedKey>
    </security></MSM>
</WLANProfile>
EOF
cp /tmp/wifi_profile.xml /mnt/c/Users/Public/wifi_profile.xml

# Add profile and connect
/mnt/c/Windows/System32/netsh.exe wlan add profile filename="C:\\Users\\Public\\wifi_profile.xml"
/mnt/c/Windows/System32/netsh.exe wlan connect name="NETWORK_NAME"

# Verify connection
/mnt/c/Windows/System32/netsh.exe wlan show interfaces

# Now test WiFi features with the CLI
domes-cli --wifi <ESP32_IP>:5000 feature list
```

## Best Practices

- Keep commits atomic and well-described
- Review all AI-generated code for security and correctness
- Use Claude for exploration, implementation, and documentation
- Maintain clear documentation for human developers
- **For debugging**: Always use the debug-esp32 skill and follow its workflow
- **For BLE testing**: Use native Linux, never WSL2

## Resources

- [Claude Code GitHub](https://github.com/anthropics/claude-code)
- [Anthropic Documentation](https://docs.anthropic.com)

---

## Learnings & Gotchas

### ALWAYS Check for Proto Files First

**Before writing ANY protocol/serialization code:**
```bash
# Check for existing proto definitions
ls firmware/common/proto/

# Search for proto-related files
find . -name "*.proto" -o -name "*.options"
```

If proto files exist, USE THEM. Generate code with nanopb/prost. DO NOT hand-roll definitions.

### Always Search Codebase First

Before asking the user for information, search the codebase:

```bash
# WiFi credentials are in secrets.hpp
grep -r "kWifiSsid\|kWifiPassword" firmware/

# Feature flags and config
grep -r "CONFIG_DOMES" firmware/domes/sdkconfig*
```

### Initialization Order Matters

In `main.cpp`, components must be initialized in the correct order:
1. **WiFi** before TCP config server and BLE (for coexistence)
2. **BLE OTA service** early (advertising starts automatically)
3. **FeatureManager** before TCP/Serial/BLE config handlers
4. **TCP config server** before Serial OTA (to see logs)

**BLE Note**: The BLE GATT characteristic handle is only valid after NimBLE syncs. The firmware uses the global `g_statusCharHandle` populated by NimBLE after sync.

### USB-CDC Console Takeover

After `initSerialOta()`, USB-CDC becomes the OTA/config transport and logs stop appearing on the serial monitor. To debug initialization issues:
- Add delays before serial OTA init
- Or temporarily disable serial OTA
- Check for errors BEFORE the USB-CDC takeover point

### Network Testing from WSL2

WSL2 is NAT'd and cannot reach WiFi devices. Always:
1. Connect Windows WiFi to target network
2. Use Windows Python: `/mnt/c/Python313/python.exe`
3. Or test from another device on the same network

---

*Last updated: 2026-01-20*
