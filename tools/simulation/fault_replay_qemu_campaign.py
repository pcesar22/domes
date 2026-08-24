#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any, Mapping

from deterministic_peer_campaign import (
    ROLES,
    normalized_trace,
    parse_delivery_records,
)
from fault_replay_acceptance import (
    MODELED_STAGES,
    Case,
    canonical,
    cases,
    digest,
    expected_result,
)
from qemu_feasibility import (
    Toolchain,
    build_qemu_command,
    execute_until_marker,
    generate_run_images,
    sha256_file,
    verify_run_images_unchanged,
)
from qemu_link.verify import PATCH, validate_runtime_log

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MARKER = "DOMES_QEMU_LINK_RESULT"
SPECIFICATION_REVISION = "498ae0203dc8b7048682fbff718a0629243a98a8"
QEMU_REVISION = "4f4148e2f68689eb8861bf9fce0b46ada9200fef"
ACCEPTANCE_RUNNER = HERE / "fault_replay_acceptance.py"
QEMU_ROM_NAME = "esp32s3_rev0_rom.bin"
FAULT_PATTERN = re.compile(
    r"DOMES_FAULT_RECORD schema=1 fault=(\d+) sequence=(\d+) virtual_ns=(\d+) "
    r"stage=(\S+) outcome=(\S+) queued=(\d+)"
)
RESULT_PATTERN = re.compile(
    r"DOMES_QEMU_LINK_RESULT schema=2 status=(PASS|FAIL) failure_mask=(0x[0-9a-f]+) "
    r"token=(\d+) service_dispatches=(\d+) trace_drops=(\d+) trace_discontinuities=(\d+)"
)
STATE_PATTERN = re.compile(r"DOMES_FAULT_STATE schema=1 virtual_ns=(\d+) queued=(\d+)")
PIPELINE_PATTERN = re.compile(
    r"DOMES_PIPELINE_DELAY schema=1 tx_queue=(\d+) channel=(\d+) airtime=(\d+) "
    r"completion=(\d+) peer=(\d+) rx_callback=(\d+)"
)


class CampaignFailure(RuntimeError):
    pass


def repository_revision() -> str:
    return subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()


def _campaign_toolchain(qemu: Path) -> Toolchain:
    idf_path = Path(os.environ["IDF_PATH"]).resolve()
    python_env = Path(os.environ["IDF_PYTHON_ENV_PATH"]).resolve()
    python = python_env / "bin/python"
    compiler = Path(shutil.which("xtensa-esp32s3-elf-g++") or "").resolve()
    if not python.is_file() or not compiler.is_file() or not qemu.is_file():
        raise CampaignFailure(
            "pinned ESP-IDF, compiler, or patched QEMU is unavailable"
        )

    def output(command: list[str]) -> str:
        return subprocess.run(
            command, check=True, stdout=subprocess.PIPE, text=True
        ).stdout.strip()

    idf_revision = output(["git", "-C", str(idf_path), "rev-parse", "HEAD"])
    compiler_version = output([str(compiler), "--version"]).splitlines()[0]
    qemu_version = output([str(qemu), "--version"]).splitlines()[0]
    return Toolchain(
        idf_path=idf_path,
        idf_version="ESP-IDF v5.4.4",
        idf_revision=idf_revision,
        python=python,
        compiler=compiler,
        compiler_version=compiler_version,
        compiler_sha256=sha256_file(compiler),
        compiler_archive=None,
        compiler_archive_sha256=None,
        qemu=qemu,
        qemu_version=qemu_version,
        qemu_sha256=sha256_file(qemu),
        qemu_archive=None,
        qemu_archive_sha256=None,
        qemu_dynamic_dependencies="campaign-local source build",
        libslirp=Path(),
        libslirp_sha256="statically-linked-campaign-build",
        gdb=Path(),
        gdb_version="not-required",
    )


