import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

PATH = Path(__file__).with_name("fault_replay_acceptance.py")
SPEC = importlib.util.spec_from_file_location("fault_replay_acceptance", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def build_campaign_fixture(root: Path) -> Path:
    """Build a complete small campaign in temporary test storage."""
    repository_revision = MODULE.subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=MODULE.ROOT,
        check=True,
        text=True,
        stdout=MODULE.subprocess.PIPE,
    ).stdout.strip()
    current_hashes = {
        "qemu_patch_sha256": MODULE.hashlib.sha256(
            (
                MODULE.HERE / "qemu_link/patches/0001-domes-link-device.patch"
            ).read_bytes()
        ).hexdigest(),
        "campaign_runner_sha256": MODULE.hashlib.sha256(
            (MODULE.HERE / "fault_replay_qemu_campaign.py").read_bytes()
        ).hexdigest(),
        "acceptance_runner_sha256": MODULE.hashlib.sha256(
            (MODULE.HERE / "fault_replay_acceptance.py").read_bytes()
        ).hexdigest(),
    }
    artifact_contents = {
        "delivery-records.json": b"[]",
        "efuse-generation.log": b"",
        "fault-records.json": b"[]",
        "flash-generation.log": b"fixture\n",
        "qemu-device.log": b"fixture\n",
        "qemu.log": b"fixture\n",
        "trace.normalized.json": b"[]",
    }
    artifact_hashes = {
        name: MODULE.hashlib.sha256(content).hexdigest()
        for name, content in artifact_contents.items()
    }
    empty_digest = MODULE.digest([])
    stages = {
        name: True
        for name in (
            "mmio",
            "irq",
            "task",
            "callback",
            "ring",
            "semaphore",
            "dequeue",
            "service_dispatch",
            "tx_complete",
        )
    }
    matrix = []
    no_service_dispatch = {1, 5, 6, 7, 8, 9, 15, 17, 18, 19, 20}
    expected_failure = no_service_dispatch | {2, 3, 10}
    for fault_id, case in enumerate(MODULE.cases()):
        expected_status = "FAIL" if fault_id in expected_failure else "PASS"
        expected_dispatches = 0 if fault_id in no_service_dispatch else 1
        roles = []
        for role in ("master", "slave"):
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
                        "fault_records_sha256": empty_digest,
                        "delivery_records_sha256": empty_digest,
                        "trace_sha256": empty_digest,
                        "result_sha256": "2" * 64,
                        "result": {
                            "status": expected_status,
                            "failure_mask": (
                                "0x00000020"
                                if fault_id in expected_failure
                                else "0x00000000"
                            ),
                            "service_dispatches": expected_dispatches,
                            "trace_drops": 0,
                            "trace_discontinuities": 0,
                        },
                        "artifact_sha256": artifact_hashes,
                        "runtime": {"stages": stages},
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
                "fault_records_sha256": empty_digest,
                "delivery_records_sha256": empty_digest,
                "raw_trace_sha256": "a" * 64,
                "normalized_trace_sha256": empty_digest,
                "assertions": ["production_qemu_radio_submission"],
                "termination": "firmware_bounded_result",
                "expected_result": {
                    "status": expected_status,
                    "service_dispatches": expected_dispatches,
                },
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
        case = MODULE.cases()[0]
        valid = MODULE.execute(case)
        MODULE.validate_execution(case, valid)
        corrupted = json.loads(json.dumps(valid))
        corrupted["raw"]["unconsumed_events"] = 1
        with self.assertRaisesRegex(MODULE.AcceptanceFailure, "unconsumed"):
            MODULE.validate_execution(case, corrupted)

    def test_host_time_patch_budget_and_physical_isolation_pass(self):
        self.assertEqual(MODULE.audit_host_time(PATH)["status"], "PASS")
        self.assertEqual(MODULE.patch_budget()["status"], "PASS")
        self.assertEqual(MODULE.physical_image_isolation()["status"], "PASS")
        path_audit = MODULE.protected_path_audit()
        self.assertEqual(path_audit["status"], "PASS")
        self.assertEqual(path_audit["outside_allowed_paths"], [])

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

    def test_report_is_self_contained_and_json_round_trips(self):
        report = MODULE.run(self.campaign)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(
            report["dependency_acceptance_matrix"][0]["artifact"],
            "controller-private qemu_link.verify JSON",
        )
        self.assertEqual(
            report["role_rotation"]["two_firmware_state_machines"], "OUTSIDE_SCOPE"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text(json.dumps(report, sort_keys=True))
            self.assertEqual(
                json.loads(path.read_text())["report_sha256"], report["report_sha256"]
            )


if __name__ == "__main__":
    unittest.main()
