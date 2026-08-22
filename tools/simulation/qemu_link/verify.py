#!/usr/bin/env python3
"""Fail-closed ABI, rejection, source-closure, and QEMU patch-budget checks."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
ABI = HERE / "abi.json"
MANIFEST = HERE / "patch_manifest.json"
PATCH = HERE / "patches/0001-domes-link-device.patch"
HEADER = ROOT / "firmware/domes/main/platform/qemu/qemuLinkAbi.hpp"
ADAPTER_HEADER = ROOT / "firmware/domes/main/platform/qemu/qemuEspNowRadio.hpp"


class LinkModel:
    """Small executable oracle for the device's fail-closed state contract."""

    TX_PENDING = 1
    ST_OVERFLOW = 1
    ST_INVALID = 2
    ST_MODEL_FAILURE = 8
    ST_SEQUENCE = 16
    ST_TRUNCATED = 32
    ST_OVERWRITE = 64
    ST_UNKNOWN_VERSION = 128

    def __init__(self) -> None:
        self.sticky = 0
        self.tx_status = 0
        self.tx_length = 0
        self.correlation = 0
        self.rx_ready = False

    def version(self, value: int) -> None:
        if value != 1:
            self.sticky |= self.ST_UNKNOWN_VERSION

    def access(self, offset: int, size: int, write: bool = False) -> None:
        if size != 4 or offset & 3 or offset < 0 or offset >= 0x1000:
            self.sticky |= self.ST_INVALID
        if write and 0x200 <= offset < 0x300:
            self.sticky |= self.ST_INVALID

    def length(self, value: int) -> None:
        if value == 0 or value > 250:
            self.sticky |= self.ST_TRUNCATED
        else:
            self.tx_length = value

    def token(self, value: int) -> None:
        if value == 0:
            self.sticky |= self.ST_SEQUENCE
        else:
            self.correlation = value

    def submit(self) -> None:
        if self.tx_status == self.TX_PENDING:
            self.sticky |= self.ST_OVERWRITE
        elif self.sticky or not self.tx_length or not self.correlation:
            self.sticky |= self.ST_SEQUENCE
        else:
            self.tx_status = self.TX_PENDING

    def complete(self) -> None:
        if self.tx_status != self.TX_PENDING:
            self.sticky |= self.ST_MODEL_FAILURE
        elif self.rx_ready:
            self.sticky |= self.ST_OVERFLOW | self.ST_OVERWRITE
        else:
            self.tx_status = 2
            self.rx_ready = True

    def consume(self) -> None:
        if not self.rx_ready:
            self.sticky |= self.ST_SEQUENCE
        else:
            self.rx_ready = False


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def changed_patch_paths(text: str) -> list[str]:
    return re.findall(r"^diff --git a/(\S+) b/\S+$", text, re.MULTILINE)


def verify(output: Path | None) -> dict[str, object]:
    abi = json.loads(ABI.read_text())
    manifest = json.loads(MANIFEST.read_text())
    header = HEADER.read_text()
    patch = PATCH.read_text()
    failures: list[str] = []

    expected_header_values = {
        "kMmioBase": "0x600D0000U",
        "kMmioWindowSize": "0x1000U",
        "kCapabilityMagic": "0x444C4E4BU",
        "kAbiVersion": "1U",
        "kInterruptSource": "0",
        "kPayloadWindowSize": "0x100U",
    }
    for symbol, value in expected_header_values.items():
        if not re.search(rf"{symbol}\s*=\s*{re.escape(value)}", header):
            failures.append(f"header layout mismatch: {symbol}")
    if abi["maximum_payload"] != 250 or abi["endianness"] != "little":
        failures.append("manifest payload or endianness mismatch")

    paths = changed_patch_paths(patch)
    changed_lines = sum(
        1
        for line in patch.splitlines()
        if line[:1] in {"+", "-"} and not line.startswith(("+++", "---"))
    )
    prohibited = [
        p
        for p in paths
        if any(p.startswith(x) for x in manifest["prohibited_prefixes"])
    ]
    if len(paths) > manifest["maximum_non_generated_files"]:
        failures.append("QEMU patch file budget exceeded")
    if changed_lines > manifest["maximum_changed_lines"]:
        failures.append("QEMU patch line budget exceeded")
    if prohibited:
        failures.append(f"prohibited QEMU paths: {prohibited}")
    if sha256(PATCH) != manifest["patch_sha256"]:
        failures.append("QEMU patch digest mismatch")

    physical_sources = subprocess.run(
        [
            "git",
            "grep",
            "-l",
            "QemuEspNowRadio",
            "--",
            "firmware/domes/main/main.cpp",
            "firmware/domes/main/platform/physical",
            "firmware/domes/main/transport/physicalEspNowRadio.hpp",
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        check=False,
    ).stdout.splitlines()
    if physical_sources:
        failures.append(
            f"QEMU adapter reachable from physical sources: {physical_sources}"
        )
    if (
        '#error "QemuEspNowRadio is available only in the isolated QEMU image"'
        not in ADAPTER_HEADER.read_text()
    ):
        failures.append("adapter compile-time physical-image denial missing")

    rejection_cases: dict[str, bool] = {}
    model = LinkModel()
    model.version(2)
    rejection_cases["unknown_version"] = bool(model.sticky & 128)
    model = LinkModel()
    model.access(1, 4)
    rejection_cases["unaligned"] = bool(model.sticky & 2)
    model = LinkModel()
    model.access(0, 1)
    rejection_cases["narrow_access"] = bool(model.sticky & 2)
    model = LinkModel()
    model.access(0x200, 4, True)
    rejection_cases["rx_write"] = bool(model.sticky & 2)
    model = LinkModel()
    model.length(251)
    rejection_cases["over_length"] = bool(model.sticky & 32)
    model = LinkModel()
    model.submit()
    rejection_cases["sequence"] = bool(model.sticky & 16)
    model = LinkModel()
    model.length(1)
    model.token(1)
    model.submit()
    model.submit()
    rejection_cases["overwrite"] = bool(model.sticky & 64)
    model = LinkModel()
    model.length(1)
    model.token(1)
    model.submit()
    model.rx_ready = True
    model.complete()
    rejection_cases["overflow"] = bool(model.sticky & 1)
    model = LinkModel()
    model.complete()
    rejection_cases["model_failure"] = bool(model.sticky & 8)
    model = LinkModel()
    model.consume()
    rejection_cases["consume_sequence"] = bool(model.sticky & 16)
    if not all(rejection_cases.values()):
        failures.append("one or more rejection cases did not fail closed")

    report = {
        "schema_version": 1,
        "status": "PASS" if not failures else "FAIL",
        "abi_sha256": sha256(ABI),
        "patch_sha256": sha256(PATCH),
        "upstream_revision": manifest["upstream_revision"],
        "changed_paths": paths,
        "non_generated_file_count": len(paths),
        "changed_line_count": changed_lines,
        "prohibited_paths": prohibited,
        "rejection_cases": rejection_cases,
        "physical_source_closure": "denied" if not physical_sources else "reachable",
        "failures": failures,
    }
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if failures:
        raise SystemExit(json.dumps(report, indent=2, sort_keys=True))
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