def _fault_records(text: str) -> list[dict[str, object]]:
    records = [
        {
            "fault_id": int(fault),
            "sequence": int(sequence),
            "virtual_ns": int(virtual_ns),
            "stage": stage,
            "outcome": outcome,
            "queued": int(queued),
        }
        for fault, sequence, virtual_ns, stage, outcome, queued in FAULT_PATTERN.findall(
            text
        )
    ]
    if records:
        base_virtual_ns = int(records[0]["virtual_ns"])
        for record in records:
            record["virtual_ns"] = int(record["virtual_ns"]) - base_virtual_ns
    return records


def _replay_trace(text: str) -> list[dict[str, object]]:
    return normalized_trace(text)


def _result(text: str) -> dict[str, object]:
    match = RESULT_PATTERN.search(text)
    if not match:
        raise CampaignFailure("real firmware DUT emitted no bounded result")
    return {
        "status": match.group(1),
        "failure_mask": match.group(2),
        "token": int(match.group(3)),
        "service_dispatches": int(match.group(4)),
        "trace_drops": int(match.group(5)),
        "trace_discontinuities": int(match.group(6)),
    }


def _required_stages(fault_id: int) -> set[str]:
    stages = {"mmio"}
    if fault_id not in {7, 9}:
        stages.add("task")
    if fault_id not in {7, 9}:
        stages |= {"irq", "tx_complete"}
    if expected_result(fault_id)["delivery_records"]:
        stages |= {"callback", "ring", "semaphore", "dequeue"}
    if expected_result(fault_id)["status"] == "PASS":
        stages.add("service_dispatch")
    return stages


def _pipeline_records(text: str) -> list[dict[str, int]]:
    names = (
        "tx_queue_delay",
        "channel_access",
        "airtime",
        "completion_delay",
        "peer_processing",
        "rx_callback_delay",
    )
    return [
        dict(zip(names, map(int, values), strict=True))
        for values in PIPELINE_PATTERN.findall(text)
    ]


