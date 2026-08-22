# VC-WP-002A prequalification freeze

`public-freeze-interface.fixture.json` is a blinded, synthetic positive fixture. It is not evidence
that FS-WP-002H or FS-WP-002G has executed. The operational gate must reject issue #159 closure or
its planning artifact and accept only a later terminal execution child with immutable lineage.

Generate and verify the example artifacts before any held-out campaign exists:

```sh
python3 tools/simulation/vc_prequalification.py \
  tools/simulation/qualification/public-freeze-interface.fixture.json \
  --manifest tools/simulation/qualification/frozen-manifest.fixture.json \
  --report tools/simulation/qualification/prequalification-report.fixture.json
python3 -m unittest tools/simulation/test_vc_prequalification.py
```

The generated files are canonical JSON. Their embedded SHA-256 digest detects every post-freeze
edit. No file in this directory contains calibration parameter derivation or held-out results.
