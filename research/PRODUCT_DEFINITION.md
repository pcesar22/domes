# DOMES Product Brief And Canonical Six-Pod Workflow

This is the product hypothesis, workflow, and requirements-entry authority for DOMES. It defines the
experience the product must make coherent without treating development hardware, prototype UI, or
uncollected customer evidence as product proof. Delivery status and accepted evidence live in
[`../PROGRAM_STATUS.md`](../PROGRAM_STATUS.md).

| Control | Value |
| --- | --- |
| Baseline | `PS-WP-001` hypothesis baseline |
| State | Working product decision; PS0 customer and economic validation remain open |
| Owner | CEO/product owner with AI product/system lead |
| Last reviewed | 2026-08-04 |
| Applies to | Requirements entry for PS1, FS3, HW1, VC1, and G1 |
| Does not authorize | Product claim, market validation, architecture/part freeze, spend, fabrication, or release |

## How To Read This Brief

Material statements have one of these meanings:

| Class | Meaning in this document |
| --- | --- |
| **Evidence** | Direct, dated repository or external observation with a stated boundary |
| **Hypothesis** | A proposition that must be tested with customers, economics, or representative product evidence |
| **Working decision** | The current boundary used to drive engineering until its invalidation rule fires |
| **Target** | Intended product behavior that is not a claim about the current implementation |
| **Current gap** | Missing, divergent, or unverified behavior that downstream work must close |

Working decisions are reversible during P1. They become accepted product requirements only through
stable identifiers, rationale, measurable bounds, ownership, and verification mapping in PS1.

## Product Brief

### Vision And Differentiated Promise

**Working decision:** DOMES is an open, local-first reaction-training system of portable light,
touch, sound, and haptic pods. Its commercial wedge is a dependable coach workflow plus ownership of
the hardware, software, data, update path, repair information, and reproducible source. Feature count
alone is not the differentiator.

**Hypothesis:** A buyer will choose DOMES over a closed incumbent when the complete six-pod workflow
is fast and reliable, core operation has no recurring fee or cloud dependency, results remain locally
owned and exportable, and the product has a credible repair and open-source path.

The project has not yet established a repository license or release-content decision. “Open source”
is therefore a product objective, not a current distribution claim.

### Buyer, Operator, User, And Job

| Role | P1 hypothesis | What remains unverified |
| --- | --- | --- |
| Economic buyer | Independent coach/trainer or the program owner of a small club or team | Purchase authority, budget cycle, willingness to pay, and service expectations |
| Operator | Coach or trainer who configures and supervises repeated sessions | Setup tolerance, drill-authoring needs, group size, and required history/analytics |
| Physical user | Athlete or training client who responds to pod cues | Age/accessibility range, cue preferences, strike behavior, and acceptable failure modes |
| Primary job | Set up and run a repeatable indoor reaction/agility drill, understand misses and reaction results, then pack the kit for the next session | Whether this outranks rehabilitation, cognitive-motor, education, or individual training jobs |

Rehabilitation and medical-adjacent use remain discovery segments. This baseline makes no medical
purpose, therapeutic outcome, safety, or regulatory claim.

### Launch Kit Boundary

**Working decision:** The baseline offer is one coherent six-pod kit:

- six interchangeable form-factor pods with stable product identities;
- one supported phone app used to configure, supervise, and review a session; the phone is not part
  of the purchased kit;
- charging and storage for all six pods, with unambiguous per-pod and kit readiness;
- touch, visible color cues, audio, and haptic capability on each pod;
- field update, rollback/recovery, diagnostics, local result storage, and user-controlled export;
- the documentation and source-release content required by the eventual open-product decision.

Accessories, cloud services, athlete-management depth, outdoor mounting, and kits larger than six
are not required for the first offer. The app may use network access for installation or an explicit
update, but the canonical setup, drill, results, and device-management workflow must remain usable
without cloud availability after installation.

### Operating Environment

**Working decision:** Launch use is supervised, indoor training on flat gym wood, rubber, vinyl,
table, or concrete surfaces. The pod must be legible under typical indoor gym lighting and stable
under the accepted touch interaction.

