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
cd domes/firmware
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## License

[Add license information]
