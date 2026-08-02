#!/usr/bin/env python3
"""Select DOMES verification checks and produce structured summaries."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = 1
ROOT = Path(__file__).resolve().parents[2]
CHECKS = (
    ("protocol", "Generated protocol bindings"),
    ("host_firmware", "Host firmware tests"),
    ("cli", "Rust CLI"),
    ("host_tooling", "Host tooling and documentation"),
    ("flutter", "Flutter app"),
    ("firmware", "ESP-IDF firmware and release package"),
)
ALL_CHECKS = tuple(identifier for identifier, _ in CHECKS)
CI_JOB_BY_CHECK = {
    "protocol": "firmware-build",
    "host_firmware": "unit-tests",
    "cli": "cli-build",
    "host_tooling": "host-tooling",
    "flutter": "flutter",
    "firmware": "firmware-build",
}
COMPONENT_CHECKS = {
    "firmware": ("host_firmware", "host_tooling", "firmware"),
    "cli": ("cli", "host_tooling"),
    "flutter": ("flutter", "host_tooling"),
    "docs": ("host_tooling",),
    "tooling": ("host_tooling",),
    "protocol": ALL_CHECKS,
    "workflow": ALL_CHECKS,
}


def write_json(path: Path, document: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def git_changed_paths(root: Path, base: str) -> list[str]:
    revision = subprocess.run(
        ["git", "rev-parse", "--verify", f"{base}^{{commit}}"],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
    )
    if revision.returncode != 0:
        raise ValueError(f"invalid Git base: {base}")
    tracked = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACDMR", base, "--"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    return sorted({path for path in tracked + untracked if path})


def _is_documentation(path: str) -> bool:
    return (
        path.endswith(".md")
        or path.startswith("docs/")
        or path.startswith("research/")
        or path.endswith("AGENTS.md")
    )


def classify_path(path: str) -> tuple[set[str], list[str]]:
    lowered = path.casefold()
    hardware: list[str] = []
    workflow = path.startswith(".github/workflows/")
    ota = "ota" in lowered
    transport = (
        "/transport/" in lowered
        or "/protocol/" in lowered
        or "configcommandhandler" in lowered
        or "config_protocol" in lowered
        or "framecodec" in lowered
        or "frame_codec" in lowered
    )
    protobuf = (
        path.startswith("firmware/common/proto/")
        or path == "tools/generate_protocols.sh"
        or "/proto/generated/" in path
    )
    if workflow:
        if path.endswith("firmware-hw-test.yml"):
            hardware.extend(("multi_device", "physical_hardware"))
        return set(ALL_CHECKS), hardware
    if protobuf or transport or ota:
        hardware.append("protocol_transport")
        if ota:
            hardware.append("ota")
        if "espnow" in lowered or "esp-now" in lowered or "multi_pod" in lowered:
            hardware.append("multi_device")
        if path.startswith("ios/domes_app/"):
            hardware.append("mobile_ble")
        return set(ALL_CHECKS), hardware
    if _is_documentation(path):
        return {"host_tooling"}, hardware
    if path.startswith("firmware/"):
        hardware.append("single_device")
        return {"host_firmware", "host_tooling", "firmware"}, hardware
    if path.startswith("tools/domes-cli/"):
        return {"cli", "host_tooling"}, hardware
    if path.startswith("ios/domes_app/"):
        if "ble" in lowered or "drill" in lowered:
            hardware.append("mobile_ble")
        return {"flutter", "host_tooling"}, hardware
    if path.startswith("tools/firmware/"):
        hardware.append("single_device")
        return {"host_firmware", "host_tooling", "firmware"}, hardware
    if path.startswith(("tools/", "scripts/")) or path in {
        ".pre-commit-config.yaml",
        "pyproject.toml",
    }:
        return {"host_tooling"}, hardware
    if path.startswith("hardware/"):
        hardware.append("physical_hardware")
        return {"host_tooling"}, hardware
    return set(ALL_CHECKS), hardware


def _hardware_entries(identifiers: Iterable[str]) -> list[dict[str, str]]:
    descriptions = {
        "single_device": "Flash one pod and verify the affected runtime behavior.",
        "protocol_transport": "Verify request/response behavior on each affected transport.",
        "ota": "Verify OTA success/reboot checks and forced rollback separately.",
        "multi_device": "Use two pods for discovery, fan-out, and peer behavior.",
        "mobile_ble": "Use a supported app host and physical pod for BLE/device behavior.",
        "physical_hardware": "Perform the physical hardware inspection or measurement.",
    }
    return [
        {
            "id": identifier,
            "status": "outstanding",
            "reason": descriptions[identifier],
        }
        for identifier in sorted(set(identifiers))
    ]


def build_plan(
    *,
    changed_paths: list[str] | None,
    components: list[str],
    quick: bool,
    base: str | None,
) -> dict[str, Any]:
    selected: set[str] = set()
    reasons: dict[str, list[str]] = {identifier: [] for identifier in ALL_CHECKS}
    hardware: list[str] = []
    if changed_paths is None and not components:
        selected.update(ALL_CHECKS)
        for identifier in selected:
            reasons[identifier].append("full repository gate")
        mode = "quick" if quick else "full"
    else:
        mode = "scoped"
        for component in components:
            for identifier in COMPONENT_CHECKS[component]:
                selected.add(identifier)
                reasons[identifier].append(f"component: {component}")
            if component == "firmware":
                hardware.append("single_device")
            elif component == "protocol":
                hardware.append("protocol_transport")
        for path in changed_paths or []:
            path_checks, path_hardware = classify_path(path)
            for identifier in path_checks:
                selected.add(identifier)
                reasons[identifier].append(f"changed path: {path}")
            hardware.extend(path_hardware)
    if quick and "firmware" in selected:
        selected.remove("firmware")
        reasons["firmware"].append("skipped by --quick")
    if changed_paths is None and not components:
        hardware_entries = [
            {
                "id": "change_specific_hardware",
                "status": "not_assessed",
                "reason": (
                    "Full verification is a software gate; apply docs/TESTING.md to "
                    "the change before claiming hardware behavior."
                ),
            }
        ]
    else:
        hardware_entries = _hardware_entries(hardware)
    return {
        "schema_version": SCHEMA_VERSION,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "mode": mode,
        "base": base,
        "components": components,
        "changed_paths": changed_paths or [],
        "quick": quick,
        "checks": [
            {
                "id": identifier,
                "title": title,
                "ci_job": CI_JOB_BY_CHECK[identifier],
                "selected": identifier in selected,
                "selection_reasons": sorted(set(reasons[identifier]))
                or ["not selected for this scope"],
            }
            for identifier, title in CHECKS
        ],
        "hardware": hardware_entries,
    }


def read_results(path: Path) -> dict[str, dict[str, Any]]:
    results: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return results
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        fields = line.split("\t")
        if len(fields) != 5:
            raise ValueError(f"invalid result line {line_number}")
        identifier, status, exit_code, duration, log_path = fields
        if status not in {"passed", "failed", "skipped"}:
            raise ValueError(f"invalid result status on line {line_number}")
        results[identifier] = {
            "status": status,
            "exit_code": int(exit_code),
            "duration_seconds": int(duration),
            "log": log_path or None,
        }
    return results


def summarize(
    plan: dict[str, Any], results: dict[str, dict[str, Any]], artifacts: str | None
) -> dict[str, Any]:
    checks = []
    for check in plan["checks"]:
        result = results.get(check["id"])
        if result is None:
            result = (
                {
                    "status": "failed",
                    "exit_code": 1,
                    "duration_seconds": 0,
                    "log": None,
                }
                if check["selected"]
                else {
                    "status": "skipped",
                    "exit_code": 0,
                    "duration_seconds": 0,
                    "log": None,
                }
            )
        checks.append({**check, **result})
    failed = sum(check["status"] == "failed" for check in checks)
    passed = sum(check["status"] == "passed" for check in checks)
    skipped = sum(check["status"] == "skipped" for check in checks)
    return {
        "schema_version": SCHEMA_VERSION,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "mode": plan["mode"],
        "base": plan["base"],
        "components": plan["components"],
        "changed_paths": plan["changed_paths"],
        "quick": plan["quick"],
        "artifacts": artifacts,
        "checks": checks,
        "hardware": plan["hardware"],
        "summary": {
            "passed": passed,
            "failed": failed,
            "skipped": skipped,
            "exit_code": 1 if failed else 0,
        },
    }


def render_plan(plan: dict[str, Any]) -> str:
    lines = [f"Verification selection ({plan['mode']}):"]
    for check in plan["checks"]:
        action = "RUN" if check["selected"] else "SKIP"
        reasons = "; ".join(check["selection_reasons"])
        lines.append(f"  {action:<4} {check['id']}: {reasons}")
    return "\n".join(lines) + "\n"


def render_summary(document: dict[str, Any]) -> str:
    summary = document["summary"]
    lines = [
        (
            f"Verification summary: {summary['passed']} passed, "
            f"{summary['failed']} failed, {summary['skipped']} skipped"
        )
    ]
    if document["hardware"]:
        lines.append("Hardware status:")
        for item in document["hardware"]:
            lines.append(f"  {item['status'].upper()} {item['id']}: {item['reason']}")
    else:
        lines.append("Hardware status: no hardware checks selected for this scope")
    if document["artifacts"]:
        lines.append(f"Artifacts: {document['artifacts']}")
    return "\n".join(lines) + "\n"


def plan_command(args: argparse.Namespace) -> int:
    unknown = set(args.component) - set(COMPONENT_CHECKS)
    if unknown:
        raise ValueError(f"unknown component: {', '.join(sorted(unknown))}")
    paths = git_changed_paths(args.root, args.base) if args.base else None
    plan = build_plan(
        changed_paths=paths,
        components=args.component,
        quick=args.quick,
        base=args.base,
    )
    write_json(args.output, plan)
    return 0


def selected_command(args: argparse.Namespace) -> int:
    plan = json.loads(args.input.read_text(encoding="utf-8"))
    for check in plan["checks"]:
        if check["selected"]:
            print(check["id"])
    return 0


def summarize_command(args: argparse.Namespace) -> int:
    plan = json.loads(args.plan.read_text(encoding="utf-8"))
    document = summarize(plan, read_results(args.results), args.artifacts)
    write_json(args.output, document)
    return document["summary"]["exit_code"]


def render_plan_command(args: argparse.Namespace) -> int:
    print(render_plan(json.loads(args.input.read_text(encoding="utf-8"))), end="")
    return 0


def render_summary_command(args: argparse.Namespace) -> int:
    print(render_summary(json.loads(args.input.read_text(encoding="utf-8"))), end="")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan = subparsers.add_parser("plan")
    plan.add_argument("--root", type=Path, default=ROOT)
    plan.add_argument("--base")
    plan.add_argument("--component", action="append", default=[])
    plan.add_argument("--quick", action="store_true")
    plan.add_argument("--output", type=Path, required=True)
    plan.set_defaults(func=plan_command)
    selected = subparsers.add_parser("selected")
    selected.add_argument("--input", type=Path, required=True)
    selected.set_defaults(func=selected_command)
    summary = subparsers.add_parser("summarize")
    summary.add_argument("--plan", type=Path, required=True)
    summary.add_argument("--results", type=Path, required=True)
    summary.add_argument("--output", type=Path, required=True)
    summary.add_argument("--artifacts")
    summary.set_defaults(func=summarize_command)
    plan_text = subparsers.add_parser("render-plan")
    plan_text.add_argument("--input", type=Path, required=True)
    plan_text.set_defaults(func=render_plan_command)
    summary_text = subparsers.add_parser("render-summary")
    summary_text.add_argument("--input", type=Path, required=True)
    summary_text.set_defaults(func=render_summary_command)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except (
        OSError,
        subprocess.CalledProcessError,
        ValueError,
        json.JSONDecodeError,
    ) as error:
        parser.error(str(error))
    return 2


if __name__ == "__main__":
    sys.exit(main())
