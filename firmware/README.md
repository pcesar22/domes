# DOMES Firmware

ESP-IDF based firmware for the DOMES reaction training pod system.

## Requirements

- ESP-IDF v5.2.x or later
- Python 3.8+
- CMake 3.16+
- ESP32-S3-DevKitC-1 (for development) or DOMES custom PCB

## Quick Setup

### 1. Install ESP-IDF

```bash
# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.2.2 --recursive https://github.com/espressif/esp-idf.git esp-idf

# Install tools (run once)
cd esp-idf
./install.sh esp32s3

# Activate environment (run in each new terminal)
. ~/esp/esp-idf/export.sh
```

Or use the setup script:
```bash
./setup.sh
```

### 2. Build Firmware

```bash
cd firmware

# Set target (first time only)
idf.py set-target esp32s3

# Build
idf.py build

# Flash (connect DevKit via USB)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

### 3. Configure Options

```bash
idf.py menuconfig
```

Navigate to `DOMES Configuration` to set:
- Target platform (DevKit or PCB)
- Hardware feature toggles
- Network settings

## Project Structure

```
firmware/
├── CMakeLists.txt          # Root build file
├── sdkconfig.defaults      # Default configuration
├── partitions.csv          # Flash partition table
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp            # Entry point
│   ├── Kconfig.projbuild   # Configuration menu
│   ├── config/
│   │   └── constants.hpp   # Global constants
│   ├── platform/
│   │   ├── pins.hpp        # Pin configuration router
│   │   ├── pinsDevkit.hpp  # DevKit pin mappings
│   │   └── pinsPcbV1.hpp   # PCB v1 pin mappings
│   ├── interfaces/         # Driver interfaces
│   │   ├── iLedDriver.hpp
│   │   ├── iAudioDriver.hpp
│   │   ├── iHapticDriver.hpp
│   │   ├── iTouchDriver.hpp
│   │   └── iImuDriver.hpp
│   ├── drivers/            # Hardware implementations
│   ├── services/           # Business logic
│   ├── game/               # Game engine
│   └── utils/
│       ├── expected.hpp    # Error handling
│       └── mutex.hpp       # Thread safety
├── components/             # Reusable components
└── test/
    ├── CMakeLists.txt
    ├── test_main.cpp
    └── mocks/              # Mock implementations
```

## Development Workflow

### Building for Different Platforms

```bash
# DevKit (default)
idf.py build

# PCB v1
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.pcbV1" build
```

### Running Tests

```bash
# Host-based unit tests (Linux target)
idf.py --preview set-target linux
idf.py build
./build/domes-firmware.elf
```

### Debugging

```bash
# OpenOCD + GDB (via USB-JTAG)
idf.py openocd gdb
```

## Key Technologies

| Component | Technology |
|-----------|------------|
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Framework | ESP-IDF v5.x |
| Language | C++20 |
| RTOS | FreeRTOS |
| Communication | ESP-NOW + BLE |

## References

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [DOMES System Architecture](../research/SYSTEM_ARCHITECTURE.md)
- [DOMES Software Architecture](../research/SOFTWARE_ARCHITECTURE.md)
- [Development Guidelines](./GUIDELINES.md)
