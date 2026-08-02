# Development Platform Requirements

## Host Machine For BLE And ESP-NOW Testing

Use native Linux such as Arch or Ubuntu for validation involving BLE, USB serial, or multiple pods.

Do not use:

- WSL2 with USB passthrough for BLE.
- Raspberry Pi Bluetooth for validation-critical BLE work.
- Windows for BLE validation unless the host tooling is explicitly ported.

Recommended Bluetooth adapters:

- Intel AX200 or AX210.
- Realtek RTL8761B-based USB dongles.

Avoid cheap CSR clone adapters for BLE work.

## Host Tools

Use `tools/domes-cli` for serial, WiFi, BLE, OTA, trace, and multi-device operations.

```bash
cd tools/domes-cli
cargo build
```

Common commands:

```bash
domes-cli --port /dev/ttyACM0 feature list
domes-cli --wifi 192.168.1.100:5000 feature list
domes-cli --scan-ble
domes-cli --ble "DOMES-Pod-01" feature list
```

OTA:

```bash
domes-cli --port /dev/ttyACM0 ota flash firmware/domes/build/domes.bin --version v1.0.0
domes-cli --ble "DOMES-Pod-01" ota flash firmware/domes/build/domes.bin --version v1.0.0
```

## Multi-Device Setup

Two connected ESP32-S3 pods usually appear as `/dev/ttyACM0` and `/dev/ttyACM1`. Kernel port order
can change on reboot.

Install udev rules for stable symlinks:

```bash
sudo cp tools/udev/99-domes-pods.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Devices will also appear as `/dev/domes-pod-usb0`, `/dev/domes-pod-usb1`, and similar symlinks.

Register pods with the CLI:

```bash
domes-cli devices add pod1 serial /dev/ttyACM0
domes-cli devices add pod2 serial /dev/ttyACM1
domes-cli --target pod1 --target pod2 feature list
domes-cli --all led off
```

Set pod identity:

```bash
domes-cli --port /dev/ttyACM0 system set-pod-id 1
domes-cli --port /dev/ttyACM1 system set-pod-id 2
```

After reboot, pods advertise as `DOMES-Pod-01`, `DOMES-Pod-02`, and include `pod_id` in protocol
responses.

Discovery:

```bash
domes-cli devices scan
```

WiFi pods advertise with mDNS service `_domes._tcp.local.`. BLE pods advertise as `DOMES-Pod-XX`.

## CI Multi-Device Testing

The self-hosted hardware runner needs one or more ESP32-S3 devices connected. For two-device tests,
set `SERIAL_PORTS=/dev/ttyACM0,/dev/ttyACM1` or use stable udev symlinks.

The `hw-test` PR label triggers hardware CI. Ask before adding it to a PR.
