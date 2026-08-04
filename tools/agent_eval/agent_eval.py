#!/usr/bin/env python3
"""Run and capture auditable DOMES coding-agent evaluations."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 3
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CASES = Path(__file__).with_name("cases.json")
DEFAULT_RESPONSE_SCHEMA = Path(__file__).with_name("response.schema.json")
RESPONSE_FIELDS = {
    "summary",
    "files",
    "invariants",
    "verification",
    "hardware_requirement",
    "claims",
    "criterion_evidence",
}
RESPONSE_ARRAY_FIELDS = {"files", "invariants", "verification", "claims"}
CRITERION_EVIDENCE_FIELDS = {"criterion_id", "evidence", "files"}
HARDWARE_REQUIREMENTS = {"not_required", "required"}
HARDWARE_REQUIREMENT_DESCRIPTION = (
    "Whether physical hardware is required for full validation. This is not an "
    "execution status; the evaluation harness never accesses hardware."
)
CRITERION_ID_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
EVALUATOR_RELATIVE_PATH = Path("tools/agent_eval")

# Codex needs its executable path, auth location, locale, and TLS configuration.
# HOME and temporary paths are replaced with case-local directories. Proxy and
# user-session variables are deliberately omitted because they can carry secrets.
CHILD_ENV_ALLOWLIST = {
    "LANG",
    "LANGUAGE",
    "LC_ALL",
    "LC_CTYPE",
    "SSL_CERT_DIR",
    "SSL_CERT_FILE",
    "TERM",
}
SENSITIVE_ENV_VARS = {
    "ANTHROPIC_API_KEY",
    "AWS_ACCESS_KEY_ID",
    "AWS_SECRET_ACCESS_KEY",
    "AWS_SESSION_TOKEN",
    "AZURE_CLIENT_SECRET",
    "CODEX_API_KEY",
    "DOCKER_AUTH_CONFIG",
    "GOOGLE_APPLICATION_CREDENTIALS",
    "GITHUB_TOKEN",
    "GH_TOKEN",
    "HF_TOKEN",
    "NPM_TOKEN",
    "OPENAI_API_KEY",
    "SSH_AUTH_SOCK",
}


@dataclass(frozen=True)
class EvaluationCriterion:
    identifier: str
    description: str


@dataclass(frozen=True)
class EvaluationCase:
    identifier: str
    title: str
    category: str
    prompt: str
    reference_files: tuple[str, ...]
    criteria: tuple[EvaluationCriterion, ...]
    sandbox: str
    hardware_requirement: str
    cleanup: str


@dataclass(frozen=True)
class PreparedCheckout:
    path: Path
    snapshot_revision: str
    submodules: tuple[dict[str, str], ...]
    baseline_manifest: dict[str, str]


@dataclass(frozen=True)
class LocalSubmodule:
    path: str
    revision: str
    repository: Path


@dataclass(frozen=True)
class EvaluationRuntime:
    codex_executable: Path
    codex_read_root: Path
    bubblewrap_executable: Path
    shell_path: str


def _decode_json(document: bytes, source: Path) -> dict[str, Any]:
    try:
        value = json.loads(document.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read JSON from {source}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"expected a JSON object in {source}")
    return value


def _read_json(path: Path) -> dict[str, Any]:
    try:
        document = path.read_bytes()
    except OSError as error:
        raise ValueError(f"cannot read JSON from {path}: {error}") from error
    return _decode_json(document, path)


def validate_response_schema(document: dict[str, Any]) -> None:
    """Validate the checked-in response contract without a third-party package."""
    if document.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise ValueError("response schema must declare JSON Schema draft 2020-12")
    if document.get("type") != "object":
        raise ValueError("response schema root type must be object")
    if document.get("additionalProperties") is not False:
        raise ValueError("response schema must reject additional properties")

    required = document.get("required")
    if not isinstance(required, list) or any(
        not isinstance(item, str) for item in required
    ):
        raise ValueError("response schema required must be an array of strings")
    if len(required) != len(set(required)) or set(required) != RESPONSE_FIELDS:
        raise ValueError("response schema required fields do not match the contract")

    properties = document.get("properties")
    if not isinstance(properties, dict) or set(properties) != RESPONSE_FIELDS:
        raise ValueError("response schema properties do not match the contract")
    summary = properties.get("summary")
    if not isinstance(summary, dict) or summary.get("type") != "string":
        raise ValueError("response summary must be a string")

    for field in RESPONSE_ARRAY_FIELDS:
        definition = properties.get(field)
        if not isinstance(definition, dict) or definition.get("type") != "array":
            raise ValueError(f"response {field} must be an array")
        items = definition.get("items")
        if not isinstance(items, dict) or items.get("type") != "string":
            raise ValueError(f"response {field} items must be strings")

    hardware = properties.get("hardware_requirement")
    if not isinstance(hardware, dict) or hardware.get("type") != "string":
        raise ValueError("response hardware_requirement must be a string")
    if hardware.get("description") != HARDWARE_REQUIREMENT_DESCRIPTION:
        raise ValueError("response hardware_requirement description is ambiguous")
    values = hardware.get("enum")
    if (
        not isinstance(values, list)
        or any(not isinstance(value, str) for value in values)
        or len(values) != len(set(values))
        or set(values) != HARDWARE_REQUIREMENTS
    ):
        raise ValueError(
            "response hardware_requirement enum does not match the contract"
        )

    criterion_evidence = properties.get("criterion_evidence")
    if (
        not isinstance(criterion_evidence, dict)
        or criterion_evidence.get("type") != "array"
    ):
        raise ValueError("response criterion_evidence must be an array")
    item = criterion_evidence.get("items")
    if not isinstance(item, dict) or item.get("type") != "object":
        raise ValueError("response criterion_evidence items must be objects")
    if item.get("additionalProperties") is not False:
        raise ValueError("criterion evidence must reject additional properties")
    item_required = item.get("required")
    if (
        not isinstance(item_required, list)
        or any(not isinstance(value, str) for value in item_required)
        or set(item_required) != CRITERION_EVIDENCE_FIELDS
        or len(item_required) != len(set(item_required))
    ):
        raise ValueError("criterion evidence required fields do not match the contract")
    item_properties = item.get("properties")
    if not isinstance(item_properties, dict) or set(item_properties) != (
        CRITERION_EVIDENCE_FIELDS
    ):
        raise ValueError("criterion evidence properties do not match the contract")
    criterion_id = item_properties.get("criterion_id")
    if not isinstance(criterion_id, dict) or criterion_id.get("type") != "string":
        raise ValueError("criterion evidence id must be a string")
    for field in ("evidence", "files"):
        definition = item_properties.get(field)
        if not isinstance(definition, dict) or definition.get("type") != "array":
            raise ValueError(f"criterion evidence {field} must be an array")
        items = definition.get("items")
        if not isinstance(items, dict) or items.get("type") != "string":
            raise ValueError(f"criterion evidence {field} items must be strings")


def validate_response_document(response: dict[str, Any]) -> None:
    if set(response) != RESPONSE_FIELDS:
        raise ValueError("structured response fields do not match the response schema")
    if not isinstance(response["summary"], str) or not response["summary"].strip():
        raise ValueError("structured response summary must be a string")
    for field in RESPONSE_ARRAY_FIELDS:
        value = response[field]
        if not isinstance(value, list) or any(
            not isinstance(item, str) or not item.strip() for item in value
        ):
            raise ValueError(
                f"structured response {field} must contain only non-empty strings"
            )
    hardware_requirement = response["hardware_requirement"]
    if (
        not isinstance(hardware_requirement, str)
        or hardware_requirement not in HARDWARE_REQUIREMENTS
    ):
        raise ValueError("structured response has an invalid hardware_requirement")

    criterion_evidence = response["criterion_evidence"]
    if not isinstance(criterion_evidence, list):
        raise ValueError("structured response criterion_evidence must be an array")
    seen: set[str] = set()
    for index, entry in enumerate(criterion_evidence):
        if not isinstance(entry, dict) or set(entry) != CRITERION_EVIDENCE_FIELDS:
            raise ValueError(
                f"criterion evidence {index} fields do not match the schema"
            )
        criterion_id = entry["criterion_id"]
        if not isinstance(criterion_id, str) or not CRITERION_ID_PATTERN.fullmatch(
            criterion_id
        ):
            raise ValueError(f"criterion evidence {index} has an invalid id")
        if criterion_id in seen:
            raise ValueError(f"duplicate criterion evidence id: {criterion_id}")
        seen.add(criterion_id)
        for field in ("evidence", "files"):
            values = entry[field]
            if (
                not isinstance(values, list)
                or not values
                or any(
                    not isinstance(value, str) or not value.strip() for value in values
                )
            ):
                raise ValueError(
                    f"criterion evidence {index} {field} must contain non-empty strings"
                )


def _require_text(raw: dict[str, Any], key: str, index: int) -> str:
    value = raw.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"case {index} field {key} must be non-empty text")
    return value.strip()


def _require_text_list(raw: dict[str, Any], key: str, index: int) -> tuple[str, ...]:
    value = raw.get(key)
    if not isinstance(value, list) or any(
        not isinstance(item, str) or not item.strip() for item in value
    ):
        raise ValueError(f"case {index} field {key} must be an array of text")
    return tuple(item.strip() for item in value)


def _load_criteria(raw: dict[str, Any], index: int) -> tuple[EvaluationCriterion, ...]:
    values = raw.get("criteria")
    if not isinstance(values, list) or not values:
        raise ValueError(f"case {index} field criteria must be a non-empty array")
    criteria: list[EvaluationCriterion] = []
    seen: set[str] = set()
    for criterion_index, value in enumerate(values):
        if not isinstance(value, dict) or set(value) != {"id", "description"}:
            raise ValueError(
                f"case {index} criterion {criterion_index} must contain id and description"
            )
        identifier = value.get("id")
        description = value.get("description")
        if not isinstance(identifier, str) or not CRITERION_ID_PATTERN.fullmatch(
            identifier
        ):
            raise ValueError(
                f"case {index} criterion {criterion_index} has an invalid id"
            )
        if identifier in seen:
            raise ValueError(f"case {index} has duplicate criterion id: {identifier}")
        if not isinstance(description, str) or not description.strip():
            raise ValueError(
                f"case {index} criterion {criterion_index} has no description"
            )
        seen.add(identifier)
        criteria.append(EvaluationCriterion(identifier, description.strip()))
    return tuple(criteria)


def _load_case_document(document: dict[str, Any]) -> list[EvaluationCase]:
    if document.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"unsupported case schema {document.get('schema_version')!r}; "
            f"expected {SCHEMA_VERSION}"
        )
    raw_cases = document.get("cases")
    if not isinstance(raw_cases, list) or not raw_cases:
        raise ValueError("cases must be a non-empty array")

    cases: list[EvaluationCase] = []
    seen: set[str] = set()
    for index, raw in enumerate(raw_cases):
        if not isinstance(raw, dict):
            raise ValueError(f"case {index} must be an object")
        required = {
            "id",
            "title",
            "category",
            "prompt",
            "reference_files",
            "criteria",
            "sandbox",
            "hardware_requirement",
            "cleanup",
        }
        missing = sorted(key for key in required if key not in raw)
        if missing:
            raise ValueError(f"case {index} is missing: {', '.join(missing)}")
        unknown = sorted(set(raw) - required)
        if unknown:
            raise ValueError(f"case {index} has unknown fields: {', '.join(unknown)}")

        identifier = _require_text(raw, "id", index)
        if identifier in seen:
            raise ValueError(f"duplicate case id: {identifier}")
        seen.add(identifier)

        sandbox = _require_text(raw, "sandbox", index)
        if sandbox != "read-only":
            raise ValueError(f"case {identifier} has invalid sandbox: {sandbox}")
        hardware_requirement = _require_text(raw, "hardware_requirement", index)
        if hardware_requirement not in HARDWARE_REQUIREMENTS:
            raise ValueError(
                f"case {identifier} has invalid hardware requirement: "
                f"{hardware_requirement}"
            )

        case = EvaluationCase(
            identifier=identifier,
            title=_require_text(raw, "title", index),
            category=_require_text(raw, "category", index),
            prompt=_require_text(raw, "prompt", index),
            reference_files=_require_text_list(raw, "reference_files", index),
            criteria=_load_criteria(raw, index),
            sandbox=sandbox,
            hardware_requirement=hardware_requirement,
            cleanup=_require_text(raw, "cleanup", index),
        )
        if len(case.reference_files) != len(set(case.reference_files)):
            raise ValueError(f"case {identifier} has duplicate reference files")
        for relative in case.reference_files:
            if not _safe_repository_path(relative):
                raise ValueError(
                    f"case {identifier} has unsafe reference file path: {relative}"
                )
        cases.append(case)
    return cases


def load_cases(path: Path = DEFAULT_CASES) -> list[EvaluationCase]:
    return _load_case_document(_read_json(path))


def _safe_repository_path(relative: str) -> bool:
    path = Path(relative)
    return bool(
        relative
        and "\\" not in relative
        and not path.is_absolute()
        and ".." not in path.parts
        and path.parts
        and ".git" not in path.parts
    )


def _path_exists_in_checkout(checkout: Path, relative: str) -> bool:
    if not _safe_repository_path(relative):
        return False
    root = checkout.resolve()
    candidate = (checkout / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError:
        return False
    return candidate.exists()


def assess_response_coverage(
    case: EvaluationCase, response: dict[str, Any], checkout: Path
) -> dict[str, Any]:
    """Check evidence coverage without claiming that the response is correct."""
    validate_response_document(response)
    reported_files = set(response["files"])
    evidence_by_id = {
        entry["criterion_id"]: entry for entry in response["criterion_evidence"]
    }
    expected_ids = {criterion.identifier for criterion in case.criteria}

    contract_checks: list[dict[str, Any]] = []
    for relative in response["files"]:
        contract_checks.append(
            {
                "kind": "reported_path",
                "value": relative,
                "satisfied": _path_exists_in_checkout(checkout, relative),
            }
        )
    for relative in case.reference_files:
        contract_checks.append(
            {
                "kind": "reference_file_coverage",
                "value": relative,
                "satisfied": relative in reported_files
                and _path_exists_in_checkout(checkout, relative),
            }
        )

    criterion_results: list[dict[str, Any]] = []
    for criterion in case.criteria:
        entry = evidence_by_id.get(criterion.identifier)
        reasons: list[str] = []
        if entry is None:
            reasons.append("missing criterion evidence")
            evidence: list[str] = []
            files: list[str] = []
        else:
            evidence = entry["evidence"]
            files = entry["files"]
            for relative in files:
                if relative not in reported_files:
                    reasons.append(f"evidence path is absent from files: {relative}")
                if not _path_exists_in_checkout(checkout, relative):
                    reasons.append(f"evidence path is not in the checkout: {relative}")
        criterion_results.append(
            {
                "id": criterion.identifier,
                "description": criterion.description,
                "covered": not reasons,
                "evidence": evidence,
                "files": files,
                "coverage_issues": reasons,
            }
        )

    unexpected_ids = sorted(set(evidence_by_id) - expected_ids)
    contract_checks.append(
        {
            "kind": "criterion_id_set",
            "value": sorted(expected_ids),
            "actual": sorted(evidence_by_id),
            "satisfied": not unexpected_ids
            and len(evidence_by_id) == len(expected_ids),
        }
    )
    contract_checks.append(
        {
            "kind": "hardware_requirement",
            "value": case.hardware_requirement,
            "actual": response["hardware_requirement"],
            "satisfied": response["hardware_requirement"] == case.hardware_requirement,
        }
    )

    covered = sum(1 for result in criterion_results if result["covered"])
    complete = covered == len(criterion_results)
    review_ready = complete and all(check["satisfied"] for check in contract_checks)
    return {
        "review_required": True,
        "review_ready": review_ready,
        "coverage": {
            "complete": complete,
            "covered": covered,
            "possible": len(criterion_results),
            "criteria": criterion_results,
        },
        "contract_checks": contract_checks,
    }


def _assert_offline_git_operation(args: tuple[str, ...]) -> None:
    if not args:
        raise ValueError("Git operation is missing")
    network_tokens = {"clone", "fetch", "pull", "push", "ls-remote"}
    has_network_token = any(token in network_tokens for token in args)
    has_submodule_update = any(
        args[index : index + 2] == ("submodule", "update")
        for index in range(len(args) - 1)
    )
    has_remote_update = any(
        args[index : index + 2] == ("remote", "update")
        for index in range(len(args) - 1)
    )
    if has_network_token or has_submodule_update or has_remote_update:
        raise ValueError(
            f"network-capable Git operation is prohibited: {' '.join(args)}"
        )


def _git(
    *args: str,
    cwd: Path = ROOT,
    environment: dict[str, str] | None = None,
) -> str:
    _assert_offline_git_operation(args)
    command = ["git", *args]
    try:
        process = subprocess.run(
            command,
            cwd=cwd,
            check=True,
            capture_output=True,
            text=True,
            env=environment,
            timeout=120,
        )
    except subprocess.TimeoutExpired as error:
        raise ValueError(f"git command timed out: {' '.join(command)}") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip() or error.stdout.strip() or str(error)
        raise ValueError(
            f"git command failed: {' '.join(command)}: {detail}"
        ) from error
    return process.stdout.strip()


def _resolve_commit(revision: str, cwd: Path = ROOT) -> str:
    return _git(
        "rev-parse",
        "--verify",
        "--end-of-options",
        f"{revision}^{{commit}}",
        cwd=cwd,
        environment=_local_git_environment(),
    )


def _resolve_runtime(source: dict[str, str] | None = None) -> EvaluationRuntime:
    source = os.environ if source is None else source
    search_path = source.get("PATH", "/usr/local/bin:/usr/bin:/bin")
    codex = shutil.which("codex", path=search_path)
    bubblewrap = shutil.which("bwrap", path=search_path)
    if sys.platform != "linux" or os.name != "posix":
        raise ValueError("agent evaluation requires Linux PID namespaces")
    if codex is None:
        raise ValueError("codex executable is not available")
    if bubblewrap is None:
        raise ValueError("bubblewrap is required for agent evaluation containment")

    codex_executable = Path(codex).resolve()
    if codex_executable.name == "codex.js" and codex_executable.parent.name == "bin":
        codex_read_root = codex_executable.parent.parent
    else:
        codex_read_root = codex_executable.parent

    shell_directories = [Path("/usr/local/bin"), Path("/usr/bin"), Path("/bin")]
    rg = shutil.which("rg", path=search_path)
    if rg is not None:
        rg_directory = Path(rg).resolve().parent
        if rg_directory == codex_read_root or codex_read_root in rg_directory.parents:
            shell_directories.insert(0, rg_directory)
    shell_path = os.pathsep.join(
        str(path) for path in shell_directories if path.is_dir()
    )
    return EvaluationRuntime(
        codex_executable=codex_executable,
        codex_read_root=codex_read_root,
        bubblewrap_executable=Path(bubblewrap).resolve(),
        shell_path=shell_path,
    )


def _prepare_codex_home(
    destination: Path, source: dict[str, str] | None = None
) -> Path:
    source = os.environ if source is None else source
    source_home = Path(source.get("HOME", str(Path.home())))
    source_codex_home = Path(source.get("CODEX_HOME", source_home / ".codex"))
    source_auth = source_codex_home / "auth.json"
    if source_auth.is_symlink() or not source_auth.is_file():
        raise ValueError(
            "agent evaluation requires file-backed Codex authentication at "
            f"{source_auth}"
        )
    destination.mkdir(mode=0o700)
    destination.chmod(0o700)
    isolated_auth = destination / "auth.json"
    try:
        shutil.copyfile(source_auth, isolated_auth)
        isolated_auth.chmod(0o600)
    except OSError as error:
        raise ValueError(f"cannot isolate Codex authentication: {error}") from error
    return source_auth


def _codex_environment(
    isolated_home: Path,
    temporary: Path,
    codex_home: Path,
    source: dict[str, str] | None = None,
    executable_path: str = "/usr/local/bin:/usr/bin:/bin",
) -> dict[str, str]:
    source = os.environ if source is None else source
    environment = {
        key: value for key, value in source.items() if key in CHILD_ENV_ALLOWLIST
    }
    for key in SENSITIVE_ENV_VARS:
        environment.pop(key, None)
    environment.update(
        {
            "CODEX_HOME": str(codex_home),
            "GIT_TERMINAL_PROMPT": "0",
            "HOME": str(isolated_home),
            "NO_COLOR": "1",
            "PATH": executable_path,
            "TEMP": str(temporary),
            "TMP": str(temporary),
            "TMPDIR": str(temporary),
        }
    )
    return environment


def _toml_string(value: str | Path) -> str:
    return json.dumps(str(value), ensure_ascii=True)


def _permission_profile_config(
    case: EvaluationCase,
    checkout: Path,
    shell_home: Path,
    command_tmp: Path,
    runtime: EvaluationRuntime,
) -> str:
    if case.sandbox != "read-only":
        raise ValueError("agent evaluation supports read-only cases only")
    entries = [
        (":minimal", "read"),
        (str(runtime.codex_read_root), "read"),
        (str(checkout), "read"),
        (str(shell_home), "write"),
        (str(command_tmp), "write"),
    ]
    filesystem = ",".join(
        f"{_toml_string(path)}={_toml_string(access)}" for path, access in entries
    )
    return f"{{filesystem={{{filesystem}}},network={{enabled=false}}}}"


def _shell_environment_config(
    shell_home: Path, command_tmp: Path, shell_path: str
) -> str:
    values = {
        "GIT_TERMINAL_PROMPT": "0",
        "HOME": str(shell_home),
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
        "NO_COLOR": "1",
        "PATH": shell_path,
        "TEMP": str(command_tmp),
        "TMP": str(command_tmp),
        "TMPDIR": str(command_tmp),
        "TZ": "UTC",
    }
    assignments = ",".join(
        f"{key}={_toml_string(value)}" for key, value in values.items()
    )
    return f'{{inherit="none",set={{{assignments}}}}}'


def _containment_command(
    command: list[str],
    runtime: EvaluationRuntime,
    writable_roots: tuple[Path, ...],
) -> list[str]:
    wrapped = [
        str(runtime.bubblewrap_executable),
        "--ro-bind",
        "/",
        "/",
        "--dev-bind",
        "/dev",
        "/dev",
    ]
    for root in dict.fromkeys(path.resolve() for path in writable_roots):
        if not root.is_dir():
            raise ValueError(f"containment writable root does not exist: {root}")
        wrapped.extend(("--bind", str(root), str(root)))
    wrapped.extend(
        (
            "--unshare-user",
            "--unshare-pid",
            "--proc",
            "/proc",
            "--die-with-parent",
            "--new-session",
            "--",
            *command,
        )
    )
    return wrapped


def evaluation_exit_code(summary: dict[str, int]) -> int:
    incomplete = summary["completed"] - summary["review_ready"]
    return 1 if summary["errors"] or incomplete else 0


def _usage_from_events(output: str) -> dict[str, int]:
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
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        visit(event)
    return latest


def _case_prompt(case: EvaluationCase) -> str:
    criteria = "\n".join(
        f"- {criterion.identifier}: {criterion.description}"
        for criterion in case.criteria
    )
    return f"""DOMES agent evaluation: {case.title}

