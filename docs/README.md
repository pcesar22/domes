# DOMES Documentation

This directory is the entry point for current project documentation. Use the ownership rules below
when documents disagree; do not resolve conflicts by copying the same fact into more files.

## Source Of Truth

| Concern | Authoritative source | Supporting documentation |
| --- | --- | --- |
| Product-realization lifecycle and phase transitions | [`PRODUCT_REALIZATION_FRAMEWORK.md`](PRODUCT_REALIZATION_FRAMEWORK.md) | [`PROGRAM_GATE_TEMPLATE.md`](PROGRAM_GATE_TEMPLATE.md) |
| Delivery packages and dependencies | [`PROGRAM_MILESTONES.md`](PROGRAM_MILESTONES.md) | [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) and GitHub milestones |
| Product vision, customer, and launch hypotheses | [`research/PRODUCT_DEFINITION.md`](../research/PRODUCT_DEFINITION.md) | Accepted requirements and research evidence linked from it |
| CEO status, phases, gates, workstreams, hardware releases, and decisions | [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) | Pull requests and verification results |
| Program and gate contract structure | [`PROGRAM_GATE_TEMPLATE.md`](PROGRAM_GATE_TEMPLATE.md) | [`PRODUCT_REALIZATION_FRAMEWORK.md`](PRODUCT_REALIZATION_FRAMEWORK.md) |
| Firmware behavior | Code under [`firmware/domes/main/`](../firmware/domes/main/) | [`firmware/README.md`](../firmware/README.md) |
| Active compiled board profile and GPIO values | [`firmware/domes/main/config.hpp`](../firmware/domes/main/config.hpp) | [`PIN_REFERENCE.md`](PIN_REFERENCE.md) and board schematic |
| Config and trace messages | [`firmware/common/proto/`](../firmware/common/proto/) | [`tools/domes-cli/README.md`](../tools/domes-cli/README.md) |
| Legacy OTA chunk wire format | [`firmware/common/protocol/otaProtocol.hpp`](../firmware/common/protocol/otaProtocol.hpp) plus its Rust and Dart consumers | Compatibility tests in the CLI and Flutter app |
| Generated protocol consumers | [`tools/generate_protocols.sh`](../tools/generate_protocols.sh) | [`firmware/common/proto/README.md`](../firmware/common/proto/README.md) |
| Frame encoding and per-message response envelope | [`firmware/common/protocol/frameCodec.hpp`](../firmware/common/protocol/frameCodec.hpp) plus the paired firmware sender/host decoder | [`firmware/common/proto/README.md`](../firmware/common/proto/README.md) and protocol tests |
| CLI commands and options | `domes-cli --help` from [`tools/domes-cli`](../tools/domes-cli/) | [`tools/domes-cli/README.md`](../tools/domes-cli/README.md) |
| Automated verification | [`.github/workflows/firmware-ci.yml`](../.github/workflows/firmware-ci.yml) and its `CI Gate` | [`TESTING.md`](TESTING.md) |
| Hardware verification | [`.github/workflows/firmware-hw-test.yml`](../.github/workflows/firmware-hw-test.yml) plus retained device evidence | [`TESTING.md`](TESTING.md), [`PLATFORM.md`](PLATFORM.md), and [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) |
| Current hardware authorization and next-iteration definition | [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) and [`hardware/NEXT_ITERATION_REQUEST.md`](../hardware/NEXT_ITERATION_REQUEST.md) | [`hardware/README.md`](../hardware/README.md) |
| Panic coredumps and clean-restart snapshots | `firmware/domes/partitions.csv`, `sdkconfig.defaults`, and the owning firmware implementation | [`firmware/README.md`](../firmware/README.md) and project debug runbooks |
| System design and hardware targets | [`research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md) | Hardware files under [`hardware/`](../hardware/) |
| As-built software boundaries and decisions | [`research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md) | Implementation source and tests |
| Deterministic firmware virtual-platform target | [`research/architecture/13-deterministic-virtual-platform.md`](../research/architecture/13-deterministic-virtual-platform.md) | Current host boundary in [`research/architecture/10-host-simulation.md`](../research/architecture/10-host-simulation.md) and delivery state in [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) |
| Detailed design-document lifecycle | [`research/architecture/README.md`](../research/architecture/README.md) | [`research/README.md`](../research/README.md) |

