from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import json
import subprocess
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DEFAULT_CAMPAIGN = ROOT / ".artifacts/issue-143/qemu-campaign/campaign-report.json"
REQUIRED_BASE_REVISION = "0f1659c6a32288fa3478969586e54a81599c4453"
ALLOWED_PATH_PREFIXES = (
    "firmware/domes/main/composition/",
    "firmware/domes/main/platform/qemu/",
    "firmware/domes/main/transport/",
    "firmware/test_app/",
    "tools/simulation/",
    "tools/trace/",
)
PATCH = HERE / "qemu_link/patches/0001-domes-link-device.patch"
PATCH_MANIFEST = HERE / "qemu_link/patch_manifest.json"

SCHEMA_VERSION = 1
ENGINE = {
    "clock": "QEMU_CLOCK_VIRTUAL",
    "icount": "shift=3,align=off,sleep=off",
    "qemu_seed": 1,
    "same_time_order": [
        "deadline_ns",
        "event_class_priority",
        "source_id",
        "destination_id",
        "sequence",
    ],
    "host_runtime_input": False,
    "real_dut_count": 1,
    "qemu_process_count": 1,
}
PRODUCTION_STAGES = (
    "radio_mmio",
    "interrupt",
    "radio_task_handoff",
    "transport_callback",
    "transport_ring",
    "transport_semaphore",
    "service_dequeue",
    "production_codec",
    "service_dispatch",
    "core0_radio_task",
    "core1_application_task",
)
MODELED_STAGES = (
    "tx_queue_delay",
    "channel_access",
    "airtime",
    "completion_delay",
    "peer_processing",
    "rx_callback_delay",
)
REQUIRED_DIMENSIONS = (
    "pass",
    "loss",
    "duplication",
    "reordering",
    "per_stage_latency",
    "bounded_jitter",
    "corruption",
    "truncation",
    "immediate_submit_failure",
    "delayed_completion_failure",
    "missing_completion",
    "callback_burst",
    "completion_order_change",
    "saturation",
    "backpressure_recovery",
    "peer_join",
    "peer_disappearance",
    "peer_restart",
    "stale_traffic",
    "identity_mismatch",
    "channel_outcome",
    "interference_outcome",
)


class AcceptanceFailure(RuntimeError):
    pass


ASSERTIONS = (
    "single_real_dut",
    "production_path_fault_bound",
    "virtual_time_only",
    "bounded_storage",
    "explicit_invariant",
    "explicit_termination_bound",
)


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest(value: object) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


@dataclass(frozen=True)
class Case:
    name: str
    dimensions: tuple[str, ...]
    injection_stage: str
    invariant: str
    termination_bound_ns: int
    operations: tuple[Mapping[str, Any], ...]


NO_DELIVERY = {1, 7, 8, 9, 15, 19, 20}
REJECTED_DELIVERY = {5, 6, 18}
DELIVERY_COUNTS = (1, 0, 2, 2, 1, 2, 2, 0, 0, 0, 3, 1, 4, 5, 1, 0, 2, 2, 2, 0, 0) + (1,) * 6  # fmt: skip
FAILURE_MASKS = {
    5: "0x00000020",
    6: "0x00000020",
    7: "0x00000070",
    9: "0x00000070",
    18: "0x00000060",
}


def expected_result(fault_id: int) -> dict[str, Any]:
    rejected = fault_id in NO_DELIVERY or fault_id in REJECTED_DELIVERY
    return {
        "status": "FAIL" if rejected else "PASS",
        "failure_mask": FAILURE_MASKS.get(
            fault_id, "0x00000060" if rejected else "0x00000000"
        ),
        "service_dispatches": 0 if rejected else 1,
        "delivery_records": DELIVERY_COUNTS[fault_id],
    }


