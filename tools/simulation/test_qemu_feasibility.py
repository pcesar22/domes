#!/usr/bin/env python3
"""Unit tests for the bounded QEMU feasibility runner."""

from __future__ import annotations

import importlib.util
import os
import signal
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

MODULE_PATH = Path(__file__).with_name("qemu_feasibility.py")
SPEC = importlib.util.spec_from_file_location("qemu_feasibility", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
qemu_feasibility = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = qemu_feasibility
SPEC.loader.exec_module(qemu_feasibility)


def valid_log(*, tick_start: int = 5, status: str = "PASS", extra: str = "") -> str:
    return f"""ESP-ROM:esp32s3-20210327
{extra}
DOMES_QEMU_OBSERVATION schema=3 core0_wait_ticks=2 core1_wait_ticks=4 irq_wait_ticks=2 tick_start={tick_start} tick_end={tick_start + 2} tick_delta=2 irq_alarm=2000 irq_count_value=2001 irq_count_delta=1
DOMES_QEMU_RESULT schema=3 status={status} failure_mask=0 cores=2 controller_core=0 core0_task_core=0 core1_task_core=1 core0_runs=1 core1_runs=1 core0_phases=5 core1_phases=5 core0_blocks=1 core1_blocks=2 core0_wakeups=1 core1_wakeups=2 task_handoff_0_to_1=1 task_handoff_1_to_0=1 tick_progress=1 irq_source_core=0 irq_count=1 irq_drops=0 irq_sequence=1 irq_consumer_core=1 irq_consumer_wakeups=1 irq_to_core1_handoff=1 timer_cleanup=1 probe_state=complete
"""


class MarkerTests(unittest.TestCase):
    def test_valid_log_produces_normalized_signature(self) -> None:
        observation_a, result_a = qemu_feasibility.analyze_log(valid_log(tick_start=5))
        observation_b, result_b = qemu_feasibility.analyze_log(
            valid_log(tick_start=105)
        )
        structural_a, structural_signature_a, canonical_a, signature_a = (
            qemu_feasibility.canonical_signatures(observation_a, result_a)
        )
        structural_b, structural_signature_b, canonical_b, signature_b = (
            qemu_feasibility.canonical_signatures(observation_b, result_b)
        )

        self.assertEqual(signature_a, signature_b)
        self.assertEqual(canonical_a, canonical_b)
        self.assertEqual(structural_signature_a, structural_signature_b)
        self.assertEqual(structural_a, structural_b)
        self.assertEqual(canonical_a["tick_delta"], 2)
        self.assertNotIn("tick_start", canonical_a)
        self.assertNotIn("tick_end", canonical_a)
        self.assertEqual(canonical_a["irq_alarm"], 2000)
        self.assertEqual(canonical_a["irq_count_value"], 2001)

    def test_signature_retains_relative_scheduler_values(self) -> None:
        observation_a, result = qemu_feasibility.analyze_log(valid_log())
        observation_b = dict(observation_a)
        observation_b["core1_wait_ticks"] += 1

        _, _, _, signature_a = qemu_feasibility.canonical_signatures(
            observation_a, result
        )
        _, _, _, signature_b = qemu_feasibility.canonical_signatures(
            observation_b, result
        )

        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "mismatch"):
            qemu_feasibility.require_identical_signatures([signature_a, signature_b])

    def test_signature_retains_structural_values(self) -> None:
        observation, result_a = qemu_feasibility.analyze_log(valid_log())
        result_b = dict(result_a)
        result_b["timer_cleanup"] = 0

        _, signature_a, _, _ = qemu_feasibility.canonical_signatures(
            observation, result_a
        )
        _, signature_b, _, _ = qemu_feasibility.canonical_signatures(
            observation, result_b
        )

        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "mismatch"):
            qemu_feasibility.require_identical_signatures([signature_a, signature_b])

    def test_rejects_multiple_markers(self) -> None:
        log = valid_log() + valid_log().splitlines()[-1] + "\n"
        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "found 2"):
            qemu_feasibility.analyze_log(log)

    def test_rejects_target_fail(self) -> None:
        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "status"):
            qemu_feasibility.analyze_log(valid_log(status="FAIL"))

    def test_rejects_panic_and_reset_signatures(self) -> None:
        for marker in ("Guru Meditation Error", "SW_CPU_RESET", "Rebooting..."):
            with self.subTest(marker=marker):
                with self.assertRaisesRegex(
                    qemu_feasibility.FeasibilityError, "panic or reset"
                ):
                    qemu_feasibility.analyze_log(valid_log(extra=marker))

    def test_rejects_second_boot(self) -> None:
        with self.assertRaisesRegex(
            qemu_feasibility.FeasibilityError, "more than once"
        ):
            qemu_feasibility.analyze_log(valid_log(extra="ESP-ROM:esp32s3-20210327"))

    def test_rejects_unknown_schema_field(self) -> None:
        log = valid_log().replace(
            " irq_source_core=0", " unexpected=1 irq_source_core=0"
        )
        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "extra"):
            qemu_feasibility.analyze_log(log)

    def test_rejects_inconsistent_relative_observation(self) -> None:
        log = valid_log().replace(" tick_delta=2", " tick_delta=3", 1)
        with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "tick delta"):
            qemu_feasibility.analyze_log(log)


