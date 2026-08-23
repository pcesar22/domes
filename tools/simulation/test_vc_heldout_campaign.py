#!/usr/bin/env python3
"""Adversarial and reproducibility tests for the frozen held-out campaign."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
MODULE = HERE / "vc_heldout_campaign.py"
SPEC = importlib.util.spec_from_file_location("vc_heldout_campaign", MODULE)
assert SPEC and SPEC.loader
campaign = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = campaign
SPEC.loader.exec_module(campaign)

MANIFEST_PATH = HERE / "qualification" / "frozen-manifest.fixture.json"
ENTRY_PATH = HERE / "qualification" / "operational-entry-report.json"


def manifest() -> dict:
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def accepted_entry(value: dict) -> dict:
    terminal = value["entry"]["terminal_candidate"]
    return {
        "schema_version": 1,
        "kind": "vc-wp-002a-operational-entry-report",
        "specification_revision": campaign.SPEC_REVISION,
        "entry_result": "passed",
        "manifest_created": True,
        "held_out_results_accessed": False,
        "manifest_sha256": value["manifest_sha256"],
        "review_state": "human-reviewed",
        "reviewed_manifest_sha256": value["manifest_sha256"],
        "terminal_fs_wp_002h": {
            "commit": terminal["commit"],
            "evidence_sha256": terminal["evidence_sha256"],
        },
        "terminal_fs_wp_002g": {
            "commit": terminal["fs_wp_002g"]["commit"],
            "evidence_sha256": terminal["fs_wp_002g"]["evidence_sha256"],
            "campaign_sha256": terminal["fs_wp_002g"]["campaign_sha256"],
        },
        "stop_condition": "software-review-required-human-review",
    }


def observations(value: dict) -> list[dict]:
    result = []
    order = 0
    for mutant in value["mutation_corpus"]:
        for scenario in value["held_out_scenarios"]:
            order += 1
            result.append(
                {
                    "order": order,
                    "mutant_id": mutant["id"],
                    "mutant_definition_sha256": mutant["definition_sha256"],
                    "mutant_class": mutant["class"],
                    "critical_seeded_fault": mutant["critical_seeded_fault"],
                    "scenario": scenario["id"],
                    "scenario_sha256": scenario["scenario_sha256"],
                    "seed": scenario["seed"],
                    "seed_sha256": scenario["seed_sha256"],
                    "dataset_sha256": scenario["dataset_sha256"],
                    "raw_trace_sha256": f"{order:064x}",
                    "normalized_trace_sha256": f"{order + 1000:064x}",
                    "clock_correlation_uncertainty_ns": value["clock_correlation"][
                        "uncertainty_ns"
                    ],
                    "tool_identities": copy.deepcopy(value["tools"]),
                    "expected_outcome": "candidate-prediction",
                    "observed_outcome": "mutant-divergence",
                    "unexplained_ordering_inversions": 0,
                    "timeout_absolute_error_us": 0,
                    "queue_depth_absolute_error": 0,
                    "latency_p95_absolute_error_us": 0,
                    "termination_state": "completed",
                    "divergence_explanation": "frozen mutation changed the outcome",
                }
            )
    return result


def evaluate(value: dict | None = None, result: list[dict] | None = None) -> dict:
    value = value or manifest()
    return campaign.evaluate(
        value,
        value["manifest_sha256"],
        accepted_entry(value),
        result if result is not None else observations(value),
    )


class HeldOutCampaignTests(unittest.TestCase):
    def test_operational_entry_rejection_does_not_access_held_out_data(self) -> None:
        entry = json.loads(ENTRY_PATH.read_text(encoding="utf-8"))
        report = campaign.rejected_report(entry)
        self.assertEqual(report["entry_result"], "rejected")
        self.assertFalse(report["campaign_executed"])
        self.assertFalse(report["manifest_accessed"])
        self.assertFalse(report["held_out_results_accessed"])
        self.assertEqual(report["case_inventory"], [])
        self.assertIn(
            "terminal-fs-wp-002h-execution-evidence-absent", report["reason_codes"]
        )

    def test_complete_frozen_matrix_is_evaluated_and_sealed(self) -> None:
        value = manifest()
        report = evaluate(value)
        expected_count = len(value["mutation_corpus"]) * len(
            value["held_out_scenarios"]
        )
        self.assertEqual(len(report["case_inventory"]), expected_count)
        self.assertEqual(
            [item["order"] for item in report["case_inventory"]],
            list(range(1, expected_count + 1)),
        )
        self.assertTrue(report["aggregate"]["thresholds_passed"])
        self.assertFalse(report["trust_verdict_issued"])
        body = {key: item for key, item in report.items() if key != "report_sha256"}
        self.assertEqual(report["report_sha256"], campaign.freeze_gate.digest(body))

    def test_observation_bundle_requires_embedded_and_external_digest(self) -> None:
        value = manifest()
        body = {
            "schema_version": 1,
            "kind": "vc-wp-002a-held-out-observation-bundle",
            "manifest_sha256": value["manifest_sha256"],
            "cases": observations(value),
        }
        bundle = {**body, "artifact_set_sha256": campaign.freeze_gate.digest(body)}
        self.assertEqual(
            campaign.verify_observation_bundle(
                bundle, bundle["artifact_set_sha256"], value["manifest_sha256"]
            ),
            bundle["cases"],
        )
        with self.assertRaises(campaign.CampaignError):
            campaign.verify_observation_bundle(
                bundle, "f" * 64, value["manifest_sha256"]
            )

    def test_rerun_is_byte_identical(self) -> None:
        first = evaluate()
        second = evaluate()
        self.assertEqual(
            campaign.freeze_gate.canonical_bytes(first),
            campaign.freeze_gate.canonical_bytes(second),
        )

    def test_lower_detection_is_not_rounded_into_acceptance(self) -> None:
        value = manifest()
        result = observations(value)
        for item in result[-4:]:
            item["observed_outcome"] = item["expected_outcome"]
            item["divergence_explanation"] = None
        report = evaluate(value, result)
        complete = report["aggregate"]["complete_corpus_detection"]
        self.assertEqual((complete["numerator"], complete["denominator"]), (71, 75))
        self.assertFalse(complete["passed"])
        self.assertFalse(report["aggregate"]["thresholds_passed"])
        self.assertEqual(len(report["aggregate"]["survivors"]), 4)

    def test_every_metric_bound_is_computed_not_supplied(self) -> None:
        value = manifest()
        result = observations(value)
        result[0].update(
            {
                "observed_outcome": "candidate-prediction",
                "unexplained_ordering_inversions": 1,
                "timeout_absolute_error_us": 251,
                "queue_depth_absolute_error": 2,
                "latency_p95_absolute_error_us": 501,
                "divergence_explanation": None,
            }
        )
        item = evaluate(value, result)["case_inventory"][0]
        self.assertEqual(
            [metric["id"] for metric in item["metric_evaluations"]],
            list(campaign.METRIC_IDS),
        )
        self.assertEqual(
            [metric["passed"] for metric in item["metric_evaluations"]],
            [True, False, False, False, False],
        )
        self.assertEqual(item["explanation_status"], "unexplained")

    def test_infrastructure_error_is_retained_and_not_counted_as_detection(
        self,
    ) -> None:
        value = manifest()
        result = observations(value)
        result[0]["termination_state"] = "infrastructure-error"
        report = evaluate(value, result)
        self.assertEqual(report["aggregate"]["infrastructure_errors"], [1])
        self.assertFalse(report["case_inventory"][0]["detected"])

    def test_rejects_missing_duplicate_reordered_or_extra_cases(self) -> None:
        value = manifest()
        complete = observations(value)
        for changed in (
            complete[:-1],
            complete + [copy.deepcopy(complete[-1])],
            [complete[1], complete[0], *complete[2:]],
        ):
            with self.subTest(case_count=len(changed)):
                with self.assertRaises(campaign.CampaignError):
                    evaluate(value, changed)

    def test_rejects_manifest_entry_tool_and_identity_drift(self) -> None:
        value = manifest()
        result = observations(value)
        mutations = (
            lambda items: items[0].__setitem__("seed", items[0]["seed"] + 1),
            lambda items: items[0]["tool_identities"][0].__setitem__(
                "sha256", "f" * 64
            ),
            lambda items: items[0].__setitem__("mutant_class", "transport"),
            lambda items: items[0].__setitem__("scenario_sha256", "f" * 64),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(result)
                mutate(changed)
                with self.assertRaises(campaign.CampaignError):
                    evaluate(value, changed)

        entry = accepted_entry(value)
        entry["review_state"] = "controller-accepted"
        with self.assertRaises(campaign.CampaignError):
            campaign.evaluate(value, value["manifest_sha256"], entry, result)
        with self.assertRaises(campaign.freeze_gate.GateError):
            campaign.evaluate(value, "f" * 64, accepted_entry(value), result)


if __name__ == "__main__":
    unittest.main()