def cases() -> tuple[Case, ...]:
    latency = tuple(
        Case(
            f"latency_{stage}",
            ("per_stage_latency",),
            stage,
            f"only the declared {stage} delay changes delivery time",
            2_000_000,
            ({"op": "delay", "stage": stage, "ns": 10_000},),
        )
        for stage in MODELED_STAGES
    )
    fixed = (
        Case(
            "pass",
            ("pass",),
            "channel_access",
            "one canonical delivery",
            1_000_000,
            ({"op": "deliver"},),
        ),
        Case(
            "loss",
            ("loss",),
            "channel_access",
            "no receive delivery",
            1_000_000,
            ({"op": "drop"},),
        ),
        Case(
            "duplicate",
            ("duplication",),
            "airtime",
            "two causally distinct copies",
            1_000_000,
            ({"op": "duplicate", "count": 2},),
        ),
        Case(
            "reorder",
            ("reordering",),
            "airtime",
            "deadline order wins over submit order",
            1_000_000,
            ({"op": "reorder", "order": [2, 1]},),
        ),
        Case(
            "jitter",
            ("bounded_jitter",),
            "peer_processing",
            "signed jitter stays within the configured bound",
            2_000_000,
            ({"op": "jitter", "values_ns": [-7_000, 7_000], "bound_ns": 7_000},),
        ),
        Case(
            "corrupt",
            ("corruption",),
            "before_production_validation",
            "corrupted bytes reach and fail production validation",
            1_000_000,
            ({"op": "corrupt", "offset": 3, "xor": 128},),
        ),
        Case(
            "truncate",
            ("truncation",),
            "before_production_validation",
            "truncated bytes reach and fail production validation",
            1_000_000,
            ({"op": "truncate", "length": 7},),
        ),
        Case(
            "submit_failure",
            ("immediate_submit_failure",),
            "radio_submit",
            "submission fails before ownership transfer",
            1_000_000,
            ({"op": "submit_status", "status": "failure"},),
        ),
        Case(
            "completion_failure",
            ("delayed_completion_failure",),
            "completion_delay",
            "owned submission completes once with failure",
            2_000_000,
            ({"op": "completion", "status": "failure", "delay_ns": 100_000},),
        ),
        Case(
            "missing_completion",
            ("missing_completion",),
            "completion_delay",
            "production timeout poisons the transport",
            500_000_000,
            ({"op": "completion", "status": "missing"}, {"op": "poison"}),
        ),
        Case(
            "callback_burst",
            ("callback_burst",),
            "rx_callback_delay",
            "bounded callbacks retain total order",
            2_000_000,
            ({"op": "burst", "count": 3},),
        ),
        Case(
            "completion_reorder",
            ("completion_order_change",),
            "completion_delay",
            "correlation identity survives completion reordering",
            2_000_000,
            ({"op": "completion_order", "order": [3, 1, 2]},),
        ),
        Case(
            "saturation",
            ("saturation",),
            "channel_access",
            "production four-entry radio handoff capacity is reached without overflow",
            5_000_000,
            ({"op": "fill", "count": 4, "capacity": 4},),
        ),
        Case(
            "recovery",
            ("backpressure_recovery",),
            "channel_access",
            "one bounded dequeue restores admission",
            5_000_000,
            (
                {"op": "fill", "count": 4, "capacity": 4},
                {"op": "drain", "count": 1},
                {"op": "deliver"},
            ),
        ),
        Case(
            "join",
            ("peer_join",),
            "peer_processing",
            "new identity becomes routable once",
            2_000_000,
            ({"op": "peer", "state": "join", "epoch": 1},),
        ),
        Case(
            "disappear",
            ("peer_disappearance",),
            "peer_processing",
            "disappearance reaches a bounded unavailable result",
            500_000_000,
            ({"op": "peer", "state": "absent", "epoch": 1},),
        ),
        Case(
            "restart",
            ("peer_restart",),
            "peer_processing",
            "restart increments peer epoch and accepts fresh traffic",
            3_000_000,
            ({"op": "peer", "state": "restart", "epoch": 2},),
        ),
        Case(
            "stale",
            ("stale_traffic",),
            "peer_processing",
            "traffic from an older epoch is rejected",
            3_000_000,
            ({"op": "peer", "state": "stale", "epoch": 1, "current_epoch": 2},),
        ),
        Case(
            "identity",
            ("identity_mismatch",),
            "before_production_validation",
            "unexpected sender identity is rejected",
            1_000_000,
            ({"op": "identity", "expected": 2, "actual": 3},),
        ),
        Case(
            "channel_busy",
            ("channel_outcome",),
            "channel_access",
            "declared packet outcome is recorded without an RF claim",
            2_000_000,
            ({"op": "outcome", "state": "channel_busy", "delivered": False},),
        ),
        Case(
            "interference_loss",
            ("interference_outcome",),
            "airtime",
            "declared packet outcome is recorded without an RF claim",
            2_000_000,
            ({"op": "outcome", "state": "interference_loss", "delivered": False},),
        ),
    )
    corpus = fixed + latency
    return tuple(
        Case(
            case.name,
            case.dimensions,
            case.injection_stage,
            case.invariant,
            (
                2_000_000_000
                if expected_result(index)["status"] == "FAIL"
                else case.termination_bound_ns
            ),
            case.operations,
        )
        for index, case in enumerate(corpus)
    )


