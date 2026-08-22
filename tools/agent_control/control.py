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
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence

from hardware_broker import BrokerError, DeviceLease, create_capability
from hardware_client import request as hardware_request

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
SURFACE_RESERVING_ROLES = frozenset({"worker", "verification-worker"})
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
        "Independent agent judgment recorded; controller is reconciling exact-head CI",
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
SELECTOR_ERROR_COOLDOWN_SECONDS = 30
MAX_RECURSIVE_PLANNER_DEPTH = 4
AUTOMATED_REVIEW_POLICY = "software-review-required"
LEGACY_AUTOMERGE_POLICY = "software-auto-merge"
AUTOMATED_DELIVERY_POLICIES = frozenset(
    {AUTOMATED_REVIEW_POLICY, LEGACY_AUTOMERGE_POLICY}
)
REGISTERED_NFF_CP2102N_SERIALS = frozenset(
    {
        "5edf3f45576def11a245cea7c169b110",
        "002a9f8e536def119f38c1a7c169b110",
    }
)


def selector_retry_cooldown(result: dict[str, Any]) -> int:
    return (
        SELECTOR_ERROR_COOLDOWN_SECONDS
        if result.get("state") == "error"
        else SELECTOR_COOLDOWN_SECONDS
    )


def selector_retry_snapshot(result: dict[str, Any], last_snapshot: str) -> str:
    return str(result.get("snapshot", "")) or last_snapshot


HARDWARE_CAPABILITY_RESTRICTIONS = (
    "Allowed only on the preflighted ports: ticket-required repository-standard "
    "application build/flash or serial OTA, framed CLI trace/config commands, reboot, "
    "observation, and restoration. Do not whole-flash, factory-erase, erase NVS, "
    "alter eFuses/security/keys, trigger hw-test, create extra PRs, or release. "
    "The ticket's existing PR workflow remains allowed."
)
HARDWARE_OPERATIONS = frozenset(
    {
        "info",
        "health",
        "self-test",
        "memory",
        "feature-list",
        "trace-start",
        "trace-stop",
        "trace-clear",
        "trace-status",
        "trace-dump",
        "espnow-regression",
        "flash",
        "flash-trace-acceptance",
        "ota",
        "reset",
        "run",
        "artifact-hash",
    }
)
HARDWARE_BOARD_ALIASES = frozenset({0, 1})
_HARDWARE_RUNTIME_LOCK = threading.Lock()
_HARDWARE_RUNTIMES: dict[int, dict[str, Any]] = {}
_BASE_REFRESH_LOCK = threading.RLock()
_SELECTOR_MATERIALIZATION_LOCK = threading.Lock()


class ControlError(RuntimeError):
    """A deterministic validation or control-plane failure."""


class TrackerError(ControlError):
    """A read-side tracker failure that is safe to retry without state mutation."""


class StackInvalidated(ControlError):
    """A stacked parent changed; child must rework rather than become blocked."""


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
    tracker_actor: str = ""


@dataclass(frozen=True)
class AutopilotPolicy:
    schema_version: int
    policy_name: str
    allowed_work_classes: tuple[str, ...]
    required_ci_checks: tuple[str, ...]
    protected_autonomous_paths: tuple[str, ...]
    max_ci_repair_cycles: int
    review_authority: str


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


