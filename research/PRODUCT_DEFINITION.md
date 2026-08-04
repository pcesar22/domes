# DOMES Product Definition

This is the product hypothesis and requirements-entry record for DOMES. It deliberately separates
what the project intends to prove from what current hardware has demonstrated. Delivery status and
acceptance evidence live in [`../firmware/MILESTONES.md`](../firmware/MILESTONES.md).

**State:** Hypothesis; M1 validation incomplete
**Last reviewed:** 2026-08-03

## Product Vision

DOMES is an open, local-first reaction-training system of portable light, touch, sound, and haptic
pods. It should be desirable to coaches and athletes who need fast setup, reliable offline drills,
low-latency multi-pod behavior, and ownership of the hardware, software, data, and repair path.

The commercial wedge is not feature count. It is a dependable product experience combined with
customizability, inspectable behavior, local operation, repairability, and reproducible open source.
BlazePod is a competitive benchmark, not the product requirements authority.

## Initial Customer Hypothesis

The initial target segment is independent coaches, trainers, clubs, and small teams that run
repeatable reaction, agility, rehabilitation, or cognitive-motor drills and are constrained by
closed software, subscriptions, limited customization, or uncertain long-term support.

This hypothesis is not yet validated. M1 requires observed evidence sufficient to identify:

- the primary buyer and user;
- the highest-value training job and current workaround;
- the switching reason relative to existing products;
- the minimum acceptable kit and workflow;
- willingness to pay, including hardware and any support/service expectation; and
- which openness and repairability attributes affect purchase behavior rather than only preference.

## Product Principles

1. Core drill setup, execution, results, and device management work without cloud availability.
2. Physical feedback is immediate, legible, and robust in the stated operating environment.
3. Six pods behave as one understandable system under normal operation and recover predictably from
   partial failure.
4. Updates are authenticated, recoverable, observable, and possible without replacing hardware.
5. Product source and manufacturing information are sufficient for meaningful study, modification,
   repair, and reproduction at release.
6. Claims are bounded by measured evidence; simulation and development boards are not presented as
   form-factor product proof.

## Launch Scope Hypothesis

The first product is a six-pod kit for indoor training, controlled by a supported phone and usable
offline after installation. It includes configurable drills, multi-pod timing and result capture,
visible light, touch input, audio and haptic feedback, field update and rollback, diagnostics, and a
charging/storage solution.

The initial release does not promise medical-device use, outdoor weatherproofing beyond an accepted
rating, cloud analytics, arbitrary third-party radio compatibility, or every global market. Those
claims require explicit requirements and evidence.

## M1 Definition Outputs

Before this definition becomes an accepted product baseline, M1 must produce:

| Output | Required content | Current state |
| --- | --- | --- |
| Customer evidence record | Interview/observation protocol, participants, findings, contradictions, purchase evidence, and decision | `Not started` |
| Competitive benchmark | Comparable kit, workflow, latency, offline, service, repair, price, and ownership observations | `Not started` |
| Product requirements | Stable IDs, rationale, measurable statement, verification method, environment, owner, and status | `Not started` |
| Verification matrix | Every product requirement mapped to test, analysis, inspection, or demonstration | `Not started` |
| Launch compliance matrix | Market, product classification, applicable standards/regulations, evidence route, owner, and timing | `Not started` |
| Product economics | Target kit price, preliminary COGS, margin, warranty, support, certification, and manufacturing assumptions | `Not started` |
| Open-source plan | Hardware, firmware, CLI, app, documentation, licensing, third-party obligations, and release package | `Not started` |

Proposed numeric thresholds and design targets remain in
[`SYSTEM_ARCHITECTURE.md`](SYSTEM_ARCHITECTURE.md) and [`ID_REQUIREMENTS.md`](ID_REQUIREMENTS.md)
until customer evidence, engineering analysis, and a verification method make them accepted product
requirements.
