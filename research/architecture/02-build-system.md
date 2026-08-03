# 02 - Retired Build-System Scaffold

> **Document status: Retired decision record.** The original platform selector, Linux firmware
> target, source lists, partition examples, size limits, and flash commands were removed because
> they do not describe the implemented build. Git history retains that proposal. Use the maintained
> build inputs and verification documents below.

## Current Authorities

| Subject | Current authority |
| --- | --- |
| Repository verification entry point | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| Firmware build and programming | [`../../firmware/README.md`](../../firmware/README.md) |
| ESP-IDF project and component graph | [`firmware/domes/CMakeLists.txt`](../../firmware/domes/CMakeLists.txt) and [`firmware/domes/main/CMakeLists.txt`](../../firmware/domes/main/CMakeLists.txt) |
| Toolchain and component versions | [`dependencies.lock`](../../firmware/domes/dependencies.lock) and [`sdkconfig.defaults`](../../firmware/domes/sdkconfig.defaults) |
| Flash and OTA partition layout | [`partitions.csv`](../../firmware/domes/partitions.csv) |
| Host firmware test build | [`../../firmware/test_app/README.md`](../../firmware/test_app/README.md) |
| Pull-request and release automation | [`firmware-ci.yml`](../../.github/workflows/firmware-ci.yml) and [`firmware-release.yml`](../../.github/workflows/firmware-release.yml) |

The supported firmware target is the checked-in NFF ESP32-S3 profile built with ESP-IDF v5.4.4.
Host tests are a separate GoogleTest/CTest project; they are not a Linux firmware target. Release
evidence uses an isolated build directory and fresh SDKCONFIG as specified in `docs/TESTING.md`.

## Retired Assumptions

The early scaffold assumed multiple Kconfig-selected boards, manually maintained source lists, a
16 MB development layout, four-megabyte OTA slots, an executable Linux firmware image, copied binary
size budgets, and direct `/dev/ttyUSB0` commands. None of those assumptions is a current build or
programming contract.

This file remains only so links from other early design records resolve to an explicit retirement
notice.
