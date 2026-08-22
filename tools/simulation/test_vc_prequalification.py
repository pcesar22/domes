#!/usr/bin/env python3
"""Focused positive and adversarial tests for VC-WP-002A prequalification."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

MODULE = Path(__file__).with_name("vc_prequalification.py")
SPEC = importlib.util.spec_from_file_location("vc_prequalification", MODULE)
assert SPEC and SPEC.loader
gate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = gate
SPEC.loader.exec_module(gate)
FIXTURE = MODULE.with_name("qualification") / "public-freeze-interface.fixture.json"
SCHEMA = MODULE.with_name("qualification") / "qualification-manifest.schema.json"
OPERATIONAL_REPORT = MODULE.with_name("qualification") / "operational-entry-report.json"


def fixture() -> dict:
    return json.loads(FIXTURE.read_text(encoding="utf-8"))


ATTESTATION_SHA256 = gate.digest(fixture()["controller_attestation"])


def freeze(value: dict | None = None) -> tuple[dict, dict]:
    return gate.freeze(value if value is not None else fixture(), ATTESTATION_SHA256)


class PrequalificationTests(unittest.TestCase):
    def assertRejected(self, value: dict) -> None:  # noqa: N802 - unittest idiom
        with self.assertRaises(gate.GateError):
            freeze(value)

    def test_positive_freeze_is_canonical_complete_and_pre_result(self) -> None:
        manifest, report = freeze()
        gate.verify_manifest(manifest, manifest["manifest_sha256"])
        self.assertEqual(
            manifest["manifest_sha256"],
            gate.digest({k: v for k, v in manifest.items() if k != "manifest_sha256"}),
        )
        self.assertEqual(
            {item["class"] for item in manifest["mutation_corpus"]},
            {"scheduler", "transport", "stale-event", "recovery", "concurrency"},
        )
        self.assertTrue(
            all(item["suppressed"] is False for item in manifest["mutation_corpus"])
        )
        self.assertTrue(
            any(item["critical_seeded_fault"] for item in manifest["mutation_corpus"])
        )
        self.assertEqual(
            [item["id"] for item in manifest["metrics"]],
            ["outcome", "ordering", "timeout", "queue-depth", "latency"],
        )
        self.assertEqual(
            manifest["thresholds"],
            {
                "critical_detection_percent": 100,
                "complete_corpus_detection_percent_min": 95,
            },
        )
        self.assertFalse(report["held_out_results_accessed"])
        self.assertEqual(report["sensitive_fields_seen"], [])

    def test_operational_entry_stays_fail_closed_without_terminal_evidence(
        self,
    ) -> None:
        report = json.loads(OPERATIONAL_REPORT.read_text(encoding="utf-8"))
        self.assertEqual(report["entry_result"], "rejected")
        self.assertFalse(report["manifest_created"])
        self.assertFalse(report["held_out_results_accessed"])
        self.assertIsNone(report["terminal_fs_wp_002h"]["terminal_commit"])
        self.assertIsNone(report["terminal_fs_wp_002g"]["campaign_sha256"])

    def test_rejects_missing_terminal_child_and_issue_closure_only(self) -> None:
        missing = fixture()
        del missing["terminal_candidate"]
        self.assertRejected(missing)
        closure = fixture()
        closure["terminal_candidate"]["issue"] = 159
        closure["terminal_candidate"]["artifact_class"] = "issue-closed"
        self.assertRejected(closure)
        relabeled_closure = fixture()
        relabeled_closure["terminal_candidate"]["issue"] = 159
        self.assertRejected(relabeled_closure)
        closure_class_only = fixture()
        closure_class_only["terminal_candidate"]["artifact_class"] = "issue-closed"
        self.assertRejected(closure_class_only)

    def test_rejects_unpinned_or_fabricated_controller_topology(self) -> None:
        value = fixture()
        with self.assertRaisesRegex(gate.GateError, "externally pinned"):
            gate.freeze(value, "f" * 64)
        value["controller_attestation"]["terminal_child_issue"] = 9002
        with self.assertRaises(gate.GateError):
            gate.freeze(value, gate.digest(value["controller_attestation"]))

    def test_rejects_intermediate_artifact_and_missing_fs2g_lineage(self) -> None:
        intermediate = fixture()
        intermediate["terminal_candidate"]["artifact_class"] = "intermediate"
        self.assertRejected(intermediate)
        no_lineage = fixture()
        del no_lineage["terminal_candidate"]["fs_wp_002g"]
        self.assertRejected(no_lineage)

    def test_all_exact_identities_are_mandatory_and_hashed(self) -> None:
        paths = (
            ("candidate", "model_sha256"),
            ("candidate", "firmware_revision"),
            ("candidate", "firmware_image_sha256"),
            ("candidate", "hardware"),
            ("candidate", "configuration"),
            ("datasets", "calibration"),
            ("datasets", "held_out"),
            ("clock_correlation", "method_sha256"),
            ("clock_correlation", "uncertainty_ns"),
            ("tools",),
        )
        for path in paths:
            with self.subTest(path=path):
                value = fixture()
                parent = value
                for key in path[:-1]:
                    parent = parent[key]
                del parent[path[-1]]
                self.assertRejected(value)
        for key in (
            "sha256",
            "scenario_sha256",
            "seed_sha256",
            "raw_trace_sha256",
            "normalized_trace_sha256",
        ):
            with self.subTest(dataset_identity=key):
                value = fixture()
                value["datasets"]["held_out"][0][key] = "main"
                self.assertRejected(value)

    def test_published_schema_requires_every_identity_family(self) -> None:
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(set(schema["required"]), set(schema["properties"]))
        dataset = schema["$defs"]["dataset"]
        self.assertFalse(dataset["additionalProperties"])
        self.assertEqual(
            set(dataset["required"]),
            {
                "id",
                "sha256",
                "scenario",
                "scenario_sha256",
                "seed",
                "seed_sha256",
                "raw_trace_sha256",
                "normalized_trace_sha256",
            },
        )
        for definition in (
            "identity",
            "dataset",
            "fs2g",
            "terminalCandidate",
            "candidate",
            "clock",
            "task",
            "entry",
            "predictionEnvelope",
            "mutationDefinition",
            "corpusConstruction",
            "mutant",
            "heldOutScenario",
            "metric",
        ):
            self.assertFalse(schema["$defs"][definition]["additionalProperties"])

    def test_rejects_calibration_held_out_overlap(self) -> None:
        value = fixture()
        value["datasets"]["held_out"][0] = copy.deepcopy(
            value["datasets"]["calibration"][0]
        )
        self.assertRejected(value)

    def test_rejects_relabeled_calibration_dataset_digest_reuse(self) -> None:
        value = fixture()
        calibration = value["datasets"]["calibration"][0]
        held_out = value["datasets"]["held_out"][0]
        self.assertNotEqual(calibration["id"], held_out["id"])
        held_out["sha256"] = calibration["sha256"]
        self.assertRejected(value)

    def test_rejects_relabeled_calibration_trace_identity_reuse(self) -> None:
        for identity in ("raw_trace_sha256", "normalized_trace_sha256"):
            with self.subTest(identity=identity):
                value = fixture()
                calibration = value["datasets"]["calibration"][0]
                held_out = value["datasets"]["held_out"][0]
                self.assertNotEqual(calibration["id"], held_out["id"])
                held_out[identity] = calibration[identity]
                self.assertRejected(value)

    def test_rejects_relabeled_calibration_scenario_seed_reuse(self) -> None:
        value = fixture()
        calibration = value["datasets"]["calibration"][0]
        held_out = value["datasets"]["held_out"][0]
        self.assertNotEqual(calibration["id"], held_out["id"])
        for identity in ("scenario", "scenario_sha256", "seed", "seed_sha256"):
            held_out[identity] = calibration[identity]
        self.assertRejected(value)

    def test_rejects_tuning_details_and_held_out_results_at_any_depth(self) -> None:
        for field in (
            "tuning_details",
            "calibration_parameters",
            "held_out_results",
            "observed_result",
        ):
            with self.subTest(field=field):
                value = fixture()
                value["candidate"][field] = {"secret": True}
                self.assertRejected(value)

    def test_rejects_stale_changed_and_mutable_identities(self) -> None:
        stale = fixture()
        stale["candidate"]["model_sha256"] = "f" * 64
        self.assertRejected(stale)
        changed_lineage = fixture()
        changed_lineage["candidate"]["fs_wp_002g_campaign_sha256"] = "f" * 64
        self.assertRejected(changed_lineage)
        mutable = fixture()
        mutable["candidate"]["firmware_revision"] = "main"
        self.assertRejected(mutable)
        mutable_tool = fixture()
        mutable_tool["tools"][0]["sha256"] = "latest"
        self.assertRejected(mutable_tool)

    def test_rejects_undeclared_or_duplicate_exclusions(self) -> None:
        missing = fixture()
        del missing["candidate"]["exclusions"]
        self.assertRejected(missing)
        duplicate = fixture()
        duplicate["candidate"]["exclusions"].append(
            duplicate["candidate"]["exclusions"][0]
        )
        self.assertRejected(duplicate)

    def test_rejects_duplicate_suppressed_or_incomplete_mutants(self) -> None:
        manifest, _ = freeze()
        expected_sha256 = manifest["manifest_sha256"]
        duplicate = copy.deepcopy(manifest)
        duplicate["mutation_corpus"][1]["id"] = duplicate["mutation_corpus"][0]["id"]
        duplicate["manifest_sha256"] = gate.digest(
            {k: v for k, v in duplicate.items() if k != "manifest_sha256"}
        )
        with self.assertRaises(gate.GateError):
            gate.verify_manifest(duplicate, expected_sha256)
        suppressed = copy.deepcopy(manifest)
        suppressed["mutation_corpus"][0]["suppressed"] = True
        suppressed["manifest_sha256"] = gate.digest(
            {k: v for k, v in suppressed.items() if k != "manifest_sha256"}
        )
        with self.assertRaises(gate.GateError):
            gate.verify_manifest(suppressed, expected_sha256)
        incomplete = copy.deepcopy(manifest)
        incomplete["mutation_corpus"].pop()
        incomplete["manifest_sha256"] = gate.digest(
            {k: v for k, v in incomplete.items() if k != "manifest_sha256"}
        )
        with self.assertRaises(gate.GateError):
            gate.verify_manifest(incomplete, expected_sha256)

    def test_rejects_post_freeze_edits_even_if_structurally_valid(self) -> None:
        manifest, _ = freeze()
        expected_sha256 = manifest["manifest_sha256"]
        manifest["thresholds"]["complete_corpus_detection_percent_min"] = 96
        with self.assertRaises(gate.GateError):
            gate.verify_manifest(manifest, expected_sha256)

    def test_external_pin_rejects_recomputed_digest_edits_across_manifest(self) -> None:
        original, _ = freeze()
        expected_sha256 = original["manifest_sha256"]
        mutations = (
            lambda value: value["mutation_corpus"][0].__setitem__(
                "critical_seeded_fault", False
            ),
            lambda value: value["candidate"].__setitem__("model_sha256", "f" * 64),
            lambda value: value["datasets"]["calibration"].pop(),
            lambda value: value["held_out_scenarios"].pop(),
            lambda value: value["entry"].__setitem__("result", "rejected"),
            lambda value: value["invalidation_rules"].pop(),
            lambda value: value.pop("candidate"),
            lambda value: value.pop("entry"),
            lambda value: value.pop("held_out_scenarios"),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                changed = copy.deepcopy(original)
                mutate(changed)
                changed["manifest_sha256"] = gate.digest(
                    {k: v for k, v in changed.items() if k != "manifest_sha256"}
                )
                with self.assertRaises(gate.GateError):
                    gate.verify_manifest(changed, expected_sha256)

    def test_schema_is_enforced_during_verification(self) -> None:
        manifest, _ = freeze()
        expected_sha256 = manifest["manifest_sha256"]
        manifest["mutation_corpus"][0]["undeclared"] = True
        with self.assertRaisesRegex(gate.GateError, "schema violation"):
            gate.verify_manifest(manifest, expected_sha256)
        manifest, _ = freeze()
        expected_sha256 = manifest["manifest_sha256"]
        manifest["metrics"][0]["bound"] = "relaxed"
        manifest["manifest_sha256"] = gate.digest(
            {k: v for k, v in manifest.items() if k != "manifest_sha256"}
        )
        with self.assertRaisesRegex(gate.GateError, "externally pinned"):
            gate.verify_manifest(manifest, expected_sha256)

    def test_rejects_changed_invalidation_rules_and_extra_input_fields(self) -> None:
        value = fixture()
        value["invalidation_rules"].pop()
        self.assertRejected(value)
        value = fixture()
        value["branch"] = "main"
        self.assertRejected(value)


if __name__ == "__main__":
    unittest.main()
