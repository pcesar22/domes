# Scoring differential campaign

`fixtures/fixed_two_pod_v1.json` is the immutable, versioned input shared by the fixed-drill
simulator and Flutter scoring checks. The C++ and Dart tests use its exact identities, outcomes,
tokens, and reaction-time values. `campaign.py` validates that input without inference, normalizes
the two result shapes, and emits two repeated outputs plus a machine-readable verdict.

Run the campaign and its fail-closed negative tests:

```bash
python3 tools/scoring_validation/campaign.py \
  --fixture tools/scoring_validation/fixtures/fixed_two_pod_v1.json \
  --output-dir tools/scoring_validation/artifacts
python3 -m unittest discover -s tools/scoring_validation -p 'test_*.py' -v
```

The verdict deliberately separates scoring equality from provenance. A matching software result
does not establish physical BLE, ESP-NOW, touch, or wall-clock equivalence.
