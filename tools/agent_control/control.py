#!/usr/bin/env python3
"""Deterministic GitHub/Codex control plane for DOMES agent work."""

from __future__ import annotations

import argparse
import concurrent.futures
import fcntl
import fnmatch
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
    "agent:verification": ("006B75", "Judge-approved work awaits CI verification"),
    "agent:human-review": ("FBCA04", "Agent workflow complete; human review boundary"),
    "agent:blocked": ("B60205", "External condition blocks further agent progress"),
    "agent:done": ("6F42C1", "Agent task reached its accepted terminal state"),
    "priority:p0": ("B60205", "Highest dispatch priority"),
    "priority:p1": ("D93F0B", "High dispatch priority"),
    "priority:p2": ("FBCA04", "Normal dispatch priority"),
    "priority:p3": ("C2E0C6", "Low dispatch priority"),
}


class ControlError(RuntimeError):
    """A deterministic validation or control-plane failure."""


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


def parse_sections(body: str) -> dict[str, str]:
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in body.splitlines():
        match = HEADING.match(line)
        if match:
            current = match.group(1).strip()
            sections.setdefault(current, [])
        elif current is not None:
            sections[current].append(line)
    return {name: "\n".join(lines).strip() for name, lines in sections.items()}


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
        raise ControlError(f"command failed: {' '.join(command)}: {detail}")
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ControlError(
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


def ensure_workspace(workflow: Workflow, item: TicketValidation, role: str) -> Path:
    workspace = workflow.workspace_root / f"issue-{item.ticket.number}"
    if workspace.exists():
        check = _git("rev-parse", "--is-inside-work-tree", cwd=workspace)
        if check.returncode != 0 or check.stdout.strip() != "true":
            raise ControlError(f"refusing non-worktree workspace: {workspace}")
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


def result_state(role: str, result: dict[str, Any]) -> str:
    if role == "worker" and result["blockers"]:
        return "agent:blocked"
    if role == "planner" and result["blockers"]:
        return "agent:blocked"
    if role in NEXT_STATE:
        return NEXT_STATE[role]
    if role == "judge":
        return {
            "approve": "agent:verification",
            "reject": "agent:rework",
            "blocked": "agent:blocked",
        }[result["verdict"]]
    if role == "verification-worker":
        return {
            "human_review": "agent:human-review",
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


def write_handoff(path: Path, result: dict[str, Any]) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


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


def execute_one(workflow: Workflow, item: TicketValidation) -> dict[str, Any]:
    role = role_for(item.ticket)
    workspace = ensure_workspace(workflow, item, role)
    schema = ORCHESTRATION_DIR / "schemas" / SCHEMA_BY_ROLE[role]
    state_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    run_root = state_root / "domes-agent-control" / f"issue-{item.ticket.number}"
    run_root.mkdir(parents=True, exist_ok=True)
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
    if role == "worker":
        verify_worker_artifact(workspace, item, result)
    write_handoff(run_root / f"handoff-{role}.json", result)
    post_result(workflow, item.ticket, role, result)
    transition(workflow, item.ticket, result_state(role, result))
    return {
        "issue": item.ticket.number,
        "role": role,
        "state": result_state(role, result),
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
    workspace: Path, item: TicketValidation, result: dict[str, Any]
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
    violations = [
        path
        for path in changed.stdout.splitlines()
        if not any(
            fnmatch.fnmatchcase(path, pattern)
            or path == pattern
            or path.startswith(f"{pattern}/")
            for pattern in surfaces
        )
    ]
    if violations:
        raise ControlError(
            f"issue #{item.ticket.number}: changes outside allowed surfaces: "
            f"{', '.join(violations)}"
        )


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
        enforce_scheduler_host(workflow)
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
            with concurrent.futures.ThreadPoolExecutor(max_workers=maximum) as executor:
                while True:
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
                        claimed = claim_for_dispatch(workflow, item)
                        active[executor.submit(execute_one, workflow, claimed)] = (
                            claimed
                        )

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
                        print(
                            json.dumps(
                                {"runs": [], "failures": [], "blocked": blockers},
                                indent=2,
                            ),
                            flush=True,
                        )
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
            block_failed_run(workflow, item, error)
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
