#!/usr/bin/env python3
"""Report DOMES development and hardware-verification capabilities."""

from __future__ import annotations

import argparse
import grp
import json
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Protocol, Sequence

SCHEMA_VERSION = 1
ROOT = Path(__file__).resolve().parents[2]
EXPECTED_IDF = "5.4.4"


class CommandRunner(Protocol):
    def run(
        self, command: Sequence[str], *, cwd: Path | None = None
    ) -> subprocess.CompletedProcess[str]: ...

    def which(self, command: str) -> str | None: ...


class SubprocessRunner:
    def run(
        self, command: Sequence[str], *, cwd: Path | None = None
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            list(command),
            cwd=cwd,
            check=False,
            capture_output=True,
            text=True,
            stdin=subprocess.DEVNULL,
            timeout=15,
        )

    def which(self, command: str) -> str | None:
        return shutil.which(command)


@dataclass(frozen=True)
class ToolSpec:
    identifier: str
    command: tuple[str, ...]
    pattern: str
    expected: str | None
    remediation: str


TOOL_SPECS = (
    ToolSpec(
        "git", ("git", "--version"), r"git version ([^\s]+)", None, "Install Git."
    ),
    ToolSpec(
        "python",
        ("python3", "--version"),
        r"Python ([^\s]+)",
        None,
        "Install Python 3.",
    ),
    ToolSpec(
        "cmake",
        ("cmake", "--version"),
        r"cmake version ([^\s]+)",
        None,
        "Install CMake.",
    ),
    ToolSpec(
        "ninja",
        ("ninja", "--version"),
        r"^([^\s]+)",
        None,
        "Install Ninja for ESP-IDF and CMake builds.",
    ),
    ToolSpec(
        "pkg_config",
        ("pkg-config", "--version"),
        r"^([^\s]+)",
        None,
        "Install pkg-config for host CLI dependencies.",
    ),
    ToolSpec(
        "cxx",
        ("c++", "--version"),
        r"(?:c\+\+|g\+\+|clang)\D*([0-9]+(?:\.[0-9]+)+)",
        None,
        "Install a C++20 compiler.",
    ),
    ToolSpec(
        "rustc",
        ("rustc", "--version"),
        r"rustc ([^\s]+)",
        "1.92.0",
        "Install Rust 1.92.0, matching repository CI.",
    ),
    ToolSpec(
        "cargo",
        ("cargo", "--version"),
        r"cargo ([^\s]+)",
        None,
        "Install Cargo with the pinned Rust toolchain.",
    ),
    ToolSpec(
        "flutter",
        ("flutter", "--version"),
        r"Flutter ([^\s]+)",
        "3.44.8",
        "Install Flutter 3.44.8, matching repository CI.",
    ),
    ToolSpec(
        "dart",
        ("dart", "--version"),
        r"Dart SDK version: ([^\s]+)",
        None,
        "Use the Dart SDK bundled with Flutter 3.44.8.",
    ),
    ToolSpec(
        "protoc",
        ("protoc", "--version"),
        r"libprotoc ([^\s]+)",
        None,
        "Install protoc.",
    ),
    ToolSpec(
        "protoc_plugin",
        ("dart", "pub", "global", "list"),
        r"(?m)^protoc_plugin ([^\s]+)$",
        "25.0.0",
        "Activate Dart protoc_plugin 25.0.0 with the pinned Dart SDK.",
    ),
    ToolSpec(
        "pre_commit",
        ("pre-commit", "--version"),
        r"pre-commit ([^\s]+)",
        "4.6.1",
        "Install pre-commit 4.6.1.",
    ),
    ToolSpec(
        "go",
        ("go", "version"),
        r"go version go([^\s]+)",
        None,
        "Install Go for actionlint.",
    ),
    ToolSpec(
        "shellcheck",
        ("shellcheck", "--version"),
        r"(?m)^version:\s*([^\s]+)$",
        None,
        "Install ShellCheck.",
    ),
)


