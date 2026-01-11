# DOMES

**Distributed Open-source Motion & Exercise System**

A wireless reaction training pod system for athletic training, physical therapy, and fitness. Each pod features RGB LEDs, audio, haptic feedback, and touch sensing, communicating via ESP-NOW for sub-2ms latency synchronized drills.

## Overview

- **Hardware**: ESP32-S3 based pods with RGBW LED ring, speaker, haptic motor, and capacitive touch
- **Firmware**: ESP-IDF v5.x with FreeRTOS, written in C++20
- **Communication**: Hybrid ESP-NOW + BLE for pod-to-pod and phone connectivity

## Getting Started

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
