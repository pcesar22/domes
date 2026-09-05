#!/usr/bin/env python3

import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.ci import verify_firmware_provenance as provenance


class FirmwareProvenanceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repository = self.root / "repository"
        self.repository.mkdir()
        self.environment = os.environ.copy()
        self.environment.update(
            GIT_AUTHOR_NAME="Firmware Test",
            GIT_AUTHOR_EMAIL="firmware@example.invalid",
            GIT_COMMITTER_NAME="Firmware Test",
            GIT_COMMITTER_EMAIL="firmware@example.invalid",
        )
        self.git("init", "-q")
        self.tracked = self.repository / "source.txt"
        self.tracked.write_text("source\n")
        self.git("add", "source.txt")
        self.git("commit", "-q", "-m", "Source fixture")
        self.head = self.git("rev-parse", "HEAD")
        self.run_process = subprocess.run

    def git(self, *arguments: str) -> str:
        return subprocess.run(
            ["git", "-C", str(self.repository), *arguments],
            env=self.environment,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

    def make_build(self, name: str, version: str) -> Path:
        build = self.root / name
        build.mkdir()
        (build / "project_description.json").write_text(
            json.dumps({"project_version": version})
        )
        (build / "domes.bin").write_bytes(b"image fixture")
        return build

    def image_tool(self, output: str):
        def run(command, **kwargs):
            if command[:3] == [sys.executable, "-m", "esptool"]:
                self.assertEqual(
                    command[3:-1],
                    ["--chip", "esp32s3", "image_info", "--version", "2"],
                )
                self.assertTrue(kwargs["check"])
                return subprocess.CompletedProcess(command, 0, output, "")
            return self.run_process(command, **kwargs)

        return patch.object(provenance.subprocess, "run", side_effect=run)

    def test_tagged_and_untagged_clean_versions_match_git(self) -> None:
        self.assertEqual(
            f"v0.0.0-0-g{self.head[:12]}",
            provenance.source_version(self.repository, self.head),
        )
        self.git("tag", "v1.2.3")
        self.assertEqual(
            "v1.2.3", provenance.source_version(self.repository, self.head)
        )
        self.tracked.write_text("next source\n")
        self.git("commit", "-q", "-am", "Next fixture")
        head = self.git("rev-parse", "HEAD")
        self.assertEqual(
            self.git(
                "describe",
                "--tags",
                "--always",
                "--dirty",
                "--match",
                provenance.TAG_PATTERN,
            ),
            provenance.source_version(self.repository, head),
        )

    def test_tracked_dirty_source_is_rejected_but_untracked_output_is_allowed(
        self,
    ) -> None:
        (self.repository / "untracked-output.txt").write_text("output\n")
        provenance.source_version(self.repository, self.head)
        self.tracked.write_text("modified source\n")
        with self.assertRaisesRegex(provenance.ProvenanceError, "Tracked source"):
            provenance.source_version(self.repository, self.head)

    def test_wrong_or_abbreviated_expected_head_is_rejected(self) -> None:
        for expected in ("0" * 40, self.head[:12], ""):
            with self.subTest(expected=expected):
                with self.assertRaises(provenance.ProvenanceError):
                    provenance.source_version(self.repository, expected)

    def test_unavailable_git_cannot_produce_a_fallback(self) -> None:
        with patch.dict(os.environ, {"PATH": str(self.root / "no-executables")}):
            with self.assertRaisesRegex(provenance.ProvenanceError, "command failed"):
                provenance.source_version(self.repository, self.head)

    def test_each_git_command_failure_is_fatal(self) -> None:
        for failure in ("head", "status", "describe", "short"):
            with self.subTest(failure=failure):

                def run(command, **kwargs):
                    arguments = command[3:]
                    selected = {
                        "head": arguments == ["rev-parse", "HEAD"],
                        "status": arguments[0] == "status",
                        "describe": arguments[0] == "describe",
                        "short": "--short=12" in arguments,
                    }
                    if selected[failure]:
                        raise subprocess.CalledProcessError(128, command)
                    return self.run_process(command, **kwargs)

                with patch.object(provenance.subprocess, "run", side_effect=run):
                    with self.assertRaisesRegex(
                        provenance.ProvenanceError, "command failed"
                    ):
                        provenance.source_version(self.repository, self.head)

    def test_dirty_empty_and_overlong_git_versions_are_rejected(self) -> None:
        for version in ("", "v0.0.0-dirty", "v1.2.3-dirty", "v" + "1" * 27 + ".0.0"):
            with self.subTest(version=version):

                def run(command, **kwargs):
                    if command[3] == "describe":
                        return subprocess.CompletedProcess(command, 0, version, "")
                    return self.run_process(command, **kwargs)

                with patch.object(provenance.subprocess, "run", side_effect=run):
                    with self.assertRaises(provenance.ProvenanceError):
                        provenance.source_version(self.repository, self.head)

    def test_metadata_mismatch_empty_and_fallback_dirty_fail_before_image_tool(
        self,
    ) -> None:
        for index, version in enumerate(("v9.9.9", "", "v0.0.0-dirty")):
            with self.subTest(version=version):
                build = self.make_build(str(index), version)
                with patch.object(provenance.subprocess, "run") as run:
                    with self.assertRaisesRegex(
                        provenance.ProvenanceError, "metadata version"
                    ):
                        provenance.verify_build(build, "v1.2.3")
                    run.assert_not_called()

    def test_image_version_must_be_single_nonempty_and_exact(self) -> None:
        build = self.make_build("build", "v1.2.3")
        outputs = (
            "App version: v9.9.9\n",
            "App version: v0.0.0-dirty\n",
            "App version: \n",
            "No application descriptor\n",
            "App version: v1.2.3\nApp version: v1.2.3\n",
            "App version: v1.2.3\nApp version : malformed\n",
        )
        for output in outputs:
            with self.subTest(output=output), self.image_tool(output):
                with self.assertRaises(provenance.ProvenanceError):
                    provenance.verify_build(build, "v1.2.3")

    def test_failed_image_inspection_is_fatal(self) -> None:
        build = self.make_build("build", "v1.2.3")
        with patch.object(
            provenance.subprocess,
            "run",
            side_effect=subprocess.CalledProcessError(2, "esptool"),
        ):
            with self.assertRaisesRegex(provenance.ProvenanceError, "command failed"):
                provenance.verify_build(build, "v1.2.3")

    def test_untagged_build_matches_checked_source_version(self) -> None:
        version = f"v0.0.0-0-g{self.head[:12]}"
        build = self.make_build("untagged", version)
        with self.image_tool(f"App version: {version}\n"):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(
                    0,
                    provenance.main(
                        [
                            "--repository",
                            str(self.repository),
                            "--expected-head",
                            self.head,
                            "--build",
                            str(build),
                        ]
                    ),
                )

    def test_cli_checks_every_requested_build(self) -> None:
        self.git("tag", "v1.2.3")
        physical = self.make_build("physical", "v1.2.3")
        qemu = self.make_build("qemu", "v1.2.3")
        args = [
            "--repository",
            str(self.repository),
            "--expected-head",
            self.head,
            "--build",
            str(physical),
            "--build",
            str(qemu),
        ]
        output = io.StringIO()
        with self.image_tool("Image information\nApp version: v1.2.3\n"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(0, provenance.main(args))
        self.assertEqual(2, json.loads(output.getvalue())["verified_builds"])
        (qemu / "project_description.json").write_text('{"project_version":"v9.9.9"}')
        with self.image_tool("App version: v1.2.3\n"):
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(1, provenance.main(args))


if __name__ == "__main__":
    unittest.main()
