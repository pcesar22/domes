from __future__ import annotations

import importlib.util
import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

MODULE_PATH = Path(__file__).with_name("qemu_runtime.py")
SPEC = importlib.util.spec_from_file_location("qemu_runtime", MODULE_PATH)
assert SPEC and SPEC.loader
runtime = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runtime)


def manifest() -> dict:
    tasks = [
        {
            "id": "main",
            "name": "main",
            "stack_size": 4096,
            "priority": 1,
            "core_affinity": 0,
            "watchdog": False,
            "evidence_mask": 1,
            "presence": "required",
        },
        {
            "id": "worker",
            "name": "worker",
            "stack_size": 4096,
            "priority": 5,
            "core_affinity": 1,
            "watchdog": False,
            "evidence_mask": 2,
            "presence": "required",
        },
    ]
    return {
        "profile": "qemu_esp32s3",
        "spec_sha256": "1" * 64,
        "sdkconfig_sha256": "2" * 64,
        "supported_feature_mask": 0xE2,
        "ready_enabled_feature_mask": 0x02,
        "inputs": {"identity": "020000000001", "random_u32_count": 1},
        "tasks": tasks,
        "readiness_scenario": {
            "name": "service_ready_v1",
            "dwell_ms": 350,
            "touch_pad": 0,
        },
    }


def valid_log(**overrides: str) -> str:
    profile = manifest()
    fields = {
        "schema": "1",
        "status": "PASS",
        "profile": "qemu_esp32s3",
        "scenario": "service_ready_v1",
        "manifest_sha256": "0" * 64,
        "spec_sha256": "1" * 64,
        "sdkconfig_sha256": "2" * 64,
        "identity": "020000000001",
        "random_consumed": "1",
        "mode": "idle",
        "supported_mask": "0x000000e2",
        "enabled_mask": "0x00000002",
        "expected_tasks": "2",
        "present_tasks": "2",
        "expected_task_mask": "0x00000003",
        "started_task_mask": "0x00000003",
        "duplicate_task_mask": "0x00000000",
        "core0_task_mask": "0x00000001",
        "core1_task_mask": "0x00000002",
        "task_config_sha256": runtime._task_config_sha256(profile),
        "task_snapshot_sha256": "3" * 64,
        "tick_start": "68",
        "tick_end": "418",
        "tick_delta": "350",
        "cpu0_progress": "1",
        "cpu1_progress": "1",
        "adapter_init_mask": "0x0000001f",
        "adapter_progress_mask": "0x0000001f",
        "game_state": "READY",
        "game_hits": "1",
        "game_misses": "0",
        "game_pad_mask": "0x00000001",
        "nvs_roundtrip": "1",
        "trace_count": "20",
        "trace_drops": "0",
        "failure_mask": "0x00000000",
    }
    fields.update(overrides)
    marker = " ".join(f"{key}={value}" for key, value in fields.items())
    return f"ESP-ROM:esp32s3-20210327\nI (697) qemu_root: {runtime.READY_MARKER} {marker}\n"


def write_ci_report(root: Path, head: str = "a" * 40) -> Path:
    for relative in runtime.CI_REQUIRED_ARTIFACTS - {"runtime-report.json"}:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"fixture:{relative}\n", encoding="utf-8")
    signature = "b" * 64
    run_evidence = [
        {
            "index": index,
            "ready_signature": signature,
            "execution": {"qemu_returncode": 0},
            "result": {"status": "PASS", "failure_mask": 0},
        }
        for index in range(1, runtime.ACCEPTANCE_RUNS + 1)
    ]
    report_path = root / "runtime-report.json"
    report_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "status": "PASS",
                "qualification": "accepted",
                "runs": runtime.ACCEPTANCE_RUNS,
                "required_acceptance_runs": runtime.ACCEPTANCE_RUNS,
                "ready_signature": signature,
                "git": {"head": head, "dirty": False},
                "run_evidence": run_evidence,
            }
        ),
        encoding="utf-8",
    )
    runtime._write_artifact_manifest(root)
    return report_path