Grass, turf, uneven outdoor surfaces, rain exposure, unsupervised use, impact beyond accepted product
requirements, and medical environments are outside this baseline. IP54, drop, touch-force, dimension,
and material values in [`ID_REQUIREMENTS.md`](ID_REQUIREMENTS.md) remain targets pending PS1 allocation
and verification; they do not expand the launch environment by implication.

### Economic Guardrails

**Dated evidence:** On 2026-08-04 the option selector on the official BlazePod US build-your-own page
listed six pods at USD 439, while the page also displayed promotional pricing elsewhere. Its
membership page listed a free tier and an optional Pro tier at USD 79.99 per year or USD 10.39 per
month. These are list-page observations, not a completed transaction or DOMES demand evidence; tax,
shipping, promotion eligibility, returns, and channel economics are unknown. Refresh them before a
gate uses them: [kit](https://www.blazepod.com/products/single-product),
[membership](https://www.blazepod.com/pages/membership).

**Hypothesis guardrails for P1 engineering:**

| Economic input | Working bound | Invalidation / next evidence |
| --- | --- | --- |
| Customer price | USD 349-439 for the complete six-pod kit; the lower endpoint is a deliberate approximately 20% switching-price hypothesis below the dated USD 439 option, not observed demand | Replace with interview, purchase, channel, tax, and regional evidence; CEO owns offer/price commitment |
| Core software access | No paid subscription required for the canonical offline workflow, local results, export, diagnostics, or updates | Test whether buyers value this enough to affect purchase; optional paid services must not remove purchased core capability |
| Fully burdened product cost | At or below 50% of planned net selling price | HW1 must include electronics, enclosure, charger, packaging, assembly, test, inbound freight, scrap, and warranty reserve before G1 |
| Support and warranty | Included in the purchase economics; duration and reserve uncommitted | Customer discovery, reliability evidence, channel model, repair strategy, and legal review |
| Open-product cost | Documentation, source preparation, compliance, security response, and spare/repair support are real launch costs | Open-source plan and release BOM must enter product economics before DVT |

For this guardrail, **net selling price** is the revenue DOMES retains per kit after discounts,
rebates, returns, and channel commissions, excluding pass-through sales tax and separately charged
outbound shipping. The price and margin ranges are planning hypotheses, not an approved offer. If a
credible cost stack cannot close inside the price hypothesis with the required safety, reliability,
support, compliance, and open-product scope, the team must change the offer or product boundary
before architecture freeze; it may not hide the miss by excluding required costs.

### Product Principles

1. Core setup, execution, results, export, and device management work without cloud availability.
2. Six pods present one understandable kit while preserving traceable per-pod identity and state.
3. Physical feedback is immediate, legible, and robust in the accepted operating environment.
4. Partial failure is visible, bounded, and recoverable; the system never silently changes roster or
   scoring semantics.
5. Updates are authenticated, recoverable, observable, and possible without replacing hardware.
6. Results identify their configuration, roster, timing domain, missing data, and recovery events.
7. Product source and manufacturing information are sufficient for the accepted open-product scope.
8. Claims remain bounded by measured evidence; simulation and development boards are not form-factor
   product proof.

## Canonical Six-Pod Workflow

### Actors And Session Objects

- The **operator** owns one active session through the supported app.
- The **participant** performs the physical drill; a participant label is optional in the first
  baseline, but the result must not imply a verified person identity when none was entered.
- The **kit roster** is the six stable product pod identities available to the session. Transport
  addresses, CLI aliases, and radio MACs are implementation identifiers, not user-facing identity.
- The **active roster** is the exact subset required by the selected drill definition. The canonical
  baseline uses all six; a reduced-roster variant must be explicitly defined and selected.
- The **drill definition** is a versioned, validated description of targets, timing, cues, completion,
  cancellation, and permitted degradation. FS3 owns its eventual protocol and execution topology.
- The **session result** binds the drill definition, active roster, per-round outcome, timing domain,
  interruptions, recovery decisions, and completion state.

### Reference Flow

| ID | Operator-visible step | Successful target outcome | Failure and recovery contract |
| --- | --- | --- | --- |
| CW-01 | Unpack and power | Remove six pods from storage, power them, open the installed app, and see that the app can operate offline | A pod that cannot power or report identity/readiness is named and excluded from readiness; the app does not show “ready” for the kit |
| CW-02 | Identify kit roster | Discover the six stable pod identities, show each pod’s location with a bounded identify cue, and confirm firmware/config compatibility and available energy | Duplicate, unknown, incompatible, or missing identity blocks the six-pod preflight until the operator resolves or explicitly chooses a compatible reduced-roster drill |
| CW-03 | Choose participant and drill | Select an existing template or create a bounded drill; optionally attach a participant/session label | Invalid or unsupported values are rejected before any pod changes mode; last-known configuration is not silently reused |
| CW-04 | Configure and validate | Set active roster, rounds/duration, cue policy, delay/timeout, completion, and permitted recovery; review one plain-language summary | The complete versioned definition is validated for resource, timing, roster, and capability bounds before arming |
| CW-05 | Place and preflight | Place all six pods, run identify/readiness once more, and receive one explicit “ready to start” state | A moved, disconnected, low-energy, or unhealthy required pod returns the workflow to a named preflight action |
| CW-06 | Start and play | Start once; exactly one session authority arms cues, accepts only correlated input, provides immediate feedback, and advances according to the definition | Duplicate commands, stale touches, late results, or another controller cannot advance the active round |
| CW-07 | Handle partial failure | Pause safely when a required pod or controller path fails; preserve completed rounds and identify the affected pod and round | No silent substitution or implicit five-pod continuation. Retry/rejoin is allowed only with the same identity and correlated session state; reduced-roster continuation requires a definition that permits it plus explicit operator confirmation |
| CW-08 | Recover, abort, or resume | Offer the operator bounded choices: retry the affected round, resume from the recorded boundary, continue an allowed degraded variant, or abort with partial results | Every choice records interruption, roster/config change, invalidated round, and result completeness; failure cleanup returns reachable pods to a safe non-cue state |
| CW-09 | Review results | Show hits, misses, reaction results, completion state, pod/round breakdown, and interruption/recovery annotations; save locally and export under user control | Results with missing, phone-timed, uncorrelated, or partial data say so and are not promoted to synchronized product measurements |
| CW-10 | End session | End explicitly, clear cues, return reachable pods to idle, disconnect control paths, and retain the result | Cleanup is best effort but visible; unreachable pods remain named for manual recovery and are not shown as shut down |
| CW-11 | Charge and store | Power off as designed, place all six into the accepted consolidated charging/storage solution, and see unambiguous per-pod and kit readiness before the next session | Missing or incorrectly placed pods, charge faults, temperature/fault conditions, and incomplete charge remain visible; the exact topology and any individual-charging fallback remain HW1 decisions |

The workflow specifies user-visible semantics, not the transport topology. It does not decide whether
the phone controls every pod directly, designates an execution pod, or uses another bounded authority
model. That downselect belongs to FS3 and must preserve the same identity, validation, cancellation,
failure, result, offline, and update contracts.

### Partial-Failure Decision

The baseline policy is **pause, preserve, disclose, and require an explicit operator decision**.
Automatic continuation is prohibited when a required pod disappears or session authority becomes
ambiguous. A retry must correlate to the same session/round; a rejoined device must match the same
stable product identity; a reduced-roster continuation must be allowed by the validated drill and
record a new active roster. Otherwise the only valid outcomes are recovery or an explicitly partial
abort.

This policy is a working product decision. PS1 and FS3 must quantify detection/recovery time, define
session authority and disconnect continuity, specify persistent state, and prove that implementation
cannot accept stale or duplicated events.

## Current Evidence And Gaps

The following is the requirements-entry baseline, not acceptance of the target workflow:

| Area | Direct current evidence | Gap to the canonical workflow |
| --- | --- | --- |
| Hardware scale | Two serialized NFF development boards have passed automated campaigns | No six-node, product power, battery, charger, enclosure, consolidated charging/storage, or form-factor evidence |
| Firmware drill | Each pod has a local game engine; current ESP-NOW service runs one fixed two-pod MAC-selected drill with correlated rounds | No general versioned drill, phone-selected authority, six-pod execution, or product failure/recovery contract |
| App connection | BLE transport, single-pod connection, and an internal multi-pod provider exist with unit/widget coverage | The production UI does not populate the multi-pod provider, so no real user path currently creates the drill roster |
| App drill types | UI models show reaction, sequence, and speed labels | All three currently use the same random-target round path; sequence and speed are not distinct implemented semantics |
| Input and scoring | Physical touch notification carries pod identity, pad, and pod-local timestamp; app accepts only the active address | App scoring uses phone `DateTime.now()` and does not consume the pod timestamp; there is no cross-clock correlation |
| Failure cleanup | App aborts on preparation/control/touch-stream errors and attempts LED-off/idle cleanup | No pause/rejoin/resume/degraded-roster UX, durable recovery boundary, or annotated partial-result contract |
| Results | App computes hit/miss, reaction summaries, per-address breakdown, and text/JSON export in memory | No durable session/player history, stable product-identity binding, timing provenance, or interruption metadata |
| Shutdown | App can clear LEDs, return reachable pods to idle, and disconnect | Idle/disconnect is not physical power-off; product shutdown and charging behavior are unimplemented |
| Open product | Repository contains source, build/test workflows, and architecture records | No accepted license, source-release bill, manufacturing release content, support model, or third-party obligation baseline |

The current authority map is [`SOFTWARE_ARCHITECTURE.md`](SOFTWARE_ARCHITECTURE.md). Detailed game and
multi-pod records are explicitly partial target designs in [`architecture/README.md`](architecture/README.md).

## Downstream Requirement Seeds

PS1, FS3, HW1, and VC1 must turn the workflow into measurable, owned, verifiable requirements. At
minimum they must allocate:

1. stable kit/pod/session/drill/result identity and replacement behavior;
2. supported phone/platform, offline boundary, installation, permissions, and local data lifecycle;
3. setup/preflight time, readiness inputs, identify behavior, and compatibility policy;
4. drill schema bounds, validation, authority, cue/input correlation, cancellation, and safe state;
5. local timing source, clock correlation, uncertainty, scoring, and result provenance;
6. disconnect detection, retry/rejoin, partial result, reduced-roster, and abort semantics;
7. touch, light, audio, haptic, visibility, stability, durability, and accessibility behavior;
8. battery runtime, charge time, six-pod charge capacity/topology, thermal/fault protection, status
   indication, and storage readiness;
9. authenticated update, rollback, recovery, diagnostics, support, and repair workflows;
10. launch environment, safety, security, privacy, compliance, reliability, and misuse boundaries;
11. kit price, fully burdened cost, warranty/support reserve, channel assumptions, and open-product
    release/support cost; and
12. verification methods and representative configurations for normal, boundary, and failure cases.

No seed above is an accepted numeric requirement until PS1 records its rationale, bound, owner,
verification method, and status.

The [`G1 requirements and interface candidate`](G1_REQUIREMENTS_AND_INTERFACES.md) performs that
allocation for PS-WP-002. It remains a candidate with explicit closure inputs; the link does not
promote its targets or bounded fallbacks to accepted requirements or G1 evidence.

## PS0 Discovery And Invalidation

This brief completes the PS-WP-001 hypothesis baseline; it does not complete PS0 validation. P1 must
still produce:

| Evidence output | Required content | Current state |
| --- | --- | --- |
| Customer evidence | Interview/observation protocol, participants, findings, contradictions, purchase evidence, and decision | `Not started` |
| Competitive workflow benchmark | Comparable kit, setup, drill, failure/recovery, results, offline, service, repair, current price, and ownership observations | `Started`; only dated official BlazePod offer observations recorded here |
| Product requirements | Stable IDs, rationale, measurable statement, verification method, environment, owner, and status | `Candidate`; PS-WP-002 allocation written, acceptance evidence remains open |
| Verification matrix | Every accepted product requirement mapped to test, analysis, inspection, or demonstration | `Not started` |
| Launch compliance matrix | Market, classification, standards/regulations, evidence route, owner, and timing | `Not started` |
| Product economics | Price, fully burdened cost, margin, warranty, support, certification, manufacturing, and open-product assumptions | `Started`; hypothesis guardrails only |
| Open-product plan | License decision, hardware/firmware/CLI/app/docs scope, editable manufacturing sources, third-party obligations, support, and release package | `Not started` |

Reopen the working product boundary when credible evidence changes the primary buyer/job, requires a
different minimum kit or environment, shows that the workflow cannot close technically or
economically, invalidates the no-subscription core, or makes a safety/compliance/support obligation
incompatible with the offer. Record the decision in this document and the resulting program impact
in `PROGRAM_STATUS.md`.