def parse_version(output: str, pattern: str) -> str | None:
    match = re.search(pattern, output)
    return match.group(1) if match else None


def probe_tool(spec: ToolSpec, runner: CommandRunner) -> dict[str, Any]:
    executable = spec.command[0]
    if runner.which(executable) is None:
        return {
            "id": spec.identifier,
            "required": True,
            "status": "unavailable",
            "version": None,
            "expected": spec.expected,
            "detail": f"{executable} is not on PATH",
            "remediation": spec.remediation,
        }
    try:
        process = runner.run(spec.command)
    except (OSError, subprocess.TimeoutExpired) as error:
        return {
            "id": spec.identifier,
            "required": True,
            "status": "failed",
            "version": None,
            "expected": spec.expected,
            "detail": str(error),
            "remediation": spec.remediation,
        }
    output = "\n".join((process.stdout, process.stderr)).strip()
    version = parse_version(output, spec.pattern)
    if process.returncode != 0 or version is None:
        status = "failed"
        detail = output.splitlines()[0] if output else "version command failed"
    elif spec.expected is not None and version != spec.expected:
        status = "failed"
        detail = f"expected {spec.expected}, found {version}"
    else:
        status = "available"
        detail = f"version {version}"
    return {
        "id": spec.identifier,
        "required": True,
        "status": status,
        "version": version,
        "expected": spec.expected,
        "detail": detail,
        "remediation": None if status == "available" else spec.remediation,
    }


def probe_idf(runner: CommandRunner, environment: Mapping[str, str]) -> dict[str, Any]:
    configured = environment.get("IDF_EXPORT_SCRIPT")
    export_script = (
        Path(configured) if configured else Path.home() / "esp/esp-idf/export.sh"
    )
    if not export_script.is_file():
        return {
            "id": "esp_idf",
            "required": True,
            "status": "unavailable",
            "version": None,
            "expected": EXPECTED_IDF,
            "detail": f"export script not found: {export_script}",
            "remediation": "Install ESP-IDF v5.4.4 or set IDF_EXPORT_SCRIPT.",
        }
    command = (
        "bash",
        "-c",
        'source "$1" >/dev/null 2>&1 && idf.py --version',
        "domes-doctor",
        str(export_script),
    )
    try:
        process = runner.run(command)
    except (OSError, subprocess.TimeoutExpired) as error:
        process = subprocess.CompletedProcess(command, 1, "", str(error))
    output = "\n".join((process.stdout, process.stderr)).strip()
    version = parse_version(output, r"ESP-IDF v([^\s]+)")
    if process.returncode != 0 or version is None:
        status = "failed"
        detail = output.splitlines()[0] if output else "ESP-IDF export failed"
    elif version != EXPECTED_IDF:
        status = "failed"
        detail = f"expected {EXPECTED_IDF}, found {version}"
    else:
        status = "available"
        detail = f"version {version}"
    return {
        "id": "esp_idf",
        "required": True,
        "status": status,
        "version": version,
        "expected": EXPECTED_IDF,
        "detail": detail,
        "remediation": (
            None
            if status == "available"
            else "Install and export ESP-IDF v5.4.4; see docs/TESTING.md."
        ),
    }


def probe_repository(runner: CommandRunner, root: Path) -> dict[str, Any]:
    try:
        process = runner.run(("git", "submodule", "status", "--recursive"), cwd=root)
    except (OSError, subprocess.TimeoutExpired) as error:
        return {
            "root": str(root),
            "status": "failed",
            "submodules": [],
            "detail": str(error),
            "remediation": "Run the doctor from a Git checkout.",
        }
    submodules = []
    for line in process.stdout.splitlines():
        if not line:
            continue
        prefix = line[0]
        parts = line[1:].strip().split()
        submodules.append(
            {
                "path": parts[1] if len(parts) > 1 else "unknown",
                "revision": parts[0] if parts else None,
                "status": {
                    " ": "available",
                    "-": "unavailable",
                    "+": "failed",
                    "U": "failed",
                }.get(prefix, "failed"),
            }
        )
    statuses = {item["status"] for item in submodules}
    if process.returncode != 0 or "failed" in statuses:
        status = "failed"
    elif "unavailable" in statuses:
        status = "unavailable"
    else:
        status = "available"
    return {
        "root": str(root),
        "status": status,
        "submodules": submodules,
        "detail": (
            "submodules initialized at recorded revisions"
            if status == "available"
            else (
                "one or more submodules are missing or differ from the recorded "
                "revision"
            )
        ),
        "remediation": (
            None
            if status == "available"
            else (
                "Inspect submodule changes, then initialize the intended recorded "
                "revisions."
            )
        ),
    }


