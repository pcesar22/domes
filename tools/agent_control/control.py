#!/usr/bin/env python3
"""Deterministic GitHub/Codex control plane for DOMES agent work."""

from __future__ import annotations

import argparse
import concurrent.futures
import fcntl
import fnmatch
import hashlib
import json
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_PATH = ROOT / "WORKFLOW.md"
ORCHESTRATION_DIR = ROOT / ".codex" / "orchestration"
AUTOPILOT_POLICY_PATH = ORCHESTRATION_DIR / "autopilot-policy.json"
STATE_PREFIX = "agent:"
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
ISSUE_REFERENCE = re.compile(r"(?<![\w/])#([1-9][0-9]*)\b")
HEADING = re.compile(r"^#{2,3}\s+(.+?)\s*$")
GATED_EXEC = """\
import os
import sys

gate = int(sys.argv[1])
try:
    released = os.read(gate, 1)
finally:
    os.close(gate)
if released != b"1":
    raise SystemExit(125)
os.execvp(sys.argv[2], sys.argv[2:])
"""
REQUIRED_SECTIONS = (
    "Specification revision",
    "Parent objective",
    "Goal",
    "Non-goals",
    "Required behavior",
    "Acceptance checks",
    "Allowed architectural surfaces",
    "Dependencies",
    "Required proof",
)
ROLE_BY_STATE = {
    "agent:plan": "planner",
    "agent:ready": "worker",
    "agent:running": "worker",
    "agent:rework": "worker",
    "agent:agent-review": "judge",
    "agent:verification": "verification-worker",
}
SCHEMA_BY_ROLE = {
    "selector": "selector-result.schema.json",
    "planner": "planner-result.schema.json",
    "worker": "worker-result.schema.json",
    "judge": "judge-result.schema.json",
    "verification-worker": "verification-result.schema.json",
}
NEXT_STATE = {
    "planner": "agent:plan-review",
    "worker": "agent:agent-review",
}
MANAGED_LABELS = {
    "agent:needs-specification": (
        "BFD4F2",
        "Agent task needs an accepted specification",
    ),
    "agent:plan": ("D4C5F9", "Ready for disposable planner execution"),
    "agent:plan-review": ("C5DEF5", "Proposed task DAG awaits steward review"),
    "agent:ready": ("0E8A16", "Accepted and eligible for worker dispatch"),
    "agent:running": ("1D76DB", "Implementation worker owns the issue"),
    "agent:rework": ("D93F0B", "Independent judge requires implementation rework"),
    "agent:agent-review": (
        "5319E7",
        "Implementation awaits independent agent judgment",
    ),
    "agent:ci-pending": (
        "1D76DB",
        "Independent approval recorded; controller is reconciling exact-head CI",
    ),
    "agent:verification": ("006B75", "Judge-approved work awaits CI verification"),
    "agent:human-review": ("FBCA04", "Agent workflow complete; human review boundary"),
    "agent:blocked": ("B60205", "External condition blocks further agent progress"),
    "agent:done": ("6F42C1", "Agent task reached its accepted terminal state"),
    "priority:p0": ("B60205", "Highest dispatch priority"),
    "priority:p1": ("D93F0B", "High dispatch priority"),
    "priority:p2": ("FBCA04", "Normal dispatch priority"),
    "priority:p3": ("C2E0C6", "Low dispatch priority"),
}

AUTOPILOT_ACTIVE_STATES = frozenset(
    {
        "agent:plan",
        "agent:ready",
        "agent:running",
        "agent:rework",
        "agent:agent-review",
        "agent:ci-pending",
        "agent:verification",
    }
)
AUTOPILOT_MARKER_RE = re.compile(
    r"<!-- domes-autopilot-contract:v1 digest=([0-9a-f]{64}) -->"
)
AUTOPILOT_BLOCK_RE = re.compile(
    r"\n?<!-- domes-autopilot:start -->.*?<!-- domes-autopilot:end -->\n?",
    re.DOTALL,
)
PLAN_TASK_MARKER_RE = re.compile(
    r"<!-- domes-autopilot-task:v1 parent=([0-9]+) plan=([0-9a-f]{64}) "
    r"key=([^ ]+) uid=([0-9a-f]{64}) -->"
)
SELECTOR_COOLDOWN_SECONDS = 600


class ControlError(RuntimeError):
    """A deterministic validation or control-plane failure."""


class TrackerError(ControlError):
    """A read-side tracker failure that is safe to retry without state mutation."""


@dataclass(frozen=True)
class Workflow:
    repository: str
    state_prefix: str
    scheduler_host: str
    max_concurrent_workers: int
    workspace_root: Path
    base_branch: str
    poll_interval_seconds: int
    stall_timeout_seconds: int
    max_retry_backoff_seconds: int


@dataclass(frozen=True)
class AutopilotPolicy:
    schema_version: int
    policy_name: str
    allowed_work_classes: tuple[str, ...]
    required_ci_checks: tuple[str, ...]
    forbidden_auto_merge_paths: tuple[str, ...]
    max_ci_repair_cycles: int
    merge_method: str


@dataclass(frozen=True)
class PullRequest:
    number: int
    state: str
    is_draft: bool
    base_ref: str
    base_oid: str
    head_ref: str
    head_oid: str
    mergeable: str
    merge_state: str
    review_decision: str
    files: tuple[str, ...]
    checks: tuple[dict[str, str], ...]
    merge_commit: str = ""


@dataclass(frozen=True)
class Ticket:
    number: int
    title: str
    body: str
    state: str
    labels: tuple[str, ...]
    url: str = ""

    @classmethod
    def from_json(cls, document: dict[str, Any]) -> "Ticket":
        labels = tuple(
            label["name"] if isinstance(label, dict) else str(label)
            for label in document.get("labels", [])
        )
        return cls(
            number=int(document["number"]),
            title=str(document.get("title", "")),
            body=str(document.get("body", "")),
            state=str(document.get("state", "OPEN")).upper(),
            labels=labels,
            url=str(document.get("url", "")),
        )

    @property
    def agent_labels(self) -> tuple[str, ...]:
        return tuple(label for label in self.labels if label.startswith(STATE_PREFIX))

    @property
    def agent_state(self) -> str | None:
        return self.agent_labels[0] if len(self.agent_labels) == 1 else None

    @property
    def priority(self) -> int:
        priorities = []
        for label in self.labels:
            match = re.fullmatch(r"priority:p([0-9]+)", label)
            if match:
                priorities.append(int(match.group(1)))
        return min(priorities, default=99)


@dataclass(frozen=True)
class TicketValidation:
    ticket: Ticket
    sections: dict[str, str]
    errors: tuple[str, ...]
    dependencies: tuple[int, ...]
    source_state: str | None = None

    @property
    def valid(self) -> bool:
        return not self.errors


def _scalar(value: str) -> Any:
    value = value.strip()
    if value.isdigit():
        return int(value)
    if value.casefold() in {"true", "false"}:
        return value.casefold() == "true"
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
        return value[1:-1]
    return value


def load_workflow(path: Path = WORKFLOW_PATH) -> Workflow:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    if not lines or lines[0] != "---":
        raise ControlError(f"{path}: missing YAML front matter")
    try:
        end = lines.index("---", 1)
    except ValueError as error:
        raise ControlError(f"{path}: unterminated YAML front matter") from error
    config: dict[str, Any] = {}
    for line_number, line in enumerate(lines[1:end], 2):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if ":" not in line:
            raise ControlError(f"{path}:{line_number}: expected key: value")
        key, value = line.split(":", 1)
        if not key.strip() or not value.strip():
            raise ControlError(f"{path}:{line_number}: empty key or value")
        config[key.strip()] = _scalar(value)
    required = {
        "schema_version",
        "tracker_kind",
        "repository",
        "state_prefix",
        "scheduler_host",
        "max_concurrent_workers",
        "workspace_root",
        "base_branch",
        "poll_interval_seconds",
        "stall_timeout_seconds",
        "max_retry_backoff_seconds",
    }
    missing = sorted(required - config.keys())
    if missing:
        raise ControlError(f"{path}: missing workflow keys: {', '.join(missing)}")
    if config["schema_version"] != 1 or config["tracker_kind"] != "github":
        raise ControlError(f"{path}: unsupported schema or tracker")
    maximum = config["max_concurrent_workers"]
    if not isinstance(maximum, int) or not 1 <= maximum <= 16:
        raise ControlError(f"{path}: max_concurrent_workers must be between 1 and 16")
    for key in (
        "poll_interval_seconds",
        "stall_timeout_seconds",
        "max_retry_backoff_seconds",
    ):
        if not isinstance(config[key], int) or config[key] < 1:
            raise ControlError(f"{path}: {key} must be a positive integer")
    return Workflow(
        repository=str(config["repository"]),
        state_prefix=str(config["state_prefix"]),
        scheduler_host=str(config["scheduler_host"]),
        max_concurrent_workers=maximum,
        workspace_root=ROOT / str(config["workspace_root"]),
        base_branch=str(config["base_branch"]),
        poll_interval_seconds=config["poll_interval_seconds"],
        stall_timeout_seconds=config["stall_timeout_seconds"],
        max_retry_backoff_seconds=config["max_retry_backoff_seconds"],
    )


def load_autopilot_policy(
    path: Path = AUTOPILOT_POLICY_PATH,
) -> AutopilotPolicy:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ControlError(f"{path}: invalid autopilot policy: {error}") from error
    required = {
        "schema_version",
        "policy_name",
        "allowed_work_classes",
        "required_ci_checks",
        "forbidden_auto_merge_paths",
        "max_ci_repair_cycles",
        "merge_method",
    }
    if not isinstance(document, dict) or set(document) != required:
        raise ControlError(f"{path}: autopilot policy keys do not match schema")
    if document["schema_version"] != 1:
        raise ControlError(f"{path}: unsupported autopilot policy schema")
    for key in (
        "allowed_work_classes",
        "required_ci_checks",
        "forbidden_auto_merge_paths",
    ):
        values = document[key]
        if (
            not isinstance(values, list)
            or not values
            or any(not isinstance(value, str) or not value.strip() for value in values)
            or len(values) != len(set(values))
        ):
            raise ControlError(f"{path}: {key} must be unique non-empty strings")
    repairs = document["max_ci_repair_cycles"]
    if (
        not isinstance(repairs, int)
        or isinstance(repairs, bool)
        or not 1 <= repairs <= 10
    ):
        raise ControlError(f"{path}: max_ci_repair_cycles must be between 1 and 10")
    merge_method = document["merge_method"]
    if merge_method not in {"merge", "squash", "rebase"}:
        raise ControlError(f"{path}: unsupported merge method")
    policy_name = document["policy_name"]
    if not isinstance(policy_name, str) or not policy_name.strip():
        raise ControlError(f"{path}: policy_name must be a non-empty string")
    return AutopilotPolicy(
        schema_version=1,
        policy_name=policy_name,
        allowed_work_classes=tuple(document["allowed_work_classes"]),
        required_ci_checks=tuple(document["required_ci_checks"]),
        forbidden_auto_merge_paths=tuple(document["forbidden_auto_merge_paths"]),
        max_ci_repair_cycles=repairs,
        merge_method=merge_method,
    )


def parse_sections(body: str) -> dict[str, str]:
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in body.splitlines():
        if line.strip().startswith("<!-- domes-"):
            continue
        match = HEADING.match(line)
        if match:
            current = match.group(1).strip()
            sections.setdefault(current, [])
        elif current is not None:
            sections[current].append(line)
    return {name: "\n".join(lines).strip() for name, lines in sections.items()}


def autonomy_policy(sections: dict[str, str]) -> str:
    return sections.get("Autonomy policy", "review-only").strip().casefold()


def existing_pull_request(sections: dict[str, str]) -> int:
    value = sections.get("Existing pull request", "").strip()
    if not value or value.casefold() == "none":
        return 0
    match = re.fullmatch(r"#?([1-9][0-9]*)", value)
    if not match:
        raise ControlError("Existing pull request must be `None` or one PR number")
    return int(match.group(1))


def _contract_digest_payload(sections: dict[str, str]) -> dict[str, str]:
    names = (
        *REQUIRED_SECTIONS,
        "Work package",
        "Work class",
        "Autonomy policy",
        "Existing pull request",
    )
    return {name: sections.get(name, "").strip() for name in names}


def contract_digest(sections: dict[str, str]) -> str:
    canonical = json.dumps(
        _contract_digest_payload(sections), sort_keys=True, separators=(",", ":")
    )
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def has_valid_autopilot_marker(ticket: Ticket, sections: dict[str, str]) -> bool:
    match = AUTOPILOT_MARKER_RE.search(ticket.body)
    return match is not None and match.group(1) == contract_digest(sections)


