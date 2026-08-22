# Scoring differential campaign

`fixtures/fixed_two_pod_v1.json` is the single immutable input for this campaign. Its generated
C++ binding drives `sim::DrillOrchestrator::execute`; the Flutter test reads the JSON directly and
drives the production `DrillResult` getters. Each path emits its own machine-readable result.
`campaign.py` only validates and compares those independent outputs; it does not score fixtures.

Generate or check the C++ binding:

```bash
python3 tools/scoring_validation/generate_fixed_fixture.py \
  --fixture tools/scoring_validation/fixtures/fixed_two_pod_v1.json \
  --output tools/scoring_validation/generated/fixed_two_pod_v1.hpp --check
```

After building `firmware/test_app`, emit two fixed-simulator repetitions from the repository root:

```bash
DOMES_FIXED_SCORING_RESULT="$PWD/tools/scoring_validation/artifacts/fixed-run-1.json" \
  <test_app-binary> --gtest_filter=SimDrillTest.FixedTwoPodScoringFixture
DOMES_FIXED_SCORING_RESULT="$PWD/tools/scoring_validation/artifacts/fixed-run-2.json" \
  <test_app-binary> --gtest_filter=SimDrillTest.FixedTwoPodScoringFixture
```

From `ios/domes_app`, emit the mobile repetitions using the pinned Flutter toolchain:

```bash
DOMES_MOBILE_SCORING_RESULT="$PWD/../../tools/scoring_validation/artifacts/mobile-run-1.json" \
  flutter test test/domain/models/drill_result_test.dart \
  --plain-name 'matches the deterministic two-pod scoring fixture'
DOMES_MOBILE_SCORING_RESULT="$PWD/../../tools/scoring_validation/artifacts/mobile-run-2.json" \
  flutter test test/domain/models/drill_result_test.dart \
  --plain-name 'matches the deterministic two-pod scoring fixture'
```

Compare both repetitions and run the fail-closed negative tests:

```bash
python3 tools/scoring_validation/campaign.py \
  --fixture tools/scoring_validation/fixtures/fixed_two_pod_v1.json \
  --fixed-result tools/scoring_validation/artifacts/fixed-run-1.json \
  --fixed-result tools/scoring_validation/artifacts/fixed-run-2.json \
  --mobile-result tools/scoring_validation/artifacts/mobile-run-1.json \
  --mobile-result tools/scoring_validation/artifacts/mobile-run-2.json \
  --output-dir tools/scoring_validation/artifacts
python3 -m unittest discover -s tools/scoring_validation -p 'test_*.py' -v
```

The verdict reports the mobile result's absent round-token field as a divergence. Matching scoring
fields do not establish physical BLE, ESP-NOW, touch, host-clock capture, or wall-clock equivalence.

## Retained verification

`artifacts/verification/` retains the complete output from the software-only acceptance rerun of
commit `b9ce5be2b1a852bc00a6d20ea4e86222ce0d5483`:

- `cpp-tests.log` records the exact CMake build and CTest commands and all 321 passing host tests.
- `flutter-tests.log` records Flutter 3.44.8, locked dependency restore, the exact affected-test
  command, and all 40 passing affected tests.
- `campaign-tests.log` records all 22 passing comparator and fail-closed negative tests plus the
  fixture, normalized-output, and verdict SHA-256 digests.
- `verify.log` records the no-argument `scripts/verify.sh` full gate from a clean isolated checkout
  with initialized submodules. Protocol generation, host firmware, the Rust CLI, Flutter, and the
  ESP-IDF firmware build passed; host tooling failed on the supplied base revision because
  `test_materialize_plan_sets_execute_and_plan_child_states` does not mock the newly required
  `update_issue_body` call and therefore invokes unauthenticated `gh`. No live credential was
  supplied because doing so would mutate a real issue from a unit test.

The rerun is software evidence only. Physical BLE, ESP-NOW, touch, device timing, host-clock
capture, and wall-clock equivalence remain unverified.
