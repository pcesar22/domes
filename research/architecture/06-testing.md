# 06 - Retired Testing Scaffold

> **Document status: Retired decision record.** The original Unity/CMock suite, test executable,
> coverage targets, direct serial commands, and CI examples were removed because they were never the
> implemented repository test system. Git history retains that proposal. Use the maintained matrix
> and executable workflows below.

## Current Authorities

| Subject | Current authority |
| --- | --- |
| Required checks by change type | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| Host firmware tests and simulation | [`../../firmware/test_app/README.md`](../../firmware/test_app/README.md) |
| Current CI jobs and triggers | [`firmware-ci.yml`](../../.github/workflows/firmware-ci.yml) and [`firmware-hw-test.yml`](../../.github/workflows/firmware-hw-test.yml) |
| NFF programming and physical acceptance | [`../../hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../../hardware/nff-devboard/BRING_UP_CHECKLIST.md) |
| Dated software and hardware results | [`../../firmware/MILESTONES.md`](../../firmware/MILESTONES.md) |

The host suite uses GoogleTest and CTest under `firmware/test_app/`. It verifies deterministic host
logic and shared contracts but does not establish an ESP-IDF build, a transport result, radio timing,
or physical peripheral behavior. Device-facing changes require the affected builds and hardware
checks in `docs/TESTING.md`.

## Retired Assumptions

The early scaffold assumed Unity/CMock, colocated ESP-IDF test components, a special firmware test
binary, command-line test selectors on `domes.elf`, fixed coverage percentages, copied test counts,
and direct `/dev/ttyUSB0` identity. Those are not current commands, gates, or metrics.

This file remains only so links from other early design records resolve to an explicit retirement
notice.
