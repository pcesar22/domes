import json
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

import hardware_broker as broker
import hardware_client


class HardwareBrokerTest(unittest.TestCase):
    def capability(self, root: Path, operations: list[str] = ["info"]):
        workspace, evidence = root / "issue-101", root / "evidence"
        workspace.mkdir(parents=True)
        evidence.mkdir(parents=True)
        snapshot = {
            "link": "/dev/serial/by-id/usb-Silicon_Labs_CP2102N_5edf3f45576def11a245cea7c169b110-if00-port0",
            "target": "/dev/ttyUSB0",
            "rdev": 1,
            "vendor": "10c4",
            "model": "ea60",
            "serial": "5edf3f45576def11a245cea7c169b110",
        }
        with mock.patch.object(broker, "snapshot_port", return_value=snapshot):
            return broker.create_capability(
                root / "cap",
                issue=101,
                spec_revision="a" * 40,
                pr_head="b" * 40,
                workspace=workspace,
                evidence=evidence,
                ports=[snapshot["link"]],
                operations=operations,
                boards=[0],
            )

    def request(self, cap, **extra):
        return {
            "token": cap.token,
            "issue": 101,
            "spec_revision": "a" * 40,
            "pr_head": "b" * 40,
            "operation": "info",
            **extra,
        }

    def test_101_contract_is_ticket_bound_and_no_device_path_is_exposed(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory), ["info", "trace-status"])
            self.assertNotIn("/dev", str(cap.document()))
            self.assertEqual([0], cap.document()["boards"])
            self.assertEqual(
                ("info", None), broker.validate_request(cap, self.request(cap))
            )

    def test_board_alias_is_capability_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory))
            with self.assertRaisesRegex(broker.BrokerError, "board must"):
                broker._verified_port(cap, 1)
            with self.assertRaisesRegex(broker.BrokerError, "board must"):
                broker._verified_port(cap, True)

    def test_client_queue_is_writable_and_exposes_no_device_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            _request_id, queued = hardware_client.submit(
                root / "cap", {"operation": "info", "board": 0}
            )
            self.assertTrue(queued.is_file())
            self.assertNotIn("/dev", queued.read_text(encoding="utf-8"))

    def test_no_explicit_field_cannot_escalate_and_disallowed_command_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory))
            with self.assertRaisesRegex(broker.BrokerError, "allowlisted"):
                broker.validate_request(cap, self.request(cap, operation="flash"))
            with self.assertRaisesRegex(broker.BrokerError, "unauthenticated"):
                broker.validate_request(cap, self.request(cap, token="bad"))
            flash_cap = self.capability(Path(directory) / "flash", ["flash"])
            with self.assertRaisesRegex(broker.BrokerError, "does not accept"):
                broker.validate_request(
                    flash_cap,
                    self.request(flash_cap, operation="flash", path="build"),
                )

    def test_trace_acceptance_flash_is_a_distinct_ticket_operation(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory), ["flash", "flash-trace-acceptance"])
            self.assertEqual(
                ("flash-trace-acceptance", None),
                broker.validate_request(
                    cap,
                    self.request(cap, operation="flash-trace-acceptance", board=0),
                ),
            )
            ordinary = self.capability(Path(directory) / "ordinary", ["flash"])
            with self.assertRaisesRegex(broker.BrokerError, "allowlisted"):
                broker.validate_request(
                    ordinary,
                    self.request(ordinary, operation="flash-trace-acceptance", board=0),
                )

    def test_path_escape_and_wrong_ticket_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory), ["artifact-hash"])
            with self.assertRaisesRegex(broker.BrokerError, "escapes"):
                broker.validate_request(
                    cap,
                    self.request(cap, operation="artifact-hash", path="/etc/passwd"),
                )
            with self.assertRaisesRegex(broker.BrokerError, "bound"):
                broker.validate_request(cap, self.request(cap, issue=102))

    def test_lease_serializes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "lease"
            with broker.DeviceLease(path):
                with self.assertRaisesRegex(broker.BrokerError, "lease is held"):
                    with broker.DeviceLease(path):
                        pass

    def test_client_round_trip_error_timeout_and_atomic_request(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            # A broker error is a result, not a leaked traceback or an arbitrary argv.
            response: dict[str, object] = {}

            def client() -> None:
                response.update(
                    hardware_client.request(
                        root / "cap", {"operation": "info", "board": 0}, 2
                    )
                )

            thread = threading.Thread(target=client)
            thread.start()
            time.sleep(0.05)
            broker.serve_queue(
                root / "cap",
                broker.load_private_capability(cap.private_document()),
                once=True,
            )
            thread.join(2)
            answer = response
            self.assertIn("error", answer)
            self.assertFalse(list((root / "cap" / "requests").glob("*.tmp")))
            with self.assertRaises(TimeoutError):
                hardware_client.request(
                    root / "cap", {"operation": "info", "board": 0}, 0.01
                )

    def test_failed_bound_request_retains_operation_and_artifact_head(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root, ["artifact-hash"])
            hardware_client.submit(root / "cap", {"operation": "artifact-hash"})
            with mock.patch.object(broker, "_workspace_head", return_value="f" * 40):
                broker.serve_queue(
                    root / "cap",
                    broker.load_private_capability(cap.private_document()),
                    once=True,
                )
            event = json.loads(
                (root / "evidence" / "broker-manifest.jsonl").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual("artifact-hash", event["operation"])
            self.assertEqual("f" * 40, event["artifact_head"])
            self.assertEqual(1, event["returncode"])
            self.assertIn("requires an evidence file", event["error"])

    def test_malformed_request_is_retained_as_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            queued = root / "cap" / "requests" / "request-malformed.json"
            queued.write_text("[]\n", encoding="utf-8")
            with mock.patch.object(broker, "_workspace_head", return_value="f" * 40):
                broker.serve_queue(
                    root / "cap",
                    broker.load_private_capability(cap.private_document()),
                    once=True,
                )
            event = json.loads(
                (root / "evidence" / "broker-manifest.jsonl").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual("invalid", event["operation"])
            self.assertEqual("f" * 40, event["artifact_head"])
            self.assertEqual(1, event["returncode"])

    def test_ota_version_comes_from_image_info_not_a_literal(self):
        with mock.patch.object(
            broker.subprocess,
            "run",
            return_value=mock.Mock(
                returncode=0, stdout="App version: v1.2.3\n", stderr=""
            ),
        ):
            self.assertEqual(
                "v1.2.3", broker._ota_version(Path("image.bin"), "esptool.py")
            )

    def test_candidate_workspace_is_never_used_as_a_host_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            candidate = workspace / "tools/domes-cli/target/debug/domes-cli"
            candidate.parent.mkdir(parents=True)
            candidate.write_text("malicious", encoding="utf-8")
            candidate.chmod(0o700)
            trusted_file = workspace / "trusted"
            trusted_file.write_text("trusted", encoding="utf-8")
            cap = broker.Capability(
                1,
                "a",
                "b",
                workspace,
                workspace,
                ("info",),
                (0,),
                "token",
                (),
                {
                    "domes-cli": {
                        "path": str(trusted_file),
                        "sha256": __import__("hashlib").sha256(b"trusted").hexdigest(),
                    }
                },
            )
            trusted = broker._cli_path(cap)
            self.assertNotEqual(str(candidate), trusted)
            self.assertEqual(str(trusted_file), trusted)

    def test_staging_copies_candidate_bytes_before_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "candidate.bin"
            source.write_bytes(b"first")
            cap = broker.Capability(
                1, "a", "b", root, root / "evidence", ("ota",), (0,), "token"
            )
            staged, digest = broker._stage_input(cap, source)
            source.write_bytes(b"changed")
            self.assertEqual(b"first", staged.read_bytes())
            self.assertEqual(__import__("hashlib").sha256(b"first").hexdigest(), digest)

    def test_controller_owned_trace_acceptance_defaults_are_finite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "firmware" / "domes"
            project.mkdir(parents=True)
            (project / "sdkconfig.defaults").write_text(
                'CONFIG_IDF_TARGET="esp32s3"\n'
                "CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE=y\n",
                encoding="utf-8",
            )
            cap = broker.Capability(
                1, "a", "b", root, root / "evidence", ("flash",), (0,), "token"
            )
            cap.evidence.mkdir()
            default = broker._write_profile_defaults(
                cap, project, "head-default", "default"
            ).read_text(encoding="utf-8")
            acceptance = broker._write_profile_defaults(
                cap, project, "head-probe", "trace-acceptance"
            ).read_text(encoding="utf-8")
            self.assertIn("# CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE is not set", default)
            self.assertNotIn("CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE=y", default)
            self.assertEqual(
                1, acceptance.count("CONFIG_DOMES_TRACE_ACCEPTANCE_PROBE=y")
            )
            self.assertIn("CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL=y", acceptance)
            self.assertIn("# CONFIG_DOMES_RUNTIME_PROFILE_QEMU is not set", acceptance)

    def test_incomplete_trusted_build_state_is_retryable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "evidence"
            evidence.mkdir()
            cap = broker.Capability(
                1, "a", "b", root, evidence, ("flash",), (0,), "token"
            )
            source = evidence / "source-head-default"
            build = evidence / "build-head-default"
            sdkconfig = evidence / "sdkconfig-head-default"
            defaults = evidence / "sdkconfig-defaults-head-default"
            source.mkdir()
            build.mkdir()
            sdkconfig.write_text("partial", encoding="utf-8")
            defaults.write_text("partial", encoding="utf-8")

            broker._discard_incomplete_trusted_build(
                cap, (source, build, sdkconfig, defaults)
            )

            self.assertFalse(source.exists())
            self.assertFalse(build.exists())
            self.assertFalse(sdkconfig.exists())
            self.assertFalse(defaults.exists())

    def test_compiler_temp_directory_is_reusable_between_profiles(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "evidence"
            evidence.mkdir()
            cap = broker.Capability(
                1, "a", "b", root, evidence, ("flash",), (0,), "token"
            )

            first = broker._compiler_temp_directory(cap)
            second = broker._compiler_temp_directory(cap)

            self.assertEqual(first, second)
            self.assertTrue(first.is_dir())
            self.assertEqual(0o700, first.stat().st_mode & 0o777)

    def test_flash_accepts_only_standard_domes_application_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "source" / "firmware" / "domes"
            project.mkdir(parents=True)
            evidence = root / "evidence"
            build = evidence / "build"
            build.mkdir(parents=True)
            for relative in (
                "bootloader/bootloader.bin",
                "partition_table/partition-table.bin",
                "domes.bin",
                "ota_data_initial.bin",
            ):
                image = build / relative
                image.parent.mkdir(parents=True, exist_ok=True)
                image.write_bytes(b"image")
            description = {
                "git_revision": "v5.4.4",
                "project_path": str(project),
                "build_dir": str(build),
                "target": "esp32s3",
                "project_name": "domes",
                "app_bin": "domes.bin",
            }
            (build / "project_description.json").write_text(
                __import__("json").dumps(description), encoding="utf-8"
            )
            layout = {
                "0x0": "bootloader/bootloader.bin",
                "0x8000": "partition_table/partition-table.bin",
                "0x20000": "domes.bin",
                "0xf000": "ota_data_initial.bin",
            }
            (build / "flasher_args.json").write_text(
                __import__("json").dumps(
                    {
                        "flash_files": layout,
                        "flash_settings": {
                            "flash_mode": "dio",
                            "flash_freq": "80m",
                            "flash_size": "8MB",
                        },
                    }
                ),
                encoding="utf-8",
            )
            tool = root / "esptool.py"
            tool.write_text("tool", encoding="utf-8")
            cap = broker.Capability(
                1,
                "a",
                "b",
                root,
                evidence,
                ("flash",),
                (0,),
                "token",
                (),
                {
                    "esptool": {
                        "path": str(tool),
                        "sha256": __import__("hashlib").sha256(b"tool").hexdigest(),
                    }
                },
            )
            with mock.patch.object(
                broker, "_stage_input", side_effect=lambda _cap, image: (image, "x")
            ):
                argv, inputs = broker._flash_argv(cap, project, build, "/dev/fake")
            self.assertIn("0x20000", argv)
            self.assertNotIn("ota_data_initial.bin", argv)
            self.assertEqual(3, len(inputs))
            for offset, name in (("0x9000", "nvs.bin"), ("0x20000", "renamed.bin")):
                rejected = dict(layout)
                rejected[offset] = name if offset not in rejected else name
                (build / "flasher_args.json").write_text(
                    __import__("json").dumps(
                        {
                            "flash_files": rejected,
                            "flash_settings": {
                                "flash_mode": "dio",
                                "flash_freq": "80m",
                                "flash_size": "8MB",
                            },
                        }
                    ),
                    encoding="utf-8",
                )
                with self.assertRaisesRegex(broker.BrokerError, "standard DOMES"):
                    broker._flash_argv(cap, project, build, "/dev/fake")