def _validate_run(case: Case, fault_id: int, run: Mapping[str, Any]) -> None:
    records = run["fault_records"]
    if not records or any(record["fault_id"] != fault_id for record in records):
        raise CampaignFailure(f"{case.name}: missing exact QEMU fault injection record")
    if any(record["stage"] != case.injection_stage for record in records):
        raise CampaignFailure(
            f"{case.name}: fault stage differs from {case.injection_stage}"
        )
    if [record["sequence"] for record in records] != list(range(len(records))):
        raise CampaignFailure(f"{case.name}: discontinuous fault record sequence")
    if run["result"]["trace_drops"] or run["result"]["trace_discontinuities"]:
        raise CampaignFailure(f"{case.name}: firmware trace overflow or discontinuity")
    expected = expected_result(fault_id)
    if any(
        run["result"].get(field) != expected[field]
        for field in ("status", "failure_mask", "service_dispatches")
    ):
        raise CampaignFailure(
            f"{case.name}: firmware outcome differs from specification"
        )
    if len(run["delivery_records"]) != expected["delivery_records"]:
        raise CampaignFailure(f"{case.name}: incomplete delivery record")
    deliveries = run["delivery_records"]
    sequences = [item["sequence"] for item in deliveries]
    if fault_id == 3 and sequences != [1, 0]:
        raise CampaignFailure(f"{case.name}: submit order was not reversed")
    if fault_id == 11 and sequences != [2, 0, 1]:
        raise CampaignFailure(f"{case.name}: completion order did not change to 3,1,2")
    if fault_id == 10 and sequences != list(range(3)):
        raise CampaignFailure(
            f"{case.name}: callback order or saturation capacity changed"
        )
    counts = run["runtime"]["stage_counts"]
    if fault_id in {10, 12, 13} and counts["callbacks"] < 8:
        raise CampaignFailure(
            f"{case.name}: production callbacks did not carry the burst"
        )
    if fault_id == 0 and deliveries[-1]["payload_hex"][:2] != (
        "01" if run["role"] == "master" else "10"
    ):
        raise CampaignFailure(
            f"{case.name}: role-specific production message was not dispatched"
        )
    service_messages = counts["service_messages"]
    expected_message = "EspNow.RxBeacon" if run["role"] == "master" else "EspNow.RxJoinGame"  # fmt: skip
    if expected["status"] == "PASS" and (expected_message not in service_messages or any(name != expected_message for name in service_messages)):  # fmt: skip
        raise CampaignFailure(f"{case.name}: wrong production role interaction")
    outcomes = [record["outcome"] for record in records]
    if fault_id == 12 and (sequences != list(range(4)) or counts["rx_queue"] != 4):
        raise CampaignFailure(f"{case.name}: production receive capacity was not saturated")  # fmt: skip
    if fault_id == 13 and (sequences != list(range(5)) or counts["rx_queue"] != 5):
        raise CampaignFailure(f"{case.name}: recovered frame was not delivered")
    if fault_id == 13 and not {"production_dequeued", "readmitted"} <= set(outcomes):
        raise CampaignFailure(f"{case.name}: no dequeue and readmission recovery")
    if fault_id == 16 and "restart_epoch_2" not in outcomes:
        raise CampaignFailure(f"{case.name}: peer epoch did not restart")
    if fault_id == 17 and "stale_epoch_1_then_2" not in outcomes:
        raise CampaignFailure(
            f"{case.name}: stale and fresh epochs were not distinguished"
        )
    if fault_id >= 21 and (
        run["absolute_delivery_deadlines"][0] - records[0]["absolute_virtual_ns"]
        != (
            12_000
            if case.injection_stage
            in {"tx_queue_delay", "channel_access", "completion_delay"}
            else 11_000
        )
    ):
        raise CampaignFailure(f"{case.name}: declared stage latency was not injected")
    if fault_id >= 21 and run["pipeline_records"] != [
        {
            stage: 10_000 if stage == case.injection_stage else 0
            for stage in MODELED_STAGES
        }
    ]:
        raise CampaignFailure(
            f"{case.name}: latency changed an undeclared pipeline stage"
        )
    final = run["final_state"]
    if final["queued"] != 0:
        raise CampaignFailure(f"{case.name}: {final['queued']} unconsumed QEMU events")
    elapsed = final["virtual_ns"] - records[0]["absolute_virtual_ns"]
    if elapsed < 0 or elapsed > case.termination_bound_ns:
        raise CampaignFailure(f"{case.name}: termination bound exceeded ({elapsed} ns)")
    stages = run["runtime"]["stages"]
    missing = sorted(stage for stage in _required_stages(fault_id) if not stages[stage])
    if missing:
        raise CampaignFailure(
            f"{case.name}: production stages not exercised: {missing}"
        )
    if run["image_integrity"]["flash_sha256_after"] != run["flash_sha256"]:
        raise CampaignFailure(f"{case.name}: QEMU mutated the immutable flash input")


