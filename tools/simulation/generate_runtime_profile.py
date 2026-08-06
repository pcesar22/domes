#!/usr/bin/env python3
"""Validate and generate one build-selected DOMES runtime profile."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any, Mapping, Sequence


class ProfileError(RuntimeError):
    """Raised when a runtime profile is incomplete or contradictory."""


ROOT_KEYS = frozenset(
    {
        "schema_version",
        "component_catalog",
        "task_catalog",
        "fidelity_contracts",
        "profiles",
    }
)
TASK_KEYS = frozenset(
    {
        "id",
        "cpp_symbol",
        "name",
        "trace_id",
        "stack_size",
        "priority",
        "core_affinity",
        "watchdog",
    }
)
PROFILE_KEYS = frozenset(
    {
        "name",
        "root",
        "supported_feature_ids",
        "optional_feature_ids",
        "ready_enabled_feature_ids",
        "optional_ready_enabled_feature_ids",
        "deterministic_identity",
        "deterministic_random_u32",
        "sdkconfig_require_defined",
        "sdkconfig_require",
        "sdkconfig_prohibit_true",
        "init_order",
        "components",
        "tasks",
        "state_contracts",
        "task_state_contracts",
        "component_contracts",
        "readiness_scenario",
    }
)
TASK_PROJECTION_KEYS = frozenset({"state", "presence", "startup_gate"})
FIDELITY_CONTRACT_KEYS = frozenset(
    {"implementation", "inputs", "outputs", "timing", "calibration", "limitations"}
)
READINESS_SCENARIO_KEYS = frozenset(
    {
        "schema_version",
        "name",
        "dwell_ms",
        "game_timeout_ms",
        "touch_pad",
        "touch_release_ms",
        "imu_single_tap",
    }
)
COMPONENT_STATES = frozenset(
    {"production", "adapter", "modeled", "synthetic-load", "disabled"}
)
TASK_PRESENCE = frozenset({"required", "conditional", "absent"})
CPP_SYMBOL = re.compile(r"^k[A-Z][A-Za-z0-9]*$")
IDENTITY = re.compile(r"^[0-9a-f]{12}$")


def _require_mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise ProfileError(f"{label} must be an object")
    return value


def _require_list(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise ProfileError(f"{label} must be an array")
    return value


def _require_exact_keys(
    value: Mapping[str, Any], expected: frozenset[str], label: str
) -> None:
    actual = frozenset(value)
    if actual != expected:
        missing = sorted(expected - actual)
        unknown = sorted(actual - expected)
        raise ProfileError(f"{label} keys differ: missing={missing}, unknown={unknown}")


def _unique_strings(values: Any, label: str) -> list[str]:
    result = _require_list(values, label)
    if not all(isinstance(item, str) and item for item in result):
        raise ProfileError(f"{label} entries must be non-empty strings")
    if len(result) != len(set(result)):
        raise ProfileError(f"{label} contains duplicate entries")
    return result


def _feature_ids(values: Any, label: str) -> list[int]:
    result = _require_list(values, label)
    if not all(
        isinstance(item, int) and not isinstance(item, bool) and 1 <= item <= 31
        for item in result
    ):
        raise ProfileError(f"{label} entries must be unique feature IDs in [1, 31]")
    if len(result) != len(set(result)):
        raise ProfileError(f"{label} contains duplicate feature IDs")
    return result


def parse_sdkconfig(path: Path) -> dict[str, Any]:
    values: dict[str, Any] = {}
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        disabled = re.fullmatch(r"# CONFIG_([A-Z0-9_]+) is not set", line)
        if disabled:
            key = disabled.group(1)
            if key in values:
                raise ProfileError(f"{path}:{line_number}: duplicate CONFIG_{key}")
            values[key] = False
            continue
        enabled = re.fullmatch(r"CONFIG_([A-Z0-9_]+)=(.*)", line)
        if not enabled:
            continue
        key, raw_value = enabled.groups()
        if key in values:
            raise ProfileError(f"{path}:{line_number}: duplicate CONFIG_{key}")
        if raw_value == "y":
            value: Any = True
        elif raw_value == "n":
            value = False
        elif len(raw_value) >= 2 and raw_value[0] == raw_value[-1] == '"':
            value = raw_value[1:-1]
        else:
            try:
                value = int(raw_value, 0)
            except ValueError:
                value = raw_value
        values[key] = value
    return values


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )


def _reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ProfileError(f"duplicate JSON key {key!r}")
        value[key] = item
    return value


def _load_spec(path: Path) -> Mapping[str, Any]:
    try:
        raw = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_reject_duplicate_json_keys,
        )
    except (OSError, json.JSONDecodeError, ProfileError) as error:
        raise ProfileError(f"cannot read profile spec {path}: {error}") from error
    root = _require_mapping(raw, "root")
    _require_exact_keys(root, ROOT_KEYS, "root")
    if root["schema_version"] != 1:
        raise ProfileError("schema_version must be exactly 1")
    return root


def resolve_profile(
    spec_path: Path, profile_key: str, sdkconfig_path: Path
) -> dict[str, Any]:
    spec = _load_spec(spec_path)
    component_ids = _unique_strings(spec["component_catalog"], "component_catalog")

    raw_contracts = _require_mapping(spec["fidelity_contracts"], "fidelity_contracts")
    contracts: dict[str, dict[str, str]] = {}
    for contract_id, raw_contract in raw_contracts.items():
        if not isinstance(contract_id, str) or not contract_id:
            raise ProfileError("fidelity contract IDs must be non-empty strings")
        contract = dict(
            _require_mapping(raw_contract, f"fidelity_contracts.{contract_id}")
        )
        _require_exact_keys(
            contract, FIDELITY_CONTRACT_KEYS, f"fidelity_contracts.{contract_id}"
        )
        if not all(isinstance(value, str) and value for value in contract.values()):
            raise ProfileError(
                f"fidelity contract {contract_id!r} fields must be non-empty strings"
            )
        contracts[contract_id] = contract

    task_catalog = _require_list(spec["task_catalog"], "task_catalog")
    tasks: list[dict[str, Any]] = []
    task_ids: list[str] = []
    task_names: list[str] = []
    task_symbols: list[str] = []
    task_trace_ids: list[int] = []
    for index, raw_task in enumerate(task_catalog):
        task = dict(_require_mapping(raw_task, f"task_catalog[{index}]"))
        _require_exact_keys(task, TASK_KEYS, f"task_catalog[{index}]")
        if not isinstance(task["id"], str) or not task["id"]:
            raise ProfileError(f"task_catalog[{index}].id must be a non-empty string")
        if not isinstance(task["name"], str) or not 1 <= len(task["name"]) <= 15:
            raise ProfileError(
                f"task_catalog[{index}].name must contain 1..15 characters"
            )
        if not isinstance(task["cpp_symbol"], str) or not CPP_SYMBOL.fullmatch(
            task["cpp_symbol"]
        ):
            raise ProfileError(f"task_catalog[{index}].cpp_symbol is invalid")
        for field in ("trace_id", "stack_size", "priority", "core_affinity"):
            if not isinstance(task[field], int) or isinstance(task[field], bool):
                raise ProfileError(f"task_catalog[{index}].{field} must be an integer")
        if task["stack_size"] <= 0 or not 0 <= task["priority"] <= 24:
            raise ProfileError(
                f"task_catalog[{index}] has an invalid stack or priority"
            )
        if task["core_affinity"] not in {-1, 0, 1}:
            raise ProfileError(
                f"task_catalog[{index}].core_affinity must be -1, 0, or 1"
            )
        if not 1 <= task["trace_id"] <= 31:
            raise ProfileError(f"task_catalog[{index}].trace_id must be in [1, 31]")
        if not isinstance(task["watchdog"], bool):
            raise ProfileError(f"task_catalog[{index}].watchdog must be boolean")
        task["evidence_mask"] = 1 << index
        tasks.append(task)
        task_ids.append(task["id"])
        task_names.append(task["name"])
        task_symbols.append(task["cpp_symbol"])
        task_trace_ids.append(task["trace_id"])
    for values, label in (
        (task_ids, "task IDs"),
        (task_names, "task names"),
        (task_symbols, "task symbols"),
        (task_trace_ids, "task trace IDs"),
    ):
        if len(values) != len(set(values)):
            raise ProfileError(f"task_catalog contains duplicate {label}")

    profiles = _require_mapping(spec["profiles"], "profiles")
    if frozenset(profiles) != frozenset({"physical", "qemu"}):
        raise ProfileError("profiles must contain exactly physical and qemu")
    if profile_key not in profiles:
        raise ProfileError(f"unknown profile {profile_key!r}")
    profile = _require_mapping(profiles[profile_key], f"profiles.{profile_key}")
    _require_exact_keys(profile, PROFILE_KEYS, f"profiles.{profile_key}")

    name = profile["name"]
    root = profile["root"]
    if (
        not isinstance(name, str)
        or not name
        or not isinstance(root, str)
        or not root.endswith(".cpp")
    ):
        raise ProfileError(f"profiles.{profile_key} requires a name and C++ root")
    expected_root = (
        "main.cpp" if profile_key == "physical" else "composition/qemuRoot.cpp"
    )
    if root != expected_root:
        raise ProfileError(f"profiles.{profile_key}.root must be {expected_root!r}")

    components = _require_mapping(
        profile["components"], f"profiles.{profile_key}.components"
    )
    if set(components) != set(component_ids):
        raise ProfileError(
            f"profiles.{profile_key}.components must classify every catalog component exactly once"
        )
    for component_id, state in components.items():
        if state not in COMPONENT_STATES:
            raise ProfileError(
                f"component {component_id!r} has invalid state {state!r}"
            )

    state_contracts = _require_mapping(
        profile["state_contracts"], f"profiles.{profile_key}.state_contracts"
    )
    if set(state_contracts) != set(components.values()):
        raise ProfileError(
            f"profiles.{profile_key}.state_contracts must map every used component state"
        )
    component_contracts = _require_mapping(
        profile["component_contracts"], f"profiles.{profile_key}.component_contracts"
    )
    if not set(component_contracts).issubset(component_ids):
        raise ProfileError(
            f"profiles.{profile_key}.component_contracts contains an unknown component"
        )
    projected_components: list[dict[str, Any]] = []
    for component_id in component_ids:
        contract_id = component_contracts.get(
            component_id, state_contracts[components[component_id]]
        )
        if not isinstance(contract_id, str) or contract_id not in contracts:
            raise ProfileError(
                f"component {component_id!r} references unknown fidelity contract {contract_id!r}"
            )
        projected_components.append(
            {
                "id": component_id,
                "state": components[component_id],
                "contract": contract_id,
                **contracts[contract_id],
            }
        )

    task_projection = _require_mapping(
        profile["tasks"], f"profiles.{profile_key}.tasks"
    )
    if set(task_projection) != set(task_ids):
        raise ProfileError(
            f"profiles.{profile_key}.tasks must project every task exactly once"
        )
    task_state_contracts = _require_mapping(
        profile["task_state_contracts"],
        f"profiles.{profile_key}.task_state_contracts",
    )
    projected_tasks: list[dict[str, Any]] = []
    for task in tasks:
        projection = _require_mapping(
            task_projection[task["id"]], f"tasks.{task['id']}"
        )
        _require_exact_keys(projection, TASK_PROJECTION_KEYS, f"tasks.{task['id']}")
        state = projection["state"]
        presence = projection["presence"]
        startup_gate = projection["startup_gate"]
        if state not in COMPONENT_STATES or presence not in TASK_PRESENCE:
            raise ProfileError(f"task {task['id']!r} has invalid state or presence")
        if not isinstance(startup_gate, bool):
            raise ProfileError(f"task {task['id']!r} startup_gate must be boolean")
        if (state == "disabled") != (presence == "absent"):
            raise ProfileError(
                f"task {task['id']!r} must be disabled exactly when absent"
            )
        if startup_gate and presence != "required":
            raise ProfileError(
                f"task {task['id']!r} startup_gate requires required presence"
            )
        contract_id = task_state_contracts.get(state)
        if not isinstance(contract_id, str) or contract_id not in contracts:
            raise ProfileError(
                f"task state {state!r} references unknown fidelity contract {contract_id!r}"
            )
        projected_tasks.append(
            {**task, **projection, "contract": contract_id, **contracts[contract_id]}
        )
    if set(task_state_contracts) != {task["state"] for task in projected_tasks}:
        raise ProfileError(
            f"profiles.{profile_key}.task_state_contracts must map every used task state"
        )

    supported = _feature_ids(profile["supported_feature_ids"], "supported_feature_ids")
    ready = _feature_ids(
        profile["ready_enabled_feature_ids"], "ready_enabled_feature_ids"
    )
    optional_raw = _require_mapping(
        profile["optional_feature_ids"], "optional_feature_ids"
    )
    optional: dict[int, str] = {}
    for feature_id, config_key in optional_raw.items():
        try:
            parsed_id = int(feature_id)
        except (TypeError, ValueError) as error:
            raise ProfileError(
                f"optional feature ID {feature_id!r} is invalid"
            ) from error
        if (
            not 1 <= parsed_id <= 31
            or not isinstance(config_key, str)
            or not config_key
        ):
            raise ProfileError(f"optional feature mapping {feature_id!r} is invalid")
        optional[parsed_id] = config_key
    if set(supported) & set(optional):
        raise ProfileError("required and optional feature IDs overlap")

    optional_ready_raw = _require_mapping(
        profile["optional_ready_enabled_feature_ids"],
        "optional_ready_enabled_feature_ids",
    )
    optional_ready: dict[int, str] = {}
    for feature_id, config_key in optional_ready_raw.items():
        try:
            parsed_id = int(feature_id)
        except (TypeError, ValueError) as error:
            raise ProfileError(
                f"optional ready feature ID {feature_id!r} is invalid"
            ) from error
        if optional.get(parsed_id) != config_key:
            raise ProfileError(
                f"optional ready feature {feature_id!r} must match optional_feature_ids"
            )
        optional_ready[parsed_id] = config_key
    if set(ready) & set(optional_ready):
        raise ProfileError("required and optional ready feature IDs overlap")

    sdkconfig = parse_sdkconfig(sdkconfig_path)
    required_defined = _unique_strings(
        profile["sdkconfig_require_defined"], "sdkconfig_require_defined"
    )
    for key in required_defined:
        if key not in sdkconfig:
            raise ProfileError(
                f"CONFIG_{key} must be defined by the resolved SDKCONFIG"
            )
    requirements = _require_mapping(profile["sdkconfig_require"], "sdkconfig_require")
    for key, expected in requirements.items():
        if key not in sdkconfig:
            raise ProfileError(
                f"CONFIG_{key} must be defined by the resolved SDKCONFIG"
            )
        actual = sdkconfig[key]
        if actual != expected:
            raise ProfileError(f"CONFIG_{key} must be {expected!r}, found {actual!r}")
    prohibited = _unique_strings(
        profile["sdkconfig_prohibit_true"], "sdkconfig_prohibit_true"
    )
    for key in prohibited:
        if sdkconfig.get(key, False) is True:
            raise ProfileError(
                f"CONFIG_{key}=y is prohibited for profile {profile_key}"
            )
    for feature_id, config_key in optional.items():
        if sdkconfig.get(config_key, False) is True:
            supported.append(feature_id)
    for feature_id, config_key in optional_ready.items():
        if sdkconfig.get(config_key, False) is True:
            ready.append(feature_id)
    if not set(ready).issubset(supported):
        raise ProfileError("ready feature IDs must be supported by the resolved build")

    identity = profile["deterministic_identity"]
    random_values = _require_list(
        profile["deterministic_random_u32"], "deterministic_random_u32"
    )
    if profile_key == "qemu":
        if not isinstance(identity, str) or not IDENTITY.fullmatch(identity):
            raise ProfileError(
                "qemu deterministic_identity must be 12 lowercase hex characters"
            )
        if not random_values:
            raise ProfileError("qemu deterministic_random_u32 must not be empty")
    elif identity is not None or random_values:
        raise ProfileError(
            "physical profile cannot declare deterministic platform inputs"
        )
    if not all(
        isinstance(value, int)
        and not isinstance(value, bool)
        and 0 <= value <= 0xFFFFFFFF
        for value in random_values
    ):
        raise ProfileError("deterministic_random_u32 values must fit uint32")

    init_order = _unique_strings(profile["init_order"], "init_order")
    raw_scenario = profile["readiness_scenario"]
    scenario: dict[str, Any] | None = None
    if profile_key == "qemu":
        scenario = dict(_require_mapping(raw_scenario, "readiness_scenario"))
        _require_exact_keys(scenario, READINESS_SCENARIO_KEYS, "readiness_scenario")
        if scenario["schema_version"] != 1 or scenario["name"] != "service_ready_v1":
            raise ProfileError(
                "qemu readiness_scenario must be service_ready_v1 schema 1"
            )
        for field in ("dwell_ms", "game_timeout_ms", "touch_pad", "touch_release_ms"):
            if not isinstance(scenario[field], int) or isinstance(
                scenario[field], bool
            ):
                raise ProfileError(f"readiness_scenario.{field} must be an integer")
        if (
            scenario["dwell_ms"] < 100
            or scenario["game_timeout_ms"] < scenario["dwell_ms"]
        ):
            raise ProfileError(
                "readiness_scenario dwell and game timeout are inconsistent"
            )
        if not 0 <= scenario["touch_pad"] <= 3:
            raise ProfileError("readiness_scenario.touch_pad must be in [0, 3]")
        if not 0 <= scenario["touch_release_ms"] < scenario["dwell_ms"]:
            raise ProfileError(
                "readiness_scenario.touch_release_ms must be inside the dwell"
            )
        if scenario["imu_single_tap"] is not True:
            raise ProfileError("readiness_scenario.imu_single_tap must be true")
    elif raw_scenario is not None:
        raise ProfileError("physical profile cannot declare a readiness scenario")
    supported_mask = sum(1 << feature_id for feature_id in supported)
    ready_mask = sum(1 << feature_id for feature_id in ready)
    required_tasks = [
        task for task in projected_tasks if task["presence"] == "required"
    ]
    absent_tasks = [task for task in projected_tasks if task["presence"] == "absent"]
    required_task_masks = {
        affinity: sum(
            task["evidence_mask"]
            for task in required_tasks
            if task["core_affinity"] == affinity
        )
        for affinity in (-1, 0, 1)
    }
    task_config_payload = [
        {
            "id": task["id"],
            "name": task["name"],
            "trace_id": task["trace_id"],
            "stack_size": task["stack_size"],
            "priority": task["priority"],
            "core_affinity": task["core_affinity"],
            "watchdog": task["watchdog"],
            "evidence_mask": task["evidence_mask"],
        }
        for task in required_tasks
    ]

    manifest = {
        "schema_version": 1,
        "profile": name,
        "profile_key": profile_key,
        "root": root,
        "spec_sha256": _sha256(spec_path),
        "sdkconfig_sha256": _sha256(sdkconfig_path),
        "supported_feature_mask": supported_mask,
        "ready_enabled_feature_mask": ready_mask,
        "inputs": {
            "identity": identity,
            "random_u32_count": len(random_values),
            "random_u32_sha256": hashlib.sha256(
                _canonical_bytes(random_values)
            ).hexdigest(),
        },
        "init_order": init_order,
        "components": projected_components,
        "tasks": projected_tasks,
        "readiness_scenario": scenario,
    }
    manifest_bytes = _canonical_bytes(manifest)
    return {
        "manifest": manifest,
        "manifest_bytes": manifest_bytes,
        "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "task_config_sha256": hashlib.sha256(
            _canonical_bytes(task_config_payload)
        ).hexdigest(),
        "tasks": tasks,
        "required_tasks": required_tasks,
        "absent_tasks": absent_tasks,
        "required_task_masks": required_task_masks,
        "profile_name": name,
        "profile_key": profile_key,
        "root": root,
        "supported_mask": supported_mask,
        "ready_mask": ready_mask,
        "identity": identity,
        "random_values": random_values,
        "init_order": init_order,
        "readiness_scenario": scenario,
    }


def _cpp_bool(value: bool) -> str:
    return "true" if value else "false"


def _render_header(resolved: Mapping[str, Any]) -> str:
    manifest = resolved["manifest"]
    lines = [
        "#pragma once",
        "",
        "// Generated by tools/simulation/generate_runtime_profile.py. Do not edit.",
        '#include "infra/taskConfig.hpp"',
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace domes::infra::task {",
        "",
    ]
    for task in resolved["tasks"]:
        core = (
            "core::kAny" if task["core_affinity"] == -1 else str(task["core_affinity"])
        )
        lines.extend(
            [
                f"inline constexpr TaskConfig {task['cpp_symbol']} = {{",
                f'    .name = "{task["name"]}",',
                f"    .traceId = {task['trace_id']},",
                f"    .stackSize = {task['stack_size']},",
                f"    .priority = {task['priority']},",
                f"    .coreAffinity = {core},",
                f"    .subscribeToWatchdog = {_cpp_bool(task['watchdog'])},",
                f"    .evidenceMask = 0x{task['evidence_mask']:08x}U,",
                "};",
                "",
            ]
        )
    lines.extend(
        [
            "}  // namespace domes::infra::task",
            "",
            "namespace domes::runtime_profile {",
            "",
            "struct ExpectedTask {",
            "    const char* id;",
            "    const infra::TaskConfig* config;",
            "};",
            "",
            f'inline constexpr char kProfileKey[] = "{resolved["profile_key"]}";',
            f'inline constexpr char kProfileName[] = "{resolved["profile_name"]}";',
            f'inline constexpr char kRootSource[] = "{resolved["root"]}";',
            f'inline constexpr char kManifestSha256[] = "{resolved["manifest_sha256"]}";',
            f'inline constexpr char kSpecSha256[] = "{manifest["spec_sha256"]}";',
            f'inline constexpr char kSdkconfigSha256[] = "{manifest["sdkconfig_sha256"]}";',
            f'inline constexpr char kTaskConfigSha256[] = "{resolved["task_config_sha256"]}";',
            f"inline constexpr uint32_t kSupportedFeatureMask = 0x{resolved['supported_mask']:08x}U;",
            f"inline constexpr uint32_t kReadyEnabledFeatureMask = 0x{resolved['ready_mask']:08x}U;",
            "inline constexpr uint32_t kRequiredTaskEvidenceMask = "
            f"0x{sum(task['evidence_mask'] for task in resolved['required_tasks']):08x}U;",
            "inline constexpr uint32_t kUnpinnedRequiredTaskEvidenceMask = "
            f"0x{resolved['required_task_masks'][-1]:08x}U;",
            "inline constexpr uint32_t kCore0RequiredTaskEvidenceMask = "
            f"0x{resolved['required_task_masks'][0]:08x}U;",
            "inline constexpr uint32_t kCore1RequiredTaskEvidenceMask = "
            f"0x{resolved['required_task_masks'][1]:08x}U;",
            "",
        ]
    )
    identity = resolved["identity"]
    identity_values = (
        [int(identity[index : index + 2], 16) for index in range(0, 12, 2)]
        if identity
        else [0] * 6
    )
    lines.append(
        "inline constexpr std::array<uint8_t, 6> kDeterministicIdentity = {"
        + ", ".join(f"0x{value:02x}" for value in identity_values)
        + "};"
    )
    random_values = resolved["random_values"]
    lines.append(
        f"inline constexpr std::array<uint32_t, {len(random_values)}> kDeterministicRandom = {{"
        + ", ".join(f"0x{value:08x}U" for value in random_values)
        + "};"
    )
    lines.extend(
        [
            "",
            f"inline constexpr std::array<ExpectedTask, {len(resolved['tasks'])}> kTraceTasks = {{{{",
        ]
    )
    for task in resolved["tasks"]:
        lines.append(f'    {{"{task["id"]}", &infra::task::{task["cpp_symbol"]}}},')
    lines.extend(
        [
            "}};",
            "",
            f"inline constexpr std::array<ExpectedTask, {len(resolved['required_tasks'])}> kRequiredTasks = {{{{",
        ]
    )
    for task in resolved["required_tasks"]:
        lines.append(f'    {{"{task["id"]}", &infra::task::{task["cpp_symbol"]}}},')
    lines.extend(["}};", ""])
    lines.append(
        f"inline constexpr std::array<const char*, {len(resolved['absent_tasks'])}> kAbsentTaskNames = {{{{"
    )
    for task in resolved["absent_tasks"]:
        lines.append(f'    "{task["name"]}",')
    lines.extend(["}};", ""])
    lines.append(
        f"inline constexpr std::array<const char*, {len(resolved['init_order'])}> kInitOrder = {{{{"
    )
    for stage in resolved["init_order"]:
        lines.append(f'    "{stage}",')
    lines.extend(["}};", ""])
    scenario = resolved["readiness_scenario"]
    if scenario is not None:
        lines.extend(
            [
                f'inline constexpr char kReadinessScenario[] = "{scenario["name"]}";',
                f"inline constexpr uint32_t kReadinessDwellMs = {scenario['dwell_ms']}U;",
                f"inline constexpr uint32_t kReadinessGameTimeoutMs = {scenario['game_timeout_ms']}U;",
                f"inline constexpr uint8_t kReadinessTouchPad = {scenario['touch_pad']}U;",
                f"inline constexpr uint32_t kReadinessTouchReleaseMs = {scenario['touch_release_ms']}U;",
                "",
            ]
        )
    lines.extend(["}  // namespace domes::runtime_profile", ""])
    return "\n".join(lines)


def generate(
    spec: Path, profile: str, sdkconfig: Path, header: Path, manifest: Path
) -> None:
    resolved = resolve_profile(spec, profile, sdkconfig)
    header.parent.mkdir(parents=True, exist_ok=True)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(_render_header(resolved), encoding="utf-8")
    manifest.write_bytes(resolved["manifest_bytes"])


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--profile", choices=("physical", "qemu"), required=True)
    parser.add_argument("--sdkconfig", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        generate(args.spec, args.profile, args.sdkconfig, args.header, args.manifest)
    except (OSError, ProfileError) as error:
        raise SystemExit(f"runtime profile generation failed: {error}") from error
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
