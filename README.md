# DOMES

**Distributed Open-source Motion & Exercise System**

A reaction training pod system designed for athletic training, physical therapy, and fitness applications. Each pod features RGB LEDs, audio feedback, haptic response, and capacitive touch sensing, all communicating wirelessly for synchronized multi-pod drills.

## Features

- **Multi-Pod Synchronization** - Up to 24 pods communicating via ESP-NOW with <2ms latency
- **RGB LED Ring** - 16 SK6812 RGBW LEDs per pod for visual feedback
- **Audio Feedback** - I2S audio via MAX98357A amplifier and 23mm speaker
- **Haptic Response** - LRA motor driven by DRV2605L for tactile feedback
- **Touch Detection** - Capacitive touch sensing with LIS2DW12 accelerometer tap detection
- **Wireless Communication** - Hybrid ESP-NOW + BLE architecture for phone connectivity
- **OTA Updates** - Over-the-air firmware updates via BLE
- **Long Battery Life** - 1200mAh LiPo with 14+ hours active training time

## Hardware

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM) |
| LEDs | 16x SK6812 RGBW Mini (3535 package) |
| Audio | MAX98357A I2S amplifier + 23mm speaker |
| Haptic | DRV2605L driver + LRA motor |
| Touch | ESP32 capacitive touch + LIS2DW12 IMU |
| Power | 1200mAh LiPo, TP4056 charger, magnetic pogo charging |

See [research/SYSTEM_ARCHITECTURE.md](research/SYSTEM_ARCHITECTURE.md) for full hardware design.

## Software Stack

| Layer | Technology |
|-------|------------|
| Framework | ESP-IDF v5.x |
| RTOS | FreeRTOS (dual-core SMP) |
| Language | C++20 (application), C (drivers) |
| Build | CMake via `idf.py` |
| Communication | ESP-NOW + BLE (NimBLE) |

See [research/SOFTWARE_ARCHITECTURE.md](research/SOFTWARE_ARCHITECTURE.md) for firmware architecture.

## Project Structure

```
domes/
├── firmware/           # ESP-IDF firmware project
│   ├── CLAUDE.md       # Firmware development guidelines
│   └── GUIDELINES.md   # Coding standards
├── hardware/           # PCB design and BOM
│   ├── CLAUDE.md       # Hardware development guidelines
│   └── BOM.csv         # Bill of materials
├── research/           # Architecture documentation
│   ├── SYSTEM_ARCHITECTURE.md   # Hardware design spec
│   ├── SOFTWARE_ARCHITECTURE.md # Firmware design spec
│   ├── DEVELOPMENT_ROADMAP.md   # Milestone planning
│   └── architecture/            # Modular firmware docs
└── CLAUDE.md           # Project-level guidelines
```

## Getting Started

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- ESP32-S3 development board (for prototyping)
- USB-C cable

### Installation

```bash
# Clone the repository
git clone https://github.com/pcesar22/domes.git
cd domes

# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Build firmware
cd firmware
idf.py set-target esp32s3
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

### Development

See [firmware/GUIDELINES.md](firmware/GUIDELINES.md) for coding standards and [research/DEVELOPMENT_ROADMAP.md](research/DEVELOPMENT_ROADMAP.md) for the development plan.

## Documentation

| Document | Description |
|----------|-------------|
| [SYSTEM_ARCHITECTURE.md](research/SYSTEM_ARCHITECTURE.md) | Hardware design, schematics, power budget |
| [SOFTWARE_ARCHITECTURE.md](research/SOFTWARE_ARCHITECTURE.md) | Firmware design, task architecture, protocols |
| [DEVELOPMENT_ROADMAP.md](research/DEVELOPMENT_ROADMAP.md) | Milestones, dependencies, CI pipeline |
| [firmware/GUIDELINES.md](firmware/GUIDELINES.md) | C++20 coding standards, memory policy |
| [hardware/BOM.csv](hardware/BOM.csv) | Component list with LCSC part numbers |

## Contributing

Contributions are welcome! Please review the coding guidelines before submitting a Pull Request.

## License

[Add license information]

## Contact

- Repository: [pcesar22/domes](https://github.com/pcesar22/domes)

---

*This project is developed with assistance from [Claude Code](https://github.com/anthropics/claude-code)*
