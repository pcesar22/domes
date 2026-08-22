#!/usr/bin/env python3
"""Run and validate the deterministic FS4 command-recovery soak campaign."""

from __future__ import annotations

import argparse
import functools
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

SPECIFICATION_REVISION = "be347355d3747b849b0521e40c539aae88d33614"
PREDECESSOR_REVISION = "7d2a8466e3f96fa96a820a82589fba5c7de014f0"
REPO_ROOT = Path(__file__).resolve().parents[2]
SCENARIO = (
    REPO_ROOT
    / "ios/domes_app/test/application/providers/fs4_command_recovery_soak_test.dart"
)
APP_ROOT = REPO_ROOT / "ios/domes_app"
FLUTTER_LOCKFILE = APP_ROOT / "pubspec.lock"
IDENTITIES = tuple(f"soak-pod-{number}" for number in range(1, 7))
STAGES = (
    "prepare_mode",
    "prepare_clear",
    "arm_target",
    "hit_clear",
    "miss_feedback",
    "miss_clear",
    "cleanup_clear",
    "cleanup_mode",
)
SUMMARY_PREFIX = "FS4_COMMAND_RECOVERY_SOAK "
SUMMARY_FIELDS = {
    "schema_version",
    "scenario",
    "identities",
    "stages",
    "cycles",
    "faults",
    "reconnects",
    "completed_results",
    "per_identity",
    "per_stage",
    "terminal_state",
    "invariant_counters",
}
COUNTER_FIELDS = {
    "duplicate_or_lost_results",
    "stale_mutations",
    "duplicate_failure_events",
    "cleanup_order_violations",
    "leaked_subscriptions",
    "healthy_peer_mutations",
    "quarantined_generation_reuse",
}
MANIFEST_FIELDS = {
    "schema_version",
    "campaign",
    "specification_revision",
    "tested_git_sha",
    "predecessor_git_sha",
    "tool_versions",
    "flutter_lockfile",
    "invocation",
    "scenario",
    "counts",
    "invariant_counters",
    "terminal_state",
    "artifact_hashes",
    "canonical_verdict_digest",
    "physical_validation",
    "claim_boundary",
    "ownership_and_gaps",
    "predecessor_reconciliation",
}


class SoakError(RuntimeError):
    """Fail-closed campaign or artifact validation error."""


def _run(command: Sequence[str], *, cwd: Path = REPO_ROOT, timeout: int = 900) -> str:
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise SoakError(f"cannot execute {shlex.join(command)}: {error}") from error
    if result.returncode:
        raise SoakError(
            f"command exited {result.returncode}: {shlex.join(command)}\n{result.stdout}"
        )
    return result.stdout


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def _git(*args: str) -> str:
    return _run(("git", *args)).strip()


@functools.lru_cache(maxsize=1)
def _tool_version_tuple() -> tuple[str, str, str]:
    try:
        flutter = json.loads(_run(("flutter", "--version", "--machine")))
    except (json.JSONDecodeError, SoakError) as error:
        raise SoakError(f"cannot identify the Flutter toolchain: {error}") from error
    dart = _run(("dart", "--version")).strip()
    return (
        str(flutter.get("frameworkVersion", "")),
        dart.removeprefix("Dart SDK version: "),
        sys.version.split()[0],
    )


def _tool_versions() -> dict[str, str]:
    flutter, dart, python = _tool_version_tuple()
    return {"flutter": flutter, "dart": dart, "python": python}


def _reconcile_predecessors() -> list[dict[str, Any]]:
    expected = {
        174: "deterministic",
        175: "diagnostic",
        176: "prequalification",
    }
    records = []
    for number, scope_word in expected.items():
        raw = _run(
            ("gh", "issue", "view", str(number), "--json", "number,state,title,url")
        )
        issue = json.loads(raw)
        if (
            not isinstance(issue, dict)
            or issue.get("number") != number
            or scope_word not in str(issue.get("title", "")).lower()
        ):
            raise SoakError(f"issue {number} identity or scope changed")
        records.append(
            {
                "issue": number,
                "state": issue.get("state"),
                "title": issue.get("title"),
                "url": issue.get("url"),
                "artifacts_consumed": False,
            }
        )
    return records


def _expected_totals(items: Sequence[str], cycles: int) -> dict[str, int]:
    return {
        item: sum(1 for index in range(cycles) if items[index % len(items)] == item)
        for item in items
    }