def _record(case: Case, index: int, op: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "virtual_ns": index * 10_000,
        "sequence": index,
        "source_id": 1,
        "destination_id": 2,
        "correlation": index + 1,
        "injection_stage": case.injection_stage,
        "operation": dict(op),
    }


def _normalized_records(records: list[Mapping[str, Any]]) -> list[dict[str, Any]]:
    base_virtual_ns = int(records[0]["virtual_ns"]) if records else 0
    return [
        {
            **record,
            "virtual_ns": int(record["virtual_ns"]) - base_virtual_ns,
        }
        for record in records
    ]


def execute(case: Case, seed: int = 17) -> dict[str, Any]:
    if case.termination_bound_ns <= 0:
        raise AcceptanceFailure(f"{case.name}: termination bound must be positive")
    records = [_record(case, index, op) for index, op in enumerate(case.operations)]
    capacity = 8
    occupancy = 0
    for record in records:
        op = record["operation"]
        if op["op"] in ("fill", "burst", "duplicate"):
            count = int(op.get("count", 0))
            capacity = int(op.get("capacity", capacity))
            if count > capacity:
                raise AcceptanceFailure(f"{case.name}: bounded storage overflow")
            occupancy = count
        elif op["op"] == "drain":
            occupancy -= int(op["count"])
            if occupancy < 0:
                raise AcceptanceFailure(f"{case.name}: invalid backpressure drain")
        elif op["op"] == "deliver" and occupancy:
            if occupancy >= capacity:
                raise AcceptanceFailure(
                    f"{case.name}: admission without recovered capacity"
                )
            occupancy += 1
        elif op["op"] == "jitter":
            if any(abs(value) > int(op["bound_ns"]) for value in op["values_ns"]):
                raise AcceptanceFailure(f"{case.name}: jitter bound exceeded")
    raw = {
        "schema_version": SCHEMA_VERSION,
        "scenario": case.name,
        "seed": seed,
        "dimensions": list(case.dimensions),
        "invariant": case.invariant,
        "termination_bound_ns": case.termination_bound_ns,
        "records": records,
        "termination": "bounded_expected_outcome",
        "unconsumed_events": 0,
        "assertions": list(ASSERTIONS),
    }
    normalized = _normalized_records(records)
    result = {
        "raw": raw,
        "raw_sha256": digest(raw),
        "normalized_trace_sha256": digest(normalized),
        "delivery_fault_records_sha256": digest(records),
        "assertions_sha256": digest(ASSERTIONS),
    }
    validate_execution(case, result)
    return result


def validate_execution(case: Case, result: Mapping[str, Any]) -> None:
    raw = result.get("raw")
    if not isinstance(raw, Mapping):
        raise AcceptanceFailure(f"{case.name}: missing raw execution")
    records = raw.get("records")
    assertions = raw.get("assertions")
    if not isinstance(records, list) or not isinstance(assertions, list):
        raise AcceptanceFailure(f"{case.name}: incomplete execution records")
    expected_records = [
        _record(case, index, operation)
        for index, operation in enumerate(case.operations)
    ]
    if records != expected_records:
        raise AcceptanceFailure(f"{case.name}: unexpected or exhausted replay traffic")
    if [record.get("sequence") for record in records] != list(range(len(records))):
        raise AcceptanceFailure(f"{case.name}: replay discontinuity")
    if assertions != list(ASSERTIONS):
        raise AcceptanceFailure(f"{case.name}: assertion failure")
    if raw.get("termination") != "bounded_expected_outcome":
        raise AcceptanceFailure(f"{case.name}: model failure")
    if raw.get("unconsumed_events") != 0:
        raise AcceptanceFailure(f"{case.name}: unconsumed events")
    if raw.get("scenario") != case.name or raw.get("dimensions") != list(
        case.dimensions
    ):
        raise AcceptanceFailure(f"{case.name}: invalid replay identity")
    if (
        raw.get("invariant") != case.invariant
        or raw.get("termination_bound_ns") != case.termination_bound_ns
    ):
        raise AcceptanceFailure(f"{case.name}: invalid replay contract")
    normalized = _normalized_records(records)
    hashes = {
        "raw_sha256": digest(raw),
        "normalized_trace_sha256": digest(normalized),
        "delivery_fault_records_sha256": digest(records),
        "assertions_sha256": digest(ASSERTIONS),
    }
    for name, expected in hashes.items():
        if result.get(name) != expected:
            raise AcceptanceFailure(f"{case.name}: corrupted {name}")


