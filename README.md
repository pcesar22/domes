# DOMES

**Distributed Open-source Motion & Exercise System**

A wireless reaction training pod system for athletic training, physical therapy, and fitness. Each pod features RGB LEDs, audio, haptic feedback, and touch sensing, communicating via ESP-NOW for sub-2ms latency synchronized drills.

## Overview

- **Hardware**: ESP32-S3 based pods with RGBW LED ring, speaker, haptic motor, and capacitive touch
- **Firmware**: ESP-IDF v5.x with FreeRTOS, written in C++20
- **Communication**: Hybrid ESP-NOW + BLE for pod-to-pod and phone connectivity

## Hardware

| Platform | Description | Use Case |
|----------|-------------|----------|
| [ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html) | Off-the-shelf dev board | Initial firmware development |
| [NFF Development Board](hardware/nff-devboard/) | Custom sensor board with LED ring, IMU, haptics, audio | Full feature development and testing |
| Production PCB | Final form-factor (planned) | Enclosed pod units |

The **NFF devboard** is the primary development platform for testing all DOMES features. See [`hardware/nff-devboard/README.md`](hardware/nff-devboard/README.md) for specifications, pin mappings, and ordering instructions.

## Getting Started

**New to the project?** Start with [`DEVELOPER_QUICKSTART.md`](DEVELOPER_QUICKSTART.md) for a guided onboarding.

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/).

```bash
git clone https://github.com/pcesar22/domes.git
cd domes/firmware/domes
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Performance Profiling

The firmware includes a tracing framework for post-mortem performance analysis:

```bash
# Dump traces from device
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json

# Visualize at https://ui.perfetto.dev
```

See `research/architecture/07-debugging.md` for full documentation.

## License

[Add license information]