def parse_summary(log_text: str, cycles: int) -> dict[str, Any]:
    matches = [
        line.split(SUMMARY_PREFIX, 1)[1]
        for line in log_text.splitlines()
        if SUMMARY_PREFIX in line
    ]
    if len(matches) != 1:
        raise SoakError(f"expected one complete scenario summary, found {len(matches)}")
    try:
        value = json.loads(matches[0])
    except json.JSONDecodeError as error:
        raise SoakError(f"malformed scenario summary: {error}") from error
    if not isinstance(value, dict) or set(value) != SUMMARY_FIELDS:
        raise SoakError("scenario summary has missing or unexpected fields")
    if value["schema_version"] != 1 or value["scenario"] != "fs4_command_recovery_soak":
        raise SoakError("scenario identity mismatch")
    if value["identities"] != list(IDENTITIES) or len(set(value["identities"])) != 6:
        raise SoakError("scenario identities are missing, reordered, or duplicated")
    if value["stages"] != list(STAGES) or len(set(value["stages"])) != len(STAGES):
        raise SoakError("fault-stage inventory mismatch")
    for field in ("cycles", "faults", "reconnects", "completed_results"):
        if value[field] != cycles:
            raise SoakError(f"scenario {field} count mismatch")
    if value["per_identity"] != _expected_totals(IDENTITIES, cycles):
        raise SoakError("per-identity count mismatch")
    if value["per_stage"] != _expected_totals(STAGES, cycles):
        raise SoakError("per-stage count mismatch")
    counters = value["invariant_counters"]
    if not isinstance(counters, dict) or set(counters) != COUNTER_FIELDS:
        raise SoakError("invariant counter inventory mismatch")
    if any(
        not isinstance(count, int) or isinstance(count, bool) or count != 0
        for count in counters.values()
    ):
        raise SoakError("one or more lifecycle invariants are nonzero")
    if value["terminal_state"] != "disconnected":
        raise SoakError("campaign did not reach the declared terminal state")
    return value


def _forbidden_claim(value: Any) -> bool:
    if isinstance(value, Mapping):
        for key, nested in value.items():
            lowered = str(key).lower()
            if "physical" in lowered or "predictive" in lowered:
                return True
            if _forbidden_claim(nested):
                return True
    elif isinstance(value, list):
        return any(_forbidden_claim(item) for item in value)
    elif isinstance(value, str):
        lowered = value.lower().replace(
            "without issuing a physical or predictive trust verdict", ""
        )
        if "predictive" in lowered:
            return True
        if "physical" in lowered:
            positive_verdict = re.search(
                r"\b(passed|verified|validated|complete|confirmed|established|successful)\b",
                lowered,
            )
            if "unverified" not in lowered or positive_verdict:
                return True
    return False


def _require_nonempty_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise SoakError(f"manifest {field} must be a nonempty string")
    return value


