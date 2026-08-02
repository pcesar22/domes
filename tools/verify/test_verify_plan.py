import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.verify.verify_plan import (
    ALL_CHECKS,
    CI_JOB_BY_CHECK,
    build_plan,
    classify_path,
    git_changed_paths,
    render_plan,
    render_summary,
    summarize,
)

ROOT = Path(__file__).resolve().parents[2]


def selected(plan: dict) -> set[str]:
    return {check["id"] for check in plan["checks"] if check["selected"]}


class VerifyPlanTest(unittest.TestCase):
    def test_full_and_quick_preserve_gate_strength(self) -> None:
        full = build_plan(changed_paths=None, components=[], quick=False, base=None)
        quick = build_plan(changed_paths=None, components=[], quick=True, base=None)

        self.assertEqual(set(ALL_CHECKS), selected(full))
        self.assertEqual(set(ALL_CHECKS) - {"firmware"}, selected(quick))
        self.assertEqual("not_assessed", full["hardware"][0]["status"])

    def test_component_overrides_are_scoped(self) -> None:
        expected = {
            "firmware": {"host_firmware", "host_tooling", "firmware"},
            "cli": {"cli", "host_tooling"},
            "flutter": {"flutter", "host_tooling"},
            "docs": {"host_tooling"},
        }
        for component, checks in expected.items():
            with self.subTest(component=component):
                plan = build_plan(
                    changed_paths=None,
                    components=[component],
                    quick=False,
                    base=None,
                )
                self.assertEqual(checks, selected(plan))

    def test_representative_paths_select_matrix_rows(self) -> None:
        expected = {
            "docs/TESTING.md": {"host_tooling"},
            "firmware/domes/main/drivers/ledDriver.cpp": {
                "host_firmware",
                "host_tooling",
                "firmware",
            },
            "tools/domes-cli/src/main.rs": {"cli", "host_tooling"},
            "ios/domes_app/lib/presentation/screens/home_screen.dart": {
                "flutter",
                "host_tooling",
            },
            "tools/doctor/doctor.py": {"host_tooling"},
        }
        for path, checks in expected.items():
            with self.subTest(path=path):
                self.assertEqual(checks, classify_path(path)[0])

    def test_cross_component_paths_select_every_consumer(self) -> None:
        paths = (
            "firmware/common/proto/config.proto",
            "firmware/common/protocol/frameCodec.hpp",
            "firmware/domes/main/transport/bleOtaService.hpp",
            "tools/domes-cli/src/commands/ota.rs",
            "ios/domes_app/lib/data/transport/ble_transport.dart",
            ".github/workflows/firmware-ci.yml",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(set(ALL_CHECKS), classify_path(path)[0])

    def test_full_local_checks_map_to_software_ci_jobs(self) -> None:
        workflow = (ROOT / ".github/workflows/firmware-ci.yml").read_text(
            encoding="utf-8"
        )
        verify_script = (ROOT / "scripts/verify.sh").read_text(encoding="utf-8")
        self.assertEqual(set(ALL_CHECKS), set(CI_JOB_BY_CHECK))
        for identifier, job in CI_JOB_BY_CHECK.items():
            self.assertIn(f"  {job}:", workflow)
            self.assertIn(f"run_check {identifier} ", verify_script)

    def test_hardware_workflow_change_reports_physical_obligations(self) -> None:
        checks, hardware = classify_path(".github/workflows/firmware-hw-test.yml")
        self.assertEqual(set(ALL_CHECKS), checks)
        self.assertEqual({"multi_device", "physical_hardware"}, set(hardware))

    def test_protocol_and_ota_report_hardware_outstanding(self) -> None:
        plan = build_plan(
            changed_paths=["firmware/common/protocol/otaProtocol.hpp"],
            components=[],
            quick=False,
            base="main",
        )

        hardware = {item["id"]: item["status"] for item in plan["hardware"]}
        self.assertEqual("outstanding", hardware["protocol_transport"])
        self.assertEqual("outstanding", hardware["ota"])

    def test_unknown_path_fails_safe_to_full_gate(self) -> None:
        checks, _ = classify_path("new_top_level_file.xyz")
        self.assertEqual(set(ALL_CHECKS), checks)

    def test_git_changes_include_tracked_deleted_and_untracked_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", root], check=True)
            subprocess.run(
                ["git", "-C", root, "config", "user.name", "Test"], check=True
            )
            subprocess.run(
                ["git", "-C", root, "config", "user.email", "test@example.invalid"],
                check=True,
            )
            (root / "tracked.md").write_text("tracked\n", encoding="utf-8")
            subprocess.run(["git", "-C", root, "add", "tracked.md"], check=True)
            subprocess.run(["git", "-C", root, "commit", "-qm", "initial"], check=True)
            (root / "tracked.md").unlink()
            (root / "new.txt").write_text("new\n", encoding="utf-8")

            paths = git_changed_paths(root, "HEAD")

            self.assertEqual(["new.txt", "tracked.md"], paths)

    def test_summary_propagates_failure_and_preserves_skip_reasons(self) -> None:
        plan = build_plan(
            changed_paths=None,
            components=["docs"],
            quick=False,
            base=None,
        )
        results = {
            "host_tooling": {
                "status": "failed",
                "exit_code": 7,
                "duration_seconds": 3,
                "log": "/tmp/host_tooling.log",
            }
        }

        document = summarize(plan, results, "/tmp/artifacts")

        self.assertEqual(1, document["summary"]["exit_code"])
        self.assertEqual(1, document["summary"]["failed"])
        self.assertEqual(5, document["summary"]["skipped"])
        self.assertIn("component: docs", render_plan(plan))
        self.assertIn("1 failed", render_summary(document))
        json.dumps(document)

    def test_shell_dry_run_retains_plan_and_json_summary(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            summary_path = root / "summary.json"
            artifacts = root / "artifacts"

            process = subprocess.run(
                [
                    str(ROOT / "scripts/verify.sh"),
                    "--component",
                    "docs",
                    "--dry-run",
                    "--json-summary",
                    str(summary_path),
                    "--keep-artifacts",
                    str(artifacts),
                ],
                cwd=ROOT,
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(0, process.returncode, process.stderr)
            document = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(1, document["schema_version"])
            self.assertEqual(0, document["summary"]["exit_code"])
            retained = Path(document["artifacts"])
            self.assertTrue((retained / "plan.json").is_file())
            self.assertTrue((retained / "results.tsv").is_file())

    def test_shell_failure_retains_complete_check_log(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            fake_pre_commit = fake_bin / "pre-commit"
            fake_pre_commit.write_text(
                "#!/bin/sh\necho 'pre-commit 0.0.0'\n",
                encoding="utf-8",
            )
            fake_pre_commit.chmod(0o755)
            summary_path = root / "summary.json"
            env = os.environ.copy()
            env["PATH"] = f"{fake_bin}:{env['PATH']}"

            process = subprocess.run(
                [
                    str(ROOT / "scripts/verify.sh"),
                    "--component",
                    "docs",
                    "--json-summary",
                    str(summary_path),
                    "--keep-artifacts",
                    str(root / "artifacts"),
                ],
                cwd=ROOT,
                check=False,
                capture_output=True,
                text=True,
                env=env,
            )

            self.assertEqual(1, process.returncode)
            document = json.loads(summary_path.read_text(encoding="utf-8"))
            host_tooling = next(
                check for check in document["checks"] if check["id"] == "host_tooling"
            )
            self.assertEqual("failed", host_tooling["status"])
            log = Path(host_tooling["log"])
            self.assertTrue(log.is_file())
            self.assertIn("Expected pre-commit 4.6.1", log.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
