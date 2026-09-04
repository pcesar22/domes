# 07 - Retired Debugging Scaffold

> **Document status: Retired decision record.** The original GDB line breakpoints, OpenOCD examples,
> coredump settings, direct device paths, permission workarounds, and copied diagnostic output were
> removed because operational details had diverged from the firmware and ESP-IDF v5.4.4. Git history
> retains that material. Use current firmware documentation and the exact candidate artifacts instead.

## Current Authorities

| Subject | Current authority |
| --- | --- |
| Build, flash, monitor, and hardware validation | [`../../docs/TESTING.md`](../../docs/TESTING.md) and [`../../tools/firmware/`](../../tools/firmware/) |
| Current verification requirements | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| Firmware programming and diagnostic boundaries | [`../../firmware/README.md`](../../firmware/README.md) |
| Trace implementation and workflow | [`trace-overhaul-architecture.md`](trace-overhaul-architecture.md) |
| Panic-dump configuration and partition | [`sdkconfig.defaults`](../../firmware/domes/sdkconfig.defaults) and [`partitions.csv`](../../firmware/domes/partitions.csv) |

On the NFF DevKit, the CP2102N serial-number path carries flashing and framed runtime traffic.
Native ESP32-S3 USB Serial/JTAG is the separate console/JTAG interface. Device permissions belong in
the project udev policy, not ad hoc world-writable device modes.

The legacy `domes-cli system crash-dump` command reads an NVS snapshot captured during a clean
restart. A panic dump is a separate ESP-IDF ELF coredump and must be retrieved and decoded with the
exact `domes.elf` for the running image. The resolved SDKCONFIG owns the active checksum and dump
settings; do not copy historical values into a current investigation.

## Retired Assumptions

The removed scaffold assumed stable `/dev/ttyUSB0` identity, source-line breakpoints that survived
code movement, a SHA-256 coredump setting that is not in the current defaults, manual `chmod 666`,
and trace capabilities that were never wired into the active recorder.

This file remains only so links from other early design records resolve to an explicit retirement
notice.
