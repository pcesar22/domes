# DOMES Claude Code Compatibility Guide

Project instructions are maintained in [`AGENTS.md`](AGENTS.md). Claude Code sessions must read and
follow that file before making changes.

Use [`docs/README.md`](docs/README.md) to find the authoritative project documentation and
[`docs/TESTING.md`](docs/TESTING.md) for verification requirements. When working in a scoped area,
also read its local guidance:

- [`firmware/AGENTS.md`](firmware/AGENTS.md)
- [`hardware/AGENTS.md`](hardware/AGENTS.md)
- [`tools/domes-cli/AGENTS.md`](tools/domes-cli/AGENTS.md)

Claude-specific commands and skills under `.claude/` are compatibility entry points only. They must
defer technical facts, workflow requirements, and platform constraints to the shared sources above
and [`.claude/PLATFORM.md`](.claude/PLATFORM.md).
