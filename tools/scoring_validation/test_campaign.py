import copy
import json
import tempfile
import unittest
from pathlib import Path

from campaign import FixtureError, load_fixture, run_campaign, validate_fixture

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "fixed_two_pod_v1.json"


class CampaignTest(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture, _ = load_fixture(FIXTURE)

    def assert_rejected(self, mutate) -> None:
        candidate = copy.deepcopy(self.fixture)
        mutate(candidate)
        with self.assertRaises(FixtureError):
            validate_fixture(candidate)

    def test_repeated_outputs_and_verdict_are_identical(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            verdict = run_campaign(FIXTURE, Path(directory))
            first = (Path(directory) / "normalized-run-1.json").read_bytes()
            second = (Path(directory) / "normalized-run-2.json").read_bytes()
        self.assertEqual(first, second)
        self.assertEqual(
            verdict["normalized_output_sha256"][0],
            verdict["normalized_output_sha256"][1],
        )
        self.assertEqual(verdict["status"], "match_with_provenance_limitations")
        self.assertEqual(verdict["divergences"], [])

    def test_malformed_json_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            malformed = Path(directory) / "fixture.json"
            malformed.write_text('{"schema_version":')
            with self.assertRaisesRegex(FixtureError, "malformed JSON"):
                load_fixture(malformed)

    def test_incomplete_fixture_is_rejected(self) -> None:
        self.assert_rejected(lambda value: value["rounds"][0].pop("reaction_time_us"))

    def test_duplicate_token_is_rejected(self) -> None:
        self.assert_rejected(lambda value: value["rounds"][1].update(round_token=1))

    def test_wrong_pod_is_rejected(self) -> None:
        self.assert_rejected(
            lambda value: value["rounds"][0].update(target_identity="pod-unknown")
        )

    def test_missing_clock_provenance_is_rejected(self) -> None:
        self.assert_rejected(
            lambda value: value["paths"]["mobile"]["clock"].pop("origin")
        )

    def test_ambiguous_identity_is_rejected(self) -> None:
        self.assert_rejected(
            lambda value: value["pods"][1].update(identity=value["pods"][0]["identity"])
        )


if __name__ == "__main__":
    unittest.main()