Work only inside the checked-out repository. This evaluation is {case.sandbox}.
Do not modify GitHub, external services, host configuration, or physical devices.
No hardware is executed by this harness. Do not claim hardware behavior was
verified. Inspect the repository and return the requested structured assessment.

Task:
{case.prompt}

Cleanup contract:
{case.cleanup}

Evidence criteria:
{criteria}

In the final structured response, list every existing authoritative or affected
repository path in `files`, the durable contracts in `invariants`, the checks that would be
required in `verification`, and whether physical hardware is required for full
validation in `hardware_requirement` (`required` or `not_required`). This field
describes a requirement, not execution status; hardware is never run here.
Return exactly one `criterion_evidence` entry for each evidence criterion. Each
entry must identify its criterion id, concise repository evidence, and the paths
that support it. Every reported path must already exist in the sanitized checkout
and use an exact repository-relative path without a line-number suffix. Mention a
prospective file only in prose, not in `files` or criterion evidence. Keep `claims`
limited to conclusions supported by that evidence.
Automated checks measure evidence coverage only; an independent LLM semantic audit decides
correctness.
"""


def _codex_command(
    case: EvaluationCase,
    model: str,
    effort: str,
    checkout: Path,
    response_path: Path,
    shell_home: Path,
    command_tmp: Path,
    codex_home: Path,
    runtime: EvaluationRuntime,
    response_schema: Path = DEFAULT_RESPONSE_SCHEMA,
) -> list[str]:
    permission_profile = _permission_profile_config(
        case, checkout, shell_home, command_tmp, runtime
    )
    shell_environment = _shell_environment_config(
        shell_home, command_tmp, runtime.shell_path
    )
    command = [
        str(runtime.codex_executable),
        "exec",
        "--ephemeral",
        "--ignore-user-config",
        "--ignore-rules",
        "--strict-config",
        "--disable",
        "browser_use",
        "--disable",
        "computer_use",
        "--disable",
        "enable_mcp_apps",
        "--disable",
        "goals",
        "--disable",
        "multi_agent",
        "--disable",
        "plugins",
        "--json",
        "--color",
        "never",
        "--model",
        model,
        "--config",
        f'model_reasoning_effort="{effort}"',
        "--config",
        'approval_policy="never"',
        "--config",
        "memories.generate_memories=false",
        "--config",
        "memories.use_memories=false",
        "--config",
        'default_permissions="agent-eval"',
        "--config",
        f"permissions.agent-eval={permission_profile}",
        "--config",
        f"shell_environment_policy={shell_environment}",
        "--output-schema",
        str(response_schema),
        "--output-last-message",
        str(response_path),
        "--cd",
        str(checkout),
        _case_prompt(case),
    ]
    return _containment_command(
        command,
        runtime,
        writable_roots=(checkout.parent, codex_home),
    )


def _containment_preflight(
    runtime: EvaluationRuntime,
    codex_home: Path,
    source_auth: Path,
    response_schema: Path,
) -> dict[str, Any]:
    started = time.monotonic()
    script = """