class RuntimeLogTests(unittest.TestCase):
    def test_accepts_complete_ready_marker(self) -> None:
        result = runtime.analyze_runtime_log(valid_log(), manifest(), "0" * 64)
        self.assertEqual(result["started_task_mask"], 3)
        self.assertEqual(result["tick_delta"], 350)

    def test_rejects_duplicate_marker(self) -> None:
        log = valid_log() + valid_log()
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "exactly one"):
            runtime.analyze_runtime_log(log, manifest(), "0" * 64)

    def test_rejects_missing_task_entry(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "started_task_mask"):
            runtime.analyze_runtime_log(
                valid_log(started_task_mask="0x00000001"), manifest(), "0" * 64
            )

    def test_rejects_duplicate_task_entry(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "duplicate_task_mask"):
            runtime.analyze_runtime_log(
                valid_log(duplicate_task_mask="0x00000002"), manifest(), "0" * 64
            )

    def test_rejects_unconsumed_game_workload(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "game_hits"):
            runtime.analyze_runtime_log(valid_log(game_hits="0"), manifest(), "0" * 64)

    def test_rejects_task_entry_on_both_cores(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "overlaps"):
            runtime.analyze_runtime_log(
                valid_log(core0_task_mask="0x00000003"), manifest(), "0" * 64
            )

    def test_rejects_task_entry_on_wrong_pinned_core(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "core affinity"):
            runtime.analyze_runtime_log(
                valid_log(core0_task_mask="0x00000002", core1_task_mask="0x00000001"),
                manifest(),
                "0" * 64,
            )

    def test_rejects_panic_before_pass_marker(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "panic or reset"):
            runtime.analyze_runtime_log(
                "Guru Meditation Error\n" + valid_log(), manifest(), "0" * 64
            )

    def test_signature_ignores_absolute_tick_origin_only(self) -> None:
        first = runtime.analyze_runtime_log(valid_log(), manifest(), "0" * 64)
        second = runtime.analyze_runtime_log(
            valid_log(tick_start="100", tick_end="450"), manifest(), "0" * 64
        )
        self.assertEqual(
            runtime.canonical_ready_signature(first),
            runtime.canonical_ready_signature(second),
        )


