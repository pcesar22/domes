#!/usr/bin/env python3

import hashlib
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ReleaseContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow = (ROOT / ".github/workflows/firmware-release.yml").read_text()
        cls.hardware_workflow = (
            ROOT / ".github/workflows/firmware-hw-test.yml"
        ).read_text()
        cls.software_workflow = (ROOT / ".github/workflows/firmware-ci.yml").read_text()
        cls.flutter_workflow = (ROOT / ".github/workflows/flutter-ci.yml").read_text()
        cls.pre_commit_config = (ROOT / ".pre-commit-config.yaml").read_text()
        cls.flash_helper = (ROOT / "tools/firmware/flash_and_verify.sh").read_text()
        cls.restart_snapshot_helper = (
            ROOT / "tools/firmware/verify_restart_snapshot.sh"
        ).read_text()
        cls.verify_script = (ROOT / "scripts/verify.sh").read_text()
        cls.testing_docs = (ROOT / "docs/TESTING.md").read_text()
        cls.metadata = (
            ROOT / "firmware/domes/main/services/releaseMetadata.cpp"
        ).read_text()
        cls.github_client = (
            ROOT / "firmware/domes/main/services/githubClient.cpp"
        ).read_text()
        cls.ota_manager = (
            ROOT / "firmware/domes/main/services/otaManager.cpp"
        ).read_text()

    def test_release_publishes_exact_versioned_ota_asset(self) -> None:
        self.assertIn(
            'cp domes.bin "$release_dir/domes-${RELEASE_VERSION}.bin"',
            self.workflow,
        )
        self.assertIn(
            '"$release_dir/domes-${RELEASE_VERSION}-factory.bin"',
            self.workflow,
        )
        self.assertIn(
            '-o "$release_dir/domes-${RELEASE_VERSION}-factory.bin"',
            self.workflow,
        )
        self.assertNotIn('"$release_dir/domes-factory.bin"', self.workflow)
        self.assertNotIn(
            'cp domes.bin "$release_dir/domes.bin"',
            self.workflow,
        )
        self.assertIn(
            '-e "s#domes\\\\.bin#domes-${RELEASE_VERSION}.bin#"',
            self.workflow,
        )
        self.assertIn(
            'merge_bin -o "$manual_flash_image" @flash_args',
            self.workflow,
        )
        self.assertIn(
            'cmp -- "$manual_flash_image" "domes-${RELEASE_VERSION}-factory.bin"',
            self.workflow,
        )
        self.assertIn('"domes-%s.bin"', self.metadata)
        self.assertIn("formatOtaAssetName(", self.github_client)

    def test_release_publishes_digest_consumed_by_firmware(self) -> None:
        self.assertIn(
            'app_sha256=$(sha256sum "domes-${RELEASE_VERSION}.bin"',
            self.workflow,
        )
        self.assertIn('echo "app_sha256=$app_sha256"', self.workflow)
        self.assertIn(
            "SHA-256: ${{ steps.package.outputs.app_sha256 }}",
            self.workflow,
        )
        self.assertIn("isSha256Hex(sha256Out)", self.github_client)
        self.assertIn("isSha256Hex(expectedSha256)", self.ota_manager)

    def test_ci_and_release_packages_retain_exact_diagnostics(self) -> None:
        release_fragments = (
            'cp domes.elf "$release_dir/domes.elf"',
            'cp project_description.json "$release_dir/project_description.json"',
            "test -s domes.elf",
            "test -s project_description.json",
            "sha256sum -- *.bin *.elf *.json flash_args > SHA256SUMS",
            'metadata.get("project_version") != os.environ["RELEASE_VERSION"]',
        )
        for fragment in release_fragments:
            self.assertIn(fragment, self.workflow)

        ci_fragments = (
            'cp domes.elf "$release_dir/domes.elf"',
            'cp project_description.json "$release_dir/project_description.json"',
            "test -s domes.elf",
            "test -s project_description.json",
            "            domes.elf \\",
            "            project_description.json \\",
        )
        for fragment in ci_fragments:
            self.assertIn(fragment, self.software_workflow)

        for workflow in (self.workflow, self.software_workflow):
            self.assertIn('metadata.get("app_elf", "")', workflow)
            self.assertIn('metadata_elf != root / "domes.elf"', workflow)

    def test_release_build_embeds_the_validated_tag(self) -> None:
        self.assertIn(
            "RELEASE_VERSION: ${{ needs.validate-release.outputs.version }}",
            self.workflow,
        )
        self.assertIn(
            '-D DOMES_VERSION_OVERRIDE="$RELEASE_VERSION"',
            self.workflow,
        )
        self.assertIn("SDKCONFIG=$sdkconfig_path", self.workflow)
        self.assertIn("embedded_version=$(", self.workflow)
        self.assertIn('[[ "$embedded_version" != "$RELEASE_VERSION" ]]', self.workflow)

    def test_release_accepts_stable_tags_only_and_documents_that_policy(self) -> None:
        self.assertIn(
            "Release tag must be a stable vMAJOR.MINOR.PATCH version",
            self.workflow,
        )
        self.assertIn('echo "prerelease=false"', self.workflow)
        self.assertIn("Stable `vMAJOR.MINOR.PATCH` tags", self.testing_docs)
        self.assertRegex(
            self.testing_docs,
            r"Prerelease identifiers and build\s+metadata are intentionally rejected",
        )

    def test_software_ci_has_one_stable_aggregate_gate(self) -> None:
        self.assertIn(
            "  workflow_call:\n  pull_request:\n  merge_group:\n",
            self.software_workflow,
        )
        self.assertNotIn("pull_request_target", self.software_workflow)
        self.assertIn("    branches: [main]\n", self.software_workflow)
        self.assertIn("    name: CI Gate\n", self.software_workflow)
        self.assertIn(
            "needs: [firmware-build, unit-tests, cli-build, host-tooling, flutter]",
            self.software_workflow,
        )
        self.assertIn('if [[ "$result" != "success" ]]', self.software_workflow)
        self.assertEqual(1, self.software_workflow.count("    name: CI Gate\n"))
        ci_gate = self.software_workflow.split("  ci-gate:\n", maxsplit=1)[1]
        self.assertIn("    if: ${{ always() }}\n", ci_gate)

        self.assertIn("  workflow_call:\n", self.flutter_workflow)
        self.assertNotIn("  pull_request:\n", self.flutter_workflow)
        self.assertNotIn("  push:\n", self.flutter_workflow)

    def test_software_ci_runs_every_host_tool_unit_suite(self) -> None:
        agent_eval_test = (
            "python3 -m unittest discover -s tools/agent_eval -p 'test_*.py' -v"
        )
        self.assertIn(agent_eval_test, self.software_workflow)
        self.assertIn(agent_eval_test, self.verify_script)

    def test_local_and_ci_cli_checks_build_debug_and_release_profiles(self) -> None:
        build_sequence = re.compile(
            r"cargo build --locked(?: &&)?\n\s*cargo build --locked --release"
        )
        self.assertRegex(self.software_workflow, build_sequence)
        self.assertRegex(self.verify_script, build_sequence)

    def test_local_firmware_check_matches_ci_configuration_invariants(self) -> None:
        for workflow in (self.software_workflow, self.verify_script):
            self.assertIn(
                'config.get("BOOTLOADER_APP_ROLLBACK_ENABLE") is not True', workflow
            )
            self.assertIn("git status --porcelain -- dependencies.lock", workflow)
            self.assertIn("ESP-IDF rewrote firmware/domes/dependencies.lock", workflow)

        self.assertIn("<<'PY' || exit 1", self.verify_script)
        self.assertIn(
            "firmware build does not enable bootloader app rollback",
            self.verify_script,
        )

        self.assertNotIn(
            "Software/release CI uses isolated build directories",
            self.testing_docs,
        )

    def test_external_actions_and_idf_images_are_immutably_pinned(self) -> None:
        workflows = "\n".join(
            path.read_text()
            for path in sorted((ROOT / ".github/workflows").glob("*.yml"))
        )
        external_actions = re.findall(r"^\s*uses:\s+([^\s#]+)", workflows, re.MULTILINE)
        self.assertGreater(len(external_actions), 0)
        for action in external_actions:
            if action.startswith("./"):
                continue
            self.assertRegex(action, r"^[^@]+@[0-9a-f]{40}$")

        idf_images = re.findall(
            r"^\s*image:\s+(espressif/idf:[^\s]+)", workflows, re.MULTILINE
        )
        self.assertGreater(len(idf_images), 0)
        for image in idf_images:
            self.assertRegex(image, r"^espressif/idf:v5\.4\.4@sha256:[0-9a-f]{64}$")

    def test_markdownlint_uses_an_isolated_lts_node_runtime(self) -> None:
        markdownlint_hook = self.pre_commit_config.split(
            "repo: https://github.com/igorshubovych/markdownlint-cli", maxsplit=1
        )[1]
        self.assertIn("language_version: 24.18.1", markdownlint_hook)

    def test_hardware_ota_builds_use_three_bounded_distinct_versions(self) -> None:
        self.assertIn(
            'baseline_version="v0.0.0-0-g${GITHUB_SHA:0:12}"',
            self.hardware_workflow,
        )
        self.assertIn(
            'ota_version="v0.0.0-1-g${GITHUB_SHA:0:12}"',
            self.hardware_workflow,
        )
        self.assertIn(
            'rollback_version="v0.0.0-2-g${GITHUB_SHA:0:12}"',
            self.hardware_workflow,
        )
        self.assertIn(
            "assert len(set(expected_versions.values())) == 3", self.hardware_workflow
        )
        self.assertIn(
            'len(expected_version.encode("ascii")) <= 31', self.hardware_workflow
        )
        self.assertIn(
            'description["project_version"] == expected_version', self.hardware_workflow
        )
        self.assertIn('line.startswith("App version: ")', self.hardware_workflow)
        self.assertNotIn("v0.0.0-rollback-g", self.hardware_workflow)

    def test_hardware_summary_records_tested_head_and_base_commits(self) -> None:
        required_fragments = (
            "SUMMARY_HEAD_SHA: ${{ github.event.pull_request.head.sha || github.sha }}",
            "SUMMARY_BASE_SHA: ${{ github.event.pull_request.base.sha || '' }}",
            "| Tested merge/dispatch commit |",
            "| Source commit |",
            "| Pull request base commit |",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, self.hardware_workflow)

    def test_hardware_ota_proves_version_partition_and_boot_transitions(self) -> None:
        self.assertIn("timeout-minutes: 120", self.hardware_workflow)
        self.assertIn("Factory image version does not match", self.hardware_workflow)
        self.assertIn("Running App.*PASS.*ota_0,", self.hardware_workflow)
        self.assertIn("Running App.*PASS.*ota_1,", self.hardware_workflow)
        self.assertIn(
            "Serial OTA did not advance the primary boot count", self.hardware_workflow
        )
        self.assertIn(
            "BLE OTA did not advance the secondary boot count", self.hardware_workflow
        )
        self.assertIn(
            "Device accepted an OTA image with mismatched declared version",
            self.hardware_workflow,
        )
        self.assertIn(
            "Rollback did not produce both failing-image and restored-image boots",
            self.hardware_workflow,
        )
        self.assertEqual(
            3,
            self.hardware_workflow.count("tools/firmware/verify_restart_snapshot.sh"),
        )

    def test_hardware_separates_espnow_benchmark_and_drill_sessions(self) -> None:
        initial_disable = self.hardware_workflow.index(
            'wait_for_disabled "initial benchmark state"'
        )
        benchmark = self.hardware_workflow.index('run_benchmark "$SLAVE_PORT"')
        benchmark_disable = self.hardware_workflow.index(
            '"$CLI" --port "$PRIMARY" feature disable esp-now', benchmark
        )
        drill_enable = self.hardware_workflow.index(
            'wait_for_peers "simulated drill"', benchmark_disable
        )
        completed_drill_wait = self.hardware_workflow.index(
            "Waiting for the complete ten-round simulated drill", drill_enable
        )
        self.assertLess(initial_disable, benchmark)
        self.assertLess(benchmark, benchmark_disable)
        self.assertLess(benchmark_disable, drill_enable)
        self.assertLess(drill_enable, completed_drill_wait)
        self.assertIn("for session in 1 2 3", self.hardware_workflow)
        self.assertIn('run_benchmark "$MASTER_PORT"', self.hardware_workflow)
        self.assertIn(
            'wait_for_peers "benchmark session $session"', self.hardware_workflow
        )
        self.assertIn(
            'wait_for_disabled "benchmark session $session"', self.hardware_workflow
        )
        self.assertIn('"$CLI" --port "$port" trace clear', self.hardware_workflow)

    def test_hardware_ble_ota_failure_recovery_precedes_accepted_update(self) -> None:
        failure_recovery = self.hardware_workflow.index(
            "Test - BLE OTA Failure Recovery (secondary device)"
        )
        accepted_update = self.hardware_workflow.index(
            "Test - BLE OTA (secondary device)"
        )
        self.assertLess(failure_recovery, accepted_update)
        self.assertIn(
            "Device accepted a truncated BLE OTA image", self.hardware_workflow
        )
        self.assertIn("Device accepted OTA_BEGIN.", self.hardware_workflow)
        self.assertIn("Waiting for interrupted BLE OTA cleanup", self.hardware_workflow)
        self.assertGreaterEqual(
            self.hardware_workflow.count("verify_secondary_recovery"), 3
        )

    def test_hardware_ble_retry_is_bounded_to_transient_diagnostic_reads(
        self,
    ) -> None:
        required_fragments = (
            "for attempt in 1 2",
            'grep -Fq "Failed to connect to BLE device"',
            'grep -Fqi "le-connection-abort-by-local"',
            '"$BLE_READ_RETRY" "$CLI" --ble "$address" system self-test',
            '"$BLE_READ_RETRY" "$CLI" --ble "$SECONDARY_BLE" system health',
        )
        for fragment in required_fragments:
            self.assertIn(fragment, self.hardware_workflow)

        self.assertNotIn(
            '"$BLE_READ_RETRY" "$CLI" --ble "$SECONDARY_BLE" ota flash',
            self.hardware_workflow,
        )

    def test_hardware_serial_ota_negative_paths_prove_protocol_progress(self) -> None:
        required_fragments = (
            "Truncated serial OTA failed for an unexpected reason",
            'grep -Fq "Verification failed" <<< "$invalid_output"',
            "Version-mismatched serial OTA failed for an unexpected reason",
            'grep -Fq "Version error" <<< "$mismatch_output"',
            "domes-interrupted-serial-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}.log",
            "timeout --signal=TERM --kill-after=5s 5s",
            'grep -Fq "Device accepted OTA_BEGIN." "$interrupt_log"',
            "Interrupted serial OTA changed the running image",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, self.hardware_workflow)

    def test_hardware_registry_exercises_fanout_and_rejects_ambiguity(self) -> None:
        required_fragments = (
            'HOME="$REGISTRY_HOME" "$CLI" devices add pod1 serial "$PRIMARY"',
            'HOME="$REGISTRY_HOME" "$CLI" devices add pod2 serial "$SECONDARY"',
            'HOME="$REGISTRY_HOME" "$CLI" devices list',
            "--target pod1 --target pod2 feature list",
            'HOME="$REGISTRY_HOME" "$CLI" --all system memory --json',
            'profiles = document.get("devices")',
            'set(profiles) != {"pod1", "pod2"}',
            "Registry accepted a duplicate canonical serial endpoint",
            'HOME="$REGISTRY_HOME" "$CLI" devices remove pod1',
            'HOME="$REGISTRY_HOME" "$CLI" devices remove pod2',
        )
        for fragment in required_fragments:
            self.assertIn(fragment, self.hardware_workflow)

        self.assertIn('--all --port "$PRIMARY" feature list', self.hardware_workflow)

    def test_hardware_summary_and_cleanup_retain_final_state(self) -> None:
        required_fragments = (
            "id: wifi_capability",
            "WIFI_CAPABILITY_OUTCOME: ${{ steps.wifi_capability.outcome }}",
            'report "Default-build WiFi capability contract" "$WIFI_CAPABILITY_OUTCOME"',
            "'feature enable haptic'",
            "'feature enable wifi'",
            "'ota auto-update --enable'",
            '"$CLI" --port "$port" ota auto-update --disable',
            "Capturing final health and feature state",
            "Final device mode is not idle",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, self.hardware_workflow)

    def test_hardware_espnow_uses_readiness_rounds_and_cancellation_probe(self) -> None:
        self.assertIn("--rounds 100", self.hardware_workflow)
        self.assertIn("100/100 completed", self.hardware_workflow)
        self.assertIn(
            'wait_for_disabled "single-pod discovery cancellation"',
            self.hardware_workflow,
        )

    def test_hardware_requires_unique_ble_identity_and_complementary_roles(
        self,
    ) -> None:
        self.assertIn("(${#PORTS[@]} != 2)", self.hardware_workflow)
        self.assertIn(
            "requires exactly two selected NFF CP2102N runtime ports",
            self.hardware_workflow,
        )
        self.assertIn(
            "Multiple BLE addresses matched selected pod ID", self.hardware_workflow
        )
        self.assertIn(
            '"$primary_role:$secondary_role" == "master:slave"',
            self.hardware_workflow,
        )
        self.assertIn(
            '"$primary_role:$secondary_role" == "slave:master"',
            self.hardware_workflow,
        )
        self.assertIn("exactly one master and one slave", self.hardware_workflow)

    def test_hardware_cleanup_waits_for_espnow_to_stop(self) -> None:
        cleanup = self.hardware_workflow.index("Restore deterministic idle state")
        cleanup_workflow = self.hardware_workflow[cleanup:]
        self.assertIn("for attempt in {1..20}", cleanup_workflow)
        self.assertIn("State:[[:space:]]+disabled", cleanup_workflow)
        self.assertIn("ESP-NOW cleanup did not reach disabled", cleanup_workflow)

    def test_all_ci_firmware_builds_select_esp32s3_explicitly(self) -> None:
        for workflow in (
            self.software_workflow,
            self.hardware_workflow,
            self.workflow,
        ):
            self.assertIn('-D "IDF_TARGET=esp32s3"', workflow)

    def test_factory_programming_erases_persistent_state_first(self) -> None:
        hardware_erase = self.hardware_workflow.index("erase_flash")
        hardware_factory_write = self.hardware_workflow.index(
            "write_flash 0x0 build/domes-factory.bin"
        )
        self.assertLess(hardware_erase, hardware_factory_write)
        self.assertIn(
            "Factory erase did not clear the persisted pod ID",
            self.hardware_workflow,
        )

        release_erase = self.workflow.index("erase_flash")
        release_factory_write = self.workflow.index(
            "0x0 domes-${{ needs.validate-release.outputs.version }}-factory.bin"
        )
        self.assertLess(release_erase, release_factory_write)

    def test_flash_helper_checks_the_exact_image_and_device_health(self) -> None:
        self.assertIn(
            '["project_version"]',
            self.flash_helper,
        )
        self.assertIn(
            'grep -Fq "Firmware:   $EXPECTED_FIRMWARE_VERSION"',
            self.flash_helper,
        )
        self.assertIn("system health", self.flash_helper)
        self.assertIn("system self-test", self.flash_helper)
        self.assertIn("if [[ $# -gt 2 ]]", self.flash_helper)
        self.assertNotIn("legacy verify-string", self.flash_helper)

    def test_restart_snapshot_helper_checks_boot_heap_backtrace_and_elf(self) -> None:
        self.assertIn(
            '"$boot_count" != "$EXPECTED_BOOT_COUNT"', self.restart_snapshot_helper
        )
        self.assertIn(
            '"$firmware_version" != "$EXPECTED_VERSION"', self.restart_snapshot_helper
        )
        self.assertIn('metadata.get("project_version")', self.restart_snapshot_helper)
        self.assertIn("Internal free heap:", self.restart_snapshot_helper)
        self.assertIn('"$format_version" != "2"', self.restart_snapshot_helper)
        self.assertIn('sha256sum "$ELF"', self.restart_snapshot_helper)
        self.assertIn(
            '"$snapshot_elf_sha" != "$expected_elf_sha"', self.restart_snapshot_helper
        )
        self.assertIn('for i in "${!pcs[@]}"', self.restart_snapshot_helper)
        self.assertIn("repeated adjacent frame", self.restart_snapshot_helper)
        self.assertIn("xtensa-esp32s3-elf-addr2line", self.restart_snapshot_helper)

    def test_local_host_test_build_is_ephemeral(self) -> None:
        self.assertIn("$VERIFY_TMP/host-test-build", self.verify_script)
        self.assertNotIn(
            '-B "$ROOT_DIR/firmware/test_app/build"',
            self.verify_script,
        )

    def test_github_release_client_fails_closed_on_bounded_input(self) -> None:
        self.assertIn("url[sizeof(url) - 1] = '\\0';", self.github_client)
        self.assertIn("if (readLen < 0)", self.github_client)
        self.assertIn(
            "release.firmware.name[sizeof(release.firmware.name) - 1] = '\\0';",
            self.github_client,
        )
        self.assertIn(
            "release.firmware.downloadUrl[sizeof(release.firmware.downloadUrl) - 1] = '\\0';",
            self.github_client,
        )
        self.assertIn("parseReleaseAssetSize(", self.github_client)


class RestartSnapshotHelperTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.bin_dir = self.root / "bin"
        self.bin_dir.mkdir()
        self.elf = self.root / "pre-restart.elf"
        self.elf.write_bytes(b"test elf")
        self.elf_sha = hashlib.sha256(self.elf.read_bytes()).hexdigest()
        (self.root / "project_description.json").write_text(
            '{"app_elf":"pre-restart.elf","project_version":"v1.2.3"}\n'
        )

        self.cli = self.bin_dir / "domes-cli"
        self.cli.write_text(
            "#!/usr/bin/env bash\n"
            "cat <<EOF\n"
            "Clean-Restart Snapshot:\n"
            "  Reason:    shutdown/restart\n"
            "  Task:      serial_ota\n"
            "  Uptime:    42 s\n"
            "  Internal free heap: ${SNAPSHOT_HEAP:-49152} bytes\n"
            "  Boot count: ${SNAPSHOT_BOOT:-7}\n"
            "  Snapshot format: ${SNAPSHOT_FORMAT:-2}\n"
            "  Firmware:   ${SNAPSHOT_VERSION-v1.2.3}\n"
            f"  ELF SHA256: ${{SNAPSHOT_ELF_SHA:-{self.elf_sha}}}\n"
            "  Backtrace:\n"
            "    #0: ${SNAPSHOT_PC0:-0x42001234}\n"
            "    #1: 0x42005678\n"
            "    #2: ${SNAPSHOT_PC2:-0x42009ABC}\n"
            "EOF\n"
        )
        self.cli.chmod(0o755)

        addr2line = self.bin_dir / "xtensa-esp32s3-elf-addr2line"
        addr2line.write_text(
            "#!/usr/bin/env bash\n"
            'for arg in "$@"; do\n'
            "  if [[ $arg != 0x* ]]; then\n"
            "    continue\n"
            "  fi\n"
            "  if [[ ${ADDR2LINE_UNRESOLVED:-0} == 1 || "
            "$arg == ${ADDR2LINE_UNRESOLVED_PC:-none} ]]; then\n"
            '    echo "$arg: ?? at ??:0"\n'
            "  else\n"
            '    echo "$arg: snapshotFrame() at crashDumpHandler.cpp:185"\n'
            "  fi\n"
            "done\n"
        )
        addr2line.chmod(0o755)

        self.script = ROOT / "tools/firmware/verify_restart_snapshot.sh"
        self.env = {**os.environ, "PATH": f"{self.bin_dir}:/usr/bin:/bin"}

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def run_helper(self, **environment: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                str(self.script),
                "/dev/test",
                "7",
                "v1.2.3",
                str(self.elf),
                str(self.cli),
            ],
            check=False,
            capture_output=True,
            text=True,
            env={**self.env, **environment},
        )

    def test_accepts_consistent_snapshot_and_version_matched_elf(self) -> None:
        result = self.run_helper()
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn("Restart snapshot verified for boot 7", result.stdout)

    def test_rejects_wrong_boot_psram_heap_and_duplicate_frame(self) -> None:
        cases = (
            ({"SNAPSHOT_BOOT": "6"}, "boot count mismatch"),
            ({"SNAPSHOT_HEAP": "8388608"}, "internal heap is invalid"),
            ({"SNAPSHOT_PC2": "0x42005678"}, "repeated adjacent frame"),
            ({"SNAPSHOT_VERSION": "v9.9.9"}, "firmware mismatch"),
            ({"SNAPSHOT_VERSION": ""}, "firmware mismatch"),
            ({"SNAPSHOT_FORMAT": "0"}, "format mismatch"),
            ({"SNAPSHOT_ELF_SHA": "0" * 64}, "ELF mismatch"),
            ({"SNAPSHOT_PC0": "0x00000000"}, "invalid PC"),
            ({"ADDR2LINE_UNRESOLVED": "1"}, "did not resolve"),
            (
                {"ADDR2LINE_UNRESOLVED_PC": "0x42009ABC"},
                "entry 2 (0x42009ABC) did not resolve",
            ),
        )
        for environment, message in cases:
            with self.subTest(message=message):
                result = self.run_helper(**environment)
                self.assertNotEqual(0, result.returncode)
                self.assertIn(message, result.stderr)


class FirmwareVersionCmakeTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.repo = Path(self.temp_dir.name) / "repo"
        self.repo.mkdir()
        subprocess.run(["git", "init", "-q", str(self.repo)], check=True)
        (self.repo / "tracked.txt").write_text("initial\n")
        env = os.environ.copy()
        env.update(
            {
                "GIT_AUTHOR_NAME": "DOMES CI",
                "GIT_AUTHOR_EMAIL": "ci@domes.invalid",
                "GIT_COMMITTER_NAME": "DOMES CI",
                "GIT_COMMITTER_EMAIL": "ci@domes.invalid",
            }
        )
        subprocess.run(["git", "-C", str(self.repo), "add", "tracked.txt"], check=True)
        subprocess.run(
            ["git", "-C", str(self.repo), "commit", "-q", "-m", "initial"],
            check=True,
            env=env,
        )
        self.output = Path(self.temp_dir.name) / "version.txt"
        self.script = Path(self.temp_dir.name) / "resolve.cmake"
        module = ROOT / "firmware/domes/cmake/ResolveFirmwareVersion.cmake"
        self.script.write_text(
            f'include("{module}")\n'
            'domes_resolve_firmware_version(version "${SOURCE_DIR}" "${OVERRIDE}")\n'
            'file(WRITE "${OUTPUT_FILE}" "${version}")\n'
        )

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def resolve(self, override: str = "") -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                "cmake",
                f"-DSOURCE_DIR={self.repo}",
                f"-DOUTPUT_FILE={self.output}",
                f"-DOVERRIDE={override}",
                "-P",
                str(self.script),
            ],
            text=True,
            capture_output=True,
        )

    def test_tagless_checkout_gets_semver_shaped_fallback(self) -> None:
        result = self.resolve()
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertRegex(self.output.read_text(), r"^v0\.0\.0-0-g[0-9a-f]{12}$")

    def test_tagged_and_dirty_versions_are_preserved(self) -> None:
        subprocess.run(["git", "-C", str(self.repo), "tag", "v1.2.3"], check=True)
        self.assertEqual(0, self.resolve().returncode)
        self.assertEqual("v1.2.3", self.output.read_text())

        (self.repo / "tracked.txt").write_text("changed\n")
        self.assertEqual(0, self.resolve().returncode)
        self.assertEqual("v1.2.3-dirty", self.output.read_text())

    def test_invalid_override_fails_configuration(self) -> None:
        result = self.resolve("not-a-version")
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Invalid DOMES firmware version override", result.stderr)

    def test_31_byte_override_is_accepted(self) -> None:
        override = "v0.0.0-0-g0123456789abcdef01234"
        self.assertEqual(31, len(override))
        result = self.resolve(override)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(override, self.output.read_text())

    def test_32_byte_override_is_rejected_before_descriptor_truncation(self) -> None:
        override = "v0.0.0-0-g0123456789abcdef012345"
        self.assertEqual(32, len(override))
        result = self.resolve(override)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("ESP app descriptor limit is 31 bytes", result.stderr)

    def test_32_byte_git_version_is_rejected_before_descriptor_truncation(self) -> None:
        tag = "v" + ("1" * 27) + ".0.0"
        self.assertEqual(32, len(tag))
        subprocess.run(["git", "-C", str(self.repo), "tag", tag], check=True)

        result = self.resolve()

        self.assertNotEqual(0, result.returncode)
        self.assertIn("ESP app descriptor limit is 31 bytes", result.stderr)

    def test_dirty_suffix_cannot_push_git_version_past_descriptor_limit(self) -> None:
        tag = "v" + ("1" * 21) + ".0.0"
        self.assertEqual(26, len(tag))
        subprocess.run(["git", "-C", str(self.repo), "tag", tag], check=True)
        (self.repo / "tracked.txt").write_text("dirty\n")

        result = self.resolve()

        self.assertNotEqual(0, result.returncode)
        self.assertIn("ESP app descriptor limit is 31 bytes", result.stderr)


if __name__ == "__main__":
    unittest.main()
