#!/usr/bin/env python3
"""Host-side, fail-closed broker for the two registered DOMES NFFs.

The public capability is deliberately useless for opening a device: it contains
only a random token and a queue path.  The private capability (including ports
and identity snapshots) stays with this host process.
"""

from __future__ import annotations

import argparse
import fcntl
import fnmatch
import hashlib
import json
import os
import re
import secrets
import shutil
import signal
import stat
import subprocess
import sys
import threading
import time
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from serial_trace_proxy import SerialTraceProxy, SerialTraceProxyError

OPERATIONS = frozenset(
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
        "flash",
        "flash-trace-acceptance",
        "ota",
        "reset",
        "run",
        "artifact-hash",
    }
)
MAX_REQUEST_BYTES = 16 * 1024
MAX_REQUESTS = 256
MAX_CANDIDATE_LOG_BYTES = 8 * 1024 * 1024
MAX_CANDIDATE_DISK_GROWTH_BYTES = 1024 * 1024 * 1024
MAX_CAPABILITY_EVIDENCE_BYTES = 4 * 1024 * 1024 * 1024
MAX_CANDIDATE_MEMORY_BYTES = 4 * 1024 * 1024 * 1024
MAX_CANDIDATE_PIDS = 128
MAX_CANDIDATE_EVIDENCE_ENTRIES = 100_000
MIN_HOST_FREE_BYTES = 10 * 1024 * 1024 * 1024
MIN_HOST_FREE_INODES = 100_000


class BrokerError(RuntimeError):
    pass


def beneath(path: Path, root: Path) -> Path:
    resolved, base = path.resolve(strict=False), root.resolve(strict=False)
    try:
        resolved.relative_to(base)
    except ValueError as error:
        raise BrokerError("path escapes broker root") from error
    return resolved


def _json_write_atomic(path: Path, value: dict[str, Any]) -> None:
    temp = path.with_name(f".{path.name}.{secrets.token_hex(8)}.tmp")
    temp.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")
    os.chmod(temp, 0o600)
    temp.replace(path)


