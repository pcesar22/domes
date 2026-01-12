# DOMES Host Tools

Host-side tools for interacting with DOMES firmware.

## Test Scripts

### test_config.py - Serial Config Protocol Test

Tests the runtime configuration protocol over USB serial.

```bash
python3 test_config.py /dev/ttyACM0
```

**What it tests:**
- LIST_FEATURES command (0x20) - List all features and their enabled state
- SET_FEATURE command (0x22) - Enable/disable features
- Frame encoding/decoding with CRC32 verification

### test_config_wifi.py - WiFi/TCP Config Protocol Test

Tests the runtime configuration protocol over WiFi TCP (port 5000).

```bash
python3 test_config_wifi.py 192.168.50.173
python3 test_config_wifi.py 192.168.50.173:5000  # Explicit port
```

**Prerequisites:**
- ESP32 must be connected to WiFi (see `CONFIG_DOMES_WIFI_AUTO_CONNECT`)
- Test machine must be on the same network as ESP32
- TCP config server must be running on firmware (port 5000)

**WSL2 Users:** WSL2 cannot reach WiFi devices directly. Use Windows Python:
```bash
/mnt/c/Python313/python.exe test_config_wifi.py <ESP32_IP>
```

See `CLAUDE.md` in repo root for WiFi setup instructions.

## CLI Tool

### domes-cli - Rust Command Line Interface

Full-featured CLI for firmware interaction.

```bash
cd domes-cli
cargo build --release

# List features
cargo run -- --port /dev/ttyACM0 feature list

# Enable/disable features
cargo run -- --port /dev/ttyACM0 feature enable led-effects
cargo run -- --port /dev/ttyACM0 feature disable ble

# WiFi transport (future)
cargo run -- --wifi 192.168.50.173:5000 feature list
```

## Trace Tools

### trace/trace_dump.py - Performance Trace Exporter

Dumps trace events from firmware and exports to Chrome JSON format.

```bash
python3 trace/trace_dump.py -p /dev/ttyACM0 -o trace.json
# Open trace.json in https://ui.perfetto.dev
```

## Protocol Reference

### Frame Format

```
[Start0][Start1][LenLE16][Type][Payload...][CRC32LE]
  0xAA    0x55    2 bytes  1 byte  N bytes   4 bytes

Length = 1 (type) + payload length
CRC32 = IEEE 802.3 CRC over [Type][Payload]
```

### Config Message Types (0x20-0x2F)

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x20 | LIST_FEATURES_REQ | Host→Device | (none) |
| 0x21 | LIST_FEATURES_RSP | Device→Host | status, count, [id, enabled]... |
| 0x22 | SET_FEATURE_REQ | Host→Device | feature_id, enabled |
| 0x23 | SET_FEATURE_RSP | Device→Host | status, feature_id, enabled |

### Feature IDs

| ID | Name | Description |
|----|------|-------------|
| 1 | led-effects | LED animations |
| 2 | ble | Bluetooth advertising |
| 3 | wifi | WiFi connectivity |
| 4 | esp-now | ESP-NOW protocol |
| 5 | touch | Touch sensing |
| 6 | haptic | Haptic feedback |
| 7 | audio | Audio output |

## Troubleshooting

### Serial Port Permission (Linux)
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### WiFi Test Fails with Timeout
1. Check ESP32 has IP: monitor serial logs for "Got IP: x.x.x.x"
2. Verify same network: ping the ESP32 IP from test machine
3. Check TCP server running: look for "TCP config server listening on port 5000" in logs
4. WSL2 users: use Windows Python (see above)

### CRC Mismatch Errors
- Verify frame format matches protocol spec
- Check byte order (little-endian for length and CRC)
- Ensure CRC is calculated over type + payload only (not start bytes or length)
