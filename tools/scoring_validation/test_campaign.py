import copy
import json
import tempfile
import unittest
from pathlib import Path

from campaign import (
    FixtureError,
    ResultError,
    compare_results,
    load_fixture,
    load_result,
    run_campaign,
    validate_fixture,
)
from generate_fixed_fixture import render

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "fixed_two_pod_v1.json"
GENERATED = HERE / "generated" / "fixed_two_pod_v1.hpp"


class CampaignTest(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture, self.digest = load_fixture(FIXTURE)

    def assert_rejected(self, mutate) -> None:
        candidate = copy.deepcopy(self.fixture)
        mutate(candidate)
        with self.assertRaises(FixtureError):
            validate_fixture(candidate)

    def path_result(self, name: str) -> dict:
        reactions = [
            round_data["reaction_time_us"]
            for round_data in self.fixture["rounds"]
            if round_data["hit"]
        ]
        return {
            "aggregate": {
                "average_reaction_us": sum(reactions) // len(reactions),
                "best_reaction_us": min(reactions),
                "hits": len(reactions),
                "misses": len(self.fixture["rounds"]) - len(reactions),
                "worst_reaction_us": max(reactions),
            },
            "clock_provenance": self.fixture["paths"][name]["clock"],
            "fixture_id": self.fixture["fixture_id"],
            "fixture_sha256": self.digest,
            "path": name,
            "result_provenance": self.fixture["paths"][name]["result"],
            "rounds": [
                {
                    "hit": source["hit"],
                    "index": source["index"],
                    "reaction_time_us": source["reaction_time_us"],
                    "round_token": source["round_token"] if name == "fixed" else None,
                    "target_identity": source["target_identity"],
                }
                for source in self.fixture["rounds"]
            ],
            "schema_version": 1,
        }

    def assert_result_rejected(self, name: str, mutate, message: str) -> None:
        candidate = self.path_result(name)
        mutate(candidate)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / f"{name}.json"
            path.write_text(json.dumps(candidate))
            with self.assertRaisesRegex(ResultError, message):
                load_result(path, name, self.fixture, self.digest)

    def test_repeated_independent_outputs_and_verdict_are_identical(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixed = root / "fixed.json"
            mobile = root / "mobile.json"
            fixed.write_text(json.dumps(self.path_result("fixed")))
            mobile.write_text(json.dumps(self.path_result("mobile")))
            verdict = run_campaign(
                FIXTURE, [fixed, fixed], [mobile, mobile], root / "out"
            )
            first = (root / "out" / "normalized-run-1.json").read_bytes()
            second = (root / "out" / "normalized-run-2.json").read_bytes()
        self.assertEqual(first, second)
        self.assertEqual(
            verdict["normalized_output_sha256"][0],
            verdict["normalized_output_sha256"][1],
        )
        self.assertEqual(
            verdict["comparison_verdict_sha256"][0],
            verdict["comparison_verdict_sha256"][1],
        )
        self.assertEqual(verdict["status"], "diverged")
        token_divergences = [
            item
            for item in verdict["divergences"]
            if item["field"].endswith("round_token")
        ]
        self.assertEqual(len(token_divergences), len(self.fixture["rounds"]))
        self.assertTrue(
            all(
                item["reason"] == "unavailable_in_mobile_result"
                for item in token_divergences
            )
        )

    def test_every_changed_independent_field_is_reported(self) -> None:
        fixed = self.path_result("fixed")
        mobile = self.path_result("mobile")
        mobile["aggregate"].update(
            hits=3, best_reaction_us=2000, worst_reaction_us=3000000
        )
        mobile["rounds"][0].update(hit=False, reaction_time_us=None)
        mobile["rounds"][1].update(target_identity="local-pod-0")
        _, verdict = compare_results(self.fixture, self.digest, fixed, mobile)
        fields = {item["field"] for item in verdict["divergences"]}
        self.assertTrue(
            {
                "aggregate.hits",
                "aggregate.best_reaction_us",
                "aggregate.worst_reaction_us",
                "rounds[0].hit",
                "rounds[0].reaction_time_us",
                "rounds[1].target_identity",
            }.issubset(fields)
        )

    def test_generated_cpp_binding_is_current(self) -> None:
        self.assertEqual(GENERATED.read_text(), render(FIXTURE))

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

    def test_result_without_clock_provenance_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixed = root / "fixed.json"
            mobile = root / "mobile.json"
            fixed.write_text(json.dumps(self.path_result("fixed")))
            candidate = self.path_result("mobile")
            candidate["clock_provenance"].pop("origin")
            mobile.write_text(json.dumps(candidate))
            with self.assertRaisesRegex(ResultError, "clock provenance"):
                run_campaign(FIXTURE, [fixed, fixed], [mobile, mobile], root / "out")

    def test_malformed_result_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixed.json"
            path.write_text('{"schema_version":')
            with self.assertRaisesRegex(ResultError, "missing or malformed"):
                load_result(path, "fixed", self.fixture, self.digest)

    def test_incomplete_result_round_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["rounds"][0].pop("reaction_time_us"),
            "round 0 is incomplete",
        )

    def test_duplicate_fixed_result_token_is_rejected(self) -> None:
        self.assert_result_rejected(
            "fixed",
            lambda value: value["rounds"][1].update(round_token=1),
            "token is duplicated",
        )

    def test_incorrect_fixed_result_token_is_rejected(self) -> None:
        self.assert_result_rejected(
            "fixed",
            lambda value: value["rounds"][1].update(round_token=99),
            "fixture token",
        )

    def test_wrong_known_result_pod_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["rounds"][0].update(target_identity="peer-pod-1"),
            "fixture pod identity",
        )

    def test_missing_hit_reaction_time_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["rounds"][0].update(reaction_time_us=None),
            "hit timing is missing",
        )

    def test_ambiguous_miss_reaction_time_is_rejected(self) -> None:
        self.assert_result_rejected(
            "fixed",
            lambda value: value["rounds"][2].update(reaction_time_us=1000),
            "miss has ambiguous reaction timing",
        )

    def test_boundary_result_reaction_time_is_rejected(self) -> None:
        self.assert_result_rejected(
            "fixed",
            lambda value: value["rounds"][0].update(reaction_time_us=3000000),
            "outside its boundary",
        )

    def test_sub_resolution_result_reaction_time_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["rounds"][0].update(reaction_time_us=1001),
            "outside its boundary",
        )

    def test_non_boolean_result_hit_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["rounds"][0].update(hit=1),
            "hit must be boolean",
        )

    def test_non_integer_aggregate_is_rejected(self) -> None:
        self.assert_result_rejected(
            "fixed",
            lambda value: value["aggregate"].update(hits=True),
            "aggregate hits must be an integer",
        )

    def test_inconsistent_aggregate_is_rejected(self) -> None:
        self.assert_result_rejected(
            "mobile",
            lambda value: value["aggregate"].update(average_reaction_us=1),
            "aggregate average_reaction_us is inconsistent",
        )


if __name__ == "__main__":
    unittest.main()