class ProcessTests(unittest.TestCase):
    def test_execute_rejects_timeout(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "qemu.log"
            command = [sys.executable, "-c", "import time; time.sleep(10)"]
            with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "timed out"):
                qemu_feasibility.execute_probe(command, log, 0.05)
            self.assertTrue(log.is_file())

    def test_execute_rejects_process_exit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "qemu.log"
            command = [sys.executable, "-c", "print('exited')"]
            with self.assertRaisesRegex(
                qemu_feasibility.FeasibilityError, "exited before"
            ):
                qemu_feasibility.execute_probe(command, log, 1.0)
            self.assertIn("exited", log.read_text(encoding="utf-8"))

    def test_interrupt_handler_terminates_registered_qemu_group(self) -> None:
        process = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(60)"],
            start_new_session=True,
        )
        qemu_feasibility._register_qemu_process(process)
        try:
            with self.assertRaisesRegex(
                qemu_feasibility.FeasibilityError, "interrupted by SIGTERM"
            ):
                qemu_feasibility._handle_runner_signal(signal.SIGTERM, None)
            process.wait(timeout=2.0)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=2.0)
            qemu_feasibility._unregister_qemu_process(process)
        self.assertIsNotNone(process.returncode)

    def test_terminate_handles_process_lookup_race(self) -> None:
        process = mock.Mock()
        process.pid = 12345
        process.poll.return_value = None
        process.wait.return_value = 0
        process.returncode = 0
        with mock.patch.object(os, "killpg", side_effect=ProcessLookupError):
            returncode, action = qemu_feasibility._terminate_process(process)

        self.assertEqual(returncode, 0)
        self.assertEqual(action, "process_exit_race")
        process.wait.assert_called_once_with(timeout=2.0)


class ReportTests(unittest.TestCase):
    def test_observation_ranges_retain_raw_min_and_max(self) -> None:
        runs = [
            {"observation": {"tick_start": 5, "tick_end": 7}},
            {"observation": {"tick_start": 6, "tick_end": 8}},
        ]

        self.assertEqual(
            qemu_feasibility._observation_ranges(runs),
            {
                "tick_end": {"min": 7, "max": 8},
                "tick_start": {"min": 5, "max": 6},
            },
        )

    def test_identity_revalidation_rejects_source_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdkconfig = root / "sdkconfig"
            elf = root / "probe.elf"
            retained_sdkconfig = root / "retained-sdkconfig"
            retained_elf = root / "retained.elf"
            sdkconfig.write_text("config", encoding="utf-8")
            retained_sdkconfig.write_text("config", encoding="utf-8")
            elf.write_bytes(b"elf")
            retained_elf.write_bytes(b"elf")
            repository = {"commit": "abc", "relevant_worktree_status": []}
            build_hashes = {
                "sdkconfig": qemu_feasibility.sha256_file(sdkconfig),
                "app_elf": qemu_feasibility.sha256_file(elf),
            }
            with (
                mock.patch.object(
                    qemu_feasibility, "_git_state", return_value=repository
                ),
                mock.patch.object(
                    qemu_feasibility,
                    "_implementation_hashes",
                    return_value={"runner": "changed"},
                ),
                mock.patch.object(
                    qemu_feasibility,
                    "_build_output_hashes",
                    return_value=build_hashes,
                ),
            ):
                with self.assertRaisesRegex(
                    qemu_feasibility.FeasibilityError, "source hashes changed"
                ):
                    qemu_feasibility._require_identity_unchanged(
                        repository_before=repository,
                        sources_before={"runner": "original"},
                        build_before=build_hashes,
                        build_dir=root,
                        sdkconfig=sdkconfig,
                        elf=elf,
                        retained_sdkconfig=retained_sdkconfig,
                        retained_elf=retained_elf,
                        toolchain_before=mock.sentinel.toolchain,
                        require_gdb=True,
                    )

    def test_identity_revalidation_rejects_build_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdkconfig = root / "sdkconfig"
            elf = root / "probe.elf"
            retained_sdkconfig = root / "retained-sdkconfig"
            retained_elf = root / "retained.elf"
            for path, contents in (
                (sdkconfig, b"config"),
                (retained_sdkconfig, b"config"),
                (elf, b"elf"),
                (retained_elf, b"elf"),
            ):
                path.write_bytes(contents)
            repository = {"commit": "abc", "relevant_worktree_status": []}
            original = {"sdkconfig": "original", "app_elf": "original"}
            changed = {"sdkconfig": "changed", "app_elf": "original"}
            with (
                mock.patch.object(
                    qemu_feasibility, "_git_state", return_value=repository
                ),
                mock.patch.object(
                    qemu_feasibility,
                    "_implementation_hashes",
                    return_value={"runner": "same"},
                ),
                mock.patch.object(
                    qemu_feasibility,
                    "_build_output_hashes",
                    return_value=changed,
                ),
            ):
                with self.assertRaisesRegex(
                    qemu_feasibility.FeasibilityError, "build outputs changed"
                ):
                    qemu_feasibility._require_identity_unchanged(
                        repository_before=repository,
                        sources_before={"runner": "same"},
                        build_before=original,
                        build_dir=root,
                        sdkconfig=sdkconfig,
                        elf=elf,
                        retained_sdkconfig=retained_sdkconfig,
                        retained_elf=retained_elf,
                        toolchain_before=mock.sentinel.toolchain,
                        require_gdb=True,
                    )


