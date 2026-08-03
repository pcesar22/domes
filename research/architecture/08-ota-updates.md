# 08 - Retired OTA Proposal

> **Document status: Retired decision record.** The original 16 MB partition table, phone-relay and
> HTTP flows, raw WiFi transfer, four-megabyte size checks, and outdated CLI examples were removed
> because they are not the implemented update contract. Git history retains that proposal. Use the
> current firmware, host, and release authorities below.

## Current Authorities

| Subject | Current authority |
| --- | --- |
| Programming images and OTA operator workflow | [`../../firmware/README.md`](../../firmware/README.md) |
| OTA success, recovery, and rollback evidence | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| Active 8 MB partition layout | [`partitions.csv`](../../firmware/domes/partitions.csv) |
| Firmware OTA wire definitions | [`otaProtocol.hpp`](../../firmware/common/protocol/otaProtocol.hpp) |
| Rust CLI sender | [`ota.rs`](../../tools/domes-cli/src/commands/ota.rs) |
| Flutter sender | [`ota_protocol.dart`](../../ios/domes_app/lib/data/protocol/ota_protocol.dart) |
| Release and hardware automation | [`firmware-release.yml`](../../.github/workflows/firmware-release.yml) and [`firmware-hw-test.yml`](../../.github/workflows/firmware-hw-test.yml) |

`domes.bin` is an application image for an existing matching layout. A merged factory image contains
the bootloader, partition table, initial OTA metadata, and application needed for initial
programming; it is not a factory app partition.

Raw image transfer is supported over CP2102N-backed serial and BLE. The TCP config server does not
route raw OTA frames, and the CLI rejects that path. The declared OTA version must equal the version
embedded in the exact image, and the transmitted digest must match. A successful first boot does not
prove durable acceptance or forced failed-self-test rollback; follow `docs/TESTING.md` for the full
sequence.

## Retired Assumptions

The early proposal assumed a 16 MB development layout, four-megabyte OTA slots, a factory app
partition, HTTP and phone-relay transfers, raw WiFi image upload, independently typed example
versions, and multi-device commands that predated the strict version contract. None of those details
is a current programming or release instruction.

This file remains only so links from other early design records resolve to an explicit retirement
notice.
