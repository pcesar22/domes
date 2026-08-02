#!/usr/bin/env python3

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
        cls.verify_script = (ROOT / "scripts/verify.sh").read_text()
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

    def test_software_ci_has_one_stable_aggregate_gate(self) -> None:
        self.assertIn("  pull_request:\n", self.software_workflow)
        self.assertIn("  merge_group:\n", self.software_workflow)
        self.assertIn("    branches: [main]\n", self.software_workflow)
        self.assertIn("    name: CI Gate\n", self.software_workflow)
        self.assertIn(
            "needs: [firmware-build, unit-tests, cli-build, host-tooling, flutter]",
            self.software_workflow,
        )
        self.assertIn('if [[ "$result" != "success" ]]', self.software_workflow)
        self.assertEqual(1, self.software_workflow.count("    name: CI Gate\n"))

        self.assertIn("  workflow_call:\n", self.flutter_workflow)
        self.assertNotIn("  pull_request:\n", self.flutter_workflow)
        self.assertNotIn("  push:\n", self.flutter_workflow)

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

    def test_hardware_ota_proves_version_partition_and_boot_transitions(self) -> None:
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
