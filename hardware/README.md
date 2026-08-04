# DOMES Hardware

This directory contains the current NFF development carrier, candidate product components, and the
work packages that mature those inputs into controlled product-hardware releases.

## Current Position

- [`nff-devboard/`](nff-devboard/) is the as-built development reference.
- [`BOM.csv`](BOM.csv) is a candidate component list, not an approved product BOM, AVL, or NFF
  manufacturing BOM.
- [`NEXT_ITERATION_REQUEST.md`](NEXT_ITERATION_REQUEST.md) is the active request for NFF
  characterization, architecture downselect, and component baseline.
- [`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md) owns program gates, current authorization, evidence,
  and hardware release status.

No product schematic, PCB layout, fabrication release, production M-BOM/AVL, placement file, or
form-factor hardware claim exists yet. Product targets in `research/` are inputs to HR1/HR2, not
frozen design decisions.

## Release Path

| Release | Result |
| --- | --- |
| HR0 | Measured NFF reference closes physical/electrical inputs |
| HR1 | Product architecture downselect closes topology |
| HR2 | Exact component baseline closes selected and alternate parts |
| HR3 | Controlled schematic release authorizes layout only |
| HR4 | Controlled PCB/manufacturing package supports EVT release-to-fab decision |
| HR5 | EVT exit supports DVT authorization |
| HR6 | DVT exit supports PVT authorization |
| HR7 | PVT exit supports release-candidate authorization |

Use [`AGENTS.md`](AGENTS.md) for sourcing and design-review rules.
