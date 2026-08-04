# 03 - Driver Development

> **Document status: Retired decision record.** The original implementation tutorial was removed on
> 2026-08-02 because its example interfaces, filenames, service layer, build commands, and
> initialization order no longer matched the firmware. Git history preserves the proposal.

## Retained Decisions

DOMES keeps hardware access behind narrow interfaces where substitution provides useful host
coverage. Concrete drivers may use ESP-IDF directly, while services and control logic depend on the
smallest interface they need. Driver construction and lifetime remain explicit in `main.cpp`.

New firmware code follows the repository conventions: camelCase filenames and methods, PascalCase
classes, `I`-prefixed interfaces, explicit `esp_err_t` handling, and no exceptions or RTTI.
Interface extraction is a design choice for testability, not a requirement to recreate the generic
class hierarchy from the retired proposal.

## Current Authorities

| Concern | Current source |
| --- | --- |
| Firmware conventions and initialization constraints | [`firmware/AGENTS.md`](../../firmware/AGENTS.md) |
| Driver-facing interfaces | [`firmware/domes/main/interfaces/`](../../firmware/domes/main/interfaces/) |
| Concrete driver implementations | [`firmware/domes/main/drivers/`](../../firmware/domes/main/drivers/) |
| Active board pins and device configuration | [`firmware/domes/main/config.hpp`](../../firmware/domes/main/config.hpp) |
| Dependency construction and startup order | [`firmware/domes/main/main.cpp`](../../firmware/domes/main/main.cpp) |
| Host coverage | [`firmware/test_app/`](../../firmware/test_app/) |
| Build and hardware verification | [`docs/TESTING.md`](../../docs/TESTING.md) |
| Delivered status and remaining peripheral work | [`PROGRAM_STATUS.md`](../../PROGRAM_STATUS.md) |

## Superseded Material

The removed tutorial's `IPowerDriver`, `FeedbackService`, snake_case paths, generic mock tree,
copied pin values, and standalone build/flash sequence are not implementation commitments. Add a
component only when the current architecture requires it, then verify it using the maintained
matrix and the affected hardware.

See the [architecture lifecycle index](README.md) before using another detailed design record as an
implementation guide.
