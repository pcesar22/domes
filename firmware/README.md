# DOMES Firmware

ESP-IDF v5.4.4 firmware for an ESP32-S3 reaction-training pod. This version matches Software CI and
the component dependency lock; use it for reproducible local and release builds.

## Responsibilities

The firmware owns per-pod hardware, feature and mode state, local game timing, config/trace command
handling, and ESP-NOW participation. Host discovery, named-device registration, and command fan-out
belong to `tools/domes-cli`.

Integrated delivery status is maintained in [`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md). Software boundaries are
documented in [`../research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md).

## Build

```bash
VERIFY_ROOT="$(mktemp -d)"
(cd firmware/domes && . ~/esp/esp-idf/export.sh && \
  idf.py -B "$VERIFY_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$VERIFY_ROOT/sdkconfig" build)
```

Use `scripts/verify.sh` from the repository root for the complete final check. Do not treat an
incremental build that reused the ignored project-local `sdkconfig` as release evidence; that file
can preserve options removed or changed in `sdkconfig.defaults`.

Flash only when a device is available:

```bash
PORT="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh firmware/domes "$PORT"
```

The NFF DevKit CP2102N bridge (`/dev/ttyUSB*`) carries flashing, framed UART config, and serial OTA;
prefer its `/dev/serial/by-id/` link. Native USB Serial/JTAG (`/dev/ttyACM*`) is the separate
console/JTAG interface.

## Programming Images

The `domes.bin` application image, such as `$VERIFY_ROOT/build/domes.bin` from the clean command
above, is OTA-only. It does not contain the bootloader, partition table, or initial OTA metadata and
must not be used alone to provision a blank board. Use the flash helper from a matching checkout for
development. Software CI packages an unversioned `domes-factory.bin`; a tagged release publishes
the corresponding `domes-<tag>-factory.bin`. Both packages retain the exact `domes.elf` and
`project_description.json` needed to validate restart snapshots and symbolize panic dumps. The
factory image is for initial installation only when it matches the source and application image
being verified:

```bash
FACTORY_BIN='domes-<tag>-factory.bin'  # or domes-factory.bin from Software CI
APP_BIN='domes-<tag>.bin'              # or domes.bin from the same package
CLI=tools/domes-cli/target/release/domes-cli
POD_ID=1
sha256sum --check SHA256SUMS
EXPECTED_VERSION=$(
  python -m esptool image_info --version 2 "$APP_BIN" |
    sed -n 's/^App version: //p'
)
test -n "$EXPECTED_VERSION"
python -m esptool --chip esp32s3 --port "$PORT" erase_flash
python -m esptool --chip esp32s3 --port "$PORT" write_flash \
  0x0 "$FACTORY_BIN"
sleep 10
info=$($CLI --port "$PORT" system info)
grep -F "Firmware:   $EXPECTED_VERSION" <<< "$info"
grep -Eq 'Pod ID:[[:space:]]+not set$' <<< "$info"
$CLI --port "$PORT" system health
$CLI --port "$PORT" system self-test | grep -E 'Running App.*PASS.*ota_0,'
$CLI --port "$PORT" system set-pod-id "$POD_ID"
python -m esptool --chip esp32s3 --port "$PORT" run
sleep 10
$CLI --port "$PORT" system info | grep -Eq "Pod ID:[[:space:]]+$POD_ID$"
```

Erasing first is part of factory verification; otherwise stale NVS identity or OTA metadata can
survive and make a merged-image test inconclusive.

Use `domes.bin` with `domes-cli ota flash` only on a board that already has the matching partition
layout. The declared OTA version must be parser-valid, no longer than 31 ASCII bytes, and
byte-for-byte equal to the application version embedded in that exact image; firmware rejects a
mismatch before selecting the new boot partition. After OTA, verify the expected version, health,
and self-test, reboot again, and repeat those checks. A normal successful update does not exercise
the forced failed-self-test rollback path.

After firmware changes, follow [`../docs/TESTING.md`](../docs/TESTING.md). A build alone does not
verify device-facing behavior.

## Source Layout