def validate_manifest(
    manifest: Mapping[str, Any],
    log_bytes: bytes,
    *,
    expected_git_sha: str | None = None,
) -> None:
    if set(manifest) != MANIFEST_FIELDS:
        raise SoakError("manifest has missing or unexpected fields")
    if (
        manifest.get("schema_version") != 1
        or manifest.get("campaign") != "fs4_command_recovery_soak"
    ):
        raise SoakError("manifest identity mismatch")
    if manifest.get("specification_revision") != SPECIFICATION_REVISION:
        raise SoakError("specification revision mismatch")
    tested_git_sha = str(manifest.get("tested_git_sha", ""))
    if not re.fullmatch(r"[0-9a-f]{40}", tested_git_sha):
        raise SoakError("tested Git SHA is invalid")
    validated_git_sha = expected_git_sha or _git("rev-parse", "HEAD")
    if not re.fullmatch(r"[0-9a-f]{40}", validated_git_sha):
        raise SoakError("expected tested Git SHA is invalid")
    if tested_git_sha != validated_git_sha:
        raise SoakError("tested Git SHA differs from the validated Git head")
    if manifest.get("predecessor_git_sha") != PREDECESSOR_REVISION:
        raise SoakError("predecessor revision mismatch")
    tools = manifest.get("tool_versions")
    if not isinstance(tools, dict) or set(tools) != {"flutter", "dart", "python"}:
        raise SoakError("manifest tool_versions inventory is incomplete")
    for name, version in tools.items():
        _require_nonempty_string(version, f"tool_versions.{name}")
    if tools != _tool_versions():
        raise SoakError("retained toolchain differs from the validating toolchain")
    lockfile = manifest.get("flutter_lockfile")
    expected_lockfile = {
        "path": str(FLUTTER_LOCKFILE.relative_to(REPO_ROOT)),
        "sha256": _sha256(FLUTTER_LOCKFILE.read_bytes()),
    }
    if lockfile != expected_lockfile:
        raise SoakError("Flutter lockfile identity mismatch")
    _require_nonempty_string(manifest.get("invocation"), "invocation")
    _require_nonempty_string(manifest.get("claim_boundary"), "claim_boundary")
    ownership = manifest.get("ownership_and_gaps")
    if not isinstance(ownership, dict) or set(ownership) != {
        "owned",
        "excluded",
        "unverified",
    }:
        raise SoakError("manifest ownership_and_gaps is incomplete")
    _require_nonempty_string(ownership.get("owned"), "ownership_and_gaps.owned")
    _require_nonempty_string(
        ownership.get("unverified"), "ownership_and_gaps.unverified"
    )
    excluded = ownership.get("excluded")
    if not isinstance(excluded, list) or len(excluded) != 3:
        raise SoakError("manifest ownership exclusions are incomplete")
    for index, item in enumerate(excluded):
        _require_nonempty_string(item, f"ownership_and_gaps.excluded[{index}]")
    reconciliation = manifest.get("predecessor_reconciliation")
    if not isinstance(reconciliation, list) or len(reconciliation) != 3:
        raise SoakError("manifest predecessor_reconciliation is incomplete")
    expected_issues = [174, 175, 176]
    for expected_issue, record in zip(expected_issues, reconciliation, strict=True):
        if not isinstance(record, dict) or set(record) != {
            "issue",
            "state",
            "title",
            "url",
            "artifacts_consumed",
        }:
            raise SoakError("manifest predecessor record is incomplete")
        if (
            record.get("issue") != expected_issue
            or record.get("artifacts_consumed") is not False
        ):
            raise SoakError("manifest predecessor identity or ownership mismatch")
        for field in ("state", "title", "url"):
            _require_nonempty_string(
                record.get(field), f"predecessor_reconciliation.{field}"
            )
    claim_fields = {
        key: value for key, value in manifest.items() if key != "physical_validation"
    }
    if manifest.get("physical_validation") != "unverified" or _forbidden_claim(
        claim_fields
    ):
        raise SoakError("manifest attempts a physical or predictive claim")
    artifacts = manifest.get("artifact_hashes")
    if artifacts != {"raw_flutter_log_sha256": _sha256(log_bytes)}:
        raise SoakError("retained artifact hash mismatch")
    scenario = manifest.get("scenario")
    if not isinstance(scenario, dict) or set(scenario) != {
        "path",
        "sha256",
        "inventory",
    }:
        raise SoakError("scenario definition is incomplete")
    if scenario["inventory"] != list(STAGES):
        raise SoakError("scenario inventory contains foreign scope")
    if scenario["path"] != str(SCENARIO.relative_to(REPO_ROOT)) or scenario[
        "sha256"
    ] != _sha256(SCENARIO.read_bytes()):
        raise SoakError("scenario definition digest mismatch")
    counts = manifest.get("counts")
    if not isinstance(counts, dict) or set(counts) != {
        "cycles",
        "faults",
        "reconnects",
        "completed_results",
        "per_identity",
        "per_stage",
    }:
        raise SoakError("manifest count inventory mismatch")
    summary = parse_summary(log_bytes.decode(), counts.get("cycles", -1))
    expected_counts = {key: summary[key] for key in counts}
    if (
        counts != expected_counts
        or manifest.get("invariant_counters") != summary["invariant_counters"]
    ):
        raise SoakError("manifest and raw log counters disagree")
    verdict = {
        "campaign": manifest["campaign"],
        "specification_revision": manifest["specification_revision"],
        "tested_git_sha": manifest["tested_git_sha"],
        "predecessor_git_sha": manifest["predecessor_git_sha"],
        "tool_versions": manifest["tool_versions"],
        "flutter_lockfile": manifest["flutter_lockfile"],
        "scenario": manifest["scenario"],
        "counts": manifest["counts"],
        "invariant_counters": manifest["invariant_counters"],
        "terminal_state": manifest["terminal_state"],
        "physical_validation": manifest["physical_validation"],
    }
    if manifest.get("canonical_verdict_digest") != _sha256(_canonical(verdict)):
        raise SoakError("canonical verdict digest mismatch")