def _run_once(
    toolchain: Toolchain,
    build_dir: Path,
    artifact_dir: Path,
    case: Case,
    fault_id: int,
    role: str,
    role_number: int,
    index: int,
    timeout: float,
) -> dict[str, object]:
    run_dir = artifact_dir / case.name / role / f"{index:03d}"
    images = generate_run_images(toolchain, build_dir, run_dir, flash_size="8MB")
    command = build_qemu_command(
        toolchain.qemu, Path(images["flash"]), Path(images["efuse"])
    )
    qemu_rom_dir = toolchain.qemu.parent / "pc-bios"
    if not (qemu_rom_dir / QEMU_ROM_NAME).is_file():
        raise CampaignFailure(
            f"missing pinned QEMU ROM: {qemu_rom_dir / QEMU_ROM_NAME}"
        )
    command[1:1] = ["-L", str(qemu_rom_dir)]
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
            "-global",
            f"domes-link.fault-id={fault_id}",
        ]
    )
    log = run_dir / "qemu.log"
    device_log = run_dir / "qemu-device.log"
    command.extend(["-D", str(device_log)])
    execution = execute_until_marker(command, log, timeout, MARKER)
    raw_text = str(execution.pop("text")).replace("\r", "")
    text = "\n".join(line.rstrip() for line in raw_text.splitlines()) + "\n"
    log.write_text(text, newline="\n")
    device_text = device_log.read_text().replace("\r", "")
    faults = _fault_records(device_text)
    pipeline_records = _pipeline_records(device_text)
    deliveries = (
        parse_delivery_records(device_text)
        if "DOMES_PEER_DELIVERY" in device_text
        else []
    )
    absolute_delivery_deadlines = [
        int(value)
        for value in re.findall(
            r"DOMES_PEER_DELIVERY schema=1 deadline_ns=(\d+)", device_text
        )
    ]
    for record, match in zip(faults, FAULT_PATTERN.findall(device_text), strict=True):
        record["absolute_virtual_ns"] = int(match[2])
    observations = [
        (int(virtual_ns), int(queued))
        for virtual_ns, queued in STATE_PATTERN.findall(device_text)
    ]
    if not observations:
        raise CampaignFailure(f"{case.name}: QEMU emitted no observed queue state")
    final_state = {
        "virtual_ns": max(
            [record["absolute_virtual_ns"] for record in faults]
            + absolute_delivery_deadlines
            + [item[0] for item in observations]
        ),
        "queued": observations[-1][1],
        "observations": len(observations),
    }
    trace = _replay_trace(text)
    result = _result(text)
    runtime = validate_runtime_log(log)
    fault_path = run_dir / "fault-records.json"
    delivery_path = run_dir / "delivery-records.json"
    trace_path = run_dir / "trace.normalized.json"
    fault_path.write_bytes(canonical(faults) + b"\n")
    delivery_path.write_bytes(canonical(deliveries) + b"\n")
    trace_path.write_bytes(canonical(trace) + b"\n")
    image_integrity = verify_run_images_unchanged(images)
    run = {
        "role": role,
        "index": index,
        "fault_id": fault_id,
        "command": command,
        "flash_sha256": images["flash_sha256"],
        "efuse_sha256": images["efuse_sha256"],
        "fault_records": faults,
        "pipeline_records": pipeline_records,
        "delivery_records": deliveries,
        "absolute_delivery_deadlines": absolute_delivery_deadlines,
        "final_state": final_state,
        "fault_records_sha256": digest(faults),
        "pipeline_records_sha256": digest(pipeline_records),
        "delivery_records_sha256": digest(deliveries),
        "trace_sha256": digest(trace),
        "result_sha256": digest(result),
        "result": result,
        "runtime": runtime,
        "execution": execution,
        "image_integrity": image_integrity,
        "artifact_sha256": {
            name: sha256_file(run_dir / name)
            for name in (
                "delivery-records.json",
                "efuse-generation.log",
                "fault-records.json",
                "flash-generation.log",
                "qemu-device.log",
                "qemu.log",
                "trace.normalized.json",
            )
        },
    }
    _validate_run(case, fault_id, run)
    Path(images["flash"]).unlink()
    Path(images["efuse"]).unlink()
    return run


def _require_replay(case: Case, runs: list[dict[str, object]]) -> None:
    if len(runs) != 2:
        raise CampaignFailure(f"{case.name}: exactly two fixed replay runs required")
    for field in (
        "flash_sha256",
        "fault_records_sha256",
        "pipeline_records_sha256",
        "delivery_records_sha256",
        "trace_sha256",
        "result_sha256",
    ):
        if len({str(run[field]) for run in runs}) != 1:
            raise CampaignFailure(f"{case.name}: replay changed {field}")


