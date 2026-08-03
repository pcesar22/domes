import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Sequence
from unittest.mock import patch

from tools.doctor.doctor import (
    TOOL_SPECS,
    ToolSpec,
    build_capabilities,
    collect_report,
    detect_serial_devices,
    parse_version,
    probe_repository,
    probe_tool,
)


class FakeRunner:
    def __init__(self, outputs: dict[tuple[str, ...], tuple[int, str, str]]) -> None:
        self.outputs = outputs
        self.executables = {command[0] for command in outputs}

    def which(self, command: str) -> str | None:
        return f"/fake/{command}" if command in self.executables else None

    def run(
        self, command: Sequence[str], *, cwd: Path | None = None
    ) -> subprocess.CompletedProcess[str]:
        key = tuple(command)
        if command[0] == "bash":
            return subprocess.CompletedProcess(command, 0, "ESP-IDF v5.4.4\n", "")
        if key not in self.outputs:
            raise OSError(f"unexpected command: {key}")
        return_code, stdout, stderr = self.outputs[key]
        return subprocess.CompletedProcess(command, return_code, stdout, stderr)


def successful_outputs() -> dict[tuple[str, ...], tuple[int, str, str]]:
    values = {
        "git": "git version 2.50.1",
        "python": "Python 3.13.5",
        "cmake": "cmake version 4.0.3",
        "ninja": "1.13.1",
        "pkg_config": "2.5.1",
        "cxx": "c++ (GCC) 13.3.0",
        "rustc": "rustc 1.92.0 (hash date)",
        "cargo": "cargo 1.92.0 (hash date)",
        "flutter": "Flutter 3.44.8 • channel stable",
        "dart": "Dart SDK version: 3.12.2 (stable)",
        "protoc": "libprotoc 32.0",
        "protoc_plugin": "protoc_plugin 25.0.0",
        "pre_commit": "pre-commit 4.6.1",
        "go": "go version go1.25.0 linux/amd64",
        "shellcheck": "ShellCheck - shell script analysis tool\nversion: 0.10.0",
    }
    outputs = {
        spec.command: (0, values[spec.identifier] + "\n", "") for spec in TOOL_SPECS
    }
    outputs[("git", "submodule", "status", "--recursive")] = (0, "", "")
    return outputs


class DoctorTest(unittest.TestCase):
    def test_version_parser_and_pinned_mismatch(self) -> None:
        self.assertEqual(
            "3.44.8",
            parse_version("Flutter 3.44.8 • stable", r"Flutter ([^\s]+)"),
        )
        spec = ToolSpec(
            "flutter",
            ("flutter", "--version"),
            r"Flutter ([^\s]+)",
            "3.44.8",
            "pin it",
        )
        runner = FakeRunner({spec.command: (0, "Flutter 3.38.9\n", "")})

        result = probe_tool(spec, runner)

        self.assertEqual("failed", result["status"])
        self.assertEqual("3.38.9", result["version"])

    def test_missing_required_tool_is_unavailable(self) -> None:
        spec = ToolSpec(
            "shellcheck",
            ("shellcheck", "--version"),
            r"version: ([^\s]+)",
            None,
            "install",
        )

        result = probe_tool(spec, FakeRunner({}))

        self.assertEqual("unavailable", result["status"])
        self.assertTrue(result["required"])

    def test_detects_stable_one_and_two_pod_layouts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            dev_root = Path(directory)
            by_id = dev_root / "serial/by-id"
            by_id.mkdir(parents=True)
            (dev_root / "ttyUSB0").touch()
            (by_id / "usb-Silicon_Labs_CP2102N_A-if00-port0").touch()
            (by_id / "usb-Espressif_USB_JTAG_serial_debug_unit_A-if00").touch()

            one = detect_serial_devices(dev_root)

            self.assertEqual(1, one["detected_pod_count"])
            self.assertIn("/serial/by-id/", one["cp2102n"][0]["path"])

            (by_id / "usb-Silicon_Labs_CP2102N_B-if00-port0").touch()
            (by_id / "usb-Espressif_USB_JTAG_serial_debug_unit_B-if00").touch()

            two = detect_serial_devices(dev_root)

            self.assertEqual(2, two["detected_pod_count"])
            self.assertEqual(2, len(two["native_usb"]))

    def test_uninitialized_submodule_is_unavailable(self) -> None:
        command = ("git", "submodule", "status", "--recursive")
        runner = FakeRunner(
            {
                command: (
                    0,
                    "-c716db13070bfb7de03b33f5a6558528cbf8a249 "
                    "firmware/third_party/nanopb\n",
                    "",
                )
            }
        )

        result = probe_repository(runner, Path("/repo"))

        self.assertEqual("unavailable", result["status"])
        self.assertEqual("unavailable", result["submodules"][0]["status"])

    def test_optional_hardware_does_not_fail_software_doctor(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            export_script = root / "export.sh"
            export_script.touch()
            runner = FakeRunner(successful_outputs())

            with (
                patch("tools.doctor.doctor.platform.system", return_value="Linux"),
                patch("tools.doctor.doctor.platform.release", return_value="6.19"),
                patch("tools.doctor.doctor.platform.machine", return_value="x86_64"),
            ):
                report = collect_report(
                    runner=runner,
                    root=root,
                    dev_root=root / "missing-dev",
                    environment={"IDF_EXPORT_SCRIPT": str(export_script)},
                )

            self.assertEqual(0, report["summary"]["mandatory_failures"])
            self.assertEqual(0, report["summary"]["exit_code"])
            statuses = {item["id"]: item["status"] for item in report["capabilities"]}
            self.assertEqual("available", statuses["software_verification"])
            self.assertEqual("unavailable", statuses["single_device"])
            json.dumps(report)

    def test_capabilities_distinguish_not_applicable(self) -> None:
        report = {
            "repository": {"status": "available"},
            "host": {"native_linux": False, "architecture": "arm64"},
            "tools": [
                {"id": spec.identifier, "status": "available"} for spec in TOOL_SPECS
            ]
            + [{"id": "esp_idf", "status": "available"}],
            "devices": {"cp2102n": [], "native_usb": []},
            "bluetooth": {
                "status": "not_applicable",
                "detail": "not native Linux",
            },
        }

        statuses = {item["id"]: item["status"] for item in build_capabilities(report)}

        self.assertEqual("not_applicable", statuses["ble"])
        self.assertEqual("not_applicable", statuses["hardware_ci"])

    def test_capabilities_propagate_failed_software(self) -> None:
        report = {
            "repository": {"status": "available"},
            "host": {"native_linux": True, "architecture": "x86_64"},
            "tools": [
                {
                    "id": spec.identifier,
                    "status": "failed" if spec.identifier == "flutter" else "available",
                }
                for spec in TOOL_SPECS
            ]
            + [{"id": "esp_idf", "status": "available"}],
            "devices": {"cp2102n": [], "native_usb": []},
            "bluetooth": {"status": "unavailable", "detail": "no adapter"},
        }

        statuses = {item["id"]: item["status"] for item in build_capabilities(report)}

        self.assertEqual("failed", statuses["software_verification"])
        self.assertEqual("unavailable", statuses["ble"])


if __name__ == "__main__":
    unittest.main()
