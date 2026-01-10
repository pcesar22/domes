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
firmware/host/build/simple_ota_sender /dev/ttyACM0 firmware/domes/build/domes.bin v1.0.0
```

**BLE OTA:**
```bash
firmware/host/build/ble_ota_sender firmware/domes/build/domes.bin DOMES-Pod v1.0.0
```

---

*This document exists because WSL2+USB Bluetooth wastes hours of debugging time. Don't repeat this mistake.*