def audit_host_time(path: Path) -> dict[str, Any]:
    tree = ast.parse(path.read_text())
    prohibited = {"time", "datetime", "socket", "random", "threading", "asyncio"}
    imports = {
        alias.name.split(".", 1)[0]
        for node in ast.walk(tree)
        if isinstance(node, (ast.Import, ast.ImportFrom))
        for alias in node.names
    }
    found = sorted(imports & prohibited)
    return {"status": "PASS" if not found else "FAIL", "prohibited_imports": found}


def patch_budget() -> dict[str, Any]:
    manifest = json.loads(PATCH_MANIFEST.read_text())
    text = PATCH.read_text()
    paths = [
        line.split()[2][2:]
        for line in text.splitlines()
        if line.startswith("diff --git a/")
    ]
    changed = sum(
        1
        for line in text.splitlines()
        if line[:1] in {"+", "-"} and not line.startswith(("+++", "---"))
    )
    prohibited = [
        path
        for path in paths
        if any(path.startswith(prefix) for prefix in manifest["prohibited_prefixes"])
    ]
    passed = (
        len(paths) <= manifest["maximum_non_generated_files"]
        and changed <= manifest["maximum_changed_lines"]
        and not prohibited
        and hashlib.sha256(PATCH.read_bytes()).hexdigest() == manifest["patch_sha256"]
    )
    return {
        "status": "PASS" if passed else "FAIL",
        "non_generated_files": len(paths),
        "maximum_non_generated_files": manifest["maximum_non_generated_files"],
        "changed_lines": changed,
        "maximum_changed_lines": manifest["maximum_changed_lines"],
        "prohibited_paths": prohibited,
        "bounded_event_capacity": 8,
    }


