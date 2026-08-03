# DOMES ESP-IDF Configuration Reference

This reference describes the checked-in development configuration. The live authorities are
`firmware/domes/sdkconfig.defaults`, `firmware/domes/partitions.csv`,
`firmware/domes/main/Kconfig.projbuild`, and `firmware/domes/main/config.hpp`. Do not paste generic
ESP-IDF example layouts or options here.

## Active Development Profile

| Setting | Checked-in value |
| --- | --- |
| ESP-IDF | v5.4.4, matching CI and the component dependency lock |
| Target | ESP32-S3 |
| Active board | Sole NFF carrier profile compiled directly in `main/config.hpp` |
| Flash | 8 MB, QIO, 80 MHz |
| Console | Native USB Serial/JTAG |
| Runtime serial | UART0 through the DevKit CP2102N bridge, 115200 8N1 |
| Bluetooth | NimBLE peripheral/broadcaster, one connection, preferred MTU 517 |
| App slots | Two `0x1E0000` OTA slots |

There is no board selector or alternate profile in `config.hpp`. In particular, the production
16 MB layout is not represented by the active 8 MB partition table. Adding a target requires a
complete pin/config/partition profile, an explicit selection mechanism, and hardware verification.

## USB And UART

`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` sends ESP-IDF logs to native USB (`/dev/ttyACM*`). Framed
config and serial OTA use UART0 through CP2102N (`/dev/ttyUSB*`, preferably its
`/dev/serial/by-id/` link). Do not enable the UART console while the framed runtime transport owns
UART0.

## Partition Table

The current 8 MB table is:

| Partition | Offset | Size | Purpose |
| --- | --- | --- | --- |
| `nvs` | `0x9000` | `0x6000` | Persistent configuration and statistics |
| `otadata` | `0xf000` | `0x2000` | OTA boot selection |
| `phy_init` | `0x11000` | `0x1000` | PHY calibration data |
| `ota_0` | `0x20000` | `0x1E0000` | First application slot |
| `ota_1` | `0x200000` | `0x1E0000` | Second application slot |
| `spiffs` | `0x3E0000` | `0x400000` | Reserved data partition; not mounted or consumed by current firmware |
| `coredump` | `0x7E0000` | `0x20000` | ESP-IDF flash panic dumps |

There is no factory application partition. A standalone `domes.bin` written at `0x20000` is only
the application image; it is not a complete factory installation. Use `idf.py flash` or the merged
factory image produced by the release workflow for a blank device so the bootloader, partition
table, OTA metadata, and app are written consistently.

Core-dump-to-flash and ELF output are enabled in `sdkconfig.defaults`, and the partition table
reserves space for the dump. Decode a panic with `idf.py coredump-info` or `idf.py coredump-debug`
from the exact build that produced the running image. The CLI `system crash-dump` command remains a
separate clean-restart snapshot; it is not a panic core dump.

## Active NFF Pins

`firmware/domes/main/config.hpp` is authoritative. The reviewed table is also maintained in
`docs/PIN_REFERENCE.md`; do not duplicate additional pin tables in runbooks.

## Commands

Run from the repository root:

```bash
CONFIG_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$CONFIG_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$CONFIG_ROOT/sdkconfig" build)
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$CONFIG_ROOT/build" -D "SDKCONFIG=$CONFIG_ROOT/sdkconfig" size)
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$CONFIG_ROOT/build" -D "SDKCONFIG=$CONFIG_ROOT/sdkconfig" size-components)
```

Use an isolated `menuconfig` invocation only when deliberately reviewing generated options, then
move the intended durable settings into `sdkconfig.defaults` and repeat the clean build:

```bash
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$CONFIG_ROOT/build" -D "SDKCONFIG=$CONFIG_ROOT/sdkconfig" menuconfig)
```

Flash through the CP2102N programming port:

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh firmware/domes "$PORT"
```

After changing `sdkconfig.defaults`, use an isolated SDKCONFIG before claiming the defaults were
tested. After changing a protobuf schema, run `tools/generate_protocols.sh`; an ordinary firmware
build does not regenerate committed bindings.
