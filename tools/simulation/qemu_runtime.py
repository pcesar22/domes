#!/usr/bin/env python3
"""Build, validate, and execute the deterministic DOMES QEMU runtime profile."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
TRACE_DIR = SCRIPT_DIR.parent / "trace"
if str(TRACE_DIR) not in sys.path:
    sys.path.insert(0, str(TRACE_DIR))

import generate_runtime_profile as profile_generator
from qemu_feasibility import (
    ANSI_ESCAPE,
    PANIC_PATTERNS,
    FeasibilityError,
    build_qemu_command,
    discover_toolchain,
    execute_until_marker,
    generate_run_images,
    sha256_file,
    verify_run_images_unchanged,
)
from trace_normalizer import (
    canonical_json,
    normalize_trace,
    object_map_from_qemu_log,
    raw_from_qemu_log,
    semantic_projection,
)

REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "domes"
MAIN_DIR = FIRMWARE_DIR / "main"
PROFILE_SPEC = FIRMWARE_DIR / "profiles" / "runtime_profiles.json"
QEMU_DEFAULTS = FIRMWARE_DIR / "sdkconfig.qemu.defaults"
READY_MARKER = "DOMES_QEMU_READY"
MARKER_SCHEMA = 1
ACCEPTANCE_RUNS = 100
EXPECTED_FLASH_SIZE = "8MB"
CI_REQUIRED_ARTIFACTS = frozenset(
    {
        "build.log",
        "domes-fidelity-manifest.json",
        "runtime-report.json",
        "sdkconfig.qemu",
    }
    | {
        artifact
        for index in range(1, ACCEPTANCE_RUNS + 1)
        for artifact in (
            f"runs/{index:03d}/qemu.log",
            f"runs/{index:03d}/trace.raw",
            f"runs/{index:03d}/trace.raw.sha256",
            f"runs/{index:03d}/trace.normalized.json",
            f"runs/{index:03d}/trace.semantic.json",
        )
    }
)

SHARED_MAIN_SOURCES = frozenset(
    {
        "infra/nvsConfig.cpp",
        "infra/diagnostics.cpp",
        "infra/memoryProfiler.cpp",
        "infra/taskStartEvidence.cpp",
        "runtime/runtimeAssembly.cpp",
        "trace/traceBuffer.cpp",
        "trace/traceDumpSnapshot.cpp",
        "trace/kernelTrace.cpp",
        "trace/traceAcceptanceProbe.cpp",
        "trace/traceRecorder.cpp",
        "config/featureManager.cpp",
        "config/modeManager.cpp",
        "game/gameEngine.cpp",
    }
)
QEMU_MAIN_SOURCES = SHARED_MAIN_SOURCES | frozenset(
    {
        "composition/qemuRoot.cpp",
        "platform/qemu/deterministicPlatformInputs.cpp",
        "platform/qemu/qemuPeripheralAdapters.cpp",
    }
)
PHYSICAL_MAIN_SOURCES = SHARED_MAIN_SOURCES | frozenset(
    {
        "main.cpp",
        "infra/watchdog.cpp",
        "infra/taskManager.cpp",
        "infra/crashDumpHandler.cpp",
        "services/wifiManager.cpp",
        "services/firmwareVersion.cpp",
        "services/releaseMetadata.cpp",
        "services/githubClient.cpp",
        "services/otaManager.cpp",
        "transport/uartTransport.cpp",
        "transport/serialOtaReceiver.cpp",
        "transport/bleOtaService.cpp",
        "transport/tcpTransport.cpp",
        "transport/tcpConfigServer.cpp",
        "transport/espNowTransport.cpp",
        "services/espNowService.cpp",
        "platform/physical/espPlatformInputs.cpp",
        "runtime/runtimeEspNowAssembly.cpp",
        "trace/traceCommandHandler.cpp",
        "trace/traceStreamServer.cpp",
        "config/configCommandHandler.cpp",
    }
)
EXPECTED_SOURCES = {
    "physical": PHYSICAL_MAIN_SOURCES,
    "qemu": QEMU_MAIN_SOURCES,
}
EXPECTED_PROFILE_NAMES = {"physical": "physical_nff", "qemu": "qemu_esp32s3"}
EXPECTED_ROOT_OBJECTS = {"physical": "main.cpp.obj", "qemu": "qemuRoot.cpp.obj"}
EXPECTED_PROFILE_DEFINES = {
    "physical": "-DDOMES_RUNTIME_PROFILE_PHYSICAL=1",
    "qemu": "-DDOMES_RUNTIME_PROFILE_QEMU=1",
}
READY_FIELDS = frozenset(
    {
        "schema",
        "status",
        "profile",
        "scenario",
        "manifest_sha256",
        "spec_sha256",
        "sdkconfig_sha256",
        "identity",
        "random_consumed",
        "mode",
        "supported_mask",
        "enabled_mask",
        "expected_tasks",
        "present_tasks",
        "expected_task_mask",
        "started_task_mask",
        "duplicate_task_mask",
        "core0_task_mask",
        "core1_task_mask",
        "task_config_sha256",
        "task_snapshot_sha256",
        "tick_start",
        "tick_end",
        "tick_delta",
        "cpu0_progress",
        "cpu1_progress",
        "adapter_init_mask",
        "adapter_progress_mask",
        "game_state",
        "game_hits",
        "game_misses",
        "game_pad_mask",
        "nvs_roundtrip",
        "trace_count",
        "trace_drops",
        "trace_schema",
        "trace_causal_id",
        "trace_discontinuities",
        "trace_disabled_us",
        "trace_enabled_us",
        "failure_mask",
    }
)
STRING_FIELDS = frozenset(
    {
        "status",
        "profile",
        "scenario",
        "manifest_sha256",
        "spec_sha256",
        "sdkconfig_sha256",
        "identity",
        "mode",
        "game_state",
        "task_config_sha256",
        "task_snapshot_sha256",
    }
)
FORBIDDEN_RUNTIME_PATTERNS = (
    "wifi_init:",
    "BLE OTA service initialized",
    "ESP-NOW service initialized",
    "TCP config server started",
    "Serial OTA receiver started",
)
HASH = re.compile(r"^[0-9a-f]{64}$")
INIT_STAGE_CALL = re.compile(r'advanceInitStage\(\s*initOrder\s*,\s*"([^"]+)"\s*\)')
QEMU_FORBIDDEN_SYMBOLS = (
    re.compile(r"^esp_wifi_init$"),
    re.compile(r"^esp_now_init$"),
    re.compile(r"^nimble_port_init$"),
    re.compile(r"^esp_http_client_init$"),
    re.compile(r"^esp_https_ota$"),
    re.compile(
        r"^domes::(?:BleOtaService|EspNowService|EspNowTransport|GithubClient|OtaManager|"
        r"SerialOtaReceiver|TcpConfigServer|TcpTransport|UartTransport|WifiManager)::"
    ),
)
QEMU_ALLOWED_ARCHIVE_ORIGINS = frozenset(
    {
        "esp-idf/app_update",
        "esp-idf/bootloader_support",
        "esp-idf/cxx",
        "esp-idf/efuse",
        "esp-idf/esp_app_format",
        "esp-idf/esp_common",
        "esp-idf/esp_driver_gpio",
        "esp-idf/esp_driver_gptimer",
        "esp-idf/esp_driver_spi",
        "esp-idf/esp_driver_uart",
        "esp-idf/esp_driver_usb_serial_jtag",
        "esp-idf/esp_hw_support",
        "esp-idf/esp_mm",
        "esp-idf/esp_partition",
        "esp-idf/esp_pm",
        "esp-idf/esp_phy",
        "esp-idf/esp_ringbuf",
        "esp-idf/esp_rom",
        "esp-idf/esp_security",
        "esp-idf/esp_system",
        "esp-idf/esp_timer",
        "esp-idf/esp_vfs_console",
        "esp-idf/freertos",
        "esp-idf/hal",
        "esp-idf/heap",
        "esp-idf/log",
        "esp-idf/main",
        "esp-idf/mbedtls",
        "esp-idf/newlib",
        "esp-idf/nvs_flash",
        "esp-idf/nvs_sec_provider",
        "esp-idf/pthread",
        "esp-idf/soc",
        "esp-idf/spi_flash",
        "esp-idf/vfs",
        "esp-idf/xtensa",
        "toolchain",
    }
)
QEMU_ALLOWED_DIRECT_OBJECT_ORIGINS = frozenset({"build/project_elf", "toolchain"})
LINK_MAP_HEADER = "Archive member included to satisfy reference by file (symbol)"
LINK_MAP_ARCHIVE_MEMBER = re.compile(r"^(\S.*?\.a)\(([^()]+)\)$")
LINK_MAP_LOAD_INPUT = re.compile(r"^LOAD (.+)$")
LINK_MAP_DIRECT_OBJECT_SUFFIXES = (".o", ".obj")
AR_MAGIC_HEADERS = frozenset({b"!<arch>\n", b"!<thin>\n"})


class RuntimeProfileError(FeasibilityError):
    """Raised when build or runtime evidence violates the profile contract."""


def _read_json(path: Path, label: str) -> Mapping[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeProfileError(f"cannot read {label} {path}: {error}") from error
    if not isinstance(value, dict):
        raise RuntimeProfileError(f"{label} must be a JSON object: {path}")
    return value


def _canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def _is_positive_number(value: Any) -> bool:
    return (type(value) is int and value > 0) or (
        type(value) is float and math.isfinite(value) and value > 0
    )


def _task_config_sha256(manifest: Mapping[str, Any]) -> str:
    payload = [
        {
            key: task[key]
            for key in (
                "id",
                "name",
                "trace_id",
                "stack_size",
                "priority",
                "core_affinity",
                "watchdog",
                "evidence_mask",
            )
        }
        for task in manifest["tasks"]
        if task["presence"] == "required"
    ]
    return hashlib.sha256(_canonical_bytes(payload)).hexdigest()


def _resolve_path(value: str, base: Path) -> Path:
    path = Path(value)
    return path.resolve() if path.is_absolute() else (base / path).resolve()


def _main_sources(description: Mapping[str, Any]) -> frozenset[str]:
    try:
        main = description["build_component_info"]["main"]
        source_root = Path(main["dir"]).resolve()
        sources = main["sources"]
    except (KeyError, TypeError) as error:
        raise RuntimeProfileError(
            "project description omits the main component"
        ) from error

    relative: set[str] = set()
    for raw_source in sources:
        source = Path(raw_source).resolve()
        try:
            name = source.relative_to(source_root).as_posix()
        except ValueError as error:
            raise RuntimeProfileError(
                f"main source is outside its component: {source}"
            ) from error
        if name in relative:
            raise RuntimeProfileError(f"duplicate main source: {name}")
        relative.add(name)
    return frozenset(relative)


def validate_source_closure(
    profile_key: str, description: Mapping[str, Any]
) -> frozenset[str]:
    actual = _main_sources(description)
    expected = EXPECTED_SOURCES[profile_key]
    if actual != expected:
        raise RuntimeProfileError(
            f"{profile_key} main source closure differs; "
            f"missing={sorted(expected - actual)}, extra={sorted(actual - expected)}"
        )
    return actual


def validate_init_order_binding(
    profile_key: str,
    manifest: Mapping[str, Any],
    source_text: str | None = None,
) -> list[str]:
    root = MAIN_DIR / str(manifest["root"])
    if source_text is None:
        try:
            source_text = root.read_text(encoding="utf-8")
        except OSError as error:
            raise RuntimeProfileError(
                f"cannot read {profile_key} root {root}: {error}"
            ) from error
    actual = INIT_STAGE_CALL.findall(source_text)
    expected = manifest.get("init_order")
    if actual != expected:
        raise RuntimeProfileError(
            f"{profile_key} init-order binding differs; expected={expected!r}, actual={actual!r}"
        )
    return actual


def _validate_compile_commands(
    build_dir: Path, profile_key: str, sources: frozenset[str]
) -> None:
    path = build_dir / "compile_commands.json"
    try:
        commands = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeProfileError(
            f"cannot read compile commands {path}: {error}"
        ) from error
    if not isinstance(commands, list):
        raise RuntimeProfileError("compile_commands.json must be an array")

    entries: dict[str, str] = {}
    for entry in commands:
        try:
            source = Path(entry["file"]).resolve()
            relative = source.relative_to(MAIN_DIR.resolve()).as_posix()
        except (KeyError, TypeError, ValueError):
            continue
        command = entry.get("command")
        if command is None:
            command = " ".join(entry.get("arguments", ()))
        if relative in entries:
            raise RuntimeProfileError(
                f"duplicate compile command for main source {relative}"
            )
        entries[relative] = str(command)

    if frozenset(entries) != sources:
        raise RuntimeProfileError(
            "compile commands do not match the resolved main source closure"
        )
    expected_define = EXPECTED_PROFILE_DEFINES[profile_key]
    opposite_define = EXPECTED_PROFILE_DEFINES[
        "qemu" if profile_key == "physical" else "physical"
    ]
    for source, command in entries.items():
        if expected_define not in command or opposite_define in command:
            raise RuntimeProfileError(f"wrong profile compile definition for {source}")


def _linked_archive_members(map_text: str) -> frozenset[tuple[str, str]]:
    lines = map_text.splitlines()
    try:
        start = lines.index(LINK_MAP_HEADER) + 1
        end = lines.index("Discarded input sections", start)
    except ValueError as error:
        raise RuntimeProfileError(
            "linker map omits the archive-member contribution section"
        ) from error

    members: set[tuple[str, str]] = set()
    for line in lines[start:end]:
        match = LINK_MAP_ARCHIVE_MEMBER.fullmatch(line)
        if match:
            members.add((match.group(1), match.group(2)))
    if not members:
        raise RuntimeProfileError("linker map records no contributing archive members")
    return frozenset(members)


def _linked_direct_objects(map_text: str, build_dir: Path) -> frozenset[str]:
    objects: set[str] = set()
    for line in map_text.splitlines():
        match = LINK_MAP_LOAD_INPUT.fullmatch(line)
        if not match:
            continue
        linked_input = match.group(1)
        if linked_input.endswith(".a"):
            archive = _resolve_path(linked_input, build_dir)
            try:
                with archive.open("rb") as input_file:
                    magic = input_file.read(8)
            except OSError as error:
                raise RuntimeProfileError(
                    f"cannot inspect linker LOAD input {linked_input}: {error}"
                ) from error
            if magic not in AR_MAGIC_HEADERS:
                raise RuntimeProfileError(
                    "linker LOAD input has .a suffix without archive magic: "
                    f"{linked_input}"
                )
            continue
        if not linked_input.endswith(LINK_MAP_DIRECT_OBJECT_SUFFIXES):
            raise RuntimeProfileError(f"unclassified linker LOAD input: {linked_input}")
        objects.add(linked_input)
    return frozenset(objects)


def _component_origin(parts: Sequence[str], archive: str) -> str:
    if (
        len(parts) < 3
        or parts[0] != "esp-idf"
        or any(part in {"", ".", ".."} for part in parts)
    ):
        raise RuntimeProfileError(f"malformed ESP-IDF archive origin: {archive}")
    return f"esp-idf/{parts[1]}"


def _toolchain_package_root(description: Mapping[str, Any]) -> Path:
    compiler = Path(str(description.get("c_compiler", ""))).resolve()
    if compiler.parent.name != "bin" or compiler.parent.parent.name != "xtensa-esp-elf":
        raise RuntimeProfileError(f"unexpected Xtensa compiler path: {compiler}")
    return compiler.parent.parent.parent


def _linked_archive_origin(
    archive: str, build_dir: Path, description: Mapping[str, Any]
) -> str:
    path = Path(archive)
    if not path.is_absolute():
        return _component_origin(path.parts, archive)

    resolved = path.resolve()
    idf_path = description.get("idf_path")
    if not isinstance(idf_path, str) or not idf_path:
        raise RuntimeProfileError("project description omits the ESP-IDF path")
    roots = (
        build_dir.resolve() / "esp-idf",
        Path(idf_path).resolve() / "components",
    )
    for root in roots:
        try:
            relative = resolved.relative_to(root)
        except ValueError:
            continue
        return _component_origin(("esp-idf", *relative.parts), archive)

    try:
        resolved.relative_to(_toolchain_package_root(description))
    except ValueError as error:
        raise RuntimeProfileError(
            f"unexpected linked archive origin: {archive}"
        ) from error
    return "toolchain"


def _linked_direct_object_origin(
    obj: str, build_dir: Path, description: Mapping[str, Any]
) -> str:
    path = Path(obj)
    if not path.is_absolute():
        expected = Path("CMakeFiles/domes.elf.dir/project_elf_src_esp32s3.c.obj")
        if path == expected:
            return "build/project_elf"
        raise RuntimeProfileError(f"unexpected directly linked object origin: {obj}")

    try:
        path.resolve().relative_to(_toolchain_package_root(description))
    except ValueError as error:
        raise RuntimeProfileError(
            f"unexpected directly linked object origin: {obj}"
        ) from error
    return "toolchain"


def validate_qemu_linked_component_closure(
    map_text: str, build_dir: Path, description: Mapping[str, Any]
) -> Mapping[str, Any]:
    members = _linked_archive_members(map_text)
    archive_origins = {
        _linked_archive_origin(archive, build_dir, description)
        for archive, _ in members
    }
    unapproved_archives = archive_origins - QEMU_ALLOWED_ARCHIVE_ORIGINS
    if unapproved_archives:
        raise RuntimeProfileError(
            "QEMU ELF links unapproved archive component origins: "
            f"{sorted(unapproved_archives)}"
        )

    direct_objects = _linked_direct_objects(map_text, build_dir)
    direct_object_origins = {
        _linked_direct_object_origin(obj, build_dir, description)
        for obj in direct_objects
    }
    unapproved_objects = direct_object_origins - QEMU_ALLOWED_DIRECT_OBJECT_ORIGINS
    if unapproved_objects:
        raise RuntimeProfileError(
            "QEMU ELF links unapproved direct object origins: "
            f"{sorted(unapproved_objects)}"
        )
    return {
        "archive_origins": sorted(archive_origins),
        "archive_member_count": len(members),
        "direct_object_origins": sorted(direct_object_origins),
        "direct_object_count": len(direct_objects),
    }


def _validate_qemu_linked_component_closure(
    build_dir: Path, description: Mapping[str, Any]
) -> Mapping[str, Any]:
    map_path = build_dir / "domes.map"
    try:
        map_text = map_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        raise RuntimeProfileError(
            f"cannot read linker map {map_path}: {error}"
        ) from error
    return validate_qemu_linked_component_closure(map_text, build_dir, description)


def _nm_for_description(description: Mapping[str, Any]) -> Path:
    compiler = Path(str(description.get("c_compiler", ""))).resolve()
    if not compiler.name.endswith("-gcc"):
        raise RuntimeProfileError(f"unexpected Xtensa compiler path: {compiler}")
    nm = compiler.with_name(compiler.name.removesuffix("-gcc") + "-nm")
    if not nm.is_file():
        raise RuntimeProfileError(f"Xtensa nm is unavailable beside the compiler: {nm}")
    return nm


def reject_qemu_forbidden_symbols(nm_output: str) -> None:
    forbidden: list[str] = []
    for line in nm_output.splitlines():
        match = re.match(r"^[0-9a-fA-F]+\s+\S\s+(.+)$", line.strip())
        if not match:
            continue
        symbol = match.group(1)
        if any(pattern.search(symbol) for pattern in QEMU_FORBIDDEN_SYMBOLS):
            forbidden.append(symbol)
    if forbidden:
        raise RuntimeProfileError(
            f"QEMU ELF links disabled vendor/service symbols: {sorted(set(forbidden))}"
        )


def _validate_qemu_symbol_denylist(
    build_dir: Path, description: Mapping[str, Any]
) -> list[str]:
    elf = _resolve_path(str(description["app_elf"]), build_dir)
    completed = subprocess.run(
        [_nm_for_description(description), "-C", "--defined-only", elf],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=30.0,
    )
    if completed.returncode != 0:
        raise RuntimeProfileError(
            f"nm failed while checking QEMU disabled symbols: {completed.stdout.strip()}"
        )
    reject_qemu_forbidden_symbols(completed.stdout)
    return [pattern.pattern for pattern in QEMU_FORBIDDEN_SYMBOLS]


def _validate_root_symbol(
    build_dir: Path, profile_key: str, description: Mapping[str, Any]
) -> None:
    archive = _resolve_path(
        str(description["build_component_info"]["main"]["file"]), build_dir
    )
    completed = subprocess.run(
        [_nm_for_description(description), "-A", "--defined-only", archive],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=30.0,
    )
    if completed.returncode != 0:
        raise RuntimeProfileError(
            f"nm failed for {archive}: {completed.stdout.strip()}"
        )
    definitions = [
        line for line in completed.stdout.splitlines() if line.endswith(" T app_main")
    ]
    if len(definitions) != 1:
        raise RuntimeProfileError(
            f"expected exactly one app_main definition, found {len(definitions)}"
        )
    expected_object = EXPECTED_ROOT_OBJECTS[profile_key]
    other_object = EXPECTED_ROOT_OBJECTS[
        "qemu" if profile_key == "physical" else "physical"
    ]
    if expected_object not in definitions[0] or other_object in definitions[0]:
        raise RuntimeProfileError(
            f"app_main came from the wrong root: {definitions[0]}"
        )

    map_path = build_dir / "domes.map"
    try:
        map_text = map_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        raise RuntimeProfileError(
            f"cannot read linker map {map_path}: {error}"
        ) from error
    if expected_object not in map_text or other_object in map_text:
        raise RuntimeProfileError(f"linker map violates {profile_key} root closure")


def _validate_embedded_identity(
    build_dir: Path,
    description: Mapping[str, Any],
    manifest: Mapping[str, Any],
    manifest_sha256: str,
) -> Mapping[str, str]:
    elf = _resolve_path(str(description["app_elf"]), build_dir)
    try:
        image = elf.read_bytes()
    except OSError as error:
        raise RuntimeProfileError(
            f"cannot read application ELF {elf}: {error}"
        ) from error
    identities = {
        "profile": str(manifest["profile"]),
        "manifest_sha256": manifest_sha256,
        "spec_sha256": str(manifest["spec_sha256"]),
        "sdkconfig_sha256": str(manifest["sdkconfig_sha256"]),
        "task_config_sha256": _task_config_sha256(manifest),
    }
    missing = [
        name for name, value in identities.items() if value.encode() not in image
    ]
    if missing:
        raise RuntimeProfileError(
            f"ELF omits generated profile identity fields: {missing}"
        )
    return identities


def validate_build(build_dir: Path, profile_key: str) -> Mapping[str, Any]:
    build_dir = build_dir.resolve()
    description_path = build_dir / "project_description.json"
    manifest_path = build_dir / "domes-fidelity-manifest.json"
    description = _read_json(description_path, "project description")
    manifest = _read_json(manifest_path, "fidelity manifest")
    if description.get("target") != "esp32s3":
        raise RuntimeProfileError(f"{profile_key} build target is not esp32s3")
    if manifest.get("profile_key") != profile_key:
        raise RuntimeProfileError(
            f"expected {profile_key} manifest, found {manifest.get('profile_key')!r}"
        )
    if manifest.get("profile") != EXPECTED_PROFILE_NAMES[profile_key]:
        raise RuntimeProfileError(f"unexpected {profile_key} profile name")

    config_file = _resolve_path(str(description.get("config_file", "")), build_dir)
    try:
        resolved = profile_generator.resolve_profile(
            PROFILE_SPEC, profile_key, config_file
        )
    except (OSError, profile_generator.ProfileError) as error:
        raise RuntimeProfileError(
            f"cannot resolve {profile_key} profile: {error}"
        ) from error
    if manifest != resolved["manifest"]:
        raise RuntimeProfileError(
            f"{profile_key} manifest differs from the current spec/config"
        )
    manifest_sha256 = sha256_file(manifest_path)
    if manifest_sha256 != resolved["manifest_sha256"]:
        raise RuntimeProfileError(f"{profile_key} manifest canonical hash differs")

    validate_init_order_binding(profile_key, manifest)
    sources = validate_source_closure(profile_key, description)
    _validate_compile_commands(build_dir, profile_key, sources)
    _validate_root_symbol(build_dir, profile_key, description)
    disabled_symbol_denylist = (
        _validate_qemu_symbol_denylist(build_dir, description)
        if profile_key == "qemu"
        else []
    )
    linked_component_closure = (
        _validate_qemu_linked_component_closure(build_dir, description)
        if profile_key == "qemu"
        else None
    )
    identities = _validate_embedded_identity(
        build_dir, description, manifest, manifest_sha256
    )
    return {
        "profile": profile_key,
        "profile_name": manifest["profile"],
        "build_dir": str(build_dir),
        "sdkconfig": str(config_file),
        "source_count": len(sources),
        "sources": sorted(sources),
        "manifest": manifest,
        "manifest_sha256": manifest_sha256,
        "task_config_sha256": identities["task_config_sha256"],
        "disabled_symbol_denylist": disabled_symbol_denylist,
        "linked_component_closure": linked_component_closure,
        "elf": str(_resolve_path(str(description["app_elf"]), build_dir)),
        "elf_sha256": sha256_file(
            _resolve_path(str(description["app_elf"]), build_dir)
        ),
    }


def validate_build_pair(physical_build: Path, qemu_build: Path) -> Mapping[str, Any]:
    physical = validate_build(physical_build, "physical")
    qemu = validate_build(qemu_build, "qemu")
    physical_tasks = {task["id"]: task for task in physical["manifest"]["tasks"]}
    compared: list[str] = []
    fields = (
        "name",
        "stack_size",
        "priority",
        "core_affinity",
        "watchdog",
        "evidence_mask",
    )
    for task in qemu["manifest"]["tasks"]:
        if task["presence"] != "required":
            continue
        physical_task = physical_tasks[task["id"]]
        if any(task[field] != physical_task[field] for field in fields):
            raise RuntimeProfileError(f"task configuration drift for {task['id']}")
        compared.append(task["id"])
    return {
        "status": "PASS",
        "physical": {
            key: value for key, value in physical.items() if key != "manifest"
        },
        "qemu": {key: value for key, value in qemu.items() if key != "manifest"},
        "shared_task_configs": compared,
    }


def parse_ready_marker(log: str) -> Mapping[str, str]:
    clean = ANSI_ESCAPE.sub("", log).replace("\r", "")
    matches = []
    for line in clean.splitlines():
        offset = line.find(READY_MARKER)
        if offset >= 0:
            matches.append(line[offset:].strip())
    if len(matches) != 1:
        raise RuntimeProfileError(
            f"expected exactly one {READY_MARKER} marker, found {len(matches)}"
        )
    fields: dict[str, str] = {}
    for token in matches[0][len(READY_MARKER) :].split():
        if token.count("=") != 1:
            raise RuntimeProfileError(f"malformed readiness token: {token!r}")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise RuntimeProfileError(
                f"invalid or duplicate readiness field: {token!r}"
            )
        fields[key] = value
    if frozenset(fields) != READY_FIELDS:
        raise RuntimeProfileError(
            "readiness fields differ; "
            f"missing={sorted(READY_FIELDS - fields.keys())}, "
            f"extra={sorted(fields.keys() - READY_FIELDS)}"
        )
    return fields


def _integer_fields(fields: Mapping[str, str]) -> Mapping[str, int | str]:
    converted: dict[str, int | str] = {}
    for key, value in fields.items():
        if key in STRING_FIELDS:
            converted[key] = value
            continue
        try:
            converted[key] = int(value, 0)
        except ValueError as error:
            raise RuntimeProfileError(
                f"readiness field {key} is not an integer: {value!r}"
            ) from error
    return converted


def analyze_runtime_log(
    log: str, manifest: Mapping[str, Any], manifest_sha256: str
) -> Mapping[str, int | str]:
    for pattern in PANIC_PATTERNS:
        if pattern in log:
            raise RuntimeProfileError(
                f"target panic or reset marker observed: {pattern}"
            )
    for pattern in FORBIDDEN_RUNTIME_PATTERNS:
        if pattern in log:
            raise RuntimeProfileError(
                f"disabled vendor/runtime path initialized: {pattern}"
            )
    if log.count("ESP-ROM:esp32s3") != 1:
        raise RuntimeProfileError("target must have exactly one ESP32-S3 boot")

    result = _integer_fields(parse_ready_marker(log))
    required_tasks = [
        task for task in manifest["tasks"] if task["presence"] == "required"
    ]
    expected_task_mask = sum(task["evidence_mask"] for task in required_tasks)
    scenario = manifest["readiness_scenario"]
    exact: Mapping[str, int | str] = {
        "schema": MARKER_SCHEMA,
        "status": "PASS",
        "profile": manifest["profile"],
        "scenario": scenario["name"],
        "manifest_sha256": manifest_sha256,
        "spec_sha256": manifest["spec_sha256"],
        "sdkconfig_sha256": manifest["sdkconfig_sha256"],
        "identity": manifest["inputs"]["identity"],
        "random_consumed": manifest["inputs"]["random_u32_count"],
        "mode": "idle",
        "supported_mask": manifest["supported_feature_mask"],
        "enabled_mask": manifest["ready_enabled_feature_mask"],
        "expected_tasks": len(required_tasks),
        "present_tasks": len(required_tasks),
        "expected_task_mask": expected_task_mask,
        "started_task_mask": expected_task_mask,
        "duplicate_task_mask": 0,
        "task_config_sha256": _task_config_sha256(manifest),
        "tick_delta": scenario["dwell_ms"],
        "cpu0_progress": 1,
        "cpu1_progress": 1,
        "adapter_init_mask": 0x1F,
        "adapter_progress_mask": 0x1F,
        "game_state": "READY",
        "game_hits": 1,
        "game_misses": 0,
        "game_pad_mask": 1 << scenario["touch_pad"],
        "nvs_roundtrip": 1,
        "trace_drops": 0,
        "trace_schema": 1,
        "trace_causal_id": 1,
        "trace_discontinuities": 0,
        "failure_mask": 0,
    }
    for key, expected in exact.items():
        if result[key] != expected:
            raise RuntimeProfileError(
                f"readiness {key} expected {expected!r}, found {result[key]!r}"
            )
    if result["tick_end"] - result["tick_start"] != result["tick_delta"]:
        raise RuntimeProfileError("readiness target tick fields are inconsistent")
    core0_mask = int(result["core0_task_mask"])
    core1_mask = int(result["core1_task_mask"])
    required_core0_mask = sum(
        task["evidence_mask"] for task in required_tasks if task["core_affinity"] == 0
    )
    required_core1_mask = sum(
        task["evidence_mask"] for task in required_tasks if task["core_affinity"] == 1
    )
    unpinned_mask = sum(
        task["evidence_mask"] for task in required_tasks if task["core_affinity"] == -1
    )
    if core0_mask & core1_mask:
        raise RuntimeProfileError("task-entry core evidence overlaps")
    if (
        core0_mask == 0
        or core1_mask == 0
        or (core0_mask | core1_mask) != expected_task_mask
        or (core0_mask & required_core1_mask) != 0
        or (core1_mask & required_core0_mask) != 0
        or (core0_mask & required_core0_mask) != required_core0_mask
        or (core1_mask & required_core1_mask) != required_core1_mask
        or ((core0_mask | core1_mask) & unpinned_mask) != unpinned_mask
    ):
        raise RuntimeProfileError(
            "task-entry evidence violates required target-core affinity"
        )
    if int(result["trace_count"]) <= 0:
        raise RuntimeProfileError("readiness drill emitted no target trace evidence")
    if int(result["trace_enabled_us"]) < int(result["trace_disabled_us"]):
        raise RuntimeProfileError(
            "trace enabled overhead is smaller than disabled baseline"
        )
    for key in (
        "manifest_sha256",
        "spec_sha256",
        "sdkconfig_sha256",
        "task_config_sha256",
        "task_snapshot_sha256",
    ):
        if not HASH.fullmatch(str(result[key])):
            raise RuntimeProfileError(f"readiness {key} is not lowercase SHA-256")
    return result


def canonical_ready_signature(result: Mapping[str, int | str]) -> str:
    normalized = {
        key: value
        for key, value in result.items()
        if key not in {"tick_start", "tick_end"}
    }
    return hashlib.sha256(_canonical_bytes(normalized)).hexdigest()


def _flash_size(build_dir: Path) -> str:
    settings = _read_json(build_dir / "flasher_args.json", "flasher arguments")
    try:
        flash_size = settings["flash_settings"]["flash_size"]
    except (KeyError, TypeError) as error:
        raise RuntimeProfileError("flasher arguments omit flash size") from error
    if flash_size != EXPECTED_FLASH_SIZE:
        raise RuntimeProfileError(
            f"runtime profile requires {EXPECTED_FLASH_SIZE} flash, found {flash_size!r}"
        )
    return str(flash_size)


def _run_logged(
    command: Sequence[str], cwd: Path, log_path: Path, timeout: float
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
        raise RuntimeProfileError(
            f"command failed: {' '.join(command)}: {error}"
        ) from error
    log_path.write_bytes(completed.stdout)
    if completed.returncode != 0:
        raise RuntimeProfileError(
            f"command exited {completed.returncode}; see {log_path}"
        )
    return time.monotonic() - started


def _build_qemu(
    toolchain: Any, build_dir: Path, sdkconfig: Path, log_path: Path
) -> Mapping[str, Any]:
    if build_dir.exists() and any(build_dir.iterdir()):
        raise RuntimeProfileError(
            f"QEMU build directory must be absent or empty: {build_dir}"
        )
    if sdkconfig.exists():
        raise RuntimeProfileError(
            f"isolated QEMU SDKCONFIG already exists: {sdkconfig}"
        )
    build_dir.mkdir(parents=True, exist_ok=True)
    command = [
        str(toolchain.python),
        str(toolchain.idf_path / "tools" / "idf.py"),
        "-C",
        str(FIRMWARE_DIR),
        "-B",
        str(build_dir),
        "-D",
        "IDF_TARGET=esp32s3",
        "-D",
        f"SDKCONFIG={sdkconfig}",
        "-D",
        f"SDKCONFIG_DEFAULTS={QEMU_DEFAULTS}",
        "build",
    ]
    seconds = _run_logged(command, REPO_ROOT, log_path, 1800.0)
    return {
        "skipped": False,
        "command": command,
        "seconds": seconds,
        "sdkconfig": str(sdkconfig),
        "log": str(log_path),
        "log_sha256": sha256_file(log_path),
    }


def _source_snapshot() -> Mapping[str, str]:
    paths = [
        PROFILE_SPEC,
        QEMU_DEFAULTS,
        FIRMWARE_DIR / "main" / "CMakeLists.txt",
        FIRMWARE_DIR / "main" / "Kconfig.projbuild",
        Path(__file__).resolve(),
        SCRIPT_DIR / "generate_runtime_profile.py",
        SCRIPT_DIR / "qemu_feasibility.py",
    ]
    paths.extend(
        path
        for path in MAIN_DIR.rglob("*")
        if path.is_file() and path.suffix in {".cpp", ".hpp", ".h", ".c"}
    )
    return {
        str(path.relative_to(REPO_ROOT)): sha256_file(path)
        for path in sorted(set(paths))
    }


def _git_identity(allow_dirty: bool) -> Mapping[str, Any]:
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.strip()
    status = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=all"],
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout
    if status and not allow_dirty:
        raise RuntimeProfileError(
            "acceptance runner requires a clean candidate; use --allow-dirty for development"
        )
    return {
        "head": head,
        "dirty": bool(status),
        "status_sha256": hashlib.sha256(status.encode()).hexdigest(),
    }


def _write_artifact_manifest(artifact_dir: Path) -> Path:
    path = artifact_dir / "artifact-manifest.json"
    files = {
        str(candidate.relative_to(artifact_dir)): sha256_file(candidate)
        for candidate in sorted(artifact_dir.rglob("*"))
        if candidate.is_file() and candidate != path
    }
    path.write_bytes(_canonical_bytes({"schema_version": 1, "files": files}))
    return path


def verify_ci_report(report_path: Path, expected_head: str) -> Mapping[str, Any]:
    if not re.fullmatch(r"[0-9a-f]{40}", expected_head):
        raise RuntimeProfileError(f"invalid expected CI commit: {expected_head!r}")

    report_path = report_path.resolve()
    artifact_dir = report_path.parent
    report = _read_json(report_path, "QEMU runtime report")
    expected: Mapping[str, Any] = {
        "schema_version": 1,
        "status": "PASS",
        "qualification": "accepted",
        "runs": ACCEPTANCE_RUNS,
        "required_acceptance_runs": ACCEPTANCE_RUNS,
    }
    for key, value in expected.items():
        actual = report.get(key)
        if type(actual) is not type(value) or actual != value:
            raise RuntimeProfileError(
                f"QEMU runtime report {key} expected {value!r}, found {actual!r}"
            )

    git = report.get("git")
    if (
        not isinstance(git, dict)
        or git.get("head") != expected_head
        or git.get("dirty") is not False
    ):
        raise RuntimeProfileError(
            "QEMU runtime report is not tied to the clean expected CI commit"
        )

    build = report.get("build")
    build_command = build.get("command") if isinstance(build, dict) else None
    expected_build_tail = [
        "-C",
        str(FIRMWARE_DIR),
        "-B",
        str((artifact_dir.parent / "build").resolve()),
        "-D",
        "IDF_TARGET=esp32s3",
        "-D",
        f"SDKCONFIG={(artifact_dir / 'sdkconfig.qemu').resolve()}",
        "-D",
        f"SDKCONFIG_DEFAULTS={QEMU_DEFAULTS}",
        "build",
    ]
    if (
        not isinstance(build, dict)
        or build.get("skipped") is not False
        or not isinstance(build_command, list)
        or len(build_command) != 13
        or not all(isinstance(part, str) for part in build_command)
        or Path(build_command[0]).name != "python"
        or Path(build_command[1]).name != "idf.py"
        or build_command[2:] != expected_build_tail
        or not _is_positive_number(build.get("seconds"))
        or not isinstance(build.get("sdkconfig"), str)
        or not isinstance(build.get("log"), str)
        or not isinstance(build.get("log_sha256"), str)
    ):
        raise RuntimeProfileError(
            "QEMU CI report does not prove a fresh firmware build"
        )

    signature = report.get("ready_signature")
    if not isinstance(signature, str) or not HASH.fullmatch(signature):
        raise RuntimeProfileError(
            "QEMU runtime report has an invalid readiness signature"
        )
    trace_signature = report.get("trace_signature")
    if not isinstance(trace_signature, str) or not HASH.fullmatch(trace_signature):
        raise RuntimeProfileError(
            "QEMU runtime report has an invalid normalized trace signature"
        )
    run_evidence = report.get("run_evidence")
    if not isinstance(run_evidence, list) or len(run_evidence) != ACCEPTANCE_RUNS:
        count = len(run_evidence) if isinstance(run_evidence, list) else "invalid"
        raise RuntimeProfileError(
            f"expected {ACCEPTANCE_RUNS} QEMU run records, found {count}"
        )
    manifest_path = artifact_dir / "artifact-manifest.json"
    manifest = _read_json(manifest_path, "QEMU artifact manifest")
    if (
        type(manifest.get("schema_version")) is not int
        or manifest["schema_version"] != 1
    ):
        raise RuntimeProfileError("QEMU artifact manifest schema must be 1")
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise RuntimeProfileError("QEMU artifact manifest must contain files")
    manifest_files = set(files)
    missing = CI_REQUIRED_ARTIFACTS - manifest_files
    if missing:
        raise RuntimeProfileError(
            f"QEMU artifact manifest omits required files: {sorted(missing)}"
        )
    actual_files = {
        str(candidate.relative_to(artifact_dir))
        for candidate in artifact_dir.rglob("*")
        if candidate.is_file() and candidate != manifest_path
    }
    if manifest_files != actual_files:
        raise RuntimeProfileError(
            "QEMU artifact manifest file set differs from generated output: "
            f"missing={sorted(actual_files - manifest_files)} "
            f"extra={sorted(manifest_files - actual_files)}"
        )
    for relative, expected_hash in files.items():
        relative_path = Path(relative)
        if relative_path.is_absolute() or ".." in relative_path.parts:
            raise RuntimeProfileError(
                f"QEMU artifact path escapes output directory: {relative!r}"
            )
        if not isinstance(expected_hash, str) or not HASH.fullmatch(expected_hash):
            raise RuntimeProfileError(f"QEMU artifact has invalid SHA-256: {relative}")
        candidate = artifact_dir / relative_path
        if sha256_file(candidate) != expected_hash:
            raise RuntimeProfileError(f"QEMU artifact hash mismatch: {relative}")

    if (
        Path(build["sdkconfig"]).resolve()
        != (artifact_dir / "sdkconfig.qemu").resolve()
        or Path(build["log"]).resolve() != (artifact_dir / "build.log").resolve()
        or build["log_sha256"] != files["build.log"]
    ):
        raise RuntimeProfileError(
            "QEMU CI report is not bound to its fresh build artifacts"
        )

    fidelity = report.get("fidelity_manifest")
    fidelity_path = artifact_dir / "domes-fidelity-manifest.json"
    if (
        not isinstance(fidelity, dict)
        or fidelity.get("path") != fidelity_path.name
        or not isinstance(fidelity.get("sha256"), str)
        or not HASH.fullmatch(fidelity["sha256"])
        or files.get(fidelity_path.name) != fidelity["sha256"]
    ):
        raise RuntimeProfileError(
            "QEMU runtime report has invalid fidelity-manifest evidence"
        )
    runtime_manifest = _read_json(fidelity_path, "retained QEMU fidelity manifest")

    for expected_index, run in enumerate(run_evidence, start=1):
        if (
            not isinstance(run, dict)
            or type(run.get("index")) is not int
            or run["index"] != expected_index
        ):
            raise RuntimeProfileError(
                f"QEMU run record {expected_index} is missing or out of order"
            )
        execution = run.get("execution")
        if (
            not isinstance(execution, dict)
            or execution.get("termination") != "marker_observed_then_runner_sigterm"
            or execution.get("termination_action") != "sigterm"
            or type(execution.get("qemu_returncode")) is not int
            or execution["qemu_returncode"] != 0
            or not _is_positive_number(execution.get("seconds"))
        ):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} did not terminate cleanly"
            )

        log_relative = f"runs/{expected_index:03d}/qemu.log"
        log_path = artifact_dir / log_relative
        reported_log = execution.get("log")
        if (
            not isinstance(reported_log, str)
            or Path(reported_log).resolve() != log_path.resolve()
            or execution.get("log_sha256") != files[log_relative]
        ):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} is not bound to its retained log"
            )
        log_text = log_path.read_bytes().decode("utf-8", errors="replace")
        try:
            reparsed = analyze_runtime_log(
                log_text, runtime_manifest, str(fidelity["sha256"])
            )
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeProfileError(
                f"QEMU run {expected_index} cannot be checked against the fidelity manifest: {error}"
            ) from error
        result = run.get("result")
        if not isinstance(result, dict) or _canonical_bytes(
            reparsed
        ) != _canonical_bytes(result):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} report differs from its retained log"
            )
        run_signature = canonical_ready_signature(reparsed)
        if run.get("ready_signature") != run_signature or run_signature != signature:
            raise RuntimeProfileError(
                "QEMU runtime readiness signatures are not identical"
            )
        trace = run.get("trace")
        raw_relative = f"runs/{expected_index:03d}/trace.raw"
        raw_hash_relative = f"runs/{expected_index:03d}/trace.raw.sha256"
        normalized_relative = f"runs/{expected_index:03d}/trace.normalized.json"
        semantic_relative = f"runs/{expected_index:03d}/trace.semantic.json"
        raw_path = artifact_dir / raw_relative
        normalized_path = artifact_dir / normalized_relative
        semantic_path = artifact_dir / semantic_relative
        if (
            not isinstance(trace, dict)
            or Path(str(trace.get("raw"))).resolve() != raw_path.resolve()
            or Path(str(trace.get("normalized"))).resolve() != normalized_path.resolve()
            or Path(str(trace.get("semantic"))).resolve() != semantic_path.resolve()
            or trace.get("raw_sha256") != files[raw_relative]
            or raw_hash_relative not in files
            or (artifact_dir / raw_hash_relative).read_text(encoding="utf-8")
            != f"{files[raw_relative]}  trace.raw\n"
            or files[normalized_relative] != sha256_file(normalized_path)
            or files[semantic_relative] != sha256_file(semantic_path)
        ):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} is not bound to retained trace artifacts"
            )
        renormalized = normalize_trace(
            raw_path.read_bytes(),
            runtime_manifest,
            objects=trace.get("objects", {}),
            dropped=int(reparsed["trace_drops"]),
            discontinuities=int(reparsed["trace_discontinuities"]),
        )
        retained_normalized = _read_json(normalized_path, "normalized trace")
        if _canonical_bytes(renormalized) != _canonical_bytes(retained_normalized):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} normalized trace differs from raw evidence"
            )
        if (
            trace.get("normalized_sha256") != renormalized["normalized_sha256"]
            or renormalized["normalized_sha256"] != trace_signature
            or trace.get("event_count") != len(renormalized["events"])
        ):
            raise RuntimeProfileError(
                "QEMU normalized trace signatures are not identical"
            )
        retained_semantic = _read_json(semantic_path, "semantic trace")
        if _canonical_bytes(semantic_projection(renormalized)) != _canonical_bytes(
            retained_semantic
        ):
            raise RuntimeProfileError(
                f"QEMU run {expected_index} semantic projection differs from raw evidence"
            )

    return {
        "status": "PASS",
        "commit": expected_head,
        "runs": len(run_evidence),
        "ready_signature": signature,
        "trace_signature": trace_signature,
        "artifact_count": len(files),
    }


def _retain_fidelity_manifest(
    build_dir: Path, artifact_dir: Path, expected_sha256: str
) -> Path:
    source = build_dir / "domes-fidelity-manifest.json"
    destination = artifact_dir / "domes-fidelity-manifest.json"
    try:
        shutil.copy2(source, destination)
    except OSError as error:
        raise RuntimeProfileError(
            f"cannot retain fidelity manifest {source}: {error}"
        ) from error
    actual_sha256 = sha256_file(destination)
    if actual_sha256 != expected_sha256:
        raise RuntimeProfileError(
            "retained fidelity manifest differs from the validated build manifest"
        )
    return destination


def runtime_qualification(*, runs: int, dirty: bool, build_skipped: bool) -> str:
    if runs == ACCEPTANCE_RUNS and not dirty and not build_skipped:
        return "accepted"
    return "development"


def run_runtime(args: argparse.Namespace) -> int:
    artifact_dir = args.artifact_dir.resolve()
    build_dir = args.build_dir.resolve()
    if artifact_dir.exists():
        raise RuntimeProfileError(
            f"artifact directory must not already exist: {artifact_dir}"
        )
    artifact_dir.mkdir(parents=True)
    git_before = _git_identity(args.allow_dirty)
    source_before = _source_snapshot()
    toolchain = discover_toolchain(require_gdb=False)

    build_evidence: Mapping[str, Any] = {"skipped": True}
    if not args.skip_build:
        build_evidence = _build_qemu(
            toolchain,
            build_dir,
            artifact_dir / "sdkconfig.qemu",
            artifact_dir / "build.log",
        )
    validated = validate_build(build_dir, "qemu")
    manifest = validated["manifest"]
    retained_manifest = _retain_fidelity_manifest(
        build_dir, artifact_dir, str(validated["manifest_sha256"])
    )
    flash_size = _flash_size(build_dir)

    runs_dir = artifact_dir / "runs"
    runs_dir.mkdir()
    run_evidence = []
    signatures = []
    trace_signatures = []
    flash_hashes = []
    efuse_hashes = []
    for index in range(1, args.runs + 1):
        run_dir = runs_dir / f"{index:03d}"
        images = generate_run_images(
            toolchain, build_dir, run_dir, flash_size=flash_size
        )
        command = build_qemu_command(
            toolchain.qemu, Path(images["flash"]), Path(images["efuse"])
        )
        execution = execute_until_marker(
            command, run_dir / "qemu.log", args.timeout, READY_MARKER
        )
        log_text = str(execution.pop("text"))
        result = analyze_runtime_log(log_text, manifest, validated["manifest_sha256"])
        raw_trace = raw_from_qemu_log(log_text)
        raw_path = run_dir / "trace.raw"
        raw_path.write_bytes(raw_trace)
        raw_sha256 = hashlib.sha256(raw_trace).hexdigest()
        raw_path.with_suffix(".raw.sha256").write_text(
            f"{raw_sha256}  {raw_path.name}\n", encoding="utf-8"
        )
        trace_objects = object_map_from_qemu_log(log_text)
        normalized_trace = normalize_trace(
            raw_trace,
            manifest,
            objects=trace_objects,
            dropped=int(result["trace_drops"]),
            discontinuities=int(result["trace_discontinuities"]),
        )
        if len(normalized_trace["events"]) != int(result["trace_count"]):
            raise RuntimeProfileError("raw trace count differs from readiness evidence")
        if normalized_trace["overhead_us"] != {
            "disabled_32_records": int(result["trace_disabled_us"]),
            "enabled_32_records": int(result["trace_enabled_us"]),
        }:
            raise RuntimeProfileError(
                "raw trace overhead differs from readiness evidence"
            )
        normalized_path = run_dir / "trace.normalized.json"
        semantic_path = run_dir / "trace.semantic.json"
        normalized_path.write_bytes(canonical_json(normalized_trace))
        semantic_path.write_bytes(canonical_json(semantic_projection(normalized_trace)))
        unchanged = verify_run_images_unchanged(images)
        signature = canonical_ready_signature(result)
        signatures.append(signature)
        trace_signatures.append(str(normalized_trace["normalized_sha256"]))
        flash_hashes.append(str(images["flash_sha256"]))
        efuse_hashes.append(str(images["efuse_sha256"]))
        run_evidence.append(
            {
                "index": index,
                "command": command,
                "images": {
                    key: str(value) if isinstance(value, Path) else value
                    for key, value in images.items()
                },
                "image_integrity": unchanged,
                "execution": execution,
                "result": result,
                "ready_signature": signature,
                "trace": {
                    "raw": str(raw_path),
                    "raw_sha256": normalized_trace["raw_sha256"],
                    "objects": trace_objects,
                    "normalized": str(normalized_path),
                    "normalized_sha256": normalized_trace["normalized_sha256"],
                    "semantic": str(semantic_path),
                    "event_count": len(normalized_trace["events"]),
                },
            }
        )
        if index > 1:
            Path(images["flash"]).unlink()
            Path(images["efuse"]).unlink()

    if len(set(signatures)) != 1:
        raise RuntimeProfileError(
            f"readiness signature drift: {sorted(set(signatures))}"
        )
    if len(set(trace_signatures)) != 1:
        raise RuntimeProfileError(
            f"normalized trace signature drift: {sorted(set(trace_signatures))}"
        )
    if len(set(flash_hashes)) != 1 or len(set(efuse_hashes)) != 1:
        raise RuntimeProfileError(
            "fresh QEMU run-image generation was not deterministic"
        )
    source_after = _source_snapshot()
    if source_after != source_before:
        raise RuntimeProfileError(
            "profile source hashes changed during the run campaign"
        )
    git_after = _git_identity(args.allow_dirty)
    if git_after != git_before:
        raise RuntimeProfileError("repository identity changed during the run campaign")

    report = {
        "schema_version": 1,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "status": "PASS",
        "qualification": runtime_qualification(
            runs=args.runs,
            dirty=bool(git_before["dirty"]),
            build_skipped=args.skip_build,
        ),
        "runs": args.runs,
        "required_acceptance_runs": ACCEPTANCE_RUNS,
        "ready_signature": signatures[0],
        "trace_signature": trace_signatures[0],
        "git": git_before,
        "source_hashes": source_before,
        "build": build_evidence,
        "validated_build": {
            key: value for key, value in validated.items() if key != "manifest"
        },
        "fidelity_manifest": {
            "path": retained_manifest.name,
            "sha256": validated["manifest_sha256"],
        },
        "toolchain": {
            "idf_version": toolchain.idf_version,
            "idf_revision": toolchain.idf_revision,
            "compiler": str(toolchain.compiler),
            "compiler_sha256": toolchain.compiler_sha256,
            "qemu": str(toolchain.qemu),
            "qemu_version": toolchain.qemu_version,
            "qemu_sha256": toolchain.qemu_sha256,
            "libslirp": str(toolchain.libslirp),
            "libslirp_sha256": toolchain.libslirp_sha256,
        },
        "run_evidence": run_evidence,
    }
    report_path = artifact_dir / "runtime-report.json"
    report_path.write_bytes(_canonical_bytes(report))
    manifest_path = _write_artifact_manifest(artifact_dir)
    print(
        f"PASS ({report['qualification']}): {args.runs} identical runs; "
        f"report {report_path}; artifacts {manifest_path}"
    )
    return 0


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser(
        "validate-builds",
        help="validate physical/QEMU build profiles without executing QEMU",
    )
    validate.add_argument("--physical-build", type=Path, required=True)
    validate.add_argument("--qemu-build", type=Path, required=True)

    validate_single = subparsers.add_parser(
        "validate-build",
        help="validate one exact firmware build profile",
    )
    validate_single.add_argument(
        "--profile", choices=sorted(EXPECTED_PROFILE_NAMES), required=True
    )
    validate_single.add_argument("--build-dir", type=Path, required=True)

    verify_ci = subparsers.add_parser(
        "verify-ci-report", help="verify an accepted exact-checkout QEMU CI report"
    )
    verify_ci.add_argument("--report", type=Path, required=True)
    verify_ci.add_argument("--expected-head", required=True)

    run = subparsers.add_parser(
        "run", help="build and execute the QEMU runtime profile"
    )
    run.add_argument("--build-dir", type=Path, required=True)
    run.add_argument("--artifact-dir", type=Path, required=True)
    run.add_argument("--runs", type=int, default=1)
    run.add_argument("--timeout", type=float, default=15.0)
    run.add_argument("--skip-build", action="store_true")
    run.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args(argv)
    if args.command == "run":
        if args.runs < 1:
            parser.error("--runs must be at least 1")
        if args.timeout <= 0.2:
            parser.error("--timeout must exceed marker capture grace")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    try:
        args = parse_args(argv)
        if args.command == "validate-builds":
            report = validate_build_pair(args.physical_build, args.qemu_build)
            print(json.dumps(report, sort_keys=True, indent=2))
            return 0
        if args.command == "validate-build":
            validated = validate_build(args.build_dir, args.profile)
            report = {
                "status": "PASS",
                **{key: value for key, value in validated.items() if key != "manifest"},
            }
            print(json.dumps(report, sort_keys=True, indent=2))
            return 0
        if args.command == "verify-ci-report":
            report = verify_ci_report(args.report, args.expected_head)
            print(json.dumps(report, sort_keys=True, indent=2))
            return 0
        return run_runtime(args)
    except (
        OSError,
        subprocess.CalledProcessError,
        FeasibilityError,
        profile_generator.ProfileError,
        RuntimeProfileError,
    ) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
