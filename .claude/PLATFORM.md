# DOMES Claude Platform Compatibility Guide

Current host requirements, BLE constraints, udev setup, and multi-device workflows are maintained in
[`../.codex/PLATFORM.md`](../.codex/PLATFORM.md). Claude Code sessions must use that guide rather than
maintaining a second copy of platform commands here.

Repository verification requirements are in [`../docs/TESTING.md`](../docs/TESTING.md), and the
supported device interface is documented in [`../tools/domes-cli/README.md`](../tools/domes-cli/README.md).

Claude-specific commands under `.claude/commands/` are compatibility helpers. If a helper disagrees
with the current platform guide, CLI help, or testing matrix, the shared current source wins.
