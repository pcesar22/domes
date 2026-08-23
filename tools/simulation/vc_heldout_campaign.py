#!/usr/bin/env python3
"""Evaluate the frozen VC-WP-002A held-out campaign without changing it."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
from pathlib import Path
from typing import Any, Mapping

import vc_prequalification as freeze_gate

SPEC_REVISION = freeze_gate.SPEC_REVISION
TASK = {
    "issue": 166,
    "parent_issue": 160,
    "dependency_issue": 165,
    "work_class": "executed-validation",
    "allowed_surface": "tools/simulation/**",
    "owner": "independent-qualification-campaign-executor",
    "entry_criteria": "exact-human-reviewed-corpus-freeze",
    "stop_condition": "software-review-required-human-review",
    "invalidation_rule": "any-frozen-identity-or-invalidation-trigger-change-invalidates-run",
}
METRIC_IDS = ("outcome", "ordering", "timeout", "queue-depth", "latency")
OBSERVATION_FIELDS = {
    "order",
    "mutant_id",
    "mutant_definition_sha256",
    "mutant_class",
    "critical_seeded_fault",
    "scenario",
    "scenario_sha256",
    "seed",
    "seed_sha256",
    "dataset_sha256",
    "raw_trace_sha256",
    "normalized_trace_sha256",
    "clock_correlation_uncertainty_ns",
    "tool_identities",
    "expected_outcome",
    "observed_outcome",
    "unexplained_ordering_inversions",
    "timeout_absolute_error_us",
    "queue_depth_absolute_error",
    "latency_p95_absolute_error_us",
    "termination_state",
    "divergence_explanation",
}


class CampaignError(ValueError):
    """The campaign cannot produce qualification evidence."""


def tool_identity() -> dict[str, str]:
    return {
        "id": "vc-heldout-campaign-v1",
        "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }


def execution_environment() -> dict[str, str]:
    return {
        "python_implementation": platform.python_implementation(),
        "python_version": platform.python_version(),
        "serialization": "canonical-json-sort-keys-utf8-lf-v1",
    }


def _exact(value: Mapping[str, Any], fields: set[str], where: str) -> None:
    if not isinstance(value, Mapping):
        raise CampaignError(f"{where} must be an object")
    if set(value) != fields:
        raise CampaignError(
            f"{where} fields differ: missing={sorted(fields-set(value))} "
            f"extra={sorted(set(value)-fields)}"
        )


def _integer(value: Any, where: str, *, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        raise CampaignError(f"{where} must be an integer >= {minimum}")
    return value


def _sha(value: Any, where: str) -> str:
    if not isinstance(value, str) or freeze_gate.SHA256.fullmatch(value) is None:
        raise CampaignError(f"{where} must be a lowercase SHA-256 identity")
    return value


def rejected_report(entry_report: Mapping[str, Any]) -> dict[str, Any]:
    reasons = entry_report.get("reason_codes", ["corpus-freeze-entry-not-passed"])
    if not isinstance(reasons, list) or not all(
        isinstance(item, str) and item for item in reasons
    ):
        reasons = ["corpus-freeze-entry-not-passed"]
    report: dict[str, Any] = {
        "schema_version": 1,
        "kind": "vc-wp-002a-held-out-campaign-report",
        "specification_revision": SPEC_REVISION,
        "task": TASK,
        "entry_result": "rejected",
        "execution_command": "python3 tools/simulation/vc_heldout_campaign.py --entry-report tools/simulation/qualification/operational-entry-report.json --report tools/simulation/qualification/heldout-campaign-report.json",
        "executor_tool": tool_identity(),
        "execution_environment": execution_environment(),
        "campaign_executed": False,
        "manifest_accessed": False,
        "held_out_results_accessed": False,
        "case_inventory": [],
        "aggregate": None,
        "reason_codes": reasons,
        "stop_condition": TASK["stop_condition"],
    }
    report["report_sha256"] = freeze_gate.digest(report)
    return report


def validate_entry(entry: Mapping[str, Any], manifest: Mapping[str, Any]) -> None:
    manifest_sha256 = manifest["manifest_sha256"]
    required = {
        "schema_version",
        "kind",
        "specification_revision",
        "entry_result",
        "manifest_created",
        "held_out_results_accessed",
        "manifest_sha256",
        "review_state",
        "reviewed_manifest_sha256",
        "terminal_fs_wp_002h",
        "terminal_fs_wp_002g",
        "stop_condition",
    }
    _exact(entry, required, "entry_report")
    expected = {
        "schema_version": 1,
        "kind": "vc-wp-002a-operational-entry-report",
        "specification_revision": SPEC_REVISION,
        "entry_result": "passed",
        "manifest_created": True,
        "held_out_results_accessed": False,
        "manifest_sha256": manifest_sha256,
        "review_state": "human-reviewed",
        "reviewed_manifest_sha256": manifest_sha256,
        "stop_condition": "software-review-required-human-review",
    }
    for key, expected_value in expected.items():
        if entry[key] != expected_value:
            raise CampaignError(f"entry_report.{key} is not the frozen reviewed value")
    terminal = manifest["entry"]["terminal_candidate"]
    expected_terminal = {
        "terminal_fs_wp_002h": {
            "commit": terminal["commit"],
            "evidence_sha256": terminal["evidence_sha256"],
        },
        "terminal_fs_wp_002g": {
            "commit": terminal["fs_wp_002g"]["commit"],
            "evidence_sha256": terminal["fs_wp_002g"]["evidence_sha256"],
            "campaign_sha256": terminal["fs_wp_002g"]["campaign_sha256"],
        },
    }
    for key, expected_record in expected_terminal.items():
        _exact(entry[key], set(expected_record), f"entry_report.{key}")
        if entry[key] != expected_record:
            raise CampaignError(f"entry_report.{key} differs from the frozen lineage")


def _expected_cases(
    manifest: Mapping[str, Any],
) -> list[tuple[dict[str, Any], dict[str, Any]]]:
    return [
        (mutant, scenario)
        for mutant in manifest["mutation_corpus"]
        for scenario in manifest["held_out_scenarios"]
    ]


def verify_observation_bundle(
    bundle: Mapping[str, Any],
    expected_bundle_sha256: str,
    manifest_sha256: str,
) -> list[Mapping[str, Any]]:
    _exact(
        bundle,
        {"schema_version", "kind", "manifest_sha256", "cases", "artifact_set_sha256"},
        "observation_bundle",
    )
    _sha(expected_bundle_sha256, "externally pinned observation bundle digest")
    expected = {
        "schema_version": 1,
        "kind": "vc-wp-002a-held-out-observation-bundle",
        "manifest_sha256": manifest_sha256,
        "cases": bundle["cases"],
    }
    if bundle["artifact_set_sha256"] != freeze_gate.digest(expected):
        raise CampaignError("observation bundle embedded digest is stale")
    if bundle["artifact_set_sha256"] != expected_bundle_sha256:
        raise CampaignError("observation bundle does not match its external digest pin")
    if not isinstance(bundle["cases"], list):
        raise CampaignError("observation_bundle.cases must be an array")
    return bundle["cases"]


def _metric_results(observation: Mapping[str, Any]) -> list[dict[str, Any]]:
    return [
        {
            "id": "outcome",
            "bound": "exact-match",
            "observed": observation["observed_outcome"],
            "passed": observation["observed_outcome"]
            == observation["expected_outcome"],
        },
        {
            "id": "ordering",
            "bound": "zero-unexplained-inversions",
            "observed": observation["unexplained_ordering_inversions"],
            "passed": observation["unexplained_ordering_inversions"] == 0,
        },
        {
            "id": "timeout",
            "bound": "absolute-error-us<=250",
            "observed": observation["timeout_absolute_error_us"],
            "passed": observation["timeout_absolute_error_us"] <= 250,
        },
        {
            "id": "queue-depth",
            "bound": "absolute-error<=1",
            "observed": observation["queue_depth_absolute_error"],
            "passed": observation["queue_depth_absolute_error"] <= 1,
        },
        {
            "id": "latency",
            "bound": "p95-absolute-error-us<=500",
            "observed": observation["latency_p95_absolute_error_us"],
            "passed": observation["latency_p95_absolute_error_us"] <= 500,
        },
    ]


def _validate_observation(
    observation: Mapping[str, Any],
    order: int,
    mutant: Mapping[str, Any],
    scenario: Mapping[str, Any],
    manifest: Mapping[str, Any],
) -> None:
    _exact(observation, OBSERVATION_FIELDS, f"observations[{order}]")
    frozen = {
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
        "clock_correlation_uncertainty_ns": manifest["clock_correlation"][
            "uncertainty_ns"
        ],
        "tool_identities": manifest["tools"],
    }
    for key, expected in frozen.items():
        if observation[key] != expected:
            raise CampaignError(
                f"observations[{order}].{key} drifted from the manifest"
            )
    for key in ("raw_trace_sha256", "normalized_trace_sha256"):
        _sha(observation[key], f"observations[{order}].{key}")
    for key in (
        "unexplained_ordering_inversions",
        "timeout_absolute_error_us",
        "queue_depth_absolute_error",
        "latency_p95_absolute_error_us",
    ):
        _integer(observation[key], f"observations[{order}].{key}")
    if (
        not isinstance(observation["expected_outcome"], str)
        or not observation["expected_outcome"]
    ):
        raise CampaignError(f"observations[{order}].expected_outcome is required")
    if (
        not isinstance(observation["observed_outcome"], str)
        or not observation["observed_outcome"]
    ):
        raise CampaignError(f"observations[{order}].observed_outcome is required")
    if observation["termination_state"] not in {
        "completed",
        "timeout",
        "queue-overflow",
        "infrastructure-error",
    }:
        raise CampaignError(f"observations[{order}].termination_state is invalid")
    explanation = observation["divergence_explanation"]
    if explanation is not None and (
        not isinstance(explanation, str) or not explanation
    ):
        raise CampaignError(f"observations[{order}].divergence_explanation is invalid")


def evaluate(
    manifest: Mapping[str, Any],
    expected_manifest_sha256: str,
    entry_report: Mapping[str, Any],
    observations: list[Mapping[str, Any]],
) -> dict[str, Any]:
    freeze_gate.verify_manifest(manifest, expected_manifest_sha256)
    validate_entry(entry_report, manifest)
    expected_cases = _expected_cases(manifest)
    if not isinstance(observations, list) or len(observations) != len(expected_cases):
        raise CampaignError(
            f"complete campaign requires {len(expected_cases)} cases exactly once"
        )

    inventory = []
    for order, ((mutant, scenario), observation) in enumerate(
        zip(expected_cases, observations, strict=True), start=1
    ):
        _validate_observation(observation, order, mutant, scenario, manifest)
        metrics = _metric_results(observation)
        infrastructure_error = (
            observation["termination_state"] == "infrastructure-error"
        )
        detected = not infrastructure_error and any(
            not item["passed"] for item in metrics
        )
        unexplained_divergence = (
            any(not item["passed"] for item in metrics)
            and not observation["divergence_explanation"]
        )
        inventory.append(
            {
                **dict(observation),
                "metric_evaluations": metrics,
                "detected": detected,
                "survived": not detected and not infrastructure_error,
                "infrastructure_error": infrastructure_error,
                "explanation_status": (
                    "unexplained"
                    if unexplained_divergence
                    else (
                        "explained"
                        if observation["divergence_explanation"]
                        else "not-required"
                    )
                ),
            }
        )

    critical = [item for item in inventory if item["critical_seeded_fault"]]
    critical_detected = sum(item["detected"] for item in critical)
    total_detected = sum(item["detected"] for item in inventory)
    critical_passed = critical_detected * 100 == len(critical) * 100
    corpus_passed = total_detected * 100 >= len(inventory) * 95
    metric_summary = []
    for metric_id in METRIC_IDS:
        values = [
            next(
                metric
                for metric in item["metric_evaluations"]
                if metric["id"] == metric_id
            )
            for item in inventory
        ]
        metric_summary.append(
            {
                "id": metric_id,
                "evaluated": len(values),
                "passed": sum(item["passed"] for item in values),
                "failed": sum(not item["passed"] for item in values),
            }
        )
    aggregate = {
        "case_count": len(inventory),
        "critical_detection": {
            "numerator": critical_detected,
            "denominator": len(critical),
            "required_percent": 100,
            "passed": critical_passed,
        },
        "complete_corpus_detection": {
            "numerator": total_detected,
            "denominator": len(inventory),
            "required_percent_min": 95,
            "passed": corpus_passed,
        },
        "metric_bounds": metric_summary,
        "survivors": [item["order"] for item in inventory if item["survived"]],
        "infrastructure_errors": [
            item["order"] for item in inventory if item["infrastructure_error"]
        ],
        "unexplained_divergences": [
            item["order"]
            for item in inventory
            if item["explanation_status"] == "unexplained"
        ],
        "thresholds_passed": critical_passed and corpus_passed,
    }
    report: dict[str, Any] = {
        "schema_version": 1,
        "kind": "vc-wp-002a-held-out-campaign-report",
        "specification_revision": SPEC_REVISION,
        "task": TASK,
        "entry_result": "passed",
        "executor_tool": tool_identity(),
        "execution_environment": execution_environment(),
        "campaign_executed": True,
        "manifest_accessed": True,
        "held_out_results_accessed": True,
        "manifest_sha256": expected_manifest_sha256,
        "tool_identities": manifest["tools"],
        "case_inventory": inventory,
        "aggregate": aggregate,
        "qualification_outcome": (
            "thresholds-passed"
            if aggregate["thresholds_passed"]
            else "thresholds-failed"
        ),
        "trust_verdict_issued": False,
        "stop_condition": TASK["stop_condition"],
    }
    report["report_sha256"] = freeze_gate.digest(report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--entry-report", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--manifest-sha256")
    parser.add_argument("--observations", type=Path)
    parser.add_argument("--observations-sha256")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        entry = json.loads(args.entry_report.read_text(encoding="utf-8"))
        if entry.get("entry_result") != "passed":
            report = rejected_report(entry)
        else:
            if (
                not args.manifest
                or not args.manifest_sha256
                or not args.observations
                or not args.observations_sha256
            ):
                raise CampaignError(
                    "accepted entry requires manifest and observation bundle external digest pins"
                )
            manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
            observation_bundle = json.loads(
                args.observations.read_text(encoding="utf-8")
            )
            observations = verify_observation_bundle(
                observation_bundle,
                args.observations_sha256,
                args.manifest_sha256,
            )
            report = evaluate(manifest, args.manifest_sha256, entry, observations)
        args.report.write_bytes(freeze_gate.canonical_bytes(report))
    except (
        OSError,
        json.JSONDecodeError,
        CampaignError,
        freeze_gate.GateError,
        KeyError,
        TypeError,
    ) as error:
        print(f"held-out campaign: INVALID: {error}")
        return 2
    print(
        f"held-out campaign: {report['entry_result'].upper()} "
        f"{report['report_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
