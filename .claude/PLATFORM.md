# Development Platform Requirements

## Host Machine for BLE/ESP-NOW Testing

**USE: Native Linux (Arch, Ubuntu, etc.)**

Do NOT use:
- WSL2 with USB passthrough - BLE scanning is broken
- Raspberry Pi - BCM Bluetooth firmware has known bugs
- Windows - Would require rewriting all host tools for WinRT

### Hardware Requirements

**Bluetooth Adapter (for BLE OTA testing):**
- Intel AX200/AX210 (recommended)
- Realtek RTL8761B-based dongles (good)
- Do NOT use cheap CSR clones - unreliable LE support

### Why Native Linux

1. BlueZ-based host tools (`ble_ota_sender`) already work
2. Claude Code runs natively with full support
3. USB Bluetooth dongles work reliably
4. No code changes required

### Host Tools Location

- Serial OTA sender: `firmware/host/build/simple_ota_sender`
- BLE OTA sender: `firmware/host/build/ble_ota_sender`

### Testing Commands

**Serial OTA (USB-CDC):**
```bash
domes-cli --port /dev/ttyACM0 ota flash firmware/domes/build/domes.bin --version v1.0.0
```

**BLE OTA:**
```bash
domes-cli --ble "DOMES-Pod-01" ota flash firmware/domes/build/domes.bin --version v1.0.0
```

---

## Multi-Device Setup

### Connecting Two Pods

Two ESP32-S3 pods connected via USB appear as `/dev/ttyACM0` and `/dev/ttyACM1`. Port numbers are assigned by the kernel in plug order and can change on reboot.

### Stable Serial Port Names (udev)

Install the udev rules for deterministic device symlinks:

```bash
sudo cp tools/udev/99-domes-pods.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Devices will then also appear as `/dev/domes-pod-usb0`, `/dev/domes-pod-usb1`, etc.

### Device Registry

Register pods once, reference by name:

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli --target pod1 --target pod2 feature list
domes-cli --all led off
```

### Setting Pod Identity

Each pod can be assigned a unique ID (1-255), persisted to NVS:

```bash
domes-cli --port /dev/ttyACM0 system set-pod-id 1
domes-cli --port /dev/ttyACM1 system set-pod-id 2
```

After reboot, pods advertise unique BLE names (`DOMES-Pod-01`, `DOMES-Pod-02`) and include `pod_id` in protocol responses.

### Device Discovery

```bash
# Scan all transports (serial probing + BLE scan)
domes-cli devices scan

# WiFi pods advertise via mDNS: domes-pod-1.local, _domes._tcp
# BLE pods advertise as DOMES-Pod-XX
```

### CI Multi-Device Testing

The self-hosted runner needs two ESP32 devices connected. Set `SERIAL_PORTS=/dev/ttyACM0,/dev/ttyACM1` in the workflow.

---

*This document exists because WSL2+USB Bluetooth wastes hours of debugging time. Don't repeat this mistake.*
