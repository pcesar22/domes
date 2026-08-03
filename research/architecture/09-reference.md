# 09 - Retired Reference Tables

> **Document status: Retired decision record.** The copied pin, NVS, partition, UUID, error-code,
> timing, and capacity tables were removed because they diverged from their owning source files. This
> path remains as a stable redirect for older links; it is not a live lookup reference.

## Current Authorities

| Subject | Authority |
| --- | --- |
| Documentation ownership | [`../../docs/README.md`](../../docs/README.md) |
| Active firmware pins and constants | [`../../firmware/domes/main/config.hpp`](../../firmware/domes/main/config.hpp) |
| Human-readable NFF pin reconciliation | [`../../docs/PIN_REFERENCE.md`](../../docs/PIN_REFERENCE.md) |
| Physical carrier nets | [`../../hardware/nff-devboard/docs/schematic.pdf`](../../hardware/nff-devboard/docs/schematic.pdf) and EasyEDA source under `hardware/nff-devboard/source/` |
| NVS namespaces and keys | Owning headers under [`../../firmware/domes/main/infra/`](../../firmware/domes/main/infra/) and service implementations |
| Flash layout | [`../../firmware/domes/partitions.csv`](../../firmware/domes/partitions.csv) and [`sdkconfig.defaults`](../../firmware/domes/sdkconfig.defaults) |
| Config and trace messages | [`../../firmware/common/proto/`](../../firmware/common/proto/) |
| BLE service and characteristics | [`../../firmware/domes/main/transport/bleOtaService.hpp`](../../firmware/domes/main/transport/bleOtaService.hpp) |
| ESP-NOW messages | [`../../firmware/domes/main/services/espNowProtocol.hpp`](../../firmware/domes/main/services/espNowProtocol.hpp) |
| CLI syntax | `domes-cli --help` and [`../../tools/domes-cli/README.md`](../../tools/domes-cli/README.md) |
| Build and verification limits | [`../../docs/TESTING.md`](../../docs/TESTING.md) |
| Delivered status | [`../../firmware/MILESTONES.md`](../../firmware/MILESTONES.md) |

## Decision

A manually maintained all-in-one reference appeared convenient but duplicated fast-changing code,
hardware, and protocol facts. It accumulated values for board profiles, storage keys, wire formats,
and partitions that were never implemented together. The project now keeps each fact with its owning
source and uses focused human guides only where reconciliation adds value.

Do not recreate a global constants catalog in documentation. When a source-controlled value needs a
human explanation, link the owner and state whether the value is current, historical, or a product
target.

## Historical Material Removed

The retired content included preliminary board pin maps, proposed NVS schemas, invented project
error values, speculative memory budgets, proposed partition layouts, and obsolete BLE UUIDs. Git
history retains those inputs for archaeology without allowing them to masquerade as the active NFF
configuration.
