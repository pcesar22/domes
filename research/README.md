# DOMES Research And Design Documents

This directory contains the current software overview, product target documents, detailed design
history, and archived plans. Delivery status and executable workflows live elsewhere; start with
[`../docs/README.md`](../docs/README.md) when deciding which source owns a fact.

## Document Map

| Document | Lifecycle | Use |
| --- | --- | --- |
| [`SOFTWARE_ARCHITECTURE.md`](SOFTWARE_ARCHITECTURE.md) | Current as-built overview | Software surfaces, runtime boundaries, protocol ownership, and implementation links |
| [`PRODUCT_DEFINITION.md`](PRODUCT_DEFINITION.md) | Product hypothesis | Intended customer, value proposition, launch scope, and requirements-entry outputs |
| [`SYSTEM_ARCHITECTURE.md`](SYSTEM_ARCHITECTURE.md) | Product target | Intended hardware, networking, power, enclosure, and production direction |
| [`ID_REQUIREMENTS.md`](ID_REQUIREMENTS.md) | Product target | Industrial-design goals and unresolved physical-design inputs |
| [`architecture/README.md`](architecture/README.md) | Current lifecycle index | Classification and replacements for detailed architecture proposals |
| [`AI_DEVELOPMENT_RECOMMENDATIONS.md`](AI_DEVELOPMENT_RECOMMENDATIONS.md) | Retired decision record | Adopted agent-workflow decisions and links to current authorities; obsolete commands were removed |
| [`archive/README.md`](archive/README.md) | Archive index | Superseded roadmaps, implementation plans, and simulation artifacts |

## Authority Boundaries

- Use [`../firmware/MILESTONES.md`](../firmware/MILESTONES.md) for implemented and hardware-verified
  status.
- Use [`../docs/PRODUCT_REALIZATION_FRAMEWORK.md`](../docs/PRODUCT_REALIZATION_FRAMEWORK.md) for
  phase entry, exit, and status-reporting rules.
- Use [`../docs/TESTING.md`](../docs/TESTING.md) for build, test, and hardware verification commands.
- Use [`../docs/PIN_REFERENCE.md`](../docs/PIN_REFERENCE.md), the active firmware configuration, and
  board design files for GPIO and physical connectivity.
- Use protobuf schemas and implementation source for wire contracts.
- Treat prices, performance targets, proposed parts, and unimplemented flows in research documents as
  inputs to validate, not current capabilities.

Detailed proposals remain in place to preserve rationale and inbound links. Their lifecycle banners
and the architecture index distinguish implemented boundaries from future design. Retired records
link to current authorities instead of preserving obsolete commands or copied lookup tables.
