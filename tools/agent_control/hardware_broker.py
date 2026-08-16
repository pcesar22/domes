#!/usr/bin/env python3
"""Host-side, fail-closed broker for the two registered DOMES NFFs.

The public capability is deliberately useless for opening a device: it contains
only a random token and a queue path.  The private capability (including ports
and identity snapshots) stays with this host process.
"""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
import re
import secrets
import shutil
import stat
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

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

    def document(self) -> dict[str, Any]:
        # No workspace/evidence paths either: the worker already knows its workspace;
        # keeping this public capability minimal prevents path/port leakage.
        return {
            "version": 1,
            "issue": self.issue,
            "spec_revision": self.spec_revision,
            "pr_head": self.pr_head,
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
    trusted_tools: dict[str, dict[str, str]] | None = None,
) -> Capability:
    allowed = tuple(sorted(set(operations)))
    if not allowed or not set(allowed).issubset(OPERATIONS):
        raise BrokerError("capability operations are not an allowlisted finite subset")
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


def _cli_path(cap: Capability) -> str:
    # The controller's already-built CLI is trusted host tooling.  A candidate
    # worktree may supply firmware *data*, never executable host code.
    return _trusted_path(cap, "domes-cli")


def _workspace_head(cap: Capability) -> str:
    """Bind physical activity to a committed, tracked-clean candidate artifact."""
    resolved = subprocess.run(
        ["git", "-C", str(cap.workspace), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    head = resolved.stdout.strip()
    if resolved.returncode or not re.fullmatch(r"[0-9a-f]{40}", head):
        raise BrokerError("hardware operation requires a committed workspace HEAD")
    clean = subprocess.run(
        ["git", "-C", str(cap.workspace), "diff", "--quiet", "--exit-code"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    cached = subprocess.run(
        ["git", "-C", str(cap.workspace), "diff", "--cached", "--quiet", "--exit-code"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    if clean.returncode or cached.returncode:
        raise BrokerError("hardware operation requires all tracked changes committed")
    return head


def _stage_input(cap: Capability, source: Path) -> tuple[Path, str]:
    """Copy immutable candidate data before trusted tooling consumes it."""
    if not source.is_file():
        raise BrokerError("candidate input is not a file")
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
    target.write_text("\n".join(lines) + "\n", encoding="utf-8")
    os.chmod(target, 0o600)
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
    provenance_path = cap.evidence / f"build-{suffix}.json"
    git = _trusted_path(cap, "git")
    idf_export = _trusted_path(cap, "idf-export")
    idf_py = _trusted_path(cap, "idf.py")
    idf_record = (cap.tools or {}).get("idf.py", {})
    idf_root = Path(idf_export).parent
    idf_revision = subprocess.run(
        [git, "-C", str(idf_root), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    idf_tag = subprocess.run(
        [git, "-C", str(idf_root), "describe", "--tags", "--exact-match", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
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
        resolved = subprocess.run(
            [git, "-C", str(source), "rev-parse", "HEAD"],
            check=False,
            capture_output=True,
            text=True,
            shell=False,
        )
        submodule_status = subprocess.run(
            [git, "-C", str(source), "submodule", "status", "--recursive"],
            check=False,
            capture_output=True,
            text=True,
            shell=False,
        )
        if (
            resolved.returncode
            or resolved.stdout.strip() != artifact_head
            or submodule_status.returncode
            or provenance.get("build_profile") != build_profile
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
        return project, build, provenance
    if source.exists() or build.exists():
        raise BrokerError("trusted build directory already exists without provenance")
    cloned = subprocess.run(
        [git, "clone", "--shared", "--no-checkout", str(cap.workspace), str(source)],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=120,
    )
    if cloned.returncode:
        raise BrokerError("failed to create controller-owned firmware source clone")
    checked_out = subprocess.run(
        [git, "-C", str(source), "checkout", "--detach", artifact_head],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=120,
    )
    if checked_out.returncode or not project.is_dir():
        raise BrokerError("failed to check out committed firmware source")
    submodules = subprocess.run(
        [git, "-C", str(source), "submodule", "update", "--init", "--recursive"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=300,
    )
    if submodules.returncode:
        raise BrokerError("failed to check out committed firmware submodules")
    submodule_status = subprocess.run(
        [git, "-C", str(source), "submodule", "status", "--recursive"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    if submodule_status.returncode or any(
        line and not line.startswith(" ")
        for line in submodule_status.stdout.splitlines()
    ):
        raise BrokerError("trusted firmware submodule state is not exact")
    defaults = _write_profile_defaults(cap, project, suffix, build_profile)
    compiler_tmp = cap.evidence / "tmp"
    compiler_tmp.mkdir(mode=0o700)
    build_environment = dict(os.environ)
    build_environment["TMPDIR"] = str(compiler_tmp)
    script = (
        'set -euo pipefail; source "$1" >/dev/null; '
        'exec "$2" -B "$3" -DSDKCONFIG="$4" -DSDKCONFIG_DEFAULTS="$5" '
        "-DCCACHE_ENABLE=0 build"
    )
    completed = subprocess.run(
        [
            "/usr/bin/bash",
            "-c",
            script,
            "domes-broker-build",
            idf_export,
            idf_py,
            str(build),
            str(sdkconfig),
            str(defaults),
        ],
        cwd=project,
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=1200,
        env=build_environment,
    )
    stdout = completed.stdout
    stderr = completed.stderr
    (cap.evidence / "trusted-build.stdout.log").write_text(stdout, encoding="utf-8")
    (cap.evidence / "trusted-build.stderr.log").write_text(stderr, encoding="utf-8")
    if completed.returncode:
        raise BrokerError("controller-owned ESP-IDF v5.4.4 firmware build failed")
    if not _profile_build_matches(build, build_profile):
        raise BrokerError("controller-owned firmware build profile is inconsistent")
    resolved = subprocess.run(
        [git, "-C", str(source), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    tracked = subprocess.run(
        [git, "-C", str(source), "diff", "--quiet", "--exit-code"],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
    )
    if (
        resolved.returncode
        or resolved.stdout.strip() != artifact_head
        or tracked.returncode
    ):
        raise BrokerError("trusted firmware source changed during build")
    provenance = {
        "kind": "controller-clean-clone-idf-build",
        "source_head": artifact_head,
        "build_profile": build_profile,
        "idf_version": "v5.4.4",
        "idf_revision": str(idf_record["revision"]),
        "idf_export_sha256": (cap.tools or {})["idf-export"]["sha256"],
        "idf_py_sha256": (cap.tools or {})["idf.py"]["sha256"],
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
        "project_path": str(project),
        "build_dir": str(build),
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


def execute(cap: Capability, request: dict[str, Any]) -> dict[str, Any]:
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
        output = (
            cap.evidence
            / f"trace-{cap.issue}-{int(time.time()*1000)}-{secrets.token_hex(4)}.json"
        )
        names = cap.workspace / "tools/trace/trace_names.json"
        if not names.is_file():
            raise BrokerError("checked-in trace names unavailable")
        argv = [
            _cli_path(cap),
            "--port",
            port,
            "trace",
            "dump",
            "--output",
            str(output),
            "--names",
            str(names),
        ]
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
        completed = subprocess.run(
            argv, check=False, capture_output=True, text=True, shell=False, timeout=300
        )
    except subprocess.TimeoutExpired as error:
        raise BrokerError("allowlisted hardware operation timed out") from error

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
        output = Path(argv[argv.index("--output") + 1])
        if output.is_file():
            digest = hashlib.sha256(output.read_bytes()).hexdigest()
            result["artifact_id"] = f"trace-{digest[:16]}"
            result["sha256"] = digest
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
        "snapshots": cap.snapshots,
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
    with manifest.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(event, sort_keys=True) + "\n")


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
    parser = argparse.ArgumentParser()
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("capability_directory", type=Path)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args(argv)
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
