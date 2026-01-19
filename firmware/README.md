# DOMES Firmware

ESP-IDF v5.x firmware for the DOMES reaction training pod.

## Quick Start

```bash
cd firmware/domes
. ~/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build flash monitor
```

## Critical: Protocol Buffers

> **ALL protocol definitions MUST come from `.proto` files.**
> **NEVER hand-roll enums, structs, or message types in code.**

```
┌─────────────────────────────────────────────────────────────────┐
│  Source of truth: firmware/common/proto/config.proto           │
│                                                                 │
│  WRONG: enum Feature { kLed = 1; };  // in C++ code            │
│  RIGHT: #include "config.pb.h"       // use generated types    │
└─────────────────────────────────────────────────────────────────┘
```

### How It Works

```
config.proto (SOURCE OF TRUTH)
       │
       ├──► nanopb generator ──► config.pb.h / config.pb.c (firmware)
       │
       └──► prost build.rs ──► config.rs (CLI, generated at build time)
```

### Adding a New Message/Enum

1. Edit `firmware/common/proto/config.proto`
2. Rebuild firmware (`idf.py build` - nanopb runs automatically)
3. Rebuild CLI (`cargo build` - prost runs automatically)
4. Use the generated types in your code

See `CLAUDE.md` Section 2 for complete documentation.

## Directory Structure

```
firmware/
├── domes/                  # Main firmware application
│   ├── main/
│   │   ├── config/         # Runtime configuration, feature manager
│   │   ├── drivers/        # Hardware drivers (LED, audio, haptic)
│   │   ├── infra/          # FreeRTOS, logging, watchdog
│   │   ├── interfaces/     # Abstract driver interfaces
│   │   ├── services/       # Business logic (WiFi, OTA, GitHub)
│   │   ├── trace/          # Performance tracing
│   │   ├── transport/      # Communication (USB-CDC, TCP, BLE)
│   │   └── utils/          # Helpers (mutex, LED animator)
│   └── components/         # ESP-IDF components (certs, nanopb)
├── common/                 # Shared across projects
│   ├── proto/              # Protocol Buffer definitions
│   ├── protocol/           # Frame codec, OTA protocol
│   ├── interfaces/         # Transport interface
│   └── utils/              # CRC32, etc.
├── test_app/               # Unit tests (Google Test + CMake)
└── third_party/            # External libraries (nanopb)
```

## Building

### Firmware

```bash
cd firmware/domes
idf.py build           # Build only
idf.py flash           # Flash to device
idf.py monitor         # View serial output
idf.py flash monitor   # Flash and monitor
```

### Unit Tests

```bash
cd firmware/test_app
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure
```

## Key Files

| File | Purpose |
|------|---------|
| `common/proto/config.proto` | Protocol definitions (SOURCE OF TRUTH) |
| `domes/main/main.cpp` | Application entry point |
| `domes/main/config.hpp` | Pin definitions, constants |
| `domes/sdkconfig.defaults` | ESP-IDF configuration |
| `domes/partitions.csv` | Flash partition layout |

## Documentation

- `GUIDELINES.md` - Coding standards and memory rules
- `MILESTONES.md` - Development phases and status
- `CLAUDE.md` - AI assistant context and verification requirements
- `../docs/PIN_REFERENCE.md` - GPIO pin mappings for all platforms

## Hardware Platforms

See `../docs/PIN_REFERENCE.md` for complete pin mappings.

| Platform | LED GPIO | I2C | I2S | Notes |
|----------|----------|-----|-----|-------|
| ESP32-S3-DevKitC-1 v1.1 | 38 | - | - | Single WS2812 |
| NFF Development Board | 48 | 8/9 | 10/11/12 | Full peripherals |
| Production PCB | 14 | 8/9 | 11/12/13 | Planned |