def _device_record(path: Path) -> dict[str, Any]:
    try:
        metadata = path.stat()
        group = grp.getgrgid(metadata.st_gid).gr_name
        target = str(path.resolve(strict=True))
        kind = "character" if stat.S_ISCHR(metadata.st_mode) else "fixture"
        readable = os.access(path, os.R_OK)
        writable = os.access(path, os.W_OK)
        return {
            "path": str(path),
            "target": target,
            "kind": kind,
            "group": group,
            "readable": readable,
            "writable": writable,
            "status": "available" if readable and writable else "failed",
        }
    except (OSError, KeyError) as error:
        return {
            "path": str(path),
            "target": None,
            "kind": "unknown",
            "group": None,
            "readable": False,
            "writable": False,
            "status": "failed",
            "detail": str(error),
        }


def detect_serial_devices(dev_root: Path = Path("/dev")) -> dict[str, Any]:
    by_id = dev_root / "serial/by-id"
    if not by_id.is_dir():
        return {
            "stable_by_id": "unavailable",
            "cp2102n": [],
            "native_usb": [],
            "detected_pod_count": 0,
            "remediation": (
                "Attach the device and ensure /dev/serial/by-id links are available."
            ),
        }
    cp2102n = sorted(by_id.glob("usb-Silicon_Labs_CP2102N*"))
    native = sorted(by_id.glob("usb-Espressif_USB_JTAG_serial_debug_unit*"))
    cp_records = [_device_record(path) for path in cp2102n]
    return {
        "stable_by_id": "available",
        "cp2102n": cp_records,
        "native_usb": [_device_record(path) for path in native],
        "detected_pod_count": len(cp_records),
        "remediation": (
            None
            if cp_records
            else (
                "Attach an NFF CP2102N bridge; do not use ttyUSB numbering as "
                "identity."
            )
        ),
    }


def probe_bluetooth(runner: CommandRunner, native_linux: bool) -> dict[str, Any]:
    if not native_linux:
        return {
            "status": "not_applicable",
            "bluez_version": None,
            "adapter_count": 0,
            "powered": False,
            "detail": "validation-critical BLE requires native Linux",
            "remediation": "Use a supported native Linux or mobile host.",
        }
    if runner.which("bluetoothctl") is None:
        return {
            "status": "unavailable",
            "bluez_version": None,
            "adapter_count": 0,
            "powered": False,
            "detail": "bluetoothctl is not on PATH",
            "remediation": "Install BlueZ and provide a supported Bluetooth adapter.",
        }
    try:
        version_process = runner.run(("bluetoothctl", "--version"))
        list_process = runner.run(("bluetoothctl", "list"))
        show_process = runner.run(("bluetoothctl", "show"))
    except (OSError, subprocess.TimeoutExpired) as error:
        return {
            "status": "failed",
            "bluez_version": None,
            "adapter_count": 0,
            "powered": False,
            "detail": str(error),
            "remediation": (
                "Inspect BlueZ and adapter state; the doctor will not restart "
                "services."
            ),
        }
    version = parse_version(
        version_process.stdout + version_process.stderr,
        r"bluetoothctl:\s*([^\s]+)",
    )
    adapters = [
        line
        for line in list_process.stdout.splitlines()
        if line.startswith("Controller ")
    ]
    powered = bool(re.search(r"(?m)^\s*Powered:\s*yes\s*$", show_process.stdout))
    command_failed = any(
        process.returncode != 0
        for process in (version_process, list_process, show_process)
    )
    if command_failed:
        status = "failed"
        detail = "BlueZ query failed"
    elif not adapters:
        status = "unavailable"
        detail = "no Bluetooth adapter detected"
    elif not powered:
        status = "unavailable"
        detail = "Bluetooth adapter is not powered"
    else:
        status = "available"
        detail = f"{len(adapters)} powered adapter(s)"
    return {
        "status": status,
        "bluez_version": version,
        "adapter_count": len(adapters),
        "powered": powered,
        "detail": detail,
        "remediation": (
            None
            if status == "available"
            else "Inspect BlueZ and adapter state; the doctor will not change either."
        ),
    }