import pathlib
import sys

checkout = pathlib.Path(sys.argv[1])
checkout.joinpath("visible.txt").read_bytes()
for raw in sys.argv[2:7]:
    try:
        pathlib.Path(raw).read_bytes()
    except OSError:
        pass
    else:
        raise SystemExit(f"unexpected readable path: {raw}")
for raw in sys.argv[7:9]:
    probe = pathlib.Path(raw) / "preflight-write"
    probe.write_text("ok", encoding="utf-8")
    probe.unlink()
checkout_probe = checkout / "preflight-write"
try:
    checkout_probe.write_text("unexpected", encoding="utf-8")
except OSError:
    pass
else:
    checkout_probe.unlink(missing_ok=True)
    raise SystemExit("read-only checkout was writable")
""".strip()

    with tempfile.TemporaryDirectory(prefix="domes-agent-eval-preflight-") as raw:
        temporary = Path(raw)
        checkout = temporary / "checkout"
        shell_home = temporary / "shell-home"
        command_tmp = temporary / "command-tmp"
        private = temporary / "private"
        for directory in (checkout, shell_home, command_tmp, private):
            directory.mkdir(mode=0o700)
        (checkout / "visible.txt").write_text("visible\n", encoding="utf-8")
        private_probe = private / "probe.txt"
        private_probe.write_text("private\n", encoding="utf-8")
        original_evaluator = ROOT / EVALUATOR_RELATIVE_PATH / "cases.json"
        isolated_auth = codex_home / "auth.json"

        case = EvaluationCase(
            identifier="preflight-read-only",
            title="Containment preflight",
            category="preflight",
            prompt="Containment preflight.",
            reference_files=(),
            criteria=(EvaluationCriterion("containment", "Containment."),),
            sandbox="read-only",
            hardware_requirement="not_required",
            cleanup="Remove the temporary checkout.",
        )
        profile = _permission_profile_config(
            case, checkout, shell_home, command_tmp, runtime
        )
        shell_environment = _shell_environment_config(
            shell_home, command_tmp, runtime.shell_path
        )
        command = [
            str(runtime.codex_executable),
            "sandbox",
            "--permission-profile",
            "agent-eval",
            "--cd",
            str(checkout),
            "--config",
            f"permissions.agent-eval={profile}",
            "--config",
            f"shell_environment_policy={shell_environment}",
            "--",
            sys.executable,
            "-c",
            script,
            str(checkout),
            str(private_probe),
            str(original_evaluator),
            str(source_auth),
            str(isolated_auth),
            str(response_schema),
            str(shell_home),
            str(command_tmp),
        ]
        wrapped = _containment_command(
            command,
            runtime,
            writable_roots=(temporary, codex_home),
        )
        process = _run_codex_process(
            wrapped,
            timeout_seconds=20,
            environment=_codex_environment(
                shell_home,
                command_tmp,
                codex_home,
                executable_path=runtime.shell_path,
            ),
        )
        if process.returncode != 0:
            detail = (process.stdout + process.stderr)[-4000:]
            raise ValueError(
                f"read-only containment preflight failed: {detail.strip()}"
            )

        exec_case = EvaluationCase(
            identifier="preflight-exec-config",
            title="Exec configuration preflight",
            category="preflight",
            prompt="Configuration preflight.",
            reference_files=(),
            criteria=(EvaluationCriterion("configuration", "Configuration."),),
            sandbox="read-only",
            hardware_requirement="not_required",
            cleanup="Remove the temporary checkout.",
        )
        exec_command = _codex_command(
            exec_case,
            "gpt-5.6-sol",
            "medium",
            checkout,
            private / "response.json",
            shell_home,
            command_tmp,
            codex_home,
            runtime,
            response_schema,
        )
        # Removing the final prompt makes `codex exec` load and validate every
        # real option, feature, profile, and schema without contacting a model.
        exec_command.pop()
        process = _run_codex_process(
            exec_command,
            timeout_seconds=20,
            environment=_codex_environment(
                shell_home,
                command_tmp,
                codex_home,
                executable_path=runtime.shell_path,
            ),
        )
        detail = process.stdout + process.stderr
        if process.returncode == 0 or "No prompt provided via stdin" not in detail:
            raise ValueError(
                "codex exec configuration preflight failed: " + detail[-4000:].strip()
            )

    return {
        "passed": True,
        "modes": ["read-only"],
        "exec_configuration_validated": True,
        "duration_seconds": round(time.monotonic() - started, 3),
    }


def _signal_process_group(process: subprocess.Popen[str], sig: signal.Signals) -> None:
    try:
        if os.name == "posix":
            os.killpg(process.pid, sig)
        elif sig == signal.SIGTERM:
            process.terminate()
        else:
            process.kill()
    except OSError:
        pass


def _close_process_pipes(process: subprocess.Popen[str]) -> None:
    for stream in (process.stdout, process.stderr):
        if stream is not None:
            try:
                stream.close()
            except OSError:
                pass


def _terminate_process_group(process: subprocess.Popen[str]) -> None:
    # Signal the group even when its leader has already exited. Descendants may
    # still own the captured pipes, and an unbounded communicate() would hang.
    _signal_process_group(process, signal.SIGTERM)

    try:
        process.communicate(timeout=2)
    except subprocess.TimeoutExpired:
        pass
    # Always escalate. A descendant can close the captured pipes, ignore TERM,
    # and outlive a leader that has already exited successfully.
    _signal_process_group(process, signal.SIGKILL)
    try:
        process.communicate(timeout=2)
    except subprocess.TimeoutExpired:
        _close_process_pipes(process)
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            pass


def _run_codex_process(
    command: list[str], timeout_seconds: int, environment: dict[str, str]
) -> subprocess.CompletedProcess[str]:
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        stdin=subprocess.DEVNULL,
        env=environment,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        _terminate_process_group(process)
        raise
    except BaseException:
        _terminate_process_group(process)
        raise
    # Codex is not expected to leave background work running after it exits.
    # Kill any process-group descendants that closed the inherited pipes first.
    _signal_process_group(process, signal.SIGKILL)
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)


def _local_git_environment() -> dict[str, str]:
    return {
        "GIT_ALLOW_PROTOCOL": "",
        "GIT_CONFIG_GLOBAL": os.devnull,
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_NO_LAZY_FETCH": "1",
        "GIT_NO_REPLACE_OBJECTS": "1",
        "GIT_TERMINAL_PROMPT": "0",
        "LC_ALL": "C.UTF-8",
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
    }


def _git_tree_entries(
    repository: Path, revision: str
) -> tuple[tuple[str, str, str, str], ...]:
    output = _git(
        "ls-tree",
        "-r",
        "-z",
        revision,
        cwd=repository,
        environment=_local_git_environment(),
    )
    entries: list[tuple[str, str, str, str]] = []
    for raw in output.split("\0"):
        if not raw:
            continue
        try:
            metadata, relative = raw.split("\t", 1)
            mode, object_type, object_id = metadata.split(" ", 2)
        except ValueError as error:
            raise ValueError(f"cannot parse Git tree entry: {raw!r}") from error
        if (
            len(object_id) != 40
            or not re.fullmatch(r"[0-9a-f]{40}", object_id)
            or not _safe_repository_path(relative)
        ):
            raise ValueError(f"unsafe or invalid Git tree entry: {raw!r}")
        entries.append((mode, object_type, object_id, relative))
    return tuple(entries)


def _initialized_submodule_repository(root: Path, relative: str) -> Path:
    candidate = root / relative
    root_resolved = root.resolve()
    cursor = root
    for part in Path(relative).parts:
        cursor /= part
        if cursor.is_symlink():
            raise ValueError(f"submodule worktree is not initialized: {relative}")
    try:
        candidate_resolved = candidate.resolve(strict=True)
        candidate_resolved.relative_to(root_resolved)
    except (OSError, ValueError) as error:
        raise ValueError(
            f"submodule worktree is not initialized: {relative}"
        ) from error
    if candidate.is_symlink() or not candidate_resolved.is_dir():
        raise ValueError(f"submodule worktree is not initialized: {relative}")
    try:
        top_level = Path(
            _git(
                "rev-parse",
                "--show-toplevel",
                cwd=candidate_resolved,
                environment=_local_git_environment(),
            )
        ).resolve(strict=True)
    except (OSError, ValueError) as error:
        raise ValueError(
            f"submodule worktree is not initialized: {relative}"
        ) from error
    if top_level != candidate_resolved:
        raise ValueError(f"submodule worktree is not initialized: {relative}")
    return candidate_resolved


def _require_local_commit(repository: Path, relative: str, revision: str) -> None:
    try:
        resolved = _git(
            "rev-parse",
            "--verify",
            "--end-of-options",
            f"{revision}^{{commit}}",
            cwd=repository,
            environment=_local_git_environment(),
        )
    except ValueError as error:
        raise ValueError(
            f"recorded submodule commit is unavailable locally: "
            f"{relative} at {revision}"
        ) from error
    if resolved != revision:
        raise ValueError(
            f"recorded submodule object is not the expected commit: "
            f"{relative} at {revision}"
        )


def _collect_submodule_plan(
    revision: str, root: Path = ROOT
) -> tuple[LocalSubmodule, ...]:
    root = root.resolve(strict=True)
    plan: list[LocalSubmodule] = []
    seen_paths: set[str] = set()

    def visit(repository: Path, commit: str, prefix: str) -> None:
        entries = _git_tree_entries(repository, commit)
        if prefix:
            _read_git_blobs(
                repository,
                [object_id for mode, _, object_id, _ in entries if mode != "160000"],
            )
        for mode, object_type, object_id, relative in entries:
            if mode != "160000" and object_type != "commit":
                continue
            if mode != "160000" or object_type != "commit":
                raise ValueError(f"invalid submodule tree entry: {prefix + relative}")
            full_path = f"{prefix}{relative}"
            if full_path in seen_paths:
                raise ValueError(f"duplicate submodule path: {full_path}")
            local_repository = _initialized_submodule_repository(root, full_path)
            _require_local_commit(local_repository, full_path, object_id)
            seen_paths.add(full_path)
            plan.append(LocalSubmodule(full_path, object_id, local_repository))
            visit(local_repository, object_id, f"{full_path}/")

    visit(root, revision, "")
    return tuple(plan)


def _read_git_blobs(repository: Path, object_ids: list[str]) -> dict[str, bytes]:
    unique_ids = list(dict.fromkeys(object_ids))
    if not unique_ids:
        return {}
    request = "".join(f"{object_id}\n" for object_id in unique_ids).encode("ascii")
    command = ["git", "cat-file", "--batch"]
    try:
        process = subprocess.run(
            command,
            cwd=repository,
            check=True,
            capture_output=True,
            input=request,
            env=_local_git_environment(),
            timeout=120,
        )
    except subprocess.TimeoutExpired as error:
        raise ValueError("git cat-file --batch timed out") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.decode("utf-8", errors="replace").strip() or str(error)
        raise ValueError(f"git cat-file --batch failed: {detail}") from error
    except OSError as error:
        raise ValueError(f"cannot run git cat-file --batch: {error}") from error

    stream = io.BytesIO(process.stdout)
    blobs: dict[str, bytes] = {}
    for expected in unique_ids:
        header = stream.readline().rstrip(b"\n").split()
        if len(header) != 3:
            raise ValueError(f"Git object is unavailable locally: {expected}")
        returned, object_type, raw_size = header
        try:
            size = int(raw_size)
        except ValueError as error:
            raise ValueError(f"invalid Git object size for {expected}") from error
        if returned.decode("ascii") != expected or object_type != b"blob":
            raise ValueError(f"unexpected Git object for {expected}")
        data = stream.read(size)
        if len(data) != size or stream.read(1) != b"\n":
            raise ValueError(f"truncated Git object for {expected}")
        blobs[expected] = data
    if stream.read():
        raise ValueError("unexpected trailing data from git cat-file --batch")
    return blobs


def _materialize_git_tree(repository: Path, revision: str, destination: Path) -> None:
    if destination.exists():
        if destination.is_symlink() or not destination.is_dir():
            raise ValueError(f"Git tree destination is not a directory: {destination}")
        if any(destination.iterdir()):
            raise ValueError(f"Git tree destination is not empty: {destination}")
    else:
        destination.mkdir(parents=True)
    destination.chmod(0o755)
    destination_root = destination.resolve(strict=True)
    entries = _git_tree_entries(repository, revision)
    blobs = _read_git_blobs(
        repository,
        [object_id for mode, _, object_id, _ in entries if mode != "160000"],
    )

    for mode, object_type, object_id, relative in entries:
        target = destination / relative
        parent = destination
        for part in Path(relative).parts[:-1]:
            parent /= part
            if os.path.lexists(parent):
                if parent.is_symlink() or not parent.is_dir():
                    raise ValueError(f"unsafe Git tree destination: {relative}")
            else:
                parent.mkdir(mode=0o755)
        try:
            target.parent.resolve(strict=True).relative_to(destination_root)
        except (OSError, ValueError) as error:
            raise ValueError(f"unsafe Git tree destination: {relative}") from error
        if os.path.lexists(target):
            raise ValueError(f"duplicate Git tree destination: {relative}")
        if mode == "160000" and object_type == "commit":
            target.mkdir(parents=True)
            continue
        if object_type != "blob" or mode not in {"100644", "100755", "120000"}:
            raise ValueError(
                f"unsupported Git tree entry: {mode} {object_type} {relative}"
            )
        target.parent.mkdir(parents=True, exist_ok=True)
        data = blobs[object_id]
        if mode == "120000":
            os.symlink(os.fsdecode(data), target)
        else:
            target.write_bytes(data)
            target.chmod(0o755 if mode == "100755" else 0o644)


def _copy_sanitized_checkout(source: Path, destination: Path) -> None:
    source_root = source.resolve()

    def ignored(directory: str, names: list[str]) -> set[str]:
        relative = Path(directory).resolve().relative_to(source_root)
        result = {".git"} if ".git" in names else set()
        if relative == Path("tools") and "agent_eval" in names:
            result.add("agent_eval")
        return result

    shutil.copytree(source, destination, symlinks=True, ignore=ignored)


def _initialize_snapshot_repository(checkout: Path, isolated_home: Path) -> str:
    environment = {
        **_local_git_environment(),
        "GIT_AUTHOR_DATE": "2000-01-01T00:00:00+00:00",
        "GIT_COMMITTER_DATE": "2000-01-01T00:00:00+00:00",
        "HOME": str(isolated_home),
        "TZ": "UTC",
    }
    _git("init", "--quiet", cwd=checkout, environment=environment)
    # Flattened submodules can contain tracked files matching a parent or local
    # ignore rule. Force-add every copied path so the sanitized commit is exact.
    _git("add", "--all", "--force", cwd=checkout, environment=environment)
    _git(
        "-c",
        "user.name=DOMES Agent Evaluation",
        "-c",
        "user.email=agent-eval@invalid",
        "-c",
        "commit.gpgSign=false",
        "commit",
        "--quiet",
        "--no-verify",
        "-m",
        "Sanitized evaluation snapshot",
        cwd=checkout,
        environment=environment,
    )
    status = _git(
        "status",
        "--short",
        "--ignored=matching",
        "--untracked-files=all",
        cwd=checkout,
        environment=environment,
    )
    if status:
        raise ValueError(f"sanitized checkout is not clean: {status}")
    return _resolve_commit("HEAD", cwd=checkout)


def _checkout_manifest(checkout: Path) -> dict[str, str]:
    manifest: dict[str, str] = {}
    for path in sorted(checkout.rglob("*")):
        relative_path = path.relative_to(checkout)
        if relative_path.parts and relative_path.parts[0] == ".git":
            continue
        relative = relative_path.as_posix()
        metadata = path.lstat()
        mode = metadata.st_mode & 0o7777
        if path.is_symlink():
            value = f"symlink:{mode:o}:{os.readlink(path)}"
        elif path.is_dir():
            value = f"directory:{mode:o}"
        elif path.is_file():
            digest = hashlib.sha256()
            with path.open("rb") as handle:
                for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                    digest.update(chunk)
            value = f"file:{mode:o}:{metadata.st_size}:{digest.hexdigest()}"
        else:
            value = f"other:{mode:o}:{metadata.st_size}"
        manifest[relative] = value
    return manifest


def _manifest_digest(manifest: dict[str, str]) -> str:
    encoded = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def _checkout_changes(checkout: Path, baseline_manifest: dict[str, str]) -> list[str]:
    current = _checkout_manifest(checkout)
    changes: list[str] = []
    for relative in sorted(set(baseline_manifest) | set(current)):
        before = baseline_manifest.get(relative)
        after = current.get(relative)
        if before == after:
            continue
        kind = (
            "created" if before is None else "deleted" if after is None else "modified"
        )
        changes.append(f"{kind}: {relative}")
    status = _git(
        "status",
        "--short",
        "--ignored=matching",
        "--untracked-files=all",
        cwd=checkout,
        environment=_local_git_environment(),
    )
    changes.extend(f"git-status: {line}" for line in status.splitlines())
    return sorted(set(changes))


def _prepare_checkout(
    revision: str, temporary: Path, repository: Path = ROOT
) -> PreparedCheckout:
    source = temporary / "source"
    checkout = temporary / "checkout"
    isolated_home = temporary / "home"
    isolated_home.mkdir(mode=0o700)
    repository = repository.resolve(strict=True)
    submodule_plan = _collect_submodule_plan(revision, repository)
    _materialize_git_tree(repository, revision, source)
    for submodule in submodule_plan:
        _materialize_git_tree(
            submodule.repository,
            submodule.revision,
            source / submodule.path,
        )
    submodules = tuple(
        {"path": submodule.path, "revision": submodule.revision}
        for submodule in submodule_plan
    )
    _copy_sanitized_checkout(source, checkout)
    if (checkout / EVALUATOR_RELATIVE_PATH).exists():
        raise ValueError("sanitized checkout still contains evaluator material")

    snapshot_revision = _initialize_snapshot_repository(checkout, isolated_home)
    baseline_manifest = _checkout_manifest(checkout)
    return PreparedCheckout(
        path=checkout,
        snapshot_revision=snapshot_revision,
        submodules=submodules,
        baseline_manifest=baseline_manifest,
    )


def _cleanup_case_directory(temporary: Path) -> list[str]:
    try:
        shutil.rmtree(temporary)
    except FileNotFoundError:
        return []
    except OSError as error:
        return [f"temporary directory removal failed: {error}"]
    return []


def _attach_checkout_audit(result: dict[str, Any], prepared: PreparedCheckout) -> None:
    result["checkout_snapshot"] = {
        "revision": prepared.snapshot_revision,
        "manifest_sha256": _manifest_digest(prepared.baseline_manifest),
        "submodules": list(prepared.submodules),
        "evaluator_material_present": False,
    }
    try:
        changed = _checkout_changes(prepared.path, prepared.baseline_manifest)
    except (OSError, TypeError, ValueError) as error:
        message = f"checkout write audit failed: {error}"
        result["checkout_audit_error"] = message
        if result.get("status") != "error":
            result["original_status"] = result.get("status")
        if "error" in result:
            result["original_error"] = result["error"]
            result["error"] = f"{result['error']}; {message}"
        else:
            result["error"] = message
        result["status"] = "error"
        result["review_ready"] = False
        return

    result["checkout_changes"] = changed
    checkout_clean = not changed
    if result.get("status") == "completed":
        result["contract_checks"].append(
            {
                "kind": "read_only_checkout",
                "value": "no checkout writes",
                "satisfied": checkout_clean,
            }
        )
        if not checkout_clean:
            result["review_ready"] = False
    elif not checkout_clean:
        result["read_only_violation"] = True


def _run_case(
    case: EvaluationCase,
    model: str,
    effort: str,
    revision: str,
    timeout_seconds: int,
    response_schema: Path,
    codex_home: Path,
    runtime: EvaluationRuntime,
) -> dict[str, Any]:
    started = time.monotonic()
    temporary = Path(tempfile.mkdtemp(prefix="domes-agent-eval-"))
    response_path = temporary / "response.json"
    command_tmp = temporary / "tmp"
    shell_home = temporary / "shell-home"
    prepared: PreparedCheckout | None = None
    result: dict[str, Any] = {
        "id": case.identifier,
        "title": case.title,
        "status": "error",
        "review_required": True,
        "review_ready": False,
        "error": "case execution ended before producing a result",
        "cleanup_contract": case.cleanup,
    }
    try:
        command_tmp.mkdir(mode=0o700)
        shell_home.mkdir(mode=0o700)
        prepared = _prepare_checkout(revision, temporary)
        command = _codex_command(
            case,
            model,
            effort,
            prepared.path,
            response_path,
            shell_home,
            command_tmp,
            codex_home,
            runtime,
            response_schema,
        )
        environment = _codex_environment(
            shell_home,
            command_tmp,
            codex_home,
            executable_path=runtime.shell_path,
        )
        process = _run_codex_process(command, timeout_seconds, environment)
        duration = round(time.monotonic() - started, 3)
        if process.returncode != 0 or not response_path.exists():
            result = {
                "id": case.identifier,
                "title": case.title,
                "status": "error",
                "review_required": True,
                "review_ready": False,
                "duration_seconds": duration,
                "exit_code": process.returncode,
                "error": (process.stdout + process.stderr)[-4000:],
                "usage": _usage_from_events(process.stdout),
                "cleanup_contract": case.cleanup,
            }
        else:
            response = _read_json(response_path)
            assessment = assess_response_coverage(case, response, prepared.path)
            digest = hashlib.sha256(response_path.read_bytes()).hexdigest()
            result = {
                "id": case.identifier,
                "title": case.title,
                "category": case.category,
                "status": "completed",
                "duration_seconds": duration,
                "usage": _usage_from_events(process.stdout),
                "response_sha256": digest,
                "response": response,
                "summary": response["summary"],
                "files": response["files"],
                "hardware_requirement": response["hardware_requirement"],
                "cleanup_contract": case.cleanup,
                **assessment,
            }
    except subprocess.TimeoutExpired:
        result = {
            "id": case.identifier,
            "title": case.title,
            "status": "error",
            "review_required": True,
            "review_ready": False,
            "duration_seconds": round(time.monotonic() - started, 3),
            "error": f"timed out after {timeout_seconds} seconds",
            "cleanup_contract": case.cleanup,
        }
    except (
        KeyError,
        OSError,
        subprocess.CalledProcessError,
        TypeError,
        ValueError,
    ) as error:
        result = {
            "id": case.identifier,
            "title": case.title,
            "status": "error",
            "review_required": True,
            "review_ready": False,
            "duration_seconds": round(time.monotonic() - started, 3),
            "error": str(error),
            "cleanup_contract": case.cleanup,
        }
    finally:
        if prepared is not None:
            _attach_checkout_audit(result, prepared)
        cleanup_errors = _cleanup_case_directory(temporary)

    if cleanup_errors:
        result["cleanup_errors"] = cleanup_errors
        result["original_status"] = result["status"]
        if "error" in result:
            result["original_error"] = result["error"]
        result["status"] = "error"
        result["review_ready"] = False
        result["error"] = "; ".join(cleanup_errors)
    return result


def _command_version(command: list[str]) -> str | None:
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


def _executable_metadata(path: Path, version_args: list[str]) -> dict[str, Any]:
    digest = None
    try:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        pass
    return {
        "version": _command_version([str(path), *version_args]),
        "path": str(path),
        "sha256": digest,
    }


def _environment_metadata(runtime: EvaluationRuntime) -> dict[str, Any]:
    metadata = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "codex": _executable_metadata(runtime.codex_executable, ["--version"]),
        "bubblewrap": _executable_metadata(
            runtime.bubblewrap_executable, ["--version"]
        ),
    }
    native_candidates = sorted(
        runtime.codex_read_root.glob("node_modules/@openai/codex-*/vendor/*/bin/codex")
    )
    if len(native_candidates) == 1:
        metadata["codex_native"] = _executable_metadata(
            native_candidates[0], ["--version"]
        )
    return metadata


def _case_contract(case: EvaluationCase) -> dict[str, Any]:
    return {
        "id": case.identifier,
        "title": case.title,
        "category": case.category,
        "prompt": case.prompt,
        "reference_files": list(case.reference_files),
        "criteria": [
            {"id": criterion.identifier, "description": criterion.description}
            for criterion in case.criteria
        ],
        "sandbox": case.sandbox,
        "hardware_requirement": case.hardware_requirement,
        "cleanup": case.cleanup,
    }


def _tracked_definition(path: Path) -> tuple[Path, str]:
    try:
        resolved = path.resolve(strict=True)
        relative = resolved.relative_to(ROOT.resolve()).as_posix()
    except (OSError, ValueError) as error:
        raise ValueError(
            f"evaluation definition must be a tracked repository file: {path}"
        ) from error
    if not resolved.is_file():
        raise ValueError(f"evaluation definition is not a file: {path}")
    try:
        _git(
            "ls-files",
            "--error-unmatch",
            "--",
            relative,
            environment=_local_git_environment(),
        )
    except ValueError as error:
        raise ValueError(
            f"evaluation definition must be tracked by Git: {relative}"
        ) from error
    return resolved, relative


def run_evaluations(args: argparse.Namespace) -> int:
    dirty = _git(
        "status",
        "--porcelain",
        "--untracked-files=all",
        environment=_local_git_environment(),
    )
    if dirty:
        raise ValueError(
            "working tree is dirty; commit or remove every change before capturing a run"
        )
    if args.timeout <= 0:
        raise ValueError("timeout must be greater than zero")

    cases_path, cases_relative = _tracked_definition(args.cases)
    schema_path, schema_relative = _tracked_definition(DEFAULT_RESPONSE_SCHEMA)
    harness_path, harness_relative = _tracked_definition(Path(__file__))
    runtime = _resolve_runtime()
    try:
        case_definition = cases_path.read_bytes()
        response_schema_definition = schema_path.read_bytes()
        harness_source = harness_path.read_bytes()
    except OSError as error:
        raise ValueError(f"cannot snapshot evaluation definitions: {error}") from error

    cases = _load_case_document(_decode_json(case_definition, cases_path))
    validate_response_schema(_decode_json(response_schema_definition, schema_path))
    if _git(
        "status",
        "--porcelain",
        "--untracked-files=all",
        environment=_local_git_environment(),
    ):
        raise ValueError(
            "working tree changed while evaluation definitions were captured"
        )
    selected = set(args.case or [])
    if selected:
        unknown = selected - {case.identifier for case in cases}
        if unknown:
            raise ValueError(f"unknown case ids: {', '.join(sorted(unknown))}")
        cases = [case for case in cases if case.identifier in selected]

    revision = _resolve_commit(args.revision)
    harness_revision = _resolve_commit("HEAD")
    environment_metadata = _environment_metadata(runtime)
    results: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="domes-agent-eval-run-") as directory:
        run_temporary = Path(directory)
        immutable_schema = run_temporary / "response.schema.json"
        immutable_schema.write_bytes(response_schema_definition)
        immutable_schema.chmod(0o444)
        codex_home = run_temporary / "codex-home"
        source_auth = _prepare_codex_home(codex_home)
        containment_preflight = _containment_preflight(
            runtime, codex_home, source_auth, immutable_schema
        )
        for index, case in enumerate(cases, start=1):
            print(f"[{index}/{len(cases)}] {case.identifier}", flush=True)
            result = _run_case(
                case,
                args.model,
                args.effort,
                revision,
                args.timeout,
                immutable_schema,
                codex_home,
                runtime,
            )
            results.append(result)
            print(f"  {result['status']}", flush=True)

    completed = [result for result in results if result["status"] == "completed"]
    document = {
        "schema_version": SCHEMA_VERSION,
        "run_id": args.run_id,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "repository": "pcesar22/domes",
        "revision": revision,
        "model": args.model,
        "reasoning_effort": args.effort,
        "execution": {
            "timeout_seconds": args.timeout,
            "workspace_access": "read-only",
            "hardware_execution": "prohibited",
            "containment": "bubblewrap-pid-namespace-and-codex-permission-profile",
            "containment_preflight": containment_preflight,
            "auth": "isolated-file-copy",
        },
        "case_definition_path": cases_relative,
        "case_definition_sha256": hashlib.sha256(case_definition).hexdigest(),
        "response_schema_path": schema_relative,
        "response_schema_sha256": hashlib.sha256(
            response_schema_definition
        ).hexdigest(),
        "harness": {
            "revision": harness_revision,
            "path": harness_relative,
            "source_sha256": hashlib.sha256(harness_source).hexdigest(),
            "dirty": False,
        },
        "case_contracts": [_case_contract(case) for case in cases],
        "environment": environment_metadata,
        "summary": {
            "total": len(results),
            "completed": len(completed),
            "review_ready": sum(1 for result in completed if result["review_ready"]),
            "errors": sum(1 for result in results if result["status"] == "error"),
            "criteria_covered": sum(
                result["coverage"]["covered"] for result in completed
            ),
            "criteria_possible": sum(
                result["coverage"]["possible"] for result in completed
            ),
        },
        "results": results,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output}")
    return evaluation_exit_code(document["summary"])


def render_report(document: dict[str, Any]) -> str:
    summary = document["summary"]
    execution = document.get("execution", {})
    containment = execution.get("containment", "-")
    preflight = execution.get("containment_preflight", {})
    preflight_detail = json.dumps(preflight, sort_keys=True, ensure_ascii=True)
    lines = [
        f"# Agent Evaluation: {document['run_id']}",
        "",
        f"- Revision: `{document['revision']}`",
        f"- Model: `{document['model']}`",
        f"- Reasoning effort: `{document['reasoning_effort']}`",
        f"- Completed: {summary['completed']} / {summary['total']}",
        f"- Structurally review-ready: {summary['review_ready']} / {summary['completed']}",
        "- Criterion evidence coverage: "
        f"{summary['criteria_covered']} / {summary['criteria_possible']}",
        f"- Errors: {summary['errors']}",
        f"- Containment: `{containment}`",
        f"- Containment preflight: `{preflight_detail}`",
        f"- Workspace access: `{execution.get('workspace_access', '-')}`",
        f"- Hardware execution: `{execution.get('hardware_execution', '-')}`",
        f"- Authentication isolation: `{execution.get('auth', '-')}`",
        f"- Case timeout: {execution.get('timeout_seconds', '-')} seconds",
        "",
        "> Automated checks validate evidence coverage and execution contracts only. "
        "Every completed case requires an independent LLM semantic audit.",
        "",
        "| Case | Automated status | Coverage | Duration | Hardware requirement |",
        "| --- | --- | ---: | ---: | --- |",
    ]
    for result in document["results"]:
        coverage = (
            f"{result['coverage']['covered']}/{result['coverage']['possible']}"
            if result["status"] == "completed"
            else "-"
        )
        duration = (
            f"{result.get('duration_seconds', 0):.1f}s"
            if "duration_seconds" in result
            else "-"
        )
        lines.append(
            "| {id} | {status} | {coverage} | {duration} | {hardware} |".format(
                id=result["id"],
                status=(
                    "semantic audit required"
                    if result.get("review_ready")
                    else (
                        "incomplete coverage"
                        if result["status"] == "completed"
                        else result["status"]
                    )
                ),
                coverage=coverage,
                duration=duration,
                hardware=result.get("hardware_requirement", "-"),
            )
        )
    for result in document["results"]:
        if result["status"] != "completed":
            lines.extend(["", f"## {result['id']}", ""])
            if result.get("reason"):
                lines.append(f"- Reason: {result['reason']}")
            if result.get("error"):
                lines.append(f"- Error: {result['error']}")
            if result.get("read_only_violation"):
                lines.append("- Read-only checkout violation: detected")
            for name, value in sorted(result.get("usage", {}).items()):
                lines.append(f"- Token usage `{name}`: {value}")
            for change in result.get("checkout_changes", []):
                lines.append(f"- Checkout change: `{change}`")
            for error in result.get("cleanup_errors", []):
                lines.append(f"- Cleanup error: {error}")
            continue
        lines.extend(["", f"## {result['id']}", ""])
        response = result.get("response", {})
        lines.extend(["### Agent summary", "", response.get("summary", "-")])
        for heading, field, code in (
            ("Agent-reported files", "files", True),
            ("Agent claims", "claims", False),
            ("Agent invariants", "invariants", False),
            ("Agent verification plan", "verification", False),
        ):
            lines.extend(["", f"### {heading}", ""])
            values = response.get(field, [])
            if values:
                if code:
                    lines.extend(f"- `{value}`" for value in values)
                else:
                    lines.extend(f"- {value}" for value in values)
            else:
                lines.append("- None reported.")
        usage = result.get("usage", {})
        lines.extend(["", "### Token usage", ""])
        if usage:
            for name, value in sorted(usage.items()):
                lines.append(f"- `{name}`: {value}")
        else:
            lines.append("- Not reported by Codex.")
        for criterion in result["coverage"]["criteria"]:
            status = "covered" if criterion["covered"] else "incomplete"
            lines.extend(["", f"### {criterion['id']} ({status})", ""])
            lines.append(criterion["description"])
            lines.append("")
            for evidence in criterion["evidence"]:
                lines.append(f"- Evidence: {evidence}")
            for path in criterion["files"]:
                lines.append(f"- Path: `{path}`")
            for issue in criterion["coverage_issues"]:
                lines.append(f"- Coverage issue: {issue}")
        failed_checks = [
            check for check in result["contract_checks"] if not check["satisfied"]
        ]
        if failed_checks:
            lines.extend(["", "### Contract issues", ""])
            for check in failed_checks:
                detail = json.dumps(check, sort_keys=True, ensure_ascii=True)
                lines.append(f"- `{detail}`")
        for change in result.get("checkout_changes", []):
            lines.append(f"- Checkout change: `{change}`")
    lines.extend(
        [
            "",
            "## Interpretation",
            "",
            "This report is a repository-understanding and change-planning baseline. "
            "Coverage means the response supplied auditable evidence for each requested "
            "dimension; it is not a correctness score or approval.",
            "",
        ]
    )
    return "\n".join(lines)


def report_command(args: argparse.Namespace) -> int:
    document = _read_json(args.input)
    if document.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported result schema")
    report = render_report(document)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
        print(f"wrote {args.output}")
    else:
        print(report, end="")
    return 0


def validate_command(args: argparse.Namespace) -> int:
    cases = load_cases(args.cases)
    validate_response_schema(_read_json(args.response_schema))
    print(f"validated {len(cases)} cases")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser("validate", help="validate cases and schemas")
    validate.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    validate.add_argument(
        "--response-schema", type=Path, default=DEFAULT_RESPONSE_SCHEMA
    )
    validate.set_defaults(func=validate_command)

    run = subparsers.add_parser("run", help="run coding-agent evaluations")
    run.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    run.add_argument("--case", action="append", help="case id; repeat to select")
    run.add_argument("--model", default="gpt-5.6-sol")
    run.add_argument(
        "--effort",
        choices=("low", "medium", "high", "xhigh", "max", "ultra"),
        default="medium",
    )
    run.add_argument("--revision", default="HEAD")
    run.add_argument("--run-id", required=True)
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--timeout", type=int, default=600)
    run.set_defaults(func=run_evaluations)

    report = subparsers.add_parser("report", help="render a Markdown report")
    report.add_argument("--input", type=Path, required=True)
    report.add_argument("--output", type=Path)
    report.set_defaults(func=report_command)
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
