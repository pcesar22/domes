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
SCHEMA_PATH = (
    Path(__file__).with_name("qualification") / "qualification-manifest.schema.json"
)
TERMINAL_FS2H_ISSUE = 171
TERMINAL_FS2G_ISSUE = 164
TERMINAL_TASK_UID = "1211337a1d026c6f4bfb9f964887904ccce4af07d22e429f844ae17d94d707ee"
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

MUTATION_DEFINITIONS = (
    (
        "scheduler.priority-inversion",
        "scheduler",
        True,
        "invert-ready-priority",
        "scheduler.ready-queue",
    ),
    (
        "scheduler.affinity-drift",
        "scheduler",
        True,
        "move-task-affinity",
        "scheduler.dispatch",
    ),
    (
        "scheduler.missed-wakeup",
        "scheduler",
        True,
        "drop-wakeup",
        "scheduler.notification",
    ),
    (
        "transport.completion-loss",
        "transport",
        True,
        "drop-completion",
        "transport.completion",
    ),
    (
        "transport.duplicate-completion",
        "transport",
        False,
        "duplicate-completion",
        "transport.completion",
    ),
    (
        "transport.queue-saturation",
        "transport",
        True,
        "force-capacity",
        "transport.queue",
    ),
    (
        "stale-event.peer-generation",
        "stale-event",
        True,
        "reuse-peer-generation",
        "peer.lifecycle",
    ),
    (
        "stale-event.late-callback",
        "stale-event",
        False,
        "delay-callback",
        "transport.callback",
    ),
    (
        "stale-event.reordered-timeout",
        "stale-event",
        True,
        "reorder-timeout",
        "timer.delivery",
    ),
    (
        "recovery.restart-during-flight",
        "recovery",
        True,
        "restart-owner",
        "recovery.in-flight",
    ),
    (
        "recovery.retry-budget-reset",
        "recovery",
        False,
        "reset-retry-budget",
        "recovery.retry",
    ),
    (
        "recovery.partial-peer-reset",
        "recovery",
        True,
        "reset-one-peer-side",
        "peer.recovery",
    ),
    (
        "concurrency.callback-ring-race",
        "concurrency",
        True,
        "interleave-ring-callback",
        "callback.ring",
    ),
    (
        "concurrency.queue-owner-race",
        "concurrency",
        True,
        "interleave-queue-owner",
        "transport.queue-owner",
    ),
    (
        "concurrency.timeout-completion-race",
        "concurrency",
        False,
        "co-schedule-timeout-completion",
        "timer.completion",
    ),
)
REQUIRED_MUTATION_CLASSES = (
    "scheduler",
    "transport",
    "stale-event",
    "recovery",
    "concurrency",
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


def _prediction_envelope(record: Mapping[str, Any], where: str) -> None:
    _exact(record, {"id", "dimensions", "sha256"}, where)
    if not isinstance(record["id"], str) or not record["id"]:
        raise GateError(f"{where}.id is required")
    dimensions = record["dimensions"]
    if (
        not isinstance(dimensions, list)
        or tuple(dimensions) != REQUIRED_MUTATION_CLASSES
    ):
        raise GateError(
            f"{where}.dimensions do not declare the complete corpus envelope"
        )
    _sha(record["sha256"], f"{where}.sha256")
    if record["sha256"] != digest({"id": record["id"], "dimensions": dimensions}):
        raise GateError(f"{where}.sha256 does not bind the declared envelope")


def _controller_attestation(record: Mapping[str, Any], expected_sha256: str) -> None:
    _sha(expected_sha256, "expected controller attestation digest")
    if digest(record) != expected_sha256:
        raise GateError("controller topology attestation is not externally pinned")
    _exact(
        record,
        {
            "kind",
            "specification_revision",
            "task_issue",
            "parent_issue",
            "planning_issue",
            "terminal_child_issue",
            "terminal_task_key",
            "terminal_task_uid",
            "terminal_work_class",
            "terminal_state",
            "terminal_commit",
            "terminal_evidence_sha256",
            "terminal_candidate_sha256",
            "fs_wp_002g_terminal_issue",
            "fs_wp_002g_commit",
            "fs_wp_002g_evidence_sha256",
            "fs_wp_002g_campaign_sha256",
        },
        "controller_attestation",
    )
    expected = {
        "kind": "domes-controller-terminal-child-attestation-v1",
        "specification_revision": SPEC_REVISION,
        "task_issue": 165,
        "parent_issue": 160,
        "planning_issue": 159,
        "terminal_child_issue": TERMINAL_FS2H_ISSUE,
        "terminal_task_key": "fs2h-candidate-acceptance",
        "terminal_task_uid": TERMINAL_TASK_UID,
        "terminal_work_class": "executed-validation",
        "terminal_state": "accepted",
        "fs_wp_002g_terminal_issue": TERMINAL_FS2G_ISSUE,
    }
    if any(record[key] != value for key, value in expected.items()):
        raise GateError(
            "controller attestation does not prove the required terminal topology"
        )
    _sha(record["terminal_commit"], "controller_attestation.terminal_commit", git=True)
    _sha(
        record["terminal_evidence_sha256"],
        "controller_attestation.terminal_evidence_sha256",
    )
    _sha(
        record["terminal_candidate_sha256"],
        "controller_attestation.terminal_candidate_sha256",
    )
    _sha(
        record["fs_wp_002g_commit"],
        "controller_attestation.fs_wp_002g_commit",
        git=True,
    )
    _sha(
        record["fs_wp_002g_evidence_sha256"],
        "controller_attestation.fs_wp_002g_evidence_sha256",
    )
    _sha(
        record["fs_wp_002g_campaign_sha256"],
        "controller_attestation.fs_wp_002g_campaign_sha256",
    )


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


def validate_public_input(
    value: Mapping[str, Any], expected_attestation_sha256: str
) -> None:
    _reject_sensitive(value)
    _exact(
        value,
        {
            "schema_version",
            "kind",
            "specification_revision",
            "creation_phase",
            "controller_attestation",
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

    _controller_attestation(
        value["controller_attestation"], expected_attestation_sha256
    )

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
    if terminal["issue"] != TERMINAL_FS2H_ISSUE:
        raise GateError(
            "terminal candidate does not match the controller-materialized child"
        )
    if terminal["artifact_class"] != "terminal-executed-validation":
        raise GateError("intermediate or planning artifacts cannot open the gate")
    _sha(terminal["commit"], "terminal_candidate.commit", git=True)
    _sha(terminal["evidence_sha256"], "terminal_candidate.evidence_sha256")
    _sha(terminal["candidate_sha256"], "terminal_candidate.candidate_sha256")
    attestation = value["controller_attestation"]
    if (
        terminal["commit"] != attestation["terminal_commit"]
        or terminal["evidence_sha256"] != attestation["terminal_evidence_sha256"]
        or terminal["candidate_sha256"] != attestation["terminal_candidate_sha256"]
    ):
        raise GateError(
            "terminal candidate identity differs from controller attestation"
        )
    lineage = terminal["fs_wp_002g"]
    _exact(
        lineage,
        {"issue", "commit", "evidence_sha256", "campaign_sha256"},
        "terminal_candidate.fs_wp_002g",
    )
    if lineage["issue"] != TERMINAL_FS2G_ISSUE:
        raise GateError("FS-WP-002G lineage does not name its terminal campaign child")
    _sha(lineage["commit"], "terminal_candidate.fs_wp_002g.commit", git=True)
    _sha(lineage["evidence_sha256"], "terminal_candidate.fs_wp_002g.evidence_sha256")
    _sha(lineage["campaign_sha256"], "terminal_candidate.fs_wp_002g.campaign_sha256")
    if (
        lineage["commit"] != attestation["fs_wp_002g_commit"]
        or lineage["evidence_sha256"] != attestation["fs_wp_002g_evidence_sha256"]
        or lineage["campaign_sha256"] != attestation["fs_wp_002g_campaign_sha256"]
    ):
        raise GateError("FS-WP-002G lineage differs from controller attestation")

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
    for key in ("hardware", "configuration"):
        _identity(candidate[key], f"candidate.{key}")
    _prediction_envelope(
        candidate["prediction_envelope"], "candidate.prediction_envelope"
    )
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
    identity_namespaces = {
        "dataset id": lambda item: item["id"],
        "dataset SHA-256": lambda item: item["sha256"],
        "raw trace SHA-256": lambda item: item["raw_trace_sha256"],
        "normalized trace SHA-256": lambda item: item["normalized_trace_sha256"],
        "scenario/seed": lambda item: (item["scenario"], item["seed"]),
    }
    for namespace, identity in identity_namespaces.items():
        calibration = {identity(item) for item in datasets["calibration"]}
        held_out = {identity(item) for item in datasets["held_out"]}
        if calibration & held_out:
            raise GateError(f"calibration and held-out {namespace} identities overlap")

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
    if "vc-prequalification-v1" not in {item["id"] for item in value["tools"]}:
        raise GateError("the corpus construction tool identity is required")
    if tuple(value["invalidation_rules"]) != INVALIDATION_RULES:
        raise GateError("invalidation rules are incomplete or changed")


def _mutation_corpus(public_input: Mapping[str, Any]) -> list[dict[str, Any]]:
    candidate = public_input["candidate"]
    tool_sha256 = next(
        tool["sha256"]
        for tool in public_input["tools"]
        if tool["id"] == "vc-prequalification-v1"
    )
    corpus = []
    for (
        mutant_id,
        mutation_class,
        critical,
        operator,
        injection_point,
    ) in MUTATION_DEFINITIONS:
        definition = {
            "operator": operator,
            "injection_point": injection_point,
            "activation": "single-seeded-fault",
            "expected_observation": "candidate-prediction-versus-mutated-trace",
        }
        binding = {
            "definition_sha256": digest(definition),
            "model_sha256": candidate["model_sha256"],
            "prediction_envelope_sha256": candidate["prediction_envelope"]["sha256"],
            "tool_sha256": tool_sha256,
        }
        corpus.append(
            {
                "id": mutant_id,
                "class": mutation_class,
                "critical_seeded_fault": critical,
                "suppressed": False,
                "definition": definition,
                "definition_sha256": binding["definition_sha256"],
                "execution_contract_sha256": digest(binding),
            }
        )
    return corpus


def freeze(
    public_input: Mapping[str, Any], expected_attestation_sha256: str
) -> tuple[dict[str, Any], dict[str, Any]]:
    validate_public_input(public_input, expected_attestation_sha256)
    terminal = public_input["terminal_candidate"]
    corpus = _mutation_corpus(public_input)
    corpus_construction = {
        "method": "independent-static-envelope-enumeration-v1",
        "prediction_envelope_sha256": public_input["candidate"]["prediction_envelope"][
            "sha256"
        ],
        "required_classes": list(REQUIRED_MUTATION_CLASSES),
        "mutant_definition_sha256s": [item["definition_sha256"] for item in corpus],
    }
    corpus_construction["completeness_sha256"] = digest(corpus_construction)
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
        "controller_attestation_sha256": expected_attestation_sha256,
        "input_allowlist": sorted(public_input),
        "input_allowlist_sha256": digest(sorted(public_input)),
        "public_input_sha256": digest(public_input),
        "candidate": public_input["candidate"],
        "datasets": public_input["datasets"],
        "clock_correlation": public_input["clock_correlation"],
        "tools": public_input["tools"],
        "corpus_construction": corpus_construction,
        "mutation_corpus": corpus,
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
        "controller_attestation_sha256": expected_attestation_sha256,
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


def _schema_error(path: str, message: str) -> None:
    raise GateError(f"manifest schema violation at {path}: {message}")


def _validate_schema(
    value: Any,
    schema: Mapping[str, Any],
    root: Mapping[str, Any],
    path: str = "$",
) -> None:
    if "$ref" in schema:
        ref = schema["$ref"]
        if not isinstance(ref, str) or not ref.startswith("#/"):
            _schema_error(path, "only local schema references are supported")
        target: Any = root
        for part in ref[2:].split("/"):
            target = target[part.replace("~1", "/").replace("~0", "~")]
        _validate_schema(value, target, root, path)
        return
    if "const" in schema and value != schema["const"]:
        _schema_error(path, f"must equal {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        _schema_error(path, "is outside the declared enum")
    expected_type = schema.get("type")
    type_ok = {
        "object": isinstance(value, Mapping),
        "array": isinstance(value, list),
        "string": isinstance(value, str),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "boolean": isinstance(value, bool),
        None: True,
    }.get(expected_type, False)
    if not type_ok:
        _schema_error(path, f"must have type {expected_type}")
    if expected_type == "object":
        required = set(schema.get("required", []))
        actual = set(value)
        missing = required - actual
        if missing:
            _schema_error(path, f"missing required fields {sorted(missing)}")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            extra = actual - set(properties)
            if extra:
                _schema_error(path, f"contains undeclared fields {sorted(extra)}")
        for key, child_schema in properties.items():
            if key in value:
                _validate_schema(value[key], child_schema, root, f"{path}.{key}")
    elif expected_type == "array":
        if len(value) < schema.get("minItems", 0):
            _schema_error(path, "has too few items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            _schema_error(path, "has too many items")
        if schema.get("uniqueItems") and len(
            {canonical_bytes(item) for item in value}
        ) != len(value):
            _schema_error(path, "contains duplicate items")
        if "items" in schema:
            for index, item in enumerate(value):
                _validate_schema(item, schema["items"], root, f"{path}[{index}]")
    elif expected_type == "string":
        if len(value) < schema.get("minLength", 0):
            _schema_error(path, "is too short")
        if "pattern" in schema and re.fullmatch(schema["pattern"], value) is None:
            _schema_error(path, "does not match the required pattern")
    elif expected_type == "integer" and value < schema.get("minimum", value):
        _schema_error(path, "is below the minimum")


def validate_manifest_schema(
    manifest: Mapping[str, Any], schema_path: Path = SCHEMA_PATH
) -> None:
    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GateError(
            f"qualification manifest schema is unavailable: {error}"
        ) from error
    _validate_schema(manifest, schema, schema)


def verify_manifest(manifest: Mapping[str, Any], expected_manifest_sha256: str) -> None:
    _sha(expected_manifest_sha256, "externally pinned manifest digest")
    validate_manifest_schema(manifest)
    supplied = manifest.get("manifest_sha256")
    if supplied != expected_manifest_sha256:
        raise GateError("manifest does not match the externally pinned digest")
    body = dict(manifest)
    del body["manifest_sha256"]
    if digest(body) != supplied:
        raise GateError("post-freeze manifest edit detected")
    corpus = body.get("mutation_corpus")
    expected = [item[0] for item in MUTATION_DEFINITIONS]
    if not isinstance(corpus, list) or [item.get("id") for item in corpus] != expected:
        raise GateError(
            "mutation corpus is incomplete, reordered, duplicated, or changed"
        )
    if any(item.get("suppressed") is not False for item in corpus):
        raise GateError("suppressed mutants are forbidden")
    for item, definition in zip(corpus, MUTATION_DEFINITIONS):
        if (item.get("class"), item.get("critical_seeded_fault")) != definition[1:3]:
            raise GateError("mutation classification or criticality changed")
        if item.get("definition_sha256") != digest(item.get("definition")):
            raise GateError("mutation definition identity changed")
        tool_sha256 = next(
            tool["sha256"]
            for tool in body["tools"]
            if tool["id"] == "vc-prequalification-v1"
        )
        binding = {
            "definition_sha256": item["definition_sha256"],
            "model_sha256": body["candidate"]["model_sha256"],
            "prediction_envelope_sha256": body["prediction_envelope"]["sha256"],
            "tool_sha256": tool_sha256,
        }
        if item.get("execution_contract_sha256") != digest(binding):
            raise GateError("mutation execution contract is stale")
    construction = dict(body["corpus_construction"])
    completeness_sha256 = construction.pop("completeness_sha256")
    if completeness_sha256 != digest(construction):
        raise GateError("corpus completeness identity changed")
    if construction != {
        "method": "independent-static-envelope-enumeration-v1",
        "prediction_envelope_sha256": body["prediction_envelope"]["sha256"],
        "required_classes": list(REQUIRED_MUTATION_CLASSES),
        "mutant_definition_sha256s": [item["definition_sha256"] for item in corpus],
    }:
        raise GateError("mutation corpus does not cover the complete frozen envelope")
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
    parser.add_argument("--controller-attestation-sha256", required=True)
    args = parser.parse_args()
    try:
        public_input = json.loads(args.input.read_text(encoding="utf-8"))
        manifest, report = freeze(public_input, args.controller_attestation_sha256)
        verify_manifest(manifest, manifest["manifest_sha256"])
    except (OSError, json.JSONDecodeError, GateError, TypeError, KeyError) as error:
        print(f"prequalification gate: REJECTED: {error}")
        return 2
    args.manifest.write_bytes(canonical_bytes(manifest))
    args.report.write_bytes(canonical_bytes(report))
    print(f"prequalification gate: FROZEN {manifest['manifest_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