def run_campaign(args: argparse.Namespace) -> dict[str, object]:
    artifact_dir = args.artifact_dir.resolve()
    if artifact_dir.exists():
        raise CampaignFailure("artifact directory must not already exist")
    artifact_dir.mkdir(parents=True)
    toolchain = _campaign_toolchain(args.qemu_binary.resolve())
    build_dir = args.build_dir.resolve()
    firmware = build_dir / "domes_qemu_link_probe.elf"
    profile = Path(
        json.loads((build_dir / "project_description.json").read_text())["config_file"]
    )
    matrix = []
    all_runs = 0
    for fault_id, case in enumerate(cases()):
        role_entries = []
        for role, role_number in ROLES.items():
            runs = [
                _run_once(
                    toolchain,
                    build_dir,
                    artifact_dir,
                    case,
                    fault_id,
                    role,
                    role_number,
                    index,
                    args.timeout,
                )
                for index in (1, 2)
            ]
            _require_replay(case, runs)
            all_runs += len(runs)
            identity = {
                "specification_revision": SPECIFICATION_REVISION,
                "repository_revision": repository_revision(),
                "firmware_sha256": sha256_file(firmware),
                "flash_sha256": runs[0]["flash_sha256"],
                "toolchain_identity": f"{toolchain.idf_version};{toolchain.compiler_version}",
                "compiler_sha256": toolchain.compiler_sha256,
                "qemu_revision": QEMU_REVISION,
                "qemu_binary_sha256": toolchain.qemu_sha256,
                "qemu_rom_sha256": sha256_file(
                    toolchain.qemu.parent / "pc-bios" / QEMU_ROM_NAME
                ),
                "qemu_patch_sha256": sha256_file(PATCH),
                "campaign_runner_sha256": sha256_file(Path(__file__)),
                "acceptance_runner_sha256": sha256_file(ACCEPTANCE_RUNNER),
                "profile_sha256": sha256_file(profile),
                "fidelity_manifest_sha256": sha256_file(
                    build_dir / "domes-fidelity-manifest.json"
                ),
                "scenario": case.name,
                "scenario_sha256": digest(case.__dict__),
                "seed": 17,
                "fault_id": fault_id,
                "dut_role": role,
                "engine": {
                    "clock": "QEMU_CLOCK_VIRTUAL",
                    "icount": "shift=3,align=off,sleep=off",
                    "qemu_seed": 1,
                    "runtime_input": False,
                    "real_dut_count": 1,
                },
                "fault_records_sha256": runs[0]["fault_records_sha256"],
                "delivery_records_sha256": runs[0]["delivery_records_sha256"],
                "raw_trace_sha256": sha256_file(
                    artifact_dir / case.name / role / "001/qemu.log"
                ),
                "normalized_trace_sha256": runs[0]["trace_sha256"],
                "assertions": [
                    "production_qemu_radio_submission",
                    *sorted(_required_stages(fault_id)),
                ],
                "termination": "firmware_bounded_result",
                "expected_result": expected_result(fault_id),
                "unconsumed_events": runs[0]["final_state"]["queued"],
            }
            manifest = {
                "schema_version": 1,
                "case": case.name,
                "role": role,
                "real_dut_count": 1,
                "production_codec_actor": True,
                "identity": identity,
                "identity_sha256": digest(identity),
                "runs": runs,
            }
            manifest_path = artifact_dir / case.name / role / "replay-manifest.json"
            manifest_path.write_bytes(canonical(manifest) + b"\n")
            role_entries.append(
                {
                    "role": role,
                    "identity_sha256": manifest["identity_sha256"],
                    "manifest": str(manifest_path.relative_to(artifact_dir)),
                }
            )
        matrix.append(
            {
                "case": case.name,
                "fault_id": fault_id,
                "dimensions": list(case.dimensions),
                "injection_stage": case.injection_stage,
                "invariant": case.invariant,
                "termination_bound_ns": case.termination_bound_ns,
                "roles": role_entries,
                "status": "PASS",
            }
        )
    report = {
        "schema_version": 2,
        "status": "PASS",
        "specification_revision": SPECIFICATION_REVISION,
        "real_dut_count_per_run": 1,
        "qemu_processes_per_run": 1,
        "production_codec_actors": True,
        "runtime_input": False,
        "runs": all_runs,
        "matrix": matrix,
    }
    report["report_sha256"] = digest(report)
    (artifact_dir / "campaign-report.json").write_bytes(canonical(report) + b"\n")
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu-binary", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    print(json.dumps(run_campaign(args), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
