#!/usr/bin/env python3
"""Freeze the independent VC-WP-002A corpus without opening qualification data."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any, Mapping

SPEC_REVISION = "be347355d3747b849b0521e40c539aae88d33614"
OWNER = "independent-ai-verification"
SHA256 = re.compile(r"^[0-9a-f]{64}$")
GIT_SHA = re.compile(r"^[0-9a-f]{40}$")
FORBIDDEN_KEYS = frozenset(
    {
        "calibration_parameters",
        "derived_parameters",
        "parameter_derivation",
        "tuning",
        "tuning_details",
        "held_out_result",
        "held_out_results",
        "observed_result",
        "observed_results",
        "qualification_result",
        "verdict",
    }
)
INVALIDATION_RULES = (
    "model_identity_changed",
    "firmware_revision_or_image_changed",
    "task_topology_priority_or_affinity_changed",
    "transport_lifecycle_or_peer_protocol_changed",
    "timing_source_changed",
    "esp_idf_or_qemu_version_changed",
    "board_profile_or_relevant_hardware_changed",
    "calibrated_environment_changed",
    "corpus_or_held_out_scenarios_changed",
    "metrics_bounds_or_thresholds_changed",
)

MUTATION_CORPUS = (
    ("scheduler.priority-inversion", "scheduler", True),
    ("scheduler.affinity-drift", "scheduler", True),
    ("scheduler.missed-wakeup", "scheduler", True),
    ("transport.completion-loss", "transport", True),
    ("transport.duplicate-completion", "transport", False),
    ("transport.queue-saturation", "transport", True),
    ("stale-event.peer-generation", "stale-event", True),
    ("stale-event.late-callback", "stale-event", False),
    ("stale-event.reordered-timeout", "stale-event", True),
    ("recovery.restart-during-flight", "recovery", True),
    ("recovery.retry-budget-reset", "recovery", False),
    ("recovery.partial-peer-reset", "recovery", True),
    ("concurrency.callback-ring-race", "concurrency", True),
    ("concurrency.queue-owner-race", "concurrency", True),
    ("concurrency.timeout-completion-race", "concurrency", False),
)

METRICS = (
    {"id": "outcome", "bound": "exact-match"},
    {"id": "ordering", "bound": "zero-unexplained-inversions"},
    {"id": "timeout", "bound": "absolute-error-us<=250"},
    {"id": "queue-depth", "bound": "absolute-error<=1"},
    {"id": "latency", "bound": "p95-absolute-error-us<=500"},
)


class GateError(ValueError):
    """A public freeze interface failed closed."""


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(value: Any) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest()


def _exact(value: Mapping[str, Any], fields: set[str], where: str) -> None:
    if not isinstance(value, Mapping):
        raise GateError(f"{where} must be an object")
    actual = set(value)
    if actual != fields:
        raise GateError(
            f"{where} fields differ: missing={sorted(fields-actual)} extra={sorted(actual-fields)}"
        )


def _sha(value: Any, where: str, *, git: bool = False) -> str:
    pattern = GIT_SHA if git else SHA256
    if not isinstance(value, str) or not pattern.fullmatch(value):
        raise GateError(
            f"{where} must be an immutable lowercase {'Git' if git else 'SHA-256'} identity"
        )
    return value


def _reject_sensitive(value: Any, where: str = "input") -> None:
    if isinstance(value, dict):
        rejected = FORBIDDEN_KEYS.intersection(value)
        if rejected:
            raise GateError(
                f"{where} exposes forbidden sensitive fields: {sorted(rejected)}"
            )
        for key, child in value.items():
            _reject_sensitive(child, f"{where}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _reject_sensitive(child, f"{where}[{index}]")


def _identity(record: Mapping[str, Any], where: str) -> None:
    _exact(record, {"id", "sha256"}, where)
    if not isinstance(record["id"], str) or not record["id"].strip():
        raise GateError(f"{where}.id is required")
    _sha(record["sha256"], f"{where}.sha256")


def _dataset(record: Mapping[str, Any], where: str) -> None:
    _exact(
        record,
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
        where,
    )
    _sha(record["sha256"], f"{where}.sha256")
    _sha(record["raw_trace_sha256"], f"{where}.raw_trace_sha256")
    _sha(record["normalized_trace_sha256"], f"{where}.normalized_trace_sha256")
    if not isinstance(record["id"], str) or not record["id"]:
        raise GateError(f"{where}.id is required")
    if not isinstance(record["scenario"], str) or not record["scenario"]:
        raise GateError(f"{where}.scenario is required")
    if (
        not isinstance(record["seed"], int)
        or isinstance(record["seed"], bool)
        or record["seed"] < 0
    ):
        raise GateError(f"{where}.seed must be a non-negative integer")
    _sha(record["scenario_sha256"], f"{where}.scenario_sha256")
    _sha(record["seed_sha256"], f"{where}.seed_sha256")
    if record["scenario_sha256"] != digest(record["scenario"]):
        raise GateError(f"{where}.scenario_sha256 does not bind the scenario")
    if record["seed_sha256"] != digest(record["seed"]):
        raise GateError(f"{where}.seed_sha256 does not bind the seed")


def validate_public_input(value: Mapping[str, Any]) -> None:
    _reject_sensitive(value)
    _exact(
        value,
        {
            "schema_version",
            "kind",
            "specification_revision",
            "creation_phase",
            "terminal_candidate",
            "candidate",
            "datasets",
            "clock_correlation",
            "tools",
            "invalidation_rules",
        },
        "input",
    )
    if (
        value["schema_version"] != 1
        or value["kind"] != "vc-prequalification-public-freeze-interface"
    ):
        raise GateError("unsupported public freeze interface")
    if (
        value["specification_revision"] != SPEC_REVISION
        or value["creation_phase"] != "pre-held-out"
    ):
        raise GateError("stale specification or held-out access phase")

    terminal = value["terminal_candidate"]
    _exact(
        terminal,
        {
            "issue",
            "artifact_class",
            "commit",
            "evidence_sha256",
            "candidate_sha256",
            "fs_wp_002g",
        },
        "terminal_candidate",
    )
    if not isinstance(terminal["issue"], int) or terminal["issue"] in (159, 160, 165):
        raise GateError(
            "terminal candidate must name a materialized FS-WP-002H execution child"
        )
    if terminal["artifact_class"] != "terminal-executed-validation":
        raise GateError("intermediate or planning artifacts cannot open the gate")
    _sha(terminal["commit"], "terminal_candidate.commit", git=True)
    _sha(terminal["evidence_sha256"], "terminal_candidate.evidence_sha256")
    _sha(terminal["candidate_sha256"], "terminal_candidate.candidate_sha256")
    lineage = terminal["fs_wp_002g"]
    _exact(
        lineage,
        {"issue", "commit", "evidence_sha256", "campaign_sha256"},
        "terminal_candidate.fs_wp_002g",
    )
    if not isinstance(lineage["issue"], int):
        raise GateError("FS-WP-002G issue identity is required")
    _sha(lineage["commit"], "terminal_candidate.fs_wp_002g.commit", git=True)
    _sha(lineage["evidence_sha256"], "terminal_candidate.fs_wp_002g.evidence_sha256")
    _sha(lineage["campaign_sha256"], "terminal_candidate.fs_wp_002g.campaign_sha256")

    candidate = value["candidate"]
    _exact(
        candidate,
        {
            "model_sha256",
            "firmware_revision",
            "firmware_image_sha256",
            "hardware",
            "configuration",
            "prediction_envelope",
            "exclusions",
            "fs_wp_002g_campaign_sha256",
        },
        "candidate",
    )
    for key in ("model_sha256", "firmware_image_sha256", "fs_wp_002g_campaign_sha256"):
        _sha(candidate[key], f"candidate.{key}")
    _sha(candidate["firmware_revision"], "candidate.firmware_revision", git=True)
    for key in ("hardware", "configuration", "prediction_envelope"):
        _identity(candidate[key], f"candidate.{key}")
    if digest(candidate) != terminal["candidate_sha256"]:
        raise GateError(
            "terminal evidence is stale for the complete candidate identity"
        )
    if candidate["fs_wp_002g_campaign_sha256"] != lineage["campaign_sha256"]:
        raise GateError("candidate does not preserve exact FS-WP-002G campaign lineage")
    exclusions = candidate["exclusions"]
    if (
        not isinstance(exclusions, list)
        or any(not isinstance(item, str) or not item for item in exclusions)
        or len(exclusions) != len(set(exclusions))
    ):
        raise GateError("exclusions must be declared, unique strings")

    datasets = value["datasets"]
    _exact(datasets, {"calibration", "held_out"}, "datasets")
    for group in ("calibration", "held_out"):
        records = datasets[group]
        if not isinstance(records, list) or not records:
            raise GateError(f"datasets.{group} must not be empty")
        for index, record in enumerate(records):
            if not isinstance(record, dict):
                raise GateError(f"datasets.{group}[{index}] must be an object")
            _dataset(record, f"datasets.{group}[{index}]")
        identities = [(item["id"], item["sha256"]) for item in records]
        if len(identities) != len(set(identities)) or len(
            {item[0] for item in identities}
        ) != len(identities):
            raise GateError(f"datasets.{group} contains duplicate identities")
    calibration = {(item["id"], item["sha256"]) for item in datasets["calibration"]}
    held_out = {(item["id"], item["sha256"]) for item in datasets["held_out"]}
    if calibration & held_out or {x[0] for x in calibration} & {x[0] for x in held_out}:
        raise GateError("calibration and held-out dataset identities overlap")

    clock = value["clock_correlation"]
    _exact(clock, {"method", "method_sha256", "uncertainty_ns"}, "clock_correlation")
    if not isinstance(clock["method"], str) or not clock["method"]:
        raise GateError("clock correlation method is required")
    _sha(clock["method_sha256"], "clock_correlation.method_sha256")
    if clock["method_sha256"] != digest(clock["method"]):
        raise GateError("clock correlation digest does not bind the method")
    if (
        not isinstance(clock["uncertainty_ns"], int)
        or isinstance(clock["uncertainty_ns"], bool)
        or clock["uncertainty_ns"] < 0
    ):
        raise GateError("clock uncertainty must be a non-negative integer")
    if not isinstance(value["tools"], list) or not value["tools"]:
        raise GateError("tool identities are required")
    for index, tool in enumerate(value["tools"]):
        _identity(tool, f"tools[{index}]")
    tool_ids = [(item["id"], item["sha256"]) for item in value["tools"]]
    if len(tool_ids) != len(set(tool_ids)) or len(
        {item[0] for item in tool_ids}
    ) != len(tool_ids):
        raise GateError("tool identities must be unique")
    if tuple(value["invalidation_rules"]) != INVALIDATION_RULES:
        raise GateError("invalidation rules are incomplete or changed")


def freeze(public_input: Mapping[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    validate_public_input(public_input)
    terminal = public_input["terminal_candidate"]
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "kind": "vc-wp-002a-corpus-threshold-freeze",
        "creation_phase": "pre-held-out",
        "specification_revision": SPEC_REVISION,
        "owner": OWNER,
        "task": {
            "issue": 165,
            "parent_issue": 160,
            "dependency_issue": 159,
            "work_class": "executed-validation",
            "allowed_surface": "tools/simulation/**",
            "stop_condition": "software-review-required-human-review",
        },
        "entry": {"result": "passed", "terminal_candidate": terminal},
        "input_allowlist": sorted(public_input),
        "input_allowlist_sha256": digest(sorted(public_input)),
        "public_input_sha256": digest(public_input),
        "candidate": public_input["candidate"],
        "datasets": public_input["datasets"],
        "clock_correlation": public_input["clock_correlation"],
        "tools": public_input["tools"],
        "mutation_corpus": [
            {
                "id": item[0],
                "class": item[1],
                "critical_seeded_fault": item[2],
                "suppressed": False,
            }
            for item in MUTATION_CORPUS
        ],
        "held_out_scenarios": [
            {
                "id": item["scenario"],
                "scenario_sha256": item["scenario_sha256"],
                "seed": item["seed"],
                "seed_sha256": item["seed_sha256"],
                "dataset_id": item["id"],
                "dataset_sha256": item["sha256"],
                "raw_trace_sha256": item["raw_trace_sha256"],
                "normalized_trace_sha256": item["normalized_trace_sha256"],
            }
            for item in public_input["datasets"]["held_out"]
        ],
        "metrics": [dict(item) for item in METRICS],
        "thresholds": {
            "critical_detection_percent": 100,
            "complete_corpus_detection_percent_min": 95,
        },
        "prediction_envelope": public_input["candidate"]["prediction_envelope"],
        "exclusions": public_input["candidate"]["exclusions"],
        "invalidation_rules": list(INVALIDATION_RULES),
    }
    manifest["manifest_sha256"] = digest(manifest)
    report = {
        "schema_version": 1,
        "kind": "vc-wp-002a-prequalification-report",
        "entry_result": "passed",
        "creation_phase": "pre-held-out",
        "manifest_sha256": manifest["manifest_sha256"],
        "terminal_fs_wp_002h": {
            "issue": terminal["issue"],
            "commit": terminal["commit"],
            "evidence_sha256": terminal["evidence_sha256"],
        },
        "inherited_fs_wp_002g": terminal["fs_wp_002g"],
        "sensitive_fields_seen": [],
        "held_out_results_accessed": False,
        "stop_condition": "software-review-required-human-review",
    }
    return manifest, report


def verify_manifest(manifest: Mapping[str, Any]) -> None:
    supplied = manifest.get("manifest_sha256")
    if not isinstance(supplied, str):
        raise GateError("manifest digest is missing")
    body = dict(manifest)
    del body["manifest_sha256"]
    if digest(body) != supplied:
        raise GateError("post-freeze manifest edit detected")
    corpus = body.get("mutation_corpus")
    expected = [item[0] for item in MUTATION_CORPUS]
    if not isinstance(corpus, list) or [item.get("id") for item in corpus] != expected:
        raise GateError(
            "mutation corpus is incomplete, reordered, duplicated, or changed"
        )
    if any(item.get("suppressed") is not False for item in corpus):
        raise GateError("suppressed mutants are forbidden")
    if body.get("metrics") != [dict(item) for item in METRICS] or body.get(
        "thresholds"
    ) != {
        "critical_detection_percent": 100,
        "complete_corpus_detection_percent_min": 95,
    }:
        raise GateError("post-freeze metric or threshold change detected")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        public_input = json.loads(args.input.read_text(encoding="utf-8"))
        manifest, report = freeze(public_input)
        verify_manifest(manifest)
    except (OSError, json.JSONDecodeError, GateError, TypeError, KeyError) as error:
        print(f"prequalification gate: REJECTED: {error}")
        return 2
    args.manifest.write_bytes(canonical_bytes(manifest))
    args.report.write_bytes(canonical_bytes(report))
    print(f"prequalification gate: FROZEN {manifest['manifest_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
