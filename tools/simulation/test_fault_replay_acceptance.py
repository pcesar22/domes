import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

PATH = Path(__file__).with_name("fault_replay_acceptance.py")
if str(PATH.parent) not in sys.path:
    sys.path.insert(0, str(PATH.parent))
SPEC = importlib.util.spec_from_file_location("fault_replay_acceptance", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def build_campaign_fixture(root: Path) -> Path:
    repository_revision = MODULE.subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=MODULE.ROOT,
        check=True,
        text=True,
        stdout=MODULE.subprocess.PIPE,
    ).stdout.strip()
    current_hashes = {"qemu_patch_sha256": MODULE.hashlib.sha256(MODULE.PATCH.read_bytes()).hexdigest(), "campaign_runner_sha256": MODULE.hashlib.sha256((MODULE.HERE / "fault_replay_qemu_campaign.py").read_bytes()).hexdigest(), "acceptance_runner_sha256": MODULE.hashlib.sha256((MODULE.HERE / "fault_replay_acceptance.py").read_bytes()).hexdigest()}  # fmt: skip
    common_artifacts = {"delivery-records.json": b"[]", "efuse-generation.log": b"", "fault-records.json": b"[]", "flash-generation.log": b"fixture\n", "qemu-device.log": b"fixture\n", "qemu.log": b"fixture\n", "trace.normalized.json": b"[]"}  # fmt: skip
    stages = dict.fromkeys("mmio core0_radio_task core1_application_task irq task callback ring semaphore dequeue service_dispatch tx_complete".split(), True)  # fmt: skip
    matrix = []
    for fault_id, case in enumerate(MODULE.cases()):
        expected = MODULE.expected_result(fault_id)
        outcomes = {13: ["production_capacity_4", "production_dequeued", "readmitted"], 16: ["restart_epoch_2"], 17: ["stale_epoch_1_then_2"]}.get(fault_id, ["fixture"])  # fmt: skip
        faults = [{"fault_id": fault_id, "sequence": index, "virtual_ns": 0, "absolute_virtual_ns": 100, "stage": case.injection_stage, "outcome": outcome, "queued": 0} for index, outcome in enumerate(outcomes)]  # fmt: skip
        delivery_sequences = {3: [1, 0]}.get(fault_id, list(range(expected["delivery_records"])))  # fmt: skip
        deliveries = [
            {"sequence": index, "payload_hex": ""} for index in delivery_sequences
        ]
        trace = ([{"arg1": 1184188258, "token": 1, "type": 26}] * 3 + [{"arg1": 3517568895, "token": 1}] + [{"arg1": 4059320606, "token": 1, "type": 30}] * 2 + [{"arg1": 3517568895, "token": 2}] + [{"arg1": 1184188258, "token": token, "type": 26} for token in (2, 1, 1)] + [{"arg1": 4059320606, "token": 2, "type": 30}]) if fault_id == 11 else (([{"arg1": 1184188258, "token": 1, "type": 26}] * (fault_id - 8) + [{"arg1": 1184188258, "token": 4, "type": 35}]) if fault_id in {12, 13} else [])  # fmt: skip
        trace = [{**event, "index": index} for index, event in enumerate(trace)]
        artifact_contents = {
            **common_artifacts,
            "fault-records.json": MODULE.canonical(faults),
            "delivery-records.json": MODULE.canonical(deliveries),
            "trace.normalized.json": MODULE.canonical(trace),
        }
        artifact_hashes = {name: MODULE.hashlib.sha256(content).hexdigest() for name, content in artifact_contents.items()}  # fmt: skip
        fault_digest, delivery_digest = MODULE.digest(faults), MODULE.digest(deliveries)
        roles = []
        for role in ("master", "slave"):
            deliveries = [
                {**item, "payload_hex": "01" if role == "master" else "10"}
                for item in deliveries
            ]
            artifact_contents["delivery-records.json"] = MODULE.canonical(deliveries)
            artifact_hashes["delivery-records.json"] = MODULE.hashlib.sha256(
                artifact_contents["delivery-records.json"]
            ).hexdigest()
            delivery_digest = MODULE.digest(deliveries)
            deadlines = [12_100 if case.injection_stage in {"tx_queue_delay", "channel_access", "completion_delay"} else 11_100] * len(deliveries) if fault_id >= 21 else [101] * len(deliveries)  # fmt: skip
            role_dir = root / case.name / role
            runs = []
            for index in (1, 2):
                run_dir = role_dir / f"{index:03d}"
                run_dir.mkdir(parents=True)
                for name, content in artifact_contents.items():
                    (run_dir / name).write_bytes(content)
                runs.append(
                    {
                        "index": index,
                        "flash_sha256": "1" * 64,
                        "fault_records_sha256": fault_digest,
                        "pipeline_records_sha256": MODULE.digest([]),
                        "pipeline_records": [],
                        "peer_records_sha256": MODULE.digest([]),
                        "peer_records": [],
                        "delivery_records_sha256": delivery_digest,
                        "absolute_delivery_deadlines": deadlines,
                        "trace_sha256": MODULE.digest(trace),
                        "trace": trace,
                        "result_sha256": "2" * 64,
                        "result": {"status": expected["status"], "failure_mask": expected["failure_mask"], "service_dispatches": expected["service_dispatches"], "trace_drops": 0, "trace_discontinuities": 0},  # fmt: skip
                        "artifact_sha256": artifact_hashes,
                        "runtime": {
                            "stages": stages,
                            "stage_counts": {"callbacks": 12 if fault_id == 13 else 10 if fault_id == 12 else 8, "rx_queue": 5 if fault_id == 13 else 4 if fault_id == 12 else len(deliveries), "service_messages": (["EspNow.RxPing"] if fault_id == 11 else ["EspNow.RxBeacon" if role == "master" else "EspNow.RxJoinGame"]) if expected["status"] == "PASS" else []},  # fmt: skip
                        },
                        "final_state": {"virtual_ns": 100, "result_marker_virtual_ns": 100, "queued": 0, "tx_status": 0, "irq_status": 0, "sticky": 0},  # fmt: skip
                    }
                )
            identity = {
                "specification_revision": "498ae0203dc8b7048682fbff718a0629243a98a8",
                "repository_revision": repository_revision,
                "firmware_sha256": "3" * 64,
                "flash_sha256": "1" * 64,
                "toolchain_identity": "ESP-IDF v5.4.4;fixture compiler",
                "compiler_sha256": "4" * 64,
                "qemu_revision": "4f4148e2f68689eb8861bf9fce0b46ada9200fef",
                "qemu_binary_sha256": "5" * 64,
                "qemu_rom_sha256": "6" * 64,
                **current_hashes,
                "profile_sha256": "7" * 64,
                "fidelity_manifest_sha256": "8" * 64,
                "scenario": case.name,
                "scenario_sha256": "9" * 64,
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
                "unconsumed_events": 0,
                "fault_records_sha256": fault_digest,
                "pipeline_records_sha256": MODULE.digest([]),
                "delivery_records_sha256": delivery_digest,
                "raw_trace_sha256": artifact_hashes["qemu.log"],
                "normalized_trace_sha256": MODULE.digest(trace),
                "assertions": ["production_qemu_radio_submission"],
                "termination": "firmware_bounded_result",
                "expected_result": expected,
            }
            manifest = {
                "case": case.name,
                "role": role,
                "real_dut_count": 1,
                "production_codec_actor": True,
                "identity": identity,
                "identity_sha256": MODULE.digest(identity),
                "runs": runs,
            }
            (role_dir / "replay-manifest.json").write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            roles.append({"role": role})
        matrix.append(
            {
                "case": case.name,
                "fault_id": fault_id,
                "dimensions": list(case.dimensions),
                "injection_stage": case.injection_stage,
                "invariant": case.invariant,
                "termination_bound_ns": case.termination_bound_ns,
                "status": "PASS",
                "roles": roles,
            }
        )
    report = {
        "status": "PASS",
        "specification_revision": "498ae0203dc8b7048682fbff718a0629243a98a8",
        "real_dut_count_per_run": 1,
        "qemu_processes_per_run": 1,
        "production_codec_actors": True,
        "runtime_input": False,
        "runs": len(MODULE.cases()) * 4,
        "matrix": matrix,
    }
    path = root / "campaign-report.json"
    path.write_text(json.dumps(report), encoding="utf-8")
    return path


class FaultReplayAcceptanceTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.campaign = build_campaign_fixture(
            Path(self.temporary_directory.name) / "qemu-campaign"
        )

    def tearDown(self):
        self.temporary_directory.cleanup()

    def test_matrix_is_complete_bounded_and_has_explicit_invariants(self):
        cases = MODULE.cases()
        covered = {dimension for case in cases for dimension in case.dimensions}
        self.assertEqual(covered, set(MODULE.REQUIRED_DIMENSIONS))
        self.assertEqual(
            {
                case.injection_stage
                for case in cases
                if case.name.startswith("latency_")
            },
            set(MODULE.MODELED_STAGES),
        )
        self.assertTrue(
            all(case.invariant and case.termination_bound_ns > 0 for case in cases)
        )
        self.assertEqual(len({case.name for case in cases}), len(cases))

    def test_every_fixed_case_has_identical_complete_replay(self):
        for case in MODULE.cases():
            first = MODULE.execute(case)
            replay = MODULE.execute(case)
            self.assertEqual(first, replay)
            self.assertEqual(first["raw"]["unconsumed_events"], 0)
            self.assertEqual(first["raw_sha256"], MODULE.digest(first["raw"]))

    def test_overflow_and_corrupted_identity_fail_closed(self):
        overflow = MODULE.Case(
            "overflow",
            ("saturation",),
            "channel_access",
            "reject",
            1,
            ({"op": "fill", "count": 9, "capacity": 8},),
        )
        with self.assertRaisesRegex(MODULE.AcceptanceFailure, "overflow"):
            MODULE.execute(overflow)
        checks = MODULE.negative_checks(MODULE.cases()[0])
        self.assertTrue(checks)
        self.assertEqual(set(checks.values()), {"REJECTED"})

    def test_each_negative_check_executes_a_rejecting_mutation(self):
        for mutation in (
            "expected",
            "stage",
            "termination",
            "empty_faults",
            "pipeline",
            "raw_identity",
            "completion_order",
            "sequential_saturation", "failed_fourth_insertion",  # fmt: skip
        ):
            with self.subTest(
                mutation=mutation
            ), tempfile.TemporaryDirectory() as directory:
                copied = Path(directory) / "qemu-campaign"
                shutil.copytree(self.campaign.parent, copied)
                case_name = "completion_reorder" if mutation == "completion_order" else "saturation" if mutation in {"sequential_saturation", "failed_fourth_insertion"} else "pass"  # fmt: skip
                path = copied / f"{case_name}/master/replay-manifest.json"
                manifest = json.loads(path.read_text())
                if mutation == "expected":
                    manifest["identity"]["expected_result"]["status"] = "FAIL"
                elif mutation == "stage":
                    manifest["runs"][0]["runtime"]["stages"][
                        "core1_application_task"
                    ] = False
                elif mutation == "termination":
                    manifest["runs"][0]["final_state"]["virtual_ns"] = 4_000_000_001
                    manifest["runs"][0]["final_state"][
                        "result_marker_virtual_ns"
                    ] = 4_000_000_001
                elif mutation == "empty_faults":
                    (copied / "pass/master/001/fault-records.json").write_text("[]")
                elif mutation == "pipeline":
                    manifest["runs"][0]["pipeline_records"].append({"stage": "changed"})
                elif mutation == "raw_identity":
                    manifest["identity"]["raw_trace_sha256"] = "0" * 64
                elif mutation == "completion_order":
                    handoffs = [event for event in manifest["runs"][0]["trace"] if event["arg1"] == 1184188258 and event.get("type") == 26]  # fmt: skip
                    for event, token in zip(handoffs[-3:], (1, 2, 2), strict=True):  # fmt: skip
                        event["token"] = token
                elif mutation == "sequential_saturation":
                    next(event for event in manifest["runs"][0]["trace"] if event.get("type") == 35)["token"] = 1  # fmt: skip
                else:
                    handoffs = [event for event in manifest["runs"][0]["trace"] if event.get("type") == 26]  # fmt: skip
                    manifest["runs"][0]["trace"].remove(handoffs[-1])
                manifest["identity_sha256"] = MODULE.digest(manifest["identity"])
                path.write_text(json.dumps(manifest))
                self.assertEqual(
                    MODULE.validate_real_dut_campaign(copied / self.campaign.name)[
                        "status"
                    ],
                    "FAIL",
                )

    def test_host_time_patch_budget_and_physical_isolation_pass(self):
        self.assertEqual(MODULE.audit_host_time(PATH)["status"], "PASS")
        self.assertEqual(MODULE.patch_budget()["status"], "PASS")
        self.assertEqual(MODULE.physical_image_isolation()["status"], "PASS")
        path_audit = MODULE.protected_path_audit()
        self.assertIn(path_audit["status"], {"PASS", "UNAVAILABLE"})
        if path_audit["status"] == "PASS":
            self.assertEqual(path_audit["outside_allowed_paths"], [])
        else:
            self.assertIn("base revision", path_audit["reason"])

    def test_retained_real_dut_campaign_closes_roles_and_production_stages(self):
        result = MODULE.validate_real_dut_campaign(self.campaign)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["roles"], ["master", "slave"])
        self.assertEqual(result["fresh_runs"], 108)

    def test_retained_campaign_rejects_artifact_hash_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory) / "qemu-campaign"
            shutil.copytree(self.campaign.parent, copied)
            copied_log = copied / "pass/master/001/qemu.log"
            copied_log.write_bytes(copied_log.read_bytes() + b"changed\n")
            self.assertEqual(
                MODULE.validate_real_dut_campaign(copied / self.campaign.name)[
                    "status"
                ],
                "FAIL",
            )


if __name__ == "__main__":
    unittest.main()
