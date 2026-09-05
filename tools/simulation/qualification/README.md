# VC-WP-002A prequalification freeze

`public-freeze-interface.fixture.json`, `frozen-manifest.fixture.json`, and
`prequalification-report.fixture.json` are blinded synthetic contract fixtures. They are not
evidence that FS-WP-002H or FS-WP-002G executed. `rejected-entry.fixture.json` is a synthetic
negative example for missing terminal evidence. It records no live issue state or qualification
result. Current acceptance evidence is tracked in terminal FS-WP-002H issue #171 and terminal
FS-WP-002G issue #164.

Before qualification, an independent review must establish the attestation from accepted terminal
G/H artifacts and retain its digest outside the candidate input. Preserve the exact terminal issue
identities, specification revision, commits, evidence hashes, and campaign lineage required by the
published interface. An issue-closure record, arbitrary issue number, or caller-asserted artifact
class cannot establish acceptance. The `controller_attestation` field and
`--controller-attestation-sha256` option retain their legacy names for interface compatibility;
the validator consumes the pinned record directly without a running controller. Retain the emitted
manifest digest independently of the manifest as well. Verification always compares against that
external pin and applies the complete published schema before checking corpus construction.

Generate and verify the example artifacts before any held-out campaign exists:

```sh
python3 tools/simulation/vc_prequalification.py \
  tools/simulation/qualification/public-freeze-interface.fixture.json \
  --controller-attestation-sha256 \
  755b2c1a3837b93c17f67f91150a5c17bedaef03e6aa1a9143e512b6bbd0ea1b \
  --manifest tools/simulation/qualification/frozen-manifest.fixture.json \
  --report tools/simulation/qualification/prequalification-report.fixture.json
python3 -m unittest tools/simulation/test_vc_prequalification.py
```

The generated fixtures are canonical JSON. Each mutant carries an immutable definition digest and
an execution-contract digest bound to the frozen model, declared prediction envelope, and tool.
The corpus completeness record binds all five envelope dimensions. The embedded digest detects
accidental edits; the required external digest pin rejects malicious recomputation. No file in
this directory contains calibration parameter derivation or held-out results.
