#!/usr/bin/env python3
"""Verify firmware metadata and embedded versions against a clean Git checkout."""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

VERSION_PATTERN = re.compile(
    r"[vV]?[0-9]+\.[0-9]+\.[0-9]+(-dirty|-[0-9]+-g[0-9A-Fa-f]+(-dirty)?)?"
)
TAG_PATTERN = "v[0-9]*.[0-9]*.[0-9]*"


class ProvenanceError(ValueError):
    pass


def checked_output(command: list[str]) -> str:
    try:
        return subprocess.run(
            command, check=True, capture_output=True, text=True
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise ProvenanceError(
            f"Required {Path(command[0]).name} command failed"
        ) from error


def source_version(repository: Path, expected_head: str) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", expected_head):
        raise ProvenanceError("Expected head must be a full Git commit SHA")

    def git(*arguments: str) -> str:
        return checked_output(["git", "-C", str(repository), *arguments])

    head = git("rev-parse", "HEAD")
    if head != expected_head.lower():
        raise ProvenanceError("Git HEAD does not match the expected source revision")
    if git("status", "--porcelain", "--untracked-files=no"):
        raise ProvenanceError("Tracked source changes prevent provenance verification")

    described = git("describe", "--tags", "--always", "--dirty", "--match", TAG_PATTERN)
    if not described or described.endswith("-dirty"):
        raise ProvenanceError("Git did not provide a clean firmware version")
    if VERSION_PATTERN.fullmatch(described):
        version = described
    else:
        # Match ResolveFirmwareVersion.cmake when no accepted version tag exists.
        # A failed Git command must never become the source-export fallback.
        short_head = git("rev-parse", "--short=12", "HEAD")
        if not re.fullmatch(r"[0-9a-f]{12,40}", short_head) or not head.startswith(
            short_head
        ):
            raise ProvenanceError("Git did not provide a valid abbreviated source SHA")
        version = f"v0.0.0-0-g{short_head}"
    if len(version.encode("ascii")) > 31:
        raise ProvenanceError("Firmware version exceeds the 31-byte image descriptor")
    return version


def verify_build(build: Path, expected_version: str) -> None:
    try:
        metadata = json.loads((build / "project_description.json").read_text())
    except (OSError, ValueError) as error:
        raise ProvenanceError("Cannot read firmware project metadata") from error
    if (
        not isinstance(metadata, dict)
        or metadata.get("project_version") != expected_version
    ):
        raise ProvenanceError("Project metadata version does not match the source")
    image = build / "domes.bin"
    if not image.is_file() or image.stat().st_size == 0:
        raise ProvenanceError("Firmware application image is missing or empty")
    output = checked_output(
        [
            sys.executable,
            "-m",
            "esptool",
            "--chip",
            "esp32s3",
            "image_info",
            "--version",
            "2",
            str(image),
        ]
    )
    versions = [
        line.strip()
        for line in output.splitlines()
        if line.strip().startswith("App version")
    ]
    if len(versions) != 1 or not versions[0].startswith("App version:"):
        raise ProvenanceError("Image must report exactly one App version field")
    if versions[0].partition(":")[2].strip() != expected_version:
        raise ProvenanceError("Embedded image version does not match the source")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--build", type=Path, action="append", required=True)
    args = parser.parse_args(arguments)
    try:
        version = source_version(args.repository, args.expected_head)
        for build in args.build:
            verify_build(build, version)
    except (ProvenanceError, OSError) as error:
        print(f"Firmware provenance verification failed: {error}", file=sys.stderr)
        return 1
    print(
        json.dumps(
            {
                "source_revision": args.expected_head.lower(),
                "firmware_version": version,
                "verified_builds": len(args.build),
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
