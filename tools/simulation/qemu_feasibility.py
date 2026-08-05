#!/usr/bin/env python3
"""Build and repeatedly execute the bounded ESP32-S3 QEMU feasibility probe."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import re
import selectors
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from statistics import median
from typing import Any, Mapping, Sequence

EXPECTED_IDF_VERSION = "ESP-IDF v5.4.4"
EXPECTED_IDF_REVISION = "296b6eab9445fd720e71aecab961e2d3fbca9944"
EXPECTED_COMPILER_VERSION = (
    "xtensa-esp-elf-g++ (crosstool-NG esp-14.2.0_20260121) 14.2.0"
)
EXPECTED_COMPILER_SHA256 = (
    "004e294577ab054218508eaba90f92f9c2d504217ba6b78ecbd1d81f58f6ae73"
)
EXPECTED_COMPILER_PACKAGE = "esp-14.2.0_20260121"
EXPECTED_COMPILER_ARCHIVE_SHA256 = (
    "da31f36d79d4e99f24e55a90a71e65d5694714f16199960bf7885724b706a48c"
)
EXPECTED_COMPILER_ARCHIVE_NAME = (
    "xtensa-esp-elf-14.2.0_20260121-x86_64-linux-gnu.tar.xz"
)
EXPECTED_QEMU_VERSION = "QEMU emulator version 9.2.2 (esp_develop_9.2.2_20250817)"
EXPECTED_QEMU_SHA256 = (
    "57cd2d1909c08c2b810f4bf7f6fb2c1d2523fc8d3b564e9d5e871c0f471381f7"
)
EXPECTED_QEMU_PACKAGE = "esp_develop_9.2.2_20250817"
EXPECTED_QEMU_RELEASE_TAG = "esp-develop-9.2.2-20250817"
EXPECTED_QEMU_TAG_OBJECT = "bd84389ad04f4c8532c12f0c7e622035cf6f9fad"
EXPECTED_QEMU_SOURCE_REVISION = "4f4148e2f68689eb8861bf9fce0b46ada9200fef"
EXPECTED_QEMU_ARCHIVE_SHA256 = (
    "588bfaccd0f929650655d10a580f020c6ba9c131712d8fa519280081b8d126eb"
)
EXPECTED_QEMU_ARCHIVE_NAME = (
    "qemu-xtensa-softmmu-esp_develop_9.2.2_20250817-x86_64-linux-gnu.tar.xz"
)
TARGET = "esp32s3"
OBSERVATION_MARKER = "DOMES_QEMU_OBSERVATION"
RESULT_MARKER = "DOMES_QEMU_RESULT"
MARKER_GRACE_SECONDS = 0.2
ACCEPTANCE_RUNS = 100
DEBUG_START_ATTEMPTS = 3

REPO_ROOT = Path(__file__).resolve().parents[2]
PROBE_DIR = REPO_ROOT / "firmware" / "qemu_probe"

OBSERVATION_FIELDS = frozenset(
    {
        "schema",
        "core0_wait_ticks",
        "core1_wait_ticks",
        "irq_wait_ticks",
        "tick_start",
        "tick_end",
        "tick_delta",
        "irq_alarm",
        "irq_count_value",
        "irq_count_delta",
    }
)
RESULT_FIELDS = frozenset(
    {
        "schema",
        "status",
        "failure_mask",
        "cores",
        "controller_core",
        "core0_task_core",
        "core1_task_core",
        "core0_runs",
        "core1_runs",
        "core0_phases",
        "core1_phases",
        "core0_blocks",
        "core1_blocks",
        "core0_wakeups",
        "core1_wakeups",
        "task_handoff_0_to_1",
        "task_handoff_1_to_0",
        "tick_progress",
        "irq_source_core",
        "irq_count",
        "irq_drops",
        "irq_sequence",
        "irq_consumer_core",
        "irq_consumer_wakeups",
        "irq_to_core1_handoff",
        "timer_cleanup",
        "probe_state",
    }
)
PANIC_PATTERNS = (
    "Guru Meditation Error",
    "abort() was called",
    "assert failed:",
    "Backtrace:",
    "Rebooting...",
    "Restarting now",
    "SW_CPU_RESET",
)
ANSI_ESCAPE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")


class FeasibilityError(RuntimeError):
    """Raised when evidence cannot satisfy the feasibility contract."""


class DebugEndpointError(FeasibilityError):
    """Raised when the HMP or GDB endpoint cannot be established reliably."""


@dataclass(frozen=True)
class Toolchain:
    idf_path: Path
    idf_version: str
    idf_revision: str
    python: Path
    compiler: Path
    compiler_version: str
    compiler_sha256: str
    compiler_archive: Path | None
    compiler_archive_sha256: str | None
    qemu: Path
    qemu_version: str
    qemu_sha256: str
    qemu_archive: Path | None
    qemu_archive_sha256: str | None
    qemu_dynamic_dependencies: str
    libslirp: Path
    libslirp_sha256: str
    gdb: Path
    gdb_version: str


_ACTIVE_QEMU_PROCESSES: dict[int, subprocess.Popen[bytes]] = {}


def _toolchain_identity(toolchain: Toolchain) -> Mapping[str, str]:
    return {
        "idf_path": str(toolchain.idf_path),
        "idf_version": toolchain.idf_version,
        "idf_revision": toolchain.idf_revision,
        "python": str(toolchain.python),
        "compiler": str(toolchain.compiler),
        "compiler_version": toolchain.compiler_version,
        "compiler_sha256": toolchain.compiler_sha256,
        "compiler_archive": str(toolchain.compiler_archive or "not-cached"),
        "compiler_archive_sha256": toolchain.compiler_archive_sha256 or "not-cached",
        "qemu": str(toolchain.qemu),
        "qemu_version": toolchain.qemu_version,
        "qemu_sha256": toolchain.qemu_sha256,
        "qemu_archive": str(toolchain.qemu_archive or "not-cached"),
        "qemu_archive_sha256": toolchain.qemu_archive_sha256 or "not-cached",
        "libslirp": str(toolchain.libslirp),
        "libslirp_sha256": toolchain.libslirp_sha256,
        "gdb": str(toolchain.gdb),
        "gdb_version": toolchain.gdb_version,
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _command_output(
    command: Sequence[str], *, cwd: Path | None = None, timeout: float = 30.0
) -> str:
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise FeasibilityError(
            f"command failed to start: {' '.join(command)}: {error}"
        ) from error
    if completed.returncode != 0:
        output = completed.stdout.strip()
        if "libslirp" in output:
            raise FeasibilityError(
                "QEMU cannot load libslirp. Install the Linux distribution's libslirp package "
                "and re-source the ESP-IDF environment."
            )
        raise FeasibilityError(
            f"command exited {completed.returncode}: {' '.join(command)}\n{output}"
        )
    return completed.stdout.strip()


def validate_idf_version(output: str) -> str:
    version = output.splitlines()[-1].strip() if output.strip() else ""
    if version != EXPECTED_IDF_VERSION:
        raise FeasibilityError(
            f"expected exactly {EXPECTED_IDF_VERSION!r}, found {version or '<no version>'!r}"
        )
    return version


def validate_qemu_version(output: str) -> str:
    version = output.splitlines()[0].strip() if output.strip() else ""
    if version != EXPECTED_QEMU_VERSION:
        raise FeasibilityError(
            f"expected exactly {EXPECTED_QEMU_VERSION!r}, found {version or '<no version>'!r}"
        )
    return version


def _validate_qemu_tools_manifest(idf_path: Path) -> tuple[Path | None, str | None]:
    manifest_path = idf_path / "tools" / "tools.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        tool = next(item for item in manifest["tools"] if item["name"] == "qemu-xtensa")
        version = next(
            item for item in tool["versions"] if item["name"] == EXPECTED_QEMU_PACKAGE
        )
        archive = version["linux-amd64"]
    except (OSError, json.JSONDecodeError, KeyError, StopIteration, TypeError) as error:
        raise FeasibilityError(
            f"ESP-IDF QEMU manifest does not contain the pinned package: {manifest_path}: {error}"
        ) from error
    if archive.get("sha256") != EXPECTED_QEMU_ARCHIVE_SHA256:
        raise FeasibilityError(
            "ESP-IDF tools.json QEMU archive digest differs from the package pin"
        )
    if Path(str(archive.get("url", ""))).name != EXPECTED_QEMU_ARCHIVE_NAME:
        raise FeasibilityError(
            "ESP-IDF tools.json QEMU archive name differs from the package pin"
        )

    tools_root = Path(os.environ.get("IDF_TOOLS_PATH", "~/.espressif")).expanduser()
    archive_path = tools_root / "dist" / EXPECTED_QEMU_ARCHIVE_NAME
    if not archive_path.is_file():
        return None, None
    archive_sha256 = sha256_file(archive_path)
    if archive_sha256 != EXPECTED_QEMU_ARCHIVE_SHA256:
        raise FeasibilityError(
            "cached QEMU archive does not match the ESP-IDF package digest: "
            f"expected {EXPECTED_QEMU_ARCHIVE_SHA256}, found {archive_sha256}"
        )
    return archive_path.resolve(), archive_sha256


def _validate_compiler_tools_manifest(
    idf_path: Path, compiler: Path
) -> tuple[Path | None, str | None]:
    manifest_path = idf_path / "tools" / "tools.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        tool = next(
            item for item in manifest["tools"] if item["name"] == "xtensa-esp-elf"
        )
        version = next(
            item
            for item in tool["versions"]
            if item["name"] == EXPECTED_COMPILER_PACKAGE
        )
        archive = version["linux-amd64"]
    except (OSError, json.JSONDecodeError, KeyError, StopIteration, TypeError) as error:
        raise FeasibilityError(
            "ESP-IDF compiler manifest does not contain the pinned package: "
            f"{manifest_path}: {error}"
        ) from error
    if archive.get("sha256") != EXPECTED_COMPILER_ARCHIVE_SHA256:
        raise FeasibilityError(
            "ESP-IDF tools.json compiler archive digest differs from the package pin"
        )
    if Path(str(archive.get("url", ""))).name != EXPECTED_COMPILER_ARCHIVE_NAME:
        raise FeasibilityError(
            "ESP-IDF tools.json compiler archive name differs from the package pin"
        )

    tools_root = Path(os.environ.get("IDF_TOOLS_PATH", "~/.espressif")).expanduser()
    expected_root = (
        tools_root / "tools" / "xtensa-esp-elf" / EXPECTED_COMPILER_PACKAGE
    ).resolve()
    try:
        compiler.relative_to(expected_root)
    except ValueError as error:
        raise FeasibilityError(
            "compiler executable is outside the pinned ESP-IDF package directory: "
            f"expected under {expected_root}, found {compiler}"
        ) from error

    archive_path = tools_root / "dist" / EXPECTED_COMPILER_ARCHIVE_NAME
    if not archive_path.is_file():
        return None, None
    archive_sha256 = sha256_file(archive_path)
    if archive_sha256 != EXPECTED_COMPILER_ARCHIVE_SHA256:
        raise FeasibilityError(
            "cached compiler archive does not match the ESP-IDF package digest: "
            f"expected {EXPECTED_COMPILER_ARCHIVE_SHA256}, found {archive_sha256}"
        )
    return archive_path.resolve(), archive_sha256


def _resolve_qemu_executable() -> Path | None:
    on_path = shutil.which("qemu-system-xtensa")
    if on_path:
        return Path(on_path).resolve()
    tools_root = Path(os.environ.get("IDF_TOOLS_PATH", "~/.espressif")).expanduser()
    installed = (
        tools_root
        / "tools"
        / "qemu-xtensa"
        / EXPECTED_QEMU_PACKAGE
        / "qemu"
        / "bin"
        / "qemu-system-xtensa"
    )
    if installed.is_file() and os.access(installed, os.X_OK):
        return installed.resolve()
    return None


def discover_toolchain(require_gdb: bool) -> Toolchain:
    if platform.system() != "Linux":
        raise FeasibilityError("the FS-WP-002B runner is Linux-only")
    if platform.machine() not in {"x86_64", "amd64"}:
        raise FeasibilityError(
            "the pinned FS-WP-002B engine artifact is Linux x86_64-only"
        )

    idf_value = os.environ.get("IDF_PATH")
    if not idf_value:
        raise FeasibilityError(
            "IDF_PATH is unset; source ~/esp/esp-idf/export.sh from ESP-IDF v5.4.4 first"
        )
    idf_path = Path(idf_value).resolve()
    idf_py = idf_path / "tools" / "idf.py"
    idf_python_env = os.environ.get("IDF_PYTHON_ENV_PATH")
    idf_python = (
        Path(idf_python_env).resolve() / "bin" / "python"
        if idf_python_env
        else Path(shutil.which("python") or "")
    )
    compiler = shutil.which("xtensa-esp32s3-elf-g++")
    qemu = _resolve_qemu_executable()
    gdb = shutil.which("xtensa-esp32s3-elf-gdb")
    if not idf_py.is_file():
        raise FeasibilityError(f"ESP-IDF idf.py not found under IDF_PATH: {idf_py}")
    if not idf_python.is_file():
        raise FeasibilityError(
            "the ESP-IDF Python environment is unavailable; re-source ESP-IDF v5.4.4 export.sh"
        )
    if not compiler:
        raise FeasibilityError(
            "xtensa-esp32s3-elf-g++ is unavailable; install the ESP-IDF v5.4.4 tools"
        )
    if not qemu:
        raise FeasibilityError(
            "qemu-system-xtensa is unavailable; run "
            "'python $IDF_PATH/tools/idf_tools.py install qemu-xtensa' and re-source export.sh"
        )
    if require_gdb and not gdb:
        raise FeasibilityError(
            "xtensa-esp32s3-elf-gdb is unavailable; install the ESP-IDF v5.4.4 tools"
        )

    idf_version = validate_idf_version(
        _command_output([str(idf_python), str(idf_py), "--version"])
    )
    idf_revision = _command_output(["git", "-C", str(idf_path), "rev-parse", "HEAD"])
    if idf_revision != EXPECTED_IDF_REVISION:
        raise FeasibilityError(
            f"expected ESP-IDF revision {EXPECTED_IDF_REVISION}, found {idf_revision}"
        )
    if _command_output(["git", "-C", str(idf_path), "status", "--porcelain"]):
        raise FeasibilityError("the pinned ESP-IDF source tree has local modifications")

    compiler_path = Path(compiler).resolve()
    compiler_version = _command_output([str(compiler_path), "--version"]).splitlines()[
        0
    ]
    if compiler_version != EXPECTED_COMPILER_VERSION:
        raise FeasibilityError(
            f"expected compiler {EXPECTED_COMPILER_VERSION!r}, found {compiler_version!r}"
        )
    compiler_sha256 = sha256_file(compiler_path)
    if compiler_sha256 != EXPECTED_COMPILER_SHA256:
        raise FeasibilityError(
            "compiler executable does not match the pinned Linux x86_64 artifact: "
            f"expected sha256 {EXPECTED_COMPILER_SHA256}, found {compiler_sha256}"
        )
    compiler_archive, compiler_archive_sha256 = _validate_compiler_tools_manifest(
        idf_path, compiler_path
    )
    qemu_path = qemu
    qemu_version = validate_qemu_version(_command_output([str(qemu_path), "--version"]))
    qemu_sha256 = sha256_file(qemu_path)
    if qemu_sha256 != EXPECTED_QEMU_SHA256:
        raise FeasibilityError(
            "QEMU executable does not match the pinned Linux x86_64 artifact: "
            f"expected sha256 {EXPECTED_QEMU_SHA256}, found {qemu_sha256}"
        )
    qemu_dynamic_dependencies = _command_output(["ldd", str(qemu_path)])
    libslirp_match = re.search(
        r"libslirp\.so\.0\s+=>\s+(\S+)", qemu_dynamic_dependencies
    )
    if not libslirp_match:
        raise FeasibilityError(
            "QEMU runtime did not resolve the required libslirp.so.0"
        )
    libslirp = Path(libslirp_match.group(1)).resolve()
    if not libslirp.is_file():
        raise FeasibilityError(f"resolved libslirp runtime is not a file: {libslirp}")

    qemu_archive, qemu_archive_sha256 = _validate_qemu_tools_manifest(idf_path)
    gdb_path = Path(gdb).resolve() if gdb else Path()
    gdb_version = (
        _command_output([str(gdb_path), "--version"]).splitlines()[0]
        if require_gdb
        else "not-required"
    )
    return Toolchain(
        idf_path=idf_path,
        idf_version=idf_version,
        idf_revision=idf_revision,
        python=idf_python,
        compiler=compiler_path,
        compiler_version=compiler_version,
        compiler_sha256=compiler_sha256,
        compiler_archive=compiler_archive,
        compiler_archive_sha256=compiler_archive_sha256,
        qemu=qemu_path,
        qemu_version=qemu_version,
        qemu_sha256=qemu_sha256,
        qemu_archive=qemu_archive,
        qemu_archive_sha256=qemu_archive_sha256,
        qemu_dynamic_dependencies=qemu_dynamic_dependencies,
        libslirp=libslirp,
        libslirp_sha256=sha256_file(libslirp),
        gdb=gdb_path,
        gdb_version=gdb_version,
    )


def _run_logged(
    command: Sequence[str], *, cwd: Path, log_path: Path, timeout: float = 1800.0
) -> float:
    started = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise FeasibilityError(
            f"command failed: {' '.join(command)}: {error}"
        ) from error
    elapsed = time.monotonic() - started
    log_path.write_bytes(completed.stdout)
    if completed.returncode != 0:
        tail = completed.stdout.decode("utf-8", errors="replace")[-4000:]
        raise FeasibilityError(
            f"command exited {completed.returncode}; see {log_path}\n{tail}"
        )
    return elapsed


def validate_sdkconfig(path: Path) -> Mapping[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value

    expected = {
        "CONFIG_IDF_TARGET": '"esp32s3"',
        "CONFIG_APP_REPRODUCIBLE_BUILD": "y",
        "CONFIG_ESPTOOLPY_FLASHSIZE_4MB": "y",
        "CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0": "y",
        "CONFIG_FREERTOS_HZ": "1000",
        "CONFIG_FREERTOS_NUMBER_OF_CORES": "2",
    }
    for key, expected_value in expected.items():
        if values.get(key) != expected_value:
            raise FeasibilityError(
                f"generated SDKCONFIG {key} expected {expected_value}, found {values.get(key)!r}"
            )
    disabled = {
        "CONFIG_FREERTOS_SMP",
        "CONFIG_FREERTOS_UNICORE",
        "CONFIG_COMPILER_CXX_EXCEPTIONS",
        "CONFIG_COMPILER_CXX_RTTI",
    }
    for key in disabled:
        if values.get(key) == "y":
            raise FeasibilityError(f"generated SDKCONFIG must keep {key} disabled")
    return {
        **expected,
        **{key: values.get(key, "not-set") for key in sorted(disabled)},
    }


def _build_output_hashes(build_dir: Path, sdkconfig: Path, elf: Path) -> dict[str, str]:
    flash_args = build_dir / "flash_args"
    description = build_dir / "project_description.json"
    paths: dict[str, Path] = {
        "sdkconfig": sdkconfig,
        "app_elf": elf,
        "flash_args": flash_args,
        "project_description.json": description,
    }
    try:
        tokens = shlex.split(flash_args.read_text(encoding="utf-8"))
    except OSError as error:
        raise FeasibilityError(f"cannot read build flash_args: {error}") from error
    for token in tokens:
        if not token.endswith(".bin"):
            continue
        binary = (build_dir / token).resolve()
        try:
            relative = binary.relative_to(build_dir.resolve())
        except ValueError as error:
            raise FeasibilityError(
                f"flash_args references a binary outside the isolated build: {binary}"
            ) from error
        paths[f"flash_binary:{relative}"] = binary
    missing = [str(path) for path in paths.values() if not path.is_file()]
    if missing:
        raise FeasibilityError(
            f"build output identity is incomplete: {', '.join(sorted(missing))}"
        )
    return {name: sha256_file(path) for name, path in sorted(paths.items())}


def build_probe(
    toolchain: Toolchain, build_dir: Path, sdkconfig: Path, artifact_dir: Path
) -> Mapping[str, Any]:
    if build_dir.exists() and any(build_dir.iterdir()):
        raise FeasibilityError(
            f"isolated build directory must be absent or empty: {build_dir}"
        )
    build_dir.mkdir(parents=True, exist_ok=True)
    sdkconfig.parent.mkdir(parents=True, exist_ok=True)
    if sdkconfig.exists():
        raise FeasibilityError(f"isolated SDKCONFIG already exists: {sdkconfig}")

    command = [
        str(toolchain.python),
        str(toolchain.idf_path / "tools" / "idf.py"),
        "-C",
        str(PROBE_DIR),
        "-B",
        str(build_dir),
        "-D",
        f"IDF_TARGET={TARGET}",
        "-D",
        f"SDKCONFIG={sdkconfig}",
        "build",
    ]
    cold_seconds = _run_logged(
        command, cwd=REPO_ROOT, log_path=artifact_dir / "build-cold.log"
    )
    cached_seconds = _run_logged(
        command, cwd=REPO_ROOT, log_path=artifact_dir / "build-cached.log"
    )

    description_path = build_dir / "project_description.json"
    try:
        description = json.loads(description_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise FeasibilityError(
            f"invalid project description: {description_path}: {error}"
        ) from error
    if description.get("target") != TARGET:
        raise FeasibilityError(
            f"build target is not {TARGET}: {description.get('target')!r}"
        )
    if Path(description.get("idf_path", "")).resolve() != toolchain.idf_path:
        raise FeasibilityError("build used a different ESP-IDF tree")
    if Path(description.get("config_file", "")).resolve() != sdkconfig.resolve():
        raise FeasibilityError("build did not use the isolated SDKCONFIG")

    elf = build_dir / description["app_elf"]
    required = (elf, build_dir / "flash_args", sdkconfig)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FeasibilityError(
            f"build omitted required artifacts: {', '.join(missing)}"
        )
    config_assertions = validate_sdkconfig(sdkconfig)
    retained_dir = artifact_dir / "artifacts"
    retained_dir.mkdir()
    retained_elf = retained_dir / elf.name
    retained_sdkconfig = retained_dir / "sdkconfig"
    shutil.copy2(elf, retained_elf)
    shutil.copy2(sdkconfig, retained_sdkconfig)
    elf_sha256 = sha256_file(elf)
    sdkconfig_sha256 = sha256_file(sdkconfig)
    retained_elf_sha256 = sha256_file(retained_elf)
    retained_sdkconfig_sha256 = sha256_file(retained_sdkconfig)
    if (
        retained_elf_sha256 != elf_sha256
        or retained_sdkconfig_sha256 != sdkconfig_sha256
    ):
        raise FeasibilityError(
            "retained ELF or SDKCONFIG differs from the build output"
        )
    return {
        "command": list(command),
        "cold_seconds": cold_seconds,
        "cached_seconds": cached_seconds,
        "build_dir": str(build_dir),
        "sdkconfig": str(sdkconfig),
        "sdkconfig_sha256": sdkconfig_sha256,
        "sdkconfig_assertions": config_assertions,
        "elf": str(elf),
        "elf_sha256": elf_sha256,
        "retained_elf": str(retained_elf),
        "retained_elf_sha256": retained_elf_sha256,
        "retained_sdkconfig": str(retained_sdkconfig),
        "retained_sdkconfig_sha256": retained_sdkconfig_sha256,
        "project_description_sha256": sha256_file(description_path),
        "output_hashes": _build_output_hashes(build_dir, sdkconfig, elf),
    }


def _idf_subprocess_env(toolchain: Toolchain) -> dict[str, str]:
    environment = os.environ.copy()
    idf_tools = str(toolchain.idf_path / "tools")
    environment["PYTHONPATH"] = idf_tools + (
        os.pathsep + environment["PYTHONPATH"] if environment.get("PYTHONPATH") else ""
    )
    return environment


def generate_run_images(
    toolchain: Toolchain,
    build_dir: Path,
    destination: Path,
    *,
    flash_size: str = "4MB",
) -> Mapping[str, Any]:
    if not re.fullmatch(r"(?:2|4|8|16|32)MB", flash_size):
        raise FeasibilityError(f"unsupported QEMU flash geometry: {flash_size!r}")
    destination.mkdir(parents=True, exist_ok=False)
    flash = destination / "qemu_flash.bin"
    efuse = destination / "qemu_efuse.bin"
    merge_command = [
        str(toolchain.python),
        "-m",
        "esptool",
        f"--chip={TARGET}",
        "merge_bin",
        f"--output={flash}",
        f"--fill-flash-size={flash_size}",
        "@flash_args",
    ]
    _run_logged(
        merge_command,
        cwd=build_dir,
        log_path=destination / "flash-generation.log",
        timeout=120.0,
    )
    efuse_script = (
        "from pathlib import Path; from idf_py_actions.qemu_ext import QEMU_TARGETS; "
        f"Path({str(efuse)!r}).write_bytes(QEMU_TARGETS[{TARGET!r}].default_efuse)"
    )
    completed = subprocess.run(
        [str(toolchain.python), "-c", efuse_script],
        cwd=build_dir,
        env=_idf_subprocess_env(toolchain),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=30.0,
    )
    (destination / "efuse-generation.log").write_bytes(completed.stdout)
    if completed.returncode != 0 or not efuse.is_file():
        raise FeasibilityError(
            "ESP-IDF qemu_ext failed to generate the default ESP32-S3 eFuse image; see "
            f"{destination / 'efuse-generation.log'}"
        )
    return {
        "flash": flash,
        "flash_sha256": sha256_file(flash),
        "efuse": efuse,
        "efuse_sha256": sha256_file(efuse),
        "merge_command": merge_command,
        "flash_size": flash_size,
        "efuse_source": str(
            toolchain.idf_path / "tools" / "idf_py_actions" / "qemu_ext.py"
        ),
    }


def verify_run_images_unchanged(images: Mapping[str, Any]) -> Mapping[str, str]:
    flash_after = sha256_file(Path(images["flash"]))
    efuse_after = sha256_file(Path(images["efuse"]))
    if flash_after != images["flash_sha256"] or efuse_after != images["efuse_sha256"]:
        raise FeasibilityError("QEMU mutated snapshot-backed flash or eFuse inputs")
    return {
        "flash_sha256_after": flash_after,
        "efuse_sha256_after": efuse_after,
    }


def build_qemu_command(
    qemu: Path,
    flash: Path,
    efuse: Path,
    *,
    gdb_port: int | None = None,
    monitor_socket: Path | None = None,
) -> list[str]:
    if (gdb_port is None) != (monitor_socket is None):
        raise ValueError("gdb_port and monitor_socket must be supplied together")
    command = [
        str(qemu),
        "-M",
        "esp32s3",
        "-m",
        "32M",
        "-drive",
        f"file={flash},if=mtd,format=raw",
        "-drive",
        f"file={efuse},if=none,format=raw,id=efuse",
        "-global",
        "driver=nvram.esp32s3.efuse,property=drive,value=efuse",
        "-global",
        "driver=timer.esp32s3.timg,property=wdt_disable,value=true",
        "-nic",
        "none",
        "-accel",
        "tcg,thread=single",
        "-icount",
        "shift=3,align=off,sleep=off",
        "-rtc",
        "base=2026-01-01T00:00:00,clock=vm",
        "-seed",
        "1",
        "-snapshot",
        "-no-user-config",
        "-no-reboot",
    ]
    if gdb_port is None:
        command += ["-nographic", "-serial", "mon:stdio"]
    else:
        command += [
            "-display",
            "none",
            "-S",
            "-gdb",
            f"tcp:127.0.0.1:{gdb_port}",
            "-monitor",
            f"unix:{monitor_socket},server=on,wait=off",
            "-serial",
            "none",
        ]
    return command


def _parse_marker(
    log: str, marker: str, expected_fields: frozenset[str]
) -> dict[str, str]:
    clean = ANSI_ESCAPE.sub("", log).replace("\r", "")
    matches = []
    for line in clean.splitlines():
        offset = line.find(marker)
        if offset >= 0:
            matches.append(line[offset:].strip())
    if len(matches) != 1:
        raise FeasibilityError(f"expected one {marker} marker, found {len(matches)}")

    fields: dict[str, str] = {}
    for token in matches[0][len(marker) :].split():
        if token.count("=") != 1:
            raise FeasibilityError(f"malformed {marker} token: {token!r}")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise FeasibilityError(f"invalid or duplicate {marker} field: {token!r}")
        fields[key] = value
    missing = expected_fields - fields.keys()
    extra = fields.keys() - expected_fields
    if missing or extra:
        raise FeasibilityError(
            f"{marker} fields differ from schema; missing={sorted(missing)}, extra={sorted(extra)}"
        )
    return fields


def _integer_fields(
    fields: Mapping[str, str], excluded: frozenset[str]
) -> dict[str, int]:
    converted: dict[str, int] = {}
    for key, value in fields.items():
        if key in excluded:
            continue
        try:
            converted[key] = int(value, 10)
        except ValueError as error:
            raise FeasibilityError(
                f"field {key} is not a base-10 integer: {value!r}"
            ) from error
    return converted


def analyze_log(log: str) -> tuple[dict[str, int], dict[str, str | int]]:
    for pattern in PANIC_PATTERNS:
        if pattern in log:
            raise FeasibilityError(f"target panic or reset marker observed: {pattern}")
    if log.count("ESP-ROM:esp32s3") > 1:
        raise FeasibilityError("target booted more than once")

    observation_raw = _parse_marker(log, OBSERVATION_MARKER, OBSERVATION_FIELDS)
    result_raw = _parse_marker(log, RESULT_MARKER, RESULT_FIELDS)
    observation = _integer_fields(observation_raw, frozenset())
    result_numeric = _integer_fields(result_raw, frozenset({"status", "probe_state"}))
    result: dict[str, str | int] = {
        **result_numeric,
        "status": result_raw["status"],
        "probe_state": result_raw["probe_state"],
    }

    exact_observation = {
        "schema": 3,
        "irq_alarm": 2000,
    }
    for key, expected in exact_observation.items():
        if observation[key] != expected:
            raise FeasibilityError(
                f"observation {key} expected {expected}, found {observation[key]}"
            )
    if observation["core0_wait_ticks"] < 2 or observation["core1_wait_ticks"] < 4:
        raise FeasibilityError("cross-core block/wakeup delays were not observed")
    if observation["irq_wait_ticks"] <= 0:
        raise FeasibilityError("interrupt consumer did not block and wake")
    if (
        observation["tick_end"] <= observation["tick_start"]
        or observation["tick_delta"] <= 0
    ):
        raise FeasibilityError("target tick counter did not progress")
    if observation["tick_delta"] != observation["tick_end"] - observation["tick_start"]:
        raise FeasibilityError("raw tick delta is inconsistent")
    if observation["irq_count_value"] < observation["irq_alarm"]:
        raise FeasibilityError("GPTimer interrupt fired before its alarm value")
    if (
        observation["irq_count_delta"]
        != observation["irq_count_value"] - observation["irq_alarm"]
    ):
        raise FeasibilityError("raw interrupt count delta is inconsistent")

    exact_result: dict[str, str | int] = {
        "schema": 3,
        "status": "PASS",
        "failure_mask": 0,
        "cores": 2,
        "controller_core": 0,
        "core0_task_core": 0,
        "core1_task_core": 1,
        "core0_runs": 1,
        "core1_runs": 1,
        "core0_phases": 5,
        "core1_phases": 5,
        "core0_blocks": 1,
        "core1_blocks": 2,
        "core0_wakeups": 1,
        "core1_wakeups": 2,
        "task_handoff_0_to_1": 1,
        "task_handoff_1_to_0": 1,
        "tick_progress": 1,
        "irq_source_core": 0,
        "irq_count": 1,
        "irq_drops": 0,
        "irq_sequence": 1,
        "irq_consumer_core": 1,
        "irq_consumer_wakeups": 1,
        "irq_to_core1_handoff": 1,
        "timer_cleanup": 1,
        "probe_state": "complete",
    }
    for key, expected in exact_result.items():
        if result[key] != expected:
            raise FeasibilityError(
                f"result {key} expected {expected!r}, found {result[key]!r}"
            )
    return observation, result


def canonical_signatures(
    observation: Mapping[str, int], result: Mapping[str, str | int]
) -> tuple[dict[str, str | int], str, dict[str, int], str]:
    structural = dict(sorted(result.items()))
    normalized_observation = {
        key: observation[key]
        for key in sorted(observation)
        if key not in {"tick_start", "tick_end"}
    }
    structural_encoded = json.dumps(
        structural, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    observation_encoded = json.dumps(
        normalized_observation, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return (
        structural,
        hashlib.sha256(structural_encoded).hexdigest(),
        normalized_observation,
        hashlib.sha256(observation_encoded).hexdigest(),
    )


def require_identical_signatures(signatures: Sequence[str]) -> str:
    if not signatures:
        raise FeasibilityError("no deterministic signatures were produced")
    unique = sorted(set(signatures))
    if len(unique) != 1:
        raise FeasibilityError(
            f"deterministic signature mismatch: {len(unique)} unique signatures: {unique}"
        )
    return unique[0]


def _register_qemu_process(process: subprocess.Popen[bytes]) -> None:
    _ACTIVE_QEMU_PROCESSES[process.pid] = process


def _unregister_qemu_process(process: subprocess.Popen[bytes]) -> None:
    _ACTIVE_QEMU_PROCESSES.pop(process.pid, None)


def _handle_runner_signal(signum: int, _frame: Any) -> None:
    for process in tuple(_ACTIVE_QEMU_PROCESSES.values()):
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    try:
        signal_name = signal.Signals(signum).name
    except ValueError:
        signal_name = str(signum)
    raise FeasibilityError(f"runner interrupted by {signal_name}")


@contextmanager
def _runner_signal_handlers() -> Any:
    previous = {
        signum: signal.getsignal(signum) for signum in (signal.SIGINT, signal.SIGTERM)
    }
    for signum in previous:
        signal.signal(signum, _handle_runner_signal)
    try:
        yield
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def _terminate_process(process: subprocess.Popen[bytes]) -> tuple[int, str]:
    action = "already_exited"
    if process.poll() is None:
        action = "sigterm"
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            action = "process_exit_race"
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            action = "sigkill"
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                action = "process_exit_race"
            process.wait(timeout=2.0)
    return int(process.returncode if process.returncode is not None else -1), action


def execute_until_marker(
    command: Sequence[str], log_path: Path, timeout: float, marker: str
) -> Mapping[str, Any]:
    if not marker or not marker.isascii():
        raise ValueError("marker must be non-empty ASCII")
    started = time.monotonic()
    try:
        process = subprocess.Popen(
            command,
            cwd=REPO_ROOT,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    except OSError as error:
        if "libslirp" in str(error):
            raise FeasibilityError(
                "QEMU cannot load libslirp; install the distro libslirp package"
            )
        raise FeasibilityError(f"QEMU failed to start: {error}") from error
    _register_qemu_process(process)
    assert process.stdout is not None
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    output = bytearray()
    deadline = started + timeout
    marker_deadline: float | None = None
    timed_out = False
    exited_early = False
    try:
        while True:
            now = time.monotonic()
            if now >= deadline:
                timed_out = True
                break
            if marker_deadline is not None and now >= marker_deadline:
                break
            wait = min(0.1, deadline - now)
            if marker_deadline is not None:
                wait = min(wait, marker_deadline - now)
            events = selector.select(max(0.0, wait))
            for key, _ in events:
                chunk = os.read(key.fileobj.fileno(), 65536)
                if chunk:
                    output.extend(chunk)
                    if marker.encode("ascii") in output and marker_deadline is None:
                        marker_deadline = time.monotonic() + MARKER_GRACE_SECONDS
                else:
                    selector.unregister(key.fileobj)
            if process.poll() is not None:
                while True:
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        break
                    output.extend(chunk)
                exited_early = True
                break
    finally:
        selector.close()
        try:
            returncode, termination_action = _terminate_process(process)
        finally:
            _unregister_qemu_process(process)
            process.stdout.close()
            log_path.write_bytes(output)
    elapsed = time.monotonic() - started
    text = output.decode("utf-8", errors="replace")
    if timed_out:
        raise FeasibilityError(f"QEMU timed out after {timeout:.3f}s; see {log_path}")
    if exited_early:
        raise FeasibilityError(
            f"QEMU exited before runner termination with code {returncode}; see {log_path}"
        )
    if termination_action != "sigterm":
        raise FeasibilityError(
            f"QEMU required unexpected runner termination action {termination_action}; see {log_path}"
        )
    return {
        "seconds": elapsed,
        "termination": "marker_observed_then_runner_sigterm",
        "termination_action": termination_action,
        "qemu_returncode": returncode,
        "log": str(log_path),
        "log_sha256": sha256_file(log_path),
        "text": text,
    }


def execute_probe(
    command: Sequence[str], log_path: Path, timeout: float
) -> Mapping[str, Any]:
    execution = execute_until_marker(command, log_path, timeout, RESULT_MARKER)
    observation, result = analyze_log(str(execution["text"]))
    structural, structural_signature, normalized_observation, observation_signature = (
        canonical_signatures(observation, result)
    )
    return {
        "seconds": execution["seconds"],
        "termination": "result_marker_observed_then_runner_sigterm",
        "termination_action": execution["termination_action"],
        "qemu_returncode": execution["qemu_returncode"],
        "log": execution["log"],
        "log_sha256": execution["log_sha256"],
        "observation": observation,
        "result": result,
        "canonical_structural": structural,
        "structural_signature": structural_signature,
        "canonical_observation": normalized_observation,
        "observation_signature": observation_signature,
    }


def _free_tcp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def _read_hmp(
    socket_path: Path, process: subprocess.Popen[bytes], timeout: float = 10.0
) -> str:
    deadline = time.monotonic() + timeout
    last_error: OSError | None = None
    monitor: socket.socket | None = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise DebugEndpointError(
                f"paused QEMU exited with {process.returncode} before HMP was ready"
            )
        candidate = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        candidate.settimeout(0.25)
        try:
            candidate.connect(str(socket_path))
        except OSError as error:
            last_error = error
            candidate.close()
            time.sleep(0.05)
            continue
        monitor = candidate
        break
    if monitor is None:
        raise DebugEndpointError(
            f"timed out connecting to QEMU HMP socket {socket_path}: {last_error}"
        )

    with monitor:
        monitor.settimeout(1.0)
        try:
            initial = monitor.recv(65536)
            monitor.sendall(b"info cpus\n")
            chunks = [initial]
            response_deadline = time.monotonic() + 5.0
            while time.monotonic() < response_deadline:
                try:
                    chunk = monitor.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                chunks.append(chunk)
                if b"(qemu)" in chunk and b"CPU #" in b"".join(chunks):
                    break
        except OSError as error:
            raise DebugEndpointError(f"QEMU HMP exchange failed: {error}") from error
    return b"".join(chunks).decode("utf-8", errors="replace")


def _wait_for_tcp_endpoint(
    port: int, process: subprocess.Popen[bytes], timeout: float = 10.0
) -> None:
    deadline = time.monotonic() + timeout
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise DebugEndpointError(
                f"paused QEMU exited with {process.returncode} before GDB was ready"
            )
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.settimeout(0.25)
            try:
                probe.connect(("127.0.0.1", port))
            except OSError as error:
                last_error = error
                time.sleep(0.05)
                continue
        time.sleep(0.05)
        return
    raise DebugEndpointError(
        f"timed out connecting to QEMU GDB endpoint 127.0.0.1:{port}: {last_error}"
    )


def _gdb_endpoint_failed(output: str) -> bool:
    return any(
        pattern in output
        for pattern in (
            "Connection refused",
            "Connection reset",
            "Connection timed out",
            "Remote communication error",
            "Remote connection closed",
        )
    )


def _collect_debug_evidence_attempt(
    toolchain: Toolchain,
    build_dir: Path,
    attempt_dir: Path,
    elf: Path,
) -> Mapping[str, Any]:
    images = generate_run_images(toolchain, build_dir, attempt_dir)
    port = _free_tcp_port()
    with tempfile.TemporaryDirectory(prefix="domes-qemu-monitor-") as socket_dir:
        monitor_socket = Path(socket_dir) / "monitor.sock"
        command = build_qemu_command(
            toolchain.qemu,
            images["flash"],
            images["efuse"],
            gdb_port=port,
            monitor_socket=monitor_socket,
        )
        qemu_log_path = attempt_dir / "qemu.log"
        with qemu_log_path.open("wb") as qemu_log:
            try:
                process = subprocess.Popen(
                    command,
                    cwd=REPO_ROOT,
                    stdin=subprocess.DEVNULL,
                    stdout=qemu_log,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
            except OSError as error:
                raise DebugEndpointError(
                    f"debug QEMU failed to start: {error}"
                ) from error
            _register_qemu_process(process)
            try:
                monitor_text = _read_hmp(monitor_socket, process)
                monitor_path = attempt_dir / "monitor.txt"
                monitor_path.write_text(monitor_text, encoding="utf-8")
                if "CPU #0" not in monitor_text or "CPU #1" not in monitor_text:
                    raise FeasibilityError(
                        f"QEMU monitor did not report both CPUs; see {monitor_path}"
                    )

                _wait_for_tcp_endpoint(port, process)
                gdb_command = [
                    str(toolchain.gdb),
                    "-q",
                    "-batch",
                    str(elf),
                    "-ex",
                    "set pagination off",
                    "-ex",
                    f"target remote 127.0.0.1:{port}",
                    "-ex",
                    "monitor info cpus",
                    "-ex",
                    "break domesQemuProbeComplete",
                    "-ex",
                    "continue",
                    "-ex",
                    "monitor info cpus",
                    "-ex",
                    "set print pretty on",
                    "-ex",
                    "print gProbeState",
                    "-ex",
                    "info threads",
                    "-ex",
                    "thread apply all info registers pc",
                    "-ex",
                    "detach",
                ]
                completed = subprocess.run(
                    gdb_command,
                    cwd=REPO_ROOT,
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    timeout=30.0,
                )
                gdb_path = attempt_dir / "gdb.txt"
                gdb_path.write_bytes(completed.stdout)
                gdb_text = completed.stdout.decode("utf-8", errors="replace")
                if completed.returncode != 0 and _gdb_endpoint_failed(gdb_text):
                    raise DebugEndpointError(
                        f"GDB endpoint failed during startup; see {gdb_path}"
                    )
                thread_rows = re.findall(r"(?m)^\s*\*?\s*[12]\s+Thread\b", gdb_text)
                if (
                    completed.returncode != 0
                    or len(thread_rows) < 2
                    or "Breakpoint 1" not in gdb_text
                    or "schema = 3" not in gdb_text
                    or "failureMask = 0" not in gdb_text
                    or "taskHandoff0To1 = 1" not in gdb_text
                    or "taskHandoff1To0 = 1" not in gdb_text
                    or "irqConsumerCore = 1" not in gdb_text
                    or "irqConsumerWakeups = 1" not in gdb_text
                    or "timerCleanup = 1" not in gdb_text
                    or "CPU #0" not in gdb_text
                    or "CPU #1" not in gdb_text
                ):
                    raise FeasibilityError(
                        "batch GDB did not report both target CPUs and terminal probe state "
                        f"(exit {completed.returncode}); see {gdb_path}"
                    )
            finally:
                try:
                    returncode, termination_action = _terminate_process(process)
                finally:
                    _unregister_qemu_process(process)

    if termination_action != "sigterm":
        raise FeasibilityError(
            f"debug QEMU required unexpected runner termination action {termination_action}"
        )
    image_integrity = verify_run_images_unchanged(images)
    return {
        "status": "PASS",
        "qemu_command": command,
        "qemu_returncode": returncode,
        "termination_action": termination_action,
        "monitor": str(monitor_path),
        "monitor_sha256": sha256_file(monitor_path),
        "gdb_command": gdb_command,
        "gdb_elf": str(elf),
        "gdb_elf_sha256": sha256_file(elf),
        "gdb": str(gdb_path),
        "gdb_sha256": sha256_file(gdb_path),
        "qemu_log": str(qemu_log_path),
        "flash_sha256": images["flash_sha256"],
        "efuse_sha256": images["efuse_sha256"],
        **image_integrity,
    }


def collect_debug_evidence(
    toolchain: Toolchain, build_dir: Path, artifact_dir: Path, elf: Path
) -> Mapping[str, Any]:
    inspection_dir = artifact_dir / "inspection"
    inspection_dir.mkdir()
    startup_errors: list[str] = []
    for attempt in range(1, DEBUG_START_ATTEMPTS + 1):
        try:
            evidence = dict(
                _collect_debug_evidence_attempt(
                    toolchain,
                    build_dir,
                    inspection_dir / f"attempt-{attempt}",
                    elf,
                )
            )
        except DebugEndpointError as error:
            startup_errors.append(str(error))
            if attempt == DEBUG_START_ATTEMPTS:
                raise FeasibilityError(
                    f"debug endpoints failed after {DEBUG_START_ATTEMPTS} attempts: "
                    + "; ".join(startup_errors)
                ) from error
            continue
        evidence["startup_attempt"] = attempt
        evidence["prior_startup_errors"] = startup_errors
        return evidence
    raise AssertionError("debug startup loop exhausted without a result")


def _implementation_hashes() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for path in sorted(PROBE_DIR.rglob("*")):
        if path.is_file() and "build" not in path.relative_to(PROBE_DIR).parts:
            hashes[str(path.relative_to(REPO_ROOT))] = sha256_file(path)
    runner = Path(__file__).resolve()
    tests = runner.with_name("test_qemu_feasibility.py")
    for path in (runner, tests):
        hashes[str(path.relative_to(REPO_ROOT))] = sha256_file(path)
    return hashes


def write_artifact_manifest(artifact_dir: Path) -> Path:
    manifest_path = artifact_dir / "artifact-manifest.sha256"
    entries = []
    for path in sorted(artifact_dir.rglob("*")):
        if path.is_file() and path != manifest_path:
            entries.append(f"{sha256_file(path)}  {path.relative_to(artifact_dir)}")
    manifest_path.write_text("\n".join(entries) + "\n", encoding="utf-8")
    return manifest_path


def _git_state() -> Mapping[str, Any]:
    commit = _command_output(["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"])
    status = subprocess.run(
        [
            "git",
            "-C",
            str(REPO_ROOT),
            "status",
            "--porcelain",
            "--",
            "firmware/qemu_probe",
            "tools/simulation",
        ],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.splitlines()
    return {"commit": commit, "relevant_worktree_status": status}


def _paths_overlap(first: Path, second: Path) -> bool:
    return first == second or first in second.parents or second in first.parents


def _session_paths(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    session = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{os.getpid()}"
    if args.build_dir:
        build_dir = args.build_dir.resolve()
    else:
        build_dir = PROBE_DIR / "build" / session / "idf"
    sdkconfig = build_dir.parent / "sdkconfig"
    artifact_dir = (
        args.artifact_dir.resolve()
        if args.artifact_dir
        else build_dir.parent / "evidence"
    )
    if _paths_overlap(build_dir, artifact_dir):
        raise FeasibilityError(
            "build and artifact directories must be disjoint and neither may contain the other: "
            f"build={build_dir}, artifacts={artifact_dir}"
        )
    if artifact_dir.exists() and any(artifact_dir.iterdir()):
        raise FeasibilityError(
            f"artifact directory must be absent or empty: {artifact_dir}"
        )
    artifact_dir.mkdir(parents=True, exist_ok=True)
    return build_dir, sdkconfig, artifact_dir


def acceptance_eligibility(
    args: argparse.Namespace, repository: Mapping[str, Any]
) -> Mapping[str, Any]:
    reasons = []
    if args.build_only:
        reasons.append("build-only mode does not execute the acceptance campaign")
    if args.runs != ACCEPTANCE_RUNS:
        reasons.append(
            f"acceptance requires exactly {ACCEPTANCE_RUNS} runs, requested {args.runs}"
        )
    if args.allow_dirty:
        reasons.append("--allow-dirty is development-only")
    if repository["relevant_worktree_status"]:
        reasons.append("probe or runner sources are not committed and clean")
    return {
        "eligible": not reasons,
        "status": "PENDING" if not reasons else "NOT_ELIGIBLE",
        "required_mode": "execute",
        "required_runs": ACCEPTANCE_RUNS,
        "reasons": reasons,
    }


def _require_identity_unchanged(
    *,
    repository_before: Mapping[str, Any],
    sources_before: Mapping[str, str],
    build_before: Mapping[str, str],
    build_dir: Path,
    sdkconfig: Path,
    elf: Path,
    retained_sdkconfig: Path,
    retained_elf: Path,
    toolchain_before: Toolchain,
    require_gdb: bool,
) -> Mapping[str, Any]:
    repository_after = _git_state()
    sources_after = _implementation_hashes()
    build_after = _build_output_hashes(build_dir, sdkconfig, elf)
    if repository_after != repository_before:
        raise FeasibilityError("repository identity changed during the campaign")
    if sources_after != sources_before:
        raise FeasibilityError(
            "probe or runner source hashes changed during the campaign"
        )
    if build_after != build_before:
        raise FeasibilityError(
            "SDKCONFIG, ELF, or flash build outputs changed during the campaign"
        )
    retained_after = {
        "sdkconfig": sha256_file(retained_sdkconfig),
        "app_elf": sha256_file(retained_elf),
    }
    if retained_after["sdkconfig"] != build_after["sdkconfig"]:
        raise FeasibilityError(
            "retained SDKCONFIG changed or differs from the build output"
        )
    if retained_after["app_elf"] != build_after["app_elf"]:
        raise FeasibilityError("retained ELF changed or differs from the build output")
    toolchain_after = discover_toolchain(require_gdb=require_gdb)
    if _toolchain_identity(toolchain_after) != _toolchain_identity(toolchain_before):
        raise FeasibilityError("pinned toolchain identity changed during the campaign")
    return {
        "status": "PASS",
        "repository_after": repository_after,
        "implementation_sources_after": sources_after,
        "build_outputs_after": build_after,
        "retained_outputs_after": retained_after,
        "toolchain_after": _toolchain_identity(toolchain_after),
    }


def _seconds_summary(values: Sequence[float]) -> Mapping[str, float]:
    ordered = sorted(values)
    return {
        "min": ordered[0],
        "median": median(ordered),
        "p95": ordered[math.ceil(0.95 * len(ordered)) - 1],
        "max": ordered[-1],
        "mean": sum(values) / len(values),
        "total": sum(values),
    }


def _observation_ranges(
    runs: Sequence[Mapping[str, Any]],
) -> Mapping[str, Mapping[str, int]]:
    keys = sorted(runs[0]["observation"])
    return {
        key: {
            "min": min(int(run["observation"][key]) for run in runs),
            "max": max(int(run["observation"][key]) for run in runs),
        }
        for key in keys
    }


def run(args: argparse.Namespace) -> int:
    build_dir, sdkconfig, artifact_dir = _session_paths(args)
    report_path = artifact_dir / "report.json"
    report: dict[str, Any] = {
        "schema": 2,
        "package": "FS-WP-002B",
        "mode": "build-only" if args.build_only else "execute",
        "development_allow_dirty": args.allow_dirty,
        "invocation_status": "RUNNING",
        "acceptance": {
            "eligible": False,
            "status": "NOT_EVALUATED",
            "required_mode": "execute",
            "required_runs": ACCEPTANCE_RUNS,
            "reasons": ["repository identity has not been checked"],
        },
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "host": {"platform": platform.platform(), "python": sys.version},
        "paths": {
            "repository": str(REPO_ROOT),
            "probe": str(PROBE_DIR),
            "build": str(build_dir),
            "sdkconfig": str(sdkconfig),
            "artifacts": str(artifact_dir),
        },
    }
    try:
        toolchain = discover_toolchain(require_gdb=not args.build_only)
        report["toolchain"] = {
            "idf_path": str(toolchain.idf_path),
            "idf_version": toolchain.idf_version,
            "idf_revision": toolchain.idf_revision,
            "python": str(toolchain.python),
            "compiler": str(toolchain.compiler),
            "compiler_version": toolchain.compiler_version,
            "compiler_sha256": toolchain.compiler_sha256,
            "compiler_package": EXPECTED_COMPILER_PACKAGE,
            "compiler_expected_sha256": EXPECTED_COMPILER_SHA256,
            "compiler_archive_expected_sha256": EXPECTED_COMPILER_ARCHIVE_SHA256,
            "compiler_archive": (
                str(toolchain.compiler_archive)
                if toolchain.compiler_archive
                else "not-cached"
            ),
            "compiler_archive_sha256": (
                toolchain.compiler_archive_sha256 or "not-cached"
            ),
            "qemu": str(toolchain.qemu),
            "qemu_version": toolchain.qemu_version,
            "qemu_sha256": toolchain.qemu_sha256,
            "qemu_package": EXPECTED_QEMU_PACKAGE,
            "qemu_release_tag": EXPECTED_QEMU_RELEASE_TAG,
            "qemu_tag_object": EXPECTED_QEMU_TAG_OBJECT,
            "qemu_source_revision": EXPECTED_QEMU_SOURCE_REVISION,
            "qemu_archive_expected_sha256": EXPECTED_QEMU_ARCHIVE_SHA256,
            "qemu_archive": (
                str(toolchain.qemu_archive) if toolchain.qemu_archive else "not-cached"
            ),
            "qemu_archive_sha256": toolchain.qemu_archive_sha256 or "not-cached",
            "qemu_dynamic_dependencies": toolchain.qemu_dynamic_dependencies.splitlines(),
            "libslirp": str(toolchain.libslirp),
            "libslirp_sha256": toolchain.libslirp_sha256,
            "gdb": str(toolchain.gdb) if not args.build_only else "not-required",
            "gdb_version": toolchain.gdb_version,
        }
        repository = _git_state()
        report["repository"] = repository
        report["acceptance"] = dict(acceptance_eligibility(args, repository))
        if repository["relevant_worktree_status"] and not args.allow_dirty:
            raise FeasibilityError(
                "probe or runner sources are not immutable; commit them or use --allow-dirty "
                "for development-only runs"
            )
        implementation_sources = _implementation_hashes()
        report["implementation_sources"] = implementation_sources
        build = build_probe(toolchain, build_dir, sdkconfig, artifact_dir)
        report["build"] = build
        if args.build_only:
            report["identity_revalidation"] = _require_identity_unchanged(
                repository_before=repository,
                sources_before=implementation_sources,
                build_before=build["output_hashes"],
                build_dir=build_dir,
                sdkconfig=sdkconfig,
                elf=Path(str(build["elf"])),
                retained_sdkconfig=Path(str(build["retained_sdkconfig"])),
                retained_elf=Path(str(build["retained_elf"])),
                toolchain_before=toolchain,
                require_gdb=False,
            )
        else:
            elf = Path(str(build["elf"]))
            retained_elf = Path(str(build["retained_elf"]))
            report["debug_inspection"] = collect_debug_evidence(
                toolchain, build_dir, artifact_dir, retained_elf
            )
            runs = []
            for index in range(1, args.runs + 1):
                run_dir = artifact_dir / f"run-{index:03d}"
                run_dir.mkdir()
                with tempfile.TemporaryDirectory(
                    prefix=f"domes-qemu-run-{index:03d}-"
                ) as media_root:
                    images = generate_run_images(
                        toolchain, build_dir, Path(media_root) / "images"
                    )
                    command = build_qemu_command(
                        toolchain.qemu, images["flash"], images["efuse"]
                    )
                    evidence = dict(
                        execute_probe(command, run_dir / "qemu.log", args.timeout)
                    )
                    image_integrity = verify_run_images_unchanged(images)
                evidence.update(
                    {
                        "index": index,
                        "qemu_command": command,
                        "flash_sha256": images["flash_sha256"],
                        "efuse_sha256": images["efuse_sha256"],
                        **image_integrity,
                    }
                )
                runs.append(evidence)
                print(
                    f"run {index}/{args.runs}: structural={evidence['structural_signature']} "
                    f"observation={evidence['observation_signature']}",
                    flush=True,
                )

            structural_signature = require_identical_signatures(
                [str(item["structural_signature"]) for item in runs]
            )
            observation_signature = require_identical_signatures(
                [str(item["observation_signature"]) for item in runs]
            )
            flash_sha256 = require_identical_signatures(
                [str(item["flash_sha256"]) for item in runs]
            )
            efuse_sha256 = require_identical_signatures(
                [str(item["efuse_sha256"]) for item in runs]
            )
            if report["debug_inspection"]["flash_sha256"] != flash_sha256:
                raise FeasibilityError(
                    "debug inspection and deterministic campaign used different flash images"
                )
            if report["debug_inspection"]["efuse_sha256"] != efuse_sha256:
                raise FeasibilityError(
                    "debug inspection and deterministic campaign used different eFuse images"
                )
            if report["debug_inspection"]["gdb_elf_sha256"] != build["elf_sha256"]:
                raise FeasibilityError(
                    "GDB inspection did not use the immutable retained campaign ELF"
                )
            seconds = [float(item["seconds"]) for item in runs]
            report["determinism"] = {
                "requested_runs": args.runs,
                "completed_runs": len(runs),
                "structural_unique_signature_count": 1,
                "structural_signature": structural_signature,
                "canonical_structural": runs[0]["canonical_structural"],
                "observation_unique_signature_count": 1,
                "observation_signature": observation_signature,
                "canonical_observation": runs[0]["canonical_observation"],
                "observation_ranges": _observation_ranges(runs),
                "flash_sha256": flash_sha256,
                "efuse_sha256": efuse_sha256,
                "execution_seconds": _seconds_summary(seconds),
                "runs": runs,
            }
            report["identity_revalidation"] = _require_identity_unchanged(
                repository_before=repository,
                sources_before=implementation_sources,
                build_before=build["output_hashes"],
                build_dir=build_dir,
                sdkconfig=sdkconfig,
                elf=elf,
                retained_sdkconfig=Path(str(build["retained_sdkconfig"])),
                retained_elf=retained_elf,
                toolchain_before=toolchain,
                require_gdb=True,
            )
        report["invocation_status"] = "SUCCEEDED"
        if report["acceptance"]["eligible"]:
            report["acceptance"]["status"] = "PASS"
    except Exception as error:
        report["invocation_status"] = "FAILED"
        if report["acceptance"]["eligible"]:
            report["acceptance"]["status"] = "FAIL"
        report["error"] = str(error)
        report_path.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        write_artifact_manifest(artifact_dir)
        raise
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    manifest_path = write_artifact_manifest(artifact_dir)
    print(
        f"{report['invocation_status']} (acceptance {report['acceptance']['status']}): "
        f"evidence report {report_path}; manifest {manifest_path}"
    )
    return 0


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--runs", type=int, default=1, help="fresh deterministic QEMU runs"
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="build twice and omit QEMU/GDB execution",
    )
    parser.add_argument(
        "--build-dir", type=Path, help="new, isolated ESP-IDF build directory"
    )
    parser.add_argument(
        "--artifact-dir", type=Path, help="new or empty evidence directory"
    )
    parser.add_argument(
        "--timeout", type=float, default=15.0, help="seconds allowed per QEMU run"
    )
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="permit uncommitted probe/runner sources for development-only runs",
    )
    args = parser.parse_args(argv)
    if args.runs < 1:
        parser.error("--runs must be at least 1")
    if args.timeout <= MARKER_GRACE_SECONDS:
        parser.error(f"--timeout must exceed {MARKER_GRACE_SECONDS} seconds")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    try:
        with _runner_signal_handlers():
            return run(parse_args(argv))
    except FeasibilityError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