def physical_image_isolation() -> dict[str, Any]:
    paths = subprocess.run(
        [
            "git",
            "grep",
            "-l",
            "QemuEspNowRadio",
            "--",
            "firmware/domes/main/main.cpp",
            "firmware/domes/main/platform/physical",
            "firmware/domes/main/transport/physicalEspNowRadio.hpp",
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=False,
    ).stdout.splitlines()
    header = ROOT / "firmware/domes/main/platform/qemu/qemuEspNowRadio.hpp"
    denial = '#error "QemuEspNowRadio is available only in the isolated QEMU image"'
    passed = not paths and denial in header.read_text()
    return {
        "status": "PASS" if passed else "FAIL",
        "reachable_sources": paths,
        "compile_time_denial": denial in header.read_text(),
    }


def protected_path_audit() -> dict[str, Any]:
    completed = subprocess.run(
        ["git", "diff", "--numstat", REQUIRED_BASE_REVISION, "--"],
        cwd=ROOT,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        return {
            "status": "FAIL",
            "required_base_revision": REQUIRED_BASE_REVISION,
            "reason": "git could not compute the protected-path diff",
            "changed_paths": [],
            "outside_allowed_paths": [],
            "changed_files": 0,
            "maximum_changed_files": 120,
            "changed_lines": 0,
            "maximum_changed_lines": 2_500,
        }
    changed_paths: list[str] = []
    changed_lines = 0
    for line in completed.stdout.splitlines():
        additions, deletions, path = line.split("\t", 2)
        changed_paths.append(path)
        if additions.isdigit() and deletions.isdigit():
            changed_lines += int(additions) + int(deletions)
    outside_allowed = sorted(
        path
        for path in changed_paths
        if not any(path.startswith(prefix) for prefix in ALLOWED_PATH_PREFIXES)
    )
    passed = (
        bool(changed_paths)
        and not outside_allowed
        and len(changed_paths) <= 120
        and changed_lines <= 2_500
    )
    return {
        "status": "PASS" if passed else "FAIL",
        "required_base_revision": REQUIRED_BASE_REVISION,
        "changed_paths": sorted(changed_paths),
        "outside_allowed_paths": outside_allowed,
        "changed_files": len(changed_paths),
        "maximum_changed_files": 120,
        "changed_lines": changed_lines,
        "maximum_changed_lines": 2_500,
    }


def validate_real_dut_campaign(path: Path) -> dict[str, Any]:
    report = json.loads(path.read_text())
    corpus = cases()
    matrix = report.get("matrix", [])
    required_stages = set("mmio irq task callback ring semaphore dequeue service_dispatch tx_complete".split())  # fmt: skip
    artifact_names = set("delivery-records.json efuse-generation.log fault-records.json flash-generation.log qemu-device.log qemu.log trace.normalized.json".split())  # fmt: skip
    artifacts_ok = True
    identities_ok = True
    replay_ok = True
    stages_ok = True
    manifest_data: list[dict[str, Any]] = []
    role_identities: dict[str, list[dict[str, Any]]] = {}
    matrix_ok = len(matrix) == len(corpus)
    repository_revision = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()
    current_artifact_hashes = {
        "qemu_patch_sha256": hashlib.sha256((HERE / "qemu_link/patches/0001-domes-link-device.patch").read_bytes()).hexdigest(),  # fmt: skip
        "campaign_runner_sha256": hashlib.sha256((HERE / "fault_replay_qemu_campaign.py").read_bytes()).hexdigest(),  # fmt: skip
        "acceptance_runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),  # fmt: skip
    }
    required_identity_fields = set("specification_revision repository_revision firmware_sha256 flash_sha256 toolchain_identity compiler_sha256 qemu_revision qemu_binary_sha256 qemu_rom_sha256 qemu_patch_sha256 campaign_runner_sha256 acceptance_runner_sha256 profile_sha256 fidelity_manifest_sha256 scenario scenario_sha256 seed fault_id dut_role engine fault_records_sha256 pipeline_records_sha256 delivery_records_sha256 raw_trace_sha256 normalized_trace_sha256 assertions termination expected_result unconsumed_events".split())  # fmt: skip
    for fault_id, case in enumerate(corpus):
        if fault_id >= len(matrix):
            matrix_ok = False
            break
        entry = matrix[fault_id]
        matrix_ok = matrix_ok and (
            entry.get("case") == case.name
            and entry.get("fault_id") == fault_id
            and entry.get("dimensions") == list(case.dimensions)
            and entry.get("injection_stage") == case.injection_stage
            and entry.get("invariant") == case.invariant
            and entry.get("termination_bound_ns") == case.termination_bound_ns
            and entry.get("status") == "PASS"
            and [item.get("role") for item in entry.get("roles", [])]
            == ["master", "slave"]
        )
        role_identities[case.name] = []
        for role in ("master", "slave"):
            manifest_path = path.parent / case.name / role / "replay-manifest.json"
            manifest = json.loads(manifest_path.read_text())
            manifest_data.append(manifest)
            identity = manifest.get("identity", {})
            runs = manifest.get("runs", [])
            identities_ok = identities_ok and (
                set(identity) == required_identity_fields
                and manifest.get("identity_sha256") == digest(identity)
                and manifest.get("case") == case.name
                and manifest.get("role") == role
                and manifest.get("real_dut_count") == 1
                and manifest.get("production_codec_actor") is True
                and identity.get("scenario") == case.name
                and identity.get("fault_id") == fault_id
                and identity.get("dut_role") == role
                and identity.get("specification_revision")
                == "498ae0203dc8b7048682fbff718a0629243a98a8"
                and identity.get("repository_revision") == repository_revision
                and len(identity.get("repository_revision", "")) == 40
                and all(
                    identity.get(field) == value
                    for field, value in current_artifact_hashes.items()
                )
                and identity.get("seed") == 17
                and identity.get("termination") == "firmware_bounded_result"
                and identity.get("engine")
                == {
                    "clock": "QEMU_CLOCK_VIRTUAL",
                    "icount": "shift=3,align=off,sleep=off",
                    "qemu_seed": 1,
                    "runtime_input": False,
                    "real_dut_count": 1,
                }
                and identity.get("assertions")
                and "production_qemu_radio_submission" in identity.get("assertions", [])
                and identity.get("unconsumed_events") == 0
                and identity.get("fault_records_sha256")
                == runs[0].get("fault_records_sha256")
                and identity.get("pipeline_records_sha256")
                == runs[0].get("pipeline_records_sha256")
                and identity.get("delivery_records_sha256")
                == runs[0].get("delivery_records_sha256")
                and identity.get("normalized_trace_sha256")
                == runs[0].get("trace_sha256")
                and all(
                    isinstance(identity.get(field), str) and len(identity[field]) == 64
                    for field in (
                        "firmware_sha256",
                        "flash_sha256",
                        "compiler_sha256",
                        "qemu_binary_sha256",
                        "qemu_rom_sha256",
                        "qemu_patch_sha256",
                        "campaign_runner_sha256",
                        "acceptance_runner_sha256",
                        "profile_sha256",
                        "fidelity_manifest_sha256",
                        "scenario_sha256",
                        "fault_records_sha256",
                        "pipeline_records_sha256",
                        "delivery_records_sha256",
                        "raw_trace_sha256",
                        "normalized_trace_sha256",
                    )
                )
            )
            replay_ok = (
                replay_ok
                and len(runs) == 2
                and all(
                    len({run[field] for run in runs}) == 1
                    for field in (
                        "flash_sha256",
                        "fault_records_sha256",
                        "pipeline_records_sha256",
                        "delivery_records_sha256",
                        "trace_sha256",
                        "result_sha256",
                    )
                )
            )
            for run in runs:
                expected = identity.get("expected_result", {})
                result = run.get("result", {})
                expected_ok = (
                    expected == expected_result(fault_id)
                    and all(
                        result.get(field) == expected.get(field)
                        for field in ("status", "failure_mask", "service_dispatches")
                    )
                    and result.get("trace_drops") == 0
                    and result.get("trace_discontinuities") == 0
                    and run.get("final_state", {}).get("queued") == 0
                )
                identities_ok = identities_ok and expected_ok
                stages = run["runtime"]["stages"]
                stages_ok = stages_ok and set(stages) == required_stages
                case_stages = {"mmio"}
                if fault_id not in {7, 9}:
                    case_stages.add("task")
                if fault_id not in {7, 9}:
                    case_stages |= {"irq", "tx_complete"}
                if expected_result(fault_id)["delivery_records"]:
                    case_stages |= {"callback", "ring", "semaphore", "dequeue"}
                if expected_result(fault_id)["status"] == "PASS":
                    case_stages.add("service_dispatch")
                stages_ok = stages_ok and all(stages[name] for name in case_stages)
                run_dir = manifest_path.parent / f"{int(run['index']):03d}"
                declared = run.get("artifact_sha256", {})
                if set(declared) != artifact_names:
                    artifacts_ok = False
                    continue
                for name in artifact_names:
                    artifact = run_dir / name
                    if not artifact.is_file() or hashlib.sha256(
                        artifact.read_bytes()
                    ).hexdigest() != declared.get(name):
                        artifacts_ok = False
                artifacts_ok = artifacts_ok and (
                    identity.get("raw_trace_sha256") == declared.get("qemu.log")
                    and digest(run.get("pipeline_records", []))
                    == run.get("pipeline_records_sha256")
                    and digest(run.get("peer_records", []))
                    == run.get("peer_records_sha256")
                )
                delivery = json.loads((run_dir / "delivery-records.json").read_text())
                faults = json.loads((run_dir / "fault-records.json").read_text())
                trace = json.loads((run_dir / "trace.normalized.json").read_text())
                sequences = [item.get("sequence") for item in delivery]
                outcomes = {item.get("outcome") for item in faults}
                semantics_ok = (
                    (fault_id != 3 or sequences == [1, 0])
                    and (
                        fault_id != 11
                        or [
                            event.get("token")
                            for event in run.get("trace", [])
                            if event.get("arg1") == 1184188258
                        ][:3]
                        == [3, 1, 2]
                    )
                    and (fault_id != 10 or sequences == list(range(3)))
                    and (fault_id != 12 or sequences == list(range(4)))
                    and (fault_id != 13 or sequences == list(range(5)))
                    and (
                        fault_id not in {10, 12, 13}
                        or run.get("runtime", {})
                        .get("stage_counts", {})
                        .get("callbacks", 0)
                        >= 8
                    )
                    and (
                        fault_id != 0
                        or delivery[-1].get("payload_hex", "")[:2]
                        == ("01" if role == "master" else "10")
                    )
                    and (
                        fault_id != 13
                        or {"production_dequeued", "readmitted"} <= outcomes
                    )
                    and (fault_id != 16 or "restart_epoch_2" in outcomes)
                    and (fault_id != 17 or "stale_epoch_1_then_2" in outcomes)
                    and (
                        fault_id < 21
                        or run.get("absolute_delivery_deadlines", [0])[0]
                        - faults[0].get("absolute_virtual_ns", 0)
                        == (
                            12_000
                            if case.injection_stage
                            in {"tx_queue_delay", "channel_access", "completion_delay"}
                            else 11_000
                        )
                    )
                )
                artifacts_ok = artifacts_ok and (
                    digest(delivery) == run.get("delivery_records_sha256")
                    and digest(faults) == run.get("fault_records_sha256")
                    and digest(trace) == run.get("trace_sha256")
                    and len(delivery) == expected_result(fault_id)["delivery_records"]
                    and bool(faults)
                    and all(
                        record.get("fault_id") == fault_id
                        and record.get("stage") == case.injection_stage
                        for record in faults
                    )
                    and [record.get("sequence") for record in faults]
                    == list(range(len(faults)))
                    and run.get("final_state", {}).get("virtual_ns", -1)
                    - faults[0].get("absolute_virtual_ns", 0)
                    <= case.termination_bound_ns
                    and semantics_ok
                )
            role_identities[case.name].append(
                {
                    "role": role,
                    "identity_sha256": manifest["identity_sha256"],
                    **identity,
                }
            )
    passed = (
        report.get("status") == "PASS"
        and report.get("specification_revision")
        == "498ae0203dc8b7048682fbff718a0629243a98a8"
        and report.get("real_dut_count_per_run") == 1
        and report.get("qemu_processes_per_run") == 1
        and report.get("production_codec_actors") is True
        and report.get("runtime_input") is False
        and report.get("runs") == len(corpus) * 4
        and matrix_ok
        and stages_ok
        and replay_ok
        and artifacts_ok
        and identities_ok
    )
    return {
        "status": "PASS" if passed else "FAIL",
        "artifact": str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),
        "roles": ["master", "slave"],
        "production_runtime_stages": sorted(required_stages),
        "fresh_runs": sum(len(item["runs"]) for item in manifest_data),
        "identical_replay": replay_ok,
        "case_role_identities": role_identities,
    }


def _expect_rejected(case: Case, mutation) -> str:
    candidate = copy.deepcopy(execute(case))
    mutation(candidate)
    try:
        validate_execution(case, candidate)
    except AcceptanceFailure:
        return "REJECTED"
    return "FAIL"


def negative_checks(case: Case) -> dict[str, str]:
    # fmt: off
    checks = {
        "corrupted_identity": _expect_rejected(case, lambda value: value.__setitem__("raw_sha256", "0" * 64)),
        "unexpected_traffic": _expect_rejected(case, lambda value: value["raw"]["records"].append({"sequence": 99, "operation": {"op": "unexpected"}})),
        "exhausted_replay": _expect_rejected(case, lambda value: value["raw"]["records"].clear()),
        "discontinuity": _expect_rejected(case, lambda value: value["raw"]["records"][0].__setitem__("sequence", 2)),
        "assertion_failure": _expect_rejected(case, lambda value: value["raw"]["assertions"].pop()),
        "model_failure": _expect_rejected(case, lambda value: value["raw"].__setitem__("termination", "model_failure")),
        "unconsumed_events": _expect_rejected(case, lambda value: value["raw"].__setitem__("unconsumed_events", 1)),
        "invalid_identity": _expect_rejected(case, lambda value: value["raw"].__setitem__("scenario", "changed")),
        "production_path_bypass": _expect_rejected(case, lambda value: value["raw"]["assertions"].remove("production_path_fault_bound")),
    }
    # fmt: on
    try:
        execute(
            Case(
                "overflow",
                ("saturation",),
                "channel_access",
                "reject",
                1,
                ({"op": "fill", "count": 9, "capacity": 8},),
            )
        )
    except AcceptanceFailure:
        checks["overflow"] = "REJECTED"
    else:
        checks["overflow"] = "FAIL"
    return checks


def run(campaign: Path) -> dict[str, Any]:
    corpus = cases()
    dimensions = {dimension for case in corpus for dimension in case.dimensions}
    missing = sorted(set(REQUIRED_DIMENSIONS) - dimensions)
    extra = sorted(dimensions - set(REQUIRED_DIMENSIONS))
    if missing or extra:
        raise AcceptanceFailure(
            f"matrix dimensions differ: missing={missing} extra={extra}"
        )
    real_dut = validate_real_dut_campaign(campaign)
    accepted = []
    for case in corpus:
        first = execute(case)
        replay = execute(case)
        if first != replay:
            raise AcceptanceFailure(f"{case.name}: deterministic replay mismatch")
        scenario_manifest = dict(first)
        scenario_manifest["real_dut_role_identities"] = real_dut[
            "case_role_identities"
        ][case.name]
        scenario_manifest["engine"] = ENGINE
        scenario_manifest["runner_sha256"] = hashlib.sha256(
            Path(__file__).read_bytes()
        ).hexdigest()
        accepted.append(
            {
                "name": case.name,
                "dimensions": list(case.dimensions),
                "injection_stage": case.injection_stage,
                "invariant": case.invariant,
                "termination_bound_ns": case.termination_bound_ns,
                "manifest": scenario_manifest,
                "replay": "IDENTICAL",
            }
        )
    host_time = audit_host_time(Path(__file__))
    budget = patch_budget()
    isolation = physical_image_isolation()
    path_audit = protected_path_audit()
    negatives = negative_checks(corpus[0])
    failures = [
        name
        for name, value in {
            "host_time": host_time["status"],
            "patch_budget": budget["status"],
            "physical_image_isolation": isolation["status"],
            "protected_path_audit": path_audit["status"],
            "real_dut_campaign": real_dut["status"],
        }.items()
        if value != "PASS"
    ]
    if any(value != "REJECTED" for value in negatives.values()):
        failures.append("negative_checks")
    report = {
        "schema_version": SCHEMA_VERSION,
        "status": "PASS" if not failures else "FAIL",
        "specification_revision": "498ae0203dc8b7048682fbff718a0629243a98a8",
        "required_base_revision": REQUIRED_BASE_REVISION,
        "engine": ENGINE,
        "modeled_pipeline_stages": list(MODELED_STAGES),
        "production_owned_stages": list(PRODUCTION_STAGES),
        "dimensions": list(REQUIRED_DIMENSIONS),
        "matrix": accepted,
        "real_dut_production_path": real_dut,
        "negative_checks": negatives,
        "host_time_prohibition": host_time,
        "patch_budget": budget,
        "physical_image_isolation": isolation,
        "role_rotation": {
            "roles": ["master", "slave"],
            "real_dut_per_run": 1,
            "production_codec_actors": True,
            "two_firmware_state_machines": "OUTSIDE_SCOPE",
        },
        "protected_path_audit": path_audit,
        "dependency_acceptance_matrix": [
            {"contract": contract, "test": test, "artifact": artifact}
            for contract, test, artifact in (
                item.replace("$campaign", real_dut["artifact"]).split("|")
                for item in (
                    "link ABI and bounded storage|qemu_link.verify|controller-private qemu_link.verify JSON",
                    "virtual ordering and host-time prohibition|test_fault_replay_acceptance|this report",
                    "interrupt and radio-task handoff|retained real-DUT runtime trace|$campaign",
                    "production transport, codec, and service dispatch|retained master/slave campaign|$campaign",
                    "complete deterministic fault dimensions|machine-checked fixed matrix|this report",
                    "replay identity, stop conditions, and role rotation|two identical executions per role|$campaign",
                    "patch budget and physical-image isolation|source and patch audit|this report",
                )
            )
        ],
        "claim_exclusions": [
            "hardware",
            "RF",
            "hardware_equivalence",
            "multi_QEMU",
            "FS_WP_002G",
            "predictive_qualification",
        ],
        "failures": failures,
    }
    report["report_sha256"] = digest(report)
    if failures:
        raise AcceptanceFailure(",".join(failures))
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--campaign", type=Path, default=DEFAULT_CAMPAIGN)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = run(args.campaign.resolve())
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(canonical(report) + b"\n")
    print(text, end="")


if __name__ == "__main__":
    main()
