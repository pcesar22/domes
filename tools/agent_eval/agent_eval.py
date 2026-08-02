#!/usr/bin/env python3
"""Run and score repeatable DOMES coding-agent evaluations."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CASES = Path(__file__).with_name("cases.json")
DEFAULT_RESPONSE_SCHEMA = Path(__file__).with_name("response.schema.json")
RESPONSE_FIELDS = {
    "summary",
    "files",
    "invariants",
    "verification",
    "hardware_status",
    "claims",
}


@dataclass(frozen=True)
class EvaluationCase:
    identifier: str
    title: str
    category: str
    prompt: str
    required_files: tuple[str, ...]
    required_terms: tuple[str, ...]
    forbidden_terms: tuple[str, ...]
    sandbox: str
    hardware: str
    cleanup: str


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"expected a JSON object in {path}")
    return value


def _text(raw: dict[str, Any], key: str, index: int) -> str:
    value = raw.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"case {index} field {key} must be non-empty text")
    return value.strip()


def _text_list(raw: dict[str, Any], key: str, index: int) -> tuple[str, ...]:
    value = raw.get(key)
    if not isinstance(value, list) or any(
        not isinstance(item, str) or not item.strip() for item in value
    ):
        raise ValueError(f"case {index} field {key} must be an array of text")
    return tuple(item.strip() for item in value)


def load_cases(path: Path = DEFAULT_CASES) -> list[EvaluationCase]:
    document = read_json(path)
    if document.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported case schema version")
    raw_cases = document.get("cases")
    if not isinstance(raw_cases, list) or not raw_cases:
        raise ValueError("cases must be a non-empty array")

    cases: list[EvaluationCase] = []
    seen: set[str] = set()
    for index, raw in enumerate(raw_cases):
        if not isinstance(raw, dict):
            raise ValueError(f"case {index} must be an object")
        identifier = _text(raw, "id", index)
        if identifier in seen:
            raise ValueError(f"duplicate case id: {identifier}")
        seen.add(identifier)
        sandbox = _text(raw, "sandbox", index)
        if sandbox not in {"read-only", "workspace-write"}:
            raise ValueError(f"case {identifier} has invalid sandbox: {sandbox}")
        hardware = _text(raw, "hardware", index)
        if hardware not in {"not-required", "unavailable-gate", "required-gate"}:
            raise ValueError(f"case {identifier} has invalid hardware mode: {hardware}")
        case = EvaluationCase(
            identifier=identifier,
            title=_text(raw, "title", index),
            category=_text(raw, "category", index),
            prompt=_text(raw, "prompt", index),
            required_files=_text_list(raw, "required_files", index),
            required_terms=_text_list(raw, "required_terms", index),
            forbidden_terms=_text_list(raw, "forbidden_terms", index),
            sandbox=sandbox,
            hardware=hardware,
            cleanup=_text(raw, "cleanup", index),
        )
        for relative in case.required_files:
            if relative.startswith("/") or ".." in Path(relative).parts:
                raise ValueError(f"case {identifier} has unsafe path: {relative}")
        cases.append(case)
    return cases


def validate_response(response: dict[str, Any]) -> None:
    if set(response) != RESPONSE_FIELDS:
        raise ValueError("structured response fields do not match the schema")
    if not isinstance(response["summary"], str):
        raise ValueError("structured response summary must be text")
    for key in ("files", "invariants", "verification", "claims"):
        value = response[key]
        if not isinstance(value, list) or any(
            not isinstance(item, str) for item in value
        ):
            raise ValueError(f"structured response {key} must contain text")
    if response["hardware_status"] not in {
        "not_required",
        "automated_only",
        "unavailable",
        "required",
    }:
        raise ValueError("structured response has invalid hardware_status")


def score_response(case: EvaluationCase, response: dict[str, Any]) -> dict[str, Any]:
    flattened = json.dumps(response, sort_keys=True).casefold()
    files = {item.casefold() for item in response["files"]}
    criteria: list[dict[str, Any]] = []
    for relative in case.required_files:
        criteria.append(
            {
                "kind": "required_file",
                "value": relative,
                "passed": relative.casefold() in files,
            }
        )
    for term in case.required_terms:
        criteria.append(
            {
                "kind": "required_term",
                "value": term,
                "passed": term.casefold() in flattened,
            }
        )
    for term in case.forbidden_terms:
        criteria.append(
            {
                "kind": "forbidden_term",
                "value": term,
                "passed": term.casefold() not in flattened,
            }
        )
    expected_hardware = {
        "not-required": "not_required",
        "unavailable-gate": "unavailable",
        "required-gate": "required",
    }[case.hardware]
    criteria.append(
        {
            "kind": "hardware_status",
            "value": expected_hardware,
            "passed": response["hardware_status"] == expected_hardware,
        }
    )
    score = sum(1 for criterion in criteria if criterion["passed"])
    return {
        "passed": score == len(criteria),
        "score": score,
        "possible": len(criteria),
        "criteria": criteria,
    }


def git(*args: str, cwd: Path = ROOT) -> str:
    process = subprocess.run(
        ["git", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    )
    return process.stdout.strip()


def usage_from_events(output: str) -> dict[str, int]:
    latest: dict[str, int] = {}
    wanted = {
        "input_tokens",
        "output_tokens",
        "reasoning_tokens",
        "cached_input_tokens",
        "cached_tokens",
    }

    def visit(value: Any) -> None:
        if isinstance(value, dict):
            for key, nested in value.items():
                if key in wanted and isinstance(nested, int):
                    latest[key] = nested
                visit(nested)
        elif isinstance(value, list):
            for nested in value:
                visit(nested)

    for line in output.splitlines():
        try:
            visit(json.loads(line))
        except json.JSONDecodeError:
            continue
    return latest


def case_prompt(case: EvaluationCase) -> str:
    return f"""DOMES agent evaluation: {case.title}