@dataclass(frozen=True)
class StackContext:
    """Controller-derived binding to the exact live review-stack base."""

    parent_issue: int
    parent_pr: int
    base_ref: str
    base_head: str


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
        "tracker_actor",
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
    tracker_actor = str(config["tracker_actor"])
    if not re.fullmatch(
        r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?", tracker_actor
    ):
        raise ControlError(f"{path}: tracker_actor must be one GitHub login")
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
        tracker_actor=tracker_actor,
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
        "protected_autonomous_paths",
        "max_ci_repair_cycles",
        "review_authority",
    }
    if not isinstance(document, dict) or set(document) != required:
        raise ControlError(f"{path}: autopilot policy keys do not match schema")
    if document["schema_version"] != 1:
        raise ControlError(f"{path}: unsupported autopilot policy schema")
    for key in (
        "allowed_work_classes",
        "required_ci_checks",
        "protected_autonomous_paths",
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
    if document["review_authority"] != "human":
        raise ControlError(f"{path}: review_authority must be `human`")
    policy_name = document["policy_name"]
    if not isinstance(policy_name, str) or not policy_name.strip():
        raise ControlError(f"{path}: policy_name must be a non-empty string")
    return AutopilotPolicy(
        schema_version=1,
        policy_name=policy_name,
        allowed_work_classes=tuple(document["allowed_work_classes"]),
        required_ci_checks=tuple(document["required_ci_checks"]),
        protected_autonomous_paths=tuple(document["protected_autonomous_paths"]),
        max_ci_repair_cycles=repairs,
        review_authority="human",
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


def automated_delivery(sections: dict[str, str]) -> bool:
    return autonomy_policy(sections) in AUTOMATED_DELIVERY_POLICIES


def existing_pull_request(sections: dict[str, str]) -> int:
    value = sections.get("Existing pull request", "").strip()
    if not value or value.casefold() == "none":
        return 0
    match = re.fullmatch(r"#?([1-9][0-9]*)", value)
    if not match:
        raise ControlError("Existing pull request must be `None` or one PR number")
    return int(match.group(1))


def _contract_digest_payload(sections: dict[str, str]) -> dict[str, str]:
    names = [
        *REQUIRED_SECTIONS,
        "Work package",
        "Work class",
        "Autonomy policy",
        "Existing pull request",
    ]
    # Preserve signatures of pre-broker tickets.  New controller-rendered
    # contracts always carry this heading, binding any future capability edit.
    if "Hardware operations" in sections:
        names.append("Hardware operations")
    if "Hardware boards" in sections:
        names.append("Hardware boards")
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
    if policy not in {"review-only", *AUTOMATED_DELIVERY_POLICIES}:
        errors.append(
            "Autonomy policy must be `review-only` or " f"`{AUTOMATED_REVIEW_POLICY}`"
        )
    if policy in AUTOMATED_DELIVERY_POLICIES:
        if not sections.get("Work package", "").strip():
            errors.append(f"{AUTOMATED_REVIEW_POLICY} requires Work package")
        if sections.get("Work class", "").strip() not in {
            "software",
            "executed-validation",
        }:
            errors.append(
                f"{AUTOMATED_REVIEW_POLICY} requires software or "
                "executed-validation Work class"
            )
        if not has_valid_autopilot_marker(ticket, sections):
            errors.append(
                f"{AUTOMATED_REVIEW_POLICY} requires a valid controller "
                "contract marker"
            )
    try:
        existing_pull_request(sections)
    except ControlError as error:
        errors.append(str(error))
    try:
        hardware_operations(sections)
    except ControlError as error:
        errors.append(str(error))
    try:
        hardware_boards(sections)
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


def changed_paths_for_diff(cwd: Path, revision: str) -> tuple[str, ...]:
    changed = _git(
        "diff",
        "--no-ext-diff",
        "--no-textconv",
        "--name-status",
        "--find-renames",
        "--diff-filter=ACDMRT",
        "-z",
        revision,
        "--",
        cwd=cwd,
    )
    if changed.returncode != 0:
        raise ControlError("cannot resolve artifact diff")
    fields = [field for field in changed.stdout.split("\0") if field]
    paths: list[str] = []
    offset = 0
    while offset < len(fields):
        status = fields[offset]
        offset += 1
        path_count = 2 if status.startswith(("R", "C")) else 1
        if not re.fullmatch(
            r"(?:[ACDMT]|[RC][0-9]{1,3})", status
        ) or offset + path_count > len(fields):
            raise ControlError("artifact diff name-status output is invalid")
        paths.extend(fields[offset : offset + path_count])
        offset += path_count
    return tuple(paths)


def protected_autonomous_paths(
    paths: Sequence[str], policy: AutopilotPolicy
) -> list[str]:
    return sorted(
        path
        for path in paths
        if any(
            path_matches(path, pattern) for pattern in policy.protected_autonomous_paths
        )
    )


def protected_autonomous_surfaces(
    surfaces: Sequence[str], policy: AutopilotPolicy
) -> list[str]:
    tracked = _git("ls-files")
    tracked_paths = tracked.stdout.splitlines() if tracked.returncode == 0 else []
    forbidden_paths = protected_autonomous_paths(tracked_paths, policy)
    return sorted(
        surface
        for surface in surfaces
        if not _static_prefix(surface)
        or any(path_matches(path, surface) for path in forbidden_paths)
        or any(
            _static_prefix(forbidden) and surfaces_overlap((surface,), (forbidden,))
            for forbidden in policy.protected_autonomous_paths
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
        reserves_surfaces = role_for(item.ticket) in SURFACE_RESERVING_ROLES
        surfaces = (
            allowed_surfaces(item.sections["Allowed architectural surfaces"])
            if reserves_surfaces
            else ()
        )
        if reserves_surfaces:
            if any(
                surfaces_overlap(surfaces, existing) for existing in selected_surfaces
            ):
                continue
            selected_surfaces.append(surfaces)
        selected.append(item)
        if len(selected) == maximum:
            break
    return selected


def reserved_mutation_surfaces(
    active: Sequence[TicketValidation],
) -> list[tuple[str, ...]]:
    """Return only path reservations held by roles allowed to mutate artifacts."""
    return [
        allowed_surfaces(item.sections["Allowed architectural surfaces"])
        for item in active
        if role_for(item.ticket) in SURFACE_RESERVING_ROLES
    ]


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


def _stack_parent(
    item: TicketValidation, tickets: Sequence[Ticket]
) -> tuple[Ticket | None, str | None]:
    """Return the sole permitted nonterminal dependency, without tracker I/O."""
    by_number = {ticket.number: ticket for ticket in tickets}
    nonterminal = [
        by_number[number]
        for number in item.dependencies
        if number in by_number and not terminal(by_number[number])
    ]
    if not nonterminal:
        return None, None
    if len(nonterminal) != 1:
        return None, "stacked work permits exactly one nonterminal dependency"
    parent = nonterminal[0]
    parent_sections = parse_sections(parent.body)
    if (
        parent.state != "OPEN"
        or parent.agent_state != "agent:human-review"
        or not automated_delivery(item.sections)
        or not automated_delivery(parent_sections)
        or item.sections.get("Work class", "").strip() != "software"
        or parent_sections.get("Work class", "").strip() != "software"
        or hardware_operations(item.sections)
    ):
        return (
            None,
            "nonterminal dependency is not an eligible software human-review parent",
        )
    if not validate_ticket(parent, check_revision=False).valid:
        return None, "stack parent ticket contract is invalid"
    return parent, None


def stack_dependency_status(
    workflow: Workflow | None, item: TicketValidation, tickets: Sequence[Ticket]
) -> tuple[bool, str | None]:
    """Whether dependencies are terminal or form the narrow stacked-PR exception."""
    del workflow
    by_number = {ticket.number: ticket for ticket in tickets}
    missing = [number for number in item.dependencies if number not in by_number]
    if missing:
        return False, f"dependency #{missing[0]} was not returned by the tracker"
    parent, error = _stack_parent(item, tickets)
    if error:
        nonterminal = [
            number for number in item.dependencies if not terminal(by_number[number])
        ]
        if (
            len(nonterminal) == 1
            and by_number[nonterminal[0]].agent_state == "agent:human-review"
        ):
            return False, error
        if len(nonterminal) == 1:
            return False, f"dependency #{nonterminal[0]} is not terminal"
        return False, error
    if parent is not None:
        return True, None
    blocked = [
        number for number in item.dependencies if not terminal(by_number[number])
    ]
    return (
        not blocked,
        None if not blocked else f"dependency #{blocked[0]} is not terminal",
    )


def _stack_context(
    workflow: Workflow,
    item: TicketValidation,
    tickets: Sequence[Ticket],
    resolving: frozenset[int],
) -> StackContext | None:
    """Resolve the exact live review-stack base, validating every open level."""
    parent, error = _stack_parent(item, tickets)
    if error:
        raise StackInvalidated(error)
    if parent is None:
        return None
    if parent.number in resolving:
        raise StackInvalidated("dependency cycle in review stack")
    parent_sections = parse_sections(parent.body)
    try:
        artifact = load_latest_artifact_handoff(workflow, parent)
    except ControlError as error:
        raise StackInvalidated(str(error)) from error
    parent_pr = artifact.get("pull_request")
    parent_head = artifact.get("commit")
    parent_binding = artifact_stack_binding(artifact)
    parent_item = validate_ticket(parent, check_revision=False)
    if not parent_item.valid:
        raise StackInvalidated(f"stack parent #{parent.number} contract is invalid")
    live_parent_binding = _stack_context(
        workflow,
        parent_item,
        tickets,
        resolving | {parent.number},
    )
    if (
        not isinstance(parent_pr, int)
        or not FULL_SHA.fullmatch(str(parent_head))
        or artifact.get("spec_revision")
        != parent_sections.get("Specification revision")
        or existing_pull_request(parent_sections) != parent_pr
    ):
        raise StackInvalidated(f"stack parent #{parent.number} has no exact artifact")
    pull_request = load_pull_request(workflow, parent_pr)
    expected_parent_base = (
        parent_binding.base_ref if parent_binding is not None else workflow.base_branch
    )
    if (
        pull_request.is_draft
        or pull_request.base_ref != expected_parent_base
        or (
            parent_binding is not None
            and pull_request.base_oid != parent_binding.base_head
        )
        or pull_request.head_oid != parent_head
        or pull_request.merge_state == "DIRTY"
        or pull_request.mergeable in {"CONFLICTING", "UNMERGEABLE"}
        or pull_request.review_decision == "CHANGES_REQUESTED"
    ):
        raise StackInvalidated(
            f"stack parent #{parent.number} is no longer a stable review artifact"
        )
    try:
        judge = load_exact_role_handoff(workflow, parent, "judge")
    except ControlError as error:
        raise StackInvalidated(str(error)) from error
    if (
        judge.get("verdict") != "approve"
        or judge.get("commit") != parent_head
        or judge.get("pull_request") != parent_pr
    ):
        raise StackInvalidated(
            f"stack parent #{parent.number} lacks exact-head independent approval"
        )
    ci_state, _ = required_check_summary(pull_request, load_autopilot_policy())
    if ci_state != "passed":
        raise StackInvalidated(
            f"stack parent #{parent.number} lacks exact-head required CI"
        )
    if pull_request.state == "OPEN":
        if parent_binding != live_parent_binding:
            raise StackInvalidated(
                f"stack parent #{parent.number} no longer binds its reviewed base"
            )
        return StackContext(
            parent.number, parent_pr, pull_request.head_ref, pull_request.head_oid
        )
    if pull_request.state == "MERGED" and parent_binding is not None:
        if (
            live_parent_binding is None
            or not FULL_SHA.fullmatch(pull_request.merge_commit)
            or not _remote_commit_is_ancestor(
                workflow, pull_request.merge_commit, live_parent_binding.base_head
            )
        ):
            raise StackInvalidated(
                f"stack parent #{parent.number} is not integrated into its live ancestor"
            )
        # The immediate dependency has landed in an ancestor review branch. Bind
        # new/reworked children to that current branch instead of idling until
        # the entire review chain reaches main.
        return live_parent_binding
    raise StackInvalidated(
        f"stack parent #{parent.number} is no longer a stable review artifact"
    )


def stack_context(
    workflow: Workflow, item: TicketValidation, tickets: Sequence[Ticket]
) -> StackContext | None:
    """Resolve and validate the exact effective parent PR head before use."""
    return _stack_context(workflow, item, tickets, frozenset({item.ticket.number}))


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
        if ticket.agent_state == "agent:plan":
            missing = [
                number for number in item.dependencies if number not in by_number
            ]
            if missing:
                reasons.append(
                    f"dependency #{missing[0]} was not returned by the tracker"
                )
        else:
            dependencies_ok, dependency_error = stack_dependency_status(
                None, item, tickets
            )
            if not dependencies_ok and dependency_error:
                reasons.append(dependency_error)
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
            "number,title,headRefName,headRefOid,baseRefName,baseRefOid,"
            "isDraft,mergeStateStatus,url",
        ]
    )
    return [
        {
            "number": int(document["number"]),
            "title": str(document.get("title", "")),
            "head": str(document.get("headRefName", "")),
            "head_oid": str(document.get("headRefOid", "")),
            "base": str(document.get("baseRefName", "")),
            "base_oid": str(document.get("baseRefOid", "")),
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


def hardware_operations(sections: dict[str, str]) -> tuple[str, ...]:
    """Parse the explicit finite capability ticket section; prose never escalates."""
    value = sections.get("Hardware operations", "").strip()
    if not value or value.casefold() == "none":
        return ()
    operations = tuple(part.strip() for part in value.split(",") if part.strip())
    if (
        not operations
        or len(operations) != len(set(operations))
        or any(operation not in HARDWARE_OPERATIONS for operation in operations)
    ):
        raise ControlError(
            "Hardware operations must be `None` or unique allowlisted enum values"
        )
    if "flash-trace-acceptance" in operations and "flash" not in operations:
        raise ControlError(
            "Hardware operations with `flash-trace-acceptance` must also allow "
            "ordinary `flash` restoration"
        )
    return tuple(sorted(operations))


def hardware_boards(sections: dict[str, str]) -> tuple[int, ...]:
    """Return the digest-bound board aliases available to this ticket."""
    operations = hardware_operations(sections)
    value = sections.get("Hardware boards", "").strip()
    if not value or value.casefold() == "none":
        if operations:
            raise ControlError(
                "Hardware boards must explicitly list broker aliases for hardware work"
            )
        return ()
    if not operations:
        raise ControlError("Hardware boards require at least one Hardware operation")
    raw_aliases = [part.strip() for part in value.split(",") if part.strip()]
    if not raw_aliases or any(not re.fullmatch(r"[01]", part) for part in raw_aliases):
        raise ControlError(
            "Hardware boards must be `None` or unique aliases from `0, 1`"
        )
    aliases = tuple(int(part) for part in raw_aliases)
    if len(aliases) != len(set(aliases)) or not set(aliases).issubset(
        HARDWARE_BOARD_ALIASES
    ):
        raise ControlError(
            "Hardware boards must be `None` or unique aliases from `0, 1`"
        )
    return tuple(sorted(aliases))


def requires_registered_hardware(sections: dict[str, str]) -> bool:
    operations = hardware_operations(sections)
    if operations:
        hardware_boards(sections)
    return bool(operations)


def requires_worker_hardware_access(item: TicketValidation) -> bool:
    return (
        requires_registered_hardware(item.sections)
        and role_for(item.ticket) == "verification-worker"
    )


def validate_hardware_judge_checkpoint(
    workflow: Workflow,
    item: TicketValidation,
    prior_handoff: dict[str, Any] | None,
    pull_request: PullRequest,
) -> None:
    """Require a fresh judge approval for the exact live PR head before hardware."""
    expected_pull_request = existing_pull_request(item.sections)
    if expected_pull_request < 1:
        raise ControlError(
            f"issue #{item.ticket.number}: hardware verification requires one "
            "ticket-bound pull request"
        )
    if prior_handoff is None or prior_handoff.get("verdict") != "approve":
        raise ControlError(
            f"issue #{item.ticket.number}: hardware verification requires an "
            "approved judge handoff"
        )
    if (
        prior_handoff.get("commit") != pull_request.head_oid
        or prior_handoff.get("pull_request") != expected_pull_request
        or pull_request.number != expected_pull_request
        or pull_request.state != "OPEN"
        or pull_request.is_draft
        or pull_request.base_ref != workflow.base_branch
    ):
        raise ControlError(
            f"issue #{item.ticket.number}: live pull request head is not the "
            "judge-approved hardware checkpoint"
        )


def build_prompt(
    item: TicketValidation,
    role: str,
    prior_handoff: dict[str, Any] | None = None,
    hardware_capability: dict[str, Any] | None = None,
    controller_evidence: dict[str, Any] | None = None,
    required_base_head: str | None = None,
    required_base_ref: str | None = None,
    tracker_context: dict[str, Any] | None = None,
    controller_interventions: Sequence[str] = (),
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
    if required_base_head is not None:
        if not FULL_SHA.fullmatch(required_base_head):
            raise ControlError("required base revision must be a full commit SHA")
        prompt += (
            "\n# Controller repository checkpoint\n\n"
            f"Current required base revision: `{required_base_head}`\n"
            "The pushed pull-request head must descend from this exact revision.\n"
        )
        if required_base_ref is not None:
            prompt += (
                f"Required pull-request base branch: `{required_base_ref}`\n"
                "Create or retarget the pull request to this exact branch.\n"
            )
    if prior_handoff is not None:
        prompt += (
            "\n# Prior schema-validated handoff\n\n"
            "This is structured evidence, not a worker transcript or self-authored acceptance.\n\n"
            f"```json\n{json.dumps(prior_handoff, indent=2, sort_keys=True)}\n```\n"
        )
    if controller_interventions:
        prompt += (
            "\n# Durable controller interventions\n\n"
            "These concise tracker records were authored by the configured controller "
            "principal. They are not role transcripts and do not replace independent "
            "acceptance, but every unresolved rework requirement in them must be "
            "addressed explicitly. Do not return an unchanged artifact when an "
            "intervention identifies evidence that contradicts it.\n\n"
            + "\n\n---\n\n".join(controller_interventions)
            + "\n"
        )
    if tracker_context is not None:
        prompt += (
            "\n# Controller-captured tracker snapshot\n\n"
            "The deterministic controller captured this authoritative tracker state "
            "immediately before dispatch. Use it instead of attempting network access. "
            "Materialization revalidates live state after you return.\n\n"
            f"```json\n{json.dumps(tracker_context, indent=2, sort_keys=True)}\n```\n"
            "Planning may proceed while the planning ticket's declared dependencies "
            "are nonterminal. The controller will copy those dependencies onto every "
            "materialized child, so design the DAG now without weakening or omitting "
            "the parent ticket's external gates. Task dependency arrays may contain "
            "only keys from the returned DAG, never GitHub issue references. Do not "
            "report a planner blocker solely because an external gate is nonterminal "
            "or a runtime acceptance input is not yet available; encode those as "
            "fail-closed task prerequisites and stop conditions.\n"
        )
    if hardware_capability is not None:
        prompt += (
            "\n# Registered hardware capability envelope\n\n"
            "Use only the broker client queue capability directory below. You have no "
            "device access and may request only ticketed finite operations.\n\n"
            f"```json\n{json.dumps(hardware_capability, indent=2, sort_keys=True)}\n```\n\n"
            f"{HARDWARE_CAPABILITY_RESTRICTIONS}\n"
        )
    if controller_evidence is not None:
        prompt += (
            "\n# Controller-validated hardware evidence\n\n"
            "This attestation is produced from the host broker's private hash-chained "
            "manifest and freshly rehashed private artifacts, independently of the "
            "worker handoff. Judge from this privacy-safe attestation; the private "
            "manifest is deliberately not exposed to any agent.\n\n"
            f"```json\n{json.dumps(controller_evidence, indent=2, sort_keys=True)}\n```\n"
        )
    return prompt


def build_planner_tracker_context(
    workflow: Workflow,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
    *,
    revision: str | None = None,
) -> dict[str, Any]:
    """Build the concise tracker state a network-isolated planner must rehydrate."""
    issues: list[dict[str, Any]] = []
    for ticket in tickets:
        if not any(label.startswith(STATE_PREFIX) for label in ticket.labels):
            continue
        sections = parse_sections(ticket.body)
        dependencies = sorted(
            {
                int(value)
                for value in ISSUE_REFERENCE.findall(sections.get("Dependencies", ""))
            }
        )
        issues.append(
            {
                "number": ticket.number,
                "title": ticket.title,
                "state": ticket.state,
                "labels": sorted(ticket.labels),
                "url": ticket.url,
                "specification_revision": sections.get("Specification revision", ""),
                "goal": sections.get("Goal", ""),
                "work_package": sections.get("Work package", ""),
                "work_class": sections.get("Work class", ""),
                "autonomy_policy": sections.get("Autonomy policy", ""),
                "dependencies": dependencies,
                "allowed_surfaces": (
                    list(allowed_surfaces(sections["Allowed architectural surfaces"]))
                    if sections.get("Allowed architectural surfaces")
                    else []
                ),
                "existing_pull_request": sections.get("Existing pull request", ""),
                "contract_sha256": hashlib.sha256(
                    ticket.body.encode("utf-8")
                ).hexdigest(),
            }
        )
    return {
        "repository": workflow.repository,
        "base_revision": revision or origin_main_revision(workflow),
        "issues": sorted(issues, key=lambda value: int(value["number"])),
        "open_pull_requests": sorted(
            (dict(item) for item in pull_requests),
            key=lambda value: int(value["number"]),
        ),
    }


def registered_hardware_preflight() -> dict[str, Any]:
    """Fail closed unless doctor sees precisely the two registered writable bridges.

    Doctor intentionally reports unrelated host-tool failures in its exit code.  Its
    JSON device records remain authoritative for this narrowly scoped preflight.
    """
    result = subprocess.run(
        [str(ROOT / "scripts" / "doctor.sh"), "--json"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    try:
        report = json.loads(result.stdout)
        records = report["devices"]["cp2102n"]
    except (KeyError, TypeError, json.JSONDecodeError) as error:
        raise ControlError(
            "registered hardware preflight produced invalid doctor JSON"
        ) from error
    found: dict[str, dict[str, Any]] = {}
    unknown: list[str] = []
    for record in records:
        if not isinstance(record, dict):
            raise ControlError(
                "registered hardware preflight has invalid device record"
            )
        identity = " ".join(
            str(record.get(name, "")) for name in ("path", "target")
        ).casefold()
        serials = [
            serial for serial in REGISTERED_NFF_CP2102N_SERIALS if serial in identity
        ]
        if len(serials) != 1:
            unknown.append(str(record.get("path", "unknown")))
            continue
        serial = serials[0]
        if serial in found:
            raise ControlError(
                "registered hardware preflight found duplicate board identity"
            )
        found[serial] = record
    if unknown or set(found) != REGISTERED_NFF_CP2102N_SERIALS:
        raise ControlError(
            "registered hardware preflight requires exactly the two registered CP2102N boards"
        )
    if any(
        record.get("status") != "available"
        or record.get("kind") != "character"
        or not record.get("readable")
        or not record.get("writable")
        for record in found.values()
    ):
        raise ControlError(
            "registered hardware preflight requires readable and writable CP2102N boards"
        )
    return {
        "schema_version": 1,
        "kind": "registered_nff_cp2102n",
        "preflight": "scripts/doctor.sh --json",
        "doctor_exit_code": result.returncode,
        "registered_serials": sorted(found),
        "ports": [found[serial]["path"] for serial in sorted(found)],
        "restrictions": HARDWARE_CAPABILITY_RESTRICTIONS,
    }


def _trusted_tool_record(path: Path) -> dict[str, str]:
    """Attest tool bytes without discarding multicall invocation aliases."""
    return {
        "path": str(path.absolute()),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def trusted_hardware_tools() -> dict[str, dict[str, str]]:
    """Build/verify host tooling before a worker gets a broker capability."""
    cli = ROOT / "tools/domes-cli/target/release/domes-cli"
    built = subprocess.run(
        [
            "cargo",
            "build",
            "--locked",
            "--release",
            "--manifest-path",
            str(ROOT / "tools/domes-cli/Cargo.toml"),
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if built.returncode or not cli.is_file():
        raise ControlError(
            "trusted controller-root release domes-cli build is unavailable"
        )
    candidates = sorted(
        (Path.home() / ".espressif/python_env").glob("idf5.4_*_env/bin/esptool.py")
    )
    if not candidates:
        raise ControlError("ESP-IDF v5.4.4 esptool environment is unavailable")
    esptool = candidates[-1]
    idf_python = esptool.parent / "python"
    if not idf_python.is_file():
        raise ControlError("ESP-IDF v5.4.4 Python environment is unavailable")
    version = subprocess.run(
        [str(esptool), "version"], check=False, capture_output=True, text=True
    )
    if version.returncode or "esptool.py v4.12.0" not in (
        version.stdout + version.stderr
    ):
        raise ControlError("trusted ESP-IDF v5.4.4 esptool failed version check")
    tools_root = Path.home() / ".espressif" / "tools"
    xtensa_candidates = sorted(
        tools_root.glob("xtensa-esp-elf/*/xtensa-esp-elf/bin/xtensa-esp32s3-elf-gcc")
    )
    ulp_candidates = sorted(
        tools_root.glob("esp32ulp-elf/*/esp32ulp-elf/bin/esp32ulp-elf-as")
    )
    rom_candidates = sorted(tools_root.glob("esp-rom-elfs/*/esp32s3_rev0_rom.elf"))
    if not xtensa_candidates or not ulp_candidates or not rom_candidates:
        raise ControlError("pinned ESP32-S3 build tool paths are unavailable")
    xtensa, ulp, rom = (
        xtensa_candidates[-1],
        ulp_candidates[-1],
        rom_candidates[-1],
    )
    xtensa_version = subprocess.run(
        [str(xtensa), "--version"], check=False, capture_output=True, text=True
    )
    ulp_version = subprocess.run(
        [str(ulp), "--version"], check=False, capture_output=True, text=True
    )
    if (
        xtensa_version.returncode
        or "esp-14.2.0_20260121" not in xtensa_version.stdout
        or ulp_version.returncode
        or "2.38" not in ulp_version.stdout
    ):
        raise ControlError("pinned ESP32-S3 compilers failed version checks")
    idf_root = Path.home() / "esp" / "esp-idf"
    idf_export = idf_root / "export.sh"
    idf_py = idf_root / "tools" / "idf.py"
    idf_tag = subprocess.run(
        [
            "/usr/bin/git",
            "-C",
            str(idf_root),
            "describe",
            "--tags",
            "--exact-match",
            "HEAD",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    idf_revision = subprocess.run(
        ["/usr/bin/git", "-C", str(idf_root), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
    )
    if (
        idf_tag.returncode
        or idf_tag.stdout.strip() != "v5.4.4"
        or idf_revision.returncode
        or not FULL_SHA.fullmatch(idf_revision.stdout.strip())
        or not idf_export.is_file()
        or not idf_py.is_file()
    ):
        raise ControlError("trusted ESP-IDF v5.4.4 build tools are unavailable")

    git = Path("/usr/bin/git")
    bwrap = Path("/usr/bin/bwrap")
    cargo = Path("/usr/bin/cargo")
    cc = Path("/usr/bin/cc")
    prlimit = Path("/usr/bin/prlimit")
    python3 = Path("/usr/bin/python3")
    rustc = Path("/usr/bin/rustc")
    if not all(
        path.is_file() for path in (git, bwrap, cargo, cc, prlimit, python3, rustc)
    ):
        raise ControlError("trusted host git and sandbox build tools are unavailable")
    idf_py_record = _trusted_tool_record(idf_py)
    idf_py_record.update({"version": "v5.4.4", "revision": idf_revision.stdout.strip()})
    recorded = {
        "domes-cli": _trusted_tool_record(cli),
        "bwrap": _trusted_tool_record(bwrap),
        "cargo": _trusted_tool_record(cargo),
        "cc": _trusted_tool_record(cc),
        "prlimit": _trusted_tool_record(prlimit),
        "python3": _trusted_tool_record(python3),
        "rustc": _trusted_tool_record(rustc),
        "esptool": _trusted_tool_record(esptool),
        "idf-python": {
            "path": str(idf_python.absolute()),
            "sha256": hashlib.sha256(idf_python.read_bytes()).hexdigest(),
        },
        "esp-rom-elf": _trusted_tool_record(rom),
        "esp32ulp-elf-as": _trusted_tool_record(ulp),
        "xtensa-esp32s3-elf-gcc": _trusted_tool_record(xtensa),
        "git": _trusted_tool_record(git),
        "idf-export": _trusted_tool_record(idf_export),
        "idf.py": idf_py_record,
    }
    lock = ROOT / "firmware" / "domes" / "dependencies.lock"
    lock_record = _trusted_tool_record(lock)
    recorded["dependencies.lock"] = lock_record
    cache_root = Path.home() / ".cache" / "Espressif" / "ComponentManager"
    for index, (name, version, component_hash) in enumerate(
        _managed_component_requirements(lock)
    ):
        namespace, component = name.split("/", 1)
        destination = f"{namespace}__{component}"
        matches = list(
            cache_root.glob(f"service_*/{destination}_{version}_{component_hash[:8]}")
        )
        if len(matches) != 1:
            raise ControlError(
                f"pinned managed component cache is unavailable: {name}@{version}"
            )
        component_root = matches[0].resolve()
        tree_sha256 = _verified_component_tree(
            component_root, component_hash, idf_python
        )
        recorded[f"managed-component-{index}"] = {
            "path": str(component_root),
            "sha256": tree_sha256,
            "component_hash": component_hash,
            "destination": destination,
            "version": version,
        }
    return recorded


def _managed_component_requirements(lock: Path) -> list[tuple[str, str, str]]:
    """Read the finite service-component records from an ESP-IDF lock file."""
    try:
        lines = lock.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ControlError("firmware dependency lock is unavailable") from error
    records: list[tuple[str, str, str]] = []
    current = ""
    version = ""
    component_hash = ""

    def finish() -> None:
        nonlocal current, version, component_hash
        if current and "/" in current:
            if (
                not re.fullmatch(r"[a-z0-9_.-]+/[a-z0-9_.-]+", current)
                or not re.fullmatch(r"[A-Za-z0-9_.+-]+", version)
                or not re.fullmatch(r"[0-9a-f]{64}", component_hash)
            ):
                raise ControlError("firmware dependency lock is not finite and pinned")
            records.append((current, version, component_hash))
        current = version = component_hash = ""

    in_dependencies = False
    for line in lines:
        if line == "dependencies:":
            in_dependencies = True
            continue
        if in_dependencies and line and not line.startswith(" "):
            finish()
            break
        match = re.fullmatch(r" {2}([^ :][^:]*):", line) if in_dependencies else None
        if match:
            finish()
            if re.fullmatch(r"[a-z0-9_.-]+/[a-z0-9_.-]+", match.group(1)):
                current = match.group(1)
            continue
        if current:
            hash_match = re.fullmatch(r"    component_hash: ([0-9a-f]{64})", line)
            version_match = re.fullmatch(r"    version: ['\"]?([^'\"]+)['\"]?", line)
            if hash_match:
                component_hash = hash_match.group(1)
            elif version_match:
                version = version_match.group(1)
    else:
        finish()
    if not records or len({name for name, _, _ in records}) != len(records):
        raise ControlError("firmware dependency lock has no unique service components")
    return records


def _verified_component_tree(root: Path, expected_hash: str, idf_python: Path) -> str:
    """Verify registry checksums, then hash the entire pinned component tree."""
    if not root.is_dir() or root.is_symlink():
        raise ControlError("managed component cache path is unsafe")
    validated = subprocess.run(
        [
            str(idf_python),
            "-c",
            (
                "from idf_component_tools.hash_tools.validate import "
                "validate_hash_eq_hashdir; import sys; "
                "validate_hash_eq_hashdir(sys.argv[1], sys.argv[2])"
            ),
            str(root),
            expected_hash,
        ],
        check=False,
        capture_output=True,
        text=True,
        env={
            "HOME": "/tmp",
            "LANG": "C.UTF-8",
            "PATH": "/usr/bin:/bin",
            "PYTHONNOUSERSITE": "1",
        },
    )
    if validated.returncode:
        raise ControlError("managed component cache does not match dependency lock")
    try:
        checksums = json.loads((root / "CHECKSUMS.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ControlError("managed component checksum evidence is invalid") from error
    if (
        not isinstance(checksums, dict)
        or checksums.get("algorithm") != "sha256"
        or not isinstance(checksums.get("files"), list)
    ):
        raise ControlError("managed component checksum evidence is invalid")
    listed: set[str] = set()
    for entry in checksums["files"]:
        if not isinstance(entry, dict):
            raise ControlError("managed component checksum entry is invalid")
        relative = entry.get("path")
        digest = entry.get("hash")
        size = entry.get("size")
        if (
            not isinstance(relative, str)
            or relative.startswith("/")
            or ".." in Path(relative).parts
            or not re.fullmatch(r"[0-9a-f]{64}", str(digest))
            or not isinstance(size, int)
            or isinstance(size, bool)
            or relative in listed
        ):
            raise ControlError("managed component checksum entry is unsafe")
        candidate = root / relative
        if (
            not candidate.is_file()
            or candidate.is_symlink()
            or candidate.stat().st_size != size
            or hashlib.sha256(candidate.read_bytes()).hexdigest() != digest
        ):
            raise ControlError("managed component cache failed checksum verification")
        listed.add(relative)
    actual = {
        str(path.relative_to(root))
        for path in root.rglob("*")
        if path.is_file() and path.name not in {".component_hash", "CHECKSUMS.json"}
    }
    if actual != listed or any(path.is_symlink() for path in root.rglob("*")):
        raise ControlError("managed component cache contains unverified entries")
    digest = hashlib.sha256()
    for path in sorted((item for item in root.rglob("*") if item.is_file())):
        relative = str(path.relative_to(root)).encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        data = path.read_bytes()
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def hardware_block_reason(
    ticket: Ticket, sections: dict[str, str], pr_head: str, code: str
) -> dict[str, Any]:
    """Create the controller-owned blocker record; this is never a role handoff."""
    return {
        "schema_version": 1,
        "kind": "controller-hardware-block",
        "code": code,
        "issue": ticket.number,
        "spec_revision": sections["Specification revision"],
        "pr_head": pr_head,
        "resume_state": (
            "agent:verification"
            if ticket.agent_state == "agent:verification"
            else "agent:rework"
        ),
    }


def recoverable_hardware_blockers(
    reason: dict[str, Any],
    ticket: Ticket | None = None,
    sections: dict[str, str] | None = None,
    pr_head: str = "",
) -> bool:
    expected_keys = {
        "schema_version",
        "kind",
        "code",
        "issue",
        "spec_revision",
        "pr_head",
        "resume_state",
    }
    if set(reason) != expected_keys:
        return False
    if (
        reason.get("schema_version") != 1
        or reason.get("kind") != "controller-hardware-block"
    ):
        return False
    if reason.get("code") not in {"preflight-unavailable", "lease-held"}:
        return False
    if reason.get("resume_state") not in {"agent:verification", "agent:rework"}:
        return False
    if (
        isinstance(reason.get("issue"), bool)
        or not isinstance(reason.get("issue"), int)
        or not FULL_SHA.fullmatch(str(reason.get("spec_revision", "")))
        or not FULL_SHA.fullmatch(str(reason.get("pr_head", "")))
    ):
        return False
    if ticket is not None:
        if sections is None:
            return False
        if (
            reason["issue"] != ticket.number
            or reason["spec_revision"] != sections["Specification revision"]
            or reason["pr_head"] != pr_head
        ):
            return False
    return True


def hardware_block_path(ticket_number: int) -> Path:
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    return (
        state_root
        / "domes-agent-control"
        / f"issue-{ticket_number}"
        / "hardware-block.json"
    )


def role_retry_path(ticket_number: int) -> Path:
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    return (
        state_root
        / "domes-agent-control"
        / f"issue-{ticket_number}"
        / "role-retry.json"
    )


def role_schema_sha256(role: str) -> str:
    schema_name = SCHEMA_BY_ROLE.get(role)
    if schema_name is None or role == "selector":
        raise ControlError(f"unsupported retry role: {role}")
    schema_path = ORCHESTRATION_DIR / "schemas" / schema_name
    return hashlib.sha256(schema_path.read_bytes()).hexdigest()


def planner_schema_sha256() -> str:
    """Retain the pending-plan identity helper for planner recovery journals."""
    return role_schema_sha256("planner")


def retry_state_for(item: TicketValidation, role: str) -> str:
    if role == "planner":
        return "agent:plan"
    if role == "worker":
        for state in (item.source_state, item.ticket.agent_state):
            if state in {"agent:ready", "agent:rework"}:
                return state
        return "agent:ready"
    if role == "judge":
        return "agent:agent-review"
    if role == "verification-worker":
        return "agent:verification"
    raise ControlError(f"unsupported retry role: {role}")


def persist_role_retry(
    workflow: Workflow,
    item: TicketValidation,
    error: Exception,
    role: str,
    resume_state: str,
) -> Path:
    """Keep controller-owned role failures retryable instead of inventing blockers."""
    path = role_retry_path(item.ticket.number)
    schema_sha256 = role_schema_sha256(role)
    attempt = 1
    try:
        previous = json.loads(path.read_text(encoding="utf-8"))
        if (
            previous.get("issue") == item.ticket.number
            and previous.get("spec_revision") == item.sections["Specification revision"]
            and previous.get("role") == role
            and previous.get("resume_state") == resume_state
            and previous.get("schema_sha256") == schema_sha256
        ):
            attempt = int(previous.get("attempt", 0)) + 1
    except (OSError, ValueError, TypeError, json.JSONDecodeError):
        pass
    delay = min(
        workflow.poll_interval_seconds * (2 ** min(attempt - 1, 4)),
        workflow.max_retry_backoff_seconds,
    )
    record = {
        "issue": item.ticket.number,
        "spec_revision": item.sections["Specification revision"],
        "role": role,
        "resume_state": resume_state,
        "schema_sha256": schema_sha256,
        "attempt": attempt,
        "retry_after": int(time.time()) + delay,
        "error_class": type(error).__name__,
        "error_sha256": hashlib.sha256(str(error).encode("utf-8")).hexdigest(),
    }
    write_handoff(path, record)
    return path


def deferred_role_retries(tickets: Sequence[Ticket]) -> dict[int, int]:
    """Return seconds remaining for valid controller-owned retry journals."""
    now = int(time.time())
    deferred: dict[int, int] = {}
    for ticket in tickets:
        if ticket.state != "OPEN" or ticket.agent_state not in ROLE_BY_STATE:
            continue
        sections = parse_sections(ticket.body)
        path = role_retry_path(ticket.number)
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        role = str(record.get("role", ""))
        resume_state = str(record.get("resume_state", ""))
        if (
            record.get("issue") != ticket.number
            or record.get("spec_revision") != sections.get("Specification revision")
            or resume_state != ticket.agent_state
            or ROLE_BY_STATE.get(resume_state) != role
            or not isinstance(record.get("retry_after"), int)
        ):
            continue
        try:
            schema_sha256 = role_schema_sha256(role)
        except ControlError:
            path.unlink(missing_ok=True)
            continue
        if record.get("schema_sha256") != schema_sha256:
            path.unlink(missing_ok=True)
            continue
        remaining = int(record["retry_after"]) - now
        if remaining > 0:
            deferred[ticket.number] = remaining
    return deferred


def persist_hardware_block(
    ticket: Ticket, sections: dict[str, str], pr_head: str, code: str
) -> Path:
    reason = hardware_block_reason(ticket, sections, pr_head, code)
    if not recoverable_hardware_blockers(reason, ticket, sections, pr_head):
        raise ControlError("refusing to persist an invalid hardware blocker record")
    path = hardware_block_path(ticket.number)
    write_handoff(path, reason)
    return path


def recover_hardware_blocked_tickets(
    workflow: Workflow, tickets: Sequence[Ticket], hardware_capability: dict[str, Any]
) -> list[int]:
    """Requeue only a worker handoff whose complete blocker set is recoverable."""
    recovered: list[int] = []
    for ticket in tickets:
        if ticket.agent_state != "agent:blocked":
            continue
        sections = parse_sections(ticket.body)
        if not requires_registered_hardware(sections):
            continue
        path = hardware_block_path(ticket.number)
        try:
            reason = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        pr_head = sections["Specification revision"]
        if existing_pull_request(sections):
            try:
                pr_head = load_pull_request(
                    workflow, existing_pull_request(sections)
                ).head_oid
            except ControlError:
                continue
        if recoverable_hardware_blockers(reason, ticket, sections, pr_head):
            transition(workflow, ticket, str(reason["resume_state"]))
            path.unlink(missing_ok=True)
            recovered.append(ticket.number)
    return recovered


def block_hardware_preflight_tickets(
    workflow: Workflow, tickets: Sequence[Ticket], error: str
) -> list[int]:
    """Persist a typed, artifact-bound block; never infer recovery from prose."""
    blocked: list[int] = []
    for ticket in tickets:
        # A running worker may be holding a valid process/lease; never replace its
        # handoff merely because a later global preflight is transiently unavailable.
        if ticket.agent_state not in {
            "agent:ready",
            "agent:rework",
            "agent:verification",
        }:
            continue
        sections = parse_sections(ticket.body)
        try:
            if (
                not requires_registered_hardware(sections)
                or role_for(ticket) != "verification-worker"
            ):
                continue
        except ControlError:
            continue
        pr_head = sections["Specification revision"]
        if existing_pull_request(sections):
            try:
                pr_head = load_pull_request(
                    workflow, existing_pull_request(sections)
                ).head_oid
            except ControlError:
                pass
        persist_hardware_block(ticket, sections, pr_head, "preflight-unavailable")
        transition(workflow, ticket, "agent:blocked")
        blocked.append(ticket.number)
    return blocked


def _git(*arguments: str, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    for key in ("GIT_EXTERNAL_DIFF", "GIT_DIFF_OPTS", "GIT_CONFIG_COUNT"):
        environment.pop(key, None)
    environment.update(
        {
            "GIT_PAGER": "cat",
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_ATTR_NOSYSTEM": "1",
        }
    )
    return subprocess.run(
        [
            "git",
            "-c",
            "core.fsmonitor=false",
            "-c",
            "core.hooksPath=/dev/null",
            "-c",
            "core.attributesFile=/dev/null",
            "-c",
            "core.excludesFile=/dev/null",
            "-c",
            "protocol.ext.allow=never",
            *arguments,
        ],
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )


def origin_main_revision(workflow: Workflow) -> str:
    resolved = _git("rev-parse", f"origin/{workflow.base_branch}")
    revision = resolved.stdout.strip()
    if resolved.returncode != 0 or not FULL_SHA.fullmatch(revision):
        raise ControlError(f"cannot resolve origin/{workflow.base_branch}")
    return revision


def trusted_repository_url(workflow: Workflow) -> str:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", workflow.repository):
        raise ControlError("workflow repository is not a finite GitHub slug")
    return f"https://github.com/{workflow.repository}.git"


def refresh_base_branch(workflow: Workflow) -> None:
    with _BASE_REFRESH_LOCK:
        refreshed = _git("fetch", "--quiet", "origin", workflow.base_branch)
    if refreshed.returncode != 0:
        raise TrackerError(
            refreshed.stderr.strip() or f"cannot refresh origin/{workflow.base_branch}"
        )


def load_selector_snapshot(
    workflow: Workflow,
) -> tuple[str, list[Ticket], list[dict[str, Any]], str]:
    """Read one revision-stable tracker snapshot for selection and mutation."""
    with _BASE_REFRESH_LOCK:
        refresh_base_branch(workflow)
        revision = origin_main_revision(workflow)
        tickets = load_live_tickets(workflow)
        pull_requests = load_open_pull_request_snapshot(workflow)
        if origin_main_revision(workflow) != revision:
            raise TrackerError(
                f"origin/{workflow.base_branch} changed while reading selector state"
            )
        fingerprint = selector_snapshot_fingerprint(
            workflow,
            tickets,
            pull_requests,
            revision=revision,
        )
    return revision, tickets, pull_requests, fingerprint


def _assert_clean_workspace(path: Path, *, issue: int) -> None:
    check = _git("rev-parse", "--is-inside-work-tree", cwd=path)
    if check.returncode != 0 or check.stdout.strip() != "true":
        raise ControlError(f"issue #{issue}: refusing non-Git workspace: {path}")
    top_level = _git("rev-parse", "--show-toplevel", cwd=path)
    git_dir = _git("rev-parse", "--path-format=absolute", "--git-dir", cwd=path)
    try:
        resolved_workspace = path.resolve(strict=True)
        resolved_top_level = Path(top_level.stdout.strip()).resolve(strict=True)
        resolved_git_dir = Path(git_dir.stdout.strip()).resolve(strict=True)
        resolved_git_dir.relative_to(resolved_workspace)
    except (OSError, ValueError) as error:
        raise ControlError(
            f"issue #{issue}: workspace must own private Git metadata"
        ) from error
    if (
        top_level.returncode != 0
        or git_dir.returncode != 0
        or resolved_top_level != resolved_workspace
        or not resolved_git_dir.is_dir()
        or (resolved_git_dir / "objects" / "info" / "alternates").is_file()
    ):
        raise ControlError(f"issue #{issue}: workspace must own private Git metadata")
    dirty = _git("status", "--porcelain", "--untracked-files=all", cwd=path)
    if dirty.returncode != 0 or dirty.stdout.strip():
        raise ControlError(
            f"issue #{issue}: existing agent workspace has uncommitted changes"
        )


def _clone_agent_workspace(workspace: Path, *, issue: int) -> None:
    """Create an isolated clone whose writable Git metadata stays in the sandbox."""
    remote = _git("remote", "get-url", "origin", cwd=ROOT)
    if remote.returncode != 0 or not remote.stdout.strip():
        raise ControlError(f"issue #{issue}: cannot resolve origin URL")
    workspace.parent.mkdir(parents=True, exist_ok=True)
    staging_root = Path(
        tempfile.mkdtemp(prefix=f".issue-{issue}-clone-", dir=workspace.parent)
    )
    staged_workspace = staging_root / "repository"
    try:
        cloned = _git(
            "clone",
            "--no-local",
            "--no-checkout",
            str(ROOT),
            str(staged_workspace),
            cwd=ROOT,
        )
        if cloned.returncode != 0:
            raise ControlError(
                cloned.stderr.strip()
                or f"issue #{issue}: cannot create isolated agent clone"
            )
        configured = _git(
            "remote",
            "set-url",
            "origin",
            remote.stdout.strip(),
            cwd=staged_workspace,
        )
        if configured.returncode != 0:
            raise ControlError(
                configured.stderr.strip()
                or f"issue #{issue}: cannot configure agent clone origin"
            )
        staged_workspace.replace(workspace)
        staging_root.rmdir()
    except Exception:
        shutil.rmtree(staging_root, ignore_errors=True)
        raise


def _prepare_agent_workspace(
    workflow: Workflow,
    workspace: Path,
    item: TicketValidation,
    role: str,
    pull_request: PullRequest | None,
    stack: StackContext | None = None,
) -> None:
    refreshed = _git("fetch", "--quiet", "--prune", "origin", cwd=workspace)
    if refreshed.returncode != 0:
        raise ControlError(
            refreshed.stderr.strip()
            or f"issue #{item.ticket.number}: cannot refresh agent clone"
        )
    required_remote_refs = [f"refs/remotes/origin/{workflow.base_branch}"]
    if stack is not None:
        required_remote_refs.append(f"refs/remotes/origin/{stack.base_ref}")
    if pull_request is not None:
        required_remote_refs.append(f"refs/remotes/origin/{pull_request.head_ref}")
    for remote_ref in required_remote_refs:
        resolved = _git("show-ref", "--verify", "--quiet", remote_ref, cwd=workspace)
        if resolved.returncode != 0:
            raise ControlError(
                f"issue #{item.ticket.number}: agent clone is missing {remote_ref}"
            )
    if stack is not None:
        stack_ref = _git("rev-parse", f"origin/{stack.base_ref}", cwd=workspace)
        if stack_ref.returncode != 0 or stack_ref.stdout.strip() != stack.base_head:
            raise ControlError(
                f"issue #{item.ticket.number}: stack parent branch moved before workspace preparation"
            )

    if pull_request is not None:
        branch = pull_request.head_ref
        remote_ref = f"refs/remotes/origin/{branch}"
        exists = _git(
            "show-ref", "--verify", "--quiet", f"refs/heads/{branch}", cwd=workspace
        )
        if exists.returncode == 0:
            switched = _git("checkout", "--quiet", branch, cwd=workspace)
            if switched.returncode == 0:
                switched = _git(
                    "merge", "--quiet", "--ff-only", remote_ref, cwd=workspace
                )
        else:
            switched = _git(
                "checkout",
                "--quiet",
                "-b",
                branch,
                "--track",
                remote_ref,
                cwd=workspace,
            )
        if switched.returncode != 0:
            raise ControlError(
                switched.stderr.strip()
                or f"issue #{item.ticket.number}: cannot check out PR branch {branch}"
            )
        head = _git("rev-parse", "HEAD", cwd=workspace)
        if head.returncode != 0 or head.stdout.strip() != pull_request.head_oid:
            raise ControlError(
                f"issue #{item.ticket.number}: agent clone does not match PR head"
            )
    elif role in {"worker", "verification-worker"}:
        branch = f"codex/issue-{item.ticket.number}"
        local_ref = f"refs/heads/{branch}"
        remote_ref = f"refs/remotes/origin/{branch}"
        local_exists = _git("show-ref", "--verify", "--quiet", local_ref, cwd=workspace)
        remote_exists = _git(
            "show-ref", "--verify", "--quiet", remote_ref, cwd=workspace
        )
        if local_exists.returncode == 0:
            switched = _git("checkout", "--quiet", branch, cwd=workspace)
            if switched.returncode == 0 and remote_exists.returncode == 0:
                switched = _git(
                    "merge", "--quiet", "--ff-only", remote_ref, cwd=workspace
                )
        elif remote_exists.returncode == 0:
            switched = _git(
                "checkout",
                "--quiet",
                "-b",
                branch,
                "--track",
                remote_ref,
                cwd=workspace,
            )
        else:
            base = (
                stack.base_head
                if stack is not None
                else f"origin/{workflow.base_branch}"
            )
            switched = _git(
                "checkout",
                "--quiet",
                "-b",
                branch,
                base,
                cwd=workspace,
            )
        if switched.returncode != 0:
            raise ControlError(
                switched.stderr.strip()
                or f"issue #{item.ticket.number}: cannot prepare branch {branch}"
            )
    else:
        detached = _git(
            "checkout",
            "--quiet",
            "--detach",
            item.sections["Specification revision"],
            cwd=workspace,
        )
        if detached.returncode != 0:
            raise ControlError(
                detached.stderr.strip()
                or f"issue #{item.ticket.number}: cannot check out specification"
            )

    authority_modules = ROOT / ".gitmodules"
    candidate_modules = workspace / ".gitmodules"
    if authority_modules.is_file():
        if (
            not candidate_modules.is_file()
            or hashlib.sha256(candidate_modules.read_bytes()).digest()
            != hashlib.sha256(authority_modules.read_bytes()).digest()
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: untrusted submodule configuration"
            )
        submodules = _git("submodule", "update", "--init", "--recursive", cwd=workspace)
        if submodules.returncode != 0:
            raise ControlError(
                submodules.stderr.strip()
                or f"issue #{item.ticket.number}: cannot initialize submodules"
            )
    elif candidate_modules.exists():
        raise ControlError(
            f"issue #{item.ticket.number}: untrusted submodule configuration"
        )
    _assert_clean_workspace(workspace, issue=item.ticket.number)


def ensure_workspace(
    workflow: Workflow,
    item: TicketValidation,
    role: str,
    stack: StackContext | None = None,
) -> Path:
    workspace = workflow.workspace_root / f"issue-{item.ticket.number}"
    pull_request_number = existing_pull_request(item.sections)
    pull_request: PullRequest | None = None
    if pull_request_number:
        pull_request = load_pull_request(workflow, pull_request_number)
        expected_base = stack.base_ref if stack is not None else workflow.base_branch
        rebasing_stacked_child = (
            role == "worker"
            and (item.source_state or item.ticket.agent_state) == "agent:rework"
            and automated_delivery(item.sections)
            and item.sections.get("Work class", "").strip() == "software"
            and not hardware_operations(item.sections)
            and pull_request.base_ref.startswith("codex/issue-")
            and pull_request.base_ref != expected_base
        )
        if (
            stack is None
            and role in {"judge", "verification-worker"}
            and pull_request.base_ref != workflow.base_branch
        ):
            raise StackInvalidated(
                f"issue #{item.ticket.number}: stacked base is no longer active; "
                "returning the child to main-based rework"
            )
        if pull_request.state != "OPEN" or (
            pull_request.base_ref != expected_base and not rebasing_stacked_child
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: existing PR must be open against "
                f"{workflow.base_branch}"
            )
    # The previous role owned its repository metadata. Durable work must already
    # be pushed, so discard it without invoking Git and start from trusted state.
    workflow.workspace_root.mkdir(parents=True, exist_ok=True)
    workspace_root = workflow.workspace_root.resolve(strict=True)
    if (
        workspace.parent.resolve(strict=True) != workspace_root
        or workspace.name != f"issue-{item.ticket.number}"
    ):
        raise ControlError(f"issue #{item.ticket.number}: unsafe workspace path")
    if workspace.is_symlink():
        workspace.unlink()
    elif workspace.exists():
        shutil.rmtree(workspace)
    _clone_agent_workspace(workspace, issue=item.ticket.number)
    _prepare_agent_workspace(
        workflow,
        workspace,
        item,
        role,
        pull_request,
        stack,
    )
    return workspace


def transition(workflow: Workflow, ticket: Ticket, new_state: str) -> None:
    if ticket.agent_labels == (new_state,):
        return
    command = ["gh", "issue", "edit", str(ticket.number), "--repo", workflow.repository]
    for label in ticket.agent_labels:
        if label != new_state:
            command.extend(("--remove-label", label))
    if new_state not in ticket.agent_labels:
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


def block_invalid_human_merged_stack(
    workflow: Workflow,
    ticket: Ticket,
    reason: str,
) -> dict[str, Any]:
    """Fail closed after a human merge loses its only accepted integration path."""
    transition(workflow, ticket, "agent:blocked")
    post_controller_comment(
        workflow,
        ticket,
        "Agent control-plane transition (stacked pull request)",
        reason,
    )
    return {"issue": ticket.number, "state": "agent:blocked"}


def set_issue_priority(workflow: Workflow, ticket: Ticket, priority: str) -> None:
    target = f"priority:{priority}"
    existing = tuple(
        label for label in ticket.labels if re.fullmatch(r"priority:p[0-9]+", label)
    )
    if existing == (target,):
        return
    command = ["gh", "issue", "edit", str(ticket.number), "--repo", workflow.repository]
    for label in existing:
        if label != target:
            command.extend(("--remove-label", label))
    if target not in existing:
        command.extend(("--add-label", target))
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
    hardware_operations: Sequence[str] = (),
    hardware_boards: Sequence[int] = (),
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
        "Hardware operations": (
            ", ".join(hardware_operations) if hardware_operations else "None"
        ),
        "Hardware boards": (
            ", ".join(str(alias) for alias in hardware_boards)
            if hardware_boards
            else "None"
        ),
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
    *,
    expected_revision: str | None = None,
) -> None:
    state = result["state"]
    if state != "selected":
        if (
            result["mode"] != "none"
            or result["autonomy_policy"] != "none"
            or result["priority"] != "none"
            or result["work_class"] != "none"
            or int(result["existing_issue"]) != 0
            or int(result["existing_pull_request"]) != 0
        ):
            raise ControlError(
                "idle or blocked selector result must use the empty `none` envelope"
            )
        return
    revision = expected_revision or origin_main_revision(workflow)
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
    if result["autonomy_policy"] != AUTOMATED_REVIEW_POLICY:
        raise ControlError(
            "autonomous selector must use the software-review-required policy"
        )
    surfaces = allowed_surfaces("\n".join(result["allowed_surfaces"]))
    if result["autonomy_policy"] == AUTOMATED_REVIEW_POLICY:
        protected = protected_autonomous_surfaces(surfaces, policy)
        if protected:
            raise ControlError(
                "selector requested autonomous delivery for protected surfaces: "
                + ", ".join(protected)
            )
    by_number = {ticket.number: ticket for ticket in tickets}
    issue_number = int(result["existing_issue"])
    if issue_number:
        issue = by_number.get(issue_number)
        if (
            issue is None
            or issue.state != "OPEN"
            or terminal(issue)
            or issue.agent_state != "agent:needs-specification"
        ):
            raise ControlError("selector referenced an unavailable existing issue")
        issue_validation = validate_ticket(issue, check_revision=False)
        dependencies_ok, _ = stack_dependency_status(
            workflow, issue_validation, tickets
        )
        if not dependencies_ok:
            raise ControlError(
                "selector referenced a dependency-blocked existing issue"
            )
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
        if result["autonomy_policy"] == AUTOMATED_REVIEW_POLICY:
            pull_request = load_pull_request(workflow, pull_request_number)
            outside = paths_outside_surfaces(pull_request.files, surfaces)
            protected = protected_autonomous_paths(pull_request.files, policy)
            if outside or protected:
                raise ControlError(
                    "selector existing pull request violates path policy: "
                    + ", ".join(outside or protected)
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
    workflow: Workflow,
    policy: AutopilotPolicy,
    result: dict[str, Any],
    expected_fingerprint: str,
) -> Ticket | None:
    if result["state"] != "selected":
        return None
    with _SELECTOR_MATERIALIZATION_LOCK, _BASE_REFRESH_LOCK:
        return _apply_selector_result_locked(
            workflow, policy, result, expected_fingerprint
        )


def _apply_selector_result_locked(
    workflow: Workflow,
    policy: AutopilotPolicy,
    result: dict[str, Any],
    expected_fingerprint: str,
) -> Ticket:
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
    revision, tickets, pull_requests, actual_fingerprint = load_selector_snapshot(
        workflow
    )
    if actual_fingerprint != expected_fingerprint:
        raise ControlError("live tracker state changed before selector mutation")
    validate_selector_result(
        result,
        workflow,
        policy,
        tickets,
        pull_requests,
        expected_revision=revision,
    )

    existing_number = int(result["existing_issue"])
    if existing_number:
        ticket = next(ticket for ticket in tickets if ticket.number == existing_number)
        body = with_autopilot_contract("", contract)
        validation_ticket = _ticket_from_selection(
            ticket.number, body, result, ticket.url
        )
        validation = validate_ticket(validation_ticket)
        if not validation.valid:
            raise ControlError(
                f"selector produced invalid ticket #{ticket.number}: "
                + "; ".join(validation.errors)
            )
        expected_match = AUTOPILOT_MARKER_RE.search(body)
        assert expected_match is not None
        expected_marker = expected_match.group(0)
        current_match = AUTOPILOT_MARKER_RE.search(ticket.body)
        if current_match is not None and current_match.group(0) != expected_marker:
            raise ControlError(
                f"selector refuses to overwrite issue #{ticket.number} contract"
            )
        labels = sorted(
            {
                *(
                    label
                    for label in ticket.labels
                    if not label.startswith(STATE_PREFIX)
                    and not re.fullmatch(r"priority:p[0-9]+", label)
                ),
                target_state,
                f"priority:{result['priority']}",
            }
        )
        payload: dict[str, Any] = {"labels": labels}
        if current_match is None:
            payload["body"] = body
        updated = subprocess.run(
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
            input=json.dumps(payload),
        )
        if updated.returncode != 0:
            refreshed = load_live_tickets(workflow)
            reconciled = next(
                (item for item in refreshed if item.number == ticket.number), None
            )
            if (
                reconciled is None
                or expected_marker not in reconciled.body
                or set(reconciled.labels) != set(labels)
            ):
                raise ControlError(
                    updated.stderr.strip()
                    or f"failed to materialize selected issue #{ticket.number}"
                )
        return validation_ticket

    body = with_autopilot_contract("", contract)
    digest = contract_digest(parse_sections(body))
    marker = f"domes-autopilot-contract:v1 digest={digest}"
    matches = [ticket for ticket in tickets if marker in ticket.body]
    if len(matches) > 1:
        raise ControlError("multiple issues carry the selected contract marker")
    if matches:
        return matches[0]
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
        refreshed = load_live_tickets(workflow)
        matches = [ticket for ticket in refreshed if marker in ticket.body]
        if len(matches) == 1:
            return matches[0]
        raise ControlError(created.stderr.strip() or "failed to create selected issue")
    match = re.search(r"/issues/([1-9][0-9]*)", created.stdout)
    refreshed = load_live_tickets(workflow)
    matches = [ticket for ticket in refreshed if marker in ticket.body]
    if len(matches) != 1:
        raise ControlError("cannot reconcile selected issue creation")
    if match and matches[0].number != int(match.group(1)):
        raise ControlError("selected issue creation returned a conflicting identity")
    return matches[0]


def available_role_slots(maximum: int, active_roles: int, selector_active: bool) -> int:
    """Return controller capacity after reserving the singleton selector slot."""
    if maximum < 1 or active_roles < 0 or active_roles > maximum:
        raise ControlError("invalid controller capacity accounting")
    available = maximum - active_roles - int(selector_active)
    if available < 0:
        raise ControlError("selector exceeds controller capacity")
    return available


def selector_capacity_available(
    maximum: int, active_roles: int, selector_active: bool
) -> bool:
    """Selectors fill a free role slot; CI and human review never consume one."""
    return (
        not selector_active
        and available_role_slots(maximum, active_roles, selector_active) > 0
    )


def build_selector_prompt(
    workflow: Workflow,
    policy: AutopilotPolicy,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
    *,
    revision: str | None = None,
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
        f"Current origin/main revision: {revision or origin_main_revision(workflow)}\n"
        f"Autopilot policy: {policy.policy_name}\n"
        f"Allowed work classes: {', '.join(policy.allowed_work_classes)}\n\n"
        "# Live open issue summary\n\n"
        f"```json\n{json.dumps(issue_snapshot, indent=2, sort_keys=True)}\n```\n\n"
        "# Live open pull-request summary\n\n"
        f"```json\n{json.dumps(pull_requests, indent=2, sort_keys=True)}\n```\n"
    )


def selector_snapshot_fingerprint(
    workflow: Workflow,
    tickets: Sequence[Ticket],
    pull_requests: Sequence[dict[str, Any]],
    *,
    revision: str | None = None,
) -> str:
    """Bind a selector decision to the live authority snapshot it inspected."""
    open_issues = [
        {
            "number": ticket.number,
            "title": ticket.title,
            "state": ticket.state,
            "labels": sorted(ticket.labels),
            "body_sha256": hashlib.sha256(ticket.body.encode("utf-8")).hexdigest(),
        }
        for ticket in tickets
        if ticket.state == "OPEN"
    ]
    document = {
        "revision": revision or origin_main_revision(workflow),
        "issues": sorted(open_issues, key=lambda item: int(item["number"])),
        "pull_requests": sorted(
            (dict(item) for item in pull_requests),
            key=lambda item: int(item["number"]),
        ),
    }
    canonical = json.dumps(document, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


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
    accepted_fingerprint = ""
    result: dict[str, Any]
    selected: Ticket | None = None
    for attempt in range(1, 4):
        try:
            (
                prompt_revision,
                snapshot_tickets,
                snapshot_pull_requests,
                prompt_fingerprint,
            ) = load_selector_snapshot(workflow)
            prompt = build_selector_prompt(
                workflow,
                policy,
                snapshot_tickets,
                snapshot_pull_requests,
                revision=prompt_revision,
            )
        except (ControlError, OSError, json.JSONDecodeError) as error:
            failures.append(f"attempt {attempt} snapshot: {error}")
            if attempt < 3:
                time.sleep(
                    min(
                        10 * (2 ** (attempt - 1)),
                        workflow.max_retry_backoff_seconds,
                    )
                )
            continue
        if failures:
            prompt += (
                "\n\n# Previous selector attempt was rejected\n\n"
                f"Correct this validation failure: {failures[-1][:1000]}\n"
            )
        returncode, failure = run_codex_attempt(
            command,
            prompt,
            event_path,
            stderr_path,
            workflow.stall_timeout_seconds,
            lease_path,
        )
        if returncode != 0:
            failures.append(failure)
        else:
            try:
                result = json.loads(result_path.read_text(encoding="utf-8"))
                (
                    fresh_revision,
                    fresh_tickets,
                    fresh_pull_requests,
                    fresh_fingerprint,
                ) = load_selector_snapshot(workflow)
                if fresh_fingerprint != prompt_fingerprint:
                    raise ControlError(
                        "live issue, pull-request, or main state changed while selecting"
                    )
                validate_selector_result(
                    result,
                    workflow,
                    policy,
                    fresh_tickets,
                    fresh_pull_requests,
                    expected_revision=fresh_revision,
                )
                selected = apply_selector_result(
                    workflow,
                    policy,
                    result,
                    fresh_fingerprint,
                )
            except (ControlError, OSError, json.JSONDecodeError) as error:
                failures.append(f"attempt {attempt} validation: {error}")
            else:
                accepted_fingerprint = fresh_fingerprint
                break
        if attempt < 3:
            time.sleep(
                min(10 * (2 ** (attempt - 1)), workflow.max_retry_backoff_seconds)
            )
    else:
        raise ControlError(
            "autonomous selector failed after 3 attempts: " + "; ".join(failures)
        )
    write_handoff(run_root / "handoff-selector.json", result)
    return {
        "state": result["state"],
        "issue": selected.number if selected is not None else 0,
        "work_package": result["work_package"],
        "result": str(result_path),
        "events": str(event_path),
        "stderr": str(stderr_path),
        "snapshot": accepted_fingerprint,
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
    hardware_attested: bool = False,
) -> str:
    worker_has_incomplete_evidence = role == "worker" and any(
        isinstance(record, dict)
        and record.get("status") in {"failed", "pending", "unavailable"}
        for record in result.get("verification", [])
    )
    if (
        role == "worker"
        and result["blockers"]
        and result.get("pull_request") is None
        and not (
            ticket_sections is not None
            and requires_registered_hardware(ticket_sections)
        )
    ):
        return "agent:blocked"
    if (
        role == "worker"
        and result["blockers"]
        and result.get("pull_request") is not None
        and worker_has_incomplete_evidence
    ):
        # An existing PR does not make an incomplete or unpublished repair
        # reviewable. Keep it in the worker loop instead of spending a judge
        # cycle on the stale remote head.
        return "agent:rework"
    if role == "planner" and result["blockers"]:
        return "agent:blocked"
    if role in NEXT_STATE:
        return NEXT_STATE[role]
    if role == "judge":
        approved_state = "agent:verification"
        if (
            autopilot
            and ticket_sections is not None
            and automated_delivery(ticket_sections)
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
            and automated_delivery(ticket_sections)
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


def validate_result_semantics(
    role: str,
    result: dict[str, Any],
    *,
    allow_deferred_hardware: bool = False,
) -> None:
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
        duplicate_hardware_operations = [
            task["key"]
            for task in tasks
            if len(task.get("hardware_operations", ()))
            != len(set(task.get("hardware_operations", ())))
        ]
        if duplicate_hardware_operations:
            raise ControlError(
                "planner task hardware operations must be unique: "
                + ", ".join(sorted(duplicate_hardware_operations))
            )
        task_dependencies = {task["key"]: tuple(task["dependencies"]) for task in tasks}
        fan_in = sorted(
            key
            for key, dependencies in task_dependencies.items()
            if len(dependencies) > 1
        )
        if fan_in:
            raise ControlError(
                "planner result contains multi-parent task dependencies unsupported "
                "by one-level review stacks; serialize each join: " + ", ".join(fan_in)
            )
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
        approved_statuses = statuses == {"met"}
        if allow_deferred_hardware:
            approved_statuses = (
                "met" in statuses
                and "not_met" not in statuses
                and statuses <= {"met", "not_verifiable"}
            )
        if result["verdict"] == "approve" and (
            not approved_statuses or result["required_rework"]
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


def validate_hardware_verification_result(
    item: TicketValidation,
    result: dict[str, Any],
    attestation: dict[str, Any],
) -> None:
    """Prevent a hardware worker from bypassing the final independent judge."""
    state = result.get("state")
    blockers = result.get("blockers")
    checks = result.get("checks")
    if state == "human_review":
        raise ControlError(
            f"issue #{item.ticket.number}: successful hardware verification "
            "must return through the final independent judge"
        )
    if state == "blocked":
        has_failed_or_pending_check = isinstance(checks, list) and any(
            isinstance(check, dict) and check.get("status") in {"failed", "pending"}
            for check in checks
        )
        if (
            not isinstance(blockers, list)
            or not blockers
            or (
                not has_failed_or_pending_check
                and attestation.get("failed_event_count", 0) < 1
            )
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: blocked hardware verification "
                "requires a concrete failed or pending check"
            )
    elif state != "agent_review":
        raise ControlError(
            f"issue #{item.ticket.number}: hardware verification returned an "
            "unsupported state"
        )


def hardware_attestation_artifact_head(
    item: TicketValidation,
    result: dict[str, Any],
    checkpoint_head: str,
) -> str:
    """Bind broker evidence to the judged head when verification pushes a repair."""
    returned_head = str(result.get("commit", ""))
    if returned_head == checkpoint_head:
        return returned_head
    if result.get("state") != "agent_review" or not result.get("repairs"):
        raise ControlError(
            f"issue #{item.ticket.number}: changed hardware-verification artifact "
            "must be a repair returning through independent review"
        )
    return checkpoint_head


def normalized_plan(result: dict[str, Any]) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for task in sorted(result["tasks"], key=lambda item: item["key"]):
        normalized.append(
            {
                "key": task["key"],
                "mode": task["mode"],
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
                "hardware_operations": sorted(task.get("hardware_operations", ())),
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


def planner_depth(parent: TicketValidation, tickets: Sequence[Ticket]) -> int:
    """Return tracked recursive-plan depth and reject malformed ancestry."""
    by_number = {ticket.number: ticket for ticket in tickets}
    current = parent.ticket
    visited: set[int] = set()
    depth = 0
    while True:
        if current.number in visited:
            raise ControlError("recursive planner ancestry contains a cycle")
        visited.add(current.number)
        marker = PLAN_TASK_MARKER_RE.search(current.body)
        if marker is None:
            return depth
        parent_number = int(marker.group(1))
        if parent_number not in by_number:
            raise ControlError(
                f"recursive planner ancestry is missing issue #{parent_number}"
            )
        current = by_number[parent_number]
        depth += 1


def materialize_plan(
    workflow: Workflow,
    parent: TicketValidation,
    result: dict[str, Any],
) -> list[int]:
    if result["blockers"]:
        return []
    parent_policy = autonomy_policy(parent.sections)
    if parent_policy not in AUTOMATED_DELIVERY_POLICIES:
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
        if not set(task.get("hardware_operations", ())).issubset(
            hardware_operations(parent.sections)
        ):
            raise ControlError(
                f"planner task {task['key']} expands parent hardware operations"
            )

    plan_hash = plan_digest(result)
    tickets = load_live_tickets(workflow)
    depth = planner_depth(parent, tickets)
    if any(task["mode"] == "plan" for task in result["tasks"]):
        if depth >= MAX_RECURSIVE_PLANNER_DEPTH:
            raise ControlError(
                "planner result exceeds the tracked recursive planning depth limit"
            )
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
    inherited_dependencies = tuple(
        dict.fromkeys((parent.ticket.number, *parent.dependencies))
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
                dependencies=inherited_dependencies,
                required_proof=task["required_proof"],
                work_package=parent.sections.get("Work package", task["key"]),
                work_class=parent.sections.get("Work class", "software"),
                selected_policy=task["autonomy_policy"],
                hardware_operations=task.get("hardware_operations", ()),
                hardware_boards=(
                    hardware_boards(parent.sections)
                    if task.get("hardware_operations")
                    else ()
                ),
            )
            body = marker + "\n\n" + with_autopilot_contract("", provisional_contract)
            child = create_issue(
                workflow,
                title=(
                    f"[Plan] {task['key']}: {task['goal']}"
                    if task["mode"] == "plan"
                    else f"[Agent] {task['key']}: {task['goal']}"
                ),
                body=body,
                labels=("agent:needs-specification", priority_label),
            )
            by_uid[uid] = child
        task_numbers[task["key"]] = child.number
        created_by_key[task["key"]] = child

    for task in topological_tasks(result["tasks"]):
        child = created_by_key[task["key"]]
        dependencies = list(inherited_dependencies)
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
            hardware_operations=task.get("hardware_operations", ()),
            hardware_boards=(
                hardware_boards(parent.sections)
                if task.get("hardware_operations")
                else ()
            ),
        )
        task_marker = PLAN_TASK_MARKER_RE.search(child.body)
        assert task_marker is not None
        expected_body = (
            task_marker.group(0) + "\n\n" + with_autopilot_contract("", contract)
        )
        target_state = "agent:plan" if task["mode"] == "plan" else "agent:ready"
        if child.body != expected_body:
            if child.agent_state not in {
                "agent:needs-specification",
                "agent:plan",
                "agent:ready",
            }:
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
            + (target_state,),
            child.url,
        )
        validation = validate_ticket(ready_ticket)
        if not validation.valid:
            raise ControlError(
                f"materialized task {task['key']} is invalid: "
                + "; ".join(validation.errors)
            )
        if child.agent_state != target_state:
            transition(workflow, child, target_state)
    return [task_numbers[task["key"]] for task in topological_tasks(result["tasks"])]


def write_handoff(path: Path, result: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def persist_pending_plan(path: Path, result: dict[str, Any]) -> None:
    write_handoff(
        path,
        {
            "schema_sha256": planner_schema_sha256(),
            "result": result,
        },
    )


def load_pending_plan(
    path: Path, *, issue: int, spec_revision: str
) -> dict[str, Any] | None:
    """Load only a planner journal produced under the current output contract."""
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
        result = document["result"]
        if document.get("schema_sha256") != planner_schema_sha256() or not isinstance(
            result, dict
        ):
            raise ValueError("obsolete planner journal")
        validate_result_semantics("planner", result)
        if (
            result.get("issue") != issue
            or result.get("spec_revision") != spec_revision
            or result.get("blockers")
        ):
            raise ValueError("planner journal does not match the active contract")
    except (
        OSError,
        KeyError,
        TypeError,
        ValueError,
        json.JSONDecodeError,
        ControlError,
    ):
        path.unlink(missing_ok=True)
        return None
    return result


def attest_hardware_manifest(
    item: TicketValidation,
    run_root: Path,
    manifest: Path,
    checkpoint_head: str,
    artifact_head: str,
    trusted_tool_records: dict[str, dict[str, str]] | None = None,
) -> dict[str, Any]:
    """Validate controller-owned broker evidence and bind it to the reviewed artifact."""
    path = manifest
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise ControlError(
            f"issue #{item.ticket.number}: hardware worker produced no broker manifest"
        ) from error
    lines = raw.decode("utf-8").splitlines()
    if not lines:
        raise ControlError(
            f"issue #{item.ticket.number}: hardware broker manifest is empty"
        )
    previous = ""
    successful_events = 0
    failed_events = 0
    trace_profile_boards: set[int] = set()
    final_successful_flash: dict[int, str | None] = {}
    active_board_images: dict[int, dict[str, Any]] = {}
    event_summaries: list[dict[str, Any]] = []
    allowed_operations = set(hardware_operations(item.sections))
    allowed_boards = set(hardware_boards(item.sections))
    trusted_tools = (
        trusted_hardware_tools()
        if trusted_tool_records is None
        else trusted_tool_records
    )
    expected_managed = {
        str(record.get("destination")): record
        for name, record in trusted_tools.items()
        if name.startswith("managed-component-") and isinstance(record, dict)
    }
    private_trace_hashes: set[str] = set()
    evidence_root = path.parent
    for pattern in (
        "trace-output-*/trace.json",
        "trace-output-*/trace.json.raw",
        "trace-output-*/trace.json.raw.session.json",
        "normalized-trace-*/trace.replay.json",
        "normalized-trace-*/trace.semantic.json",
    ):
        for artifact in evidence_root.glob(pattern):
            if (
                not artifact.is_file()
                or artifact.is_symlink()
                or artifact.stat().st_size > 8 * 1024 * 1024
            ):
                raise ControlError(
                    f"issue #{item.ticket.number}: private trace evidence is unsafe"
                )
            private_trace_hashes.add(hashlib.sha256(artifact.read_bytes()).hexdigest())
    for index, line in enumerate(lines, start=1):
        try:
            event = json.loads(line)
        except json.JSONDecodeError as error:
            raise ControlError(
                f"issue #{item.ticket.number}: hardware manifest line {index} is invalid"
            ) from error
        if not isinstance(event, dict):
            raise ControlError(
                f"issue #{item.ticket.number}: hardware manifest line {index} is invalid"
            )
        event_digest = event.get("event_sha256")
        payload = dict(event)
        payload.pop("event_sha256", None)
        expected_digest = hashlib.sha256(
            json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        if (
            event_digest != expected_digest
            or event.get("previous_event_sha256") != previous
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: hardware manifest hash chain is invalid"
            )
        failed = event.get("returncode") != 0 or event.get("error") is not None
        operation = event.get("operation")
        if (
            event.get("issue") != item.ticket.number
            or event.get("spec_revision") != item.sections["Specification revision"]
            or event.get("pr_head") != checkpoint_head
            or event.get("artifact_head") != artifact_head
            or (
                operation not in allowed_operations
                and not (failed and operation == "invalid")
            )
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: hardware manifest artifact binding is invalid"
            )
        if (
            operation not in {"artifact-hash", "espnow-regression", "invalid"}
            and event.get("board") not in allowed_boards
        ):
            raise ControlError(
                f"issue #{item.ticket.number}: hardware manifest board binding is invalid"
            )
        event_summary: dict[str, Any] = {
            "sequence": index,
            "operation": operation,
            "board": event.get("board"),
            "artifact_head": event.get("artifact_head"),
            "board_identity_sha256": event.get("board_identity_sha256"),
            "status": "failed" if failed else "passed",
        }
        event_summaries.append(event_summary)
        if failed:
            failed_events += 1
            if operation in {"flash", "flash-trace-acceptance", "ota"}:
                board = int(event["board"])
                active_board_images.pop(board, None)
                final_successful_flash[board] = None
            previous = str(event_digest)
            continue
        successful_events += 1
        if operation in {"flash", "flash-trace-acceptance", "ota"}:
            provenance = event.get("build_provenance")
            inputs = event.get("inputs")
            expected_profile = (
                "trace-acceptance"
                if operation == "flash-trace-acceptance"
                else "default"
            )
            if (
                not isinstance(provenance, dict)
                or provenance.get("kind") != "controller-bwrap-clean-clone-idf-build"
                or provenance.get("source_head") != artifact_head
                or provenance.get("build_profile") != expected_profile
                or provenance.get("idf_version") != "v5.4.4"
                or not FULL_SHA.fullmatch(str(provenance.get("idf_revision", "")))
                or any(
                    not re.fullmatch(r"[0-9a-f]{64}", str(provenance.get(name, "")))
                    for name in (
                        "idf_export_sha256",
                        "idf_py_sha256",
                        "bwrap_sha256",
                        "prlimit_sha256",
                        "xtensa_compiler_sha256",
                        "ulp_tool_sha256",
                        "rom_elf_sha256",
                        "idf_python_sha256",
                        "dependencies_lock_sha256",
                        "submodules_sha256",
                        "sdkconfig_defaults_sha256",
                        "sdkconfig_sha256",
                        "stdout_sha256",
                        "stderr_sha256",
                    )
                )
                or not isinstance(inputs, list)
            ):
                raise ControlError(
                    f"issue #{item.ticket.number}: trusted firmware build provenance is invalid"
                )
            if (
                provenance["idf_export_sha256"] != trusted_tools["idf-export"]["sha256"]
                or provenance["idf_py_sha256"] != trusted_tools["idf.py"]["sha256"]
                or provenance["bwrap_sha256"] != trusted_tools["bwrap"]["sha256"]
                or provenance["prlimit_sha256"] != trusted_tools["prlimit"]["sha256"]
                or provenance["xtensa_compiler_sha256"]
                != trusted_tools["xtensa-esp32s3-elf-gcc"]["sha256"]
                or provenance["ulp_tool_sha256"]
                != trusted_tools["esp32ulp-elf-as"]["sha256"]
                or provenance["rom_elf_sha256"]
                != trusted_tools["esp-rom-elf"]["sha256"]
                or provenance["idf_python_sha256"]
                != trusted_tools["idf-python"]["sha256"]
                or provenance["dependencies_lock_sha256"]
                != trusted_tools["dependencies.lock"]["sha256"]
                or not isinstance(provenance.get("managed_components"), list)
            ):
                raise ControlError(
                    f"issue #{item.ticket.number}: firmware tool authority changed"
                )
            actual_managed = {
                str(record.get("destination")): record
                for record in provenance["managed_components"]
                if isinstance(record, dict)
            }
            if (
                set(actual_managed) != set(expected_managed)
                or any(
                    actual_managed[destination].get(key)
                    != expected_managed[destination].get(key)
                    for destination in expected_managed
                    for key in ("destination", "version", "component_hash")
                )
                or any(
                    actual_managed[destination].get("source_tree_sha256")
                    != expected_managed[destination].get("sha256")
                    for destination in expected_managed
                )
                or any(
                    not re.fullmatch(
                        r"[0-9a-f]{64}",
                        str(record.get("staged_tree_sha256", "")),
                    )
                    for record in actual_managed.values()
                )
            ):
                raise ControlError(
                    f"issue #{item.ticket.number}: managed component provenance is invalid"
                )
            expected_inputs = (
                {
                    ("0x0", "bootloader/bootloader.bin"),
                    ("0x8000", "partition_table/partition-table.bin"),
                    ("0xf000", "ota_data_initial.bin"),
                    ("0x20000", "domes.bin"),
                }
                if operation in {"flash", "flash-trace-acceptance"}
                else {(None, "domes.bin")}
            )
            actual_inputs = {
                (record.get("offset"), record.get("artifact"))
                for record in inputs
                if isinstance(record, dict)
                and re.fullmatch(r"[0-9a-f]{64}", str(record.get("sha256", "")))
            }
            if actual_inputs != expected_inputs or len(inputs) != len(expected_inputs):
                raise ControlError(
                    f"issue #{item.ticket.number}: trusted firmware input hashes are invalid"
                )
            image_sha256 = next(
                str(record["sha256"])
                for record in inputs
                if isinstance(record, dict) and record.get("artifact") == "domes.bin"
            )
            board = int(event["board"])
            active_board_images[board] = {
                "artifact_head": artifact_head,
                "build_profile": expected_profile,
                "domes_bin_sha256": image_sha256,
                "build_provenance": provenance,
            }
            if operation in {"flash", "flash-trace-acceptance"}:
                final_successful_flash[board] = str(operation)
                if operation == "flash-trace-acceptance":
                    trace_profile_boards.add(board)
        elif operation == "trace-dump":
            board = int(event["board"])
            active = active_board_images.get(board)
            selected = event.get("selected_flash")
            provenance = event.get("build_provenance")
            inputs = event.get("inputs")
            trace_hashes = event.get("trace_hashes")
            candidate_cli = event.get("candidate_cli_provenance")
            trace_relay = event.get("trace_relay")
            normalization = event.get("normalization")
            trace_identity = event.get("trace_identity")
            input_hashes = (
                {
                    record.get("artifact"): record.get("sha256")
                    for record in inputs
                    if isinstance(record, dict)
                    and re.fullmatch(r"[0-9a-f]{64}", str(record.get("sha256", "")))
                }
                if isinstance(inputs, list)
                else {}
            )
            expected_selected = (
                {
                    "artifact_head": active["artifact_head"],
                    "build_profile": active["build_profile"],
                    "domes_bin_sha256": active["domes_bin_sha256"],
                }
                if active is not None
                else None
            )
            valid_trace_hashes = (
                isinstance(trace_hashes, dict)
                and set(trace_hashes)
                == {"trace_sha256", "raw_sha256", "session_sha256"}
                and all(
                    re.fullmatch(r"[0-9a-f]{64}", str(trace_hashes.get(name, "")))
                    for name in ("trace_sha256", "raw_sha256", "session_sha256")
                )
            )
            valid_trace_relay = (
                isinstance(trace_relay, dict)
                and set(trace_relay)
                == {
                    "kind",
                    "transcript_sha256",
                    "tx_frame_count",
                    "rx_frame_count",
                    "data_frame_count",
                    "raw_bytes",
                    "event_count",
                }
                and trace_relay.get("kind") == "broker-pty-frame-filter-v1"
                and re.fullmatch(
                    r"[0-9a-f]{64}", str(trace_relay.get("transcript_sha256", ""))
                )
                is not None
                and trace_relay.get("tx_frame_count") == 1
                and isinstance(trace_relay.get("data_frame_count"), int)
                and trace_relay["data_frame_count"] > 0
                and trace_relay.get("rx_frame_count")
                == trace_relay["data_frame_count"] + 2
                and isinstance(trace_relay.get("raw_bytes"), int)
                and trace_relay["raw_bytes"] > 0
                and trace_relay["raw_bytes"] % 16 == 0
                and trace_relay.get("event_count") == trace_relay["raw_bytes"] // 16
            )
            if (
                active is None
                or selected != expected_selected
                or provenance != active["build_provenance"]
                or not isinstance(inputs, list)
                or len(inputs) != 2
                or set(input_hashes) != {"domes.bin", "trace_names.json"}
                or input_hashes.get("domes.bin") != active["domes_bin_sha256"]
                or not valid_trace_hashes
                or not valid_trace_relay
                or event.get("artifact_sha256") != trace_hashes["trace_sha256"]
                or event.get("artifact_id")
                != f"trace-{trace_hashes['trace_sha256'][:16]}"
                or not isinstance(candidate_cli, dict)
                or candidate_cli.get("source_head") != artifact_head
                or any(
                    not re.fullmatch(r"[0-9a-f]{64}", str(candidate_cli.get(name, "")))
                    for name in (
                        "cargo_lock_sha256",
                        "candidate_cli_sha256",
                        "bwrap_sha256",
                        "cargo_sha256",
                        "cc_sha256",
                        "prlimit_sha256",
                        "pty_compat_source_sha256",
                        "pty_compat_binary_sha256",
                        "rustc_sha256",
                    )
                )
                or not isinstance(candidate_cli.get("cargo_version"), str)
                or not candidate_cli["cargo_version"]
                or not isinstance(candidate_cli.get("rustc_version"), str)
                or not candidate_cli["rustc_version"]
                or not isinstance(candidate_cli.get("cc_version"), str)
                or not candidate_cli["cc_version"]
                or candidate_cli.get("prlimit_sha256")
                != trusted_tools["prlimit"]["sha256"]
                or candidate_cli.get("bwrap_sha256") != trusted_tools["bwrap"]["sha256"]
                or candidate_cli.get("cargo_sha256") != trusted_tools["cargo"]["sha256"]
                or candidate_cli.get("cc_sha256") != trusted_tools["cc"]["sha256"]
                or candidate_cli.get("rustc_sha256") != trusted_tools["rustc"]["sha256"]
                or not isinstance(normalization, dict)
                or normalization.get("kind")
                not in {
                    "controller-bwrap-trace-normalizer-v1",
                    "controller-bwrap-runtime-trace-normalizer-v1",
                }
                or normalization.get("source_head") != artifact_head
                or normalization.get("build_profile") != active["build_profile"]
                or normalization.get("raw_sha256") != trace_hashes["raw_sha256"]
                or normalization.get("session_sha256") != trace_hashes["session_sha256"]
                or any(
                    not re.fullmatch(r"[0-9a-f]{64}", str(normalization.get(name, "")))
                    for name in (
                        "normalizer_sha256",
                        "trace_proto_sha256",
                        "python_sha256",
                        "bwrap_sha256",
                        "prlimit_sha256",
                        "replay_sha256",
                        "semantic_sha256",
                    )
                )
                or normalization.get("python_sha256")
                != trusted_tools["python3"]["sha256"]
                or normalization.get("bwrap_sha256") != trusted_tools["bwrap"]["sha256"]
                or normalization.get("prlimit_sha256")
                != trusted_tools["prlimit"]["sha256"]
                or normalization.get("replay_sha256") not in private_trace_hashes
                or normalization.get("semantic_sha256") not in private_trace_hashes
                or trace_hashes["trace_sha256"] not in private_trace_hashes
                or trace_hashes["raw_sha256"] not in private_trace_hashes
                or trace_hashes["session_sha256"] not in private_trace_hashes
                or not isinstance(normalization.get("summary"), dict)
                or (
                    active["build_profile"] == "default"
                    and (
                        normalization["kind"]
                        != "controller-bwrap-runtime-trace-normalizer-v1"
                        or normalization["summary"].get("tx_complete_count", 0) < 1
                        or normalization["summary"].get("rx_complete_count", 0) < 1
                    )
                )
                or (
                    active["build_profile"] == "trace-acceptance"
                    and normalization["kind"] != "controller-bwrap-trace-normalizer-v1"
                )
                or not isinstance(trace_identity, dict)
                or trace_identity.get("registered_device_match") is not True
                or trace_identity.get("candidate_file_sha256")
                != active["domes_bin_sha256"]
                or not isinstance(trace_identity.get("firmware_version"), str)
                or not trace_identity["firmware_version"]
                or not re.fullmatch(
                    r"[0-9a-f]{64}",
                    str(trace_identity.get("app_image_sha256", "")),
                )
                or not re.fullmatch(
                    r"[0-9a-f]{64}",
                    str(trace_identity.get("app_elf_sha256", "")),
                )
                or not re.fullmatch(
                    r"[0-9a-f]{64}",
                    str(trace_identity.get("device_identity_run_sha256", "")),
                )
            ):
                raise ControlError(
                    f"issue #{item.ticket.number}: trace dump artifact binding is invalid"
                )
            event_summary["trace"] = {
                "trace_sha256": trace_hashes["trace_sha256"],
                "raw_sha256": trace_hashes["raw_sha256"],
                "session_sha256": trace_hashes["session_sha256"],
                "firmware_version": trace_identity.get("firmware_version"),
                "app_elf_sha256": trace_identity.get("app_elf_sha256"),
                "app_image_sha256": trace_identity.get("app_image_sha256"),
                "candidate_file_sha256": trace_identity.get("candidate_file_sha256"),
                "registered_device_match": True,
                "normalization": normalization["summary"],
                "replay_sha256": normalization["replay_sha256"],
                "semantic_sha256": normalization["semantic_sha256"],
            }
        elif operation == "espnow-regression":
            regression = event.get("espnow_regression")
            selected = (
                {
                    str(board): {
                        "artifact_head": active_board_images[board]["artifact_head"],
                        "build_profile": active_board_images[board]["build_profile"],
                        "domes_bin_sha256": active_board_images[board][
                            "domes_bin_sha256"
                        ],
                    }
                    for board in (0, 1)
                }
                if set(active_board_images) >= {0, 1}
                else None
            )
            valid = (
                isinstance(regression, dict)
                and regression.get("kind")
                == "controller-two-board-espnow-regression-v1"
                and regression.get("artifact_head") == artifact_head
                and regression.get("boards") == [0, 1]
                and regression.get("selected_flashes") == selected
                and regression.get("benchmark_sessions") == 3
                and regression.get("benchmarks") == 6
                and regression.get("rounds_per_benchmark") == 100
                and regression.get("benchmark_failures") == 0
                and regression.get("discovery_cancellation") == "passed"
                and regression.get("drill") == "passed"
                and regression.get("final_states") == ["disabled", "disabled"]
                and regression.get("final_peer_counts") == [1, 1]
                and regression.get("final_tx_failures") == [0, 0]
                and re.fullmatch(
                    r"[0-9a-f]{64}",
                    str(regression.get("transcript_sha256", "")),
                )
                is not None
                and event.get("artifact_sha256") == regression.get("transcript_sha256")
                and event.get("artifact_id")
                == f"espnow-regression-{str(regression.get('transcript_sha256'))[:16]}"
            )
            if not valid:
                raise ControlError(
                    f"issue #{item.ticket.number}: ESP-NOW regression artifact binding is invalid"
                )
            event_summary["espnow_regression"] = regression
        previous = str(event_digest)
    unrestored = sorted(
        board
        for board in trace_profile_boards
        if final_successful_flash.get(board) != "flash"
    )
    if unrestored:
        raise ControlError(
            f"issue #{item.ticket.number}: trace-acceptance profile was not restored "
            f"to the default image on board alias(es) {unrestored}"
        )
    attestation = {
        "schema_version": 1,
        "kind": "controller-hardware-attestation",
        "issue": item.ticket.number,
        "spec_revision": item.sections["Specification revision"],
        "checkpoint_head": checkpoint_head,
        "artifact_head": artifact_head,
        "manifest": str(path),
        "manifest_sha256": hashlib.sha256(raw).hexdigest(),
        "event_count": len(lines),
        "successful_event_count": successful_events,
        "failed_event_count": failed_events,
        "last_event_sha256": previous,
        "events": event_summaries,
    }
    write_handoff(run_root / "hardware-attestation.json", attestation)
    return attestation


def controller_authored_comment(workflow: Workflow, comment: Any) -> bool:
    """Accept durable handoff comments only from the pinned controller principal."""
    if not workflow.tracker_actor or not isinstance(comment, dict):
        return False
    author = comment.get("author")
    return (
        isinstance(author, dict)
        and str(author.get("login", "")).casefold() == workflow.tracker_actor.casefold()
    )


def load_controller_interventions(
    workflow: Workflow, ticket: Ticket
) -> tuple[str, ...]:
    """Load bounded durable rework evidence from the pinned tracker principal."""
    expected_url_prefix = f"https://github.com/{workflow.repository}/issues/"
    if not ticket.url.startswith(expected_url_prefix):
        return ()
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
    selected: list[str] = []
    total_bytes = 0
    for comment in reversed(document.get("comments", [])):
        if not controller_authored_comment(workflow, comment):
            continue
        body = str(comment.get("body", "")).strip()
        if not body.startswith("Agent control-plane intervention"):
            continue
        encoded_size = len(body.encode("utf-8"))
        if encoded_size > 8_000 or total_bytes + encoded_size > 16_000:
            continue
        selected.append(body)
        total_bytes += encoded_size
        if len(selected) == 3:
            break
    return tuple(reversed(selected))


def load_exact_role_handoff(
    workflow: Workflow,
    ticket: Ticket,
    role: str,
    run_root: Path | None = None,
    *,
    allow_deferred_hardware: bool = False,
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
            if not controller_authored_comment(workflow, comment):
                continue
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
    validate_result_semantics(
        role, result, allow_deferred_hardware=allow_deferred_hardware
    )
    return result


def load_latest_worker_handoff(workflow: Workflow, ticket: Ticket) -> dict[str, Any]:
    """Prefer the newest published worker handoff before falling back to local state."""
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
    marker = "Agent control-plane transition (worker)"
    for comment in reversed(document.get("comments", [])):
        if not controller_authored_comment(workflow, comment):
            continue
        body = str(comment.get("body", ""))
        if marker not in body:
            continue
        match = re.search(r"```json\s*\n(.*?)\n```", body, re.DOTALL)
        if match is None:
            continue
        try:
            result = json.loads(match.group(1))
            validate_result_semantics("worker", result)
        except (json.JSONDecodeError, ControlError):
            continue
        if result.get("issue") != ticket.number:
            continue
        if result.get("spec_revision") == parse_sections(ticket.body).get(
            "Specification revision"
        ):
            return result
    return load_exact_role_handoff(workflow, ticket, "worker")


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
        controller_authored_comment(workflow, comment)
        and marker in str(comment.get("body", ""))
        for comment in document.get("comments", [])
    )


def count_current_ci_repair_dispatches(workflow: Workflow, ticket: Ticket) -> int:
    """Count only CI-failure repairs since the latest implementation artifact."""
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
    attempts = 0
    for comment in reversed(document.get("comments", [])):
        if not controller_authored_comment(workflow, comment):
            continue
        body = str(comment.get("body", ""))
        if "Agent control-plane transition (worker)" in body:
            break
        if (
            "Agent control-plane transition (ci)" in body
            and "Required checks failed; dispatching bounded verification repair."
            in body
        ):
            attempts += 1
    return attempts


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
        if not controller_authored_comment(workflow, comment):
            continue
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
            if not controller_authored_comment(workflow, comment):
                continue
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
    validate_result_semantics(
        matched_role,
        result,
        allow_deferred_hardware=(
            matched_role == "judge"
            and role in {"worker", "verification-worker"}
            and requires_registered_hardware(parse_sections(ticket.body))
        ),
    )
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


def sanitize_public_handoff(value: Any, workspace: Path, run_root: Path) -> Any:
    """Remove host-local paths and stable device identifiers from tracker state."""
    if isinstance(value, dict):
        return {
            str(key): sanitize_public_handoff(item, workspace, run_root)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [sanitize_public_handoff(item, workspace, run_root) for item in value]
    if not isinstance(value, str):
        return value
    rendered = value.replace(str(workspace), "$WORKSPACE").replace(
        str(run_root), "controller-private-evidence"
    )
    rendered = re.sub(
        r"/dev/(?:tty(?:USB|ACM)[0-9]+|serial/[A-Za-z0-9_./:-]+)",
        "<redacted-device-path>",
        rendered,
    )
    rendered = re.sub(
        r"/home/[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+",
        "<redacted-local-path>",
        rendered,
    )
    rendered = re.sub(
        r"(?<![0-9a-f])[0-9a-f]{12}(?![0-9a-f])",
        "<redacted-device-id>",
        rendered,
    )
    return rendered


def execute_one(
    workflow: Workflow,
    item: TicketValidation,
    *,
    autopilot: bool = False,
    hardware_capability: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Run one role with an outer teardown boundary for hardware capabilities."""
    try:
        return _execute_one(
            workflow, item, autopilot=autopilot, hardware_capability=hardware_capability
        )
    finally:
        _cleanup_registered_hardware_runtime(item.ticket.number)


def _execute_one(
    workflow: Workflow,
    item: TicketValidation,
    *,
    autopilot: bool = False,
    hardware_capability: dict[str, Any] | None = None,
) -> dict[str, Any]:
    role = role_for(item.ticket)
    hardware_required = requires_registered_hardware(item.sections)
    hardware_worker = role == "verification-worker"
    if hardware_required and hardware_worker and hardware_capability is None:
        raise ControlError(
            f"issue #{item.ticket.number}: registered hardware requires "
            "--allow-registered-hardware preflight"
        )
    hardware_access = (
        hardware_required
        and hardware_worker
        and exact_head_ci_passed(workflow, item.sections)
    )
    stack = (
        stack_context(workflow, item, load_live_tickets(workflow))
        if role in {"worker", "verification-worker", "judge"}
        and automated_delivery(item.sections)
        else None
    )
    pins_implementation_base = role in {
        "worker",
        "verification-worker",
    } and automated_delivery(item.sections)
    required_base_head = (
        (stack.base_head if stack is not None else origin_main_revision(workflow))
        if pins_implementation_base
        else None
    )
    required_base_ref = (
        (stack.base_ref if stack is not None else workflow.base_branch)
        if pins_implementation_base
        else None
    )
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / f"issue-{item.ticket.number}"
    run_root.mkdir(parents=True, exist_ok=True)
    lease_path = run_root / "active-process.json"
    terminate_recorded_process_group(lease_path)
    workspace = ensure_workspace(workflow, item, role, stack)
    prior_handoff = required_prior_handoff(
        workflow,
        item.ticket,
        run_root,
        role,
        item.source_state or item.ticket.agent_state,
    )
    tracker_context = (
        build_planner_tracker_context(
            workflow,
            load_live_tickets(workflow),
            load_open_pull_request_snapshot(workflow),
        )
        if role == "planner"
        else None
    )
    controller_interventions = (
        load_controller_interventions(workflow, item.ticket)
        if role in {"worker", "verification-worker", "judge"}
        else ()
    )
    if (
        prior_handoff is not None
        and prior_handoff.get("spec_revision")
        != item.sections["Specification revision"]
    ):
        raise ControlError(
            f"issue #{item.ticket.number}: prior handoff specification mismatch"
        )
    broker_capability: dict[str, Any] | None = None
    hardware_lease: DeviceLease | None = None
    broker_process: subprocess.Popen[str] | None = None
    hardware_evidence: Path | None = None
    hardware_checkpoint_head = item.sections["Specification revision"]
    if hardware_access:
        # Load and validate the judge handoff before taking the global lease,
        # creating a capability, or starting either the broker or an agent.
        # The broker independently rejects a later remote-head change.
        pull_request_number = existing_pull_request(item.sections)
        if pull_request_number < 1:
            raise ControlError(
                f"issue #{item.ticket.number}: hardware verification requires one "
                "ticket-bound pull request"
            )
        pull_request = load_pull_request(workflow, pull_request_number)
        validate_hardware_judge_checkpoint(workflow, item, prior_handoff, pull_request)
        # The controller owns the only lease.  Codex receives this directory, never
        # a /dev path; the host broker rechecks identity before every operation.
        hardware_checkpoint_head = pull_request.head_oid
        pr_head = hardware_checkpoint_head
        base_head = required_base_head or origin_main_revision(workflow)
        head_ref = pull_request.head_ref
        evidence = run_root / "hardware-evidence" / f"run-{time.time_ns()}"
        hardware_evidence = evidence
        tools = trusted_hardware_tools()
        capability_root = workspace / ".artifacts" / "agent-control"
        capability_root.mkdir(parents=True, exist_ok=True)
        capability_directory = Path(
            tempfile.mkdtemp(prefix=f"hw-{item.ticket.number}-", dir=capability_root)
        )
        os.chmod(capability_directory, 0o700)
        hardware_lease = DeviceLease(run_root.parent / "registered-hardware.lock")
        try:
            hardware_lease.__enter__()
        except BrokerError as error:
            persist_hardware_block(item.ticket, item.sections, pr_head, "lease-held")
            transition(workflow, item.ticket, "agent:blocked")
            return {
                "issue": item.ticket.number,
                "role": role,
                "state": "agent:blocked",
                "hardware_block": "lease-held",
            }
        with _HARDWARE_RUNTIME_LOCK:
            _HARDWARE_RUNTIMES[item.ticket.number] = {
                "process": None,
                "lease": hardware_lease,
                "public": capability_directory,
                "private": None,
                "evidence": evidence,
                "operations": tuple(hardware_operations(item.sections)),
                "boards": tuple(hardware_boards(item.sections)),
                "broker_ready": False,
            }
        # Snapshotting happens only while the global lease is held.  If this or any
        # following setup step fails, execute_one's outer finally removes both dirs.
        cap = create_capability(
            capability_directory,
            issue=item.ticket.number,
            spec_revision=item.sections["Specification revision"],
            pr_head=pr_head,
            workspace=workspace,
            evidence=evidence,
            ports=list(hardware_capability["ports"]),
            operations=list(hardware_operations(item.sections)),
            boards=list(hardware_boards(item.sections)),
            base_head=base_head,
            allowed_surfaces=list(
                allowed_surfaces(item.sections["Allowed architectural surfaces"])
            ),
            repository_url=trusted_repository_url(workflow),
            head_ref=head_ref,
            trusted_tools=tools,
        )
        broker_capability = {
            "client": str(capability_directory / "hardware_client.py"),
            "capability_directory": str(capability_directory),
            "operations": list(hardware_operations(item.sections)),
            "boards": list(hardware_boards(item.sections)),
        }
        broker_process = subprocess.Popen(
            [
                sys.executable,
                str(ROOT / "tools/agent_control/hardware_broker.py"),
                "--serve",
                str(capability_directory),
            ],
            cwd=ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        assert broker_process.stdin is not None
        broker_process.stdin.write(json.dumps(cap.private_document()))
        broker_process.stdin.close()
        with _HARDWARE_RUNTIME_LOCK:
            _HARDWARE_RUNTIMES[item.ticket.number]["process"] = broker_process
        # Do not launch a sandboxed worker until the host broker has published its
        # readiness record.  This avoids lost first requests and makes failures
        # deterministic rather than silently falling back to direct device access.
        ready = capability_directory / "ready.json"
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not ready.is_file():
            if broker_process.poll() is not None:
                raise ControlError("hardware broker exited before readiness")
            time.sleep(0.05)
        if not ready.is_file():
            raise ControlError("hardware broker readiness timed out")
        with _HARDWARE_RUNTIME_LOCK:
            _HARDWARE_RUNTIMES[item.ticket.number]["broker_ready"] = True
    schema = ORCHESTRATION_DIR / "schemas" / SCHEMA_BY_ROLE[role]
    pending_plan_path = run_root / "pending-plan.json"
    pending_plan = (
        load_pending_plan(
            pending_plan_path,
            issue=item.ticket.number,
            spec_revision=item.sections["Specification revision"],
        )
        if (
            role == "planner"
            and autopilot
            and automated_delivery(item.sections)
            and pending_plan_path.is_file()
        )
        else None
    )
    if pending_plan is not None:
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
    controller_evidence: dict[str, Any] | None = None
    attestation_path = run_root / "hardware-attestation.json"
    if role == "judge" and prior_handoff is not None and attestation_path.is_file():
        try:
            recorded_attestation = json.loads(
                attestation_path.read_text(encoding="utf-8")
            )
            if recorded_attestation.get("artifact_head") != prior_handoff.get("commit"):
                recorded_attestation = {}
            if not recorded_attestation:
                raise FileNotFoundError("no attestation for current artifact")
            recorded_manifest = Path(recorded_attestation["manifest"]).resolve(
                strict=True
            )
            recorded_manifest.relative_to(
                (run_root / "hardware-evidence").resolve(strict=True)
            )
            validated_attestation = attest_hardware_manifest(
                item,
                run_root,
                recorded_manifest,
                str(recorded_attestation["checkpoint_head"]),
                str(prior_handoff["commit"]),
            )
            controller_evidence = {
                key: value
                for key, value in validated_attestation.items()
                if key != "manifest"
            }
        except FileNotFoundError:
            controller_evidence = None
        except (
            KeyError,
            OSError,
            TypeError,
            ValueError,
            json.JSONDecodeError,
        ) as error:
            raise ControlError(
                f"issue #{item.ticket.number}: controller hardware attestation is invalid"
            ) from error
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
    ]
    if role in {"worker", "verification-worker"}:
        # Codex protects Git metadata even when it is below the workspace root.
        # This is safe only because ensure_workspace rejects linked worktrees,
        # alternates, and every git-dir outside this controller-owned clone.
        command.extend(("--add-dir", str(workspace / ".git")))
    command.extend(
        (
            "--output-schema",
            str(schema),
            "--output-last-message",
            str(result_path),
            "--json",
            "-",
        )
    )
    failures: list[str] = []
    for attempt in range(1, 4):
        returncode, failure = run_codex_attempt(
            command,
            build_prompt(
                item,
                role,
                prior_handoff,
                broker_capability if hardware_access else None,
                controller_evidence,
                required_base_head,
                required_base_ref,
                tracker_context,
                controller_interventions,
            ),
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
    if hardware_access:
        assert hardware_evidence is not None
        attested_artifact_head = hardware_attestation_artifact_head(
            item,
            result,
            hardware_checkpoint_head,
        )
        attestation = attest_hardware_manifest(
            item,
            run_root,
            hardware_evidence / "broker-manifest.jsonl",
            hardware_checkpoint_head,
            attested_artifact_head,
        )
        validate_hardware_verification_result(item, result, attestation)
    validate_result_semantics(
        role,
        result,
        allow_deferred_hardware=(
            role == "judge" and hardware_required and controller_evidence is None
        ),
    )
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
    if role in {"worker", "verification-worker"} and automated_delivery(item.sections):
        verify_worker_artifact(
            workflow,
            workspace,
            item,
            result,
            required_base_head=required_base_head,
            stack=stack,
        )
        if role == "worker" and not existing_pull_request(item.sections):
            bind_ticket_pull_request(workflow, item, int(result["pull_request"]))
    if stack is not None and role in {"worker", "verification-worker"}:
        result["controller_stack"] = {
            "parent_issue": stack.parent_issue,
            "parent_pr": stack.parent_pr,
            "base_ref": stack.base_ref,
            "base_head": stack.base_head,
        }
    result = sanitize_public_handoff(result, workspace, run_root)
    if (
        role == "planner"
        and autopilot
        and automated_delivery(item.sections)
        and not result["blockers"]
    ):
        persist_pending_plan(pending_plan_path, result)
    write_handoff(run_root / f"handoff-{role}.json", result)
    post_result(workflow, item.ticket, role, result)
    next_state = result_state(
        role,
        result,
        autopilot=autopilot,
        ticket_sections=item.sections,
        hardware_attested=controller_evidence is not None,
    )
    materialized: list[int] = []
    if role == "planner" and autopilot and automated_delivery(item.sections):
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


def _restore_default_hardware_profile(runtime: dict[str, Any]) -> None:
    """Best-effort safety restoration before a ticket-bound broker is torn down."""
    operations = set(runtime.get("operations", ()))
    if "flash-trace-acceptance" not in operations:
        return
    evidence = runtime.get("evidence")
    boards = set(runtime.get("boards", ()))
    if not isinstance(evidence, Path) or not boards:
        return
    manifest = evidence / "broker-manifest.jsonl"
    needs_restore: set[int] = set()
    if not manifest.is_file():
        # A broker can die after a trace flash but before the append. With no
        # audit record, fail safe toward the authorized default image.
        if not runtime.get("broker_ready"):
            return
        needs_restore.update(boards)
    try:
        lines = (
            manifest.read_text(encoding="utf-8").splitlines()
            if manifest.is_file()
            else []
        )
        for line in lines:
            event = json.loads(line)
            if not isinstance(event, dict):
                raise ValueError("manifest event is not an object")
            operation = event.get("operation")
            board = event.get("board")
            if operation == "flash-trace-acceptance":
                if board in boards:
                    needs_restore.add(int(board))
                else:
                    needs_restore.update(boards)
            elif (
                operation == "flash"
                and board in boards
                and event.get("returncode") == 0
                and event.get("error") is None
                and isinstance(event.get("build_provenance"), dict)
                and event["build_provenance"].get("build_profile") == "default"
            ):
                needs_restore.discard(int(board))
    except (OSError, TypeError, ValueError, json.JSONDecodeError):
        # The controller-owned audit becoming unreadable after any profile-capable
        # run is itself unsafe; restore every ticket-authorized board.
        needs_restore.update(boards)
    if not needs_restore:
        return
    process = runtime.get("process")
    public = runtime.get("public")
    if (
        not isinstance(process, subprocess.Popen)
        or process.poll() is not None
        or not isinstance(public, Path)
    ):
        raise ControlError("hardware broker unavailable for default-image restoration")
    for board in sorted(needs_restore):
        result = hardware_request(
            public, {"operation": "flash", "board": board}, 1800.0
        )
        if result.get("returncode") != 0 or result.get("error") is not None:
            raise ControlError(
                f"registered board alias {board} default-image restoration failed"
            )


def _cleanup_hardware_broker(
    process: subprocess.Popen[str] | None,
    lease: DeviceLease | None,
    directory: Path | None,
    private_directory: Path | None = None,
) -> None:
    """Best-effort cleanup used on normal completion and controller error paths."""
    try:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
    finally:
        if lease is not None:
            lease.__exit__(None, None, None)
        if directory is not None:
            import shutil

            shutil.rmtree(directory, ignore_errors=True)
        if private_directory is not None:
            import shutil

            shutil.rmtree(private_directory, ignore_errors=True)


def _cleanup_registered_hardware_runtime(issue: int) -> None:
    with _HARDWARE_RUNTIME_LOCK:
        runtime = _HARDWARE_RUNTIMES.pop(issue, None)
    if runtime is not None:
        try:
            _restore_default_hardware_profile(runtime)
        finally:
            _cleanup_hardware_broker(
                runtime["process"],
                runtime["lease"],
                runtime["public"],
                runtime["private"],
            )


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
    *,
    required_base_head: str | None = None,
    stack: StackContext | None = None,
) -> None:
    commit = result.get("commit", "")
    if not FULL_SHA.fullmatch(commit):
        raise ControlError(
            f"issue #{item.ticket.number}: worker returned an invalid commit"
        )
    surfaces = allowed_surfaces(item.sections["Allowed architectural surfaces"])
    if not automated_delivery(item.sections):
        branch = f"codex/issue-{item.ticket.number}"
        remote = _git(
            "ls-remote",
            "--exit-code",
            trusted_repository_url(workflow),
            f"refs/heads/{branch}",
        )
        fields = remote.stdout.split()
        if remote.returncode or len(fields) != 2 or fields[0] != commit:
            raise ControlError(
                f"issue #{item.ticket.number}: worker artifact was not pushed"
            )
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
    expected_base = stack.base_ref if stack is not None else workflow.base_branch
    pull_request: PullRequest | None = None
    observed: list[str] = []
    for attempt in range(3):
        try:
            candidate = load_pull_request(workflow, pull_request_number)
        except TrackerError as error:
            observed.append(str(error))
        else:
            pull_request = candidate
            if (
                candidate.state == "OPEN"
                and not candidate.is_draft
                and candidate.base_ref == expected_base
                and candidate.head_oid == commit
            ):
                break
            observed.append(
                "state="
                f"{candidate.state}, draft={candidate.is_draft}, "
                f"base={candidate.base_ref}, head={candidate.head_oid}"
            )
        if attempt < 2:
            time.sleep(min(2**attempt, workflow.max_retry_backoff_seconds))
    else:
        detail = observed[-1] if observed else "no tracker response"
        raise ControlError(
            f"issue #{item.ticket.number}: pull request does not match worker "
            f"artifact after bounded tracker reconciliation ({detail})"
        )
    assert pull_request is not None
    pr_violations = paths_outside_surfaces(pull_request.files, surfaces)
    if pr_violations:
        raise ControlError(
            f"issue #{item.ticket.number}: PR changes outside allowed surfaces: "
            f"{', '.join(pr_violations)}"
        )
    if required_base_head is None or not FULL_SHA.fullmatch(required_base_head):
        raise ControlError(
            f"issue #{item.ticket.number}: autonomous worker is missing the "
            "controller-pinned base revision"
        )
    if stack is not None:
        validate_stack_binding(workflow, item, pull_request, stack)
    fetched = _git(
        "fetch",
        "--quiet",
        "--no-write-fetch-head",
        trusted_repository_url(workflow),
        commit,
    )
    if fetched.returncode != 0:
        raise ControlError(
            fetched.stderr.strip()
            or f"issue #{item.ticket.number}: cannot fetch the exact remote PR head"
        )
    ancestry = _git("merge-base", "--is-ancestor", required_base_head, commit)
    if ancestry.returncode == 1:
        raise ControlError(
            f"issue #{item.ticket.number}: worker artifact must descend from current "
            f"base revision {required_base_head}; reconcile and push the existing PR"
        )
    if ancestry.returncode != 0:
        raise ControlError(
            ancestry.stderr.strip()
            or f"issue #{item.ticket.number}: cannot verify PR base ancestry"
        )


def validate_stack_binding(
    workflow: Workflow,
    item: TicketValidation,
    pull_request: PullRequest,
    binding: StackContext,
) -> None:
    """Require a child artifact to remain on the exact reviewed parent head."""
    if (
        pull_request.base_ref != binding.base_ref
        or pull_request.base_oid != binding.base_head
    ):
        raise StackInvalidated(
            f"issue #{item.ticket.number}: stacked PR no longer binds parent "
            f"#{binding.parent_issue} at {binding.base_head}"
        )
    tickets = load_live_tickets(workflow)
    live = stack_context(workflow, item, tickets)
    if live is None or live != binding:
        raise StackInvalidated(
            f"issue #{item.ticket.number}: stack parent changed after dispatch"
        )


def artifact_stack_binding(artifact: dict[str, Any]) -> StackContext | None:
    value = artifact.get("controller_stack")
    if not isinstance(value, dict):
        return None
    try:
        binding = StackContext(
            int(value["parent_issue"]),
            int(value["parent_pr"]),
            str(value["base_ref"]),
            str(value["base_head"]),
        )
    except (KeyError, TypeError, ValueError):
        raise ControlError("artifact has malformed controller stack binding") from None
    if (
        binding.parent_issue < 1
        or binding.parent_pr < 1
        or not binding.base_ref
        or not FULL_SHA.fullmatch(binding.base_head)
    ):
        raise ControlError("artifact has malformed controller stack binding")
    return binding


def _remote_commit_is_ancestor(
    workflow: Workflow, ancestor: str, descendant: str
) -> bool:
    if not FULL_SHA.fullmatch(ancestor) or not FULL_SHA.fullmatch(descendant):
        raise ControlError("stack integration requires exact commit identities")
    fetched = _git(
        "fetch",
        "--quiet",
        "--no-write-fetch-head",
        trusted_repository_url(workflow),
        ancestor,
        descendant,
    )
    if fetched.returncode != 0:
        raise TrackerError(
            fetched.stderr.strip() or "cannot fetch stacked integration commits"
        )
    ancestry = _git("merge-base", "--is-ancestor", ancestor, descendant)
    if ancestry.returncode not in {0, 1}:
        raise ControlError(
            ancestry.stderr.strip() or "cannot verify stacked integration ancestry"
        )
    return ancestry.returncode == 0


def reconcile_stacked_human_merge(
    workflow: Workflow,
    ticket: Ticket,
    pull_request: PullRequest,
    binding: StackContext,
) -> dict[str, Any]:
    """Keep a child nonterminal until its merged stack commit reaches main."""
    if (
        pull_request.state != "MERGED"
        or pull_request.base_ref != binding.base_ref
        or not FULL_SHA.fullmatch(pull_request.merge_commit)
    ):
        raise ControlError(
            f"issue #{ticket.number}: stacked merge artifact is malformed"
        )
    parent = load_pull_request(workflow, binding.parent_pr)
    if parent.state == "OPEN":
        if _remote_commit_is_ancestor(
            workflow, pull_request.merge_commit, parent.head_oid
        ):
            return {
                "issue": ticket.number,
                "state": "agent:human-review",
                "stack_merge": "waiting_for_parent_main",
                "parent_pull_request": parent.number,
            }
        return block_invalid_human_merged_stack(
            workflow,
            ticket,
            "The open parent no longer contains the human-merged child integration; "
            "a new steward-approved delivery is required.",
        )
    if parent.state != "MERGED":
        return block_invalid_human_merged_stack(
            workflow,
            ticket,
            "The stack parent closed without reaching main; the child must be "
            "replanned into a new accepted delivery.",
        )
    refresh_base_branch(workflow)
    main_head = origin_main_revision(workflow)
    if _remote_commit_is_ancestor(workflow, pull_request.merge_commit, main_head):
        return finalize_human_merged_ticket(workflow, ticket, pull_request)

    # A nested parent may have merged into the next review branch without yet
    # reaching main. Follow its controller-validated dependency chain and keep
    # the child pending only while that exact integration remains in the live
    # ancestor. Dropped integrations still fail closed.
    tickets = load_live_tickets(workflow)
    parent_ticket = next(
        (
            candidate
            for candidate in tickets
            if candidate.number == binding.parent_issue
        ),
        None,
    )
    if parent_ticket is not None:
        parent_item = validate_ticket(parent_ticket, check_revision=False)
        if parent_item.valid:
            try:
                live_parent = stack_context(workflow, parent_item, tickets)
            except ControlError:
                live_parent = None
            if live_parent is not None and _remote_commit_is_ancestor(
                workflow, pull_request.merge_commit, live_parent.base_head
            ):
                return {
                    "issue": ticket.number,
                    "state": "agent:human-review",
                    "stack_merge": "waiting_for_parent_main",
                    "parent_pull_request": live_parent.parent_pr,
                }
    return block_invalid_human_merged_stack(
        workflow,
        ticket,
        "The parent review chain no longer contains the child's integration commit; "
        "a new steward-approved delivery is required.",
    )


def reconcile_stacked_children(
    workflow: Workflow,
    tickets: Sequence[Ticket],
    active_numbers: Sequence[int] = (),
) -> list[dict[str, Any]]:
    """Return stale stacked children to rework without touching active workers."""
    active = set(active_numbers)
    results: list[dict[str, Any]] = []
    for ticket in tickets:
        if (
            ticket.number in active
            or ticket.state != "OPEN"
            or ticket.agent_state
            not in {
                "agent:agent-review",
                "agent:ci-pending",
                "agent:verification",
                "agent:human-review",
            }
        ):
            continue
        try:
            artifact = load_latest_artifact_handoff(workflow, ticket)
            binding = artifact_stack_binding(artifact)
            if binding is None:
                continue
            item = validate_ticket(ticket)
            if not item.valid:
                continue
            pull_request_number = artifact.get("pull_request")
            if not isinstance(pull_request_number, int):
                raise StackInvalidated("stacked artifact has no pull request")
            pull_request = load_pull_request(workflow, pull_request_number)
            if pull_request.state == "MERGED":
                if ticket.agent_state == "agent:human-review":
                    # The owning reconciler retains the child until the parent
                    # integration is observed on main.
                    continue
                results.append(
                    block_invalid_human_merged_stack(
                        workflow,
                        ticket,
                        "The stacked child was merged before reaching the human-review "
                        "boundary; a new steward-approved delivery is required.",
                    )
                )
                continue
            validate_stack_binding(workflow, item, pull_request, binding)
        except StackInvalidated as error:
            if ticket.agent_state not in {
                "agent:rework",
                "agent:blocked",
                "agent:done",
            }:
                transition(workflow, ticket, "agent:rework")
                post_controller_comment(
                    workflow,
                    ticket,
                    "Agent control-plane transition (stacked pull request)",
                    f"Parent changed, conflicted, or merged; rework is required: {error}",
                )
                results.append({"issue": ticket.number, "state": "agent:rework"})
        except TrackerError:
            continue
        except ControlError:
            # The owning state reconciler handles missing or malformed artifacts
            # without turning one ticket into a controller-wide failure.
            continue
    return results


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


def exact_head_ci_passed(workflow: Workflow, sections: dict[str, str]) -> bool:
    """Fail closed before provisioning hardware for a verification worker."""
    pull_request_number = existing_pull_request(sections)
    if pull_request_number < 1:
        return False
    pull_request = load_pull_request(workflow, pull_request_number)
    policy = load_autopilot_policy()
    state, _ = required_check_summary(pull_request, policy)
    return state == "passed"


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


def finalize_human_merged_ticket(
    workflow: Workflow,
    ticket: Ticket,
    pull_request: PullRequest,
) -> dict[str, Any]:
    try:
        refresh_base_branch(workflow)
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (human merge)",
            (
                f"Observed the human-merged PR #{pull_request.number} at head "
                f"`{pull_request.head_oid}`. Merge commit: "
                f"`{pull_request.merge_commit}`. The controller did not approve "
                "or merge this pull request."
            ),
        )
        complete_issue(workflow, ticket)
    except ControlError as error:
        raise TrackerError(
            f"issue #{ticket.number}: human-merge bookkeeping must be retried: {error}"
        ) from error
    return {
        "issue": ticket.number,
        "state": "agent:done",
        "pull_request": pull_request.number,
        "head": pull_request.head_oid,
        "merge_commit": pull_request.merge_commit,
        "merged_by": "human",
    }


def mark_review_ready(
    workflow: Workflow,
    policy: AutopilotPolicy,
    ticket: Ticket,
    pull_request: PullRequest,
    records: Sequence[dict[str, str]],
) -> dict[str, Any]:
    if policy.review_authority != "human":
        raise ControlError("autopilot policy does not require human review")
    post_controller_comment(
        workflow,
        ticket,
        "Agent control-plane transition (review ready)",
        (
            f"PR #{pull_request.number} at exact head `{pull_request.head_oid}` "
            f"passed {len(policy.required_ci_checks)} required checks and the "
            "independent agent judge. It is waiting for human review. The "
            "controller did not approve or merge it and may continue with "
            "separate unblocked work."
        ),
    )
    transition(workflow, ticket, "agent:human-review")
    return {
        "issue": ticket.number,
        "state": "agent:human-review",
        "pull_request": pull_request.number,
        "head": pull_request.head_oid,
        "checks": list(records),
        "review_authority": "human",
    }


def hardware_attestation_matches_artifact(issue: int, artifact_head: str) -> bool:
    """Return whether complete private hardware evidence matches this artifact."""
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / f"issue-{issue}"
    try:
        attestation = json.loads(
            (run_root / "hardware-attestation.json").read_text(encoding="utf-8")
        )
        verification = json.loads(
            (run_root / "handoff-verification-worker.json").read_text(encoding="utf-8")
        )
    except (OSError, json.JSONDecodeError):
        return False
    checks = verification.get("checks")
    return bool(
        attestation.get("artifact_head") == artifact_head
        and attestation.get("failed_event_count") == 0
        and verification.get("commit") == artifact_head
        and verification.get("state") == "agent_review"
        and verification.get("blockers") == []
        and isinstance(checks, list)
        and checks
        and all(
            isinstance(check, dict) and check.get("status") in {"passed", "skipped"}
            for check in checks
        )
    )


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
    artifact_head = str(artifact.get("commit", ""))
    judge = load_exact_role_handoff(
        workflow,
        ticket,
        "judge",
        allow_deferred_hardware=(
            requires_registered_hardware(validation.sections)
            and not hardware_attestation_matches_artifact(ticket.number, artifact_head)
        ),
    )
    if judge["verdict"] != "approve":
        raise ControlError(f"issue #{ticket.number}: CI state lacks judge approval")
    pull_request_number = artifact.get("pull_request")
    if not isinstance(pull_request_number, int) or pull_request_number < 1:
        raise ControlError(f"issue #{ticket.number}: CI state has no pull request")
    pull_request = load_pull_request(workflow, pull_request_number)
    binding = artifact_stack_binding(artifact)
    if pull_request.head_oid != artifact.get("commit"):
        transition(workflow, ticket, "agent:rework")
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (ci)",
            "Pull-request head changed after the reviewed artifact; returning to rework.",
        )
        return {"issue": ticket.number, "state": "agent:rework"}
    if pull_request.state == "MERGED" and binding is not None:
        return block_invalid_human_merged_stack(
            workflow,
            ticket,
            "The stacked child was merged before exact-head CI reached the human-review "
            "boundary; a new steward-approved delivery is required.",
        )
    try:
        if binding is not None:
            validate_stack_binding(workflow, validation, pull_request, binding)
    except StackInvalidated as error:
        transition(workflow, ticket, "agent:rework")
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (stacked pull request)",
            str(error),
        )
        return {"issue": ticket.number, "state": "agent:rework"}
    expected_base = binding.base_ref if binding is not None else workflow.base_branch
    if pull_request.base_ref != expected_base or pull_request.is_draft:
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
        attempts = count_current_ci_repair_dispatches(workflow, ticket)
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
    if pull_request.state == "MERGED":
        return finalize_human_merged_ticket(workflow, ticket, pull_request)
    sections = parse_sections(ticket.body)
    surfaces = allowed_surfaces(sections["Allowed architectural surfaces"])
    outside = paths_outside_surfaces(pull_request.files, surfaces)
    protected = protected_autonomous_paths(pull_request.files, policy)
    if outside or protected:
        raise ControlError(
            f"issue #{ticket.number}: autonomous PR path policy rejected: "
            + ", ".join(outside or protected)
        )
    if pull_request.merge_state == "DIRTY" or pull_request.mergeable in {
        "CONFLICTING",
        "UNMERGEABLE",
    }:
        transition(workflow, ticket, "agent:rework")
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (review readiness)",
            "The pull request is conflicting; returning it to the "
            "worker before human review.",
        )
        return {"issue": ticket.number, "state": "agent:rework"}
    if requires_registered_hardware(validation.sections) and not (
        hardware_attestation_matches_artifact(ticket.number, pull_request.head_oid)
    ):
        post_controller_comment(
            workflow,
            ticket,
            "Agent control-plane transition (hardware verification)",
            (
                f"PR #{pull_request.number} at exact head `{pull_request.head_oid}` "
                f"passed {len(policy.required_ci_checks)} required checks and the "
                "first-pass safety judge. Dispatching the separately authorized "
                "registered-hardware verification worker."
            ),
        )
        transition(workflow, ticket, "agent:verification")
        return {
            "issue": ticket.number,
            "state": "agent:verification",
            "pull_request": pull_request.number,
            "head": pull_request.head_oid,
            "checks": list(records),
        }
    return mark_review_ready(workflow, policy, ticket, pull_request, records)


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


def reconcile_human_reviews(
    workflow: Workflow,
    tickets: Sequence[Ticket],
) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    for ticket in tickets:
        if ticket.state != "OPEN" or ticket.agent_state != "agent:human-review":
            continue
        sections = parse_sections(ticket.body)
        if not automated_delivery(sections):
            continue
        try:
            validation = validate_ticket(ticket)
            if not validation.valid:
                raise ControlError(
                    f"issue #{ticket.number}: invalid human-review ticket: "
                    + "; ".join(validation.errors)
                )
            artifact = load_latest_artifact_handoff(workflow, ticket)
            pull_request_number = artifact.get("pull_request")
            if not isinstance(pull_request_number, int) or pull_request_number < 1:
                raise ControlError(
                    f"issue #{ticket.number}: human review has no pull request"
                )
            pull_request = load_pull_request(workflow, pull_request_number)
            binding = artifact_stack_binding(artifact)
            if pull_request.head_oid != artifact.get("commit"):
                transition(workflow, ticket, "agent:rework")
                post_controller_comment(
                    workflow,
                    ticket,
                    "Agent control-plane transition (human review)",
                    "The PR head changed after agent review; returning it through "
                    "worker and independent-judge validation.",
                )
                results.append({"issue": ticket.number, "state": "agent:rework"})
                continue
            if pull_request.state == "MERGED":
                if binding is not None:
                    results.append(
                        reconcile_stacked_human_merge(
                            workflow, ticket, pull_request, binding
                        )
                    )
                else:
                    results.append(
                        finalize_human_merged_ticket(workflow, ticket, pull_request)
                    )
                continue
            if pull_request.state != "OPEN":
                transition(workflow, ticket, "agent:blocked")
                post_controller_comment(
                    workflow,
                    ticket,
                    "Agent control-plane transition (human review)",
                    f"PR #{pull_request.number} closed without merge.",
                )
                results.append({"issue": ticket.number, "state": "agent:blocked"})
                continue
            if binding is not None:
                try:
                    validate_stack_binding(workflow, validation, pull_request, binding)
                except StackInvalidated as error:
                    transition(workflow, ticket, "agent:rework")
                    post_controller_comment(
                        workflow,
                        ticket,
                        "Agent control-plane transition (stacked pull request)",
                        str(error),
                    )
                    results.append({"issue": ticket.number, "state": "agent:rework"})
                    continue
            if pull_request.merge_state == "DIRTY" or (
                pull_request.mergeable in {"CONFLICTING", "UNMERGEABLE"}
            ):
                transition(workflow, ticket, "agent:rework")
                post_controller_comment(
                    workflow,
                    ticket,
                    "Agent control-plane transition (human review)",
                    "The reviewed PR is now conflicting with its base; "
                    "returning it through worker and independent-judge validation.",
                )
                results.append({"issue": ticket.number, "state": "agent:rework"})
                continue
            if pull_request.review_decision == "CHANGES_REQUESTED":
                transition(workflow, ticket, "agent:rework")
                post_controller_comment(
                    workflow,
                    ticket,
                    "Agent control-plane transition (human review)",
                    "Human review requested changes; returning the PR to the worker.",
                )
                results.append({"issue": ticket.number, "state": "agent:rework"})
                continue
            continue
        except TrackerError as error:
            results.append(
                {
                    "issue": ticket.number,
                    "state": "agent:human-review",
                    "retryable_error": str(error),
                }
            )
        except ControlError as error:
            transition(workflow, ticket, "agent:blocked")
            post_controller_comment(
                workflow,
                ticket,
                "Agent control-plane transition (human review)",
                f"Human-review reconciliation failed closed: {error}",
            )
            results.append(
                {"issue": ticket.number, "state": "agent:blocked", "error": str(error)}
            )
    return results


def output_schema_contract_errors(document: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    def visit(schema: dict[str, Any], path: str) -> None:
        for keyword in ("uniqueItems",):
            if keyword in schema:
                errors.append(
                    f"{path} uses structured-output-unsupported keyword {keyword}"
                )
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


def _single_line(value: object, limit: int = 72) -> str:
    rendered = " ".join(str(value).split())
    return rendered if len(rendered) <= limit else rendered[: limit - 1] + "…"


def _dashboard_event(payload: dict[str, Any]) -> str:
    if payload.get("controller_error"):
        return f"controller retry: {_single_line(payload['controller_error'])}"
    if payload.get("dispatch_error"):
        return f"dispatch retry: {_single_line(payload['dispatch_error'])}"
    failures = payload.get("failures") or []
    if failures:
        return f"role failure: {_single_line(failures[0])}"
    runs = payload.get("runs") or []
    if runs:
        run = runs[-1]
        return (
            f"#{run.get('issue', '?')} {run.get('role', 'role')} → "
            f"{str(run.get('state', 'unknown')).removeprefix(STATE_PREFIX)}"
        )
    ci = payload.get("ci") or []
    if ci:
        result = ci[-1]
        suffix = f" PR #{result['pull_request']}" if result.get("pull_request") else ""
        return (
            f"#{result.get('issue', '?')} → "
            f"{str(result.get('state', 'unknown')).removeprefix(STATE_PREFIX)}{suffix}"
        )
    selector = payload.get("selector")
    if isinstance(selector, dict):
        state = selector.get("state", "unknown")
        if state == "selected":
            return (
                f"selected #{selector.get('issue', '?')} "
                f"{selector.get('work_package', '')}"
            ).rstrip()
        if state == "error":
            return f"selector retry: {_single_line(selector.get('error', 'error'))}"
        return f"selector {state}"
    return "heartbeat"


def render_dashboard(
    workflow: Workflow,
    tickets: Sequence[Ticket],
    active_items: Sequence[TicketValidation],
    eligible: Sequence[TicketValidation],
    blockers: dict[int, list[str]],
    payload: dict[str, Any],
    *,
    phase: str = "running",
    selector_active: bool = False,
) -> str:
    active_agent_count = len(active_items) + int(selector_active)
    lines = [
        "DOMES AUTOPILOT — HUMAN REVIEW AND MERGE REQUIRED",
        f"Updated: {time.strftime('%Y-%m-%d %H:%M:%S %Z')}",
        (
            f"Controller: {phase.upper()} | agents {active_agent_count}/"
            f"{workflow.max_concurrent_workers} | eligible {len(eligible)} | "
            f"blocked {len(blockers)}"
        ),
        "",
        "ACTIVE AGENTS",
    ]
    if selector_active:
        lines.append("  milestone selector | reading project brain and live tracker")
    if active_items:
        for item in sorted(active_items, key=lambda value: value.ticket.number):
            pull_request = existing_pull_request(item.sections)
            pr_text = f" | PR #{pull_request}" if pull_request else ""
            lines.append(
                f"  #{item.ticket.number} {role_for(item.ticket)}{pr_text} | "
                f"{_single_line(item.ticket.title)}"
            )
    elif not selector_active:
        lines.append("  none")

    reviews = sorted(
        (
            ticket
            for ticket in tickets
            if ticket.state == "OPEN" and ticket.agent_state == "agent:human-review"
        ),
        key=lambda value: value.number,
    )
    lines.extend(("", "WAITING FOR YOUR REVIEW"))
    if reviews:
        for ticket in reviews:
            sections = parse_sections(ticket.body)
            pull_request = existing_pull_request(sections)
            if pull_request:
                pr_text = f"PR #{pull_request}"
            elif automated_delivery(sections):
                pr_text = "PR pending"
            else:
                pr_text = "manual review (no PR)"
            lines.append(
                f"  {pr_text} | issue #{ticket.number} | {_single_line(ticket.title)}"
            )
    else:
        lines.append("  none")

    lines.extend(("", "QUEUE"))
    if eligible:
        active_by_number = {item.ticket.number: item for item in active_items}
        active_reservations = [
            (
                item.ticket.number,
                allowed_surfaces(item.sections["Allowed architectural surfaces"]),
            )
            for item in active_items
            if role_for(item.ticket) in SURFACE_RESERVING_ROLES
        ]
        for item in eligible[:5]:
            if item.ticket.number in active_by_number:
                queue_state = "running"
            else:
                conflict = next(
                    (
                        issue
                        for issue, surfaces in active_reservations
                        if role_for(item.ticket) in SURFACE_RESERVING_ROLES
                        and surfaces_overlap(
                            allowed_surfaces(
                                item.sections["Allowed architectural surfaces"]
                            ),
                            surfaces,
                        )
                    ),
                    None,
                )
                queue_state = (
                    f"surface-wait (active #{conflict})" if conflict else "ready"
                )
            lines.append(
                f"  {queue_state} #{item.ticket.number} {role_for(item.ticket)} | "
                f"{_single_line(item.ticket.title)}"
            )
    else:
        lines.append("  no dispatchable tickets")
    if blockers:
        for issue, reasons in sorted(blockers.items())[:5]:
            lines.append(f"  blocked #{issue} | {_single_line('; '.join(reasons))}")

    lines.extend(
        (
            "",
            f"LAST EVENT  {_dashboard_event(payload)}",
            f"Next refresh in at most {workflow.poll_interval_seconds}s",
            "Raw role transcripts are intentionally not displayed.",
        )
    )
    return "\n".join(lines)


def emit_watch_status(
    *,
    dashboard: bool,
    workflow: Workflow,
    tickets: Sequence[Ticket],
    active_items: Sequence[TicketValidation],
    eligible: Sequence[TicketValidation],
    blockers: dict[int, list[str]],
    payload: dict[str, Any],
    phase: str = "running",
    selector_active: bool = False,
) -> None:
    if dashboard:
        print(
            "\033[2J\033[H"
            + render_dashboard(
                workflow,
                tickets,
                active_items,
                eligible,
                blockers,
                payload,
                phase=phase,
                selector_active=selector_active,
            ),
            flush=True,
        )
        return
    print(json.dumps(payload, indent=2), flush=True)


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
        help="select bounded work, repair CI, and prepare PRs for human review",
    )
    run.add_argument(
        "--dashboard",
        action="store_true",
        help="show a live human-readable watch view instead of JSON snapshots",
    )
    run.add_argument(
        "--allow-registered-hardware",
        action="store_true",
        help="allow explicitly hardware-required worker tickets after doctor preflight",
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
        if args.dashboard and not args.watch:
            raise ControlError("--dashboard requires --watch")
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
            selector_future: concurrent.futures.Future[dict[str, Any]] | None = None
            next_selector_at = 0.0
            blocked_selector_snapshot = ""
            last_selector_snapshot = ""
            last_tickets: Sequence[Ticket] = ()
            last_eligible: Sequence[TicketValidation] = ()
            last_blockers: dict[int, list[str]] = {}
            with concurrent.futures.ThreadPoolExecutor(max_workers=maximum) as executor:
                while True:
                    selector_result: dict[str, Any] | None = None
                    if selector_future is not None and selector_future.done():
                        try:
                            selector_result = selector_future.result()
                        except Exception as error:
                            selector_result = {
                                "state": "error",
                                "error": str(error),
                            }
                        selector_future = None
                        if selector_result.get("state") == "selected":
                            next_selector_at = 0.0
                            blocked_selector_snapshot = ""
                        else:
                            cooldown = selector_retry_cooldown(selector_result)
                            next_selector_at = time.monotonic() + cooldown
                            blocked_selector_snapshot = selector_retry_snapshot(
                                selector_result, last_selector_snapshot
                            )
                    try:
                        if policy is not None:
                            (
                                _,
                                tickets,
                                pull_request_snapshot,
                                current_selector_snapshot,
                            ) = load_selector_snapshot(workflow)
                        else:
                            tickets = load_live_tickets(workflow)
                            pull_request_snapshot = []
                            current_selector_snapshot = ""
                    except (ControlError, OSError, json.JSONDecodeError) as error:
                        if not args.watch:
                            raise
                        payload = {
                            "controller_error": str(error),
                            "retry_in_seconds": workflow.poll_interval_seconds,
                        }
                        emit_watch_status(
                            dashboard=args.dashboard,
                            workflow=workflow,
                            tickets=last_tickets,
                            active_items=tuple(active.values()),
                            eligible=last_eligible,
                            blockers=last_blockers,
                            payload=payload,
                            phase="retrying",
                            selector_active=selector_future is not None,
                        )
                        time.sleep(workflow.poll_interval_seconds)
                        continue
                    last_selector_snapshot = current_selector_snapshot
                    if policy is not None and blocked_selector_snapshot:
                        if current_selector_snapshot != blocked_selector_snapshot:
                            next_selector_at = 0.0
                            blocked_selector_snapshot = ""
                    ci_results: list[dict[str, Any]] = []
                    if policy is not None:
                        stack_results = reconcile_stacked_children(
                            workflow,
                            tickets,
                            tuple(item.ticket.number for item in active.values()),
                        )
                        if stack_results:
                            tickets = load_live_tickets(workflow)
                        ci_results.extend(stack_results)
                        ci_results.extend(reconcile_ci(workflow, policy, tickets))
                        ci_results.extend(reconcile_human_reviews(workflow, tickets))
                        if ci_results:
                            tickets = load_live_tickets(workflow)
                    hardware_capability: dict[str, Any] | None = None
                    hardware_preflight_error: str | None = None
                    hardware_tickets = [
                        ticket
                        for ticket in tickets
                        if requires_registered_hardware(parse_sections(ticket.body))
                    ]
                    if args.allow_registered_hardware and hardware_tickets:
                        try:
                            hardware_capability = registered_hardware_preflight()
                            recovered = recover_hardware_blocked_tickets(
                                workflow, tickets, hardware_capability
                            )
                            if recovered:
                                tickets = load_live_tickets(workflow)
                        except ControlError as error:
                            hardware_preflight_error = str(error)
                            # A preflight failure is a deterministic terminal state
                            # for eligible hardware work, with typed local evidence.
                            blocked_now = block_hardware_preflight_tickets(
                                workflow, tickets, hardware_preflight_error
                            )
                            if blocked_now:
                                tickets = load_live_tickets(workflow)
                    eligible, blockers = eligible_queue(tickets)
                    deferred_retries = deferred_role_retries(tickets)
                    for issue, seconds in deferred_retries.items():
                        blockers.setdefault(issue, []).append(
                            f"controller role retry scheduled in {seconds}s"
                        )
                    for item in eligible:
                        if not requires_worker_hardware_access(item):
                            continue
                        if not args.allow_registered_hardware:
                            blockers.setdefault(item.ticket.number, []).append(
                                "registered hardware requires --allow-registered-hardware"
                            )
                        elif hardware_capability is None:
                            blockers.setdefault(item.ticket.number, []).append(
                                hardware_preflight_error
                                or "registered hardware preflight did not pass"
                            )
                    last_tickets = tickets
                    last_eligible = eligible
                    last_blockers = blockers
                    active_numbers = {item.ticket.number for item in active.values()}
                    candidates = [
                        item
                        for item in eligible
                        if item.ticket.number not in active_numbers
                        and item.ticket.number not in deferred_retries
                        and (
                            not requires_worker_hardware_access(item)
                            or hardware_capability is not None
                        )
                    ]
                    # One physical fleet, one exclusive broker lease: reserve at
                    # most one hardware worker before any GitHub claim.
                    if any(
                        requires_worker_hardware_access(item)
                        for item in active.values()
                    ):
                        candidates = [
                            item
                            for item in candidates
                            if not requires_worker_hardware_access(item)
                        ]
                    else:
                        seen_hardware = False
                        filtered: list[TicketValidation] = []
                        for item in candidates:
                            if requires_worker_hardware_access(item):
                                if seen_hardware:
                                    continue
                                seen_hardware = True
                            filtered.append(item)
                        candidates = filtered
                    reserved = reserved_mutation_surfaces(tuple(active.values()))
                    selected = select_non_overlapping(
                        candidates,
                        available_role_slots(
                            maximum, len(active), selector_future is not None
                        ),
                        reserved,
                    )
                    dispatch_errors: list[str] = []
                    for item in selected:
                        try:
                            claimed = claim_for_dispatch(workflow, item)
                        except ControlError as error:
                            dispatch_errors.append(
                                f"issue #{item.ticket.number}: {error}"
                            )
                            continue
                        future = executor.submit(
                            execute_one,
                            workflow,
                            claimed,
                            autopilot=args.autopilot,
                            hardware_capability=(
                                hardware_capability
                                if requires_worker_hardware_access(claimed)
                                else None
                            ),
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

                    if (
                        policy is not None
                        and selector_capacity_available(
                            maximum, len(active), selector_future is not None
                        )
                        and time.monotonic() >= next_selector_at
                    ):
                        selector_future = executor.submit(
                            run_selector,
                            workflow,
                            policy,
                            tickets,
                            pull_request_snapshot,
                        )

                    selector_is_active = (
                        selector_future is not None and not selector_future.done()
                    )
                    selector_payload = (
                        {"state": "selecting"}
                        if selector_is_active
                        else selector_result
                    )
                    payload = {
                        "runs": [],
                        "failures": dispatch_errors,
                        "ci": ci_results,
                        "selector": selector_payload,
                        "blocked": blockers,
                    }
                    phase = (
                        "working and planning"
                        if active and selector_is_active
                        else (
                            "selecting next milestone"
                            if selector_is_active
                            else "working" if active else "waiting for milestone change"
                        )
                    )
                    watched: set[concurrent.futures.Future[dict[str, Any]]] = set(
                        active
                    )
                    if selector_future is not None:
                        watched.add(selector_future)
                    if not watched:
                        emit_watch_status(
                            dashboard=args.dashboard,
                            workflow=workflow,
                            tickets=tickets,
                            active_items=(),
                            eligible=eligible,
                            blockers=blockers,
                            payload=payload,
                            phase=phase,
                        )
                        time.sleep(workflow.poll_interval_seconds)
                        continue
                    done, _ = concurrent.futures.wait(
                        watched,
                        timeout=workflow.poll_interval_seconds,
                        return_when=concurrent.futures.FIRST_COMPLETED,
                    )
                    if not done:
                        emit_watch_status(
                            dashboard=args.dashboard,
                            workflow=workflow,
                            tickets=tickets,
                            active_items=tuple(active.values()),
                            eligible=eligible,
                            blockers=blockers,
                            payload={
                                "runs": [],
                                "failures": dispatch_errors,
                                "ci": ci_results,
                                "selector": selector_payload,
                                "blocked": blockers,
                            },
                            phase=phase,
                            selector_active=selector_is_active,
                        )
                        continue
                    completed = {
                        future: active.pop(future)
                        for future in done
                        if future in active
                    }
                    if not completed:
                        continue
                    results, failures = collect_results(completed, workflow)
                    payload = {
                        "runs": results,
                        "failures": [*dispatch_errors, *failures],
                        "ci": ci_results,
                        "blocked": blockers,
                    }
                    emit_watch_status(
                        dashboard=args.dashboard,
                        workflow=workflow,
                        tickets=tickets,
                        active_items=tuple(active.values()),
                        eligible=eligible,
                        blockers=blockers,
                        payload=payload,
                        phase=(
                            "working and planning"
                            if active and selector_is_active
                            else (
                                "selecting next milestone"
                                if selector_is_active
                                else (
                                    "working"
                                    if active
                                    else "waiting for milestone change"
                                )
                            )
                        ),
                        selector_active=selector_is_active,
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
            role_retry_path(item.ticket.number).unlink(missing_ok=True)
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
    role = role_for(item.ticket)
    if isinstance(error, StackInvalidated):
        resume_state = (
            "agent:ready"
            if role == "worker" and item.source_state == "agent:ready"
            else "agent:rework"
        )
        transition(workflow, item.ticket, resume_state)
        post_controller_comment(
            workflow,
            item.ticket,
            "Agent control-plane transition (stacked pull request)",
            "The reviewed parent changed, conflicted, or merged; returning the child "
            f"to `{resume_state}` without inventing a missing handoff: {error}",
        )
        return
    resume_state = retry_state_for(item, role)
    retry_path = persist_role_retry(workflow, item, error, role, resume_state)
    transition(workflow, item.ticket, resume_state)
    retry_record = json.loads(retry_path.read_text(encoding="utf-8"))
    if int(retry_record["attempt"]) > 1:
        return
    body = (
        f"Agent control-plane transition ({role} retry)\n\n"
        "The role process, sandbox, or controller contract failed after bounded "
        "in-run retries. This is not an external project blocker: the issue returned "
        f"to `{resume_state}` and will retry with bounded backoff while other work "
        "continues.\n\n"
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
            or f"failed to report retryable issue #{item.ticket.number}"
        )


if __name__ == "__main__":
    raise SystemExit(main())