class AcceptanceTests(unittest.TestCase):
    CLEAN_REPOSITORY = {"commit": "abc", "relevant_worktree_status": []}

    @staticmethod
    def _args(
        *,
        runs: int = qemu_feasibility.ACCEPTANCE_RUNS,
        build_only: bool = False,
        allow_dirty: bool = False,
    ) -> SimpleNamespace:
        return SimpleNamespace(
            runs=runs, build_only=build_only, allow_dirty=allow_dirty
        )

    def test_only_clean_exact_100_run_execution_is_eligible(self) -> None:
        result = qemu_feasibility.acceptance_eligibility(
            self._args(), self.CLEAN_REPOSITORY
        )

        self.assertTrue(result["eligible"])
        self.assertEqual(result["status"], "PENDING")
        self.assertEqual(result["reasons"], [])

    def test_build_only_is_not_acceptance_eligible(self) -> None:
        result = qemu_feasibility.acceptance_eligibility(
            self._args(build_only=True), self.CLEAN_REPOSITORY
        )

        self.assertFalse(result["eligible"])
        self.assertIn("build-only", result["reasons"][0])

    def test_dirty_execution_is_not_acceptance_eligible(self) -> None:
        dirty = {"commit": "abc", "relevant_worktree_status": [" M runner.py"]}
        result = qemu_feasibility.acceptance_eligibility(
            self._args(allow_dirty=True), dirty
        )

        self.assertFalse(result["eligible"])
        self.assertIn("--allow-dirty", " ".join(result["reasons"]))
        self.assertIn("not committed", " ".join(result["reasons"]))

    def test_one_and_99_runs_are_not_acceptance_eligible(self) -> None:
        for runs in (1, 99):
            with self.subTest(runs=runs):
                result = qemu_feasibility.acceptance_eligibility(
                    self._args(runs=runs), self.CLEAN_REPOSITORY
                )
                self.assertFalse(result["eligible"])
                self.assertIn(f"requested {runs}", " ".join(result["reasons"]))


class PathTests(unittest.TestCase):
    @staticmethod
    def _args(build_dir: Path, artifact_dir: Path) -> SimpleNamespace:
        return SimpleNamespace(build_dir=build_dir, artifact_dir=artifact_dir)

    def test_rejects_equal_build_and_artifact_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "same"
            with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "disjoint"):
                qemu_feasibility._session_paths(self._args(path, path))

    def test_rejects_artifacts_inside_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            artifacts = build / "evidence"
            with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "disjoint"):
                qemu_feasibility._session_paths(self._args(build, artifacts))

    def test_rejects_build_inside_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory) / "evidence"
            build = artifacts / "build"
            with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "disjoint"):
                qemu_feasibility._session_paths(self._args(build, artifacts))