Generated protobuf files are build artifacts derived from `.proto` files. They are never the place
to introduce a message or enum.

## Reading Order

1. [`README.md`](../README.md) for product scope and a minimal build path.
2. [`DEVELOPER_QUICKSTART.md`](../DEVELOPER_QUICKSTART.md) for local setup and the first verified change.
3. [`PRODUCT_REALIZATION_FRAMEWORK.md`](PRODUCT_REALIZATION_FRAMEWORK.md) for how phases start and exit.
4. [`PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) for the active phase and accepted evidence.
5. [`research/PRODUCT_DEFINITION.md`](../research/PRODUCT_DEFINITION.md) for product hypotheses.
6. [`research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md) for software boundaries.
7. [`research/SYSTEM_ARCHITECTURE.md`](../research/SYSTEM_ARCHITECTURE.md) for hardware and network targets.
8. [`research/architecture/README.md`](../research/architecture/README.md) for detailed design references.

## Guides By Task

| Task | Start here |
| --- | --- |
| Build, flash, or modify firmware | [`firmware/README.md`](../firmware/README.md) |
| Run or extend tests | [`TESTING.md`](TESTING.md) |
| Implement or audit deterministic ESP32-S3 simulation | [`research/architecture/13-deterministic-virtual-platform.md`](../research/architecture/13-deterministic-virtual-platform.md) |
| Use or extend the host CLI | [`tools/domes-cli/README.md`](../tools/domes-cli/README.md) |
| Build or extend the Flutter app | [`ios/domes_app/README.md`](../ios/domes_app/README.md) |
| Work with multiple pods, BLE, or Linux device setup | [`PLATFORM.md`](PLATFORM.md) |
| Check current GPIO assignments | [`PIN_REFERENCE.md`](PIN_REFERENCE.md) |
| Diagnose ESP32 crashes or inspect firmware with GDB | [`DEBUGGING.md`](DEBUGGING.md) |
| Exercise BLE, ESP-NOW, and individual peripherals | [`FIRMWARE_RUNBOOKS.md`](FIRMWARE_RUNBOOKS.md) |
| Bring up an NFF board | [`hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../hardware/nff-devboard/BRING_UP_CHECKLIST.md) |
| Start or audit the next hardware iteration | [`hardware/NEXT_ITERATION_REQUEST.md`](../hardware/NEXT_ITERATION_REQUEST.md) |
| Decide whether a product phase may start or exit | [`PRODUCT_REALIZATION_FRAMEWORK.md`](PRODUCT_REALIZATION_FRAMEWORK.md) |
| Create or audit phases, gates, work packages, or hardware releases | [`PROGRAM_GATE_TEMPLATE.md`](PROGRAM_GATE_TEMPLATE.md) |
| Inspect archived plans | [`research/archive/README.md`](../research/archive/README.md) |

## Document Lifecycle

[`research/architecture/README.md`](../research/architecture/README.md) owns lifecycle states,
promotion rules, and replacements for detailed design documents. This index only assigns factual
ownership and navigation.

Component contributor guides point to the sources above rather than defining independent
architecture specifications.

## Keeping Documentation Consistent

When behavior changes:

1. Change the authoritative code, schema, or workflow first.
2. Update `PROGRAM_STATUS.md` only after verification supports a status change.
3. Update the owning guide and any architecture decision affected by the change.
4. Search for the old command, pin, test count, or feature claim across tracked files.
5. Run the checks in [`TESTING.md`](TESTING.md), including
   `python3 tools/docs/check_markdown_links.py` for repository-relative links.
