# Developer Quickstart Guide

Welcome to DOMES! This guide will get you productive in under 30 minutes.

## Prerequisites

- **ESP-IDF v5.2+**: [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- **Python 3.8+**: For test scripts and tools
- **Rust (optional)**: For `domes-cli` tool

## 5-Minute Overview

DOMES is a wireless reaction training pod system. Each pod has:
- 16x RGBW LEDs in a ring
- Speaker + haptic motor for feedback
- Accelerometer for tap detection
- ESP-NOW for sub-2ms pod-to-pod sync

**Hardware platforms** (in development order):
1. **ESP32-S3-DevKitC-1** - Basic firmware development
2. **NFF Development Board** - Full feature testing (see `hardware/nff-devboard/`)
3. **Production PCB** - Final form factor (planned)

## Documentation Reading Order

Read these in order to understand the project:

| Order | File | Time | What You'll Learn |
|-------|------|------|-------------------|
| 1 | **This file** | 5 min | Project overview, setup |
| 2 | `firmware/GUIDELINES.md` | 15 min | Coding standards, memory rules |
| 3 | `firmware/MILESTONES.md` | 10 min | Current status, what's implemented |
| 4 | `CLAUDE.md` §2 (Protocol Buffers) | 10 min | **CRITICAL** - Protocol rules |
| 5 | `research/SYSTEM_ARCHITECTURE.md` | 20 min | Hardware design decisions |

**Skip for now** (reference later):
- `research/architecture/*.md` - Deep design docs
- `hardware/CLAUDE.md` - Component sourcing (for hardware work)
- `.claude/PLATFORM.md` - Platform-specific issues

## First Build (10 minutes)

```bash
# Clone the repo
git clone https://github.com/pcesar22/domes.git
cd domes

# Set up ESP-IDF environment
. ~/esp/esp-idf/export.sh  # Adjust path to your ESP-IDF

# Build firmware
cd firmware/domes
idf.py set-target esp32s3
idf.py build

# Flash to device (connect DevKit via USB)
idf.py flash monitor
# Press Ctrl+] to exit monitor
```

**Expected output**: You should see boot logs and "DOMES" banner.

## Run Unit Tests

Unit tests run on your host machine (no hardware needed):

```bash
cd firmware/test_app
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

**Expected**: All tests pass (currently 6 test suites).

## Critical Rules (Read Before Coding)

### 1. Protocol Buffers Are Mandatory

**ALL message definitions come from `.proto` files. NEVER hand-roll enums or structs.**

```
Source of truth: firmware/common/proto/config.proto
                           ↓
         ┌─────────────────┴─────────────────┐
         ↓                                   ↓
    nanopb (firmware)                   prost (CLI)
    config.pb.h                         config.rs
```

If you need a new message type:
1. Add it to `firmware/common/proto/config.proto`
2. Rebuild firmware (nanopb auto-generates)
3. Rebuild CLI (prost auto-generates via `build.rs`)

**WRONG**: `enum Feature { kLedEffects = 1 };` in C++ code
**RIGHT**: Use generated `Feature` enum from `config.pb.h`

### 2. Three-Level Verification

Before marking any feature complete:

| Level | Command | Required For |
|-------|---------|--------------|
| 1. Unit Tests | `cd firmware/test_app/build && ctest` | All changes |
| 2. Build | `cd firmware/domes && idf.py build` | All changes |
| 3. Hardware | Flash and test on device | New features |

### 3. Memory Rules

- **No heap allocation after init** - Use static allocation
- **No `std::vector`** - Use ETL containers with fixed capacity
- **No exceptions** - Disabled by default
- **No `<iostream>`** - Adds 200KB, use `ESP_LOG*` macros

### 4. Pin Mappings

Single source of truth: `docs/PIN_REFERENCE.md`

## Common Tasks

### Add a New Feature Toggle

1. Add to `firmware/common/proto/config.proto`:
   ```protobuf
   enum Feature {
       FEATURE_MY_NEW_THING = 8;
   }
   ```
2. Rebuild firmware and CLI
3. Handle in `FeatureManager`

### Test Over Serial

```bash
# Basic protocol test
python3 tools/test_config.py /dev/ttyACM0

# List features via CLI
cd tools/domes-cli && cargo run -- --port /dev/ttyACM0 feature list
```

### Test Over WiFi

**WSL2 Users**: WiFi testing requires Windows Python due to NAT isolation.
See `tools/test_config_wifi.py` header for setup instructions.

```bash
# From native Linux or Windows
python3 tools/test_config_wifi.py 192.168.1.100:5000
```

### Debug with GDB

```bash
# Terminal 1: Start OpenOCD
openocd -f board/esp32s3-builtin.cfg

# Terminal 2: Start GDB
xtensa-esp32s3-elf-gdb -x firmware/domes/gdbinit firmware/domes/build/domes.elf
```

## Project Structure

```
domes/
├── firmware/
│   ├── domes/              # Main firmware application
│   │   ├── main/           # Source code
│   │   │   ├── drivers/    # Hardware drivers
│   │   │   ├── services/   # Business logic
│   │   │   ├── transport/  # Communication (USB, TCP, BLE)
│   │   │   └── config/     # Runtime configuration
│   │   └── components/     # ESP-IDF components
│   ├── common/             # Shared code (protocol, interfaces)
│   │   └── proto/          # Protocol Buffer definitions
│   └── test_app/           # Unit tests (Google Test)
├── hardware/
│   └── nff-devboard/       # Development board files
├── tools/
│   ├── domes-cli/          # Rust CLI tool
│   └── test_*.py           # Python test scripts
└── research/               # Architecture documentation
```

## Working with Multiple Pods

DOMES is a multi-pod system. For full testing, you need two ESP32 devices connected via USB.

### Identifying Connected Devices

```bash
# List serial ports
domes-cli --list-ports

# Scan for all DOMES devices (serial + BLE)
domes-cli devices scan
```

Devices typically appear as `/dev/ttyACM0` and `/dev/ttyACM1`.

### Registering Devices

```bash
# Register each device with a friendly name
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1

# Verify registration
domes-cli devices list
```

### Flashing and Testing Both Pods

```bash
# Flash firmware to both devices
domes-cli --all ota flash firmware/domes/build/domes.bin --version v1.0.0

# Verify both are running
domes-cli --all feature list

# Test LED on both
domes-cli --all led solid --color 00ff00
```

### ESP-NOW (Pod-to-Pod Communication)

```bash
# Enable ESP-NOW on both pods
domes-cli --all feature enable esp-now

# Monitor both for discovery messages
python .claude/skills/esp32-firmware/scripts/monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 30
```

For full multi-device documentation, see `CLAUDE.md` section "Multi-Device Testing".

## Getting Help

- **Stuck?** Check `research/architecture/` for design decisions
- **Build issues?** Ensure ESP-IDF v5.2+ and submodules are initialized
- **Hardware issues?** See `.claude/PLATFORM.md` for known limitations
- **Protocol issues?** Read `CLAUDE.md` Section 2 (Protocol Buffers)

## Before Your First PR

Checklist:
- [ ] Code follows `firmware/GUIDELINES.md`
- [ ] Unit tests pass (`ctest --output-on-failure`)
- [ ] Firmware builds (`idf.py build`)
- [ ] No new compiler warnings
- [ ] Tested on hardware (for new features)
- [ ] Updated relevant documentation

Welcome aboard!
