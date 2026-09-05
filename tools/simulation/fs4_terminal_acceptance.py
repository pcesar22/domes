#!/usr/bin/env python3
"""Validate and reproduce the terminal FS4 software-acceptance verdict.

The tool deliberately validates retained, software-only evidence.  It never opens a
device, adds a label, or turns missing physical evidence into a passing verdict.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

SPECIFICATION_REVISION = "0f1659c6a32288fa3478969586e54a81599c4453"
REPO_ROOT = Path(__file__).resolve().parents[2]
REQUIRED_ISSUES = (154, 155, 174, 175, 176)
REQUIRED_TARGET_COUNT = 6
REQUIRED_COVERAGE = {
    "orchestration": (174,),
    "mobile_control": (154, 155, 174),
    "diagnostics": (175,),
    "lifecycle_recovery": (178, 183, 184),
    "fault_handling": (178, 183, 184),
    "bounded_soak": (179,),
    "prequalification": (176,),
}
REQUIRED_STAGES = (
    "prepare_mode",
    "prepare_clear",
    "arm_target",
    "hit_clear",
    "miss_feedback",
    "miss_clear",
    "cleanup_clear",
    "cleanup_mode",
    "command_failure",
    "stream_failure",
    "disconnect",
    "reconnect",
)
REQUIRED_COUNTERS = {
    "stale_mutations",
    "leaked_subscriptions",
    "quarantined_generation_reuse",
    "lost_results",
    "duplicate_results",
    "cleanup_order_violations",
    "unhealthy_peer_mutations",
    "unexplained_runtime_divergences",
}
EXCLUDED_ISSUES = (116, 143, *range(161, 172))
SHA256_RE = re.compile(r"[0-9a-f]{64}")
GIT_SHA_RE = re.compile(r"[0-9a-f]{40}")


class AcceptanceError(RuntimeError):
    """The evidence is insufficient for terminal software acceptance."""


def canonical(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _git(*args: str) -> str:
    result = subprocess.run(
        ("git", *args), cwd=REPO_ROOT, text=True, capture_output=True, check=False
    )
    if result.returncode:
        raise AcceptanceError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def _require_keys(value: Any, keys: set[str], label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict) or set(value) != keys:
        raise AcceptanceError(f"{label} is malformed or has missing/unexpected fields")
    return value


def _require_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or SHA256_RE.fullmatch(value) is None:
        raise AcceptanceError(f"{label} is not a SHA-256 identity")
    return value


def _read_bound_file(root: Path, record: Any, label: str) -> bytes:
    item = _require_keys(record, {"path", "sha256"}, label)
    relative = item["path"]
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise AcceptanceError(f"{label} path is invalid")
    path = (root / relative).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError as error:
        raise AcceptanceError(f"{label} escapes its input root") from error
    try:
        content = path.read_bytes()
    except OSError as error:
        raise AcceptanceError(f"{label} is missing: {relative}") from error
    if sha256(content) != _require_sha256(item["sha256"], f"{label}.sha256"):
        raise AcceptanceError(f"{label} hash mismatch")
    return content


def _read_bound_tool(root: Path, record: Any, label: str) -> dict[str, str]:
    item = _require_keys(record, {"path", "version", "sha256"}, label)
    if not isinstance(item["version"], str) or not item["version"]:
        raise AcceptanceError(f"{label} version is incomplete")
    content = _read_bound_file(
        root,
        {"path": item["path"], "sha256": item["sha256"]},
        label,
    )
    if not content:
        raise AcceptanceError(f"{label} is empty")
    return dict(item)


def _validate_version_identities(records: Any) -> dict[str, dict[str, str]]:
    if not isinstance(records, dict) or set(records) != {
        "python",
        "flutter",
        "dart",
        "rust",
    }:
        raise AcceptanceError("execution toolchain identity is incomplete")
    validated = {}
    for name, raw in records.items():
        record = _require_keys(raw, {"version", "sha256"}, f"execution tool {name}")
        version = record["version"]
        if not isinstance(version, str) or not version:
            raise AcceptanceError(f"execution tool {name} version is incomplete")
        expected = sha256(version.encode())
        if (
            _require_sha256(record["sha256"], f"execution tool {name}.sha256")
            != expected
        ):
            raise AcceptanceError(f"execution tool {name} version hash mismatch")
        validated[name] = dict(record)
    return validated


def _rotating_totals(items: Sequence[str], cycles: int) -> dict[str, int]:
    return {
        item: sum(1 for index in range(cycles) if items[index % len(items)] == item)
        for item in items
    }


def _validate_source_is_integrated(source_sha: Any, tested_sha: str, label: str) -> str:
    if not isinstance(source_sha, str) or GIT_SHA_RE.fullmatch(source_sha) is None:
        raise AcceptanceError(f"{label} source Git SHA is malformed")
    result = subprocess.run(
        ("git", "merge-base", "--is-ancestor", source_sha, tested_sha),
        cwd=REPO_ROOT,
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        raise AcceptanceError(f"{label} source revision is not integrated")
    return source_sha


def _validate_prerequisites(
    root: Path, records: Any, tested_sha: str
) -> list[dict[str, Any]]:
    if (
        not isinstance(records, list)
        or len(records) != len(REQUIRED_ISSUES)
        or any(not isinstance(record, dict) for record in records)
        or [record.get("issue") for record in records] != list(REQUIRED_ISSUES)
    ):
        raise AcceptanceError("prerequisites are missing, duplicated, or reordered")
    validated = []
    for expected_issue, raw in zip(REQUIRED_ISSUES, records, strict=True):
        record = _require_keys(
            raw,
            {
                "issue",
                "tracker_url",
                "source_git_sha",
                "status",
                "tool",
                "lockfiles",
                "artifacts",
            },
            f"prerequisite #{expected_issue}",
        )
        if record["issue"] != expected_issue or record["status"] != "accepted":
            raise AcceptanceError(
                f"prerequisite #{expected_issue} is unavailable or not accepted"
            )
        if (
            record["tracker_url"]
            != f"https://github.com/pcesar22/domes/issues/{expected_issue}"
        ):
            raise AcceptanceError(
                f"prerequisite #{expected_issue} has a foreign tracker identity"
            )
        source_sha = _validate_source_is_integrated(
            record["source_git_sha"], tested_sha, f"prerequisite #{expected_issue}"
        )
        tool = _read_bound_tool(
            root, record["tool"], f"prerequisite #{expected_issue} tool"
        )
        if not isinstance(record["lockfiles"], list) or not record["lockfiles"]:
            raise AcceptanceError(
                f"prerequisite #{expected_issue} lockfile inventory is empty"
            )
        lockfiles = []
        for index, item in enumerate(record["lockfiles"]):
            _read_bound_file(
                root, item, f"prerequisite #{expected_issue} lockfile {index}"
            )
            lockfiles.append(item)
        if not isinstance(record["artifacts"], list) or not record["artifacts"]:
            raise AcceptanceError(
                f"prerequisite #{expected_issue} artifact inventory is empty"
            )
        lockfile_sha256s = [item["sha256"] for item in lockfiles]
        artifacts = []
        for index, item in enumerate(record["artifacts"]):
            content = _read_bound_file(
                root, item, f"prerequisite #{expected_issue} artifact {index}"
            )
            try:
                parsed = json.loads(content)
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                raise AcceptanceError(
                    f"prerequisite #{expected_issue} artifact {index} is malformed"
                ) from error
            if (
                not isinstance(parsed, dict)
                or parsed.get("tested_git_sha") != source_sha
            ):
                raise AcceptanceError(
                    f"prerequisite #{expected_issue} artifact {index} belongs to another source"
                )
            if parsed.get("tool_sha256") != tool["sha256"]:
                raise AcceptanceError(
                    f"prerequisite #{expected_issue} artifact {index} belongs to another toolchain"
                )
            if parsed.get("lockfile_sha256s") != lockfile_sha256s:
                raise AcceptanceError(
                    f"prerequisite #{expected_issue} artifact {index} has foreign lockfile identities"
                )
            if parsed.get("result") != "accepted":
                raise AcceptanceError(
                    f"prerequisite #{expected_issue} artifact {index} is not accepted"
                )
            artifacts.append(item)
        validated.append(dict(record, lockfiles=lockfiles, artifacts=artifacts))
    return validated


def _validate_coverage(records: Any) -> list[dict[str, Any]]:
    if (
        not isinstance(records, list)
        or len(records) != len(REQUIRED_COVERAGE)
        or any(not isinstance(record, dict) for record in records)
        or [record.get("area") for record in records] != list(REQUIRED_COVERAGE)
    ):
        raise AcceptanceError("coverage map is missing, duplicated, or reordered")
    validated = []
    for raw, (area, owners) in zip(records, REQUIRED_COVERAGE.items(), strict=True):
        record = _require_keys(
            raw, {"area", "tracker_issues", "implementation_paths"}, f"coverage {area}"
        )
        tracker_issues = record["tracker_issues"]
        if (
            record["area"] != area
            or not isinstance(tracker_issues, list)
            or tuple(tracker_issues) != owners
        ):
            raise AcceptanceError(f"coverage {area} has a foreign owner")
        paths = record["implementation_paths"]
        if (
            not isinstance(paths, list)
            or not paths
            or any(not isinstance(path, str) or not path for path in paths)
        ):
            raise AcceptanceError(f"coverage {area} has no implementation path")
        validated.append(dict(record))
    return validated


def _validate_execution(
    root: Path, raw: Any, targets: list[str], tested_sha: str
) -> dict[str, Any]:
    execution = _require_keys(
        raw,
        {
            "tested_git_sha",
            "tool_versions",
            "lockfiles",
            "raw_logs",
            "targets",
            "stages",
            "cycles",
            "per_target",
            "per_stage",
            "invariant_counters",
            "terminal_states",
            "software_result",
            "simulation_result",
        },
        "execution",
    )
    if execution["tested_git_sha"] != tested_sha:
        raise AcceptanceError("execution belongs to another Git revision")
    execution = dict(execution)
    execution["tool_versions"] = _validate_version_identities(
        execution["tool_versions"]
    )
    for field in ("lockfiles", "raw_logs"):
        values = execution[field]
        if not isinstance(values, list) or not values:
            raise AcceptanceError(f"execution {field} inventory is empty")
        for index, record in enumerate(values):
            _read_bound_file(root, record, f"execution {field} {index}")
    if execution["targets"] != targets:
        raise AcceptanceError("execution target inventory changed")
    if tuple(execution["stages"]) != REQUIRED_STAGES:
        raise AcceptanceError(
            "lifecycle or fault-stage coverage is incomplete or reordered"
        )
    cycles = execution["cycles"]
    if not isinstance(cycles, int) or isinstance(cycles, bool) or cycles < 1000:
        raise AcceptanceError("six-identity recovery soak has fewer than 1,000 cycles")
    if execution["per_target"] != _rotating_totals(targets, cycles):
        raise AcceptanceError(
            "per-target counters show lost, duplicate, or stale execution"
        )
    if execution["per_stage"] != _rotating_totals(REQUIRED_STAGES, cycles):
        raise AcceptanceError(
            "per-stage counters show incomplete or divergent execution"
        )
    counters = execution["invariant_counters"]
    if not isinstance(counters, dict) or set(counters) != REQUIRED_COUNTERS:
        raise AcceptanceError("invariant counter inventory is incomplete")
    if any(
        not isinstance(value, int) or isinstance(value, bool) or value != 0
        for value in counters.values()
    ):
        raise AcceptanceError("one or more execution invariants are nonzero")
    states = execution["terminal_states"]
    if states != {target: "disconnected" for target in targets}:
        raise AcceptanceError("terminal states do not match the lifecycle contract")
    if (
        execution["software_result"] != "passed"
        or execution["simulation_result"] != "passed"
    ):
        raise AcceptanceError("software and simulation evidence did not both pass")
    return execution


def build_verdict(
    input_path: Path, *, expected_git_sha: str | None = None
) -> dict[str, Any]:
    root = input_path.resolve().parent
    try:
        document = json.loads(input_path.read_bytes())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AcceptanceError(
            f"input manifest is unavailable or malformed: {error}"
        ) from error
    manifest = _require_keys(
        document,
        {
            "schema_version",
            "specification_revision",
            "tested_git_sha",
            "targets",
            "prerequisites",
            "coverage",
            "duplication_audit",
            "execution",
        },
        "input manifest",
    )
    if (
        manifest["schema_version"] != 1
        or manifest["specification_revision"] != SPECIFICATION_REVISION
    ):
        raise AcceptanceError("input manifest has a foreign specification revision")
    tested_sha = expected_git_sha or _git("rev-parse", "HEAD")
    if (
        manifest["tested_git_sha"] != tested_sha
        or GIT_SHA_RE.fullmatch(tested_sha) is None
    ):
        raise AcceptanceError("input manifest belongs to another tested Git revision")
    targets = manifest["targets"]
    if (
        not isinstance(targets, list)
        or len(targets) != REQUIRED_TARGET_COUNT
        or len(set(targets)) != REQUIRED_TARGET_COUNT
        or any(not isinstance(target, str) or not target for target in targets)
    ):
        raise AcceptanceError("exactly six unique target identities are required")
    prerequisites = _validate_prerequisites(root, manifest["prerequisites"], tested_sha)
    coverage = _validate_coverage(manifest["coverage"])
    audit = _require_keys(
        manifest["duplication_audit"],
        {"consumed", "excluded", "blocked_issue_116"},
        "duplication audit",
    )
    if (
        audit["consumed"] != [154, 155, 174, 175, 176]
        or audit["excluded"] != list(EXCLUDED_ISSUES[1:])
        or audit["blocked_issue_116"] != "not_resumed_or_replaced"
    ):
        raise AcceptanceError(
            "duplication audit is incomplete or changes excluded ownership"
        )
    execution = _validate_execution(root, manifest["execution"], targets, tested_sha)
    inputs_digest = sha256(canonical(manifest))
    verdict = {
        "schema_version": 1,
        "claim": "terminal_fs4_software_and_simulation_acceptance",
        "result": "passed",
        "specification_revision": SPECIFICATION_REVISION,
        "tested_git_sha": tested_sha,
        "input_manifest_sha256": inputs_digest,
        "targets": targets,
        "coverage": {
            "records": coverage,
            "uncovered_software_behavior": [],
        },
        "prerequisites": prerequisites,
        "duplication_audit": audit,
        "execution": execution,
        "claim_boundaries": {
            "software": "passed",
            "simulation": "passed",
            "physical_validation": "unverified",
            "additional_alpha_nodes_unavailable": 4,
            "claims_not_made": [
                "physical_six_node_timing",
                "synchronized_timing",
                "radio_frequency_behavior",
                "peripheral_actuation",
                "hardware_equivalence",
                "physical_fault_recovery_or_soak",
                "predictive_trust",
            ],
        },
    }
    verdict["canonical_verdict_sha256"] = sha256(canonical(verdict))
    return verdict


def verify_verdict(
    input_path: Path, verdict_path: Path, *, expected_git_sha: str | None = None
) -> dict[str, Any]:
    expected = build_verdict(input_path, expected_git_sha=expected_git_sha)
    try:
        retained = json.loads(verdict_path.read_bytes())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AcceptanceError(
            f"retained verdict is unavailable or malformed: {error}"
        ) from error
    if retained != expected or canonical(retained) != canonical(expected):
        raise AcceptanceError("retained verdict is non-reproducible")
    return expected


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="immutable input manifest")
    parser.add_argument("--output", type=Path, help="write canonical verdict")
    parser.add_argument(
        "--verify", type=Path, help="reproduce and compare a retained verdict"
    )
    parser.add_argument(
        "--expected-git-sha", help="controller-pinned exact tested commit"
    )
    args = parser.parse_args(argv)
    try:
        if args.verify:
            verdict = verify_verdict(
                args.input, args.verify, expected_git_sha=args.expected_git_sha
            )
        else:
            verdict = build_verdict(args.input, expected_git_sha=args.expected_git_sha)
        encoded = canonical(verdict)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(encoded)
        sys.stdout.buffer.write(encoded)
        return 0
    except AcceptanceError as error:
        print(f"FS4 terminal acceptance stopped: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