Work only inside the checked-out repository. This evaluation is {case.sandbox}.
Do not modify GitHub, external services, host configuration, or physical devices.
Do not claim hardware behavior was verified. Inspect the repository and return the
requested structured assessment.

Task:
{case.prompt}

In the final structured response, list authoritative or affected paths in `files`,
durable contracts in `invariants`, required checks in `verification`, and the honest
hardware state in `hardware_status`. Keep `claims` supported by repository evidence.
"""


def codex_environment() -> dict[str, str]:
    allowed = {
        "ALL_PROXY",
        "CODEX_HOME",
        "HOME",
        "HTTPS_PROXY",
        "HTTP_PROXY",
        "LANG",
        "LC_ALL",
        "NO_PROXY",
        "PATH",
        "SHELL",
        "SSL_CERT_DIR",
        "SSL_CERT_FILE",
        "TERM",
        "TMPDIR",
        "USER",
        "all_proxy",
        "https_proxy",
        "http_proxy",
        "no_proxy",
    }
    environment = {key: value for key, value in os.environ.items() if key in allowed}
    environment["GIT_TERMINAL_PROMPT"] = "0"
    environment["NO_COLOR"] = "1"
    return environment


def run_case(
    case: EvaluationCase,
    model: str,
    effort: str,
    revision: str,
    timeout_seconds: int,
    allow_write_cases: bool,
) -> dict[str, Any]:
    if case.sandbox == "workspace-write" and not allow_write_cases:
        return {
            "id": case.identifier,
            "title": case.title,
            "status": "skipped",
            "reason": "write case requires --allow-write-cases",
        }

    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="domes-agent-eval-") as directory:
        checkout = Path(directory) / "checkout"
        response_path = Path(directory) / "response.json"
        added = False
        try:
            git("worktree", "add", "--detach", str(checkout), revision)
            added = True
            command = [
                "codex",
                "exec",
                "--ephemeral",
                "--ignore-user-config",
                "--strict-config",
                "--json",
                "--color",
                "never",
                "--model",
                model,
                "--sandbox",
                case.sandbox,
                "--config",
                f'model_reasoning_effort="{effort}"',
                "--config",
                'approval_policy="never"',
                "--output-schema",
                str(DEFAULT_RESPONSE_SCHEMA),
                "--output-last-message",
                str(response_path),
                "--cd",
                str(checkout),
                case_prompt(case),
            ]
            process = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
                stdin=subprocess.DEVNULL,
                timeout=timeout_seconds,
                env=codex_environment(),
            )
            duration = round(time.monotonic() - started, 3)
            if process.returncode != 0 or not response_path.exists():
                return {
                    "id": case.identifier,
                    "title": case.title,
                    "status": "error",
                    "duration_seconds": duration,
                    "exit_code": process.returncode,
                    "error": (process.stdout + process.stderr)[-4000:],
                    "usage": usage_from_events(process.stdout),
                }
            response = read_json(response_path)
            validate_response(response)
            score = score_response(case, response)
            changes = git("status", "--short", cwd=checkout).splitlines()
            if case.sandbox == "read-only" and changes:
                score["passed"] = False
                score["criteria"].append(
                    {
                        "kind": "read_only_checkout",
                        "value": "no repository changes",
                        "passed": False,
                    }
                )
                score["possible"] += 1
            return {
                "id": case.identifier,
                "title": case.title,
                "category": case.category,
                "status": "completed",
                "duration_seconds": duration,
                "usage": usage_from_events(process.stdout),
                "response_sha256": hashlib.sha256(
                    response_path.read_bytes()
                ).hexdigest(),
                "response": response,
                "checkout_changes": changes,
                **score,
            }
        except subprocess.TimeoutExpired:
            return {
                "id": case.identifier,
                "title": case.title,
                "status": "error",
                "duration_seconds": round(time.monotonic() - started, 3),
                "error": f"timed out after {timeout_seconds} seconds",
            }
        except (OSError, subprocess.CalledProcessError, ValueError) as error:
            return {
                "id": case.identifier,
                "title": case.title,
                "status": "error",
                "duration_seconds": round(time.monotonic() - started, 3),
                "error": str(error),
            }
        finally:
            if added:
                subprocess.run(
                    ["git", "worktree", "remove", "--force", str(checkout)],
                    cwd=ROOT,
                    check=False,
                    capture_output=True,
                    text=True,
                )
                subprocess.run(
                    ["git", "worktree", "prune"],
                    cwd=ROOT,
                    check=False,
                    capture_output=True,
                    text=True,
                )


def summarize(results: list[dict[str, Any]], planned: int) -> dict[str, int]:
    completed = [result for result in results if result["status"] == "completed"]
    return {
        "total": planned,
        "recorded": len(results),
        "pending": planned - len(results),
        "completed": len(completed),
        "passed": sum(1 for result in completed if result["passed"]),
        "errors": sum(1 for result in results if result["status"] == "error"),
        "skipped": sum(1 for result in results if result["status"] == "skipped"),
        "score": sum(result["score"] for result in completed),
        "possible": sum(result["possible"] for result in completed),
    }


def checkpoint(path: Path, document: dict[str, Any], planned: int) -> None:
    document["summary"] = summarize(document["results"], planned)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def command_version(command: list[str]) -> str | None:
    try:
        process = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=10,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    output = process.stdout.strip() or process.stderr.strip()
    return output.splitlines()[0] if output else None


def run_evaluations(args: argparse.Namespace) -> int:
    cases = load_cases(args.cases)
    selected = set(args.case or [])
    if selected:
        unknown = selected - {case.identifier for case in cases}
        if unknown:
            raise ValueError(f"unknown case ids: {', '.join(sorted(unknown))}")
        cases = [case for case in cases if case.identifier in selected]
    revision = git("rev-parse", "--verify", f"{args.revision}^{{commit}}")
    dirty = git("status", "--porcelain")
    if dirty and not args.allow_dirty:
        raise ValueError("working tree is dirty; commit it or use --allow-dirty")

    case_digest = hashlib.sha256(args.cases.read_bytes()).hexdigest()
    if args.resume:
        if not args.output.exists():
            raise ValueError(f"cannot resume missing result: {args.output}")
        document = read_json(args.output)
        expected = {
            "run_id": args.run_id,
            "revision": revision,
            "model": args.model,
            "reasoning_effort": args.effort,
            "case_definition_sha256": case_digest,
        }
        mismatched = [
            key for key, value in expected.items() if document.get(key) != value
        ]
        if mismatched:
            raise ValueError("cannot resume with different " + ", ".join(mismatched))
    else:
        if args.output.exists():
            raise ValueError(f"result exists: {args.output}; use --resume")
        document = {
            "schema_version": SCHEMA_VERSION,
            "run_id": args.run_id,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "repository": "pcesar22/domes",
            "revision": revision,
            "model": args.model,
            "reasoning_effort": args.effort,
            "case_definition_sha256": case_digest,
            "environment": {
                "platform": platform.platform(),
                "python": platform.python_version(),
                "codex": command_version(["codex", "--version"]),
            },
            "results": [],
            "summary": {},
        }
        checkpoint(args.output, document, len(cases))

    recorded = {result["id"] for result in document["results"]}
    pending = [case for case in cases if case.identifier not in recorded]
    if recorded:
        print(f"resuming with {len(recorded)} recorded case(s)", flush=True)
    for index, case in enumerate(pending, start=1):
        print(f"[{index}/{len(pending)}] {case.identifier}", flush=True)
        result = run_case(
            case,
            args.model,
            args.effort,
            revision,
            args.timeout,
            args.allow_write_cases,
        )
        result["attempts"] = 1
        result["retries"] = 0
        document["results"].append(result)
        checkpoint(args.output, document, len(cases))
        print(f"  {result['status']}", flush=True)

    summary = document["summary"]
    print(f"wrote {args.output}")
    if summary["errors"] or summary["pending"]:
        return 1
    if not args.allow_failures and summary["passed"] != summary["completed"]:
        return 1
    return 0


def render_report(document: dict[str, Any]) -> str:
    summary = document["summary"]
    lines = [
        f"# Agent Evaluation: {document['run_id']}",
        "",
        f"- Revision: `{document['revision']}`",
        f"- Model: `{document['model']}`",
        f"- Reasoning effort: `{document['reasoning_effort']}`",
        f"- Completed: {summary['completed']} / {summary['total']}",
        f"- Passed cases: {summary['passed']} / {summary['completed']}",
        f"- Criteria score: {summary['score']} / {summary['possible']}",
        f"- Errors: {summary['errors']}",
        f"- Pending: {summary['pending']}",
        "",
        "| Case | Status | Score | Duration | Hardware status |",
        "| --- | --- | ---: | ---: | --- |",
    ]
    for result in document["results"]:
        completed = result["status"] == "completed"
        status = (
            "pass"
            if result.get("passed")
            else "fail" if completed else result["status"]
        )
        score = f"{result['score']}/{result['possible']}" if completed else "-"
        duration = f"{result.get('duration_seconds', 0):.1f}s"
        hardware = result.get("response", {}).get("hardware_status", "-")
        lines.append(
            f"| {result['id']} | {status} | {score} | {duration} | {hardware} |"
        )
    lines.extend(
        [
            "",
            "## Interpretation",
            "",
            "This is a repository-understanding and change-planning baseline. "
            "Compare one model, instruction, or tooling variable at a time.",
            "",
        ]
    )
    return "\n".join(lines)


def validate_command(args: argparse.Namespace) -> int:
    cases = load_cases(args.cases)
    schema = read_json(args.response_schema)
    required = set(schema.get("required", []))
    if schema.get("type") != "object" or required != RESPONSE_FIELDS:
        raise ValueError("response schema does not match the response contract")
    print(f"validated {len(cases)} cases")
    return 0


def report_command(args: argparse.Namespace) -> int:
    report = render_report(read_json(args.input))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
        print(f"wrote {args.output}")
    else:
        print(report, end="")
    return 0


def render_comparison(documents: list[dict[str, Any]]) -> str:
    digests = {document.get("case_definition_sha256") for document in documents}
    if len(digests) != 1:
        raise ValueError("comparison inputs use different case definitions")
    lines = [
        "# Agent Evaluation Comparison",
        "",
        "| Run | Revision | Model / effort | Cases | Criteria | Duration | "
        "Input / cached / output tokens |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for document in documents:
        summary = document["summary"]
        results = document["results"]
        usage: dict[str, int] = {}
        for result in results:
            for key, value in result.get("usage", {}).items():
                usage[key] = usage.get(key, 0) + value
        duration = sum(result.get("duration_seconds", 0) for result in results)
        lines.append(
            f"| {document['run_id']} | `{document['revision'][:12]}` | "
            f"`{document['model']}` / `{document['reasoning_effort']}` | "
            f"{summary['passed']}/{summary['completed']} | "
            f"{summary['score']}/{summary['possible']} | {duration:.1f}s | "
            f"{usage.get('input_tokens', 0)} / {usage.get('cached_input_tokens', 0)} / "
            f"{usage.get('output_tokens', 0)} |"
        )
    lines.extend(
        [
            "",
            "Compare runs only when their case digest matches. Change one model, "
            "effort, instruction, tooling, or delegation variable at a time.",
            "",
        ]
    )
    return "\n".join(lines)


def compare_command(args: argparse.Namespace) -> int:
    if len(args.input) < 2:
        raise ValueError("comparison requires at least two input files")
    comparison = render_comparison([read_json(path) for path in args.input])
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(comparison, encoding="utf-8")
        print(f"wrote {args.output}")
    else:
        print(comparison, end="")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate")
    validate.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    validate.add_argument(
        "--response-schema", type=Path, default=DEFAULT_RESPONSE_SCHEMA
    )
    validate.set_defaults(func=validate_command)
    run = subparsers.add_parser("run")
    run.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    run.add_argument("--case", action="append")
    run.add_argument("--model", default="gpt-5.6-sol")
    run.add_argument("--effort", default="medium")
    run.add_argument("--revision", default="HEAD")
    run.add_argument("--run-id", required=True)
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--timeout", type=int, default=600)
    run.add_argument("--allow-dirty", action="store_true")
    run.add_argument("--allow-write-cases", action="store_true")
    run.add_argument("--allow-failures", action="store_true")
    run.add_argument("--resume", action="store_true")
    run.set_defaults(func=run_evaluations)
    report = subparsers.add_parser("report")
    report.add_argument("--input", type=Path, required=True)
    report.add_argument("--output", type=Path)
    report.set_defaults(func=report_command)
    compare = subparsers.add_parser("compare")
    compare.add_argument("--input", type=Path, action="append", required=True)
    compare.add_argument("--output", type=Path)
    compare.set_defaults(func=compare_command)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except ValueError as error:
        parser.error(str(error))
    return 2


if __name__ == "__main__":
    sys.exit(main())