class CommandTests(unittest.TestCase):
    def test_run_command_has_fixed_deterministic_controls(self) -> None:
        command = qemu_feasibility.build_qemu_command(
            Path("/qemu"), Path("/flash.bin"), Path("/efuse.bin")
        )

        self.assertEqual(
            command[command.index("-icount") + 1], "shift=3,align=off,sleep=off"
        )
        self.assertEqual(command[command.index("-accel") + 1], "tcg,thread=single")
        self.assertEqual(
            command[command.index("-rtc") + 1],
            "base=2026-01-01T00:00:00,clock=vm",
        )
        self.assertEqual(command[command.index("-seed") + 1], "1")
        self.assertEqual(command[command.index("-nic") + 1], "none")
        self.assertIn("-nographic", command)
        self.assertEqual(command[command.index("-serial") + 1], "mon:stdio")
        self.assertIn("-snapshot", command)
        self.assertIn("-no-user-config", command)
        self.assertIn("-no-reboot", command)
        self.assertNotIn("shift=auto", command)

    def test_debug_command_starts_paused_with_hmp_and_gdb(self) -> None:
        command = qemu_feasibility.build_qemu_command(
            Path("/qemu"),
            Path("/flash.bin"),
            Path("/efuse.bin"),
            gdb_port=4321,
            monitor_socket=Path("/tmp/monitor.sock"),
        )

        self.assertIn("-S", command)
        self.assertEqual(command[command.index("-gdb") + 1], "tcp:127.0.0.1:4321")
        self.assertEqual(
            command[command.index("-monitor") + 1],
            "unix:/tmp/monitor.sock,server=on,wait=off",
        )
        self.assertEqual(command[command.index("-serial") + 1], "none")

    def test_debug_endpoints_are_all_or_nothing(self) -> None:
        with self.assertRaises(ValueError):
            qemu_feasibility.build_qemu_command(
                Path("/qemu"), Path("/flash.bin"), Path("/efuse.bin"), gdb_port=1
            )


class PinTests(unittest.TestCase):
    def test_idf_pin_is_exact(self) -> None:
        self.assertEqual(
            qemu_feasibility.validate_idf_version("ESP-IDF v5.4.4\n"),
            "ESP-IDF v5.4.4",
        )
        with self.assertRaises(qemu_feasibility.FeasibilityError):
            qemu_feasibility.validate_idf_version("ESP-IDF v5.4.5")

    def test_qemu_pin_is_exact(self) -> None:
        output = qemu_feasibility.EXPECTED_QEMU_VERSION + "\nCopyright"
        self.assertEqual(
            qemu_feasibility.validate_qemu_version(output),
            qemu_feasibility.EXPECTED_QEMU_VERSION,
        )
        with self.assertRaises(qemu_feasibility.FeasibilityError):
            qemu_feasibility.validate_qemu_version("QEMU emulator version 9.2.2")

    def test_qemu_discovery_falls_back_to_pinned_on_request_package(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = (
                root
                / "tools"
                / "qemu-xtensa"
                / qemu_feasibility.EXPECTED_QEMU_PACKAGE
                / "qemu"
                / "bin"
                / "qemu-system-xtensa"
            )
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"qemu")
            executable.chmod(0o755)
            with (
                mock.patch.object(qemu_feasibility.shutil, "which", return_value=None),
                mock.patch.dict(os.environ, {"IDF_TOOLS_PATH": str(root)}),
            ):
                self.assertEqual(
                    qemu_feasibility._resolve_qemu_executable(), executable.resolve()
                )

    def test_toolchain_identity_ignores_randomized_ldd_load_addresses(self) -> None:
        toolchain = qemu_feasibility.Toolchain(
            idf_path=Path("/idf"),
            idf_version="idf",
            idf_revision="revision",
            python=Path("/python"),
            compiler=Path("/compiler"),
            compiler_version="compiler",
            compiler_sha256="compiler-sha",
            compiler_archive=Path("/compiler.tar.xz"),
            compiler_archive_sha256="compiler-archive-sha",
            qemu=Path("/qemu"),
            qemu_version="qemu",
            qemu_sha256="qemu-sha",
            qemu_archive=Path("/qemu.tar.xz"),
            qemu_archive_sha256="qemu-archive-sha",
            qemu_dynamic_dependencies="libc.so => /lib/libc.so (0x111)",
            libslirp=Path("/lib/libslirp.so.0"),
            libslirp_sha256="slirp-sha",
            gdb=Path("/gdb"),
            gdb_version="gdb",
        )
        relocated = replace(
            toolchain,
            qemu_dynamic_dependencies="libc.so => /lib/libc.so (0x222)",
        )

        self.assertEqual(
            qemu_feasibility._toolchain_identity(toolchain),
            qemu_feasibility._toolchain_identity(relocated),
        )


class ConfigTests(unittest.TestCase):
    BASE_CONFIG = """CONFIG_IDF_TARGET=\"esp32s3\"
CONFIG_APP_REPRODUCIBLE_BUILD=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_NUMBER_OF_CORES=2
"""

    def test_accepts_production_aligned_probe_config(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text(self.BASE_CONFIG, encoding="utf-8")

            assertions = qemu_feasibility.validate_sdkconfig(path)

        self.assertEqual(assertions["CONFIG_FREERTOS_HZ"], "1000")
        self.assertEqual(assertions["CONFIG_FREERTOS_SMP"], "not-set")

    def test_rejects_amazon_smp_kernel(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text(
                self.BASE_CONFIG + "CONFIG_FREERTOS_SMP=y\n", encoding="utf-8"
            )

            with self.assertRaisesRegex(qemu_feasibility.FeasibilityError, "SMP"):
                qemu_feasibility.validate_sdkconfig(path)


if __name__ == "__main__":
    unittest.main()
