#!/usr/bin/env python3
"""Run and retain the deterministic one-DUT functional-peer campaign."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from deterministic_peer_backplane import (  # noqa: E402
    PRODUCTION_CODEC,
    ReplayIdentity,
    resolve_scenario,
)
from qemu_feasibility import (  # noqa: E402
    build_qemu_command,
    discover_toolchain,
    execute_until_marker,
    generate_run_images,
    sha256_file,
    verify_run_images_unchanged,
)
from qemu_link.verify import MANIFEST as QEMU_MANIFEST  # noqa: E402
from qemu_link.verify import PATCH as QEMU_PATCH
from qemu_link.verify import (
    validate_runtime_log,
)

MARKER = "DOMES_QEMU_LINK_RESULT"
ROLES = {"master": 1, "slave": 2}
DELIVERY_PATTERN = re.compile(
    r"DOMES_PEER_DELIVERY schema=1 deadline_ns=(\d+) class=(\d+) "
    r"source=(\d+) destination=(\d+) sequence=(\d+) correlation=(\d+) "
    r"payload=([0-9a-f]+)"
)
TRACE_PATTERN = re.compile(
    r"DOMES_QEMU_LINK_TRACE schema=1 index=(\d+) timestamp=(\d+) task=(\d+) "
    r"type=(\d+) arg1=(\d+) token=(\d+)"
)


class CampaignFailure(RuntimeError):
    """A fail-closed campaign, identity, or evidence violation."""


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest(value: object) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def scenario(role: str) -> dict[str, object]:
    if role not in ROLES:
        raise CampaignFailure(f"unsupported DUT role: {role}")
    complement = "slave" if role == "master" else "master"
    return {
        "schema_version": 1,
        "name": "real_dut_role_rotation_v1",
        "model": "functional-peer-v1",
        "seed": 17,
        "dut_role": role,
        "dut_id": 1,
        "dut_mac": "02:00:00:00:00:01",
        "queue_capacity": 8,
        "termination_ns": 10_000_000_000,
        "actors": [
            {
                "pod_id": 2,
                "role": complement,
                "mac": "02:00:00:00:00:02",
                "peer_delay_ns": 1_000,
                "reaction_time_us": 500,
            }
        ],
        "expected_dut_types": ["BEACON"],
    }


def parse_delivery_records(text: str) -> list[dict[str, object]]:
    records = [
        {
            "deadline_ns": int(deadline),
            "event_class_priority": int(event_class),
            "source_id": int(source),
            "destination_id": int(destination),
            "sequence": int(sequence),
            "correlation": int(correlation),
            "codec": PRODUCTION_CODEC,
            "payload_hex": payload,
        }
        for deadline, event_class, source, destination, sequence, correlation, payload in (
            DELIVERY_PATTERN.findall(text)
        )
    ]
    if not records:
        raise CampaignFailure("real-DUT run emitted no functional-peer deliveries")
    if records != sorted(
        records,
        key=lambda item: (
            item["deadline_ns"],
            item["event_class_priority"],
            item["source_id"],
            item["destination_id"],
            item["sequence"],
        ),
    ):
        raise CampaignFailure("delivery records violate total event ordering")
    base_deadline = int(records[0]["deadline_ns"])
    for record in records:
        record["deadline_ns"] = int(record["deadline_ns"]) - base_deadline
    return records


def normalized_trace(text: str) -> list[dict[str, int]]:
    records = [
        {
            "index": int(index),
            "timestamp": int(timestamp),
            "task": int(task),
            "type": int(event_type),
            "arg1": int(arg1),
            "token": int(token),
        }
        for index, timestamp, task, event_type, arg1, token in TRACE_PATTERN.findall(
            text
        )
    ]
    if not records or [record["index"] for record in records] != list(
        range(len(records))
    ):
        raise CampaignFailure("runtime trace is absent or non-contiguous")
    timestamps = [record["timestamp"] for record in records]
    if timestamps != sorted(timestamps):
        raise CampaignFailure("runtime trace virtual time regressed")
    return [
        {key: value for key, value in record.items() if key != "timestamp"}
        for record in records
    ]


def require_identical(role: str, runs: Sequence[Mapping[str, object]]) -> None:
    if len(runs) < 2:
        raise CampaignFailure(f"{role} requires at least two fresh executions")
    for field in ("delivery_records_sha256", "trace_sha256", "flash_sha256"):
        values = {str(run[field]) for run in runs}
        if len(values) != 1:
            raise CampaignFailure(f"{role} {field} changed across fresh executions")


def require_identity(expected: Mapping[str, object], actual: ReplayIdentity) -> None:
    try:
        ReplayIdentity.create(expected).require_match(actual)
    except Exception as error:
        raise CampaignFailure(str(error)) from error


def _read_fidelity_hash(build_dir: Path) -> str:
    path = build_dir / "domes-fidelity-manifest.json"
    return sha256_file(path) if path.is_file() else "0" * 64


def _flash_size(sdkconfig: Path) -> str:
    text = sdkconfig.read_text()
    matches = re.findall(r"^CONFIG_ESPTOOLPY_FLASHSIZE_(\d+MB)=y$", text, re.MULTILINE)
    if len(matches) != 1:
        raise CampaignFailure("SDKCONFIG must select exactly one flash size")
    return matches[0]


def run_campaign(args: argparse.Namespace) -> dict[str, object]:
    args.artifact_dir = args.artifact_dir.resolve()
    args.build_dir = args.build_dir.resolve()
    args.qemu_binary = args.qemu_binary.resolve()
    if args.runs_per_role < 2:
        raise CampaignFailure("runs-per-role must be at least two")
    if args.artifact_dir.exists():
        raise CampaignFailure("artifact directory must not already exist")
    args.artifact_dir.mkdir(parents=True)

    discovered = discover_toolchain(require_gdb=False)
    qemu = args.qemu_binary.resolve()
    toolchain = dataclasses.replace(
        discovered, qemu=qemu, qemu_sha256=sha256_file(qemu)
    )
    qemu_manifest = json.loads(QEMU_MANIFEST.read_text())
    firmware = args.build_dir / "domes_qemu_link_probe.elf"
    if not firmware.is_file():
        raise CampaignFailure(f"missing real-DUT firmware ELF: {firmware}")

    description_path = args.build_dir / "project_description.json"
    if not description_path.is_file():
        raise CampaignFailure(f"missing build description: {description_path}")
    description = json.loads(description_path.read_text())
    profile_path = Path(str(description.get("config_file", ""))).resolve()
    if not profile_path.is_file():
        raise CampaignFailure(f"missing isolated SDKCONFIG: {profile_path}")
    profile_sha256 = sha256_file(profile_path)
    flash_size = _flash_size(profile_path)
    fidelity_sha256 = _read_fidelity_hash(args.build_dir)
    all_runs: list[dict[str, object]] = []
    matrix: list[dict[str, object]] = []
    identities: dict[str, ReplayIdentity] = {}

    for role, role_number in ROLES.items():
        resolved = resolve_scenario(scenario(role))
        role_runs: list[dict[str, object]] = []
        for index in range(1, args.runs_per_role + 1):
            run_dir = args.artifact_dir / role / f"{index:03d}"
            images = generate_run_images(
                toolchain, args.build_dir, run_dir, flash_size=flash_size
            )
            command = build_qemu_command(
                qemu, Path(images["flash"]), Path(images["efuse"])
            )
            command.extend(
                [
                    "-d",
                    "guest_errors",
                    "-global",
                    "domes-link.scenario-model=1",
                    "-global",
                    "domes-link.scenario-seed=17",
                    "-global",
                    f"domes-link.dut-role={role_number}",
                    "-global",
                    "domes-link.peer-delay-ns=1000",
                ]
            )
            qemu_log = run_dir / "qemu.log"
            execution = execute_until_marker(command, qemu_log, args.timeout, MARKER)
            text = str(execution.pop("text")).replace("\r", "")
            qemu_log.write_text(text, newline="\n")
            runtime = validate_runtime_log(qemu_log)
            if runtime["status"] != "PASS":
                raise CampaignFailure(f"{role} real-DUT runtime trace failed")
            deliveries = parse_delivery_records(text)
            trace = normalized_trace(text)
            delivery_path = run_dir / "delivery-records.json"
            trace_path = run_dir / "trace.normalized.json"
            delivery_path.write_bytes(canonical(deliveries) + b"\n")
            trace_path.write_bytes(canonical(trace) + b"\n")
            image_integrity = verify_run_images_unchanged(images)
            run = {
                "role": role,
                "index": index,
                "command": command,
                "flash_sha256": images["flash_sha256"],
                "efuse_sha256": images["efuse_sha256"],
                "delivery_records_sha256": digest(deliveries),
                "trace_sha256": digest(trace),
                "runtime": runtime,
                "execution": execution,
                "image_integrity": image_integrity,
                "artifact_sha256": {
                    "delivery-records.json": sha256_file(delivery_path),
                    "efuse-generation.log": sha256_file(
                        run_dir / "efuse-generation.log"
                    ),
                    "flash-generation.log": sha256_file(
                        run_dir / "flash-generation.log"
                    ),
                    "qemu.log": sha256_file(qemu_log),
                    "trace.normalized.json": sha256_file(trace_path),
                },
            }
            Path(images["flash"]).unlink()
            Path(images["efuse"]).unlink()
            role_runs.append(run)
            all_runs.append(run)

        require_identical(role, role_runs)
        first = role_runs[0]
        inputs = {
            "scenario": scenario(role),
            "icount": "shift=3,align=off,sleep=off",
            "vcpus": 2,
            "qemu_seed": 1,
            "runtime_input": "pre-reset-properties-only",
        }
        identity_values = {
            "schema_version": 1,
            "firmware_sha256": sha256_file(firmware),
            "flash_sha256": first["flash_sha256"],
            "toolchain_identity": (
                f"{discovered.idf_version};{discovered.compiler_version}"
            ),
            "qemu_revision": qemu_manifest["upstream_revision"],
            "qemu_patch_sha256": sha256_file(QEMU_PATCH),
            "profile_sha256": profile_sha256,
            "fidelity_manifest_sha256": fidelity_sha256,
            "scenario_schema": 1,
            "scenario_model": resolved.model,
            "scenario_seed": resolved.seed,
            "resolved_scenario_sha256": resolved.resolved_sha256,
            "icount_shift": 3,
            "vcpu_count": 2,
            "input_records_sha256": digest(inputs),
            "trace_sha256": first["trace_sha256"],
            "assertions": [
                "single_real_dut",
                "functional_actor",
                "production_codec",
                "production_transport",
                "virtual_time_only",
            ],
            "termination": "assertions_passed",
            "unconsumed_events": 0,
            "delivery_records_sha256": first["delivery_records_sha256"],
        }
        identity = ReplayIdentity.create(identity_values)
        identities[role] = identity
        role_dir = args.artifact_dir / role
        manifest = {
            "schema_version": 1,
            "role": role,
            "real_dut_count": 1,
            "functional_actors": [
                {"pod_id": actor.pod_id, "role": actor.role}
                for actor in resolved.actors
            ],
            "codec": PRODUCTION_CODEC,
            "scenario": scenario(role),
            "inputs": inputs,
            "identity": identity.values,
            "identity_sha256": identity.digest,
            "runs": role_runs,
        }
        (role_dir / "replay-manifest.json").write_bytes(canonical(manifest) + b"\n")
        matrix.append(
            {
                "real_dut_role": role,
                "functional_actor_roles": [actor.role for actor in resolved.actors],
                "codec": PRODUCTION_CODEC,
                "expected_invariant": "production service dispatches one actor response",
                "result": "PASS",
                "identity_sha256": identity.digest,
            }
        )

    if args.expected_identity:
        expected = json.loads(args.expected_identity.read_text())
        expected_role = str(expected.get("dut_role", ""))
        actual_path = args.artifact_dir / expected_role / "replay-manifest.json"
        if not actual_path.is_file():
            raise CampaignFailure("expected identity does not name a campaign DUT role")
        actual = ReplayIdentity.create(json.loads(actual_path.read_text())["identity"])
        require_identity(expected["identity"], actual)

    changed_identity = dict(identities["master"].values)
    changed_identity["scenario_seed"] = int(changed_identity["scenario_seed"]) + 1
    mismatch = ""
    try:
        identities["master"].require_match(ReplayIdentity.create(changed_identity))
    except Exception as error:
        mismatch = str(error)
    if "scenario_seed" not in mismatch:
        raise CampaignFailure("negative replay identity check did not fail closed")
    negative_identity = {
        "schema_version": 1,
        "status": "REJECTED",
        "changed_field": "scenario_seed",
        "baseline_identity_sha256": identities["master"].digest,
        "candidate_identity_sha256": ReplayIdentity.create(changed_identity).digest,
        "failure": mismatch,
    }
    (args.artifact_dir / "negative-identity-check.json").write_bytes(
        canonical(negative_identity) + b"\n"
    )

    report = {
        "schema_version": 1,
        "status": "PASS",
        "real_dut_count_per_run": 1,
        "qemu_processes_per_run": 1,
        "roles": matrix,
        "runs": len(all_runs),
        "negative_identity_check": negative_identity,
        "virtual_time_audit": {
            "qemu_clock": "QEMU_CLOCK_VIRTUAL",
            "icount": "shift=3,align=off,sleep=off",
            "rtc": "clock=vm",
            "seed": "immutable command input 1",
            "runtime_socket": False,
            "host_packet_delivery": False,
            "interactive_input": False,
            "host_wall_clock_callback": False,
            "pointer_order": False,
            "filesystem_order": False,
        },
    }
    (args.artifact_dir / "campaign-report.json").write_bytes(canonical(report) + b"\n")
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu-binary", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--runs-per-role", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--expected-identity", type=Path)
    args = parser.parse_args()
    print(json.dumps(run_campaign(args), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
