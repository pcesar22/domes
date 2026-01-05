# 01 - Project Structure

## AI Agent Instructions

Load this file when creating new source files or adding modules.

Prerequisites: `00-getting-started.md` completed

---

## Directory Layout

```
firmware/
├── CMakeLists.txt              # Root CMake (project definition)
├── sdkconfig.defaults          # Default menuconfig options
├── sdkconfig.defaults.devkit   # DevKit-specific overrides
├── sdkconfig.defaults.pcb      # PCB-specific overrides
├── partitions.csv              # Flash partition table
│
├── main/
│   ├── CMakeLists.txt          # Main component CMake
│   ├── main.cpp                # Entry point (app_main)
│   ├── config.hpp              # Pin definitions, constants
│   │
│   ├── interfaces/             # Abstract base classes
│   │   ├── i_led_driver.hpp
│   │   ├── i_audio_driver.hpp
│   │   └── ...
│   │
│   ├── drivers/                # Hardware implementations
│   │   ├── led_driver.hpp/.cpp
│   │   ├── audio_driver.hpp/.cpp
│   │   └── ...
│   │
│   ├── services/               # Business logic
│   │   ├── feedback_service.hpp/.cpp
│   │   ├── comm_service.hpp/.cpp
│   │   └── ...
│   │
│   ├── game/                   # Game logic
│   │   ├── game_engine.hpp/.cpp
│   │   ├── state_machine.hpp/.cpp
│   │   └── protocol.hpp/.cpp
│   │
│   ├── platform/               # Platform-specific
│   │   ├── pins_devkit.hpp
│   │   └── pins_pcb_v1.hpp
│   │
│   └── utils/                  # Utilities
│       ├── error_codes.hpp
│       └── ring_buffer.hpp
│
├── components/                 # Reusable components
│   ├── etl/                    # Embedded Template Library
│   └── esp_now_wrapper/
│
└── test/
    ├── mocks/
    └── test_*.cpp
```

---

## Naming Conventions

### Files

| Type | Convention | Example |
|------|------------|---------|
| Source | snake_case | `led_driver.cpp` |
| Header | snake_case | `led_driver.hpp` |
| Interface | `i_` prefix | `i_led_driver.hpp` |
| Mock | `mock_` prefix | `mock_led_driver.hpp` |
| Test | `test_` prefix | `test_led_driver.cpp` |

### Code Elements

| Element | Convention | Example |
|---------|------------|---------|
| Class | PascalCase | `LedDriver` |
| Interface | `I` + PascalCase | `ILedDriver` |
| Method | camelCase | `playEffect()` |
| Variable | camelCase | `reactionTime` |
| Member | camelCase + `_` | `ledDriver_` |
| Constant | `k` + PascalCase | `kMaxBrightness` |
| Namespace | lowercase | `domes` |
| Macro | SCREAMING_SNAKE | `CONFIG_LED_COUNT` |

---

## Interface Pattern

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         INTERFACE → IMPLEMENTATION                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   interfaces/i_example.hpp        drivers/example_driver.hpp                 │
│   ────────────────────────        ──────────────────────────                 │
│   ┌─────────────────────┐         ┌─────────────────────────┐               │
│   │   IExampleDriver    │         │    ExampleDriver        │               │
│   │   (abstract)        │◄────────│    : public IExample    │               │
│   ├─────────────────────┤         ├─────────────────────────┤               │
│   │ + init()            │         │ + init() override       │               │
│   │ + doAction()        │         │ + doAction() override   │               │
│   │ + isInit() const    │         │ - pin_                  │               │
│   └─────────────────────┘         │ - initialized_          │               │
│                                   └─────────────────────────┘               │
│                                                                              │
│   test/mocks/mock_example.hpp                                                │
│   ────────────────────────────                                               │
│   ┌─────────────────────────┐                                               │
│   │    MockExampleDriver    │     For unit testing                          │
│   │    : public IExample    │     Records calls, configurable returns       │
│   └─────────────────────────┘                                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Minimal Interface Template

```cpp
// EXAMPLE: Interface pattern - adapt for each driver

class IExampleDriver {
public:
    virtual ~IExampleDriver() = default;
    virtual esp_err_t init() = 0;
    virtual esp_err_t doAction(ParamType param) = 0;
    virtual bool isInitialized() const = 0;
};
```

---

## Include Order

1. Corresponding header (for .cpp)
2. ESP-IDF headers (`esp_log.h`, `driver/gpio.h`)
3. Standard library (`<cstdint>`, `<cstring>`)
4. ETL headers (`etl/vector.h`)
5. Project headers (interfaces first)

---

## Component Boundaries

**Create separate component when:**
- Reusable across projects
- Has own dependencies (Kconfig)
- Third-party library

**Keep in main/ when:**
- Project-specific
- Single-use utility
- Platform-specific

---

## Namespace Usage

| Scope | Namespace | Example |
|-------|-----------|---------|
| All project code | `domes` | `domes::LedDriver` |
| Pin definitions | `domes::pins` | `pins::kLedData` |
| Timing constants | `domes::timing` | `timing::kDebounceMs` |

---

*Related: `00-getting-started.md`, `02-build-system.md`*
