#!/usr/bin/env python3
"""Fail-closed FS-WP-002F entry verification.

The gate reads repository state and GitHub tracker state but never changes either.  A
report is always emitted when possible; exit status 0 means PASS and 2 means BLOCKED.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
import generate_runtime_profile as profile_generator
import qemu_feasibility as feasibility
import qemu_runtime as runtime

SPEC_REVISION = "498ae0203dc8b7048682fbff718a0629243a98a8"
REQUIRED_BASE_REVISION = "be347355d3747b849b0521e40c539aae88d33614"
REPOSITORY = "pcesar22/domes"
PR_NUMBERS = (105, 107, 115, 130)
ISSUE_NUMBERS = (101, 114, 123)
FS3_ISSUE = 114
TRACKER_ACTOR = "pcesar22"
DEPENDENCY_JUDGMENTS = {
    101: {
        "pull_request": 105,
        "spec_revision": "224c22931311e763db3ba304d780daa78552db41",
    },
    123: {
        "pull_request": 130,
        "spec_revision": "8ed71e4a9adadbfddbde1548ef7060bcf79a76e9",
    },
}
FS3_JUDGMENT = {
    "issue": FS3_ISSUE,
    "pull_request": 115,
    "spec_revision": "1c02a9bc0d1812837f24076e1f04372ed8572e9a",
}
REQUIRED_SOFTWARE_CHECKS = frozenset(
    {
        "Build ESP32-S3 Firmware",
        "Execute ESP32-S3 QEMU Runtime",
        "Run Unit Tests",
        "Build CLI Tool",
        "Test Host Tooling",
        "Flutter / Analyze And Test Flutter App",
        "Flutter / Build iOS App",
        "CI Gate",
    }
)
MAX_QEMU_FILES = 10
MAX_QEMU_CHANGED_LINES = 2500
PROHIBITED_QEMU_PATH_PARTS = (
    "/target/xtensa/",
    "/tcg/",
    "/accel/tcg/",
    "/hw/core/clock-vmstate.c",
    "/hw/core/irq.c",
    "/system/replay/",
)
REPORT_SCHEMA_VERSION = 1
REPO_ROOT = Path(__file__).resolve().parents[2]
QEMU_SOURCE_REPOSITORY = "https://github.com/espressif/qemu.git"
PHYSICAL_EVIDENCE_IDS = (
    "two_board_discovery",
    "complementary_roles",
    "bidirectional_benchmark_simulation_off",
    "traced_drill",
)
RETAINED_ARTIFACT_HOSTS = frozenset(
    {
        "github.com",
        "api.github.com",
        "raw.githubusercontent.com",
        "objects.githubusercontent.com",
    }
)


class GateError(RuntimeError):
    """A condition that prevents an entry PASS."""


def _run(command: Sequence[str], *, cwd: Path = REPO_ROOT) -> str:
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=60,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise GateError(f"cannot execute {' '.join(command)}: {error}") from error
    if result.returncode:
        raise GateError(
            f"command exited {result.returncode}: {' '.join(command)}: "
            f"{result.stdout.strip()}"
        )
    return result.stdout.strip()


def _git(*args: str) -> str:
    return _run(("git", *args))


def _is_ancestor(ancestor: str, descendant: str) -> bool:
    result = subprocess.run(
        ("git", "merge-base", "--is-ancestor", ancestor, descendant),
        cwd=REPO_ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=30,
    )
    if result.returncode not in (0, 1):
        raise GateError(
            f"cannot resolve ancestry {ancestor}..{descendant}: {result.stderr.strip()}"
        )
    return result.returncode == 0


def _git_file(revision: str, path: str) -> str:
    return _git("show", f"{revision}:{path}")


def _gh_json(kind: str, number: int) -> Mapping[str, Any]:
    if kind == "pr":
        fields = (
            "number,title,state,isDraft,mergedAt,mergeCommit,headRefOid,"
            "baseRefName,url,statusCheckRollup"
        )
    elif kind == "issue":
        fields = "number,title,state,url,comments"
    else:
        raise ValueError(kind)
    raw = _run(
        (
            "gh",
            kind,
            "view",
            str(number),
            "--repo",
            REPOSITORY,
            "--json",
            fields,
        )
    )
    value = json.loads(raw)
    if not isinstance(value, dict) or value.get("number") != number:
        raise GateError(f"GitHub returned the wrong {kind} identity for {number}")
    return value


def _check_runs(pr: Mapping[str, Any]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for raw in pr.get("statusCheckRollup") or []:
        if not isinstance(raw, dict):
            continue
        result.append(
            {
                "workflow": raw.get("workflowName"),
                "name": raw.get("name"),
                "status": raw.get("status"),
                "conclusion": raw.get("conclusion"),
                "url": raw.get("detailsUrl"),
            }
        )
    return result


def _pr_identity(pr: Mapping[str, Any]) -> dict[str, Any]:
    merge = pr.get("mergeCommit")
    return {
        "number": pr["number"],
        "url": pr.get("url"),
        "title": pr.get("title"),
        "state": pr.get("state"),
        "draft": bool(pr.get("isDraft")),
        "base": pr.get("baseRefName"),
        "head_commit": pr.get("headRefOid"),
        "merge_commit": merge.get("oid") if isinstance(merge, dict) else None,
        "merged_at": pr.get("mergedAt"),
        "checks": _check_runs(pr),
    }


def _software_ci(pr: Mapping[str, Any]) -> tuple[bool, list[dict[str, Any]]]:
    checks = _check_runs(pr)
    required = [item for item in checks if item["workflow"] == "Software CI"]
    names = {item["name"] for item in required}
    return (
        names == REQUIRED_SOFTWARE_CHECKS
        and all(item["conclusion"] == "SUCCESS" for item in required),
        required,
    )


def _structured_payloads(issue: Mapping[str, Any]) -> list[dict[str, Any]]:
    """Return controller-authored schema payloads with durable comment identity."""
    result: list[dict[str, Any]] = []
    for comment in issue.get("comments") or []:
        if not isinstance(comment, dict):
            continue
        author = comment.get("author")
        if not isinstance(author, dict) or author.get("login") != TRACKER_ACTOR:
            continue
        for encoded in re.findall(
            r"```json\s*(.*?)\s*```", str(comment.get("body", "")), re.DOTALL
        ):
            try:
                payload = json.loads(
                    encoded, object_pairs_hook=_reject_duplicate_json_keys
                )
            except (json.JSONDecodeError, ValueError):
                continue
            if isinstance(payload, dict):
                result.append({**payload, "_comment_url": comment.get("url")})
    return result


def _valid_judgment(
    payload: Mapping[str, Any],
    *,
    issue: int,
    pull_request: int,
    spec_revision: str,
    commit: str | None,
) -> bool:
    criteria = payload.get("criteria")
    required_rework = payload.get("required_rework")
    verdict = payload.get("verdict")
    return bool(
        commit
        and set(payload) - {"_comment_url"}
        == {
            "claim_boundary",
            "commit",
            "criteria",
            "issue",
            "pull_request",
            "required_rework",
            "spec_revision",
            "verdict",
        }
        and isinstance(payload.get("claim_boundary"), str)
        and bool(payload["claim_boundary"].strip())
        and payload.get("commit") == commit
        and payload.get("issue") == issue
        and payload.get("pull_request") == pull_request
        and payload.get("spec_revision") == spec_revision
        and verdict in {"approve", "reject"}
        and isinstance(criteria, list)
        and bool(criteria)
        and all(
            isinstance(criterion, dict)
            and set(criterion) == {"criterion", "evidence", "status"}
            and isinstance(criterion.get("criterion"), str)
            and bool(criterion["criterion"].strip())
            and isinstance(criterion.get("evidence"), list)
            and bool(criterion["evidence"])
            and all(
                isinstance(item, str) and item.strip() for item in criterion["evidence"]
            )
            and criterion.get("status") in {"met", "not_met", "not_verifiable"}
            for criterion in criteria
        )
        and isinstance(required_rework, list)
        and all(isinstance(item, str) and item.strip() for item in required_rework)
        and (
            verdict != "approve"
            or (
                not required_rework
                and all(criterion["status"] == "met" for criterion in criteria)
            )
        )
    )


def _latest_judgment(
    issue: Mapping[str, Any],
    commit: str | None,
    *,
    issue_number: int,
    pull_request: int,
    spec_revision: str,
) -> dict[str, Any] | None:
    matches = [
        payload
        for payload in _structured_payloads(issue)
        if _valid_judgment(
            payload,
            issue=issue_number,
            pull_request=pull_request,
            spec_revision=spec_revision,
            commit=commit,
        )
    ]
    return matches[-1] if matches else None


def _latest_ancestral_judgment(
    issue: Mapping[str, Any],
    descendant: str | None,
    *,
    issue_number: int,
    pull_request: int,
    spec_revision: str,
) -> dict[str, Any] | None:
    if not descendant:
        return None
    matches = []
    for payload in _structured_payloads(issue):
        commit = payload.get("commit")
        if (
            isinstance(commit, str)
            and re.fullmatch(r"[0-9a-f]{40}", commit)
            and _valid_judgment(
                payload,
                issue=issue_number,
                pull_request=pull_request,
                spec_revision=spec_revision,
                commit=commit,
            )
            and _is_ancestor(commit, descendant)
        ):
            matches.append(payload)
    return matches[-1] if matches else None


def _judgment_identity(payload: Mapping[str, Any] | None) -> dict[str, Any] | None:
    if payload is None:
        return None
    return {
        "commit": payload.get("commit"),
        "verdict": payload.get("verdict"),
        "tracker_comment": payload.get("_comment_url"),
        "required_rework_count": len(payload.get("required_rework") or []),
    }


def _physical_record_valid(
    evidence_id: str, details: Any, configuration: Any, exact_commit: str
) -> bool:
    if not isinstance(details, dict):
        return False
    board_ids = details.get("board_ids")
    if (
        not isinstance(board_ids, list)
        or len(board_ids) != 2
        or any(not isinstance(item, str) or not item for item in board_ids)
        or len(set(board_ids)) != 2
    ):
        return False
    expected_simulation = evidence_id == "traced_drill"
    if (
        not isinstance(configuration, dict)
        or set(configuration)
        != {
            "firmware_commit",
            "board_image_sha256",
            "lifecycle_initial_state",
            "simulation_enabled",
        }
        or configuration.get("firmware_commit") != exact_commit
        or configuration.get("lifecycle_initial_state") != "disabled"
        or configuration.get("simulation_enabled") is not expected_simulation
        or not isinstance(configuration.get("board_image_sha256"), dict)
        or set(configuration["board_image_sha256"]) != set(board_ids)
        or any(
            not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None
            for digest in configuration["board_image_sha256"].values()
        )
    ):
        return False
    if evidence_id == "two_board_discovery":
        return set(details) == {"board_ids", "peer_counts"} and details.get(
            "peer_counts"
        ) == {board_ids[0]: 1, board_ids[1]: 1}
    if evidence_id == "complementary_roles":
        return set(details) == {"board_ids", "roles"} and details.get("roles") in (
            {board_ids[0]: "initiator", board_ids[1]: "responder"},
            {board_ids[0]: "responder", board_ids[1]: "initiator"},
        )
    if evidence_id == "bidirectional_benchmark_simulation_off":
        directions = details.get("directions")
        expected_directions = {
            (board_ids[0], board_ids[1], "passed"),
            (board_ids[1], board_ids[0], "passed"),
        }
        return (
            set(details) == {"board_ids", "simulation_enabled", "directions"}
            and details.get("simulation_enabled") is False
            and isinstance(directions, list)
            and len(directions) == 2
            and all(isinstance(item, dict) for item in directions)
            and all(set(item) == {"from", "to", "result"} for item in directions)
            and {
                (item.get("from"), item.get("to"), item.get("result"))
                for item in directions
            }
            == expected_directions
        )
    if evidence_id == "traced_drill":
        return set(details) == {
            "board_ids",
            "simulation_enabled",
            "trace_enabled",
            "drill_result",
        } and all(
            (
                details.get("simulation_enabled") is True,
                details.get("trace_enabled") is True,
                details.get("drill_result") == "passed",
            )
        )
    return False


def _download_artifact(artifact: Any, tracker_comment: Any) -> bytes | None:
    if not isinstance(artifact, dict) or set(artifact) != {"url", "sha256"}:
        return None
    url = artifact.get("url")
    digest = artifact.get("sha256")
    parsed = urllib.parse.urlparse(url) if isinstance(url, str) else None
    if (
        not isinstance(url, str)
        or parsed is None
        or parsed.scheme != "https"
        or parsed.hostname not in RETAINED_ARTIFACT_HOSTS
        or url == tracker_comment
        or not isinstance(digest, str)
        or re.fullmatch(r"[0-9a-f]{64}", digest) is None
    ):
        return None
    try:
        with urllib.request.urlopen(url, timeout=20) as response:
            final_host = urllib.parse.urlparse(response.geturl()).hostname
            if final_host not in RETAINED_ARTIFACT_HOSTS:
                return None
            content = response.read(16 * 1024 * 1024 + 1)
    except (OSError, urllib.error.URLError, ValueError):
        return None
    if len(content) > 16 * 1024 * 1024 or hashlib.sha256(content).hexdigest() != digest:
        return None
    return content


def _reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f"duplicate JSON key {key!r}")
        value[key] = item
    return value


def _validated_physical_artifact(
    artifact: Any,
    tracker_comment: Any,
    *,
    evidence_id: str,
    exact_commit: str,
    configuration: Any,
    procedure: Any,
    details: Any,
) -> dict[str, Any] | None:
    """Return a retained artifact only when its own content proves the claim."""
    content = _download_artifact(artifact, tracker_comment)
    if content is None:
        return None
    try:
        raw = json.loads(content, object_pairs_hook=_reject_duplicate_json_keys)
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError):
        return None
    expected = {
        "schema_version": 1,
        "kind": "fs-wp-003a-physical-evidence",
        "evidence_id": evidence_id,
        "commit": exact_commit,
        "level": "physical",
        "result": "passed",
        "configuration": configuration,
        "procedure": procedure,
        "details": details,
    }
    return raw if raw == expected else None


def _physical_evidence(
    issue: Mapping[str, Any], exact_commit: str | None
) -> tuple[dict[str, dict[str, Any]], list[str]]:
    """Validate identity-bound controller attestations embedded in tracker evidence.

    Every category names and hashes its retained runtime artifact. The controller
    attestation describes that artifact but is never accepted as the artifact itself.
    """
    candidates: dict[str, list[dict[str, Any]]] = {
        evidence_id: [] for evidence_id in PHYSICAL_EVIDENCE_IDS
    }
    invalid: set[str] = set()
    for payload in _structured_payloads(issue):
        # Independent judgments and disposition records are required on the same
        # commit, but are not physical records and must not poison this category.
        if payload.get("kind") != "fs-wp-003a-physical-acceptance":
            continue
        if payload.get("commit") != exact_commit:
            continue
        records = payload.get("verification")
        artifact_valid = (
            set(payload) - {"_comment_url"}
            == {
                "schema_version",
                "kind",
                "issue",
                "pull_request",
                "spec_revision",
                "commit",
                "acceptance_authority",
                "verification",
            }
            and payload.get("schema_version") == 1
            and payload.get("kind") == "fs-wp-003a-physical-acceptance"
            and payload.get("issue") == FS3_ISSUE
            and payload.get("pull_request") == FS3_JUDGMENT["pull_request"]
            and payload.get("spec_revision") == FS3_JUDGMENT["spec_revision"]
            and payload.get("acceptance_authority")
            == {"actor": TRACKER_ACTOR, "role": "controller", "decision": "accepted"}
            and isinstance(records, list)
        )
        if not artifact_valid:
            invalid.update(PHYSICAL_EVIDENCE_IDS)
            continue
        for record in records:
            if not isinstance(record, dict):
                continue
            evidence_id = record.get("evidence_id")
            if evidence_id not in candidates:
                continue
            valid = (
                set(record)
                == {
                    "evidence_id",
                    "level",
                    "status",
                    "configuration",
                    "procedure",
                    "details",
                    "artifact",
                }
                and record.get("level") == "physical"
                and record.get("status") == "passed"
                and isinstance(record.get("configuration"), dict)
                and isinstance(record.get("procedure"), str)
                and bool(record.get("procedure"))
                and _physical_record_valid(
                    str(evidence_id),
                    record.get("details"),
                    record.get("configuration"),
                    str(exact_commit),
                )
                and _validated_physical_artifact(
                    record.get("artifact"),
                    payload.get("_comment_url"),
                    evidence_id=str(evidence_id),
                    exact_commit=str(exact_commit),
                    configuration=record.get("configuration"),
                    procedure=record.get("procedure"),
                    details=record.get("details"),
                )
                is not None
            )
            if not valid:
                invalid.add(str(evidence_id))
                continue
            candidates[str(evidence_id)].append(
                {
                    "id": evidence_id,
                    "level": "physical",
                    "exact_commit": payload["commit"],
                    "result": "PASS",
                    "artifact": record["artifact"],
                    "acceptance_authority": payload["acceptance_authority"],
                    "tracker_comment": payload.get("_comment_url"),
                    "configuration": record["configuration"],
                    "procedure": record["procedure"],
                    "details": record["details"],
                }
            )
    result: dict[str, dict[str, Any]] = {}
    errors: list[str] = []
    for evidence_id, records in candidates.items():
        if evidence_id in invalid:
            errors.append(f"FS-WP-003A {evidence_id} contains an invalid record")
        if len(records) > 1:
            errors.append(f"FS-WP-003A {evidence_id} contains duplicate records")
        if evidence_id not in invalid and len(records) == 1:
            result[evidence_id] = records[0]
    board_sets = {
        tuple(sorted(record["details"]["board_ids"])) for record in result.values()
    }
    if len(board_sets) > 1:
        errors.append("FS-WP-003A physical records identify inconsistent board pairs")
        result.clear()
    return result, errors


def _repository_report(head: str) -> tuple[dict[str, Any], list[str]]:
    blockers: list[str] = []
    pinned_exists = True
    try:
        _git("cat-file", "-e", f"{SPEC_REVISION}^{{commit}}")
    except GateError:
        pinned_exists = False
        blockers.append(f"pinned specification revision {SPEC_REVISION} is unavailable")
    descends = pinned_exists and _is_ancestor(SPEC_REVISION, head)
    if not descends:
        blockers.append(f"repository HEAD {head} does not descend from {SPEC_REVISION}")
    base_descends = _is_ancestor(REQUIRED_BASE_REVISION, head)
    if not base_descends:
        blockers.append(
            f"repository HEAD {head} does not descend from required base "
            f"{REQUIRED_BASE_REVISION}"
        )
    return (
        {
            "head": head,
            "pinned_revision": SPEC_REVISION,
            "pinned_revision_exists": pinned_exists,
            "head_descends_from_pinned_revision": descends,
            "required_base_revision": REQUIRED_BASE_REVISION,
            "head_descends_from_required_base": base_descends,
            "evidence_tree_revision": SPEC_REVISION if pinned_exists else None,
        },
        blockers,
    )


def _ledger_and_integration(
    prs: Mapping[int, Mapping[str, Any]],
    issues: Mapping[int, Mapping[str, Any]],
) -> tuple[dict[str, Any], list[str]]:
    blockers: list[str] = []
    status = _git_file(SPEC_REVISION, "PROGRAM_STATUS.md")
    scheduler_plan = _git_file(
        SPEC_REVISION, "docs/plans/scheduler-trace-observability.md"
    )
    seam_plan = _git_file(SPEC_REVISION, "docs/plans/esp-now-radio-seam.md")
    pr105 = prs[105]
    pr130 = prs[130]
    pr105_merge = (pr105.get("mergeCommit") or {}).get("oid")
    pr130_merge = (pr130.get("mergeCommit") or {}).get("oid")
    pr105_integrated = bool(
        pr105.get("state") == "MERGED"
        and pr105_merge
        and _is_ancestor(str(pr105_merge), SPEC_REVISION)
    )
    pr130_integrated = bool(
        pr130.get("state") == "MERGED"
        and pr130_merge
        and _is_ancestor(str(pr130_merge), SPEC_REVISION)
    )
    table_rows = {
        cells[0]: cells
        for line in status.splitlines()
        if line.startswith("|")
        and len(
            cells := [
                cell.strip().replace("`", "") for cell in line.strip("|").split("|")
            ]
        )
        >= 3
    }
    c_row = table_rows.get("FS-WP-002C")
    e_row = table_rows.get("FS-WP-002E")
    ledger_pr105_stale = bool(
        c_row
        and c_row[2] == "Active / Amber"
        and "PR 105 passes review and merge" in status
        and pr105_integrated
    )
    ledger_e_stale = bool(
        e_row and e_row[2] == "Not due / Not rated" and pr130_integrated
    )
    c_head = pr105.get("headRefOid")
    e_head = pr130.get("headRefOid")
    c_identity = DEPENDENCY_JUDGMENTS[101]
    e_identity = DEPENDENCY_JUDGMENTS[123]
    c_judgment = _latest_judgment(
        issues[101], str(c_head), issue_number=101, **c_identity
    )
    e_judgment = _latest_judgment(
        issues[123], str(e_head), issue_number=123, **e_identity
    )
    c_retained_judgment = _latest_ancestral_judgment(
        issues[101], str(c_head), issue_number=101, **c_identity
    )
    e_retained_judgment = _latest_ancestral_judgment(
        issues[123], str(e_head), issue_number=123, **e_identity
    )
    c_accepted = bool(c_judgment and c_judgment.get("verdict") == "approve")
    e_accepted = bool(e_judgment and e_judgment.get("verdict") == "approve")
    plan_e_match = re.search(
        r"Issue #123 implements FS-WP-002E at specification revision\s*`([0-9a-f]{40})`",
        seam_plan,
    )
    implementation_match = re.search(
        r"Reviewed PR 105 candidate `([0-9a-f]{7,40})`", scheduler_plan
    )
    plan_e_identity = bool(
        plan_e_match
        and plan_e_match.group(1) == e_identity["spec_revision"]
        and "Issue #123 implements FS-WP-002E" in seam_plan
    )
    plan_c_commit = None
    if implementation_match:
        try:
            plan_c_commit = _git(
                "rev-parse", f"{implementation_match.group(1)}^{{commit}}"
            )
        except GateError:
            pass
    plan_c_retained = bool(
        plan_c_commit
        and c_head
        and pr105_merge
        and _is_ancestor(plan_c_commit, str(c_head))
        and _is_ancestor(str(c_head), str(pr105_merge))
    )
    implementation_e = "c8a67e4904bbc1af882d278125dbc6ef80ab8319"
    implementation_e_valid = bool(
        e_head
        and _is_ancestor(implementation_e, str(e_head))
        and _is_ancestor(str(e_head), SPEC_REVISION)
    )
    if not all(
        (
            pr105_integrated,
            pr130_integrated,
            ledger_pr105_stale,
            ledger_e_stale,
            plan_e_identity,
            plan_c_retained,
            c_accepted,
            e_accepted,
            implementation_e_valid,
        )
    ):
        blockers.append(
            "FS-WP-002C/002E integration or stale-ledger reconciliation is incomplete"
        )
    return (
        {
            "program_status_edited": False,
            "program_status_pr105_pointer_stale": ledger_pr105_stale,
            "program_status_fs_wp_002e_pointer_stale": ledger_e_stale,
            "fs_wp_002c": {
                "source_pr": 105,
                "pr_head": pr105.get("headRefOid"),
                "merge_commit": pr105_merge,
                "integrated_in_pinned_revision": pr105_integrated,
                "retained_plan": "docs/plans/scheduler-trace-observability.md",
                "retained_plan_identifies_pr": plan_c_retained,
                "retained_plan_implementation_commit": plan_c_commit,
                "required_evidence_judgment": _judgment_identity(c_judgment),
                "latest_ancestral_judgment": _judgment_identity(c_retained_judgment),
                "required_evidence_still_valid": c_accepted,
            },
            "fs_wp_002e": {
                "source_issue": 123,
                "integration_pr": 130,
                "implementation_commit": implementation_e,
                "implementation_commit_is_ancestral": implementation_e_valid,
                "pr_head": pr130.get("headRefOid"),
                "merge_commit": pr130_merge,
                "integrated_in_pinned_revision": pr130_integrated,
                "retained_plan": "docs/plans/esp-now-radio-seam.md",
                "retained_plan_identifies_package": plan_e_identity,
                "required_evidence_judgment": _judgment_identity(e_judgment),
                "latest_ancestral_judgment": _judgment_identity(e_retained_judgment),
                "required_evidence_still_valid": e_accepted,
            },
        },
        blockers,
    )


def _cmake_source_set(cmake: str, pattern: str) -> frozenset[str]:
    cmake = re.sub(r"(?m)#.*$", "", cmake)
    match = re.search(pattern, cmake, re.DOTALL)
    if not match:
        return frozenset()
    return frozenset(re.findall(r'"([^"\n]+\.cpp)"', match.group(1)))


def _seam_report() -> tuple[dict[str, Any], list[str]]:
    header = _git_file(
        SPEC_REVISION, "firmware/domes/main/transport/espNowTransport.hpp"
    )
    source = _git_file(
        SPEC_REVISION, "firmware/domes/main/transport/espNowTransport.cpp"
    )
    interface = _git_file(
        SPEC_REVISION, "firmware/domes/main/transport/iEspNowRadio.hpp"
    )
    regression = _git_file(
        SPEC_REVISION, "firmware/test_app/main/test_esp_now_transport.cpp"
    )
    cmake = _git_file(SPEC_REVISION, "firmware/domes/main/CMakeLists.txt")
    header_code = re.sub(
        r"//.*?$|/\*.*?\*/", "", header, flags=re.MULTILINE | re.DOTALL
    )
    source_code = re.sub(
        r"//.*?$|/\*.*?\*/", "", source, flags=re.MULTILINE | re.DOTALL
    )
    interface_code = re.sub(
        r"//.*?$|/\*.*?\*/", "", interface, flags=re.MULTILINE | re.DOTALL
    )
    regression_code = re.sub(
        r"//.*?$|/\*.*?\*/", "", regression, flags=re.MULTILINE | re.DOTALL
    )
    shared = _cmake_source_set(
        cmake, r"set\(DOMES_SHARED_SRCS(.*?)\)\s*\n\s*if\(CONFIG_DOMES_RUNTIME_PROFILE"
    )
    qemu_root = _cmake_source_set(
        cmake,
        r"elseif\(CONFIG_DOMES_RUNTIME_PROFILE_QEMU\)(.*?)"
        r"elseif\(CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL\)",
    )
    physical_root = _cmake_source_set(
        cmake,
        r"elseif\(CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL\)(.*?)"
        r"else\(\)\s*\n\s*message\(FATAL_ERROR",
    )
    capacity_values = {
        "buffer": 2048,
        "overhead": 8,
        "address": 6,
        "token": 4,
        "payload": 250,
    }
    baseline_metadata = 3 + capacity_values["address"]
    correlated_metadata = baseline_metadata + capacity_values["token"]

    def item_storage(metadata: int) -> int:
        unaligned = metadata + capacity_values["payload"]
        return capacity_values["overhead"] + ((unaligned + 3) & ~3)

    baseline_frames = capacity_values["buffer"] // item_storage(baseline_metadata)
    correlated_frames = capacity_values["buffer"] // item_storage(correlated_metadata)
    checks = {
        "production_transport_owns_radio_seam": bool(
            re.search(
                r"explicit\s+EspNowTransport\(IEspNowRadio&\s+radio\);", header_code
            )
            and re.search(r"IEspNowRadio&\s+radio_;", header_code)
            and re.search(
                r"EspNowTransport::EspNowTransport\(IEspNowRadio&\s+radio\)\s*"
                r":\s*radio_\(radio\)\s*\{\}",
                source_code,
            )
            and all(
                re.search(rf"\bradio_\.{method}\(", source_code)
                for method in (
                    "init",
                    "deinit",
                    "send",
                    "addPeer",
                    "removePeer",
                    "getPeerCounts",
                    "peerExists",
                )
            )
        ),
        "seven_maximum_size_pending_frames": bool(
            re.search(r"kEspNowRxBufSize\s*=\s*2048\s*;", header_code)
            and re.search(r"kEspNowRingItemOverhead\s*=\s*8\s*;", header_code)
            and re.search(r"kEspNowAddressSize\s*=\s*6\s*;", interface_code)
            and re.search(r"kEspNowMaxPayload\s*=\s*250\s*;", interface_code)
            and re.search(
                r"using\s+EspNowCorrelationToken\s*=\s*uint32_t\s*;", interface_code
            )
            and re.search(
                r"kEspNowRxMaxFrames\s*=\s*kEspNowRxBufSize\s*/\s*"
                r"espNowRingItemStorage\(kEspNowRxMetadataSize\)\s*;",
                header_code,
            )
            and re.search(
                r"static_assert\(kEspNowRxMaxFrames\s*==\s*"
                r"kEspNowRxBaselineMaxFrames\s*[,)]",
                regression_code,
            )
            and re.search(
                r"static_assert\(kEspNowRxBaselineMaxFrames\s*==\s*7\s*\);",
                regression_code,
            )
            and "kEspNowRxMaxFrames + 1" in regression_code
            and baseline_frames == correlated_frames == 7
        ),
        "bounded_correlation_tokens": bool(
            re.search(
                r"using\s+EspNowCorrelationToken\s*=\s*uint32_t\s*;", interface_code
            )
            and len(re.findall(r"std::atomic<EspNowCorrelationToken>", header_code))
            >= 3
            and re.search(
                r"EspNowTransport::nextToken\(std::atomic<EspNowCorrelationToken>&\s*"
                r"counter\).*?fetch_add\(1.*?if\s*\(token\s*==\s*0\).*?fetch_add\(1",
                source_code,
                re.DOTALL,
            )
            and "nextToken(txToken_)" in source_code
        ),
        "physical_image_isolation": bool(
            shared
            and qemu_root
            and physical_root
            and shared | qemu_root == runtime.QEMU_MAIN_SOURCES
            and shared | physical_root == runtime.PHYSICAL_MAIN_SOURCES
            and not any("platform/qemu/" in path for path in shared | physical_root)
            and "transport/espNowTransport.cpp" in physical_root
            and "transport/espNowTransport.cpp" not in shared | qemu_root
        ),
    }
    blockers = (
        [] if all(checks.values()) else ["FS-WP-002E seam invariant validation failed"]
    )
    return (
        {
            "revision": SPEC_REVISION,
            "capacity": {
                "baseline_metadata_bytes": baseline_metadata,
                "correlated_metadata_bytes": correlated_metadata,
                "baseline_maximum_frames": baseline_frames,
                "correlated_maximum_frames": correlated_frames,
            },
            "source_composition": {
                "physical": sorted(shared | physical_root),
                "qemu": sorted(shared | qemu_root),
            },
            "checks": checks,
            "result": "PASS" if not blockers else "BLOCKED",
        },
        blockers,
    )


def _fidelity_schema_report() -> tuple[dict[str, Any], list[str]]:
    path = "firmware/domes/profiles/runtime_profiles.json"
    source = _git_file(SPEC_REVISION, path)
    valid = False
    error: str | None = None
    manifests: dict[str, dict[str, Any]] = {}
    try:
        with tempfile.TemporaryDirectory(prefix="domes-fidelity-gate-") as directory:
            root = Path(directory)
            spec_path = root / "runtime_profiles.json"
            spec_path.write_text(source, encoding="utf-8")
            configs = {
                "qemu": (
                    'CONFIG_IDF_TARGET="esp32s3"\n'
                    "CONFIG_DOMES_RUNTIME_PROFILE_QEMU=y\n"
                    "CONFIG_APP_REPRODUCIBLE_BUILD=y\n"
                    "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y\n"
                    "CONFIG_ESP_CONSOLE_UART_DEFAULT=y\n"
                    "CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096\n"
                    "CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y\n"
                    "CONFIG_FREERTOS_HZ=1000\n"
                    "# CONFIG_FREERTOS_UNICORE is not set\n"
                    "# CONFIG_BT_ENABLED is not set\n"
                    "# CONFIG_DOMES_OTA_AUTO_CHECK is not set\n"
                    "# CONFIG_DOMES_WIFI_AUTO_CONNECT is not set\n"
                    "# CONFIG_ESP_COEX_SW_COEXIST_ENABLE is not set\n"
                    "# CONFIG_ESP_TASK_WDT_EN is not set\n"
                    "# CONFIG_SPIRAM is not set\n"
                    "# CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH is not set\n"
                ),
                "physical": (
                    'CONFIG_IDF_TARGET="esp32s3"\n'
                    "CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL=y\n"
                    "# CONFIG_DOMES_RUNTIME_PROFILE_QEMU is not set\n"
                    "CONFIG_DOMES_WIFI_AUTO_CONNECT=y\n"
                    "CONFIG_DOMES_OTA_AUTO_CHECK=y\n"
                    "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y\n"
                    "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y\n"
                    "CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096\n"
                    "CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y\n"
                    "CONFIG_FREERTOS_HZ=1000\n"
                    "CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y\n"
                    "CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y\n"
                    "# CONFIG_FREERTOS_UNICORE is not set\n"
                ),
            }
            for profile_key, config in configs.items():
                config_path = root / f"sdkconfig.{profile_key}"
                config_path.write_text(config, encoding="utf-8")
                resolved = profile_generator.resolve_profile(
                    spec_path, profile_key, config_path
                )
                manifest = resolved["manifest"]
                manifests[profile_key] = {
                    "profile": manifest["profile"],
                    "component_count": len(manifest["components"]),
                    "task_count": len(manifest["tasks"]),
                    "manifest_sha256": resolved["manifest_sha256"],
                }
            valid = set(manifests) == {"qemu", "physical"}
    except (OSError, profile_generator.ProfileError, KeyError, TypeError) as exc:
        error = str(exc)
    return (
        {
            "path": path,
            "revision": SPEC_REVISION,
            "canonical_validator": "generate_runtime_profile.resolve_profile",
            "validated_profiles": manifests,
            "exact_schema": valid,
            "error": error,
            "result": "PASS" if valid else "BLOCKED",
        },
        [] if valid else ["fidelity-manifest schema is missing or has drifted"],
    )


def _qemu_source_identity() -> tuple[dict[str, Any], dict[str, bool]]:
    tag_ref = f"refs/tags/{feasibility.EXPECTED_QEMU_RELEASE_TAG}"
    try:
        remote_refs = _run(
            (
                "git",
                "ls-remote",
                QEMU_SOURCE_REPOSITORY,
                tag_ref,
                f"{tag_ref}^{{}}",
            )
        )
        resolved_refs = {
            ref: commit
            for line in remote_refs.splitlines()
            if len((parts := line.split())) == 2
            for commit, ref in [parts]
        }
        source_identity = {
            "repository": QEMU_SOURCE_REPOSITORY,
            "release_tag": feasibility.EXPECTED_QEMU_RELEASE_TAG,
            "tag_object": resolved_refs.get(tag_ref),
            "source_revision": resolved_refs.get(f"{tag_ref}^{{}}"),
        }
        return source_identity, {
            "qemu_tag_object": (
                source_identity["tag_object"] == feasibility.EXPECTED_QEMU_TAG_OBJECT
            ),
            "qemu_source_revision": (
                source_identity["source_revision"]
                == feasibility.EXPECTED_QEMU_SOURCE_REVISION
            ),
        }
    except GateError as error:
        return (
            {"repository": QEMU_SOURCE_REPOSITORY, "error": str(error)},
            {"qemu_tag_object": False, "qemu_source_revision": False},
        )


def _engine_report(validate_installed: bool) -> tuple[dict[str, Any], list[str]]:
    expected = {
        "idf_version": feasibility.EXPECTED_IDF_VERSION,
        "idf_revision": feasibility.EXPECTED_IDF_REVISION,
        "compiler_version": feasibility.EXPECTED_COMPILER_VERSION,
        "compiler_package": feasibility.EXPECTED_COMPILER_PACKAGE,
        "compiler_executable_sha256": feasibility.EXPECTED_COMPILER_SHA256,
        "compiler_archive_name": feasibility.EXPECTED_COMPILER_ARCHIVE_NAME,
        "compiler_archive_sha256": feasibility.EXPECTED_COMPILER_ARCHIVE_SHA256,
        "qemu_version": feasibility.EXPECTED_QEMU_VERSION,
        "qemu_package": feasibility.EXPECTED_QEMU_PACKAGE,
        "qemu_release_tag": feasibility.EXPECTED_QEMU_RELEASE_TAG,
        "qemu_tag_object": feasibility.EXPECTED_QEMU_TAG_OBJECT,
        "qemu_source_revision": feasibility.EXPECTED_QEMU_SOURCE_REVISION,
        "qemu_executable_sha256": feasibility.EXPECTED_QEMU_SHA256,
        "qemu_archive_name": feasibility.EXPECTED_QEMU_ARCHIVE_NAME,
        "qemu_archive_sha256": feasibility.EXPECTED_QEMU_ARCHIVE_SHA256,
    }
    report: dict[str, Any] = {
        "expected": expected,
        "installed_validation_requested": validate_installed,
    }
    if not validate_installed:
        report.update({"installed": None, "result": "BLOCKED"})
        return report, ["installed pinned-engine identity validation was not requested"]
    source_identity, source_checks = _qemu_source_identity()
    try:
        toolchain = feasibility.discover_toolchain(require_gdb=False)
    except feasibility.FeasibilityError as error:
        report.update(
            {
                "installed": None,
                "source_identity": source_identity,
                "identity_checks": source_checks,
                "result": "BLOCKED",
                "error": str(error),
            }
        )
        return report, [f"pinned-engine identity validation failed: {error}"]
    installed = dict(feasibility._toolchain_identity(toolchain))
    identity_checks = {
        "idf_version": toolchain.idf_version == feasibility.EXPECTED_IDF_VERSION,
        "idf_revision": toolchain.idf_revision == feasibility.EXPECTED_IDF_REVISION,
        "compiler_version": (
            toolchain.compiler_version == feasibility.EXPECTED_COMPILER_VERSION
        ),
        "compiler_executable_sha256": (
            toolchain.compiler_sha256 == feasibility.EXPECTED_COMPILER_SHA256
        ),
        "compiler_archive_sha256": (
            toolchain.compiler_archive is not None
            and toolchain.compiler_archive_sha256
            == feasibility.EXPECTED_COMPILER_ARCHIVE_SHA256
        ),
        "qemu_version": toolchain.qemu_version == feasibility.EXPECTED_QEMU_VERSION,
        "qemu_executable_sha256": (
            toolchain.qemu_sha256 == feasibility.EXPECTED_QEMU_SHA256
        ),
        "qemu_archive_sha256": (
            toolchain.qemu_archive is not None
            and toolchain.qemu_archive_sha256
            == feasibility.EXPECTED_QEMU_ARCHIVE_SHA256
        ),
    }
    identity_checks.update(source_checks)
    valid = all(identity_checks.values())
    report.update(
        {
            "installed": installed,
            "source_identity": source_identity,
            "identity_checks": identity_checks,
            "result": "PASS" if valid else "BLOCKED",
        }
    )
    if valid:
        return report, []
    failed = ", ".join(name for name, passed in identity_checks.items() if not passed)
    return report, [
        f"pinned-engine identities remain unresolved or mismatched: {failed}"
    ]


def _patch_budget_report() -> tuple[dict[str, Any], list[str]]:
    status = _git("status", "--porcelain=v1", "--untracked-files=all")
    reviewed_paths = [
        path
        for path in _git(
            "diff", "--name-only", f"{REQUIRED_BASE_REVISION}...HEAD"
        ).splitlines()
        if path
    ]
    numstat = _git("diff", "--numstat", f"{REQUIRED_BASE_REVISION}...HEAD")
    reviewed_changed_lines = sum(
        int(added) + int(deleted)
        for line in numstat.splitlines()
        if len(parts := line.split("\t", 2)) == 3
        for added, deleted in [parts[:2]]
        if added.isdigit() and deleted.isdigit()
    )
    # This entry-only ticket may not anticipate or carry an engine patch.  Derive
    # that assertion from the reviewed diff instead of manufacturing an empty list.
    paths = [
        path
        for path in reviewed_paths
        if path.startswith(("qemu/", "third_party/qemu/", "vendor/qemu/"))
    ]
    changed_lines = sum(
        int(added) + int(deleted)
        for line in numstat.splitlines()
        if len(parts := line.split("\t", 2)) == 3
        for added, deleted, path in [parts]
        if path in paths and added.isdigit() and deleted.isdigit()
    )
    prohibited = [
        path
        for path in paths
        if any(part in f"/{path}" for part in PROHIBITED_QEMU_PATH_PARTS)
    ]
    protected_paths = [
        path for path in reviewed_paths if not path.startswith("tools/simulation/")
    ]
    valid = (
        not status
        and not protected_paths
        and len(paths) <= MAX_QEMU_FILES
        and changed_lines <= MAX_QEMU_CHANGED_LINES
        and not prohibited
    )
    return (
        {
            "adopted_limits": {
                "non_generated_files": MAX_QEMU_FILES,
                "changed_lines": MAX_QEMU_CHANGED_LINES,
                "prohibited_path_parts": list(PROHIBITED_QEMU_PATH_PARTS),
            },
            "anticipated_qemu_patch": {
                "source": f"git diff {REQUIRED_BASE_REVISION}...HEAD",
                "paths": paths,
                "non_generated_files": len(paths),
                "changed_lines": changed_lines,
                "prohibited_paths": prohibited,
            },
            "reviewed_diff": {
                "paths": reviewed_paths,
                "changed_lines": reviewed_changed_lines,
                "dirty_worktree_entries": status.splitlines(),
                "protected_paths": protected_paths,
                "hardware_operations": [],
            },
            "result": "PASS" if valid else "BLOCKED",
        },
        (
            []
            if valid
            else ["QEMU budget/protected-path audit is dirty, out of scope, or invalid"]
        ),
    )


def _pr107_disposition(
    issue: Mapping[str, Any], old_head: Any, integration_head: Any
) -> dict[str, Any] | None:
    matches = []
    for payload in _structured_payloads(issue):
        if (
            set(payload) - {"_comment_url"}
            == {
                "schema_version",
                "kind",
                "issue",
                "spec_revision",
                "legacy_pull_request",
                "legacy_head",
                "disposition",
                "replacement_pull_request",
                "replacement_head",
                "acceptance_authority",
            }
            and payload.get("schema_version") == 1
            and payload.get("kind") == "fs-wp-003a-pr-disposition"
            and payload.get("issue") == FS3_ISSUE
            and payload.get("spec_revision") == FS3_JUDGMENT["spec_revision"]
            and payload.get("legacy_pull_request") == 107
            and payload.get("legacy_head") == old_head
            and payload.get("disposition") in {"superseded", "integrated"}
            and payload.get("replacement_pull_request") == 115
            and payload.get("replacement_head") == integration_head
            and payload.get("acceptance_authority")
            == {"actor": TRACKER_ACTOR, "role": "controller", "decision": "accepted"}
        ):
            matches.append(payload)
    return matches[-1] if matches else None


def _fs3_report(
    prs: Mapping[int, Mapping[str, Any]], issue: Mapping[str, Any]
) -> tuple[dict[str, Any], list[str]]:
    blockers: list[str] = []
    old_pr = prs[107]
    integrated_pr = prs[115]
    integration_head = integrated_pr.get("headRefOid")
    merge_commit = (integrated_pr.get("mergeCommit") or {}).get("oid")
    integrated = bool(
        integrated_pr.get("state") == "MERGED"
        and integration_head
        and merge_commit
        and _is_ancestor(str(integration_head), SPEC_REVISION)
        and _is_ancestor(str(merge_commit), SPEC_REVISION)
    )
    old_head = old_pr.get("headRefOid")
    old_head_ancestral = bool(old_head and _is_ancestor(str(old_head), SPEC_REVISION))
    disposition = _pr107_disposition(issue, old_head, integration_head)
    disposition_resolved = bool(
        disposition and old_pr.get("state") in {"MERGED", "CLOSED"}
    )
    if not disposition_resolved:
        blockers.append(
            f"PR 107 state/disposition is unresolved ({old_pr.get('state')})"
        )
    software_pass, software_checks = _software_ci(integrated_pr)
    if not integrated or not software_pass:
        blockers.append("FS-WP-003A lacks integrated exact-head successful Software CI")

    acceptance = _latest_judgment(
        issue,
        str(integration_head),
        issue_number=FS3_JUDGMENT["issue"],
        pull_request=FS3_JUDGMENT["pull_request"],
        spec_revision=FS3_JUDGMENT["spec_revision"],
    )
    accepted = bool(acceptance and acceptance.get("verdict") == "approve")
    if not accepted:
        blockers.append("FS-WP-003A lacks independent exact-head acceptance")

    accepted_physical, evidence_errors = _physical_evidence(
        issue, str(integration_head)
    )
    blockers.extend(evidence_errors)
    evidence = [
        {
            "id": "software_ci",
            "level": "automated",
            "exact_commit": integration_head,
            "result": "PASS" if integrated and software_pass else "MISSING",
            "artifact": next(
                (item["url"] for item in software_checks if item["name"] == "CI Gate"),
                None,
            ),
            "acceptance_authority": "GitHub Software CI",
        }
    ]
    for evidence_id in PHYSICAL_EVIDENCE_IDS:
        record = accepted_physical.get(
            evidence_id,
            {
                "id": evidence_id,
                "level": "physical",
                "exact_commit": None,
                "result": "MISSING",
                "artifact": None,
                "acceptance_authority": None,
                "tracker_comment": None,
            },
        )
        evidence.append(record)
        if (
            record["result"] != "PASS"
            or not record["artifact"]
            or not record["acceptance_authority"]
        ):
            blockers.append(
                f"FS-WP-003A {evidence_id} exact-integrated-head evidence is unavailable"
            )
    return (
        {
            "source_issue": {
                "number": issue.get("number"),
                "state": issue.get("state"),
                "url": issue.get("url"),
            },
            "legacy_pr_107": {
                **_pr_identity(old_pr),
                "head_is_ancestor_of_pinned_revision": old_head_ancestral,
                "disposition_resolved": disposition_resolved,
                "disposition": (
                    {
                        key: value
                        for key, value in disposition.items()
                        if key != "_comment_url"
                    }
                    if disposition
                    else None
                ),
                "disposition_tracker_comment": (
                    disposition.get("_comment_url") if disposition else None
                ),
            },
            "accepted_integration_candidate": {
                **_pr_identity(integrated_pr),
                "integrated_in_pinned_revision": integrated,
                "independent_acceptance": _judgment_identity(acceptance),
                "exact_head_accepted": accepted,
            },
            "evidence_matrix": evidence,
            "result": "PASS" if not blockers else "BLOCKED",
        },
        blockers,
    )


def build_report(*, validate_installed_engine: bool = True) -> dict[str, Any]:
    blockers: list[str] = []
    head = _git("rev-parse", "HEAD")
    repository, found = _repository_report(head)
    blockers.extend(found)

    prs: dict[int, Mapping[str, Any]] = {}
    tracker_error: str | None = None
    try:
        prs = {number: _gh_json("pr", number) for number in PR_NUMBERS}
        issues = {number: _gh_json("issue", number) for number in ISSUE_NUMBERS}
        issue = issues[FS3_ISSUE]
    except (GateError, json.JSONDecodeError) as error:
        tracker_error = str(error)
        issue = {}
        blockers.append(f"live tracker access is unresolved: {error}")

    ledger: dict[str, Any] = {"result": "BLOCKED", "error": tracker_error}
    fs3: dict[str, Any] = {"result": "BLOCKED", "error": tracker_error}
    if not tracker_error:
        ledger, found = _ledger_and_integration(prs, issues)
        blockers.extend(found)
        fs3, found = _fs3_report(prs, issue)
        blockers.extend(found)

    seam, found = _seam_report()
    blockers.extend(found)
    fidelity, found = _fidelity_schema_report()
    blockers.extend(found)
    engine, found = _engine_report(validate_installed_engine)
    blockers.extend(found)
    budget, found = _patch_budget_report()
    blockers.extend(found)

    unique_blockers = list(dict.fromkeys(blockers))
    verdict = "PASS" if not unique_blockers else "BLOCKED"
    return {
        "schema_version": REPORT_SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "work_package": "FS-WP-002F",
        "gate": "fs3-entry-gate",
        "repository": repository,
        "tracker": {
            "repository": REPOSITORY,
            "access_resolved": tracker_error is None,
            "pull_requests": (
                [_pr_identity(prs[number]) for number in PR_NUMBERS] if prs else []
            ),
            "error": tracker_error,
        },
        "stale_ledger_reconciliation": ledger,
        "fs_wp_002e_seam": seam,
        "fs_wp_003a": fs3,
        "fidelity_manifest_schema": fidelity,
        "pinned_engine": engine,
        "qemu_patch_budget": budget,
        "protected_path_audit": {
            "gate_allowed_surface": "tools/simulation/**",
            "identity_source": budget.get("anticipated_qemu_patch", {}).get("source"),
            **budget.get("reviewed_diff", {}),
            "result": budget.get("result", "BLOCKED"),
        },
        "blockers": unique_blockers,
        "verdict": verdict,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, help="write the canonical JSON report here"
    )
    parser.add_argument(
        "--skip-installed-engine-validation",
        action="store_true",
        help="test-only/development escape hatch; always makes the gate BLOCKED",
    )
    args = parser.parse_args()
    try:
        report = build_report(
            validate_installed_engine=not args.skip_installed_engine_validation
        )
    except GateError as error:
        report = {
            "schema_version": REPORT_SCHEMA_VERSION,
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "work_package": "FS-WP-002F",
            "gate": "fs3-entry-gate",
            "blockers": [str(error)],
            "verdict": "BLOCKED",
        }
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    sys.stdout.write(encoded)
    return 0 if report["verdict"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
