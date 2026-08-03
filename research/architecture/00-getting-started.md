# 00 - Retired Getting-Started Scaffold

> **Document status: Retired decision record.** The original tutorial was removed because its
> ESP-IDF version, project root, 16 MB partition layout, source tree, flash port, and expected output
> did not describe this repository. Git history retains that proposal. Do not use historical
> revisions as setup instructions.

## Current Entry Points

Use these maintained documents instead:

| Need | Current authority |
| --- | --- |
| Repository setup and orientation | [`../../README.md`](../../README.md) |
| Firmware build and programming | [`../../firmware/README.md`](../../firmware/README.md) |
| Exact verification matrix | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| NFF board bring-up | [`../../hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../../hardware/nff-devboard/BRING_UP_CHECKLIST.md) |
| Contributor and agent rules | [`../../AGENTS.md`](../../AGENTS.md) |

The active firmware project is `firmware/domes/`, uses ESP-IDF v5.4.4, and targets the checked-in
8 MB NFF development profile. The CP2102N `/dev/serial/by-id/` path carries flashing, framed UART
commands, and serial OTA; native ESP32-S3 USB Serial/JTAG is the separate console/JTAG interface.

## Retired Assumptions

The initial scaffold proposed all of the following, none of which is current:

- a `firmware/` ESP-IDF project root rather than `firmware/domes/`;
- an older ESP-IDF release instead of the locked v5.4.4 toolchain;
- a 16 MB development partition table instead of the active 8 MB NFF layout;
- direct `/dev/ttyUSB0` identity rather than stable CP2102N serial-number links;
- source, test, and mock paths that were never adopted; and
- a one-megabyte application target that does not match the current OTA slot contract.

This file remains only so links from the other early architecture records resolve to an explicit
retirement notice.