def _udev_properties(port: Path) -> dict[str, str]:
    completed = subprocess.run(
        ["udevadm", "info", "--query=property", f"--name={port}"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    if completed.returncode:
        raise BrokerError("udev identity lookup failed")
    return dict(
        line.split("=", 1) for line in completed.stdout.splitlines() if "=" in line
    )


def snapshot_port(port: str) -> dict[str, Any]:
    link = Path(port)
    if not link.is_symlink() or "CP2102N_" not in link.name:
        raise BrokerError("registered port is not a CP2102N by-id symlink")
    try:
        target = link.resolve(strict=True)
        details = target.stat()
    except OSError as error:
        raise BrokerError("registered port disappeared") from error
    if not stat.S_ISCHR(details.st_mode):
        raise BrokerError("registered port is not a character device")
    props = _udev_properties(target)
    if props.get("ID_VENDOR_ID") != "10c4" or props.get("ID_MODEL_ID") not in {
        "ea60",
        "ea63",
    }:
        raise BrokerError("registered port is not a Silicon Labs CP2102N")
    serial = props.get("ID_SERIAL_SHORT", "")
    if not serial or f"_{serial}-" not in link.name:
        raise BrokerError("registered port serial does not match by-id link")
    return {
        "link": str(link),
        "target": str(target),
        "rdev": details.st_rdev,
        "vendor": props["ID_VENDOR_ID"],
        "model": props["ID_MODEL_ID"],
        "serial": serial,
    }


@dataclass(frozen=True)
class Capability:
    issue: int
    spec_revision: str
    pr_head: str
    workspace: Path
    evidence: Path
    operations: tuple[str, ...]
    boards: tuple[int, ...]
    token: str
    snapshots: tuple[dict[str, Any], ...] = ()
    tools: dict[str, dict[str, str]] | None = None
    base_head: str = ""
    allowed_surfaces: tuple[str, ...] = ()
    repository_url: str = ""
    head_ref: str = ""

    def document(self) -> dict[str, Any]:
        # No workspace/evidence paths either: the worker already knows its workspace;
        # keeping this public capability minimal prevents path/port leakage.
        return {
            "version": 1,
            "issue": self.issue,
            "spec_revision": self.spec_revision,
            "pr_head": self.pr_head,
            "required_base_head": self.base_head,
            "operations": list(self.operations),
            "boards": list(self.boards),
            "token": self.token,
        }

    def private_document(self) -> dict[str, Any]:
        return {
            **self.document(),
            "workspace": str(self.workspace),
            "evidence": str(self.evidence),
            "snapshots": list(self.snapshots),
            "tools": self.tools,
            "base_head": self.base_head,
            "allowed_surfaces": list(self.allowed_surfaces),
            "repository_url": self.repository_url,
            "head_ref": self.head_ref,
        }


def create_capability(
    directory: Path,
    *,
    issue: int,
    spec_revision: str,
    pr_head: str,
    workspace: Path,
    evidence: Path,
    ports: list[str],
    operations: list[str],
    boards: list[int],
    base_head: str,
    allowed_surfaces: list[str],
    repository_url: str,
    head_ref: str,
    trusted_tools: dict[str, dict[str, str]] | None = None,
) -> Capability:
    allowed = tuple(sorted(set(operations)))
    if not allowed or not set(allowed).issubset(OPERATIONS):
        raise BrokerError("capability operations are not an allowlisted finite subset")
    if (
        not re.fullmatch(r"[0-9a-f]{40}", base_head)
        or not allowed_surfaces
        or not re.fullmatch(
            r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?",
            repository_url,
        )
        or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._/-]{0,199}", head_ref)
        or ".." in head_ref
        or "@{" in head_ref
    ):
        raise BrokerError("capability build policy is incomplete")
    workspace = workspace.resolve(strict=True)
    evidence.mkdir(parents=True, exist_ok=True)
    snapshots = tuple(snapshot_port(port) for port in ports)
    if not snapshots:
        raise BrokerError("no registered hardware ports")
    allowed_boards = tuple(sorted(set(boards)))
    if (
        not allowed_boards
        or len(allowed_boards) != len(boards)
        or any(
            isinstance(board, bool)
            or not isinstance(board, int)
            or not 0 <= board < len(snapshots)
            for board in allowed_boards
        )
    ):
        raise BrokerError("capability boards are not a finite registered alias subset")
    directory.mkdir(parents=True, exist_ok=True)
    if any(directory.iterdir()):
        raise BrokerError("public capability directory is not empty")
    os.chmod(directory, 0o700)
    (directory / "requests").mkdir(mode=0o700)
    (directory / "results").mkdir(mode=0o700)
    # The sandbox can read this self-contained client from its capability directory;
    # it need not (and must not) read controller source outside the issue worktree.
    client_source = Path(__file__).with_name("hardware_client.py")
    client_target = directory / "hardware_client.py"
    client_target.write_text(
        client_source.read_text(encoding="utf-8"), encoding="utf-8"
    )
    os.chmod(client_target, 0o500)
    cap = Capability(
        issue,
        spec_revision,
        pr_head,
        workspace,
        evidence.resolve(),
        allowed,
        allowed_boards,
        secrets.token_urlsafe(32),
        snapshots,
        trusted_tools or {},
        base_head,
        tuple(allowed_surfaces),
        repository_url,
        head_ref,
    )
    _json_write_atomic(directory / "capability.json", cap.document())
    return cap


def load_private_capability(item: dict[str, Any]) -> Capability:
    return Capability(
        int(item["issue"]),
        str(item["spec_revision"]),
        str(item["pr_head"]),
        Path(item["workspace"]),
        Path(item["evidence"]),
        tuple(item["operations"]),
        tuple(item["boards"]),
        str(item["token"]),
        tuple(item["snapshots"]),
        dict(item.get("tools", {})),
        str(item["base_head"]),
        tuple(item["allowed_surfaces"]),
        str(item["repository_url"]),
        str(item["head_ref"]),
    )


class DeviceLease:
    def __init__(self, path: Path) -> None:
        self.path, self.stream = path, None

    def __enter__(self) -> "DeviceLease":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.stream = self.path.open("a+")
        try:
            fcntl.flock(self.stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            self.stream.close()
            self.stream = None
            raise BrokerError("registered hardware lease is held") from error
        return self

    def __exit__(self, *_: object) -> None:
        if self.stream is not None:
            fcntl.flock(self.stream.fileno(), fcntl.LOCK_UN)
            self.stream.close()
            self.stream = None


def _verified_port(cap: Capability, board: int) -> str:
    if (
        isinstance(board, bool)
        or not isinstance(board, int)
        or board not in cap.boards
        or not 0 <= board < len(cap.snapshots)
    ):
        raise BrokerError("board must be a broker board alias")
    expected, current = cap.snapshots[board], snapshot_port(
        str(cap.snapshots[board]["link"])
    )
    for key in ("link", "target", "rdev", "vendor", "model", "serial"):
        if current[key] != expected[key]:
            raise BrokerError("registered port identity changed after preflight")
    return str(current["link"])


def validate_request(
    cap: Capability, request: dict[str, Any]
) -> tuple[str, Path | None]:
    if request.get("token") != cap.token:
        raise BrokerError("unauthenticated request")
    if (request.get("issue"), request.get("spec_revision"), request.get("pr_head")) != (
        cap.issue,
        cap.spec_revision,
        cap.pr_head,
    ):
        raise BrokerError("request is not bound to this ticket artifact")
    operation = request.get("operation")
    if operation not in cap.operations:
        raise BrokerError("operation is not allowlisted")
    raw_path = request.get("path")
    if raw_path is None:
        return operation, None
    if not isinstance(raw_path, str):
        raise BrokerError("invalid path")
    if operation != "artifact-hash":
        raise BrokerError("this operation does not accept a worker path")
    return operation, beneath(Path(raw_path), cap.evidence)


def _trusted_path(cap: Capability, name: str) -> str:
    tool = (cap.tools or {}).get(name)
    if not isinstance(tool, dict):
        raise BrokerError(f"trusted {name} is unavailable")
    path, digest = Path(str(tool.get("path", ""))), str(tool.get("sha256", ""))
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise BrokerError(f"trusted {name} changed after preflight")
    return str(path)


def _directory_sha256(root: Path) -> str:
    if not root.is_dir() or root.is_symlink():
        raise BrokerError("trusted directory is unavailable")
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise BrokerError("trusted directory contains a symlink")
        if not path.is_file():
            continue
        relative = str(path.relative_to(root)).encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        data = path.read_bytes()
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def _trusted_directory(cap: Capability, name: str) -> Path:
    record = (cap.tools or {}).get(name)
    if not isinstance(record, dict):
        raise BrokerError(f"trusted {name} is unavailable")
    path = Path(str(record.get("path", "")))
    if _directory_sha256(path) != record.get("sha256"):
        raise BrokerError(f"trusted {name} changed after preflight")
    return path


def _safe_git_prefix(git: str) -> list[str]:
    return [
        git,
        "-c",
        "core.fsmonitor=false",
        "-c",
        "core.hooksPath=/dev/null",
        "-c",
        "core.attributesFile=/dev/null",
        "-c",
        "core.excludesFile=/dev/null",
    ]


def _safe_git_argv(git: str, repository: Path, *arguments: str) -> list[str]:
    """Disable worker-controlled Git execution hooks for controller reads."""
    return [
        *_safe_git_prefix(git),
        "-C",
        str(repository),
        *arguments,
    ]


def _safe_git_environment() -> dict[str, str]:
    return {
        "HOME": "/tmp",
        "LANG": "C.UTF-8",
        "PATH": "/usr/bin:/bin",
        "GIT_ATTR_NOSYSTEM": "1",
        "GIT_CONFIG_GLOBAL": "/dev/null",
        "GIT_CONFIG_SYSTEM": "/dev/null",
        "GIT_PAGER": "cat",
        "GIT_TERMINAL_PROMPT": "0",
    }


def _cli_path(cap: Capability) -> str:
    # The controller's already-built CLI is trusted host tooling.  A candidate
    # worktree may supply firmware *data*, never executable host code.
    return _trusted_path(cap, "domes-cli")


def _workspace_head(cap: Capability) -> str:
    """Resolve the pushed candidate from the controller-pinned GitHub branch."""
    git = _trusted_path(cap, "git")
    resolved = subprocess.run(
        [
            *_safe_git_prefix(git),
            "ls-remote",
            "--exit-code",
            cap.repository_url,
            f"refs/heads/{cap.head_ref}",
        ],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    fields = resolved.stdout.split()
    head = fields[0] if len(fields) == 2 else ""
    if resolved.returncode or not re.fullmatch(r"[0-9a-f]{40}", head):
        raise BrokerError("hardware operation requires a pushed candidate branch")
    if head != cap.pr_head:
        raise BrokerError("pushed candidate changed after independent safety review")
    return head


def _path_matches_surface(path: str, pattern: str) -> bool:
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


def _validate_build_surfaces(
    cap: Capability, repository: Path, artifact_head: str
) -> None:
    """Reject candidate files outside the ticket contract before any build code runs."""
    git = _trusted_path(cap, "git")
    if not re.fullmatch(r"[0-9a-f]{40}", cap.base_head) or not cap.allowed_surfaces:
        raise BrokerError("firmware build has no pinned architectural surface policy")
    ancestor = subprocess.run(
        _safe_git_argv(
            git,
            repository,
            "merge-base",
            "--is-ancestor",
            cap.base_head,
            artifact_head,
        ),
        check=False,
        capture_output=True,
        shell=False,
        env=_safe_git_environment(),
    )
    changed = subprocess.run(
        _safe_git_argv(
            git,
            repository,
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--name-status",
            "--find-renames",
            "--diff-filter=ACDMRT",
            "-z",
            f"{cap.base_head}...{artifact_head}",
            "--",
        ),
        check=False,
        capture_output=True,
        shell=False,
        env=_safe_git_environment(),
    )
    if ancestor.returncode or changed.returncode:
        raise BrokerError("firmware artifact is not descended from its pinned base")
    try:
        fields = [item.decode("utf-8") for item in changed.stdout.split(b"\0") if item]
    except UnicodeDecodeError as error:
        raise BrokerError("firmware diff contains a non-UTF-8 path") from error
    paths: list[str] = []
    offset = 0
    while offset < len(fields):
        status = fields[offset]
        offset += 1
        path_count = 2 if status.startswith(("R", "C")) else 1
        if not re.fullmatch(
            r"(?:[ACDMT]|[RC][0-9]{1,3})", status
        ) or offset + path_count > len(fields):
            raise BrokerError("firmware diff name-status output is invalid")
        paths.extend(fields[offset : offset + path_count])
        offset += path_count
    violations = [
        path
        for path in paths
        if not any(
            _path_matches_surface(path, pattern) for pattern in cap.allowed_surfaces
        )
    ]
    if violations:
        raise BrokerError(
            "firmware artifact changes files outside ticket surfaces: "
            + ", ".join(violations[:8])
        )


def _validate_candidate_firmware_safety(
    cap: Capability, repository: Path, artifact_head: str
) -> None:
    """Reject explicit irreversible-device operations before compiling a candidate."""
    git = _trusted_path(cap, "git")
    changed = subprocess.run(
        _safe_git_argv(
            git,
            repository,
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--unified=0",
            f"{cap.base_head}...{artifact_head}",
            "--",
            "firmware/",
        ),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    if changed.returncode:
        raise BrokerError("cannot inspect candidate firmware safety diff")
    additions = "\n".join(
        line[1:]
        for line in changed.stdout.splitlines()
        if line.startswith("+") and not line.startswith("+++")
    )
    forbidden = re.compile(
        r"(?i)(?:"
        r"esp_efuse_(?!(?:read_|get_|check_|find_|count_|is_|"
        r"block_is_empty\b|key_block_unused\b|mac_get_(?:default|custom)\b))|"
        r"efuse_(?:write|burn)|"
        r"esp_flash_erase|spi_flash_erase|nvs_flash_erase|"
        r"esp_partition_erase_range|erase_flash|"
        r"CONFIG_(?:SECURE_BOOT|SECURE_FLASH_ENC|FLASH_ENCRYPTION)"
        r")"
    )
    match = forbidden.search(additions)
    if match is not None:
        raise BrokerError(
            f"candidate firmware adds forbidden physical operation: {match.group(0)}"
        )


def _validate_physical_sdkconfig(sdkconfig: Path) -> None:
    text = sdkconfig.read_text(encoding="utf-8")
    forbidden = (
        "CONFIG_SECURE_BOOT=y",
        "CONFIG_SECURE_BOOT_V2_ENABLED=y",
        "CONFIG_SECURE_FLASH_ENC_ENABLED=y",
        "CONFIG_FLASH_ENCRYPTION_ENABLED=y",
    )
    if any(setting in text for setting in forbidden):
        raise BrokerError(
            "physical build enables a forbidden irreversible security mode"
        )


def _stage_input(cap: Capability, source: Path) -> tuple[Path, str]:
    """Copy immutable candidate data before trusted tooling consumes it."""
    if not source.is_file():
        raise BrokerError("candidate input is not a file")
    ensure_capability_evidence_budget(cap, source.stat().st_size)
    stage = cap.evidence / "staged"
    stage.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    target = stage / f"{digest}-{secrets.token_hex(4)}{source.suffix}"
    # Copy then hash again: a worker mutation can only cause a rejected staging
    # attempt, never alter the bytes supplied to esptool or the CLI.
    shutil.copyfile(source, target)
    if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        target.unlink(missing_ok=True)
        raise BrokerError("candidate input changed while staging")
    ensure_capability_evidence_budget(cap)
    return target, digest


def _ota_version(image: Path, esptool: str) -> str:
    completed = subprocess.run(
        [esptool, "image_info", "--version", "2", str(image)],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    for line in completed.stdout.splitlines():
        if line.startswith("App version: "):
            version = line.removeprefix("App version: ").strip()
            if version and len(version.encode("ascii")) <= 31:
                return version
    raise BrokerError("cannot derive parser-valid embedded OTA version")


def _write_profile_defaults(
    cap: Capability, project: Path, suffix: str, build_profile: str
) -> Path:
    """Create a controller-owned, finite physical-profile Kconfig fragment."""
    if build_profile not in {"default", "trace-acceptance"}:
        raise BrokerError("firmware build profile is not allowlisted")
    checked_in = project / "sdkconfig.defaults"
    if not checked_in.is_file():
        raise BrokerError("checked-in physical sdkconfig defaults are unavailable")
    profile_keys = (
        "CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL",
        "CONFIG_DOMES_RUNTIME_PROFILE_QEMU",
        "CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE",
    )
    lines = [
        line
        for line in checked_in.read_text(encoding="utf-8").splitlines()
        if not any(key in line for key in profile_keys)
    ]
    lines.extend(
        (
            "",
            "# Controller-owned finite hardware build profile.",
            "CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL=y",
            "# CONFIG_DOMES_RUNTIME_PROFILE_QEMU is not set",
            (
                "CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE=y"
                if build_profile == "trace-acceptance"
                else "# CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE is not set"
            ),
        )
    )
    target = cap.evidence / f"sdkconfig-defaults-{suffix}"
    encoded = "\n".join(lines) + "\n"
    ensure_capability_evidence_budget(cap, len(encoded.encode("utf-8")))
    target.write_text(encoded, encoding="utf-8")
    os.chmod(target, 0o600)
    ensure_capability_evidence_budget(cap)
    return target


def _profile_build_matches(build: Path, build_profile: str) -> bool:
    config_path = build / "config" / "sdkconfig.json"
    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    return (
        config.get("DOMES_RUNTIME_PROFILE_PHYSICAL") is True
        and config.get("DOMES_RUNTIME_PROFILE_QEMU") is not True
        and (config.get("DOMES_TRACE_ACCEPTANCE_PROBE") is True)
        == (build_profile == "trace-acceptance")
    )


def _discard_incomplete_trusted_build(cap: Capability, paths: tuple[Path, ...]) -> None:
    """Remove only broker-private partial build state so a retry can recover."""
    for path in paths:
        beneath(path, cap.evidence)
        if path.is_symlink():
            raise BrokerError("incomplete trusted build path is a symlink")
        if path.is_dir():
            shutil.rmtree(path)
        elif path.exists():
            path.unlink()


def _compiler_temp_directory(cap: Capability) -> Path:
    """Return a fresh monitored temp directory on the evidence filesystem."""
    compiler_tmp = beneath(cap.evidence / "tmp", cap.evidence)
    if compiler_tmp.exists() or compiler_tmp.is_symlink():
        _discard_incomplete_trusted_build(cap, (compiler_tmp,))
    compiler_tmp.mkdir(mode=0o700)
    os.chmod(compiler_tmp, 0o700)
    return compiler_tmp


def _espressif_root(cap: Capability) -> Path:
    esptool = Path(_trusted_path(cap, "esptool")).resolve()
    for parent in esptool.parents:
        if parent.name == ".espressif" and parent.is_dir():
            return parent
    raise BrokerError("trusted ESP-IDF tools root is unavailable")


def _mapped_espressif_path(cap: Capability, name: str) -> Path:
    host = Path(_trusted_path(cap, name))
    root = _espressif_root(cap)
    try:
        relative = host.relative_to(root)
    except ValueError as error:
        raise BrokerError(f"trusted {name} is outside ESP-IDF tools") from error
    return Path("/idf-tools") / relative


def _resource_limited(cap: Capability, argv: list[str]) -> list[str]:
    """Bound candidate build processes without depending on systemd."""
    return [
        _trusted_path(cap, "prlimit"),
        "--nproc=1024:1024",
        "--as=4294967296:4294967296",
        "--fsize=67108864:67108864",
        "--nofile=4096:4096",
        "--cpu=1800:1800",
        "--",
        *argv,
    ]


def _directory_size(path: Path, stop_after: int) -> int:
    """Measure regular-file bytes without following candidate-created symlinks."""
    total = 0
    entry_count = 0
    pending = [path]
    while pending:
        current = pending.pop()
        try:
            entries = os.scandir(current)
        except OSError as error:
            raise BrokerError("candidate evidence tree became unreadable") from error
        with entries:
            for entry in entries:
                entry_count += 1
                if entry_count > MAX_CANDIDATE_EVIDENCE_ENTRIES:
                    raise BrokerError(
                        "candidate process exceeded aggregate evidence entry limit"
                    )
                try:
                    if entry.is_symlink():
                        continue
                    if entry.is_dir(follow_symlinks=False):
                        pending.append(Path(entry.path))
                    elif entry.is_file(follow_symlinks=False):
                        total += entry.stat(follow_symlinks=False).st_size
                        if total > stop_after:
                            return total
                except OSError as error:
                    raise BrokerError(
                        "candidate evidence tree changed during scan"
                    ) from error
    return total


def ensure_capability_evidence_budget(cap: Capability, reserve_bytes: int = 0) -> int:
    """Enforce one cumulative quota across every request and allocation path."""
    if reserve_bytes < 0 or reserve_bytes > MAX_CAPABILITY_EVIDENCE_BYTES:
        raise BrokerError("invalid capability evidence reservation")
    current = _directory_size(cap.evidence, MAX_CAPABILITY_EVIDENCE_BYTES)
    if current + reserve_bytes > MAX_CAPABILITY_EVIDENCE_BYTES:
        raise BrokerError("hardware capability exceeded its cumulative evidence quota")
    free_bytes, free_inodes = _filesystem_capacity(cap.evidence)
    if free_bytes < MIN_HOST_FREE_BYTES or free_inodes < MIN_HOST_FREE_INODES:
        raise BrokerError("host filesystem reserve is insufficient for candidate work")
    return current


def _candidate_process_tree(root_pid: int) -> tuple[set[int], int]:
    """Return the live descendant set and aggregate resident bytes from procfs."""
    parents: dict[int, int] = {}
    resident: dict[int, int] = {}
    page_size = os.sysconf("SC_PAGE_SIZE")
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        pid = int(entry.name)
        try:
            stat_fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
            parents[pid] = int(stat_fields[1])
            statm_fields = (entry / "statm").read_text().split()
            resident[pid] = int(statm_fields[1]) * page_size
        except (IndexError, OSError, ValueError):
            continue
    descendants = {root_pid}
    changed = True
    while changed:
        changed = False
        for pid, parent in parents.items():
            if parent in descendants and pid not in descendants:
                descendants.add(pid)
                changed = True
    return descendants, sum(resident.get(pid, 0) for pid in descendants)


def _open_unlinked_bytes(pids: set[int], stop_after: int) -> int:
    """Account for open deleted files that directory traversal cannot observe."""
    total = 0
    scanned = 0
    seen: set[tuple[int, int]] = set()
    for pid in sorted(pids):
        try:
            descriptors = os.scandir(f"/proc/{pid}/fd")
        except OSError:
            continue
        with descriptors:
            for descriptor in descriptors:
                scanned += 1
                if scanned > MAX_CANDIDATE_EVIDENCE_ENTRIES:
                    raise BrokerError(
                        "candidate process exceeded aggregate descriptor limit"
                    )
                try:
                    details = descriptor.stat(follow_symlinks=True)
                except OSError:
                    continue
                identity = (details.st_dev, details.st_ino)
                if (
                    identity in seen
                    or details.st_nlink != 0
                    or not stat.S_ISREG(details.st_mode)
                ):
                    continue
                seen.add(identity)
                total += max(details.st_size, details.st_blocks * 512)
                if total > stop_after:
                    return total
    return total


def _filesystem_capacity(path: Path) -> tuple[int, int]:
    try:
        details = os.statvfs(path)
    except OSError as error:
        raise BrokerError("candidate evidence filesystem became unavailable") from error
    return details.f_bavail * details.f_frsize, details.f_favail


def _kill_candidate_processes(
    process: subprocess.Popen[bytes], known_pids: set[int]
) -> None:
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass
    for pid in sorted(known_pids, reverse=True):
        if pid == os.getpid():
            continue
        try:
            os.kill(pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
    if process.poll() is None:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired as error:
            raise BrokerError(
                "candidate process tree could not be terminated"
            ) from error


def _run_with_bounded_logs(
    cap: Capability,
    argv: list[str],
    name: str,
    timeout: float,
    *,
    env: dict[str, str] | None = None,
) -> tuple[int, str, str]:
    """Run with live aggregate process, memory, disk, log, and timeout cutoffs."""
    if not re.fullmatch(r"[a-z0-9-]+", name):
        raise BrokerError("invalid bounded-log name")
    stdout_path = cap.evidence / f"{name}.stdout.log"
    stderr_path = cap.evidence / f"{name}.stderr.log"
    baseline = ensure_capability_evidence_budget(cap)
    baseline_free_bytes, baseline_free_inodes = _filesystem_capacity(cap.evidence)
    if (
        baseline_free_bytes < MIN_HOST_FREE_BYTES
        or baseline_free_inodes < MIN_HOST_FREE_INODES
    ):
        raise BrokerError("host filesystem reserve is insufficient for candidate work")
    exceeded = threading.Event()
    reader_errors: list[BaseException] = []

    def drain(stream: Any, destination: Path) -> None:
        written = 0
        try:
            with stream, destination.open("wb") as output:
                while chunk := stream.read(64 * 1024):
                    remaining = MAX_CANDIDATE_LOG_BYTES - written
                    if remaining <= 0:
                        exceeded.set()
                        return
                    output.write(chunk[:remaining])
                    written += min(len(chunk), remaining)
                    if len(chunk) > remaining:
                        exceeded.set()
                        return
        except BaseException as error:  # relayed on the controller thread
            reader_errors.append(error)
            exceeded.set()

    process = subprocess.Popen(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=False,
        start_new_session=True,
        env=env,
    )
    assert process.stdout is not None and process.stderr is not None
    readers = [
        threading.Thread(
            target=drain,
            args=(process.stdout, stdout_path),
            name=f"{name}-stdout",
            daemon=True,
        ),
        threading.Thread(
            target=drain,
            args=(process.stderr, stderr_path),
            name=f"{name}-stderr",
            daemon=True,
        ),
    ]
    for reader in readers:
        reader.start()
    deadline = time.monotonic() + timeout
    failure: str | None = None
    failure_cause: BrokerError | None = None
    next_disk_check = time.monotonic()
    known_pids = {process.pid}
    try:
        while process.poll() is None:
            now = time.monotonic()
            live_pids, resident_bytes = _candidate_process_tree(process.pid)
            known_pids.update(live_pids)
            if len(live_pids) > MAX_CANDIDATE_PIDS:
                failure = "candidate process exceeded aggregate PID limit"
                break
            if resident_bytes > MAX_CANDIDATE_MEMORY_BYTES:
                failure = "candidate process exceeded aggregate memory limit"
                break
            if exceeded.is_set():
                failure = "candidate process exceeded bounded log size"
                break
            if now >= deadline:
                failure = "candidate process exceeded wall-clock timeout"
                break
            if now >= next_disk_check:
                current = _directory_size(
                    cap.evidence,
                    baseline + MAX_CANDIDATE_DISK_GROWTH_BYTES,
                )
                unlinked = _open_unlinked_bytes(
                    live_pids, MAX_CANDIDATE_DISK_GROWTH_BYTES
                )
                free_bytes, free_inodes = _filesystem_capacity(cap.evidence)
                if (
                    current - baseline > MAX_CANDIDATE_DISK_GROWTH_BYTES
                    or current + unlinked > MAX_CAPABILITY_EVIDENCE_BYTES
                    or unlinked > MAX_CANDIDATE_DISK_GROWTH_BYTES
                    or baseline_free_bytes - free_bytes
                    > MAX_CANDIDATE_DISK_GROWTH_BYTES
                    or free_bytes < MIN_HOST_FREE_BYTES
                    or free_inodes < MIN_HOST_FREE_INODES
                    or baseline_free_inodes - free_inodes
                    > MAX_CANDIDATE_EVIDENCE_ENTRIES
                ):
                    failure = "candidate process exceeded aggregate disk-growth limit"
                    break
                next_disk_check = now + 0.1
            time.sleep(0.02)
    except BrokerError as error:
        failure = str(error)
        failure_cause = error
    if failure is not None:
        _kill_candidate_processes(process, known_pids)
    else:
        process.wait()
    for reader in readers:
        reader.join(timeout=5)
    if any(reader.is_alive() for reader in readers):
        _kill_candidate_processes(process, known_pids)
        raise BrokerError("candidate log reader failed to terminate")
    if reader_errors:
        raise BrokerError("candidate log capture failed") from reader_errors[0]
    if exceeded.is_set() and failure is None:
        failure = "candidate process exceeded bounded log size"
    if failure is not None:
        raise BrokerError(failure) from failure_cause
    # A short-lived process can complete between monitor polls. Re-scan before
    # accepting it so cumulative quota enforcement is never polling-only.
    ensure_capability_evidence_budget(cap)
    return (
        process.returncode,
        stdout_path.read_text(encoding="utf-8", errors="replace"),
        stderr_path.read_text(encoding="utf-8", errors="replace"),
    )


def _firmware_build_argv(
    cap: Capability,
    source: Path,
    build: Path,
    sdkconfig: Path,
    defaults: Path,
    idf_root: Path,
) -> list[str]:
    """Create a clear-environment, networkless ESP-IDF build sandbox."""
    build.mkdir(mode=0o700)
    sdkconfig.touch(mode=0o600)
    espressif = _espressif_root(cap)
    xtensa = _mapped_espressif_path(cap, "xtensa-esp32s3-elf-gcc")
    ulp = _mapped_espressif_path(cap, "esp32ulp-elf-as")
    rom = _mapped_espressif_path(cap, "esp-rom-elf")
    idf_python = _mapped_espressif_path(cap, "idf-python")
    python_environment = idf_python.parents[1]
    path = ":".join(
        str(item)
        for item in (
            xtensa.parent,
            ulp.parent,
            idf_python.parent,
            Path("/idf/tools"),
            Path("/idf/components/espcoredump"),
            Path("/idf/components/partition_table"),
            Path("/idf/components/app_update"),
            Path("/usr/bin"),
            Path("/bin"),
        )
    )
    sandbox = [
        _bwrap(cap),
        "--die-with-parent",
        "--unshare-all",
        "--new-session",
        "--clearenv",
        "--ro-bind",
        str(source),
        "/src",
        "--ro-bind",
        str(idf_root),
        "/idf",
        "--ro-bind",
        str(espressif),
        "/idf-tools",
        *_system_ro_binds(),
        "--dir",
        "/out",
        "--bind",
        str(build),
        "/out/build",
        "--bind",
        str(sdkconfig),
        "/out/sdkconfig",
        "--ro-bind",
        str(defaults),
        "/out/sdkconfig.defaults",
        "--bind",
        str(_compiler_temp_directory(cap)),
        "/tmp",
        "--dir",
        "/tmp/home",
        "--proc",
        "/proc",
        "--dev",
        "/dev",
        "--setenv",
        "HOME",
        "/tmp/home",
        "--setenv",
        "IDF_TOOLS_PATH",
        "/idf-tools",
        "--setenv",
        "IDF_PATH",
        "/idf",
        "--setenv",
        "IDF_PYTHON_ENV_PATH",
        str(python_environment),
        "--setenv",
        "ESP_IDF_VERSION",
        "5.4",
        "--setenv",
        "ESP_ROM_ELF_DIR",
        str(rom.parent),
        "--setenv",
        "PYTHONNOUSERSITE",
        "1",
        "--setenv",
        "TMPDIR",
        "/tmp",
        "--setenv",
        "PATH",
        path,
        "--setenv",
        "LANG",
        "C.UTF-8",
        "--chdir",
        "/src/firmware/domes",
        "--",
        str(idf_python),
        "/idf/tools/idf.py",
        "-B",
        "/out/build",
        "-DSDKCONFIG=/out/sdkconfig",
        "-DSDKCONFIG_DEFAULTS=/out/sdkconfig.defaults",
        "-DCCACHE_ENABLE=0",
        "build",
    ]
    return _resource_limited(cap, sandbox)


def _stage_managed_components(cap: Capability, project: Path) -> list[dict[str, str]]:
    """Materialize only preflight-attested, lock-pinned registry components."""
    candidate_lock = project / "dependencies.lock"
    trusted_lock = Path(_trusted_path(cap, "dependencies.lock"))
    if candidate_lock.read_bytes() != trusted_lock.read_bytes():
        raise BrokerError("candidate firmware dependency lock changed from authority")
    records = [
        (name, record)
        for name, record in sorted((cap.tools or {}).items())
        if name.startswith("managed-component-") and isinstance(record, dict)
    ]
    if not records:
        raise BrokerError("no pinned managed components passed preflight")
    target_root = project / "managed_components"
    if target_root.is_symlink():
        raise BrokerError("firmware managed component root is a symlink")
    existing = target_root.exists()
    if existing and not target_root.is_dir():
        raise BrokerError("firmware managed component root is unsafe")
    if not existing:
        target_root.mkdir(mode=0o700)
    expected_destinations = {
        str(record.get("destination", "")) for _, record in records
    }
    if (
        existing
        and {path.name for path in target_root.iterdir()} != expected_destinations
    ):
        raise BrokerError("staged managed component set changed")
    provenance: list[dict[str, str]] = []
    for name, record in records:
        destination = str(record.get("destination", ""))
        component_hash = str(record.get("component_hash", ""))
        version = str(record.get("version", ""))
        if (
            not re.fullmatch(r"[a-z0-9_.-]+__[a-z0-9_.-]+", destination)
            or not re.fullmatch(r"[0-9a-f]{64}", component_hash)
            or not re.fullmatch(r"[A-Za-z0-9_.+-]+", version)
        ):
            raise BrokerError("managed component preflight record is invalid")
        source = _trusted_directory(cap, name)
        target = target_root / destination
        if not existing:
            source_size = _directory_size(source, MAX_CAPABILITY_EVIDENCE_BYTES)
            ensure_capability_evidence_budget(cap, source_size)
            shutil.copytree(source, target, symlinks=False)
            (target / ".component_hash").write_text(component_hash, encoding="utf-8")
            ensure_capability_evidence_budget(cap)
        elif (
            not target.is_dir()
            or target.is_symlink()
            or (target / ".component_hash").read_text(encoding="utf-8")
            != component_hash
        ):
            raise BrokerError("staged managed component identity changed")
        provenance.append(
            {
                "destination": destination,
                "version": version,
                "component_hash": component_hash,
                "source_tree_sha256": str(record["sha256"]),
                "staged_tree_sha256": _directory_sha256(target),
            }
        )
    return provenance


def _trusted_firmware_build(
    cap: Capability, artifact_head: str, build_profile: str = "default"
) -> tuple[Path, Path, dict[str, Any]]:
    """Build the committed candidate in a private clean clone with pinned ESP-IDF."""
    if build_profile not in {"default", "trace-acceptance"}:
        raise BrokerError("firmware build profile is not allowlisted")
    suffix = f"{artifact_head[:16]}-{build_profile}"
    source = cap.evidence / f"source-{suffix}"
    project = source / "firmware" / "domes"
    build = cap.evidence / f"build-{suffix}"
    sdkconfig = cap.evidence / f"sdkconfig-{suffix}"
    defaults_path = cap.evidence / f"sdkconfig-defaults-{suffix}"
    provenance_path = cap.evidence / f"build-{suffix}.json"
    git = _trusted_path(cap, "git")
    idf_export = _trusted_path(cap, "idf-export")
    idf_py = _trusted_path(cap, "idf.py")
    idf_record = (cap.tools or {}).get("idf.py", {})
    idf_root = Path(idf_export).parent
    idf_revision = subprocess.run(
        _safe_git_argv(git, idf_root, "rev-parse", "HEAD"),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    idf_tag = subprocess.run(
        _safe_git_argv(git, idf_root, "describe", "--tags", "--exact-match", "HEAD"),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    if (
        idf_record.get("version") != "v5.4.4"
        or idf_tag.returncode
        or idf_tag.stdout.strip() != "v5.4.4"
        or idf_revision.returncode
        or idf_revision.stdout.strip() != idf_record.get("revision")
    ):
        raise BrokerError("trusted ESP-IDF version changed after preflight")
    if provenance_path.is_file():
        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
        defaults = _write_profile_defaults(cap, project, suffix, build_profile)
        managed_components = _stage_managed_components(cap, project)
        resolved = subprocess.run(
            _safe_git_argv(git, source, "rev-parse", "HEAD"),
            check=False,
            capture_output=True,
            text=True,
            shell=False,
            env=_safe_git_environment(),
        )
        submodule_status = subprocess.run(
            _safe_git_argv(git, source, "submodule", "status", "--recursive"),
            check=False,
            capture_output=True,
            text=True,
            shell=False,
            env=_safe_git_environment(),
        )
        _validate_build_surfaces(cap, source, artifact_head)
        _validate_candidate_firmware_safety(cap, source, artifact_head)
        if (
            resolved.returncode
            or resolved.stdout.strip() != artifact_head
            or submodule_status.returncode
            or provenance.get("kind") != "controller-bwrap-clean-clone-idf-build"
            or provenance.get("source_head") != artifact_head
            or provenance.get("build_profile") != build_profile
            or provenance.get("bwrap_sha256")
            != (cap.tools or {}).get("bwrap", {}).get("sha256")
            or provenance.get("prlimit_sha256")
            != (cap.tools or {}).get("prlimit", {}).get("sha256")
            or provenance.get("xtensa_compiler_sha256")
            != (cap.tools or {}).get("xtensa-esp32s3-elf-gcc", {}).get("sha256")
            or provenance.get("ulp_tool_sha256")
            != (cap.tools or {}).get("esp32ulp-elf-as", {}).get("sha256")
            or provenance.get("rom_elf_sha256")
            != (cap.tools or {}).get("esp-rom-elf", {}).get("sha256")
            or provenance.get("idf_python_sha256")
            != (cap.tools or {}).get("idf-python", {}).get("sha256")
            or provenance.get("dependencies_lock_sha256")
            != (cap.tools or {}).get("dependencies.lock", {}).get("sha256")
            or provenance.get("managed_components") != managed_components
            or hashlib.sha256(submodule_status.stdout.encode()).hexdigest()
            != provenance.get("submodules_sha256")
            or not sdkconfig.is_file()
            or hashlib.sha256(defaults.read_bytes()).hexdigest()
            != provenance.get("sdkconfig_defaults_sha256")
            or hashlib.sha256(sdkconfig.read_bytes()).hexdigest()
            != provenance.get("sdkconfig_sha256")
            or not _profile_build_matches(build, build_profile)
        ):
            raise BrokerError("cached trusted build source changed")
        _validate_physical_sdkconfig(sdkconfig)
        return project, build, provenance
    incomplete_paths = (source, build, sdkconfig, defaults_path)
    if any(path.exists() or path.is_symlink() for path in incomplete_paths):
        _discard_incomplete_trusted_build(cap, incomplete_paths)
    ensure_capability_evidence_budget(cap, MAX_CANDIDATE_DISK_GROWTH_BYTES)
    cloned_returncode, _cloned_stdout, _cloned_stderr = _run_with_bounded_logs(
        cap,
        [
            *_safe_git_prefix(git),
            "clone",
            "--no-hardlinks",
            "--no-checkout",
            cap.repository_url,
            str(source),
        ],
        f"firmware-clone-{suffix}",
        120,
        env=_safe_git_environment(),
    )
    if cloned_returncode:
        raise BrokerError("failed to create controller-owned firmware source clone")
    if (source / ".git" / "objects" / "info" / "alternates").exists():
        raise BrokerError("controller firmware clone retained a host object alternate")
    checked_out_returncode, _checkout_stdout, _checkout_stderr = _run_with_bounded_logs(
        cap,
        _safe_git_argv(git, source, "checkout", "--detach", artifact_head),
        f"firmware-checkout-{suffix}",
        120,
        env=_safe_git_environment(),
    )
    if checked_out_returncode or not project.is_dir():
        raise BrokerError("failed to check out committed firmware source")
    _validate_build_surfaces(cap, source, artifact_head)
    _validate_candidate_firmware_safety(cap, source, artifact_head)
    controller_root = Path(__file__).resolve().parents[2]
    candidate_modules = source / ".gitmodules"
    controller_modules = controller_root / ".gitmodules"
    if (
        not candidate_modules.is_file()
        or not controller_modules.is_file()
        or candidate_modules.read_bytes() != controller_modules.read_bytes()
    ):
        raise BrokerError("candidate firmware submodule policy changed")
    submodule_path = "firmware/third_party/nanopb"
    local_submodule = controller_root / submodule_path
    expected_submodule = subprocess.run(
        _safe_git_argv(git, source, "ls-tree", artifact_head, "--", submodule_path),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    local_submodule_head = subprocess.run(
        _safe_git_argv(git, local_submodule, "rev-parse", "HEAD"),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    local_submodule_clean = subprocess.run(
        _safe_git_argv(
            git,
            local_submodule,
            "status",
            "--porcelain",
            "--untracked-files=all",
        ),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    expected_match = re.fullmatch(
        rf"160000 commit ([0-9a-f]{{40}})\t{re.escape(submodule_path)}\n?",
        expected_submodule.stdout,
    )
    if (
        expected_submodule.returncode
        or expected_match is None
        or local_submodule_head.returncode
        or local_submodule_head.stdout.strip() != expected_match.group(1)
        or local_submodule_clean.returncode
        or local_submodule_clean.stdout
    ):
        raise BrokerError("local firmware submodule source is not exact and clean")
    initialized_returncode, _initialized_stdout, _initialized_stderr = (
        _run_with_bounded_logs(
            cap,
            _safe_git_argv(git, source, "submodule", "init", "--", submodule_path),
            f"firmware-submodule-init-{suffix}",
            60,
            env=_safe_git_environment(),
        )
    )
    configured_returncode, _configured_stdout, _configured_stderr = (
        _run_with_bounded_logs(
            cap,
            _safe_git_argv(
                git,
                source,
                "config",
                "--replace-all",
                f"submodule.{submodule_path}.url",
                str(local_submodule),
            ),
            f"firmware-submodule-config-{suffix}",
            60,
            env=_safe_git_environment(),
        )
    )
    submodules_returncode, _submodules_stdout, _submodules_stderr = (
        _run_with_bounded_logs(
            cap,
            [
                *_safe_git_prefix(git),
                "-c",
                "protocol.file.allow=always",
                "-c",
                f"submodule.{submodule_path}.url={local_submodule}",
                "-C",
                str(source),
                "submodule",
                "update",
                "--no-fetch",
                "--",
                submodule_path,
            ],
            f"firmware-submodule-update-{suffix}",
            300,
            env=_safe_git_environment(),
        )
    )
    if initialized_returncode or configured_returncode or submodules_returncode:
        raise BrokerError("failed to check out committed firmware submodules")
    submodule_status = subprocess.run(
        _safe_git_argv(git, source, "submodule", "status", "--recursive"),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    if submodule_status.returncode or any(
        line and not line.startswith(" ")
        for line in submodule_status.stdout.splitlines()
    ):
        raise BrokerError("trusted firmware submodule state is not exact")
    managed_components = _stage_managed_components(cap, project)
    defaults = _write_profile_defaults(cap, project, suffix, build_profile)
    returncode, stdout, stderr = _run_with_bounded_logs(
        cap,
        _firmware_build_argv(cap, source, build, sdkconfig, defaults, idf_root),
        f"trusted-build-{suffix}",
        1200,
    )
    if returncode:
        raise BrokerError("controller-owned ESP-IDF v5.4.4 firmware build failed")
    _validate_physical_sdkconfig(sdkconfig)
    if not _profile_build_matches(build, build_profile):
        raise BrokerError("controller-owned firmware build profile is inconsistent")
    resolved = subprocess.run(
        _safe_git_argv(git, source, "rev-parse", "HEAD"),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    tracked = subprocess.run(
        _safe_git_argv(
            git,
            source,
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--quiet",
            "--exit-code",
        ),
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        env=_safe_git_environment(),
    )
    if (
        resolved.returncode
        or resolved.stdout.strip() != artifact_head
        or tracked.returncode
    ):
        raise BrokerError("trusted firmware source changed during build")
    provenance = {
        "kind": "controller-bwrap-clean-clone-idf-build",
        "source_head": artifact_head,
        "build_profile": build_profile,
        "idf_version": "v5.4.4",
        "idf_revision": str(idf_record["revision"]),
        "idf_export_sha256": (cap.tools or {})["idf-export"]["sha256"],
        "idf_py_sha256": (cap.tools or {})["idf.py"]["sha256"],
        "bwrap_sha256": (cap.tools or {})["bwrap"]["sha256"],
        "prlimit_sha256": (cap.tools or {})["prlimit"]["sha256"],
        "xtensa_compiler_sha256": (cap.tools or {})["xtensa-esp32s3-elf-gcc"]["sha256"],
        "ulp_tool_sha256": (cap.tools or {})["esp32ulp-elf-as"]["sha256"],
        "rom_elf_sha256": (cap.tools or {})["esp-rom-elf"]["sha256"],
        "idf_python_sha256": (cap.tools or {})["idf-python"]["sha256"],
        "dependencies_lock_sha256": (cap.tools or {})["dependencies.lock"]["sha256"],
        "managed_components": managed_components,
        "submodules_sha256": hashlib.sha256(
            submodule_status.stdout.encode()
        ).hexdigest(),
        "sdkconfig_defaults_sha256": hashlib.sha256(defaults.read_bytes()).hexdigest(),
        "sdkconfig_sha256": hashlib.sha256(sdkconfig.read_bytes()).hexdigest(),
        "stdout_sha256": hashlib.sha256(stdout.encode()).hexdigest(),
        "stderr_sha256": hashlib.sha256(stderr.encode()).hexdigest(),
    }
    _json_write_atomic(provenance_path, provenance)
    return project, build, provenance


def _flash_argv(
    cap: Capability, project: Path, build: Path, port: str
) -> tuple[list[str], list[dict[str, str]]]:
    build = beneath(build, cap.evidence)
    if (
        not build.is_dir()
        or not (build / "flasher_args.json").is_file()
        or not (build / "project_description.json").is_file()
    ):
        raise BrokerError("trusted flash build is incomplete")
    description = json.loads(
        (build / "project_description.json").read_text(encoding="utf-8")
    )
    expected_description = {
        "git_revision": "v5.4.4",
        "project_path": "/src/firmware/domes",
        "build_dir": "/out/build",
        "target": "esp32s3",
        "project_name": "domes",
        "app_bin": "domes.bin",
    }
    if any(
        description.get(key) != value for key, value in expected_description.items()
    ):
        raise BrokerError("trusted flash build metadata is inconsistent")
    esptool = _trusted_path(cap, "esptool")
    arguments = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
    files = arguments.get("flash_files")
    settings = arguments.get("flash_settings", {})
    if not isinstance(files, dict) or not files:
        raise BrokerError("flash arguments contain no images")
    expected_generated_layout = {
        "0x0": "bootloader/bootloader.bin",
        "0x8000": "partition_table/partition-table.bin",
        "0x20000": "domes.bin",
        "0xf000": "ota_data_initial.bin",
    }
    # Flash only the standard application artifacts.  In particular, do not let
    # a candidate write NVS, PHY calibration, otadata, or arbitrary partitions.
    if files != expected_generated_layout:
        raise BrokerError("flash layout is not the standard DOMES application layout")
    expected_settings = {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "8MB"}
    if settings != expected_settings:
        raise BrokerError("flash settings are not the standard DOMES ESP32-S3 settings")
    argv = [
        esptool,
        "--chip",
        "esp32s3",
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "--port",
        port,
        "write_flash",
    ]
    for key in ("flash_mode", "flash_freq", "flash_size"):
        argv.extend([f"--{key}", expected_settings[key]])
    flash_layout = {
        key: expected_generated_layout[key] for key in ("0x0", "0x8000", "0x20000")
    }
    inputs: list[dict[str, str]] = []
    for offset, relative in sorted(
        flash_layout.items(), key=lambda item: int(item[0], 0)
    ):
        image = beneath(build / relative, build)
        staged, digest = _stage_input(cap, image)
        argv.extend([offset, str(staged)])
        inputs.append({"offset": offset, "artifact": relative, "sha256": digest})
    return argv, inputs


def _manifest_events(cap: Capability) -> list[dict[str, Any]]:
    """Return a verified, hash-chained controller manifest, or fail closed."""
    manifest = cap.evidence / "broker-manifest.jsonl"
    if not manifest.is_file():
        raise BrokerError("broker manifest is unavailable")
    previous = ""
    events: list[dict[str, Any]] = []
    try:
        lines = manifest.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise BrokerError("broker manifest is unreadable") from error
    if not lines:
        raise BrokerError("broker manifest is empty")
    for line in lines:
        try:
            event = json.loads(line)
        except json.JSONDecodeError as error:
            raise BrokerError("broker manifest chain is corrupt") from error
        if not isinstance(event, dict):
            raise BrokerError("broker manifest chain is corrupt")
        actual = event.get("event_sha256")
        unsigned = dict(event)
        unsigned.pop("event_sha256", None)
        expected = hashlib.sha256(
            json.dumps(unsigned, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        if (
            not isinstance(actual, str)
            or actual != expected
            or event.get("previous_event_sha256") != previous
            or (event.get("issue"), event.get("spec_revision"), event.get("pr_head"))
            != (cap.issue, cap.spec_revision, cap.pr_head)
        ):
            raise BrokerError("broker manifest chain is corrupt")
        previous = actual
        events.append(event)
    return events


def _selected_flash(
    cap: Capability, board: int
) -> tuple[str, str, str, dict[str, Any]]:
    """Find the latest successful, locally recorded flash for this board."""
    selected: tuple[str, str, str, dict[str, Any]] | None = None
    for event in _manifest_events(cap):
        if event.get("board") != board or event.get("operation") not in {
            "flash",
            "flash-trace-acceptance",
            "ota",
        }:
            continue
        selected = None  # Every later device mutation invalidates older provenance.
        if event.get("returncode") != 0 or event.get("error") is not None:
            continue
        head, provenance = event.get("artifact_head"), event.get("build_provenance")
        if not isinstance(head, str) or not re.fullmatch(r"[0-9a-f]{40}", head):
            continue
        if not isinstance(provenance, dict) or provenance.get("source_head") != head:
            continue
        profile = (
            "default"
            if event.get("operation") == "ota"
            else provenance.get("build_profile")
        )
        if profile not in {"default", "trace-acceptance"}:
            continue
        inputs = event.get("inputs")
        if not isinstance(inputs, list):
            continue
        image_hash = next(
            (
                item["sha256"]
                for item in inputs
                if (
                    isinstance(item, dict)
                    and item.get("artifact") == "domes.bin"
                    and isinstance(item.get("sha256"), str)
                    and re.fullmatch(r"[0-9a-f]{64}", item["sha256"])
                )
            ),
            None,
        )
        if image_hash is None:
            continue
        selected = (head, profile, image_hash, provenance)
    if selected is not None:
        return selected
    raise BrokerError("no successful board-local flash provenance is available")


def _bwrap(cap: Capability) -> str:
    return _trusted_path(cap, "bwrap")


def _system_ro_binds() -> list[str]:
    """Minimal host runtime required for a dynamically linked Rust binary."""
    argv: list[str] = []
    for path in ("/usr",):
        if Path(path).is_dir():
            argv.extend(["--ro-bind", path, path])
    for path, target in (
        ("/bin", "usr/bin"),
        ("/lib", "usr/lib"),
        ("/lib64", "usr/lib"),
    ):
        if Path(path).is_symlink():
            argv.extend(["--symlink", target, path])
        elif Path(path).is_dir():
            argv.extend(["--ro-bind", path, path])
    return argv


def _candidate_cli_build_argv(cap: Capability, source: Path, target: Path) -> list[str]:
    cargo = _trusted_path(cap, "cargo")
    if not Path(cargo).is_relative_to("/usr"):
        raise BrokerError("trusted cargo is outside the read-only system mount")
    cargo_home = Path(os.environ.get("CARGO_HOME", Path.home() / ".cargo"))
    rustup_home = Path(os.environ.get("RUSTUP_HOME", Path.home() / ".rustup"))
    cargo_registry = cargo_home / "registry"
    cargo_lock = source / "tools" / "domes-cli" / "Cargo.lock"
    if not cargo_registry.is_dir() or not rustup_home.is_dir():
        raise BrokerError("private Cargo and Rustup sources are unavailable")
    if not cargo_lock.is_file() or cargo_lock.is_symlink():
        raise BrokerError("candidate CLI Cargo.lock is unavailable")
    if 'source = "git+' in cargo_lock.read_text(encoding="utf-8"):
        raise BrokerError("candidate CLI git dependencies are not allowed")
    return [
        _bwrap(cap),
        "--die-with-parent",
        "--unshare-all",
        "--new-session",
        "--clearenv",
        "--ro-bind",
        str(source),
        "/src",
        "--dir",
        "/cargo",
        "--ro-bind",
        str(cargo_registry),
        "/cargo/registry",
        "--ro-bind",
        str(rustup_home),
        "/rustup",
        *_system_ro_binds(),
        "--bind",
        str(target),
        "/target",
        "--bind",
        str(_compiler_temp_directory(cap)),
        "/tmp",
        "--proc",
        "/proc",
        "--dev",
        "/dev",
        "--setenv",
        "CARGO_HOME",
        "/cargo",
        "--setenv",
        "RUSTUP_HOME",
        "/rustup",
        "--setenv",
        "CARGO_TARGET_DIR",
        "/target",
        "--setenv",
        "HOME",
        "/tmp",
        "--setenv",
        "PATH",
        "/usr/bin:/bin",
        "--chdir",
        "/src/tools/domes-cli",
        "--",
        cargo,
        "build",
        "--offline",
        "--locked",
    ]


def _candidate_source_tree(cap: Capability, source: Path, head: str) -> Path:
    """Materialize candidate code without making clone Git metadata visible."""
    sanitized = cap.evidence / f"candidate-cli-source-{head[:16]}"
    if sanitized.exists() or sanitized.is_symlink():
        _discard_incomplete_trusted_build(cap, (sanitized,))
    source_size = _directory_size(source, MAX_CAPABILITY_EVIDENCE_BYTES)
    ensure_capability_evidence_budget(cap, source_size)
    try:
        shutil.copytree(
            source, sanitized, symlinks=True, ignore=shutil.ignore_patterns(".git")
        )
    except OSError as error:
        raise BrokerError(
            "failed to prepare metadata-free candidate CLI source"
        ) from error
    if (sanitized / ".git").exists() or not (
        sanitized / "tools" / "domes-cli"
    ).is_dir():
        _discard_incomplete_trusted_build(cap, (sanitized,))
        raise BrokerError("candidate CLI source contains unsafe Git metadata")
    ensure_capability_evidence_budget(cap)
    return sanitized


def _candidate_trace_argv(
    cap: Capability,
    candidate: Path,
    pty_compat: Path,
    port: str,
    board: int,
    output: Path,
    names: Path,
    image: Path,
) -> list[str]:
    device = Path(port).resolve(strict=True)
    if not stat.S_ISCHR(device.stat().st_mode):
        raise BrokerError("registered port disappeared before trace sandbox")
    return [
        _bwrap(cap),
        "--die-with-parent",
        "--unshare-all",
        "--new-session",
        "--clearenv",
        *_system_ro_binds(),
        "--dir",
        "/candidate",
        "--ro-bind",
        str(candidate),
        "/candidate/domes-cli",
        "--ro-bind",
        str(pty_compat),
        "/candidate/serial-pty-compat.so",
        "--ro-bind",
        str(names),
        "/trace_names.json",
        "--ro-bind",
        str(image),
        "/domes.bin",
        "--bind",
        str(output),
        "/out",
        "--bind",
        str(_compiler_temp_directory(cap)),
        "/tmp",
        "--proc",
        "/proc",
        "--dev",
        "/dev",
        "--dev-bind",
        str(device),
        f"/dev/domes-board-{board}",
        "--setenv",
        "HOME",
        "/tmp",
        "--setenv",
        "PATH",
        "/usr/bin:/bin",
        "--setenv",
        "LD_PRELOAD",
        "/candidate/serial-pty-compat.so",
        "--chdir",
        "/out",
        "--",
        "/candidate/domes-cli",
        "--port",
        f"/dev/domes-board-{board}",
        "trace",
        "dump",
        "--output",
        "/out/trace.json",
        "--names",
        "/trace_names.json",
        "--firmware-bin",
        "/domes.bin",
    ]


def _sha256_file(path: Path) -> str:
    if not path.is_file() or path.is_symlink():
        raise BrokerError("trace sandbox output is missing or unsafe")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _inspect_candidate_image(image: Path) -> dict[str, str]:
    """Independently derive the identity fields embedded in an ESP app image."""
    data = image.read_bytes()
    image_header_size = 24
    segment_header_size = 8
    app_description_size = 256
    segment_data = image_header_size + segment_header_size
    minimum_size = segment_data + app_description_size + 32
    if (
        len(data) < minimum_size
        or data[0] != 0xE9
        or data[1] == 0
        or data[1] > 16
        or data[23] != 1
    ):
        raise BrokerError("candidate file is not a bounded ESP application image")
    first_segment_size = int.from_bytes(
        data[image_header_size + 4 : segment_data], "little"
    )
    first_segment_end = segment_data + first_segment_size
    if (
        first_segment_size < app_description_size
        or first_segment_end > len(data)
        or int.from_bytes(data[segment_data : segment_data + 4], "little")
        != 0xABCD_5432
    ):
        raise BrokerError("candidate ESP application image descriptor is invalid")
    version_bytes = data[segment_data + 16 : segment_data + 48]
    try:
        version_end = version_bytes.index(0)
        firmware_version = version_bytes[:version_end].decode("utf-8")
    except (ValueError, UnicodeDecodeError) as error:
        raise BrokerError("candidate firmware version is invalid") from error
    if not firmware_version:
        raise BrokerError("candidate firmware version is empty")
    appended_hash = data[-32:]
    if hashlib.sha256(data[:-32]).digest() != appended_hash:
        raise BrokerError("candidate ESP application image hash is invalid")
    app_elf_sha256 = data[segment_data + 144 : segment_data + 176]
    if not any(app_elf_sha256):
        raise BrokerError("candidate ESP application ELF hash is invalid")
    return {
        "file_sha256": hashlib.sha256(data).hexdigest(),
        "app_image_sha256": appended_hash.hex(),
        "app_elf_sha256": app_elf_sha256.hex(),
        "firmware_version": firmware_version,
    }


def _read_varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        if offset >= len(data):
            raise BrokerError("trace protobuf is truncated")
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if byte & 0x80 == 0:
            return value, offset
    raise BrokerError("trace protobuf varint is invalid")


def _protobuf_fields(payload: bytes) -> dict[int, list[tuple[int, int | bytes]]]:
    """Decode only generic protobuf wire fields; schema authority remains trace.proto."""
    fields: dict[int, list[tuple[int, int | bytes]]] = {}
    offset = 0
    while offset < len(payload):
        key, offset = _read_varint(payload, offset)
        number, wire_type = key >> 3, key & 0x07
        if number == 0:
            raise BrokerError("trace protobuf field number is invalid")
        if wire_type == 0:
            value, offset = _read_varint(payload, offset)
        elif wire_type == 1:
            if offset + 8 > len(payload):
                raise BrokerError("trace protobuf fixed64 field is truncated")
            value, offset = payload[offset : offset + 8], offset + 8
        elif wire_type == 2:
            size, offset = _read_varint(payload, offset)
            if offset + size > len(payload):
                raise BrokerError("trace protobuf bytes field is truncated")
            value, offset = payload[offset : offset + size], offset + size
        elif wire_type == 5:
            if offset + 4 > len(payload):
                raise BrokerError("trace protobuf fixed32 field is truncated")
            value, offset = payload[offset : offset + 4], offset + 4
        else:
            raise BrokerError("trace protobuf wire type is unsupported")
        fields.setdefault(number, []).append((wire_type, value))
    return fields


def _one_varint(
    fields: dict[int, list[tuple[int, int | bytes]]],
    number: int,
    *,
    default: int | None = None,
) -> int:
    values = fields.get(number, [])
    if not values and default is not None:
        return default
    if len(values) != 1 or values[0][0] != 0 or not isinstance(values[0][1], int):
        raise BrokerError("trace protobuf integer field is invalid")
    return values[0][1]


def _one_bytes(fields: dict[int, list[tuple[int, int | bytes]]], number: int) -> bytes:
    values = fields.get(number, [])
    if len(values) != 1 or values[0][0] != 2 or not isinstance(values[0][1], bytes):
        raise BrokerError("trace protobuf bytes field is invalid")
    return values[0][1]


def _decode_frames(data: bytes, *, allow_noise: bool) -> list[tuple[int, bytes]]:
    frames: list[tuple[int, bytes]] = []
    offset = 0
    while offset < len(data):
        start = data.find(b"\xaa\x55", offset)
        if start < 0:
            if not allow_noise and data[offset:]:
                raise BrokerError("trace relay contains non-frame request bytes")
            break
        if not allow_noise and start != offset:
            raise BrokerError("trace relay contains non-frame request bytes")
        if start + 4 > len(data):
            raise BrokerError("trace relay contains a truncated frame header")
        size = int.from_bytes(data[start + 2 : start + 4], "little")
        if size < 1 or size > 1025:
            if allow_noise:
                offset = start + 2
                continue
            raise BrokerError("trace relay contains an invalid frame length")
        end = start + 4 + size + 4
        if end > len(data):
            raise BrokerError("trace relay contains a truncated frame")
        body = data[start + 4 : start + 4 + size]
        crc = int.from_bytes(data[start + 4 + size : end], "little")
        if zlib.crc32(body) & 0xFFFF_FFFF != crc:
            if allow_noise:
                offset = start + 2
                continue
            raise BrokerError("trace relay contains an invalid frame CRC")
        frames.append((body[0], body[1:]))
        offset = end
    return frames


def _validate_trace_transcript(
    transcript: list[tuple[str, bytes]], raw: bytes
) -> tuple[dict[str, Any], bytes, dict[str, Any]]:
    """Prove the complete raw trace and identity came through the board relay."""
    directions = {
        direction: b"".join(data for item, data in transcript if item == direction)
        for direction in ("tx", "rx")
    }
    tx_frames = _decode_frames(directions["tx"], allow_noise=False)
    rx_frames = _decode_frames(directions["rx"], allow_noise=True)
    if tx_frames != [(0x12, b"")]:
        raise BrokerError("trace relay did not forward exactly one dump request")
    if (
        len(rx_frames) < 3
        or rx_frames[0][0] != 0x1A
        or rx_frames[-1][0] != 0x14
        or any(message_type != 0x13 for message_type, _ in rx_frames[1:-1])
    ):
        raise BrokerError("trace relay response sequence is invalid")

    session_fields = _protobuf_fields(rx_frames[0][1])
    event_count = _one_varint(session_fields, 2, default=0)
    dropped_count = _one_varint(session_fields, 3, default=0)
    format_version = _one_varint(session_fields, 9, default=0)
    discontinuity_count = _one_varint(session_fields, 11, default=0)
    try:
        firmware_version = _one_bytes(session_fields, 12).decode("utf-8")
    except UnicodeDecodeError as error:
        raise BrokerError("trace session firmware version is invalid") from error
    session_identity = {
        "event_count": event_count,
        "dropped_count": dropped_count,
        "format_version": format_version,
        "discontinuity_count": discontinuity_count,
        "firmware_version": firmware_version,
        "app_elf_sha256": _one_bytes(session_fields, 13).hex(),
        "app_image_sha256": _one_bytes(session_fields, 14).hex(),
        "device_uid": _one_bytes(session_fields, 15).hex(),
    }

    events = bytearray()
    next_offset = 0
    for message_type, payload in rx_frames[1:-1]:
        assert message_type == 0x13
        fields = _protobuf_fields(payload)
        chunk_offset = _one_varint(fields, 1, default=0)
        count = _one_varint(fields, 2, default=0)
        chunk = _one_bytes(fields, 3)
        if chunk_offset != next_offset or count == 0 or len(chunk) != count * 16:
            raise BrokerError("trace relay data chunk is invalid")
        events.extend(chunk)
        next_offset += count
    end_fields = _protobuf_fields(rx_frames[-1][1])
    total_events = _one_varint(end_fields, 1, default=0)
    checksum = _one_varint(end_fields, 2, default=0)
    if (
        not raw
        or bytes(events) != raw
        or next_offset != event_count
        or total_events != event_count
        or checksum != sum(raw) & 0xFFFF_FFFF
    ):
        raise BrokerError("trace relay raw event evidence is inconsistent")

    encoded = b"".join(
        (b"T" if direction == "tx" else b"R") + len(data).to_bytes(4, "little") + data
        for direction, data in transcript
    )
    digest = hashlib.sha256(encoded).hexdigest()
    relay = {
        "kind": "broker-pty-frame-filter-v1",
        "transcript_sha256": digest,
        "tx_frame_count": len(tx_frames),
        "rx_frame_count": len(rx_frames),
        "data_frame_count": len(rx_frames) - 2,
        "raw_bytes": len(raw),
        "event_count": event_count,
    }
    return relay, encoded, session_identity


def _validate_trace_output(
    output: Path,
    image_identity: dict[str, str],
    board: int,
    wire_identity: dict[str, Any],
) -> dict[str, str]:
    expected_names = {
        "trace.json",
        "trace.json.raw",
        "trace.json.raw.sha256",
        "trace.json.raw.session.json",
    }
    if {path.name for path in output.iterdir()} != expected_names:
        raise BrokerError("trace sandbox produced unexpected evidence files")
    trace = output / "trace.json"
    raw = output / "trace.json.raw"
    raw_hash = output / "trace.json.raw.sha256"
    session = output / "trace.json.raw.session.json"
    trace_sha256, raw_sha256 = _sha256_file(trace), _sha256_file(raw)
    _sha256_file(raw_hash)
    session_sha256 = _sha256_file(session)
    expected_hash_file = f"{raw_sha256}  /out/trace.json.raw\n"
    if raw_hash.read_text(encoding="utf-8") != expected_hash_file:
        raise BrokerError("trace raw hash evidence is inconsistent")
    try:
        rendered, evidence = json.loads(trace.read_text(encoding="utf-8")), json.loads(
            session.read_text(encoding="utf-8")
        )
    except (OSError, json.JSONDecodeError) as error:
        raise BrokerError("trace sandbox output is not valid JSON") from error
    if not isinstance(rendered, list) or not isinstance(evidence, dict):
        raise BrokerError("trace sandbox output has an invalid JSON shape")
    candidate = evidence.get("candidate_image")
    valid_digest = lambda value: isinstance(value, str) and re.fullmatch(
        r"[0-9a-f]{64}", value
    )
    if (
        evidence.get("integrity_error") is not None
        or evidence.get("raw_sha256") != raw_sha256
        or evidence.get("format_version") != 1
        or evidence.get("received_raw_bytes") != len(raw.read_bytes())
        or evidence.get("event_count") * 16 != len(raw.read_bytes())
        or evidence.get("dropped_count") != 0
        or evidence.get("discontinuity_count") != 0
        or evidence.get("event_count") != wire_identity["event_count"]
        or evidence.get("dropped_count") != wire_identity["dropped_count"]
        or evidence.get("discontinuity_count") != wire_identity["discontinuity_count"]
        or evidence.get("format_version") != wire_identity["format_version"]
        or evidence.get("app_elf_sha256") != wire_identity["app_elf_sha256"]
        or evidence.get("app_image_sha256") != wire_identity["app_image_sha256"]
        or evidence.get("firmware_version") != wire_identity["firmware_version"]
        or evidence.get("device_uid") != wire_identity["device_uid"]
        or not valid_digest(evidence.get("app_elf_sha256"))
        or not valid_digest(evidence.get("app_image_sha256"))
        or not isinstance(evidence.get("device_uid"), str)
        or not re.fullmatch(r"[0-9a-f]{12}", evidence["device_uid"])
        or evidence["device_uid"] in {"000000000000", "ffffffffffff"}
        or int(evidence["device_uid"][:2], 16) & 1 != 0
        or not isinstance(evidence.get("transport"), dict)
        or evidence["transport"].get("type") != "serial"
        or evidence["transport"].get("address") != f"/dev/domes-board-{board}"
        or not isinstance(evidence["transport"].get("device_name"), str)
        or not evidence["transport"]["device_name"]
        or not isinstance(candidate, dict)
        or candidate.get("binding_verified") is not True
        or candidate.get("path") != "/domes.bin"
        or any(candidate.get(key) != value for key, value in image_identity.items())
        or any(
            evidence.get(key) != value
            for key, value in image_identity.items()
            if key != "file_sha256"
        )
    ):
        raise BrokerError("trace evidence identity or candidate binding is invalid")
    return {
        "trace_sha256": trace_sha256,
        "raw_sha256": raw_sha256,
        "session_sha256": session_sha256,
    }


def _normalize_trace_artifacts(
    cap: Capability,
    candidate_source: Path,
    output: Path,
    trace_hashes: dict[str, str],
    device_uid: str,
    source_head: str,
) -> tuple[dict[str, Any], dict[str, dict[str, str]]]:
    """Run the judged normalizer privately and retain only content-addressed IDs."""
    normalizer = candidate_source / "tools" / "trace" / "trace_normalizer.py"
    trace_proto = candidate_source / "firmware" / "common" / "proto" / "trace.proto"
    if (
        not normalizer.is_file()
        or normalizer.is_symlink()
        or not trace_proto.is_file()
        or trace_proto.is_symlink()
    ):
        raise BrokerError("candidate trace normalizer inputs are unavailable")
    normalized = cap.evidence / (
        f"normalized-trace-{trace_hashes['raw_sha256'][:16]}-{secrets.token_hex(4)}"
    )
    normalized.mkdir(mode=0o700)
    python = _trusted_path(cap, "python3")
    if not Path(python).is_relative_to("/usr"):
        raise BrokerError("trusted normalizer Python is outside the system mount")
    argv = _resource_limited(
        cap,
        [
            _bwrap(cap),
            "--die-with-parent",
            "--unshare-all",
            "--new-session",
            "--clearenv",
            "--ro-bind",
            str(candidate_source),
            "/src",
            "--dir",
            "/input",
            "--ro-bind",
            str(output / "trace.json.raw"),
            "/input/trace.raw",
            "--ro-bind",
            str(output / "trace.json.raw.session.json"),
            "/input/session.json",
            *_system_ro_binds(),
            "--bind",
            str(normalized),
            "/out",
            "--bind",
            str(_compiler_temp_directory(cap)),
            "/tmp",
            "--proc",
            "/proc",
            "--dev",
            "/dev",
            "--setenv",
            "HOME",
            "/tmp",
            "--setenv",
            "PATH",
            "/usr/bin:/bin",
            "--setenv",
            "PYTHONNOUSERSITE",
            "1",
            "--chdir",
            "/out",
            "--",
            python,
            "/src/tools/trace/trace_normalizer.py",
            "--raw",
            "/input/trace.raw",
            "--session",
            "/input/session.json",
            "--output-prefix",
            "/out/trace",
        ],
    )
    returncode, stdout, stderr = _run_with_bounded_logs(
        cap,
        argv,
        f"trace-normalizer-{trace_hashes['raw_sha256'][:16]}",
        60,
    )
    if returncode or len(stdout) > 65536 or len(stderr) > 65536:
        raise BrokerError("sandboxed trace normalization failed")
    expected = {"trace.replay.json", "trace.semantic.json"}
    if {path.name for path in normalized.iterdir()} != expected:
        raise BrokerError("trace normalizer produced unexpected files")
    documents: dict[str, dict[str, Any]] = {}
    hashes: dict[str, str] = {}
    for kind in ("replay", "semantic"):
        path = normalized / f"trace.{kind}.json"
        if path.stat().st_size > 8 * 1024 * 1024:
            raise BrokerError("normalized trace artifact exceeds finite size")
        data = path.read_bytes()
        try:
            document = json.loads(data)
        except json.JSONDecodeError as error:
            raise BrokerError("normalized trace artifact is invalid JSON") from error
        if not isinstance(document, dict) or device_uid.encode() in data:
            raise BrokerError("normalized trace artifact leaks device identity")
        documents[kind] = document
        hashes[kind] = hashlib.sha256(data).hexdigest()
        os.chmod(path, 0o400)
    replay = documents["replay"]
    semantic = documents["semantic"]
    if (
        replay.get("artifact_kind") != "replay-normalized-trace"
        or replay.get("raw_sha256") != trace_hashes["raw_sha256"]
        or replay.get("dropped_count") != 0
        or replay.get("discontinuity_count") != 0
        or not isinstance(replay.get("events"), list)
        or len(replay["events"]) == 0
        or semantic.get("artifact_kind") != "cross-target-semantic-projection"
        or semantic.get("raw_sha256") != trace_hashes["raw_sha256"]
    ):
        raise BrokerError("normalized trace semantics are inconsistent")
    python_version = subprocess.run(
        [python, "--version"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=10,
    )
    if python_version.returncode or not python_version.stdout.strip():
        raise BrokerError("trusted normalizer Python version is unavailable")
    provenance: dict[str, Any] = {
        "kind": "controller-bwrap-trace-normalizer-v1",
        "source_head": source_head,
        "normalizer_sha256": _sha256_file(normalizer),
        "trace_proto_sha256": _sha256_file(trace_proto),
        "python_sha256": str((cap.tools or {})["python3"]["sha256"]),
        "python_version": python_version.stdout.strip(),
        "bwrap_sha256": str((cap.tools or {})["bwrap"]["sha256"]),
        "prlimit_sha256": str((cap.tools or {})["prlimit"]["sha256"]),
        "raw_sha256": trace_hashes["raw_sha256"],
        "session_sha256": trace_hashes["session_sha256"],
        "replay_sha256": hashes["replay"],
        "semantic_sha256": hashes["semantic"],
        "summary": {
            "event_count": len(replay["events"]),
            "causal_positions": replay.get("causal_positions"),
            "overhead_us": replay.get("overhead_us"),
            "normalized_sha256": replay.get("normalized_sha256"),
        },
    }
    artifacts = {
        kind: {
            "artifact_id": f"trace-{kind}-{digest[:16]}",
            "sha256": digest,
        }
        for kind, digest in hashes.items()
    }
    return provenance, artifacts


def _build_pty_compat(cap: Capability) -> tuple[Path, dict[str, str]]:
    """Build the fixed controller-owned PTY modem-control compatibility shim."""
    source = Path(__file__).resolve().with_name("serial_pty_compat.c")
    if not source.is_file() or source.is_symlink():
        raise BrokerError("controller PTY compatibility source is unavailable")
    source_sha256 = _sha256_file(source)
    output = cap.evidence / f"serial-pty-compat-{source_sha256[:16]}.so"
    temporary = output.with_suffix(f".{secrets.token_hex(4)}.tmp")
    cc = _trusted_path(cap, "cc")
    ensure_capability_evidence_budget(cap, 16 * 1024 * 1024)
    returncode, _stdout, _stderr = _run_with_bounded_logs(
        cap,
        _resource_limited(
            cap,
            [
                cc,
                "-shared",
                "-fPIC",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                str(source),
                "-o",
                str(temporary),
            ],
        ),
        "controller-pty-compat-build",
        30,
    )
    if returncode or not temporary.is_file() or temporary.is_symlink():
        temporary.unlink(missing_ok=True)
        raise BrokerError("controller PTY compatibility build failed")
    os.chmod(temporary, 0o500)
    if output.exists():
        if output.read_bytes() != temporary.read_bytes():
            temporary.unlink(missing_ok=True)
            raise BrokerError("controller PTY compatibility build is not reproducible")
        temporary.unlink()
    else:
        temporary.replace(output)
    ensure_capability_evidence_budget(cap)
    cc_version = subprocess.run(
        [cc, "--version"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=30,
    )
    if cc_version.returncode or not cc_version.stdout.strip():
        raise BrokerError("controller C compiler version is unavailable")
    return output, {
        "pty_compat_source_sha256": source_sha256,
        "pty_compat_binary_sha256": _sha256_file(output),
        "cc_sha256": str((cap.tools or {})["cc"]["sha256"]),
        "cc_version": cc_version.stdout.splitlines()[0],
    }


def _candidate_cli_provenance(
    cap: Capability,
    source: Path,
    head: str,
    candidate: Path,
    pty_compat_provenance: dict[str, str],
) -> dict[str, str]:
    lock = source / "tools" / "domes-cli" / "Cargo.lock"
    if not lock.is_file() or lock.is_symlink():
        raise BrokerError("candidate CLI Cargo.lock is unavailable")
    versions: dict[str, str] = {}
    for tool in ("cargo", "rustc"):
        path = _trusted_path(cap, tool)
        completed = subprocess.run(
            [path, "--version"],
            check=False,
            capture_output=True,
            text=True,
            shell=False,
            timeout=30,
        )
        value = completed.stdout.strip()
        if completed.returncode or not value:
            raise BrokerError(f"candidate CLI {tool} version is unavailable")
        versions[f"{tool}_version"] = value
    return {
        "source_head": head,
        "cargo_lock_sha256": _sha256_file(lock),
        "candidate_cli_sha256": _sha256_file(candidate),
        "bwrap_sha256": str((cap.tools or {})["bwrap"]["sha256"]),
        "cargo_sha256": str((cap.tools or {})["cargo"]["sha256"]),
        "prlimit_sha256": str((cap.tools or {})["prlimit"]["sha256"]),
        "rustc_sha256": str((cap.tools or {})["rustc"]["sha256"]),
        **pty_compat_provenance,
        **versions,
    }


def execute(cap: Capability, request: dict[str, Any]) -> dict[str, Any]:
    ensure_capability_evidence_budget(cap)
    operation, path = validate_request(cap, request)
    artifact_head = _workspace_head(cap)
    if operation == "artifact-hash":
        if path is None or not path.is_file():
            raise BrokerError("artifact-hash requires an evidence file")
        return {
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "artifact_head": artifact_head,
            "returncode": 0,
        }
    port = _verified_port(
        cap, request.get("board")
    )  # identity is checked immediately before action
    build_provenance: dict[str, Any] | None = None
    inputs: list[dict[str, str]] = []
    if operation in {"flash", "flash-trace-acceptance"}:
        build_profile = (
            "trace-acceptance" if operation == "flash-trace-acceptance" else "default"
        )
        project, build, build_provenance = _trusted_firmware_build(
            cap, artifact_head, build_profile
        )
        argv, inputs = _flash_argv(cap, project, build, port)
    elif operation == "ota":
        _project, build, build_provenance = _trusted_firmware_build(
            cap, artifact_head, "default"
        )
        image = beneath(build / "domes.bin", build)
        staged, digest = _stage_input(cap, image)
        inputs = [{"artifact": "domes.bin", "sha256": digest}]
        argv = [
            _cli_path(cap),
            "--port",
            port,
            "ota",
            "flash",
            str(staged),
            "--version",
            _ota_version(staged, _trusted_path(cap, "esptool")),
        ]
    elif operation == "trace-dump":
        selected_head, build_profile, recorded_image_sha256, selected_provenance = (
            _selected_flash(cap, request.get("board"))
        )
        if artifact_head != selected_head:
            raise BrokerError("ticket workspace is not the selected flashed artifact")
        source_project, build, build_provenance = _trusted_firmware_build(
            cap, selected_head, build_profile
        )
        # The rebuilt private provenance must retain the identity of the locally
        # recorded flash before its app image is made visible to candidate code.
        for key in ("source_head", "build_profile", "idf_revision", "sdkconfig_sha256"):
            if build_provenance.get(key) != selected_provenance.get(key):
                raise BrokerError("selected flash build provenance no longer matches")
        image = beneath(build / "domes.bin", build)
        staged_image, image_sha256 = _stage_input(cap, image)
        if image_sha256 != recorded_image_sha256:
            raise BrokerError("rebuilt image does not match selected board flash")
        image_identity = _inspect_candidate_image(staged_image)
        if image_identity["file_sha256"] != image_sha256:
            raise BrokerError("trusted candidate image identity is inconsistent")
        names = source_project.parents[1] / "tools" / "trace" / "trace_names.json"
        if not names.is_file() or names.is_symlink():
            raise BrokerError("checked-in trace names unavailable")
        names_sha256 = _sha256_file(names)
        output = cap.evidence / (
            f"trace-output-{cap.issue}-{int(time.time() * 1000)}-{secrets.token_hex(4)}"
        )
        output.mkdir(mode=0o700)
        candidate_source = _candidate_source_tree(
            cap, source_project.parents[1], selected_head
        )
        target = cap.evidence / f"candidate-cli-target-{selected_head[:16]}"
        if target.exists() or target.is_symlink():
            _discard_incomplete_trusted_build(cap, (target,))
        target.mkdir(mode=0o700)
        try:
            build_cli_returncode, _build_cli_stdout, _build_cli_stderr = (
                _run_with_bounded_logs(
                    cap,
                    _resource_limited(
                        cap, _candidate_cli_build_argv(cap, candidate_source, target)
                    ),
                    f"candidate-cli-build-{selected_head[:16]}",
                    600,
                )
            )
        except subprocess.TimeoutExpired as error:
            raise BrokerError(
                "sandboxed candidate domes-cli build timed out"
            ) from error
        if build_cli_returncode:
            raise BrokerError("sandboxed candidate domes-cli build failed")
        candidate = target / "debug" / "domes-cli"
        if not candidate.is_file() or candidate.is_symlink():
            raise BrokerError("sandboxed candidate domes-cli output is unsafe")
        pty_compat, pty_compat_provenance = _build_pty_compat(cap)
        candidate_cli_provenance = _candidate_cli_provenance(
            cap,
            candidate_source,
            selected_head,
            candidate,
            pty_compat_provenance,
        )
        inputs = [
            {"artifact": "domes.bin", "sha256": image_sha256},
            {"artifact": "trace_names.json", "sha256": names_sha256},
        ]
        result_selected_flash = {
            "artifact_head": selected_head,
            "build_profile": build_profile,
            "domes_bin_sha256": image_sha256,
        }
    elif operation in {"reset", "run"}:
        tool = _trusted_path(cap, "esptool")
        argv = [tool, "--chip", "esp32s3", "--port", port, "run"]
    else:
        commands = {
            "info": ["system", "info"],
            "health": ["system", "health"],
            "self-test": ["system", "self-test"],
            "memory": ["system", "memory"],
            "feature-list": ["feature", "list"],
            "trace-start": ["trace", "start"],
            "trace-stop": ["trace", "stop"],
            "trace-clear": ["trace", "clear"],
            "trace-status": ["trace", "status"],
        }
        argv = [_cli_path(cap), "--port", port, *commands[operation]]
    # Candidate staging/argv construction can take time; revalidate identity at
    # the final possible point before the trusted subprocess opens the UART.
    _verified_port(cap, request.get("board"))
    if _workspace_head(cap) != artifact_head:
        raise BrokerError("workspace artifact changed before hardware operation")
    try:
        if operation == "trace-dump":
            with SerialTraceProxy(port) as proxy:
                argv = _candidate_trace_argv(
                    cap,
                    candidate,
                    pty_compat,
                    proxy.slave_path,
                    request.get("board"),
                    output,
                    names,
                    staged_image,
                )
                returncode, stdout, stderr = _run_with_bounded_logs(
                    cap,
                    _resource_limited(cap, argv),
                    f"candidate-trace-{artifact_head[:16]}",
                    300,
                )
                completed = subprocess.CompletedProcess(
                    argv, returncode, stdout=stdout, stderr=stderr
                )
                transcript = proxy.transcript
        else:
            returncode, stdout, stderr = _run_with_bounded_logs(
                cap,
                _resource_limited(cap, argv),
                f"hardware-{operation}-board-{request.get('board')}",
                300,
            )
            completed = subprocess.CompletedProcess(
                argv,
                returncode,
                stdout=stdout,
                stderr=stderr,
            )
    except subprocess.TimeoutExpired as error:
        raise BrokerError("allowlisted hardware operation timed out") from error
    except SerialTraceProxyError as error:
        raise BrokerError("trace serial relay rejected the candidate") from error

    def redact(value: str) -> str:
        for snapshot in cap.snapshots:
            for private in (snapshot["link"], snapshot["target"], snapshot["serial"]):
                value = value.replace(
                    str(private), f"board-{request.get('board', '?')}"
                )
        return value

    if _workspace_head(cap) != artifact_head:
        raise BrokerError("workspace artifact changed during hardware operation")
    result: dict[str, Any] = {
        "returncode": completed.returncode,
        "artifact_head": artifact_head,
        "stdout": redact(completed.stdout[-8000:]),
        "stderr": redact(completed.stderr[-8000:]),
    }
    if build_provenance is not None:
        result["build_provenance"] = build_provenance
        result["inputs"] = inputs
    if operation == "trace-dump":
        raw = (output / "trace.json.raw").read_bytes()
        trace_relay, transcript_bytes, wire_identity = _validate_trace_transcript(
            transcript, raw
        )
        trace_hashes = _validate_trace_output(
            output, image_identity, request.get("board"), wire_identity
        )
        normalization, artifacts = _normalize_trace_artifacts(
            cap,
            candidate_source,
            output,
            trace_hashes,
            wire_identity["device_uid"],
            selected_head,
        )
        transcript_path = cap.evidence / (
            f"trace-relay-{trace_relay['transcript_sha256']}.bin"
        )
        if transcript_path.exists():
            if transcript_path.read_bytes() != transcript_bytes:
                raise BrokerError("trace relay transcript digest collision")
        else:
            ensure_capability_evidence_budget(cap, len(transcript_bytes))
            transcript_path.write_bytes(transcript_bytes)
            os.chmod(transcript_path, 0o600)
        result["artifact_id"] = f"trace-{trace_hashes['trace_sha256'][:16]}"
        result["sha256"] = trace_hashes["trace_sha256"]
        result["trace_hashes"] = trace_hashes
        result["selected_flash"] = result_selected_flash
        result["candidate_cli_provenance"] = candidate_cli_provenance
        result["trace_relay"] = trace_relay
        result["normalization"] = normalization
        result["trace_identity"] = {
            "firmware_version": wire_identity["firmware_version"],
            "app_elf_sha256": wire_identity["app_elf_sha256"],
            "app_image_sha256": wire_identity["app_image_sha256"],
            # The embedded ESP image digest and the SHA-256 of the complete
            # domes.bin file are different values.  Retain both so the
            # controller can bind this trace to the exact artifact it flashed.
            "candidate_file_sha256": image_identity["file_sha256"],
            "registered_device_match": True,
            "device_identity_run_sha256": hashlib.sha256(
                f"{cap.token}:{wire_identity['device_uid']}".encode()
            ).hexdigest(),
        }
        result["stdout"] = result["stdout"].replace(
            wire_identity["device_uid"], "<redacted-device-uid>"
        )
        result["stderr"] = result["stderr"].replace(
            wire_identity["device_uid"], "<redacted-device-uid>"
        )
        result["artifacts"] = artifacts
    ensure_capability_evidence_budget(cap)
    return result


def _append_manifest(
    cap: Capability, request: dict[str, Any], result: dict[str, Any]
) -> None:
    """Controller-owned append-only audit evidence; never exposed in capability."""
    manifest = cap.evidence / "broker-manifest.jsonl"
    event = {
        "time": time.time(),
        "issue": cap.issue,
        "spec_revision": cap.spec_revision,
        "pr_head": cap.pr_head,
        "operation": request.get("operation"),
        "board": request.get("board"),
        "artifact_head": result.get("artifact_head"),
        "build_provenance": result.get("build_provenance"),
        "inputs": result.get("inputs"),
        "board_identity_sha256": (
            hashlib.sha256(
                (
                    cap.token
                    + ":"
                    + json.dumps(
                        cap.snapshots[int(request["board"])],
                        sort_keys=True,
                        separators=(",", ":"),
                    )
                ).encode()
            ).hexdigest()
            if isinstance(request.get("board"), int)
            and not isinstance(request.get("board"), bool)
            and 0 <= int(request["board"]) < len(cap.snapshots)
            else None
        ),
        "returncode": result.get("returncode"),
        "error": result.get("error"),
        "stdout_sha256": hashlib.sha256(
            str(result.get("stdout", "")).encode()
        ).hexdigest(),
        "stderr_sha256": hashlib.sha256(
            str(result.get("stderr", "")).encode()
        ).hexdigest(),
        "artifact_id": result.get("artifact_id"),
        "artifact_sha256": result.get("sha256"),
        "trace_hashes": result.get("trace_hashes"),
        "selected_flash": result.get("selected_flash"),
        "candidate_cli_provenance": result.get("candidate_cli_provenance"),
        "trace_relay": result.get("trace_relay"),
        "normalization": result.get("normalization"),
        "trace_identity": result.get("trace_identity"),
    }
    previous = ""
    if manifest.is_file():
        try:
            previous = json.loads(
                manifest.read_text(encoding="utf-8").splitlines()[-1]
            ).get("event_sha256", "")
        except (IndexError, json.JSONDecodeError):
            raise BrokerError("broker manifest chain is corrupt")
    event["previous_event_sha256"] = previous
    event["event_sha256"] = hashlib.sha256(
        json.dumps(event, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
    encoded = json.dumps(event, sort_keys=True) + "\n"
    ensure_capability_evidence_budget(cap, len(encoded.encode("utf-8")))
    with manifest.open("a", encoding="utf-8") as stream:
        stream.write(encoded)
    ensure_capability_evidence_budget(cap)


def serve_queue(directory: Path, cap: Capability, *, once: bool = False) -> None:
    queue, results = directory / "requests", directory / "results"
    _json_write_atomic(directory / "ready.json", {"ready": True, "issue": cap.issue})
    processed = 0
    while True:
        for request_path in sorted(queue.glob("request-*.json")):
            claimed = request_path.with_suffix(".processing")
            try:
                request_path.replace(claimed)
            except FileNotFoundError:
                continue
            request_id = claimed.stem.removeprefix("request-")
            request: dict[str, Any] = {"operation": "invalid"}
            try:
                processed += 1
                if processed > MAX_REQUESTS:
                    raise BrokerError("hardware request quota exceeded")
                if claimed.stat().st_size > MAX_REQUEST_BYTES:
                    raise BrokerError("hardware request exceeds size limit")
                loaded = json.loads(claimed.read_text(encoding="utf-8"))
                if not isinstance(loaded, dict):
                    raise TypeError("hardware request must be an object")
                request = loaded
                result = execute(cap, request)
            except (
                BrokerError,
                json.JSONDecodeError,
                OSError,
                TypeError,
                ValueError,
            ) as error:
                operation = request.get("operation")
                request = (
                    {
                        "operation": operation,
                        "board": request.get("board"),
                    }
                    if operation in OPERATIONS
                    else {"operation": "invalid"}
                )
                try:
                    failed_head = _workspace_head(cap)
                except BrokerError:
                    failed_head = None
                result = {
                    "error": str(error),
                    "returncode": 1,
                    "artifact_head": failed_head,
                }
            _append_manifest(cap, request, result)
            _json_write_atomic(results / f"result-{request_id}.json", result)
            claimed.unlink(missing_ok=True)
        if once:
            return
        time.sleep(0.05)


def main(argv: list[str] | None = None) -> int:
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    parser = argparse.ArgumentParser()
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("capability_directory", type=Path)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args(raw_argv)
    if not args.serve:
        parser.error("--serve is required")
    try:
        private = json.loads(sys.stdin.read())
    except json.JSONDecodeError as error:
        raise SystemExit("invalid private broker bootstrap") from error
    serve_queue(
        args.capability_directory, load_private_capability(private), once=args.once
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
