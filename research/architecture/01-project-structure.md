# 01 - Retired Project-Structure Proposal

> **Document status: Retired decision record.** The original tree, snake_case filenames, ETL
> component, platform headers, mock layout, and template TODOs were proposals that the repository did
> not adopt. Git history retains the full proposal. Use the current source tree and rules below.

## Current Authorities

| Subject | Current authority |
| --- | --- |
| Repository layout | [`../../README.md`](../../README.md) |
| Firmware layout | [`../../firmware/README.md`](../../firmware/README.md) |
| Firmware naming and coding rules | [`../../firmware/AGENTS.md`](../../firmware/AGENTS.md) |
| Current software boundaries | [`../SOFTWARE_ARCHITECTURE.md`](../SOFTWARE_ARCHITECTURE.md) |
| Host test organization | [`../../firmware/test_app/README.md`](../../firmware/test_app/README.md) |

The active high-level structure is:

```text
firmware/common/       Shared schemas, framing, OTA codec, and utilities
firmware/domes/        ESP-IDF application and NFF board profile
firmware/test_app/     GoogleTest/CTest host tests and simulation
tools/domes-cli/       Rust service and development CLI
tools/                 Protocol, trace, firmware, and documentation tooling
ios/domes_app/         Flutter controller application
hardware/              Schematics, BOM, board notes, and bring-up
docs/                  Current cross-cutting operational documentation
research/              Product targets, as-built architecture, and design history
```

Firmware application files use camelCase, classes use PascalCase, and interfaces use an `I` prefix
such as `iTouchDriver.hpp`. Host fakes belong under `firmware/test_app/`; production drivers and
services belong under `firmware/domes/main/`. ETL and `tl::expected` are not repository dependencies.

## Retired Decisions

The early proposal assumed multiple generated board profiles, a reusable `components/etl` tree,
snake_case firmware filenames, and Unity/CMock tests colocated under the ESP-IDF project. The
implemented project instead keeps one compiled NFF profile in `main/config.hpp`, uses standard C++
within the firmware constraints, and runs host tests from the standalone CMake project.

This file remains only as the resolution target for links from other early design records.
