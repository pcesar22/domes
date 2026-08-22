#!/usr/bin/env python3
"""Fail-closed FS-WP-002F entry verification.

The gate reads repository state and GitHub tracker state but never changes either.  A
report is always emitted when possible; exit status 0 means PASS and 2 means BLOCKED.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
import qemu_feasibility as feasibility

SPEC_REVISION = "498ae0203dc8b7048682fbff718a0629243a98a8"
REPOSITORY = "pcesar22/domes"
PR_NUMBERS = (105, 107, 115, 130)
FS3_ISSUE = 114
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


def _physical_evidence(
    issue: Mapping[str, Any], exact_commit: str | None
) -> dict[str, dict[str, Any]]:
    """Extract accepted evidence only from schema-shaped tracker handoffs."""
    result: dict[str, dict[str, Any]] = {}
    matchers = {
        "two_board_discovery": lambda text: "discovery" in text,
        "complementary_roles": lambda text: "complementary" in text and "role" in text,
        "bidirectional_benchmark_simulation_off": lambda text: (
            "bidirectional" in text and "benchmark" in text and "simulation" in text
        ),
        "traced_drill": lambda text: "traced" in text and "drill" in text,
    }
    for comment in issue.get("comments") or []:
        if not isinstance(comment, dict):
            continue
        body = str(comment.get("body", ""))
        author = comment.get("author")
        association = comment.get("authorAssociation")
        authority = (
            f"{author.get('login')} ({association})"
            if isinstance(author, dict)
            and author.get("login")
            and association in {"OWNER", "MEMBER", "COLLABORATOR"}
            else None
        )
        for encoded in re.findall(r"```json\s*(.*?)\s*```", body, re.DOTALL):
            try:
                payload = json.loads(encoded)
            except json.JSONDecodeError:
                continue
            if not isinstance(payload, dict) or payload.get("commit") != exact_commit:
                continue
            for record in payload.get("verification") or []:
                if not isinstance(record, dict) or record.get("level") != "physical":
                    continue
                observation = str(record.get("command_or_observation", "")).lower()
                for evidence_id, matches in matchers.items():
                    if matches(observation):
                        result[evidence_id] = {
                            "id": evidence_id,
                            "level": "physical",
                            "exact_commit": payload["commit"],
                            "result": (
                                "PASS"
                                if record.get("status") == "passed"
                                else "PENDING"
                            ),
                            "artifact": record.get("artifact"),
                            "acceptance_authority": authority,
                            "tracker_comment": comment.get("url"),
                        }
    return result


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
    return (
        {
            "head": head,
            "pinned_revision": SPEC_REVISION,
            "pinned_revision_exists": pinned_exists,
            "head_descends_from_pinned_revision": descends,
            "evidence_tree_revision": SPEC_REVISION if pinned_exists else None,
        },
        blockers,
    )


def _ledger_and_integration(
    prs: Mapping[int, Mapping[str, Any]],
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
    ledger_pr105_stale = "PR 105 passes review and merge" in status and pr105_integrated
    ledger_e_stale = (
        "| FS-WP-002E |" in status
        and "`Not due` / `Not rated`" in status
        and pr130_integrated
    )
    plan_e_identity = (
        re.search(r"Issue #123 implements FS-WP-002E", seam_plan) is not None
    )
    plan_c_retained = "PR 105" in scheduler_plan
    if not all(
        (
            pr105_integrated,
            pr130_integrated,
            ledger_pr105_stale,
            ledger_e_stale,
            plan_e_identity,
            plan_c_retained,
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
            },
            "fs_wp_002e": {
                "source_issue": 123,
                "integration_pr": 130,
                "implementation_commit": "c8a67e4904bbc1af882d278125dbc6ef80ab8319",
                "pr_head": pr130.get("headRefOid"),
                "merge_commit": pr130_merge,
                "integrated_in_pinned_revision": pr130_integrated,
                "retained_plan": "docs/plans/esp-now-radio-seam.md",
                "retained_plan_identifies_package": plan_e_identity,
            },
        },
        blockers,
    )


def _seam_report() -> tuple[dict[str, Any], list[str]]:
    header = _git_file(
        SPEC_REVISION, "firmware/domes/main/transport/espNowTransport.hpp"
    )
    source = _git_file(
        SPEC_REVISION, "firmware/domes/main/transport/espNowTransport.cpp"
    )
    cmake = _git_file(SPEC_REVISION, "firmware/domes/main/CMakeLists.txt")
    checks = {
        "production_transport_owns_radio_seam": all(
            marker in header
            for marker in (
                "EspNowTransport(IEspNowRadio& radio)",
                "IEspNowRadio& radio_",
            )
        ),
        "seven_maximum_size_pending_frames": all(
            marker in header
            for marker in (
                "kEspNowRxBufSize = 2048",
                "kEspNowMaxPayload",
                "kEspNowRxMaxFrames >= kEspNowRxBaselineMaxFrames",
            )
        )
        and (2048 // (8 + 4 * ((13 + 250 + 3) // 4))) == 7,
        "bounded_correlation_tokens": all(
            marker in source
            for marker in ("nextToken(txToken_)", "std::atomic<EspNowCorrelationToken>")
        )
        and "EspNowCorrelationToken" in header,
        "physical_image_isolation": "platform/qemu" not in cmake.split("else()", 1)[-1]
        and "QemuEspNow" not in cmake,
    }
    blockers = (
        [] if all(checks.values()) else ["FS-WP-002E seam invariant validation failed"]
    )
    return (
        {
            "revision": SPEC_REVISION,
            "checks": checks,
            "result": "PASS" if not blockers else "BLOCKED",
        },
        blockers,
    )


def _fidelity_schema_report() -> tuple[dict[str, Any], list[str]]:
    expected_root = {
        "schema_version",
        "component_catalog",
        "task_catalog",
        "fidelity_contracts",
        "profiles",
    }
    expected_contract = {
        "implementation",
        "inputs",
        "outputs",
        "timing",
        "calibration",
        "limitations",
    }
    raw = json.loads(
        _git_file(SPEC_REVISION, "firmware/domes/profiles/runtime_profiles.json")
    )
    contracts = raw.get("fidelity_contracts") if isinstance(raw, dict) else None
    valid = (
        isinstance(raw, dict)
        and set(raw) == expected_root
        and raw.get("schema_version") == 1
        and isinstance(contracts, dict)
        and bool(contracts)
        and all(
            isinstance(value, dict)
            and set(value) == expected_contract
            and all(isinstance(item, str) and item for item in value.values())
            for value in contracts.values()
        )
    )
    return (
        {
            "path": "firmware/domes/profiles/runtime_profiles.json",
            "revision": SPEC_REVISION,
            "schema_version": (
                raw.get("schema_version") if isinstance(raw, dict) else None
            ),
            "contract_count": len(contracts) if isinstance(contracts, dict) else 0,
            "exact_schema": valid,
            "result": "PASS" if valid else "BLOCKED",
        },
        [] if valid else ["fidelity-manifest schema is missing or has drifted"],
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
    try:
        toolchain = feasibility.discover_toolchain(require_gdb=False)
    except feasibility.FeasibilityError as error:
        report.update({"installed": None, "result": "BLOCKED", "error": str(error)})
        return report, [f"pinned-engine identity validation failed: {error}"]
    installed = dict(feasibility._toolchain_identity(toolchain))
    report.update({"installed": installed, "result": "PASS"})
    return report, []


def _patch_budget_report() -> tuple[dict[str, Any], list[str]]:
    # No QEMU engine patch is allowed to be anticipated by this entry-only ticket.
    paths: list[str] = []
    changed_lines = 0
    prohibited = [
        path
        for path in paths
        if any(part in f"/{path}" for part in PROHIBITED_QEMU_PATH_PARTS)
    ]
    valid = (
        len(paths) <= MAX_QEMU_FILES
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
                "paths": paths,
                "non_generated_files": len(paths),
                "changed_lines": changed_lines,
                "prohibited_paths": prohibited,
            },
            "result": "PASS" if valid else "BLOCKED",
        },
        [] if valid else ["adopted QEMU structural patch budget is invalid"],
    )


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
    disposition_resolved = old_pr.get("state") in {"MERGED", "CLOSED"}
    if not disposition_resolved:
        blockers.append(
            f"PR 107 remains {old_pr.get('state')}; its integration disposition is unresolved"
        )
    software_pass, software_checks = _software_ci(integrated_pr)
    if not integrated or not software_pass:
        blockers.append("FS-WP-003A lacks integrated exact-head successful Software CI")

    accepted_physical = _physical_evidence(issue, str(integration_head))
    evidence = [
        {
            "id": "software_ci",
            "level": "automated",
            "exact_commit": integration_head,
            "result": "PASS" if software_pass else "MISSING",
            "artifact": next(
                (item["url"] for item in software_checks if item["name"] == "CI Gate"),
                None,
            ),
            "acceptance_authority": "GitHub Software CI",
        }
    ]
    for evidence_id in (
        "two_board_discovery",
        "complementary_roles",
        "bidirectional_benchmark_simulation_off",
        "traced_drill",
    ):
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
            },
            "accepted_integration_candidate": {
                **_pr_identity(integrated_pr),
                "integrated_in_pinned_revision": integrated,
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
        issue = _gh_json("issue", FS3_ISSUE)
    except (GateError, json.JSONDecodeError) as error:
        tracker_error = str(error)
        issue = {}
        blockers.append(f"live tracker access is unresolved: {error}")

    ledger: dict[str, Any] = {"result": "BLOCKED", "error": tracker_error}
    fs3: dict[str, Any] = {"result": "BLOCKED", "error": tracker_error}
    if not tracker_error:
        ledger, found = _ledger_and_integration(prs)
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
            "downstream_anticipated_diff": [],
            "protected_paths": [],
            "hardware_operations": [],
            "result": "PASS",
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
