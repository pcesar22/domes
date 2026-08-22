import hashlib
import json
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

import hardware_broker as broker
import hardware_client
import serial_trace_proxy


class HardwareBrokerTest(unittest.TestCase):
    def setUp(self):
        # Unit-test evidence roots use /tmp, whose small tmpfs intentionally does
        # not satisfy the production host reserve. Quota behavior is exercised
        # explicitly by focused tests below.
        for name in ("MIN_HOST_FREE_BYTES", "MIN_HOST_FREE_INODES"):
            patcher = mock.patch.object(broker, name, 0)
            patcher.start()
            self.addCleanup(patcher.stop)

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
                base_head="b" * 40,
                allowed_surfaces=["**"],
                repository_url="https://github.com/pcesar22/domes.git",
                head_ref="codex/issue-101",
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

    def test_espnow_regression_is_explicit_and_status_parser_is_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory), ["espnow-regression"])
            self.assertEqual(
                ("espnow-regression", None),
                broker.validate_request(
                    cap,
                    self.request(cap, operation="espnow-regression"),
                ),
            )
        self.assertEqual(
            ("master", 1, 0),
            broker._espnow_status_fields("State: master\nPeers: 1\nTX fails: 0\n"),
        )
        self.assertEqual(
            ("disabled", 0, 0),
            broker._espnow_status_fields(
                "ESP-NOW Status:\n  State:      disabled\n  Channel:    1\n"
                "  Peers:      0\n  TX packets: 0\n  RX packets: 0\n"
                "  TX fails:   0\n"
            ),
        )
        with self.assertRaisesRegex(broker.BrokerError, "incomplete"):
            broker._espnow_status_fields("State: master\nPeers: 1\n")

    def test_espnow_regression_runs_fixed_two_board_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "evidence"
            evidence.mkdir()
            cap = broker.Capability(
                101,
                "a" * 40,
                "b" * 40,
                root,
                evidence,
                ("espnow-regression",),
                (0, 1),
                "token",
            )
            enabled = {0: False, 1: False}
            drill = {"active": False}
            commands = []

            def run(_cap, argv, _name, _timeout):
                board = int(argv[argv.index("--port") + 1][-1])
                command = argv[argv.index("--port") + 2 :]
                commands.append((board, command))
                if command[:2] == ["feature", "enable"]:
                    enabled[board] = True
                elif command[:2] == ["feature", "disable"]:
                    enabled[board] = False
                elif command[:3] == ["espnow", "sim-mode", "on"]:
                    drill["active"] = True
                if command == ["espnow", "status"]:
                    if enabled[board]:
                        state = "master" if board == 0 else "slave"
                        peers = 1 if all(enabled.values()) else 0
                    else:
                        state = "disabled"
                        peers = 1 if drill["active"] else 0
                    return 0, f"State: {state}\nPeers: {peers}\nTX fails: 0\n", ""
                if command[:2] == ["espnow", "bench"]:
                    return (
                        0,
                        "ESP-NOW Benchmark Results:\n"
                        "  Rounds:     100/100 completed (0 failed)\n"
                        "  Mean RTT:   1000 us (1.00 ms)\n",
                        "",
                    )
                return 0, "ok\n", ""

            def sleep(seconds):
                if seconds == 35:
                    enabled[0] = False
                    enabled[1] = False

            selected = (
                "b" * 40,
                "default",
                "c" * 64,
                {"source_head": "b" * 40},
            )
            with (
                mock.patch.object(broker, "_selected_flash", return_value=selected),
                mock.patch.object(broker, "_cli_path", return_value="/trusted/cli"),
                mock.patch.object(
                    broker,
                    "_verified_port",
                    side_effect=lambda _cap, board: f"/dev/fake{board}",
                ),
                mock.patch.object(
                    broker, "_resource_limited", side_effect=lambda _cap, argv: argv
                ),
                mock.patch.object(broker, "_run_with_bounded_logs", side_effect=run),
                mock.patch.object(broker.time, "sleep", side_effect=sleep),
            ):
                result = broker._execute_espnow_regression(cap, "b" * 40)
            summary = result["espnow_regression"]
            self.assertEqual(6, summary["benchmarks"])
            self.assertEqual("passed", summary["drill"])
            self.assertEqual(["disabled", "disabled"], summary["final_states"])
            self.assertEqual(
                [(0, ["trace", "stop"]), (1, ["trace", "stop"])],
                [command for command in commands if command[1] == ["trace", "stop"]][
                    -2:
                ],
            )
            first_trace_start = commands.index((0, ["trace", "start"]))
            simulated_peer_status = max(
                index
                for index, command in enumerate(commands[:first_trace_start])
                if command[1] == ["espnow", "status"]
            )
            self.assertLess(simulated_peer_status, first_trace_start)
            self.assertTrue(
                (evidence / f"espnow-regression-{'b' * 16}.jsonl").is_file()
            )

    def test_failed_trace_dump_does_not_require_output_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            failed = subprocess.CompletedProcess(["domes-cli"], 1)
            self.assertFalse(broker._trace_artifacts_ready(failed, output))
            succeeded = subprocess.CompletedProcess(["domes-cli"], 0)
            with self.assertRaisesRegex(broker.BrokerError, "no raw trace"):
                broker._trace_artifacts_ready(succeeded, output)
            (output / "trace.json.raw").write_bytes(b"trace")
            self.assertTrue(broker._trace_artifacts_ready(succeeded, output))

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

    def test_bounded_runner_cuts_off_fast_log_overflow_and_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory))
            with (
                mock.patch.object(broker, "MIN_HOST_FREE_BYTES", 0),
                mock.patch.object(broker, "MIN_HOST_FREE_INODES", 0),
                mock.patch.object(broker, "MAX_CANDIDATE_LOG_BYTES", 64),
                self.assertRaisesRegex(broker.BrokerError, "bounded log size"),
            ):
                broker._run_with_bounded_logs(
                    cap,
                    [sys.executable, "-c", "import os; os.write(1, b'x' * 4096)"],
                    "log-overflow",
                    5,
                )
            with (
                mock.patch.object(broker, "MIN_HOST_FREE_BYTES", 0),
                mock.patch.object(broker, "MIN_HOST_FREE_INODES", 0),
                self.assertRaisesRegex(broker.BrokerError, "wall-clock timeout"),
            ):
                broker._run_with_bounded_logs(
                    cap,
                    [sys.executable, "-c", "import time; time.sleep(5)"],
                    "timeout",
                    0.05,
                )

    def test_bounded_runner_accounts_for_open_unlinked_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            hidden = root / "evidence" / "hidden-output"
            script = (
                "import os,sys,time; "
                "fd=os.open(sys.argv[1], os.O_CREAT|os.O_RDWR, 0o600); "
                "os.unlink(sys.argv[1]); os.ftruncate(fd, 1048576); time.sleep(5)"
            )
            with (
                mock.patch.object(broker, "MIN_HOST_FREE_BYTES", 0),
                mock.patch.object(broker, "MIN_HOST_FREE_INODES", 0),
                mock.patch.object(broker, "MAX_CANDIDATE_DISK_GROWTH_BYTES", 4096),
                self.assertRaisesRegex(broker.BrokerError, "disk-growth limit"),
            ):
                broker._run_with_bounded_logs(
                    cap,
                    [sys.executable, "-c", script, str(hidden)],
                    "unlinked-output",
                    5,
                )

    def test_bounded_runner_enforces_cumulative_capability_quota(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory))
            (cap.evidence / "prior-evidence").write_bytes(b"x" * 4096)
            with (
                mock.patch.object(broker, "MAX_CAPABILITY_EVIDENCE_BYTES", 1024),
                self.assertRaisesRegex(broker.BrokerError, "cumulative evidence quota"),
            ):
                broker._run_with_bounded_logs(
                    cap,
                    [sys.executable, "-c", "print('must not run')"],
                    "cumulative-quota",
                    5,
                )

    def test_bounded_runner_final_scan_catches_fast_quota_overflow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            output = cap.evidence / "fast-output"
            script = "from pathlib import Path; import sys; Path(sys.argv[1]).write_bytes(b'x'*65536)"
            with (
                mock.patch.object(broker, "MAX_CAPABILITY_EVIDENCE_BYTES", 32768),
                mock.patch.object(broker, "MIN_HOST_FREE_BYTES", 0),
                mock.patch.object(broker, "MIN_HOST_FREE_INODES", 0),
                self.assertRaisesRegex(broker.BrokerError, "cumulative evidence quota"),
            ):
                broker._run_with_bounded_logs(
                    cap,
                    [sys.executable, "-c", script, str(output)],
                    "fast-quota-overflow",
                    5,
                )

    def test_directory_scan_tolerates_compiler_temp_file_disappearance(self):
        class VanishingEntry:
            path = "/tmp/vanished"

            @staticmethod
            def stat(*, follow_symlinks):
                self.assertFalse(follow_symlinks)
                raise FileNotFoundError("compiler removed temporary file")

        class Entries:
            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def __iter__(self):
                return iter((VanishingEntry(),))

        with mock.patch.object(broker.os, "scandir", return_value=Entries()):
            self.assertEqual(0, broker._directory_size(Path("/tmp"), 1024))

    def test_non_trace_hardware_command_uses_bounded_streaming_runner(self):
        with tempfile.TemporaryDirectory() as directory:
            cap = self.capability(Path(directory))
            request = self.request(cap, board=0)
            with (
                mock.patch.object(broker, "MIN_HOST_FREE_BYTES", 0),
                mock.patch.object(broker, "MIN_HOST_FREE_INODES", 0),
                mock.patch.object(broker, "_workspace_head", return_value="b" * 40),
                mock.patch.object(broker, "_verified_port", return_value="/dev/fake"),
                mock.patch.object(broker, "_cli_path", return_value="/trusted/cli"),
                mock.patch.object(
                    broker, "_resource_limited", side_effect=lambda _cap, argv: argv
                ),
                mock.patch.object(
                    broker,
                    "_run_with_bounded_logs",
                    return_value=(0, "healthy", ""),
                ) as bounded,
            ):
                result = broker.execute(cap, request)
            self.assertEqual("healthy", result["stdout"])
            self.assertIn("hardware-info-board-0", bounded.call_args.args)

    def test_candidate_sandboxes_have_no_unmonitored_tmpfs(self):
        source = Path(broker.__file__).read_text(encoding="utf-8")
        self.assertNotIn('"--tmpfs"', source)
        self.assertEqual(4, source.count("str(_compiler_temp_directory(cap))"))

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

    def test_queue_continues_after_bounded_process_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            first, _ = hardware_client.submit(
                root / "cap", {"operation": "info", "board": 0}
            )
            second, _ = hardware_client.submit(
                root / "cap", {"operation": "info", "board": 0}
            )
            timed_out_id, successful_id = sorted((first, second))
            success = {
                "returncode": 0,
                "artifact_head": "b" * 40,
                "stdout": "healthy",
                "stderr": "",
            }
            with (
                mock.patch.object(
                    broker,
                    "execute",
                    side_effect=[
                        broker.BrokerError(
                            "candidate process exceeded wall-clock timeout"
                        ),
                        success,
                    ],
                ),
                mock.patch.object(broker, "_workspace_head", return_value="b" * 40),
            ):
                broker.serve_queue(
                    root / "cap",
                    broker.load_private_capability(cap.private_document()),
                    once=True,
                )
            timed_out_result = json.loads(
                (root / "cap" / "results" / f"result-{timed_out_id}.json").read_text()
            )
            successful_result = json.loads(
                (root / "cap" / "results" / f"result-{successful_id}.json").read_text()
            )
            self.assertIn("wall-clock timeout", timed_out_result["error"])
            self.assertEqual("healthy", successful_result["stdout"])

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
            (root / "evidence").mkdir()
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
                "project_path": "/src/firmware/domes",
                "build_dir": "/out/build",
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
            self.assertIn("0xf000", argv)
            self.assertEqual(4, len(inputs))
            self.assertIn("ota_data_initial.bin", {item["artifact"] for item in inputs})
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

    def test_trace_dump_selects_last_successful_flash_for_requested_board(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "evidence"
            evidence.mkdir()
            head = "a" * 40
            cap = broker.Capability(
                1, "spec", head, root, evidence, ("trace-dump",), (0, 1), "token"
            )
            events = []
            previous = ""
            for board, operation, success in (
                (0, "flash", True),
                (1, "flash-trace-acceptance", True),
            ):
                event = {
                    "issue": 1,
                    "spec_revision": "spec",
                    "pr_head": head,
                    "board": board,
                    "operation": operation,
                    "returncode": 0 if success else 1,
                    "artifact_head": head,
                    "build_provenance": {
                        "source_head": head,
                        "build_profile": "trace-acceptance" if board else "default",
                    },
                    "inputs": [{"artifact": "domes.bin", "sha256": "b" * 64}],
                    "previous_event_sha256": previous,
                }
                event["event_sha256"] = (
                    __import__("hashlib")
                    .sha256(
                        __import__("json")
                        .dumps(event, sort_keys=True, separators=(",", ":"))
                        .encode()
                    )
                    .hexdigest()
                )
                previous = event["event_sha256"]
                events.append(event)
            (evidence / "broker-manifest.jsonl").write_text(
                "\n".join(__import__("json").dumps(event) for event in events) + "\n",
                encoding="utf-8",
            )
            selected = broker._selected_flash(cap, 1)
            self.assertEqual((head, "trace-acceptance"), selected[:2])
            self.assertEqual("b" * 64, selected[2])
            self.assertEqual("default", broker._selected_flash(cap, 0)[1])

            failed = {
                "issue": 1,
                "spec_revision": "spec",
                "pr_head": head,
                "board": 1,
                "operation": "ota",
                "returncode": 1,
                "error": "interrupted",
                "artifact_head": head,
                "previous_event_sha256": previous,
            }
            failed["event_sha256"] = (
                __import__("hashlib")
                .sha256(
                    __import__("json")
                    .dumps(failed, sort_keys=True, separators=(",", ":"))
                    .encode()
                )
                .hexdigest()
            )
            with (evidence / "broker-manifest.jsonl").open(
                "a", encoding="utf-8"
            ) as stream:
                stream.write(__import__("json").dumps(failed) + "\n")
            with self.assertRaisesRegex(broker.BrokerError, "no successful"):
                broker._selected_flash(cap, 1)

    def test_trace_dump_rejects_a_malformed_manifest_chain(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence = root / "evidence"
            evidence.mkdir()
            cap = broker.Capability(
                1, "spec", "a" * 40, root, evidence, ("trace-dump",), (0,), "token"
            )
            (evidence / "broker-manifest.jsonl").write_text(
                '{"previous_event_sha256":"wrong","event_sha256":"also-wrong"}\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                broker.BrokerError, "manifest chain is corrupt"
            ):
                broker._selected_flash(cap, 0)

    def test_candidate_cli_sandbox_build_has_no_network_or_host_home_mount(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, target = root / "source", root / "target"
            source.mkdir()
            target.mkdir()
            (source / "tools" / "domes-cli").mkdir(parents=True)
            (source / "tools" / "domes-cli" / "Cargo.lock").write_text(
                "# locked registry dependencies\n", encoding="utf-8"
            )
            cargo, rustup = root / "cargo", root / "rustup"
            (cargo / "registry").mkdir(parents=True)
            rustup.mkdir()
            bwrap = root / "bwrap"
            bwrap.write_text("tool", encoding="utf-8")
            cargo_path = Path("/usr/bin/cargo")
            cap = broker.Capability(
                1,
                "a",
                "b",
                root,
                root,
                ("trace-dump",),
                (0,),
                "token",
                (),
                {
                    "bwrap": {
                        "path": str(bwrap),
                        "sha256": __import__("hashlib").sha256(b"tool").hexdigest(),
                    },
                    "cargo": {
                        "path": str(cargo_path),
                        "sha256": __import__("hashlib")
                        .sha256(cargo_path.read_bytes())
                        .hexdigest(),
                    },
                },
            )
            with mock.patch.dict(
                "os.environ",
                {"CARGO_HOME": str(cargo), "RUSTUP_HOME": str(rustup)},
                clear=True,
            ):
                argv = broker._candidate_cli_build_argv(cap, source, target)
            self.assertIn("--unshare-all", argv)
            self.assertIn("--clearenv", argv)
            self.assertIn("--ro-bind", argv)
            self.assertNotIn(str(root / "evidence"), argv)
            self.assertNotIn(str(Path.home()), argv)
            self.assertEqual("0", argv[argv.index("CARGO_INCREMENTAL") + 1])
            self.assertEqual("0", argv[argv.index("CARGO_PROFILE_DEV_DEBUG") + 1])
            self.assertEqual(
                [str(cargo_path), "build", "--offline", "--locked"],
                argv[argv.index("--") + 1 :],
            )

    def test_trace_output_validation_requires_hashes_identity_and_candidate_binding(
        self,
    ):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            raw = output / "trace.json.raw"
            raw.write_bytes(b"r" * 16)
            raw_sha = __import__("hashlib").sha256(b"r" * 16).hexdigest()
            (output / "trace.json.raw.sha256").write_text(
                f"{raw_sha}  /out/trace.json.raw\n", encoding="utf-8"
            )
            (output / "trace.json").write_text("[]", encoding="utf-8")
            image_identity = {
                "file_sha256": "c" * 64,
                "app_elf_sha256": "a" * 64,
                "app_image_sha256": "b" * 64,
                "firmware_version": "candidate",
            }
            wire_identity = {
                "event_count": 1,
                "dropped_count": 0,
                "discontinuity_count": 0,
                "format_version": 1,
                "app_elf_sha256": "a" * 64,
                "app_image_sha256": "b" * 64,
                "firmware_version": "candidate",
                "device_uid": "020000000001",
            }
            session = {
                "integrity_error": None,
                "raw_sha256": raw_sha,
                "format_version": 1,
                "received_raw_bytes": 16,
                "event_count": 1,
                "dropped_count": 0,
                "discontinuity_count": 0,
                "app_elf_sha256": "a" * 64,
                "app_image_sha256": "b" * 64,
                "firmware_version": "candidate",
                "device_uid": "020000000001",
                "transport": {
                    "type": "serial",
                    "address": "/dev/domes-board-0",
                    "device_name": "serial",
                },
                "candidate_image": {
                    "binding_verified": True,
                    "path": "/domes.bin",
                    "file_sha256": image_identity["file_sha256"],
                    "app_elf_sha256": "a" * 64,
                    "app_image_sha256": "b" * 64,
                    "firmware_version": "candidate",
                },
            }
            (output / "trace.json.raw.session.json").write_text(
                __import__("json").dumps(session), encoding="utf-8"
            )
            hashes = broker._validate_trace_output(
                output, image_identity, 0, wire_identity
            )
            self.assertEqual(raw_sha, hashes["raw_sha256"])
            session["candidate_image"]["binding_verified"] = False
            (output / "trace.json.raw.session.json").write_text(
                __import__("json").dumps(session), encoding="utf-8"
            )
            with self.assertRaisesRegex(broker.BrokerError, "candidate binding"):
                broker._validate_trace_output(output, image_identity, 0, wire_identity)

    def test_trace_relay_binds_complete_device_frames_to_raw_events(self):
        import zlib

        def varint(value):
            encoded = bytearray()
            while value >= 0x80:
                encoded.append((value & 0x7F) | 0x80)
                value >>= 7
            encoded.append(value)
            return bytes(encoded)

        def field(number, value):
            if isinstance(value, int):
                return varint(number << 3) + varint(value)
            return varint((number << 3) | 2) + varint(len(value)) + value

        def frame(message_type, payload=b""):
            body = bytes([message_type]) + payload
            return (
                b"\xaa\x55"
                + len(body).to_bytes(2, "little")
                + body
                + (zlib.crc32(body) & 0xFFFF_FFFF).to_bytes(4, "little")
            )

        raw = bytes(range(32))
        session = b"".join(
            (
                field(2, 2),
                field(9, 1),
                field(12, b"candidate"),
                field(13, b"a" * 32),
                field(14, b"b" * 32),
                field(15, b"\x02\x00\x00\x00\x00\x01"),
            )
        )
        chunk = field(2, 2) + field(3, raw)
        end = field(1, 2) + field(2, sum(raw))
        transcript = [
            ("tx", frame(0x12)),
            ("rx", frame(0x1A, session) + frame(0x13, chunk) + frame(0x14, end)),
        ]
        relay, encoded, identity = broker._validate_trace_transcript(transcript, raw)
        self.assertEqual("broker-pty-frame-filter-v1", relay["kind"])
        self.assertEqual(2, relay["event_count"])
        self.assertEqual(3, relay["rx_frame_count"])
        self.assertTrue(encoded)
        self.assertEqual("candidate", identity["firmware_version"])

        with self.assertRaisesRegex(broker.BrokerError, "raw event"):
            broker._validate_trace_transcript(transcript, b"x" * 32)

        bad_request = [("tx", frame(0x10)), transcript[1]]
        with self.assertRaisesRegex(broker.BrokerError, "dump request"):
            broker._validate_trace_transcript(bad_request, raw)

    def test_trace_execution_sandbox_exposes_one_board_and_staged_inputs_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bwrap = root / "bwrap"
            bwrap.write_text("tool", encoding="utf-8")
            cap = broker.Capability(
                1,
                "a",
                "b",
                root,
                root,
                ("trace-dump",),
                (0,),
                "token",
                (),
                {
                    "bwrap": {
                        "path": str(bwrap),
                        "sha256": __import__("hashlib").sha256(b"tool").hexdigest(),
                    }
                },
            )
            candidate, pty_compat, names, image, output = (
                root / "cli",
                root / "compat.so",
                root / "names",
                root / "domes.bin",
                root / "out",
            )
            for path in (candidate, pty_compat, names, image):
                path.write_text("x", encoding="utf-8")
            output.mkdir()
            argv = broker._candidate_trace_argv(
                cap, candidate, pty_compat, "/dev/null", 0, output, names, image
            )
            self.assertIn("--unshare-all", argv)
            self.assertIn("--clearenv", argv)
            self.assertIn("--dev-bind", argv)
            self.assertIn("/dev/domes-board-0", argv)
            self.assertNotIn("/src", argv)
            command = argv[argv.index("--") + 1 :]
            self.assertIn("--firmware-bin", command)
            self.assertIn("/domes.bin", command)
            self.assertIn("LD_PRELOAD", argv)
            self.assertIn("/candidate/serial-pty-compat.so", argv)

    def test_candidate_build_rejects_git_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, target = root / "source", root / "target"
            (source / "tools" / "domes-cli").mkdir(parents=True)
            (source / "tools" / "domes-cli" / "Cargo.lock").write_text(
                'source = "git+ssh://example.invalid/private"\n', encoding="utf-8"
            )
            target.mkdir()
            cargo, rustup = root / "cargo", root / "rustup"
            (cargo / "registry").mkdir(parents=True)
            rustup.mkdir()
            trusted = root / "bwrap"
            trusted.write_text("tool", encoding="utf-8")
            cargo_path = Path("/usr/bin/cargo")
            cap = broker.Capability(
                1,
                "a",
                "b",
                root,
                root,
                ("trace-dump",),
                (0,),
                "token",
                (),
                {
                    "bwrap": {
                        "path": str(trusted),
                        "sha256": __import__("hashlib").sha256(b"tool").hexdigest(),
                    },
                    "cargo": {
                        "path": str(cargo_path),
                        "sha256": __import__("hashlib")
                        .sha256(cargo_path.read_bytes())
                        .hexdigest(),
                    },
                },
            )
            with mock.patch.dict(
                "os.environ",
                {"CARGO_HOME": str(cargo), "RUSTUP_HOME": str(rustup)},
                clear=True,
            ):
                with self.assertRaisesRegex(broker.BrokerError, "git dependencies"):
                    broker._candidate_cli_build_argv(cap, source, target)

    def test_candidate_source_preserves_untrusted_symlinks_without_host_dereference(
        self,
    ):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            (source / "tools" / "domes-cli").mkdir(parents=True)
            outside = root / "outside"
            outside.write_text("host data", encoding="utf-8")
            (source / "escape").symlink_to(outside)
            evidence = root / "evidence"
            evidence.mkdir()
            cap = broker.Capability(
                1, "a", "b", root, evidence, ("trace-dump",), (0,), "token"
            )
            sanitized = broker._candidate_source_tree(cap, source, "a" * 40)
            self.assertTrue((sanitized / "escape").is_symlink())
            self.assertEqual(str(outside), (sanitized / "escape").readlink().as_posix())

    def test_hardware_artifact_is_resolved_only_from_pinned_remote_head(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cap = self.capability(root)
            git = root / "git"
            git.write_bytes(b"git")
            cap = broker.Capability(
                **{
                    **cap.__dict__,
                    "tools": {
                        "git": {
                            "path": str(git),
                            "sha256": hashlib.sha256(b"git").hexdigest(),
                        }
                    },
                }
            )
            completed = subprocess.CompletedProcess(
                [],
                0,
                stdout=f"{'b' * 40}\trefs/heads/codex/issue-101\n",
                stderr="",
            )
            with mock.patch.object(
                broker.subprocess, "run", return_value=completed
            ) as run:
                self.assertEqual("b" * 40, broker._workspace_head(cap))
            argv = run.call_args.args[0]
            self.assertIn(cap.repository_url, argv)
            self.assertIn(f"refs/heads/{cap.head_ref}", argv)
            self.assertNotIn(str(cap.workspace), argv)
            changed = subprocess.CompletedProcess(
                [],
                0,
                stdout=f"{'c' * 40}\trefs/heads/codex/issue-101\n",
                stderr="",
            )
            with mock.patch.object(broker.subprocess, "run", return_value=changed):
                with self.assertRaisesRegex(broker.BrokerError, "safety review"):
                    broker._workspace_head(cap)

    def test_firmware_build_sandbox_uses_explicit_tools_without_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, build = root / "source", root / "build"
            sdkconfig, defaults, idf = (
                root / "sdkconfig",
                root / "defaults",
                root / "idf",
            )
            source.mkdir()
            defaults.write_text('CONFIG_IDF_TARGET="esp32s3"\n', encoding="utf-8")
            idf.mkdir()
            cap = broker.Capability(1, "a", "b", root, root, ("flash",), (0,), "token")
            mapped = {
                "xtensa-esp32s3-elf-gcc": Path(
                    "/idf-tools/tools/xtensa/bin/xtensa-esp32s3-elf-gcc"
                ),
                "esp32ulp-elf-as": Path("/idf-tools/tools/ulp/bin/esp32ulp-elf-as"),
                "esp-rom-elf": Path("/idf-tools/tools/rom/esp32s3_rev0_rom.elf"),
                "idf-python": Path("/idf-tools/python_env/idf5.4/bin/python"),
            }
            with (
                mock.patch.object(broker, "_bwrap", return_value="/usr/bin/bwrap"),
                mock.patch.object(
                    broker, "_espressif_root", return_value=root / ".espressif"
                ),
                mock.patch.object(
                    broker,
                    "_mapped_espressif_path",
                    side_effect=lambda _c, name: mapped[name],
                ),
                mock.patch.object(
                    broker, "_resource_limited", side_effect=lambda _c, argv: argv
                ),
            ):
                argv = broker._firmware_build_argv(
                    cap, source, build, sdkconfig, defaults, idf
                )
            self.assertIn("--unshare-all", argv)
            self.assertIn("--clearenv", argv)
            self.assertNotIn("/idf/export.sh", " ".join(argv))
            self.assertNotIn("/usr/bin/bash", argv)
            self.assertEqual(
                "/idf-tools/python_env/idf5.4/bin/python",
                argv[argv.index("--") + 1],
            )

    def test_managed_components_are_lock_bound_and_staged_from_attested_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence, project, cached = (
                root / "evidence",
                root / "project",
                root / "cache",
            )
            evidence.mkdir()
            project.mkdir()
            cached.mkdir()
            lock = root / "dependencies.lock"
            lock.write_text("pinned\n", encoding="utf-8")
            (project / "dependencies.lock").write_text("pinned\n", encoding="utf-8")
            (cached / "component.c").write_text("source\n", encoding="utf-8")
            tools = {
                "dependencies.lock": {
                    "path": str(lock),
                    "sha256": hashlib.sha256(b"pinned\n").hexdigest(),
                },
                "managed-component-0": {
                    "path": str(cached),
                    "sha256": broker._directory_sha256(cached),
                    "destination": "espressif__component",
                    "component_hash": "a" * 64,
                    "version": "1.2.3",
                },
            }
            cap = broker.Capability(
                1, "a", "b", root, evidence, ("flash",), (0,), "token", tools=tools
            )
            provenance = broker._stage_managed_components(cap, project)
            target = project / "managed_components" / "espressif__component"
            self.assertEqual("source\n", (target / "component.c").read_text())
            self.assertEqual("a" * 64, (target / ".component_hash").read_text())
            self.assertEqual("a" * 64, provenance[0]["component_hash"])
            (project / "dependencies.lock").write_text("changed\n", encoding="utf-8")
            with self.assertRaisesRegex(broker.BrokerError, "dependency lock"):
                broker._stage_managed_components(cap, project)

    def test_candidate_firmware_safety_rejects_irreversible_added_calls(self):
        cap = broker.Capability(
            1,
            "a",
            "b",
            Path("/tmp/source"),
            Path("/tmp/evidence"),
            ("flash",),
            (0,),
            "token",
            tools={"git": {"path": "/usr/bin/git", "sha256": "unused"}},
            base_head="a" * 40,
        )
        completed = subprocess.CompletedProcess(
            [], 0, stdout="+esp_efuse_write_field_blob();\n", stderr=""
        )
        with (
            mock.patch.object(broker, "_trusted_path", return_value="/usr/bin/git"),
            mock.patch.object(broker.subprocess, "run", return_value=completed),
        ):
            with self.assertRaisesRegex(broker.BrokerError, "forbidden"):
                broker._validate_candidate_firmware_safety(
                    cap, Path("/tmp/source"), "b" * 40
                )

    def test_candidate_firmware_safety_allows_read_only_factory_mac(self):
        cap = broker.Capability(
            1,
            "a",
            "b",
            Path("/tmp/source"),
            Path("/tmp/evidence"),
            ("flash",),
            (0,),
            "token",
            tools={"git": {"path": "/usr/bin/git", "sha256": "unused"}},
            base_head="a" * 40,
        )
        completed = subprocess.CompletedProcess(
            [], 0, stdout="+esp_efuse_mac_get_default(device_uid);\n", stderr=""
        )
        with (
            mock.patch.object(broker, "_trusted_path", return_value="/usr/bin/git"),
            mock.patch.object(broker.subprocess, "run", return_value=completed),
        ):
            broker._validate_candidate_firmware_safety(
                cap, Path("/tmp/source"), "b" * 40
            )

    def test_candidate_firmware_safety_rejects_efuse_protection_changes(self):
        cap = broker.Capability(
            1,
            "a",
            "b",
            Path("/tmp/source"),
            Path("/tmp/evidence"),
            ("flash",),
            (0,),
            "token",
            tools={"git": {"path": "/usr/bin/git", "sha256": "unused"}},
            base_head="a" * 40,
        )
        completed = subprocess.CompletedProcess(
            [], 0, stdout="+esp_efuse_set_write_protect(EFUSE_BLK0);\n", stderr=""
        )
        with (
            mock.patch.object(broker, "_trusted_path", return_value="/usr/bin/git"),
            mock.patch.object(broker.subprocess, "run", return_value=completed),
        ):
            with self.assertRaisesRegex(broker.BrokerError, "forbidden"):
                broker._validate_candidate_firmware_safety(
                    cap, Path("/tmp/source"), "b" * 40
                )

    def test_serial_proxy_cleans_up_every_fd_when_enter_partially_fails(self):
        proxy = serial_trace_proxy.SerialTraceProxy("/dev/fake")
        with (
            mock.patch.object(serial_trace_proxy.pty, "openpty", return_value=(10, 11)),
            mock.patch.object(
                serial_trace_proxy.os, "ttyname", return_value="/dev/pts/1"
            ),
            mock.patch.object(serial_trace_proxy.os, "open", return_value=12),
            mock.patch.object(serial_trace_proxy.fcntl, "ioctl"),
            mock.patch.object(
                serial_trace_proxy,
                "_set_raw_115200",
                side_effect=[None, OSError("configure failed")],
            ),
            mock.patch.object(serial_trace_proxy.os, "close") as close,
        ):
            with self.assertRaisesRegex(OSError, "configure failed"):
                proxy.__enter__()
        self.assertEqual({10, 11, 12}, {call.args[0] for call in close.call_args_list})
        self.assertIsNone(proxy.device)
