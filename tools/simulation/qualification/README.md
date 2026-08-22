# VC-WP-002A prequalification freeze

`public-freeze-interface.fixture.json`, `frozen-manifest.fixture.json`, and
`prequalification-report.fixture.json` are blinded synthetic contract fixtures. They are not
evidence that FS-WP-002H or FS-WP-002G executed. `operational-entry-report.json` is the retained
fail-closed execution record: terminal FS-WP-002H child #171 and terminal FS-WP-002G child #164
have no accepted execution evidence, so no operational qualification manifest was created.

The controller must pin the topology attestation digest outside the selector input. This prevents
an issue-closure record, arbitrary issue number, or caller-asserted artifact class from being
relabeled as accepted terminal evidence. The controller must likewise retain the emitted manifest
digest outside the manifest; verification always compares against that external pin and applies
the complete published schema before checking corpus construction.

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