def validate_ticket(ticket: Ticket, *, check_revision: bool = True) -> TicketValidation:
    sections = parse_sections(ticket.body)
    errors: list[str] = []
    if len(ticket.agent_labels) != 1:
        errors.append(
            f"expected exactly one {STATE_PREFIX} state label; found {len(ticket.agent_labels)}"
        )
    for name in REQUIRED_SECTIONS:
        if not sections.get(name, "").strip():
            errors.append(f"missing or empty section: {name}")
    revision = sections.get("Specification revision", "").strip()
    if revision and not FULL_SHA.fullmatch(revision):
        errors.append(
            "Specification revision must be one full lowercase 40-character commit SHA"
        )
    elif revision and check_revision:
        resolved = subprocess.run(
            ["git", "cat-file", "-e", f"{revision}^{{commit}}"],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        if resolved.returncode != 0:
            errors.append(
                f"Specification revision is not available locally: {revision}"
            )
        else:
            trusted = subprocess.run(
                ["git", "merge-base", "--is-ancestor", revision, "origin/main"],
                cwd=ROOT,
                check=False,
                capture_output=True,
                text=True,
            )
            if trusted.returncode != 0:
                errors.append(
                    "Specification revision must be reachable from origin/main"
                )
    dependency_text = sections.get("Dependencies", "")
    dependencies = tuple(
        sorted({int(value) for value in ISSUE_REFERENCE.findall(dependency_text)})
    )
    if dependency_text and dependency_text.casefold() != "none" and not dependencies:
        errors.append(
            "Dependencies must be `None` or contain GitHub issue references such as #123"
        )
    if ticket.number in dependencies:
        errors.append("ticket cannot depend on itself")
    try:
        allowed_surfaces(sections.get("Allowed architectural surfaces", ""))
    except ControlError as error:
        errors.append(str(error))
    policy = autonomy_policy(sections)
    if policy not in {"review-only", "software-auto-merge"}:
        errors.append("Autonomy policy must be `review-only` or `software-auto-merge`")
    if policy == "software-auto-merge":
        if not sections.get("Work package", "").strip():
            errors.append("software-auto-merge requires Work package")
        if sections.get("Work class", "").strip() not in {
            "software",
            "executed-validation",
        }:
            errors.append(
                "software-auto-merge requires software or executed-validation Work class"
            )
        if not has_valid_autopilot_marker(ticket, sections):
            errors.append(
                "software-auto-merge requires a valid controller contract marker"
            )
    try:
        existing_pull_request(sections)
    except ControlError as error:
        errors.append(str(error))
    return TicketValidation(ticket, sections, tuple(errors), dependencies)


def allowed_surfaces(value: str) -> tuple[str, ...]:
    surfaces: list[str] = []
    for raw_line in value.splitlines():
        line = re.sub(r"^\s*(?:[-*+]\s+|[0-9]+[.)]\s+)", "", raw_line).strip()
        line = line.strip("`")
        if not line:
            continue
        path = Path(line)
        if (
            path.is_absolute()
            or line.startswith("~")
            or ".." in path.parts
            or any(character.isspace() for character in line)
        ):
            raise ControlError(
                "Allowed architectural surfaces must contain one relative path or glob per line"
            )
        surfaces.append(line.rstrip("/"))
    if not surfaces:
        raise ControlError(
            "Allowed architectural surfaces must name at least one repository path"
        )
    return tuple(sorted(set(surfaces)))


def path_matches(path: str, pattern: str) -> bool:
    return (
        fnmatch.fnmatchcase(path, pattern)
        or (
            pattern.startswith("**/")
            and fnmatch.fnmatchcase(path, pattern.removeprefix("**/"))
        )
        or path == pattern
        or (
            not any(character in pattern for character in "*?[")
            and path.startswith(f"{pattern}/")
        )
    )


def paths_outside_surfaces(paths: Sequence[str], surfaces: Sequence[str]) -> list[str]:
    return sorted(
        path
        for path in paths
        if not any(path_matches(path, pattern) for pattern in surfaces)
    )


def forbidden_auto_merge_paths(
    paths: Sequence[str], policy: AutopilotPolicy
) -> list[str]:
    return sorted(
        path
        for path in paths
        if any(
            path_matches(path, pattern) for pattern in policy.forbidden_auto_merge_paths
        )
    )


def forbidden_auto_merge_surfaces(
    surfaces: Sequence[str], policy: AutopilotPolicy
) -> list[str]:
    tracked = _git("ls-files")
    tracked_paths = tracked.stdout.splitlines() if tracked.returncode == 0 else []
    forbidden_paths = forbidden_auto_merge_paths(tracked_paths, policy)
    return sorted(
        surface
        for surface in surfaces
        if not _static_prefix(surface)
        or any(path_matches(path, surface) for path in forbidden_paths)
        or any(
            _static_prefix(forbidden) and surfaces_overlap((surface,), (forbidden,))
            for forbidden in policy.forbidden_auto_merge_paths
        )
    )


def surface_within(child: str, parent: str) -> bool:
    child_prefix = _static_prefix(child)
    parent_prefix = _static_prefix(parent)
    if not child_prefix or not parent_prefix:
        return child == parent
    if not (
        child_prefix == parent_prefix or child_prefix.startswith(f"{parent_prefix}/")
    ):
        return False
    if any(character in parent for character in "*?["):
        return True
    return child == parent or child.startswith(f"{parent}/")


def surfaces_within(child: Sequence[str], parent: Sequence[str]) -> bool:
    return all(any(surface_within(value, root) for root in parent) for value in child)


def _static_prefix(pattern: str) -> str:
    wildcard_positions = [
        position for character in "*?[" if (position := pattern.find(character)) >= 0
    ]
    prefix = pattern[: min(wildcard_positions)] if wildcard_positions else pattern
    return prefix.rstrip("/")


def surfaces_overlap(first: Sequence[str], second: Sequence[str]) -> bool:
    for left in first:
        left_prefix = _static_prefix(left)
        for right in second:
            right_prefix = _static_prefix(right)
            if not left_prefix or not right_prefix:
                return True
            if (
                left_prefix == right_prefix
                or left_prefix.startswith(f"{right_prefix}/")
                or right_prefix.startswith(f"{left_prefix}/")
            ):
                return True
    return False


def select_non_overlapping(
    eligible: Sequence[TicketValidation],
    maximum: int,
    reserved_surfaces: Sequence[Sequence[str]] = (),
) -> list[TicketValidation]:
    if maximum <= 0:
        return []
    selected: list[TicketValidation] = []
    selected_surfaces = [tuple(surfaces) for surfaces in reserved_surfaces]
    for item in eligible:
        surfaces = allowed_surfaces(item.sections["Allowed architectural surfaces"])
        if any(surfaces_overlap(surfaces, existing) for existing in selected_surfaces):
            continue
        selected.append(item)
        selected_surfaces.append(surfaces)
        if len(selected) == maximum:
            break
    return selected


def dependency_cycles(validations: Iterable[TicketValidation]) -> list[list[int]]:
    graph = {item.ticket.number: item.dependencies for item in validations}
    cycles: set[tuple[int, ...]] = set()

    def visit(node: int, path: list[int], active: set[int]) -> None:
        if node in active:
            start = path.index(node)
            cycle = path[start:]
            rotations = [
                tuple(cycle[index:] + cycle[:index]) for index in range(len(cycle))
            ]
            cycles.add(min(rotations))
            return
        if node not in graph:
            return
        active.add(node)
        path.append(node)
        for dependency in graph[node]:
            visit(dependency, path, active)
        path.pop()
        active.remove(node)

    for number in graph:
        visit(number, [], set())
    return [list(cycle) for cycle in sorted(cycles)]


def terminal(ticket: Ticket) -> bool:
    return ticket.state == "CLOSED" or ticket.agent_state == "agent:done"


def eligible_queue(
    tickets: Sequence[Ticket], *, check_revision: bool = True
) -> tuple[list[TicketValidation], dict[int, list[str]]]:
    validations = [
        validate_ticket(ticket, check_revision=check_revision) for ticket in tickets
    ]
    by_number = {ticket.number: ticket for ticket in tickets}
    blockers: dict[int, list[str]] = {}
    cycle_nodes = {
        number for cycle in dependency_cycles(validations) for number in cycle
    }
    eligible: list[TicketValidation] = []
    for item in validations:
        ticket = item.ticket
        reasons = list(item.errors)
        if ticket.state != "OPEN":
            continue
        if ticket.agent_state not in ROLE_BY_STATE:
            continue
        if ticket.number in cycle_nodes:
            reasons.append("dependency cycle")
        for dependency in item.dependencies:
            target = by_number.get(dependency)
            if target is None:
                reasons.append(
                    f"dependency #{dependency} was not returned by the tracker"
                )
            elif not terminal(target):
                reasons.append(f"dependency #{dependency} is not terminal")
        if reasons:
            blockers[ticket.number] = reasons
        else:
            eligible.append(item)
    eligible.sort(key=lambda item: (item.ticket.priority, item.ticket.number))
    return eligible, blockers


def _run_json(command: Sequence[str], *, cwd: Path = ROOT) -> Any:
    result = subprocess.run(
        command, cwd=cwd, check=False, capture_output=True, text=True
    )
    if result.returncode != 0:
        detail = (
            result.stderr.strip()
            or result.stdout.strip()
            or f"exit {result.returncode}"
        )
        raise TrackerError(f"command failed: {' '.join(command)}: {detail}")
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise TrackerError(
            f"command returned invalid JSON: {' '.join(command)}"
        ) from error


def load_live_tickets(workflow: Workflow) -> list[Ticket]:
    documents = _run_json(
        [
            "gh",
            "issue",
            "list",
            "--repo",
            workflow.repository,
            "--state",
            "all",
            "--limit",
            "1000",
            "--json",
            "number,title,body,state,labels,url",
        ]
    )
    return [Ticket.from_json(document) for document in documents]


def pull_request_from_json(document: dict[str, Any]) -> PullRequest:
    checks: list[dict[str, str]] = []
    for check in document.get("statusCheckRollup") or []:
        status = str(check.get("status") or check.get("state") or "").upper()
        conclusion = str(check.get("conclusion") or "").upper()
        state = conclusion if status == "COMPLETED" and conclusion else status
        checks.append(
            {
                "name": str(check.get("name") or check.get("context") or ""),
                "state": state,
                "url": str(check.get("detailsUrl") or check.get("targetUrl") or ""),
            }
        )
    merge_commit = document.get("mergeCommit") or {}
    return PullRequest(
        number=int(document["number"]),
        state=str(document.get("state", "")).upper(),
        is_draft=bool(document.get("isDraft", False)),
        base_ref=str(document.get("baseRefName", "")),
        base_oid=str(document.get("baseRefOid", "")),
        head_ref=str(document.get("headRefName", "")),
        head_oid=str(document.get("headRefOid", "")),
        mergeable=str(document.get("mergeable", "")).upper(),
        merge_state=str(document.get("mergeStateStatus", "")).upper(),
        review_decision=str(document.get("reviewDecision", "")).upper(),
        files=tuple(str(item.get("path", "")) for item in document.get("files") or []),
        checks=tuple(checks),
        merge_commit=(
            str(merge_commit.get("oid", "")) if isinstance(merge_commit, dict) else ""
        ),
    )


def load_pull_request(workflow: Workflow, number: int) -> PullRequest:
    document = _run_json(
        [
            "gh",
            "pr",
            "view",
            str(number),
            "--repo",
            workflow.repository,
            "--json",
            (
                "number,state,isDraft,baseRefName,baseRefOid,headRefName,headRefOid,"
                "mergeable,mergeStateStatus,reviewDecision,files,statusCheckRollup,"
                "mergeCommit,changedFiles"
            ),
        ]
    )
    pages = _run_json(
        [
            "gh",
            "api",
            "--paginate",
            "--slurp",
            f"repos/{workflow.repository}/pulls/{number}/files?per_page=100",
        ]
    )
    if not isinstance(pages, list) or any(not isinstance(page, list) for page in pages):
        raise TrackerError(
            f"PR #{number}: changed-file pagination returned invalid data"
        )
    filenames = [
        str(file.get("filename", ""))
        for page in pages
        for file in page
        if isinstance(file, dict)
    ]
    expected_files = document.get("changedFiles")
    if (
        not isinstance(expected_files, int)
        or expected_files < 0
        or len(filenames) != expected_files
        or len(filenames) != len(set(filenames))
        or any(not filename for filename in filenames)
    ):
        raise TrackerError(f"PR #{number}: changed-file list is incomplete")
    document["files"] = [{"path": filename} for filename in filenames]
    return pull_request_from_json(document)


def load_open_pull_request_snapshot(workflow: Workflow) -> list[dict[str, Any]]:
    documents = _run_json(
        [
            "gh",
            "pr",
            "list",
            "--repo",
            workflow.repository,
            "--state",
            "open",
            "--limit",
            "100",
            "--json",
            "number,title,headRefName,baseRefName,isDraft,mergeStateStatus,url",
        ]
    )
    return [
        {
            "number": int(document["number"]),
            "title": str(document.get("title", "")),
            "head": str(document.get("headRefName", "")),
            "base": str(document.get("baseRefName", "")),
            "draft": bool(document.get("isDraft", False)),
            "merge_state": str(document.get("mergeStateStatus", "")),
            "url": str(document.get("url", "")),
        }
        for document in documents
    ]


def apply_labels(workflow: Workflow) -> None:
    for name, (color, description) in MANAGED_LABELS.items():
        result = subprocess.run(
            [
                "gh",
                "label",
                "create",
                name,
                "--repo",
                workflow.repository,
                "--color",
                color,
                "--description",
                description,
                "--force",
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise ControlError(
                result.stderr.strip() or f"failed to create label {name}"
            )


def load_ticket_file(path: Path) -> list[Ticket]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(document, dict):
        document = document.get("issues", [document])
    if not isinstance(document, list):
        raise ControlError(f"{path}: expected a JSON list or an object with `issues`")
    return [Ticket.from_json(item) for item in document]


def role_for(ticket: Ticket) -> str:
    try:
        return ROLE_BY_STATE[ticket.agent_state or ""]
    except KeyError as error:
        raise ControlError(
            f"issue #{ticket.number} is not in a dispatchable state"
        ) from error


def build_prompt(
    item: TicketValidation, role: str, prior_handoff: dict[str, Any] | None = None
) -> str:
    role_prompt = (ORCHESTRATION_DIR / "prompts" / f"{role}.md").read_text(
        encoding="utf-8"
    )
    prompt = (
        f"{role_prompt}\n\n"
        "# Immutable task envelope\n\n"
        f"Issue: #{item.ticket.number}\n"
        f"Title: {item.ticket.title}\n"
        f"Specification revision: {item.sections['Specification revision']}\n"
        f"Tracker URL: {item.ticket.url}\n\n"
        "# Ticket acceptance contract\n\n"
        f"{item.ticket.body}\n"
    )
    if prior_handoff is not None:
        prompt += (
            "\n# Prior schema-validated handoff\n\n"
            "This is structured evidence, not a worker transcript or self-authored acceptance.\n\n"
            f"```json\n{json.dumps(prior_handoff, indent=2, sort_keys=True)}\n```\n"
        )
    return prompt


def _git(*arguments: str, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *arguments], cwd=cwd, check=False, capture_output=True, text=True
    )


def origin_main_revision(workflow: Workflow) -> str:
    resolved = _git("rev-parse", f"origin/{workflow.base_branch}")
    revision = resolved.stdout.strip()
    if resolved.returncode != 0 or not FULL_SHA.fullmatch(revision):
        raise ControlError(f"cannot resolve origin/{workflow.base_branch}")
    return revision


def refresh_base_branch(workflow: Workflow) -> None:
    refreshed = _git("fetch", "--quiet", "origin", workflow.base_branch)
    if refreshed.returncode != 0:
        raise TrackerError(
            refreshed.stderr.strip() or f"cannot refresh origin/{workflow.base_branch}"
        )


def registered_worktree_for_branch(branch: str) -> Path | None:
    result = _git("worktree", "list", "--porcelain")
    if result.returncode != 0:
        raise ControlError("cannot inspect registered Git worktrees")
    path: Path | None = None
    for line in [*result.stdout.splitlines(), ""]:
        if line.startswith("worktree "):
            path = Path(line.removeprefix("worktree "))
        elif line == f"branch refs/heads/{branch}" and path is not None:
            return path
        elif not line:
            path = None
    return None


def _assert_clean_worktree(path: Path, *, issue: int) -> None:
    check = _git("rev-parse", "--is-inside-work-tree", cwd=path)
    if check.returncode != 0 or check.stdout.strip() != "true":
        raise ControlError(f"issue #{issue}: refusing non-worktree workspace: {path}")
    dirty = _git("status", "--porcelain", "--untracked-files=all", cwd=path)
    if dirty.returncode != 0 or dirty.stdout.strip():
        raise ControlError(
            f"issue #{issue}: existing pull-request worktree has uncommitted changes"
        )


def ensure_workspace(workflow: Workflow, item: TicketValidation, role: str) -> Path:
    workspace = workflow.workspace_root / f"issue-{item.ticket.number}"
    pull_request_number = existing_pull_request(item.sections)
    if pull_request_number:
        pull_request = load_pull_request(workflow, pull_request_number)
        if (
            pull_request.state != "OPEN"
            or pull_request.base_ref != workflow.base_branch
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: existing PR must be open against "
                f"{workflow.base_branch}"
            )
        registered = registered_worktree_for_branch(pull_request.head_ref)
        if registered is not None:
            _assert_clean_worktree(registered, issue=item.ticket.number)
            return registered
        branch_exists = _git(
            "show-ref", "--verify", "--quiet", f"refs/heads/{pull_request.head_ref}"
        )
        workflow.workspace_root.mkdir(parents=True, exist_ok=True)
        if branch_exists.returncode == 0:
            command = ["worktree", "add", str(workspace), pull_request.head_ref]
        else:
            fetch = _git("fetch", "origin", pull_request.head_ref)
            if fetch.returncode != 0:
                raise ControlError(
                    f"issue #{item.ticket.number}: cannot fetch existing PR branch"
                )
            command = [
                "worktree",
                "add",
                "-b",
                pull_request.head_ref,
                str(workspace),
                f"origin/{pull_request.head_ref}",
            ]
        created = _git(*command)
        if created.returncode != 0:
            raise ControlError(
                created.stderr.strip() or f"failed to create {workspace}"
            )
        return workspace
    if workspace.exists():
        _assert_clean_worktree(workspace, issue=item.ticket.number)
        if role in {"worker", "verification-worker"}:
            branch = _git("branch", "--show-current", cwd=workspace)
            expected = f"codex/issue-{item.ticket.number}"
            if branch.returncode != 0 or branch.stdout.strip() != expected:
                raise ControlError(
                    f"issue #{item.ticket.number}: expected workspace branch {expected}"
                )
        return workspace
    workflow.workspace_root.mkdir(parents=True, exist_ok=True)
    if role in {"worker", "verification-worker"}:
        branch = f"codex/issue-{item.ticket.number}"
        branch_exists = _git("show-ref", "--verify", "--quiet", f"refs/heads/{branch}")
        if branch_exists.returncode == 0:
            command = ["worktree", "add", str(workspace), branch]
        else:
            command = [
                "worktree",
                "add",
                "-b",
                branch,
                str(workspace),
                f"origin/{workflow.base_branch}",
            ]
    else:
        command = [
            "worktree",
            "add",
            "--detach",
            str(workspace),
            item.sections["Specification revision"],
        ]
    created = _git(*command)
    if created.returncode != 0:
        raise ControlError(created.stderr.strip() or f"failed to create {workspace}")
    return workspace


def transition(workflow: Workflow, ticket: Ticket, new_state: str) -> None:
    command = ["gh", "issue", "edit", str(ticket.number), "--repo", workflow.repository]
    for label in ticket.agent_labels:
        command.extend(("--remove-label", label))
    command.extend(("--add-label", new_state))
    result = subprocess.run(
        command, cwd=ROOT, check=False, capture_output=True, text=True
    )
    if result.returncode != 0:
        raise ControlError(
            result.stderr.strip() or f"failed to transition issue #{ticket.number}"
        )


def update_issue_body(workflow: Workflow, ticket: Ticket, body: str) -> None:
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix="issue-body-", suffix=".md"
    ) as stream:
        stream.write(body)
        stream.flush()
        result = subprocess.run(
            [
                "gh",
                "issue",
                "edit",
                str(ticket.number),
                "--repo",
                workflow.repository,
                "--body-file",
                stream.name,
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
    if result.returncode != 0:
        raise ControlError(
            result.stderr.strip() or f"failed to update issue #{ticket.number}"
        )


def bind_ticket_pull_request(
    workflow: Workflow, item: TicketValidation, pull_request_number: int
) -> None:
    if existing_pull_request(item.sections):
        return
    if not has_valid_autopilot_marker(item.ticket, item.sections):
        raise ControlError(
            f"issue #{item.ticket.number}: cannot bind PR to an invalid contract"
        )
    pattern = re.compile(
        r"(## Existing pull request\s*\n\n).*?"
        r"(?=\n\n## |\n\n<!-- domes-autopilot:end -->)",
        re.DOTALL,
    )
    body, substitutions = pattern.subn(
        rf"\g<1>#{pull_request_number}", item.ticket.body, count=1
    )
    if substitutions != 1:
        raise ControlError(
            f"issue #{item.ticket.number}: cannot locate PR contract field"
        )
    digest = contract_digest(parse_sections(body))
    body, markers = AUTOPILOT_MARKER_RE.subn(
        f"<!-- domes-autopilot-contract:v1 digest={digest} -->", body, count=1
    )
    if markers != 1:
        raise ControlError(
            f"issue #{item.ticket.number}: cannot refresh PR contract marker"
        )
    update_issue_body(workflow, item.ticket, body)


def set_issue_priority(workflow: Workflow, ticket: Ticket, priority: str) -> None:
    command = ["gh", "issue", "edit", str(ticket.number), "--repo", workflow.repository]
    for label in ticket.labels:
        if re.fullmatch(r"priority:p[0-9]+", label):
            command.extend(("--remove-label", label))
    command.extend(("--add-label", f"priority:{priority}"))
    result = subprocess.run(
        command, cwd=ROOT, check=False, capture_output=True, text=True
    )
    if result.returncode != 0:
        raise ControlError(
            result.stderr.strip() or f"failed to prioritize issue #{ticket.number}"
        )


def close_issue(workflow: Workflow, number: int) -> None:
    result = subprocess.run(
        [
            "gh",
            "issue",
            "close",
            str(number),
            "--repo",
            workflow.repository,
            "--reason",
            "completed",
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ControlError(result.stderr.strip() or f"failed to close issue #{number}")


def complete_issue(workflow: Workflow, ticket: Ticket) -> None:
    labels = sorted(
        {
            *(label for label in ticket.labels if not label.startswith(STATE_PREFIX)),
            "agent:done",
        }
    )
    result = subprocess.run(
        [
            "gh",
            "api",
            "--method",
            "PATCH",
            f"repos/{workflow.repository}/issues/{ticket.number}",
            "--input",
            "-",
            "--silent",
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        input=json.dumps(
            {"state": "closed", "state_reason": "completed", "labels": labels}
        ),
    )
    if result.returncode != 0:
        raise ControlError(
            result.stderr.strip() or f"failed to complete issue #{ticket.number}"
        )


def _list_markdown(values: Sequence[str]) -> str:
    return "\n".join(f"- {value.strip()}" for value in values if value.strip())


def render_ticket_contract(
    *,
    spec_revision: str,
    parent_objective: str,
    goal: str,
    non_goals: Sequence[str],
    required_behavior: str,
    acceptance_checks: Sequence[str],
    allowed_surface_values: Sequence[str],
    dependencies: Sequence[int],
    required_proof: Sequence[str],
    work_package: str,
    work_class: str,
    selected_policy: str,
    pull_request: int = 0,
) -> str:
    dependency_text = "\n".join(f"- #{number}" for number in dependencies) or "None"
    values = {
        "Specification revision": spec_revision,
        "Parent objective": parent_objective.strip(),
        "Goal": goal.strip(),
        "Non-goals": _list_markdown(non_goals),
        "Required behavior": required_behavior.strip(),
        "Acceptance checks": _list_markdown(acceptance_checks),
        "Allowed architectural surfaces": _list_markdown(allowed_surface_values),
        "Dependencies": dependency_text,
        "Required proof": _list_markdown(required_proof),
        "Work package": work_package.strip(),
        "Work class": work_class.strip(),
        "Autonomy policy": selected_policy.strip(),
        "Existing pull request": f"#{pull_request}" if pull_request else "None",
    }
    return "\n\n".join(f"## {name}\n\n{value}" for name, value in values.items())


def with_autopilot_contract(original_body: str, contract: str) -> str:
    base = AUTOPILOT_BLOCK_RE.sub("\n", original_body).rstrip()
    combined = f"{base}\n\n{contract}" if base else contract
    digest = contract_digest(parse_sections(combined))
    block = (
        "<!-- domes-autopilot:start -->\n"
        f"<!-- domes-autopilot-contract:v1 digest={digest} -->\n\n"
        f"{contract}\n\n"
        "<!-- domes-autopilot:end -->"
    )
    return f"{base}\n\n{block}\n" if base else f"{block}\n"


def validate_selector_result(
    result: dict[str, Any],
    workflow: Workflow,
    policy: AutopilotPolicy,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
) -> None:
    state = result["state"]
    if state != "selected":
        if result["mode"] != "none" or result["autonomy_policy"] != "none":
            raise ControlError("idle or blocked selector result must use `none` policy")
        return
    revision = origin_main_revision(workflow)
    if result["spec_revision"] != revision:
        raise ControlError("selector must pin the current origin/main revision")
    required_strings = (
        "work_package",
        "title",
        "parent_objective",
        "goal",
        "required_behavior",
        "rationale",
    )
    if any(not str(result[name]).strip() for name in required_strings):
        raise ControlError("selector returned an incomplete execution contract")
    for name in (
        "non_goals",
        "acceptance_checks",
        "allowed_surfaces",
        "required_proof",
    ):
        if not result[name] or any(not str(value).strip() for value in result[name]):
            raise ControlError(f"selector returned empty {name}")
    if result["mode"] not in {"execute", "plan"}:
        raise ControlError("selected work must use execute or plan mode")
    if result["work_class"] not in policy.allowed_work_classes:
        raise ControlError("selector returned a prohibited work class")
    if result["priority"] not in {"p0", "p1", "p2", "p3"}:
        raise ControlError("selector returned an invalid priority")
    if result["autonomy_policy"] not in {"software-auto-merge", "review-only"}:
        raise ControlError("selector returned an invalid autonomy policy")
    surfaces = allowed_surfaces("\n".join(result["allowed_surfaces"]))
    if result["autonomy_policy"] == "software-auto-merge":
        forbidden = forbidden_auto_merge_surfaces(surfaces, policy)
        if forbidden:
            raise ControlError(
                "selector requested auto-merge for forbidden surfaces: "
                + ", ".join(forbidden)
            )
    by_number = {ticket.number: ticket for ticket in tickets}
    issue_number = int(result["existing_issue"])
    if issue_number:
        issue = by_number.get(issue_number)
        if (
            issue is None
            or issue.state != "OPEN"
            or terminal(issue)
            or issue.agent_state
            in {
                "agent:blocked",
                "agent:human-review",
                "agent:needs-specification",
                "agent:plan-review",
            }
        ):
            raise ControlError("selector referenced an unavailable existing issue")
    for ticket in tickets:
        if ticket.number == issue_number or not AUTOPILOT_MARKER_RE.search(ticket.body):
            continue
        if (
            parse_sections(ticket.body).get("Work package", "").strip()
            == result["work_package"]
        ):
            raise ControlError(
                "selector duplicated a previously materialized work package"
            )
    for dependency in result["dependencies"]:
        if dependency not in by_number:
            raise ControlError(f"selector referenced unknown dependency #{dependency}")
    pull_request_number = int(result["existing_pull_request"])
    if pull_request_number:
        if not issue_number:
            raise ControlError("an existing pull request requires an existing issue")
        matches = [
            item for item in pull_requests if int(item["number"]) == pull_request_number
        ]
        if (
            not matches
            or matches[0]["draft"]
            or matches[0]["base"] != workflow.base_branch
        ):
            raise ControlError(
                "selector referenced an unavailable existing pull request"
            )
        if result["autonomy_policy"] == "software-auto-merge":
            pull_request = load_pull_request(workflow, pull_request_number)
            outside = paths_outside_surfaces(pull_request.files, surfaces)
            forbidden = forbidden_auto_merge_paths(pull_request.files, policy)
            if outside or forbidden:
                raise ControlError(
                    "selector existing pull request violates path policy: "
                    + ", ".join(outside or forbidden)
                )
    status = _git("show", f"origin/{workflow.base_branch}:PROGRAM_STATUS.md")
    status_text = status.stdout if status.returncode == 0 else ""
    referenced_text = "\n".join(
        [status_text]
        + [ticket.title + "\n" + ticket.body for ticket in tickets]
        + [str(item["title"]) for item in pull_requests]
    )
    if result["work_package"] not in referenced_text and not issue_number:
        raise ControlError(
            "selector work package is not present in a governing live source"
        )


def _ticket_from_selection(
    number: int, body: str, result: dict[str, Any], url: str = ""
) -> Ticket:
    state_label = "agent:ready" if result["mode"] == "execute" else "agent:plan"
    return Ticket(
        number=number,
        title=str(result["title"]),
        body=body,
        state="OPEN",
        labels=(state_label, f"priority:{result['priority']}"),
        url=url,
    )


def apply_selector_result(
    workflow: Workflow, result: dict[str, Any], tickets: Sequence[Ticket]
) -> Ticket | None:
    if result["state"] != "selected":
        return None
    contract = render_ticket_contract(
        spec_revision=result["spec_revision"],
        parent_objective=result["parent_objective"],
        goal=result["goal"],
        non_goals=result["non_goals"],
        required_behavior=result["required_behavior"],
        acceptance_checks=result["acceptance_checks"],
        allowed_surface_values=result["allowed_surfaces"],
        dependencies=result["dependencies"],
        required_proof=result["required_proof"],
        work_package=result["work_package"],
        work_class=result["work_class"],
        selected_policy=result["autonomy_policy"],
        pull_request=result["existing_pull_request"],
    )
    target_state = "agent:ready" if result["mode"] == "execute" else "agent:plan"
    existing_number = int(result["existing_issue"])
    if existing_number:
        ticket = next(ticket for ticket in tickets if ticket.number == existing_number)
        body = with_autopilot_contract(ticket.body, contract)
        validation_ticket = _ticket_from_selection(
            ticket.number, body, result, ticket.url
        )
        validation = validate_ticket(validation_ticket)
        if not validation.valid:
            raise ControlError(
                f"selector produced invalid ticket #{ticket.number}: "
                + "; ".join(validation.errors)
            )
        update_issue_body(workflow, ticket, body)
        set_issue_priority(workflow, ticket, result["priority"])
        transition(workflow, ticket, target_state)
        return validation_ticket

    body = with_autopilot_contract("", contract)
    digest = contract_digest(parse_sections(body))
    marker = f"domes-autopilot-contract:v1 digest={digest}"
    for ticket in tickets:
        if marker in ticket.body:
            return ticket
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix="issue-body-", suffix=".md"
    ) as stream:
        stream.write(body)
        stream.flush()
        created = subprocess.run(
            [
                "gh",
                "issue",
                "create",
                "--repo",
                workflow.repository,
                "--title",
                result["title"],
                "--body-file",
                stream.name,
                "--label",
                target_state,
                "--label",
                f"priority:{result['priority']}",
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
    if created.returncode != 0:
        raise ControlError(created.stderr.strip() or "failed to create selected issue")
    match = re.search(r"/issues/([1-9][0-9]*)", created.stdout)
    if not match:
        refreshed = load_live_tickets(workflow)
        matches = [ticket for ticket in refreshed if marker in ticket.body]
        if len(matches) != 1:
            raise ControlError("cannot reconcile selected issue creation")
        return matches[0]
    return _ticket_from_selection(
        int(match.group(1)), body, result, created.stdout.strip()
    )


def autopilot_queue_idle(tickets: Sequence[Ticket]) -> bool:
    return not any(
        ticket.state == "OPEN" and ticket.agent_state in AUTOPILOT_ACTIVE_STATES
        for ticket in tickets
    )


def build_selector_prompt(
    workflow: Workflow,
    policy: AutopilotPolicy,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
) -> str:
    role_prompt = (ORCHESTRATION_DIR / "prompts" / "selector.md").read_text(
        encoding="utf-8"
    )
    issue_snapshot = [
        {
            "number": ticket.number,
            "title": ticket.title,
            "state": ticket.state,
            "labels": list(ticket.labels),
            "url": ticket.url,
        }
        for ticket in tickets
        if ticket.state == "OPEN"
    ]
    return (
        f"{role_prompt}\n\n"
        "# Immutable selector envelope\n\n"
        f"Repository: {workflow.repository}\n"
        f"Current origin/main revision: {origin_main_revision(workflow)}\n"
        f"Autopilot policy: {policy.policy_name}\n"
        f"Allowed work classes: {', '.join(policy.allowed_work_classes)}\n\n"
        "# Live open issue summary\n\n"
        f"```json\n{json.dumps(issue_snapshot, indent=2, sort_keys=True)}\n```\n\n"
        "# Live open pull-request summary\n\n"
        f"```json\n{json.dumps(pull_requests, indent=2, sort_keys=True)}\n```\n"
    )


def run_selector(
    workflow: Workflow,
    policy: AutopilotPolicy,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / "selector"
    run_root.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        prefix="result-",
        suffix=".json",
        dir=run_root,
        delete=False,
    ) as stream:
        result_path = Path(stream.name)
    event_path = result_path.with_suffix(".jsonl")
    stderr_path = result_path.with_suffix(".stderr.log")
    lease_path = run_root / "active-process.json"
    command = [
        "codex",
        "exec",
        "--strict-config",
        "-c",
        'approval_policy="never"',
        "--sandbox",
        "read-only",
        "--cd",
        str(ROOT),
        "--output-schema",
        str(ORCHESTRATION_DIR / "schemas" / SCHEMA_BY_ROLE["selector"]),
        "--output-last-message",
        str(result_path),
        "--json",
        "-",
    ]
    failures: list[str] = []
    for attempt in range(1, 4):
        returncode, failure = run_codex_attempt(
            command,
            build_selector_prompt(workflow, policy, tickets, pull_requests),
            event_path,
            stderr_path,
            workflow.stall_timeout_seconds,
            lease_path,
        )
        if returncode == 0:
            break
        failures.append(failure)
        if attempt < 3:
            time.sleep(
                min(10 * (2 ** (attempt - 1)), workflow.max_retry_backoff_seconds)
            )
    else:
        raise ControlError(
            "autonomous selector failed after 3 attempts: " + "; ".join(failures)
        )
    result = json.loads(result_path.read_text(encoding="utf-8"))
    validate_selector_result(result, workflow, policy, tickets, pull_requests)
    write_handoff(run_root / "handoff-selector.json", result)
    selected = apply_selector_result(workflow, result, tickets)
    return {
        "state": result["state"],
        "issue": selected.number if selected is not None else 0,
        "work_package": result["work_package"],
        "result": str(result_path),
        "events": str(event_path),
        "stderr": str(stderr_path),
    }


def claim_for_dispatch(workflow: Workflow, item: TicketValidation) -> TicketValidation:
    if item.ticket.agent_state != "agent:ready":
        return item
    transition(workflow, item.ticket, "agent:running")
    labels = tuple(
        label for label in item.ticket.labels if not label.startswith(STATE_PREFIX)
    ) + ("agent:running",)
    ticket = Ticket(
        item.ticket.number,
        item.ticket.title,
        item.ticket.body,
        item.ticket.state,
        labels,
        item.ticket.url,
    )
    return TicketValidation(
        ticket,
        item.sections,
        item.errors,
        item.dependencies,
        source_state=item.ticket.agent_state,
    )


def result_state(
    role: str,
    result: dict[str, Any],
    *,
    autopilot: bool = False,
    ticket_sections: dict[str, str] | None = None,
) -> str:
    if role == "worker" and result["blockers"]:
        return "agent:blocked"
    if role == "planner" and result["blockers"]:
        return "agent:blocked"
    if role in NEXT_STATE:
        return NEXT_STATE[role]
    if role == "judge":
        approved_state = "agent:verification"
        if (
            autopilot
            and ticket_sections is not None
            and autonomy_policy(ticket_sections) == "software-auto-merge"
        ):
            approved_state = "agent:ci-pending"
        return {
            "approve": approved_state,
            "reject": "agent:rework",
            "blocked": "agent:blocked",
        }[result["verdict"]]
    if role == "verification-worker":
        resolved_state = "agent:human-review"
        if (
            autopilot
            and ticket_sections is not None
            and autonomy_policy(ticket_sections) == "software-auto-merge"
        ):
            resolved_state = "agent:ci-pending"
        return {
            "human_review": resolved_state,
            "agent_review": "agent:agent-review",
            "blocked": "agent:blocked",
        }[result["state"]]
    raise ControlError(f"unsupported role: {role}")


def concise_result(role: str, result: dict[str, Any]) -> str:
    if role == "planner":
        return f"Planner produced {len(result['tasks'])} task(s); blockers: {len(result['blockers'])}."
    if role == "worker":
        return (
            f"Worker returned commit `{result['commit']}` and "
            f"{len(result['verification'])} verification record(s); "
            f"blockers: {len(result['blockers'])}."
        )
    if role == "judge":
        return (
            f"Independent judge verdict: **{result['verdict']}**; "
            f"criteria reviewed: {len(result['criteria'])}; "
            f"required rework items: {len(result['required_rework'])}."
        )
    return (
        f"Verification state: **{result['state']}**; checks: {len(result['checks'])}; "
        f"repairs: {len(result['repairs'])}; blockers: {len(result['blockers'])}."
    )


def validate_result_semantics(role: str, result: dict[str, Any]) -> None:
    if role == "planner":
        tasks = result["tasks"]
        keys = [task["key"] for task in tasks]
        if not result["blockers"] and not tasks:
            raise ControlError("planner result requires at least one task or blocker")
        invalid_keys = [
            key for key in keys if not re.fullmatch(r"[A-Za-z0-9._-]+", key)
        ]
        if invalid_keys:
            raise ControlError(
                "planner task keys must use only letters, digits, dot, underscore, or dash"
            )
        if len(keys) != len(set(keys)):
            raise ControlError("planner result contains duplicate task keys")
        task_dependencies = {task["key"]: tuple(task["dependencies"]) for task in tasks}
        unknown = sorted(
            {
                dependency
                for dependencies in task_dependencies.values()
                for dependency in dependencies
                if dependency not in task_dependencies
            }
        )
        if unknown:
            raise ControlError(
                f"planner result contains unknown task dependencies: {', '.join(unknown)}"
            )
        synthetic = [
            TicketValidation(
                Ticket(index + 1, key, "", "OPEN", ("agent:plan",)),
                {},
                (),
                tuple(
                    keys.index(dependency) + 1 for dependency in task_dependencies[key]
                ),
            )
            for index, key in enumerate(keys)
        ]
        if dependency_cycles(synthetic):
            raise ControlError("planner result contains a dependency cycle")
    elif role == "worker":
        if result["blockers"] and result["state"] == "agent_review":
            return
    elif role == "judge":
        statuses = {criterion["status"] for criterion in result["criteria"]}
        if result["verdict"] == "approve" and (
            statuses != {"met"} or result["required_rework"]
        ):
            raise ControlError(
                "judge approval requires every criterion met and no rework"
            )
        if result["verdict"] == "reject" and not result["required_rework"]:
            raise ControlError("judge rejection requires at least one rework item")
    elif role == "verification-worker":
        statuses = {check["status"] for check in result["checks"]}
        if result["state"] == "human_review" and not statuses <= {"passed", "skipped"}:
            raise ControlError(
                "human review requires every verification check resolved"
            )


def normalized_plan(result: dict[str, Any]) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for task in sorted(result["tasks"], key=lambda item: item["key"]):
        normalized.append(
            {
                "key": task["key"],
                "goal": task["goal"].strip(),
                "non_goals": sorted(value.strip() for value in task["non_goals"]),
                "required_behavior": task["required_behavior"].strip(),
                "acceptance_checks": sorted(
                    value.strip() for value in task["acceptance_checks"]
                ),
                "allowed_surfaces": sorted(
                    value.strip() for value in task["allowed_surfaces"]
                ),
                "dependencies": sorted(task["dependencies"]),
                "required_proof": sorted(
                    value.strip() for value in task["required_proof"]
                ),
                "autonomy_policy": task["autonomy_policy"],
            }
        )
    return normalized


def plan_digest(result: dict[str, Any]) -> str:
    document = {
        "issue": result["issue"],
        "spec_revision": result["spec_revision"],
        "tasks": normalized_plan(result),
    }
    canonical = json.dumps(document, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def topological_tasks(tasks: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    by_key = {task["key"]: task for task in tasks}
    remaining = set(by_key)
    ordered: list[dict[str, Any]] = []
    while remaining:
        ready = sorted(
            key
            for key in remaining
            if not (set(by_key[key]["dependencies"]) & remaining)
        )
        if not ready:
            raise ControlError("planner result contains a dependency cycle")
        for key in ready:
            ordered.append(by_key[key])
            remaining.remove(key)
    return ordered


def task_uid(parent: TicketValidation, plan_hash: str, task: dict[str, Any]) -> str:
    document = {
        "parent": parent.ticket.number,
        "spec_revision": parent.sections["Specification revision"],
        "plan": plan_hash,
        "task": next(
            item
            for item in normalized_plan({"tasks": [task]})
            if item["key"] == task["key"]
        ),
    }
    canonical = json.dumps(document, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def create_issue(
    workflow: Workflow, *, title: str, body: str, labels: Sequence[str]
) -> Ticket:
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix="issue-body-", suffix=".md"
    ) as stream:
        stream.write(body)
        stream.flush()
        command = [
            "gh",
            "issue",
            "create",
            "--repo",
            workflow.repository,
            "--title",
            title,
            "--body-file",
            stream.name,
        ]
        for label in labels:
            command.extend(("--label", label))
        created = subprocess.run(
            command, cwd=ROOT, check=False, capture_output=True, text=True
        )
    if created.returncode != 0:
        raise ControlError(created.stderr.strip() or "failed to create issue")
    match = re.search(r"/issues/([1-9][0-9]*)", created.stdout)
    if not match:
        raise ControlError("issue creation returned no issue number")
    return Ticket(
        number=int(match.group(1)),
        title=title,
        body=body,
        state="OPEN",
        labels=tuple(labels),
        url=created.stdout.strip(),
    )


def materialize_plan(
    workflow: Workflow,
    parent: TicketValidation,
    result: dict[str, Any],
) -> list[int]:
    if result["blockers"]:
        return []
    parent_policy = autonomy_policy(parent.sections)
    if parent_policy != "software-auto-merge":
        raise ControlError(
            "automatic plan materialization requires autopilot parent policy"
        )
    parent_surfaces = allowed_surfaces(
        parent.sections["Allowed architectural surfaces"]
    )
    for task in result["tasks"]:
        child_surfaces = allowed_surfaces("\n".join(task["allowed_surfaces"]))
        if not surfaces_within(child_surfaces, parent_surfaces):
            raise ControlError(
                f"planner task {task['key']} expands the parent's allowed surfaces"
            )
        if task["autonomy_policy"] != parent_policy:
            raise ControlError(
                f"planner task {task['key']} changed the parent's autonomy policy"
            )

    plan_hash = plan_digest(result)
    tickets = load_live_tickets(workflow)
    by_uid: dict[str, Ticket] = {}
    by_parent_key: dict[str, tuple[str, str, Ticket]] = {}
    for ticket in tickets:
        marker = PLAN_TASK_MARKER_RE.search(ticket.body)
        if marker and int(marker.group(1)) == parent.ticket.number:
            key = marker.group(3)
            uid = marker.group(4)
            if uid in by_uid:
                raise ControlError(f"duplicate materialized task marker {uid}")
            if key in by_parent_key:
                raise ControlError(f"duplicate materialized task key {key}")
            by_uid[uid] = ticket
            by_parent_key[key] = (marker.group(2), uid, ticket)

    task_numbers: dict[str, int] = {}
    created_by_key: dict[str, Ticket] = {}
    priority_label = next(
        (label for label in parent.ticket.labels if label.startswith("priority:p")),
        "priority:p2",
    )
    for task in topological_tasks(result["tasks"]):
        uid = task_uid(parent, plan_hash, task)
        marker = (
            "<!-- domes-autopilot-task:v1 "
            f"parent={parent.ticket.number} plan={plan_hash} "
            f"key={task['key']} uid={uid} -->"
        )
        child = by_uid.get(uid)
        if child is None:
            conflict = by_parent_key.get(task["key"])
            if conflict is not None:
                old_plan, old_uid, _ = conflict
                raise ControlError(
                    f"materialized task {task['key']} conflicts with plan "
                    f"{old_plan} and uid {old_uid}"
                )
            provisional_contract = render_ticket_contract(
                spec_revision=result["spec_revision"],
                parent_objective=(
                    f"{parent.sections['Parent objective']} Parent planning issue "
                    f"#{parent.ticket.number}."
                ),
                goal=task["goal"],
                non_goals=task["non_goals"],
                required_behavior=task["required_behavior"],
                acceptance_checks=task["acceptance_checks"],
                allowed_surface_values=task["allowed_surfaces"],
                dependencies=(parent.ticket.number,),
                required_proof=task["required_proof"],
                work_package=parent.sections.get("Work package", task["key"]),
                work_class=parent.sections.get("Work class", "software"),
                selected_policy=task["autonomy_policy"],
            )
            body = marker + "\n\n" + with_autopilot_contract("", provisional_contract)
            child = create_issue(
                workflow,
                title=f"[Agent] {task['key']}: {task['goal']}",
                body=body,
                labels=("agent:needs-specification", priority_label),
            )
            by_uid[uid] = child
        task_numbers[task["key"]] = child.number
        created_by_key[task["key"]] = child

    for task in topological_tasks(result["tasks"]):
        child = created_by_key[task["key"]]
        dependencies = [parent.ticket.number]
        dependencies.extend(task_numbers[key] for key in task["dependencies"])
        contract = render_ticket_contract(
            spec_revision=result["spec_revision"],
            parent_objective=(
                f"{parent.sections['Parent objective']} Parent planning issue "
                f"#{parent.ticket.number}."
            ),
            goal=task["goal"],
            non_goals=task["non_goals"],
            required_behavior=task["required_behavior"],
            acceptance_checks=task["acceptance_checks"],
            allowed_surface_values=task["allowed_surfaces"],
            dependencies=dependencies,
            required_proof=task["required_proof"],
            work_package=parent.sections.get("Work package", task["key"]),
            work_class=parent.sections.get("Work class", "software"),
            selected_policy=task["autonomy_policy"],
        )
        task_marker = PLAN_TASK_MARKER_RE.search(child.body)
        assert task_marker is not None
        expected_body = (
            task_marker.group(0) + "\n\n" + with_autopilot_contract("", contract)
        )
        if child.body != expected_body:
            if child.agent_state not in {"agent:needs-specification", "agent:ready"}:
                raise ControlError(
                    f"materialized task {task['key']} was externally modified"
                )
            update_issue_body(workflow, child, expected_body)
        ready_ticket = Ticket(
            child.number,
            child.title,
            expected_body,
            child.state,
            tuple(label for label in child.labels if not label.startswith(STATE_PREFIX))
            + ("agent:ready",),
            child.url,
        )
        validation = validate_ticket(ready_ticket)
        if not validation.valid:
            raise ControlError(
                f"materialized task {task['key']} is invalid: "
                + "; ".join(validation.errors)
            )
        if child.agent_state != "agent:ready":
            transition(workflow, child, "agent:ready")
    return [task_numbers[task["key"]] for task in topological_tasks(result["tasks"])]


def write_handoff(path: Path, result: dict[str, Any]) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def load_exact_role_handoff(
    workflow: Workflow,
    ticket: Ticket,
    role: str,
    run_root: Path | None = None,
) -> dict[str, Any]:
    if run_root is None:
        state_root = Path(
            os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
        )
        run_root = state_root / "domes-agent-control" / f"issue-{ticket.number}"
    path = run_root / f"handoff-{role}.json"
    result: dict[str, Any] | None = None
    if path.is_file():
        try:
            result = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            result = None
    if result is None:
        document = _run_json(
            [
                "gh",
                "issue",
                "view",
                str(ticket.number),
                "--repo",
                workflow.repository,
                "--json",
                "comments",
            ]
        )
        marker = f"Agent control-plane transition ({role})"
        for comment in reversed(document.get("comments", [])):
            body = str(comment.get("body", ""))
            if marker not in body:
                continue
            match = re.search(r"```json\s*\n(.*?)\n```", body, re.DOTALL)
            if match is None:
                continue
            try:
                result = json.loads(match.group(1))
            except json.JSONDecodeError:
                continue
            break
    if result is None:
        raise ControlError(f"issue #{ticket.number}: missing {role} handoff")
    if result.get("issue") != ticket.number:
        raise ControlError(f"issue #{ticket.number}: {role} handoff issue mismatch")
    sections = parse_sections(ticket.body)
    if result.get("spec_revision") != sections.get("Specification revision"):
        raise ControlError(f"issue #{ticket.number}: {role} handoff spec mismatch")
    validate_result_semantics(role, result)
    return result


def count_role_comments(workflow: Workflow, ticket: Ticket, role: str) -> int:
    document = _run_json(
        [
            "gh",
            "issue",
            "view",
            str(ticket.number),
            "--repo",
            workflow.repository,
            "--json",
            "comments",
        ]
    )
    marker = f"Agent control-plane transition ({role})"
    return sum(
        marker in str(comment.get("body", ""))
        for comment in document.get("comments", [])
    )


def load_latest_artifact_handoff(workflow: Workflow, ticket: Ticket) -> dict[str, Any]:
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / f"issue-{ticket.number}"
    local = [
        (path.stat().st_mtime_ns, role)
        for role in ("worker", "verification-worker")
        if (path := run_root / f"handoff-{role}.json").is_file()
    ]
    for _, role in sorted(local, reverse=True):
        try:
            result = load_exact_role_handoff(workflow, ticket, role, run_root)
        except ControlError:
            continue
        if result.get("commit") and result.get("pull_request"):
            return result

    document = _run_json(
        [
            "gh",
            "issue",
            "view",
            str(ticket.number),
            "--repo",
            workflow.repository,
            "--json",
            "comments",
        ]
    )
    for comment in reversed(document.get("comments", [])):
        body = str(comment.get("body", ""))
        role = next(
            (
                candidate
                for candidate in ("worker", "verification-worker")
                if f"Agent control-plane transition ({candidate})" in body
            ),
            None,
        )
        if role is None:
            continue
        match = re.search(r"```json\s*\n(.*?)\n```", body, re.DOTALL)
        if match is None:
            continue
        try:
            result = json.loads(match.group(1))
            validate_result_semantics(role, result)
        except (json.JSONDecodeError, ControlError):
            continue
        if (
            result.get("issue") == ticket.number
            and result.get("commit")
            and result.get("pull_request")
        ):
            return result
    raise ControlError(f"issue #{ticket.number}: missing current artifact handoff")


def required_prior_handoff(
    workflow: Workflow,
    ticket: Ticket,
    run_root: Path,
    role: str,
    source_state: str | None,
) -> dict[str, Any] | None:
    prior_roles: tuple[str, ...]
    if role == "judge":
        prior_roles = ("verification-worker", "worker")
    elif role == "verification-worker":
        prior_roles = ("judge",)
    elif role == "worker" and source_state == "agent:rework":
        prior_roles = ("judge",)
    else:
        return None
    candidates = [
        (path.stat().st_mtime_ns, prior_role, path)
        for prior_role in prior_roles
        if (path := run_root / f"handoff-{prior_role}.json").is_file()
    ]
    result: dict[str, Any] | None = None
    matched_role: str | None = None
    for _, prior_role, path in sorted(candidates, reverse=True):
        try:
            result = json.loads(path.read_text(encoding="utf-8"))
            matched_role = prior_role
            break
        except (OSError, json.JSONDecodeError):
            continue
    if result is None:
        document = _run_json(
            [
                "gh",
                "issue",
                "view",
                str(ticket.number),
                "--repo",
                workflow.repository,
                "--json",
                "comments",
            ]
        )
        for comment in reversed(document.get("comments", [])):
            body = str(comment.get("body", ""))
            matched_role = next(
                (
                    prior_role
                    for prior_role in prior_roles
                    if f"Agent control-plane transition ({prior_role})" in body
                ),
                None,
            )
            if matched_role is None:
                continue
            match = re.search(r"```json\s*\n(.*?)\n```", body, re.DOTALL)
            if not match:
                continue
            try:
                result = json.loads(match.group(1))
            except json.JSONDecodeError:
                continue
            break
    if result is None:
        expected = " or ".join(prior_roles)
        raise ControlError(f"{role} requires valid {expected} structured evidence")
    if result.get("issue") != ticket.number:
        raise ControlError("prior structured evidence belongs to another issue")
    assert matched_role is not None
    validate_result_semantics(matched_role, result)
    return result


def post_result(
    workflow: Workflow, ticket: Ticket, role: str, result: dict[str, Any]
) -> None:
    structured = json.dumps(result, indent=2, sort_keys=True)
    body = (
        f"Agent control-plane transition ({role})\n\n"
        f"{concise_result(role, result)}\n\n"
        f"Specification revision: `{result['spec_revision']}`\n"
        "Raw session output is intentionally excluded.\n\n"
        "<details><summary>Schema-validated handoff</summary>\n\n"
        f"```json\n{structured}\n```\n\n"
        "</details>"
    )
    if len(body.encode("utf-8")) > 60_000:
        raise ControlError(
            f"issue #{ticket.number}: structured handoff exceeds comment limit"
        )
    posted = subprocess.run(
        [
            "gh",
            "issue",
            "comment",
            str(ticket.number),
            "--repo",
            workflow.repository,
            "--body",
            body,
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if posted.returncode != 0:
        raise ControlError(
            posted.stderr.strip() or f"failed to comment on #{ticket.number}"
        )


def execute_one(
    workflow: Workflow, item: TicketValidation, *, autopilot: bool = False
) -> dict[str, Any]:
    role = role_for(item.ticket)
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / f"issue-{item.ticket.number}"
    run_root.mkdir(parents=True, exist_ok=True)
    lease_path = run_root / "active-process.json"
    terminate_recorded_process_group(lease_path)
    workspace = ensure_workspace(workflow, item, role)
    schema = ORCHESTRATION_DIR / "schemas" / SCHEMA_BY_ROLE[role]
    prior_handoff = required_prior_handoff(
        workflow,
        item.ticket,
        run_root,
        role,
        item.source_state or item.ticket.agent_state,
    )
    if (
        prior_handoff is not None
        and prior_handoff.get("spec_revision")
        != item.sections["Specification revision"]
    ):
        raise ControlError(
            f"issue #{item.ticket.number}: prior handoff specification mismatch"
        )
    pending_plan_path = run_root / "pending-plan.json"
    if (
        role == "planner"
        and autopilot
        and autonomy_policy(item.sections) == "software-auto-merge"
        and pending_plan_path.is_file()
    ):
        pending_plan = json.loads(pending_plan_path.read_text(encoding="utf-8"))
        if (
            pending_plan.get("issue") != item.ticket.number
            or pending_plan.get("spec_revision")
            != item.sections["Specification revision"]
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: pending plan journal does not match"
            )
        validate_result_semantics("planner", pending_plan)
        if pending_plan["blockers"]:
            raise ControlError(
                f"issue #{item.ticket.number}: pending plan unexpectedly has blockers"
            )
        materialized = materialize_plan(workflow, item, pending_plan)
        write_handoff(run_root / "handoff-planner.json", pending_plan)
        post_result(workflow, item.ticket, "planner", pending_plan)
        transition(workflow, item.ticket, "agent:done")
        close_issue(workflow, item.ticket.number)
        pending_plan_path.unlink(missing_ok=True)
        return {
            "issue": item.ticket.number,
            "role": role,
            "state": "agent:done",
            "materialized": materialized,
            "recovered": True,
        }
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        prefix="result-",
        suffix=".json",
        dir=run_root,
        delete=False,
    ) as stream:
        result_path = Path(stream.name)
    event_path = result_path.with_suffix(".jsonl")
    stderr_path = result_path.with_suffix(".stderr.log")
    command = [
        "codex",
        "exec",
        "--strict-config",
        "-c",
        'approval_policy="never"',
        "--sandbox",
        "workspace-write" if role in {"worker", "verification-worker"} else "read-only",
        "--cd",
        str(workspace),
        "--output-schema",
        str(schema),
        "--output-last-message",
        str(result_path),
        "--json",
        "-",
    ]
    failures: list[str] = []
    for attempt in range(1, 4):
        returncode, failure = run_codex_attempt(
            command,
            build_prompt(item, role, prior_handoff),
            event_path,
            stderr_path,
            workflow.stall_timeout_seconds,
            lease_path,
        )
        if returncode == 0:
            break
        failures.append(failure)
        if attempt < 3:
            time.sleep(
                min(10 * (2 ** (attempt - 1)), workflow.max_retry_backoff_seconds)
            )
    else:
        raise ControlError(
            f"issue #{item.ticket.number} {role} failed after 3 attempts: "
            f"{'; '.join(failures)}"
        )
    result = json.loads(result_path.read_text(encoding="utf-8"))
    if result.get("issue") != item.ticket.number:
        raise ControlError(f"issue #{item.ticket.number}: result issue mismatch")
    if result.get("spec_revision") != item.sections["Specification revision"]:
        raise ControlError(
            f"issue #{item.ticket.number}: result specification mismatch"
        )
    validate_result_semantics(role, result)
    if role == "judge":
        if prior_handoff is None:
            raise ControlError(
                f"issue #{item.ticket.number}: judge has no artifact handoff"
            )
        if result.get("commit") != prior_handoff.get("commit") or result.get(
            "pull_request"
        ) != prior_handoff.get("pull_request"):
            raise ControlError(
                f"issue #{item.ticket.number}: judge verdict is not bound to the "
                "reviewed artifact"
            )
    if role == "verification-worker":
        if prior_handoff is None:
            raise ControlError(
                f"issue #{item.ticket.number}: verification has no judge handoff"
            )
        artifact_changed = result.get("commit") != prior_handoff.get(
            "commit"
        ) or result.get("pull_request") != prior_handoff.get("pull_request")
        if (artifact_changed or result.get("repairs")) and result.get(
            "state"
        ) != "agent_review":
            raise ControlError(
                f"issue #{item.ticket.number}: every CI repair must return through "
                "independent agent review"
            )
    if (
        role in {"worker", "verification-worker"}
        and autonomy_policy(item.sections) == "software-auto-merge"
    ):
        verify_worker_artifact(workflow, workspace, item, result)
        if role == "worker" and not existing_pull_request(item.sections):
            bind_ticket_pull_request(workflow, item, int(result["pull_request"]))
    if (
        role == "planner"
        and autopilot
        and autonomy_policy(item.sections) == "software-auto-merge"
        and not result["blockers"]
    ):
        write_handoff(pending_plan_path, result)
    write_handoff(run_root / f"handoff-{role}.json", result)
    post_result(workflow, item.ticket, role, result)
    next_state = result_state(
        role,
        result,
        autopilot=autopilot,
        ticket_sections=item.sections,
    )
    materialized: list[int] = []
    if (
        role == "planner"
        and autopilot
        and autonomy_policy(item.sections) == "software-auto-merge"
    ):
        if result["blockers"]:
            next_state = "agent:blocked"
        else:
            materialized = materialize_plan(workflow, item, result)
            next_state = "agent:done"
    transition(workflow, item.ticket, next_state)
    if next_state == "agent:done":
        close_issue(workflow, item.ticket.number)
        pending_plan_path.unlink(missing_ok=True)
    return {
        "issue": item.ticket.number,
        "role": role,
        "state": next_state,
        "materialized": materialized,
        "result": str(result_path),
        "events": str(event_path),
        "stderr": str(stderr_path),
    }


def process_identity(pid: int) -> tuple[str, int, int] | None:
    try:
        value = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
        closing_parenthesis = value.rfind(")")
        fields = value[closing_parenthesis + 2 :].split()
        return fields[0], int(fields[2]), int(fields[19])
    except (OSError, ValueError, IndexError):
        return None


def process_start_ticks(pid: int) -> int | None:
    identity = process_identity(pid)
    return identity[2] if identity is not None else None


def read_process_lease(path: Path) -> tuple[int, int] | None:
    if not path.exists():
        return None
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
        pid = int(document["pid"])
        start_ticks = int(document["start_ticks"])
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise ControlError(f"invalid process lease: {path}") from error
    if pid < 1 or start_ticks < 1:
        raise ControlError(f"invalid process lease: {path}")
    return pid, start_ticks


def write_process_lease(path: Path, pid: int) -> None:
    start_ticks = process_start_ticks(pid)
    if start_ticks is None:
        raise ControlError(f"cannot identify agent process {pid}")
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary.write_text(
        json.dumps({"pid": pid, "start_ticks": start_ticks}) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def clear_process_lease(path: Path, pid: int | None = None) -> None:
    if pid is not None:
        lease = read_process_lease(path)
        if lease is None or lease[0] != pid:
            return
    path.unlink(missing_ok=True)


def process_group_exists(process_group: int) -> bool:
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        identity = process_identity(int(entry.name))
        if identity is None:
            continue
        state, member_group, _ = identity
        if member_group == process_group and state != "Z":
            return True
    return False


def wait_for_process_group_exit(process_group: int, timeout_seconds: float) -> bool:
    deadline = time.monotonic() + timeout_seconds
    while process_group_exists(process_group):
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.05)
    return True


def terminate_process_group(process_group: int, grace_seconds: float = 0.2) -> None:
    if not process_group_exists(process_group):
        return
    try:
        os.killpg(process_group, signal.SIGTERM)
    except ProcessLookupError:
        return
    if wait_for_process_group_exit(process_group, grace_seconds):
        return
    try:
        os.killpg(process_group, signal.SIGKILL)
    except ProcessLookupError:
        return
    if not wait_for_process_group_exit(process_group, 5):
        raise ControlError(f"process group {process_group} survived SIGKILL")


def terminate_recorded_process_group(
    lease_path: Path, expected_pid: int | None = None
) -> None:
    lease = read_process_lease(lease_path)
    if lease is None:
        return
    pid, expected_start_ticks = lease
    if expected_pid is not None and pid != expected_pid:
        raise ControlError(
            f"process lease PID mismatch: expected {expected_pid}, found {pid}"
        )
    actual_start_ticks = process_start_ticks(pid)
    if actual_start_ticks == expected_start_ticks or (
        actual_start_ticks is None and process_group_exists(pid)
    ):
        terminate_process_group(pid)
    clear_process_lease(lease_path, pid)


def start_leased_process(
    command: Sequence[str],
    lease_path: Path,
    *,
    stdout: Any,
    stderr: Any,
) -> subprocess.Popen[str]:
    terminate_recorded_process_group(lease_path)
    gate_read, gate_write = os.pipe()
    process: subprocess.Popen[str] | None = None
    try:
        process = subprocess.Popen(
            [sys.executable, "-c", GATED_EXEC, str(gate_read), *command],
            stdin=subprocess.PIPE,
            stdout=stdout,
            stderr=stderr,
            text=True,
            start_new_session=True,
            pass_fds=(gate_read,),
        )
        os.close(gate_read)
        gate_read = -1
        write_process_lease(lease_path, process.pid)
        os.write(gate_write, b"1")
        os.close(gate_write)
        gate_write = -1
        return process
    except Exception:
        if gate_read >= 0:
            os.close(gate_read)
        if gate_write >= 0:
            os.close(gate_write)
        if process is not None:
            if process.stdin is not None:
                process.stdin.close()
            terminate_process_group(process.pid)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            clear_process_lease(lease_path, process.pid)
        raise


def run_codex_attempt(
    command: Sequence[str],
    prompt: str,
    event_path: Path,
    stderr_path: Path,
    stall_timeout_seconds: int,
    lease_path: Path | None = None,
) -> tuple[int, str]:
    with event_path.open("a", encoding="utf-8") as events, stderr_path.open(
        "a", encoding="utf-8"
    ) as errors:
        process = (
            start_leased_process(command, lease_path, stdout=events, stderr=errors)
            if lease_path is not None
            else subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=events,
                stderr=errors,
                text=True,
                start_new_session=True,
            )
        )
        assert process.stdin is not None
        try:
            try:
                process.stdin.write(prompt)
                process.stdin.close()
            except BrokenPipeError:
                pass
            last_size = event_path.stat().st_size
            last_activity = time.monotonic()
            while process.poll() is None:
                time.sleep(1)
                current_size = event_path.stat().st_size
                if current_size != last_size:
                    last_size = current_size
                    last_activity = time.monotonic()
                if time.monotonic() - last_activity < stall_timeout_seconds:
                    continue
                terminate_process_group(process.pid)
                process.wait()
                return (
                    process.returncode or 1,
                    f"no Codex event activity for {stall_timeout_seconds}s",
                )
            if process.returncode == 0:
                return 0, ""
        finally:
            if lease_path is not None:
                terminate_recorded_process_group(lease_path, process.pid)
    tail = stderr_path.read_text(encoding="utf-8", errors="replace").splitlines()[-20:]
    return process.returncode or 1, "\n".join(tail) or f"exit {process.returncode}"


def verify_worker_artifact(
    workflow: Workflow,
    workspace: Path,
    item: TicketValidation,
    result: dict[str, Any],
) -> None:
    commit = result.get("commit", "")
    if not FULL_SHA.fullmatch(commit):
        raise ControlError(
            f"issue #{item.ticket.number}: worker returned an invalid commit"
        )
    head = _git("rev-parse", "HEAD", cwd=workspace)
    if head.returncode != 0 or head.stdout.strip() != commit:
        raise ControlError(
            f"issue #{item.ticket.number}: worker commit is not workspace HEAD"
        )
    dirty = _git("status", "--porcelain", "--untracked-files=all", cwd=workspace)
    if dirty.returncode != 0 or dirty.stdout.strip():
        raise ControlError(f"issue #{item.ticket.number}: worker left a dirty worktree")
    changed = _git(
        "diff",
        "--name-only",
        "--diff-filter=ACDMR",
        f"origin/main...{commit}",
        "--",
        cwd=workspace,
    )
    if changed.returncode != 0:
        raise ControlError(f"issue #{item.ticket.number}: cannot resolve worker diff")
    surfaces = allowed_surfaces(item.sections["Allowed architectural surfaces"])
    changed_paths = tuple(changed.stdout.splitlines())
    violations = paths_outside_surfaces(changed_paths, surfaces)
    if violations:
        raise ControlError(
            f"issue #{item.ticket.number}: changes outside allowed surfaces: "
            f"{', '.join(violations)}"
        )
    if autonomy_policy(item.sections) != "software-auto-merge":
        return
    pull_request_number = result.get("pull_request")
    if not isinstance(pull_request_number, int) or pull_request_number < 1:
        raise ControlError(
            f"issue #{item.ticket.number}: autonomous worker must return one pull request"
        )
    expected_pull_request = existing_pull_request(item.sections)
    if expected_pull_request and pull_request_number != expected_pull_request:
        raise ControlError(
            f"issue #{item.ticket.number}: worker changed the existing pull request"
        )
    pull_request = load_pull_request(workflow, pull_request_number)
    branch = _git("branch", "--show-current", cwd=workspace)
    if (
        pull_request.state != "OPEN"
        or pull_request.is_draft
        or pull_request.base_ref != workflow.base_branch
        or pull_request.head_oid != commit
        or branch.returncode != 0
        or branch.stdout.strip() != pull_request.head_ref
    ):
        raise ControlError(
            f"issue #{item.ticket.number}: pull request does not match worker artifact"
        )
    pr_violations = paths_outside_surfaces(pull_request.files, surfaces)
    if pr_violations:
        raise ControlError(
            f"issue #{item.ticket.number}: PR changes outside allowed surfaces: "
            f"{', '.join(pr_violations)}"
        )


def required_check_summary(
    pull_request: PullRequest, policy: AutopilotPolicy
) -> tuple[str, list[dict[str, str]]]:
    by_name: dict[str, list[dict[str, str]]] = {}
    for check in pull_request.checks:
        by_name.setdefault(check["name"], []).append(check)
    records: list[dict[str, str]] = []
    overall = "passed"
    failure_states = {
        "ACTION_REQUIRED",
        "CANCELLED",
        "ERROR",
        "FAILURE",
        "STARTUP_FAILURE",
        "STALE",
        "TIMED_OUT",
    }
    for name in policy.required_ci_checks:
        matches = by_name.get(name, [])
        states = {item["state"] for item in matches}
        if states & failure_states:
            state = "failed"
            overall = "failed"
        elif matches and states == {"SUCCESS"}:
            state = "passed"
        else:
            state = "pending"
            if overall != "failed":
                overall = "pending"
        records.append(
            {
                "name": name,
                "state": state,
                "url": next((item["url"] for item in matches if item["url"]), ""),
            }
        )
    return overall, records


def requires_physical_proof(sections: dict[str, str]) -> bool:
    contract = "\n".join(
        (
            sections.get("Acceptance checks", ""),
            sections.get("Required proof", ""),
        )
    )
    return bool(re.search(r"\b(?:physical|hardware)\b", contract, re.IGNORECASE))


def has_current_physical_proof(artifact: dict[str, Any]) -> bool:
    verification = artifact.get("verification")
    if not isinstance(verification, list):
        return False
    return any(
        isinstance(record, dict)
        and record.get("level") == "physical"
        and record.get("status") == "passed"
        and bool(record.get("artifact"))
        for record in verification
    )


def post_controller_comment(
    workflow: Workflow, ticket: Ticket, heading: str, body: str
) -> None:
    result = subprocess.run(
        [
            "gh",
            "issue",
            "comment",
            str(ticket.number),
            "--repo",
            workflow.repository,
            "--body",
            f"{heading}\n\n{body}",
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ControlError(
            result.stderr.strip() or f"failed to comment on #{ticket.number}"
        )


def finalize_merged_ticket(
    workflow: Workflow,
    policy: AutopilotPolicy,
    ticket: Ticket,
    pull_request: PullRequest,
    *,
    recovered: bool,
) -> dict[str, Any]:
    try:
        # Keep the issue in ci-pending until local base state can be refreshed. If
        # this read-side operation fails, the next poll can recover the verified
        # merged PR without leaving a partially finalized tracker state.
        refresh_base_branch(workflow)
        qualifier = "Recovered an already merged" if recovered else "Merged"
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (merge)",
            (
                f"{qualifier} PR #{pull_request.number} at exact head "
                f"`{pull_request.head_oid}` after independent approval and "
                f"{len(policy.required_ci_checks)} required checks passed. "
                f"Merge commit: `{pull_request.merge_commit}`."
            ),
        )
        # Closing and replacing the state label happen in one API mutation. No
        # later fallible operation may strand a merged issue in a partial state.
        complete_issue(workflow, ticket)
    except ControlError as error:
        # The PR is already verified as merged. Never reinterpret a tracker or
        # refresh failure during finalization as an unsafe merge verdict.
        raise TrackerError(
            f"issue #{ticket.number}: merged PR finalization must be retried: {error}"
        ) from error
    return {
        "issue": ticket.number,
        "state": "agent:done",
        "pull_request": pull_request.number,
        "head": pull_request.head_oid,
        "merge_commit": pull_request.merge_commit,
        "recovered": recovered,
    }


def _same_pull_request(left: PullRequest, right: PullRequest) -> bool:
    return (
        left.number == right.number
        and left.state == right.state
        and left.is_draft == right.is_draft
        and left.base_ref == right.base_ref
        and left.base_oid == right.base_oid
        and left.head_ref == right.head_ref
        and left.head_oid == right.head_oid
        and left.mergeable == right.mergeable
        and left.merge_state == right.merge_state
        and left.review_decision == right.review_decision
        and left.files == right.files
    )


def pull_request_merge_metadata_valid(
    workflow: Workflow, pull_request: PullRequest
) -> bool:
    return (
        pull_request.state == "OPEN"
        and not pull_request.is_draft
        and pull_request.base_ref == workflow.base_branch
        and pull_request.review_decision != "CHANGES_REQUESTED"
        and pull_request.mergeable == "MERGEABLE"
        and pull_request.merge_state == "CLEAN"
    )


def merge_autopilot_pull_request(
    workflow: Workflow,
    policy: AutopilotPolicy,
    ticket: Ticket,
    artifact: dict[str, Any],
    judge: dict[str, Any],
    pull_request: PullRequest,
) -> dict[str, Any]:
    sections = parse_sections(ticket.body)
    if autonomy_policy(
        sections
    ) != "software-auto-merge" or not has_valid_autopilot_marker(ticket, sections):
        raise ControlError(
            f"issue #{ticket.number}: missing controller merge authority"
        )
    if judge.get("verdict") != "approve":
        raise ControlError(
            f"issue #{ticket.number}: independent approval is not current"
        )
    if judge.get("commit") != artifact.get("commit") or judge.get(
        "pull_request"
    ) != artifact.get("pull_request"):
        raise ControlError(
            f"issue #{ticket.number}: independent approval is not bound to the PR head"
        )
    if artifact.get("commit") != pull_request.head_oid:
        raise ControlError(
            f"issue #{ticket.number}: PR head changed after artifact review"
        )
    if (
        pull_request.state not in {"OPEN", "MERGED"}
        or (pull_request.state == "OPEN" and pull_request.is_draft)
        or pull_request.base_ref != workflow.base_branch
        or pull_request.review_decision == "CHANGES_REQUESTED"
    ):
        raise ControlError(
            f"issue #{ticket.number}: PR metadata blocks automatic merge"
        )
    surfaces = allowed_surfaces(sections["Allowed architectural surfaces"])
    outside = paths_outside_surfaces(pull_request.files, surfaces)
    forbidden = forbidden_auto_merge_paths(pull_request.files, policy)
    if outside or forbidden:
        detail = outside or forbidden
        raise ControlError(
            f"issue #{ticket.number}: auto-merge path policy rejected: "
            + ", ".join(detail)
        )
    if requires_physical_proof(sections) and not has_current_physical_proof(artifact):
        raise ControlError(
            f"issue #{ticket.number}: current PR head lacks required physical proof"
        )
    ci_state, _ = required_check_summary(pull_request, policy)
    if ci_state != "passed":
        raise ControlError(f"issue #{ticket.number}: required CI is not green")
    if pull_request.state == "MERGED":
        return finalize_merged_ticket(
            workflow, policy, ticket, pull_request, recovered=True
        )
    if not pull_request_merge_metadata_valid(workflow, pull_request):
        raise ControlError(f"issue #{ticket.number}: PR is not cleanly mergeable")

    refreshed = load_pull_request(workflow, pull_request.number)
    refreshed_ci, _ = required_check_summary(refreshed, policy)
    if (
        not _same_pull_request(pull_request, refreshed)
        or refreshed_ci != "passed"
        or not pull_request_merge_metadata_valid(workflow, refreshed)
    ):
        raise ControlError(f"issue #{ticket.number}: PR changed during merge gate")
    command = [
        "gh",
        "pr",
        "merge",
        str(pull_request.number),
        "--repo",
        workflow.repository,
        f"--{policy.merge_method}",
        "--match-head-commit",
        pull_request.head_oid,
    ]
    merged = subprocess.run(
        command, cwd=ROOT, check=False, capture_output=True, text=True
    )
    if merged.returncode != 0:
        raise ControlError(
            merged.stderr.strip() or f"issue #{ticket.number}: exact-head merge failed"
        )
    verified = load_pull_request(workflow, pull_request.number)
    if verified.state != "MERGED" or verified.head_oid != pull_request.head_oid:
        raise ControlError(f"issue #{ticket.number}: merge could not be verified")
    return finalize_merged_ticket(workflow, policy, ticket, verified, recovered=False)


def reconcile_ci_ticket(
    workflow: Workflow,
    policy: AutopilotPolicy,
    ticket: Ticket,
) -> dict[str, Any]:
    validation = validate_ticket(ticket)
    if not validation.valid:
        raise ControlError(
            f"issue #{ticket.number}: invalid CI-pending ticket: "
            + "; ".join(validation.errors)
        )
    artifact = load_latest_artifact_handoff(workflow, ticket)
    judge = load_exact_role_handoff(workflow, ticket, "judge")
    if judge["verdict"] != "approve":
        raise ControlError(f"issue #{ticket.number}: CI state lacks judge approval")
    pull_request_number = artifact.get("pull_request")
    if not isinstance(pull_request_number, int) or pull_request_number < 1:
        raise ControlError(f"issue #{ticket.number}: CI state has no pull request")
    pull_request = load_pull_request(workflow, pull_request_number)
    if pull_request.head_oid != artifact.get("commit"):
        transition(workflow, ticket, "agent:rework")
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (ci)",
            "Pull-request head changed after the reviewed artifact; returning to rework.",
        )
        return {"issue": ticket.number, "state": "agent:rework"}
    if pull_request.base_ref != workflow.base_branch or pull_request.is_draft:
        transition(workflow, ticket, "agent:blocked")
        return {"issue": ticket.number, "state": "agent:blocked"}
    ci_state, records = required_check_summary(pull_request, policy)
    if ci_state == "pending":
        return {
            "issue": ticket.number,
            "state": "agent:ci-pending",
            "checks": records,
        }
    if ci_state == "failed":
        attempts = count_role_comments(workflow, ticket, "verification-worker")
        if attempts >= policy.max_ci_repair_cycles:
            transition(workflow, ticket, "agent:blocked")
            post_controller_comment(
                workflow,
                ticket,
                "Agent control-plane transition (ci)",
                f"CI repair cap of {policy.max_ci_repair_cycles} reached.",
            )
            return {"issue": ticket.number, "state": "agent:blocked", "checks": records}
        transition(workflow, ticket, "agent:verification")
        failed = [record for record in records if record["state"] == "failed"]
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (ci)",
            "Required checks failed; dispatching bounded verification repair.\n\n"
            f"```json\n{json.dumps(failed, indent=2, sort_keys=True)}\n```",
        )
        return {
            "issue": ticket.number,
            "state": "agent:verification",
            "checks": records,
        }
    if pull_request.state != "MERGED" and (
        pull_request.mergeable == "UNKNOWN" or pull_request.merge_state == "UNKNOWN"
    ):
        return {
            "issue": ticket.number,
            "state": "agent:ci-pending",
            "checks": records,
        }
    try:
        return merge_autopilot_pull_request(
            workflow, policy, ticket, artifact, judge, pull_request
        )
    except TrackerError:
        raise
    except ControlError as error:
        if pull_request.merge_state in {"BEHIND", "DIRTY"}:
            transition(workflow, ticket, "agent:rework")
            post_controller_comment(
                workflow,
                ticket,
                "Agent control-plane transition (merge)",
                f"{error}; returning to rework against current main.",
            )
            return {"issue": ticket.number, "state": "agent:rework"}
        transition(workflow, ticket, "agent:human-review")
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (merge)",
            f"Automatic merge failed closed: {error}",
        )
        return {"issue": ticket.number, "state": "agent:human-review"}


def reconcile_ci(
    workflow: Workflow,
    policy: AutopilotPolicy,
    tickets: Sequence[Ticket],
) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    for ticket in tickets:
        if ticket.state != "OPEN" or ticket.agent_state != "agent:ci-pending":
            continue
        try:
            results.append(reconcile_ci_ticket(workflow, policy, ticket))
        except TrackerError as error:
            results.append(
                {
                    "issue": ticket.number,
                    "state": "agent:ci-pending",
                    "retryable_error": str(error),
                }
            )
        except ControlError as error:
            transition(workflow, ticket, "agent:blocked")
            post_controller_comment(
                workflow,
                ticket,
                "Agent control-plane transition (ci)",
                f"CI reconciliation failed closed: {error}",
            )
            results.append(
                {"issue": ticket.number, "state": "agent:blocked", "error": str(error)}
            )
    return results


def output_schema_contract_errors(document: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    def visit(schema: dict[str, Any], path: str) -> None:
        schema_type = schema.get("type")
        if schema_type == "object":
            properties = schema.get("properties")
            if not isinstance(properties, dict):
                errors.append(f"{path} must declare object properties")
                return
            if schema.get("additionalProperties") is not False:
                errors.append(f"{path} must set additionalProperties to false")
            required = schema.get("required")
            if not isinstance(required, list) or set(required) != set(properties):
                errors.append(f"{path} must require every declared property")
            for name, definition in properties.items():
                property_path = f"{path}.properties.{name}"
                if not isinstance(definition, dict):
                    errors.append(f"{property_path} must be a schema object")
                    continue
                if "type" not in definition:
                    errors.append(f"{property_path} must declare a type")
                visit(definition, property_path)
        if schema_type == "array":
            items = schema.get("items")
            if not isinstance(items, dict):
                errors.append(f"{path} must declare array items")
            else:
                visit(items, f"{path}.items")

    visit(document, "$")
    return errors


def validate_repository() -> list[str]:
    errors: list[str] = []
    try:
        workflow = load_workflow()
        if workflow.state_prefix != STATE_PREFIX:
            errors.append(f"state_prefix must be {STATE_PREFIX}")
    except (ControlError, OSError) as error:
        errors.append(str(error))
    try:
        policy = load_autopilot_policy()
        if set(policy.allowed_work_classes) != {"software", "executed-validation"}:
            errors.append(
                "autopilot allowed_work_classes must be software and executed-validation"
            )
    except (ControlError, OSError) as error:
        errors.append(str(error))
    for role, schema_name in SCHEMA_BY_ROLE.items():
        prompt = ORCHESTRATION_DIR / "prompts" / f"{role}.md"
        schema = ORCHESTRATION_DIR / "schemas" / schema_name
        if not prompt.is_file() or not prompt.read_text(encoding="utf-8").strip():
            errors.append(f"missing role prompt: {prompt.relative_to(ROOT)}")
        try:
            document = json.loads(schema.read_text(encoding="utf-8"))
            if document.get("type") != "object" or not document.get("required"):
                errors.append(
                    f"invalid result schema contract: {schema.relative_to(ROOT)}"
                )
            for contract_error in output_schema_contract_errors(document):
                errors.append(
                    f"invalid result schema {schema.relative_to(ROOT)}: "
                    f"{contract_error}"
                )
        except (OSError, json.JSONDecodeError) as error:
            errors.append(f"invalid result schema {schema.relative_to(ROOT)}: {error}")
    return errors


def render_queue(
    eligible: Sequence[TicketValidation], blockers: dict[int, list[str]]
) -> dict[str, Any]:
    return {
        "eligible": [
            {
                "issue": item.ticket.number,
                "title": item.ticket.title,
                "state": item.ticket.agent_state,
                "role": role_for(item.ticket),
                "priority": item.ticket.priority,
                "spec_revision": item.sections["Specification revision"],
                "dependencies": list(item.dependencies),
            }
            for item in eligible
        ],
        "blocked": [
            {"issue": issue, "reasons": reasons}
            for issue, reasons in sorted(blockers.items())
        ],
    }


def acquire_lock() -> Any:
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    state_root = state_root / "domes-agent-control"
    state_root.mkdir(parents=True, exist_ok=True)
    stream = (state_root / "orchestrator.lock").open("a+", encoding="utf-8")
    try:
        fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError as error:
        stream.close()
        raise ControlError("another DOMES agent control process is active") from error
    return stream


def enforce_scheduler_host(workflow: Workflow) -> None:
    actual = socket.gethostname()
    if actual != workflow.scheduler_host:
        raise ControlError(
            "mutation-capable scheduler runs are pinned to "
            f"{workflow.scheduler_host}; current host is {actual}"
        )


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser(
        "validate", help="validate checked-in workflow, prompts, and schemas"
    )
    labels = subparsers.add_parser("labels", help="show or apply managed GitHub labels")
    labels.add_argument(
        "--apply", action="store_true", help="create or update labels on GitHub"
    )
    queue = subparsers.add_parser(
        "queue", help="show deterministically eligible and blocked work"
    )
    queue_source = queue.add_mutually_exclusive_group(required=True)
    queue_source.add_argument(
        "--live", action="store_true", help="read GitHub issues through gh"
    )
    queue_source.add_argument(
        "--issues-json", type=Path, help="read a captured issue list"
    )
    run = subparsers.add_parser("run", help="run one bounded scheduling cycle")
    run.add_argument(
        "--execute", action="store_true", help="allow Codex and GitHub mutations"
    )
    run.add_argument(
        "--limit", type=int, help="lower the workflow concurrency for this cycle"
    )
    run.add_argument(
        "--watch",
        action="store_true",
        help="continue polling and refilling slots until interrupted",
    )
    run.add_argument(
        "--autopilot",
        action="store_true",
        help="continuously select bounded work, reconcile CI, and merge policy-approved PRs",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    try:
        workflow = load_workflow()
        if args.command == "validate":
            errors = validate_repository()
            if errors:
                for error in errors:
                    print(f"ERROR: {error}", file=sys.stderr)
                return 1
            print("Agent control-plane contracts are valid.")
            return 0
        if args.command == "labels":
            if args.apply:
                apply_labels(workflow)
                print(f"Applied {len(MANAGED_LABELS)} labels to {workflow.repository}.")
            else:
                print(json.dumps(MANAGED_LABELS, indent=2))
            return 0
        if args.command == "queue":
            tickets = (
                load_live_tickets(workflow)
                if args.live
                else load_ticket_file(args.issues_json)
            )
            eligible, blockers = eligible_queue(tickets)
            print(json.dumps(render_queue(eligible, blockers), indent=2))
            return 0
        if not args.execute:
            raise ControlError("run is mutation-capable and requires --execute")
        if args.autopilot and not args.watch:
            raise ControlError("--autopilot requires --watch")
        enforce_scheduler_host(workflow)
        policy = load_autopilot_policy() if args.autopilot else None
        lock = acquire_lock()
        try:
            maximum = workflow.max_concurrent_workers
            if args.limit is not None:
                if args.limit < 1:
                    raise ControlError("--limit must be positive")
                maximum = min(maximum, args.limit)
            active: dict[
                concurrent.futures.Future[dict[str, Any]], TicketValidation
            ] = {}
            next_selector_at = 0.0
            with concurrent.futures.ThreadPoolExecutor(max_workers=maximum) as executor:
                while True:
                    try:
                        if policy is not None:
                            refresh_base_branch(workflow)
                        tickets = load_live_tickets(workflow)
                    except (ControlError, OSError, json.JSONDecodeError) as error:
                        if not args.watch:
                            raise
                        print(
                            json.dumps(
                                {
                                    "controller_error": str(error),
                                    "retry_in_seconds": workflow.poll_interval_seconds,
                                },
                                indent=2,
                            ),
                            flush=True,
                        )
                        time.sleep(workflow.poll_interval_seconds)
                        continue
                    ci_results: list[dict[str, Any]] = []
                    if policy is not None:
                        ci_results = reconcile_ci(workflow, policy, tickets)
                        if ci_results:
                            tickets = load_live_tickets(workflow)
                    eligible, blockers = eligible_queue(tickets)
                    active_numbers = {item.ticket.number for item in active.values()}
                    candidates = [
                        item
                        for item in eligible
                        if item.ticket.number not in active_numbers
                    ]
                    reserved = [
                        allowed_surfaces(
                            item.sections["Allowed architectural surfaces"]
                        )
                        for item in active.values()
                    ]
                    selected = select_non_overlapping(
                        candidates, maximum - len(active), reserved
                    )
                    for item in selected:
                        try:
                            claimed = claim_for_dispatch(workflow, item)
                        except ControlError as error:
                            print(
                                json.dumps(
                                    {
                                        "issue": item.ticket.number,
                                        "dispatch_error": str(error),
                                    },
                                    indent=2,
                                ),
                                flush=True,
                            )
                            continue
                        future = executor.submit(
                            execute_one,
                            workflow,
                            claimed,
                            autopilot=args.autopilot,
                        )
                        active[future] = claimed

                    if not args.watch:
                        results, failures = collect_results(active, workflow)
                        print(
                            json.dumps(
                                {
                                    "runs": results,
                                    "failures": failures,
                                    "blocked": blockers,
                                },
                                indent=2,
                            )
                        )
                        return 1 if failures else 0

                    if not active:
                        selector_result: dict[str, Any] | None = None
                        if (
                            policy is not None
                            and autopilot_queue_idle(tickets)
                            and time.monotonic() >= next_selector_at
                        ):
                            try:
                                selector_result = run_selector(
                                    workflow,
                                    policy,
                                    tickets,
                                    load_open_pull_request_snapshot(workflow),
                                )
                            except (
                                ControlError,
                                OSError,
                                json.JSONDecodeError,
                            ) as error:
                                selector_result = {
                                    "state": "error",
                                    "error": str(error),
                                }
                            next_selector_at = (
                                0.0
                                if selector_result.get("state") == "selected"
                                else time.monotonic() + SELECTOR_COOLDOWN_SECONDS
                            )
                        print(
                            json.dumps(
                                {
                                    "runs": [],
                                    "failures": [],
                                    "ci": ci_results,
                                    "selector": selector_result,
                                    "blocked": blockers,
                                },
                                indent=2,
                            ),
                            flush=True,
                        )
                        if (
                            selector_result is not None
                            and selector_result["state"] == "selected"
                        ):
                            continue
                        time.sleep(workflow.poll_interval_seconds)
                        continue
                    done, _ = concurrent.futures.wait(
                        active,
                        timeout=workflow.poll_interval_seconds,
                        return_when=concurrent.futures.FIRST_COMPLETED,
                    )
                    if not done:
                        continue
                    completed = {future: active.pop(future) for future in done}
                    results, failures = collect_results(completed, workflow)
                    print(
                        json.dumps(
                            {
                                "runs": results,
                                "failures": failures,
                                "ci": ci_results,
                                "blocked": blockers,
                            },
                            indent=2,
                        ),
                        flush=True,
                    )
        finally:
            lock.close()
    except (ControlError, OSError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


def collect_results(
    futures: dict[concurrent.futures.Future[dict[str, Any]], TicketValidation],
    workflow: Workflow,
) -> tuple[list[dict[str, Any]], list[str]]:
    results: list[dict[str, Any]] = []
    failures: list[str] = []
    for future in concurrent.futures.as_completed(futures):
        item = futures[future]
        try:
            results.append(future.result())
        except Exception as error:
            failures.append(f"issue #{item.ticket.number}: {error}")
            try:
                block_failed_run(workflow, item, error)
            except Exception as reporting_error:
                failures.append(
                    f"issue #{item.ticket.number}: failed to record blocked state: "
                    f"{reporting_error}"
                )
    return results, failures


def block_failed_run(
    workflow: Workflow, item: TicketValidation, error: Exception
) -> None:
    transition(workflow, item.ticket, "agent:blocked")
    role = role_for(item.ticket)
    body = (
        f"Agent control-plane transition ({role})\n\n"
        "The role failed after bounded retries and moved to `agent:blocked`. "
        "Inspect the retained local run logs before retrying.\n\n"
        f"Failure class: `{type(error).__name__}`. Raw output is intentionally excluded."
    )
    posted = subprocess.run(
        [
            "gh",
            "issue",
            "comment",
            str(item.ticket.number),
            "--repo",
            workflow.repository,
            "--body",
            body,
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if posted.returncode != 0:
        raise ControlError(
            posted.stderr.strip()
            or f"failed to report blocked issue #{item.ticket.number}"
        )


if __name__ == "__main__":
    raise SystemExit(main())
