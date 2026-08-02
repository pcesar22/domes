# Development Platform Requirements

## Host Machine

Use native Linux for validation involving BLE, serial devices, or multiple pods. Do not use WSL2
for BLE validation. Raspberry Pi Bluetooth and low-quality CSR clone adapters are unsuitable for
validation-critical BLE results. Intel AX200/AX210 and Realtek RTL8761B adapters are recommended.

Install the CLI prerequisites and ensure the current user either has an active `uaccess` ACL or
belongs to the group that owns the serial devices (`dialout` on Debian/Ubuntu, commonly `uucp` on
Arch Linux). Check with `getfacl <PORT>` and `stat -c '%G' <PORT>` rather than assuming one group.

Reproducible repository verification uses ESP-IDF v5.4.4 and Rust 1.92.0. The Flutter workflow pins
Flutter 3.44.8. A newer toolchain may be useful for investigation, but it does not replace a pass on
the pinned versions.

## NFF USB Interfaces

An NFF carrier uses an ESP32-S3 DevKit with two distinct host interfaces:

| Host interface | Typical node | Purpose |
| --- | --- | --- |
| DevKit CP2102N UART bridge | `/dev/ttyUSB*` | `idf.py` flash, `domes-cli --port`, serial OTA |
| Native ESP32-S3 USB Serial/JTAG | `/dev/ttyACM*` | ESP-IDF console logs and built-in JTAG |

The firmware keeps console output off UART0 so text cannot corrupt framed config or OTA traffic.
One CP2102N cable is enough to flash and use `domes-cli`; connect native USB separately when boot
logs or built-in JTAG are required.

Kernel `ttyUSB` numbers can change. Locate the serial-number-based links and use them for persistent
CLI registry entries:

```bash
find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort

PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort | sed -n '2p')"

# Optional native USB console/JTAG interfaces, when separately connected.
CONSOLE1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' -print | sort | sed -n '1p')"
CONSOLE2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' -print | sort | sed -n '2p')"
```

`PORT1` and `PORT2` above mean first and second sorted USB serial number; they do not imply the
firmware pod ID. Query `system info`, then record or assign identity deliberately before registering
names. The same warning applies to sorted `CONSOLE1` and `CONSOLE2` values. If a distribution does
not create native USB `by-id` links, use the matching `/dev/ttyACM*` node only for that session.

The rules in `tools/udev/99-domes-pods.rules` request mode `0660` and an active-session `uaccess`
ACL for CP2102N and native ESP32-S3 USB serial devices. They do not select the owning group or create
identity aliases; inspect the resulting ACL/group and use `/dev/serial/by-id/` for identity.

```bash
sudo cp tools/udev/99-domes-pods.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Some distributions apply generic GPS receiver rules to CP210x bridges. If a port is unexpectedly
busy, inspect it before flashing:

```bash
fuser -v "$PORT1"
udevadm info --query=property --name="$PORT1" | rg 'GPS|ID_VENDOR|ID_MODEL'
```

Stop or adjust the conflicting `gpsd` hotplug rule only when it actually owns the DOMES port; do not
disable unrelated host services as a default setup step.

## Host Tools

Run from the repository root:

```bash
(cd tools/domes-cli && cargo build --locked)
CLI=tools/domes-cli/target/debug/domes-cli

$CLI --port "$PORT1" feature list
$CLI --wifi 192.168.1.100:5000 feature list
$CLI --scan-ble
$CLI --ble "DOMES-Pod-01" feature list
```

WiFi/TCP exists only in a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build. Such builds prefer credentials
stored in NVS and use compile-time secrets only to seed an unprovisioned first boot; the default
build omits and rejects the WiFi feature. Raw TCP image transfer and trace control are not routed by
the TCP config server. Serial and BLE are the supported CLI image-transfer paths.

```bash
# FIRMWARE_BIN and EXPECTED_VERSION must describe the same retained clean build.
test -f "$FIRMWARE_BIN"
$CLI --port "$PORT1" ota flash "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
$CLI --ble "DOMES-Pod-01" ota flash "$FIRMWARE_BIN" --version "$EXPECTED_VERSION"
```

Create that retained image with the isolated firmware command in `docs/TESTING.md`; do not reuse an
unverified project-local `build/` directory.

After either transfer, reconnect, verify the expected version plus `system health` and `system
self-test`, reboot once more, and repeat the checks. The second boot distinguishes an accepted image
from one that merely booted while rollback verification was still pending. This success sequence
does not verify the forced failed-self-test rollback path; that requires a deliberately failing test
image or fault injection.

## Multi-Device Setup

For unassigned boards, register stable CP2102N paths and then set persistent pod IDs deliberately:

```bash
$CLI devices add pod1 serial "$PORT1"
$CLI devices add pod2 serial "$PORT2"
$CLI --target pod1 system set-pod-id 1
$CLI --target pod2 system set-pod-id 2
$CLI --target pod1 --target pod2 feature list
```

The registry rejects a second name for the same normalized serial, BLE, or TCP address. Use a stable
serial path or BLE MAC for destructive fan-out: a transient BLE advertising name is not assumed to
be the same identifier as that device's MAC. Update the existing entry deliberately instead of
creating aliases that can make `--all` contact one physical pod twice.

After reboot, the devices advertise as `DOMES-Pod-01` and `DOMES-Pod-02`. BlueZ can cache old
advertising names; reconnect by address or remove the cached device before treating an old name as a
firmware failure.

`domes-cli devices scan` discovers local serial ports and BLE advertisements. It does not perform
WiFi/mDNS discovery. Registry-backed `--all` operates only on saved entries.

## Hardware CI

The self-hosted hardware workflow requires an online Linux x64 runner with Actions Runner 2.327.1
or newer, ESP-IDF v5.4.4, Rust 1.92.0, native BLE support, and at least two attached NFF pods. The
runner must expose the CP2102N ports to the service account and retain stable device identity under
`/dev/serial/by-id/`. The repository workflow does not install, register, or power that host.

Manual dispatch accepts a comma-separated `ports` input; pass CP2102N `/dev/serial/by-id/` paths.
The `hw-test` pull request label uses runner auto-detection and consumes attached lab hardware; ask
before adding it. Verify a qualifying runner is online first. A queued job with no runner is not a
test result and should be cancelled rather than left indefinitely pending.