class ClosureTests(unittest.TestCase):
    def _description(self, sources: set[str]) -> dict:
        return {
            "build_component_info": {
                "main": {
                    "dir": str(runtime.MAIN_DIR),
                    "sources": [
                        str(runtime.MAIN_DIR / source) for source in sorted(sources)
                    ],
                }
            }
        }

    def test_accepts_exact_qemu_source_closure(self) -> None:
        actual = runtime.validate_source_closure(
            "qemu", self._description(set(runtime.QEMU_MAIN_SOURCES))
        )
        self.assertEqual(actual, runtime.QEMU_MAIN_SOURCES)

    def test_rejects_physical_source_in_qemu_closure(self) -> None:
        sources = set(runtime.QEMU_MAIN_SOURCES)
        sources.add("main.cpp")
        with self.assertRaisesRegex(
            runtime.RuntimeProfileError, r"extra=\['main.cpp'\]"
        ):
            runtime.validate_source_closure("qemu", self._description(sources))

    def test_accepts_exact_init_order_binding(self) -> None:
        profile = {"root": "composition/qemuRoot.cpp", "init_order": ["a", "b"]}
        source = (
            '\nadvanceInitStage(initOrder, "a");\nadvanceInitStage(initOrder, "b");\n'
        )
        self.assertEqual(
            runtime.validate_init_order_binding("qemu", profile, source), ["a", "b"]
        )

    def test_rejects_init_order_source_drift(self) -> None:
        profile = {"root": "composition/qemuRoot.cpp", "init_order": ["a", "b"]}
        source = 'advanceInitStage(initOrder, "b");'
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "binding differs"):
            runtime.validate_init_order_binding("qemu", profile, source)

    def test_rejects_disabled_vendor_symbol_in_qemu_elf(self) -> None:
        output = "42001000 T harmless\n42002000 T esp_wifi_init\n"
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "esp_wifi_init"):
            runtime.reject_qemu_forbidden_symbols(output)

    def test_rejects_disabled_service_symbol_in_qemu_elf(self) -> None:
        output = "42001000 T domes::EspNowService::init()\n"
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "EspNowService"):
            runtime.reject_qemu_forbidden_symbols(output)

    def test_rejects_unlisted_vendor_archive_contribution(self) -> None:
        map_text = """\
Archive member included to satisfy reference by file (symbol)

esp-idf/main/libmain.a(qemuRoot.cpp.obj)
                              (app_main)
esp-idf/esp_wifi/libesp_wifi.a(wifi_init.c.obj)
                              esp-idf/main/libmain.a(qemuRoot.cpp.obj) (esp_wifi_init)
Discarded input sections
"""
        with self.assertRaisesRegex(
            runtime.RuntimeProfileError, r"unapproved.*esp-idf/esp_wifi"
        ):
            runtime.validate_qemu_linked_component_closure(
                map_text, Path("build"), {"c_compiler": "unused"}
            )

    def test_ignores_available_but_unlinked_vendor_archive(self) -> None:
        map_text = """\
Archive member included to satisfy reference by file (symbol)

esp-idf/main/libmain.a(qemuRoot.cpp.obj)
                              (app_main)
Discarded input sections

LOAD esp-idf/esp_wifi/libesp_wifi.a
"""
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            archive = build_dir / "esp-idf/esp_wifi/libesp_wifi.a"
            archive.parent.mkdir(parents=True)
            archive.write_bytes(b"!<arch>\n")
            result = runtime.validate_qemu_linked_component_closure(
                map_text, build_dir, {"c_compiler": "unused"}
            )
        self.assertEqual(result["archive_origins"], ["esp-idf/main"])
        self.assertEqual(result["archive_member_count"], 1)

    def test_rejects_direct_object_disguised_with_archive_suffix(self) -> None:
        map_text = """\
Archive member included to satisfy reference by file (symbol)

esp-idf/main/libmain.a(qemuRoot.cpp.obj)
                              (app_main)
Discarded input sections

LOAD vendor/wifi_driver.a
"""
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            disguised_object = build_dir / "vendor/wifi_driver.a"
            disguised_object.parent.mkdir(parents=True)
            disguised_object.write_bytes(b"\x7fELF" + b"\0" * 12)
            with self.assertRaisesRegex(
                runtime.RuntimeProfileError, "without archive magic"
            ):
                runtime.validate_qemu_linked_component_closure(
                    map_text, build_dir, {"c_compiler": "unused"}
                )

    def test_rejects_unlisted_direct_object_contribution(self) -> None:
        map_text = """\
Archive member included to satisfy reference by file (symbol)

esp-idf/main/libmain.a(qemuRoot.cpp.obj)
                              (app_main)
Discarded input sections

LOAD vendor/wifi_driver.o
"""
        with self.assertRaisesRegex(
            runtime.RuntimeProfileError, "unexpected directly linked object"
        ):
            runtime.validate_qemu_linked_component_closure(
                map_text, Path("build"), {"c_compiler": "unused"}
            )

    def test_rejects_unclassified_direct_linker_input(self) -> None:
        map_text = """\
Archive member included to satisfy reference by file (symbol)

esp-idf/main/libmain.a(qemuRoot.cpp.obj)
                              (app_main)
Discarded input sections

LOAD vendor/wifi_driver.rel
"""
        with self.assertRaisesRegex(
            runtime.RuntimeProfileError, "unclassified linker LOAD input"
        ):
            runtime.validate_qemu_linked_component_closure(
                map_text, Path("build"), {"c_compiler": "unused"}
            )

    def test_rejects_map_without_archive_contribution_section(self) -> None:
        with self.assertRaisesRegex(runtime.RuntimeProfileError, "omits"):
            runtime.validate_qemu_linked_component_closure(
                "LOAD esp-idf/main/libmain.a\n",
                Path("build"),
                {"c_compiler": "unused"},
            )

    def test_rejects_wrong_flash_geometry(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "flasher_args.json").write_text(
                json.dumps({"flash_settings": {"flash_size": "4MB"}}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(runtime.RuntimeProfileError, "requires 8MB"):
                runtime._flash_size(path)


class QualificationTests(unittest.TestCase):
    def test_only_clean_fresh_100_run_campaign_is_accepted(self) -> None:
        self.assertEqual(
            runtime.runtime_qualification(runs=100, dirty=False, build_skipped=False),
            "accepted",
        )

    def test_skip_build_is_development_even_for_clean_100_run_campaign(self) -> None:
        self.assertEqual(
            runtime.runtime_qualification(runs=100, dirty=False, build_skipped=True),
            "development",
        )

    def test_retains_exact_fidelity_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            artifacts = root / "artifacts"
            build.mkdir()
            artifacts.mkdir()
            source = build / "domes-fidelity-manifest.json"
            source.write_text('{"schema_version":1}\n', encoding="utf-8")
            expected = runtime.sha256_file(source)
            retained = runtime._retain_fidelity_manifest(build, artifacts, expected)
            self.assertEqual(retained.read_bytes(), source.read_bytes())
            self.assertEqual(runtime.sha256_file(retained), expected)


class CiReportTests(unittest.TestCase):
    def test_accepts_complete_exact_commit_report_and_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = write_ci_report(root)
            result = runtime.verify_ci_report(report, "a" * 40)

        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["runs"], runtime.ACCEPTANCE_RUNS)
        self.assertEqual(result["artifact_count"], len(runtime.CI_REQUIRED_ARTIFACTS))

    def test_rejects_empty_artifact_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = write_ci_report(root)
            (root / "artifact-manifest.json").write_text(
                '{"schema_version":1,"files":{}}\n', encoding="utf-8"
            )
            with self.assertRaisesRegex(runtime.RuntimeProfileError, "contain files"):
                runtime.verify_ci_report(report, "a" * 40)

    def test_rejects_nonzero_qemu_returncode(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = write_ci_report(root)
            report = json.loads(report_path.read_text(encoding="utf-8"))
            report["run_evidence"][4]["execution"]["qemu_returncode"] = 7
            report_path.write_text(json.dumps(report), encoding="utf-8")
            runtime._write_artifact_manifest(root)
            with self.assertRaisesRegex(runtime.RuntimeProfileError, "run 5"):
                runtime.verify_ci_report(report_path, "a" * 40)

    def test_rejects_unmanifested_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = write_ci_report(root)
            (root / "unexpected.log").write_text("not manifested\n", encoding="utf-8")
            with self.assertRaisesRegex(
                runtime.RuntimeProfileError, "file set differs"
            ):
                runtime.verify_ci_report(report, "a" * 40)


class MainTests(unittest.TestCase):
    def test_single_build_validation_reports_without_manifest(self) -> None:
        stdout = io.StringIO()
        validated = {
            "profile": "physical",
            "manifest": {"large": "payload"},
            "manifest_sha256": "1" * 64,
        }
        with (
            mock.patch.object(
                runtime,
                "parse_args",
                return_value=SimpleNamespace(
                    command="validate-build",
                    build_dir=Path("build"),
                    profile="physical",
                ),
            ),
            mock.patch.object(runtime, "validate_build", return_value=validated),
            redirect_stdout(stdout),
        ):
            self.assertEqual(runtime.main([]), 0)
        report = json.loads(stdout.getvalue())
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["profile"], "physical")
        self.assertNotIn("manifest", report)

    def test_known_feasibility_error_is_reported_without_traceback(self) -> None:
        error = runtime.FeasibilityError("IDF_PATH is unset")
        stderr = io.StringIO()
        with (
            mock.patch.object(
                runtime, "parse_args", return_value=SimpleNamespace(command="run")
            ),
            mock.patch.object(runtime, "run_runtime", side_effect=error),
            redirect_stderr(stderr),
        ):
            self.assertEqual(runtime.main([]), 1)
        self.assertEqual(stderr.getvalue(), "FAIL: IDF_PATH is unset\n")


if __name__ == "__main__":
    unittest.main()