def run(cycles: int, specification_revision: str, output: Path) -> Path:
    if cycles < 1000:
        raise SoakError("at least 1000 recovery cycles are required")
    if specification_revision != SPECIFICATION_REVISION:
        raise SoakError("specification revision mismatch")
    if not SCENARIO.is_file():
        raise SoakError(f"missing scenario: {SCENARIO.relative_to(REPO_ROOT)}")
    if output.exists() and any(output.iterdir()):
        raise SoakError(f"output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    tested_sha = _git("rev-parse", "HEAD")
    status_lines = _git(
        "status", "--porcelain=v1", "--untracked-files=all"
    ).splitlines()
    source_changes = [line for line in status_lines if not line.startswith("?? .tmp/")]
    if source_changes:
        raise SoakError("campaign must execute from a clean tested Git head")
    try:
        _run(("git", "merge-base", "--is-ancestor", PREDECESSOR_REVISION, tested_sha))
    except SoakError as error:
        raise SoakError(
            "tested head does not descend from the required predecessor"
        ) from error
    reconciliation = _reconcile_predecessors()
    flutter_command = (
        "flutter",
        "test",
        "--no-pub",
        f"--dart-define=FS4_SOAK_CYCLES={cycles}",
        str(SCENARIO.relative_to(APP_ROOT)),
    )
    log_text = _run(flutter_command, cwd=APP_ROOT, timeout=1800)
    log_bytes = log_text.encode()
    summary = parse_summary(log_text, cycles)
    scenario_bytes = SCENARIO.read_bytes()
    counts = {
        key: summary[key]
        for key in (
            "cycles",
            "faults",
            "reconnects",
            "completed_results",
            "per_identity",
            "per_stage",
        )
    }
    invocation = shlex.join(
        ("python3", str(Path(__file__).relative_to(REPO_ROOT)), *sys.argv[1:])
    )
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "campaign": "fs4_command_recovery_soak",
        "specification_revision": specification_revision,
        "tested_git_sha": tested_sha,
        "predecessor_git_sha": PREDECESSOR_REVISION,
        "tool_versions": _tool_versions(),
        "flutter_lockfile": {
            "path": str(FLUTTER_LOCKFILE.relative_to(REPO_ROOT)),
            "sha256": _sha256(FLUTTER_LOCKFILE.read_bytes()),
        },
        "invocation": invocation,
        "scenario": {
            "path": str(SCENARIO.relative_to(REPO_ROOT)),
            "sha256": _sha256(scenario_bytes),
            "inventory": list(STAGES),
        },
        "counts": counts,
        "invariant_counters": summary["invariant_counters"],
        "terminal_state": summary["terminal_state"],
        "artifact_hashes": {"raw_flutter_log_sha256": _sha256(log_bytes)},
        "physical_validation": "unverified",
        "claim_boundary": "deterministic software regression evidence only",
        "ownership_and_gaps": {
            "owned": "command-failure quarantine and explicit reconnect soak",
            "excluded": [
                "issues 125-126 merged behavior",
                "issues 174-176 qualification, diagnostics, and bundling",
                "ESP-NOW and QEMU scheduler fault campaigns",
            ],
            "unverified": "physical six-node fault, timing, soak, peripheral, RF, and recovery validation remains unverified",
        },
        "predecessor_reconciliation": reconciliation,
    }
    verdict = {
        key: manifest[key]
        for key in (
            "campaign",
            "specification_revision",
            "tested_git_sha",
            "predecessor_git_sha",
            "tool_versions",
            "flutter_lockfile",
            "scenario",
            "counts",
            "invariant_counters",
            "terminal_state",
            "physical_validation",
        )
    }
    manifest["canonical_verdict_digest"] = _sha256(_canonical(verdict))
    validate_manifest(manifest, log_bytes)
    (output / "flutter.log").write_bytes(log_bytes)
    manifest_path = output / "manifest.json"
    manifest_path.write_bytes(_canonical(manifest))
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cycles", required=True, type=int)
    parser.add_argument("--specification-revision", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        path = run(args.cycles, args.specification_revision, args.output)
    except (SoakError, OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"FS4 command-recovery soak BLOCKED: {error}", file=sys.stderr)
        return 2
    print(f"FS4 command-recovery soak PASS: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