```text
firmware/
  common/
    interfaces/       Shared transport contracts
    proto/            Config and trace protobuf schemas and nanopb output
    protocol/         Frame codec and legacy OTA transfer codec
    utils/            Shared utilities such as CRC32
  domes/
    components/       ESP-IDF component adapters
    main/
      config/         Command, feature, and system-mode management
      drivers/        Concrete hardware drivers
      game/           Per-pod game state machine
      infra/          Diagnostics, NVS, tasks, watchdog, crash/memory support
      interfaces/     Driver and service contracts
      services/       LED, touch, IMU, audio, WiFi, OTA, and ESP-NOW services
      trace/          Recorder, commands, dump snapshots, and streaming
      transport/      UART, TCP, BLE, serial OTA, and ESP-NOW adapters
      utils/          Firmware-only helpers
  test_app/
    main/             GoogleTest cases
    sim/              Host pod, ESP-NOW, and drill simulation
```

Composition and initialization order live in `domes/main/main.cpp`. The active compiled NFF board
profile and GPIO values live in `domes/main/config.hpp`.

## Protocol Ownership

Config and trace message definitions originate in:

- `common/proto/config.proto`
- `common/proto/trace.proto`

Committed nanopb C output and Flutter bindings are generated by `tools/generate_protocols.sh`;
`tools/domes-cli/build.rs` generates matching prost types in Cargo's build directory. An ordinary
firmware build does not regenerate the committed C files.

The common frame format is implemented by `common/protocol/frameCodec.hpp`:

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

Length and CRC both cover `Type + Payload`. New config or trace messages must start in a `.proto`
file. Config command request/response types occupy `0x20-0x4F` with reserved gaps; `0x50` is the
unsolicited device-originated touch notification, not another request. The existing OTA chunk
transfer and internal ESP-NOW peer packets are bounded fixed-binary exceptions; keep their mirrored
consumers wire-compatible until they are migrated.

Most config command responses carry `[Status:u8][Protobuf payload]` inside the frame. List and
diagnostic responses without command status, plus unsolicited notifications, carry only the
protobuf. The paired sender and decoder own this per-message distinction.

## Current Hardware Target

`domes/main/config.hpp` compiles the NFF DevKit mapping reviewed against the board schematic:

| Function | GPIO |
| --- | --- |
| LED ring | 16 |
| I2C SDA / SCL | 8 / 9 |
| IMU INT1 | 5 |
| I2S BCLK / LRCLK / data | 12 / 11 / 13 |
| Audio shutdown | 7 |
| Touch pads | 1, 2, 4, 6 |
| UART0 TX / RX to CP2102N | 43 / 44 |

Use [`../docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md) for platform context. If that document
and `config.hpp` disagree about the compiled target, fix the documentation or code in the same
change and verify the board schematic.

No other board profile is supported. The checked-in partition table and sdkconfig defaults target
the attached NFF ESP32-S3 N8R8 modules with 8 MB flash and 8 MB octal PSRAM.

That layout reserves a `0x20000` flash coredump partition and enables ESP-IDF ELF panic dumps.
Decode one with ESP-IDF and the exact matching `domes.elf`. The CLI's legacy
`system crash-dump` command returns a clean-restart NVS snapshot instead and is not a panic-dump
retrieval path. Current format-2 snapshots are CRC-protected and record the boot count, firmware
version, exact ELF SHA-256, internal heap, and processed PCs. Format-0 records are displayed only as
legacy data with unverified heap/backtrace semantics. Corrupt or unsupported records fail closed;
`system crash-dump --clear` remains an explicit recovery path.

## Host Tests

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

The suite covers framing, protobuf encoding, OTA transfer encoding, feature/mode state, game logic,
multi-pod simulation, and Perfetto export. See [`test_app/README.md`](test_app/README.md) and use
`ctest -N` rather than copying a test count into new documents.

## Contributor Guidance

Use [`../docs/PLATFORM.md`](../docs/PLATFORM.md) for host setup,
[`../docs/FIRMWARE_RUNBOOKS.md`](../docs/FIRMWARE_RUNBOOKS.md) for hardware procedures, and
[`../docs/DEBUGGING.md`](../docs/DEBUGGING.md) for GDB and panic-dump inspection.

- [`AGENTS.md`](AGENTS.md): firmware coding, memory, ISR, architecture, and validation rules.
- [`../docs/TESTING.md`](../docs/TESTING.md): repository verification matrix.
- [`../research/architecture/README.md`](../research/architecture/README.md): lifecycle and scope of
  detailed design references.