def _capability(
    identifier: str,
    requirements: list[str],
    evidence: list[str],
    remediation: str,
    *,
    applicable: bool = True,
) -> dict[str, Any]:
    if not applicable:
        status = "not_applicable"
    elif "failed" in requirements:
        status = "failed"
    elif all(requirement == "available" for requirement in requirements):
        status = "available"
    else:
        status = "unavailable"
    return {
        "id": identifier,
        "status": status,
        "evidence": evidence,
        "remediation": None if status == "available" else remediation,
    }


def build_capabilities(report: dict[str, Any]) -> list[dict[str, Any]]:
    tools = {tool["id"]: tool["status"] for tool in report["tools"]}
    devices = report["devices"]
    usable_cp = sum(item["status"] == "available" for item in devices["cp2102n"])
    usable_console = sum(
        item["status"] == "available" for item in devices["native_usb"]
    )
    native_linux = report["host"]["native_linux"]
    ble_status = report["bluetooth"]["status"]

    def combined_tool_status(names: tuple[str, ...]) -> str:
        statuses = [tools.get(name, "unavailable") for name in names]
        if "failed" in statuses:
            return "failed"
        if all(status == "available" for status in statuses):
            return "available"
        return "unavailable"

    firmware_status = combined_tool_status(
        ("python", "cmake", "ninja", "cxx", "esp_idf")
    )
    all_tools_status = combined_tool_status(tuple(tools))
    cp_one = "available" if usable_cp >= 1 else "unavailable"
    cp_two = "available" if usable_cp >= 2 else "unavailable"
    console_two = "available" if usable_console >= 2 else "unavailable"
    architecture = (
        "available"
        if report["host"]["architecture"].casefold() in {"x86_64", "amd64"}
        else "unavailable"
    )
    return [
        _capability(
            "software_verification",
            [report["repository"]["status"], all_tools_status],
            ["repository state", "all required toolchains"],
            "Resolve mandatory software failures shown above.",
        ),
        _capability(
            "single_device",
            [firmware_status, cp_one],
            [f"usable CP2102N ports: {usable_cp}"],
            "Provide one writable CP2102N by-id port and the pinned firmware "
            "toolchain.",
            applicable=native_linux,
        ),
        _capability(
            "two_device",
            [firmware_status, cp_two],
            [f"usable CP2102N ports: {usable_cp}"],
            "Provide two writable CP2102N by-id ports.",
            applicable=native_linux,
        ),
        _capability(
            "ble",
            [ble_status, cp_one],
            [report["bluetooth"]["detail"], f"detected pods: {usable_cp}"],
            "Provide native Linux BlueZ, a powered supported adapter, and a pod.",
            applicable=native_linux,
        ),
        _capability(
            "esp_now",
            [firmware_status, cp_two, console_two],
            [
                f"usable CP2102N ports: {usable_cp}",
                f"usable native USB consoles: {usable_console}",
            ],
            "Provide two pods with CP2102N and separate native USB console links.",
            applicable=native_linux,
        ),
        _capability(
            "ota",
            [
                firmware_status,
                (
                    "available"
                    if usable_cp >= 1 or ble_status == "available"
                    else "unavailable"
                ),
            ],
            [
                f"serial targets: {usable_cp}",
                f"BLE host: {ble_status == 'available'}",
            ],
            "Provide the pinned firmware toolchain and one supported serial or "
            "BLE target.",
            applicable=native_linux,
        ),
        _capability(
            "hardware_ci",
            [
                architecture,
                firmware_status,
                tools.get("rustc", "unavailable"),
                cp_two,
                ble_status,
            ],
            ["native Linux x64", "two pods", "BLE", "pinned firmware and Rust"],
            "Provision the documented self-hosted runner; this tool will not "
            "register it.",
            applicable=native_linux,
        ),
    ]


