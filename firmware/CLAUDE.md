# DOMES Firmware Claude Code Compatibility Guide

Firmware instructions are maintained in [`AGENTS.md`](AGENTS.md), with repository-wide requirements
in [`../AGENTS.md`](../AGENTS.md). Claude Code sessions working in this directory must follow both.

Current supporting documentation:

- [`README.md`](README.md): as-built firmware layout and protocol ownership
- [`MILESTONES.md`](MILESTONES.md): implementation and hardware-verification status
- [`../docs/TESTING.md`](../docs/TESTING.md): required host, ESP-IDF, and device checks
- [`../docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md): reviewed board mapping
- [`../research/SOFTWARE_ARCHITECTURE.md`](../research/SOFTWARE_ARCHITECTURE.md): system boundaries and authority map

This compatibility file intentionally contains no independent coding, protocol, pin, or build rules.