def collect_report(
    *,
    runner: CommandRunner | None = None,
    root: Path = ROOT,
    dev_root: Path = Path("/dev"),
    environment: Mapping[str, str] | None = None,
) -> dict[str, Any]:
    command_runner = runner or SubprocessRunner()
    env = environment if environment is not None else os.environ
    system = platform.system()
    release = platform.release().casefold()
    native_linux = (
        system == "Linux" and "microsoft" not in release and "wsl" not in release
    )
    tools = [probe_tool(spec, command_runner) for spec in TOOL_SPECS]
    tools.append(probe_idf(command_runner, env))
    report = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repository": probe_repository(command_runner, root),
        "host": {
            "system": system,
            "release": platform.release(),
            "architecture": platform.machine(),
            "native_linux": native_linux,
        },
        "tools": tools,
        "devices": detect_serial_devices(dev_root),
        "bluetooth": probe_bluetooth(command_runner, native_linux),
    }
    report["capabilities"] = build_capabilities(report)
    mandatory_failures = sum(tool["status"] != "available" for tool in tools)
    if report["repository"]["status"] != "available":
        mandatory_failures += 1
    report["summary"] = {
        "mandatory_failures": mandatory_failures,
        "available_capabilities": sum(
            item["status"] == "available" for item in report["capabilities"]
        ),
        "exit_code": 0 if mandatory_failures == 0 else 1,
    }
    return report


def render_text(report: dict[str, Any]) -> str:
    lines = ["DOMES capability doctor", ""]
    repository = report["repository"]
    submodule_count = len(repository["submodules"])
    lines.append(f"Repository: {repository['status']} ({submodule_count} submodule(s))")
    lines.append(
        f"Host: {report['host']['system']} {report['host']['architecture']} "
        f"(native Linux: {'yes' if report['host']['native_linux'] else 'no'})"
    )
    lines.extend(("", "Required software:"))
    for tool in report["tools"]:
        version = f" {tool['version']}" if tool["version"] else ""
        lines.append(f"  {tool['status']:<11} {tool['id']}{version}")
        if tool["status"] != "available":
            lines.append(f"    {tool['detail']}; {tool['remediation']}")
    devices = report["devices"]
    lines.extend(
        (
            "",
            "Hardware discovery (stable by-id only):",
            f"  CP2102N pod ports: {len(devices['cp2102n'])}",
            f"  Native USB console/JTAG: {len(devices['native_usb'])}",
            f"  Bluetooth: {report['bluetooth']['status']} "
            f"({report['bluetooth']['detail']})",
        )
    )
    for kind in ("cp2102n", "native_usb"):
        for device in devices[kind]:
            access = "rw" if device["readable"] and device["writable"] else "denied"
            lines.append(
                f"    {device['path']} (group={device['group']}, access={access})"
            )
    lines.extend(("", "Verification capabilities:"))
    for capability in report["capabilities"]:
        lines.append(f"  {capability['status']:<14} {capability['id']}")
    failures = report["summary"]["mandatory_failures"]
    lines.extend(("", f"Mandatory software/repository failures: {failures}"))
    return "\n".join(lines) + "\n"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--json", action="store_true", help="emit schema-versioned JSON"
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    report = collect_report()
    if args.json:
        json.dump(report, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        sys.stdout.write(render_text(report))
    return report["summary"]["exit_code"]


if __name__ == "__main__":
    sys.exit(main())
