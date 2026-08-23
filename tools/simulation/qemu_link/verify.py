#!/usr/bin/env python3
"""Verify the QEMU link ABI, patched device behavior, runtime trace, and closure."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import socket
import subprocess
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
ABI = HERE / "abi.json"
MANIFEST = HERE / "patch_manifest.json"
PATCH = HERE / "patches/0001-domes-link-device.patch"
HEADER = ROOT / "firmware/domes/main/platform/qemu/qemuLinkAbi.hpp"
ADAPTER_HEADER = ROOT / "firmware/domes/main/platform/qemu/qemuEspNowRadio.hpp"
TRACE_NAMES = ROOT / "tools/trace/trace_names.json"
LEGACY_CAUSAL_NAMES = {
    814375161: "EspNow.RxDispatch",
    1457209874: "EspNow.TxComplete",
    1917337756: "EspNow.RxCallback",
    2454392546: "EspNow.RxQueue",
    3765542678: "EspNow.TxCallback",
}

REGISTER_NAMES = {
    "capability": ("kCapability", "CAPABILITY"),
    "version": ("kVersion", "ABI_VERSION"),
    "max_payload": ("kMaxPayload", "MAX_PAYLOAD"),
    "tx_destination_low": ("kTxDestinationLow", "TX_DEST_LOW"),
    "tx_destination_high": ("kTxDestinationHigh", "TX_DEST_HIGH"),
    "tx_length": ("kTxLength", "TX_LENGTH"),
    "tx_correlation": ("kTxCorrelation", "TX_CORRELATION"),
    "tx_submit": ("kTxSubmit", "TX_SUBMIT"),
    "tx_status": ("kTxStatus", "TX_STATUS"),
    "rx_source_low": ("kRxSourceLow", "RX_SOURCE_LOW"),
    "rx_source_high": ("kRxSourceHigh", "RX_SOURCE_HIGH"),
    "rx_rssi": ("kRxRssi", "RX_RSSI"),
    "rx_length": ("kRxLength", "RX_LENGTH"),
    "rx_correlation": ("kRxCorrelation", "RX_CORRELATION"),
    "rx_consume": ("kRxConsume", "RX_CONSUME"),
    "interrupt_status": ("kInterruptStatus", "IRQ_STATUS"),
    "interrupt_mask": ("kInterruptMask", "IRQ_MASK"),
    "interrupt_ack": ("kInterruptAck", "IRQ_ACK"),
    "sticky_status": ("kStickyStatus", "STICKY_STATUS"),
    "tx_payload": ("kTxPayload", "TX_PAYLOAD"),
    "rx_payload": ("kRxPayload", "RX_PAYLOAD"),
}
INTERRUPT_NAMES = {
    "tx_complete": ("kInterruptTxComplete", "IRQ_TX"),
    "rx_ready": ("kInterruptRxReady", "IRQ_RX"),
}
STICKY_NAMES = {
    "overflow": ("kStickyOverflow", "ST_OVERFLOW"),
    "invalid_access": ("kStickyInvalidAccess", "ST_INVALID"),
    "exhausted": ("kStickyExhausted", "ST_EXHAUSTED"),
    "model_failure": ("kStickyModelFailure", "ST_MODEL_FAILURE"),
    "sequence": ("kStickySequence", "ST_SEQUENCE"),
    "truncated": ("kStickyTruncated", "ST_TRUNCATED"),
    "overwrite": ("kStickyOverwrite", "ST_OVERWRITE"),
    "unknown_version": ("kStickyUnknownVersion", "ST_UNKNOWN_VERSION"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def changed_patch_paths(text: str) -> list[str]:
    return re.findall(r"^diff --git a/(\S+) b/\S+$", text, re.MULTILINE)


def parse_int(value: str) -> int:
    value = value.split("//", 1)[0].strip().rstrip("UuLl")
    bit = re.fullmatch(r"(?:1U?\s*<<|BIT\()\s*(\d+)\)?", value)
    return 1 << int(bit.group(1)) if bit else int(value, 0)


def symbol(text: str, name: str) -> int:
    match = re.search(
        rf"(?:#define\s+{re.escape(name)}\s+|\b{re.escape(name)}\s*=\s*)([^,;\n]+)",
        text,
    )
    if not match:
        raise ValueError(f"missing symbol {name}")
    return parse_int(match.group(1))


class QtestClient:
    """Minimal qtest socket client that exercises the compiled patched device."""

    def __init__(self, binary: Path, extra_args: list[str] | None = None):
        self.temp = tempfile.TemporaryDirectory(prefix="domes-link-qtest-")
        self.socket_path = Path(self.temp.name) / "qtest.sock"
        self.process = subprocess.Popen(
            [
                str(binary),
                "-M",
                "esp32s3",
                "-display",
                "none",
                "-S",
                *(extra_args or []),
                "-qtest",
                f"unix:{self.socket_path},server=on,wait=off",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 5
        while True:
            try:
                self.socket = socket.socket(socket.AF_UNIX)
                self.socket.connect(str(self.socket_path))
                break
            except (FileNotFoundError, ConnectionRefusedError):
                self.socket.close()
                if self.process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("QEMU qtest socket did not become ready")
                time.sleep(0.01)
        self.stream = self.socket.makefile("rwb", buffering=0)

    def close(self) -> None:
        self.stream.close()
        self.socket.close()
        self.process.terminate()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=2)
        self.temp.cleanup()

    def command(self, command: str) -> int | None:
        self.stream.write((command + "\n").encode())
        response = self.stream.readline().decode().strip()
        if not response.startswith("OK"):
            raise RuntimeError(f"qtest command failed: {command}: {response}")
        fields = response.split()
        return int(fields[1], 0) if len(fields) == 2 else None

    def read(self, offset: int) -> int:
        value = self.command(f"readl 0x{0x600D0000 + offset:x}")
        assert value is not None
        return value

    def write(self, offset: int, value: int) -> None:
        self.command(f"writel 0x{0x600D0000 + offset:x} 0x{value:x}")


def qtest_case(binary: Path, operation) -> bool:
    client = QtestClient(binary)
    try:
        return bool(operation(client))
    finally:
        client.close()


def run_qtest_rejections(binary: Path, abi: dict[str, object]) -> dict[str, bool]:
    registers = {name: int(value, 0) for name, value in abi["registers"].items()}
    sticky = abi["sticky_bits"]

    def bit(client: QtestClient, name: str) -> bool:
        return bool(client.read(registers["sticky_status"]) & sticky[name])

    def valid_submit(client: QtestClient, token: int = 1) -> None:
        client.write(registers["tx_destination_low"], 2)
        client.write(registers["tx_destination_high"], 0x200)
        client.write(registers["tx_payload"], 0x444F4D45)
        client.write(registers["tx_length"], 4)
        client.write(registers["tx_correlation"], token)
        client.write(registers["tx_submit"], 1)

    def wait_complete(client: QtestClient) -> bool:
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            if client.read(registers["tx_status"]) == 2:
                return True
            time.sleep(0.005)
        return False

    cases: dict[str, bool] = {}
    cases["unknown_version"] = qtest_case(
        binary,
        lambda c: (
            c.write(registers["version"], abi["abi_version"] + 1),
            bit(c, "unknown_version"),
        )[1],
    )
    cases["invalid_access"] = qtest_case(
        binary, lambda c: (c.read(0x60), bit(c, "invalid_access"))[1]
    )
    cases["over_length"] = qtest_case(
        binary,
        lambda c: (
            c.write(registers["tx_length"], abi["maximum_payload"] + 1),
            bit(c, "truncated"),
        )[1],
    )
    cases["sequence"] = qtest_case(
        binary, lambda c: (c.write(registers["tx_submit"], 1), bit(c, "sequence"))[1]
    )

    def pending_mutation(c: QtestClient) -> bool:
        valid_submit(c)
        c.write(registers["tx_destination_low"], 3)
        return (
            bit(c, "overwrite")
            and bit(c, "exhausted")
            and c.read(registers["tx_destination_low"]) == 2
        )

    cases["in_flight_overwrite"] = qtest_case(binary, pending_mutation)

    def completion_mutation(c: QtestClient) -> bool:
        valid_submit(c)
        if not wait_complete(c):
            return False
        correlation = c.read(registers["tx_correlation"])
        c.write(registers["tx_correlation"], correlation + 1)
        return (
            bit(c, "overwrite")
            and bit(c, "exhausted")
            and c.read(registers["tx_correlation"]) == correlation
        )

    cases["unacknowledged_completion_overwrite"] = qtest_case(
        binary, completion_mutation
    )

    def overflow(c: QtestClient) -> bool:
        valid_submit(c, 1)
        if not wait_complete(c):
            return False
        c.write(registers["interrupt_ack"], abi["interrupt_bits"]["tx_complete"])
        valid_submit(c, 2)
        return wait_complete(c) and bit(c, "overflow") and bit(c, "overwrite")

    cases["rx_overflow"] = qtest_case(binary, overflow)
    cases["rx_write"] = qtest_case(
        binary,
        lambda c: (c.write(registers["rx_payload"], 1), bit(c, "invalid_access"))[1],
    )
    cases["consume_sequence"] = qtest_case(
        binary, lambda c: (c.write(registers["rx_consume"], 1), bit(c, "sequence"))[1]
    )
    return cases


def run_qtest_functional_actor(binary: Path, abi: dict[str, object]) -> dict[str, bool]:
    """Exercise production wire bytes through the in-process QEMU actor model."""
    registers = {name: int(value, 0) for name, value in abi["registers"].items()}
    client = QtestClient(
        binary,
        [
            "-global",
            "domes-link.scenario-model=1",
            "-global",
            "domes-link.dut-role=1",
        ],
    )

    def submit(payload: bytes, token: int) -> None:
        client.write(registers["tx_destination_low"], 2)
        client.write(registers["tx_destination_high"], 0x200)
        for offset in range(0, len(payload), 4):
            client.write(
                registers["tx_payload"] + offset,
                int.from_bytes(payload[offset : offset + 4], "little"),
            )
        client.write(registers["tx_length"], len(payload))
        client.write(registers["tx_correlation"], token)
        client.write(registers["tx_submit"], 1)

    def wait_for(expected_status: int) -> bool:
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            if client.read(registers["tx_status"]) == expected_status:
                return True
            time.sleep(0.005)
        return False

    cases: dict[str, bool] = {}
    try:
        mac = bytes((2, 0, 0, 0, 0, 1))
        ping = bytes((2,)) + mac + (1).to_bytes(4, "little")
        submit(ping, 41)
        cases["ping_to_pong"] = (
            wait_for(2)
            and client.read(registers["rx_length"]) == 11
            and (client.read(registers["rx_payload"]) & 0xFF) == 3
            and client.read(registers["rx_correlation"]) == 41
        )
        client.write(registers["rx_consume"], 1)
        client.write(registers["interrupt_ack"], abi["interrupt_bits"]["tx_complete"])

        arm = (
            bytes((0x11,))
            + mac
            + (2).to_bytes(4, "little")
            + (7).to_bytes(4, "little")
            + (3000).to_bytes(4, "little")
            + bytes((3,))
        )
        submit(arm, 42)
        cases["arm_to_touch_event"] = (
            wait_for(2)
            and client.read(registers["rx_length"]) == 20
            and (client.read(registers["rx_payload"]) & 0xFF) == 0x20
            and (client.read(registers["rx_payload"] + 8) >> 24) == 7
            and client.read(registers["rx_correlation"]) == 42
        )
        client.write(registers["rx_consume"], 1)
        client.write(registers["interrupt_ack"], abi["interrupt_bits"]["tx_complete"])

        submit(bytes((0xFF,)) + mac + (3).to_bytes(4, "little"), 43)
        cases["unexpected_wire_fails_closed"] = wait_for(3) and bool(
            client.read(registers["sticky_status"])
            & abi["sticky_bits"]["model_failure"]
        )
    finally:
        client.close()

    def run_slave_role() -> bool:
        slave = QtestClient(
            binary,
            [
                "-global",
                "domes-link.scenario-model=1",
                "-global",
                "domes-link.dut-role=2",
            ],
        )
        try:
            payload = bytes((2,)) + mac + (4).to_bytes(4, "little")
            for offset in range(0, len(payload), 4):
                slave.write(
                    registers["tx_payload"] + offset,
                    int.from_bytes(payload[offset : offset + 4], "little"),
                )
            slave.write(registers["tx_destination_low"], 2)
            slave.write(registers["tx_destination_high"], 0x200)
            slave.write(registers["tx_length"], len(payload))
            slave.write(registers["tx_correlation"], 51)
            slave.write(registers["tx_submit"], 1)
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                if slave.read(registers["tx_status"]) == 2:
                    return (
                        slave.read(registers["rx_length"]) == 11
                        and (slave.read(registers["rx_payload"]) & 0xFF) == 3
                    )
                time.sleep(0.005)
            return False
        finally:
            slave.close()

    def run_distinct_queue_overflow() -> bool:
        queued = QtestClient(
            binary,
            [
                "-global",
                "domes-link.scenario-model=1",
                "-global",
                "domes-link.dut-role=1",
                "-global",
                "domes-link.peer-delay-ns=1000000000",
            ],
        )
        try:
            payload = bytes((2,)) + mac + (5).to_bytes(4, "little")
            for token in range(1, 10):
                queued.write(registers["tx_destination_low"], 2)
                queued.write(registers["tx_destination_high"], 0x200)
                for offset in range(0, len(payload), 4):
                    queued.write(
                        registers["tx_payload"] + offset,
                        int.from_bytes(payload[offset : offset + 4], "little"),
                    )
                queued.write(registers["tx_length"], len(payload))
                queued.write(registers["tx_correlation"], token)
                queued.write(registers["tx_submit"], 1)
                expected = 3 if token == 9 else 2
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline:
                    if queued.read(registers["tx_status"]) == expected:
                        break
                    time.sleep(0.005)
                else:
                    return False
                if token < 9:
                    queued.write(
                        registers["interrupt_ack"],
                        abi["interrupt_bits"]["tx_complete"],
                    )
            sticky = queued.read(registers["sticky_status"])
            return bool(
                sticky & abi["sticky_bits"]["overflow"]
                and sticky & abi["sticky_bits"]["model_failure"]
            )
        finally:
            queued.close()

    cases["slave_role_ping_to_pong"] = run_slave_role()
    cases["distinct_event_queue_overflow_fails_closed"] = run_distinct_queue_overflow()
    return cases


def validate_runtime_log(path: Path) -> dict[str, object]:
    text = path.read_text(errors="replace")
    names = {
        int(key): value for key, value in json.loads(TRACE_NAMES.read_text()).items()
    }
    event_pattern = re.compile(
        r"DOMES_QEMU_LINK_TRACE schema=1 index=(\d+) timestamp=(\d+) task=(\d+) "
        r"type=(\d+) arg1=(\d+) token=(\d+)"
    )
    events = [
        {
            "index": int(i),
            "task": int(task),
            "type": int(kind),
            "name": names.get(int(arg1), LEGACY_CAUSAL_NAMES.get(int(arg1))),
            "token": int(token),
        }
        for i, _timestamp, task, kind, arg1, token in event_pattern.findall(text)
    ]

    def first_index(predicate, after: int = -1):
        return next(
            (
                event["index"]
                for event in events
                if event["index"] > after and predicate(event)
            ),
            None,
        )

    result = re.search(
        r"DOMES_QEMU_LINK_RESULT schema=2 status=(PASS|FAIL) failure_mask=(0x[0-9a-f]+) "
        r"token=(\d+) service_dispatches=(\d+) trace_drops=(\d+) trace_discontinuities=(\d+)",
        text,
    )
    if not result:
        raise ValueError("missing schema-2 QEMU link result")
    token = int(result.group(3))
    token_events = [event for event in events if event["token"] == token]
    mmio = first_index(
        lambda event: event["token"] == token and event["name"] == "QemuLink.MmioSubmit"
    )
    task_handoff = first_index(
        lambda event: event["token"] == token
        and event["name"] == "QemuLink.TaskHandoff",
        mmio if mmio is not None else -1,
    )
    callback_entries = [event["index"] for event in token_events if event["type"] == 28]
    callback_exits = [event["index"] for event in token_events if event["type"] == 29]
    tx_callback = callback_entries[0] if callback_entries else None
    tx_complete = callback_exits[0] if callback_exits else None
    rx_callback = callback_entries[1] if len(callback_entries) > 1 else None
    rx_queue = first_index(
        lambda event: event["token"] == token
        and event["type"] == 25
        and event["name"] in {"EspNow.RxQueue", "EspNow.CausalQueue"},
        rx_callback if rx_callback is not None else -1,
    )
    rx_dispatch = first_index(
        lambda event: event["token"] == token
        and (
            event["name"] == "EspNow.RxDispatch"
            or (event["name"] == "EspNow.Complete" and event["type"] == 30)
        ),
        rx_queue if rx_queue is not None else -1,
    )
    service_dispatch = first_index(
        lambda event: event["token"] == token
        and event["name"] == "QemuLink.ServiceDispatch",
        rx_dispatch if rx_dispatch is not None else -1,
    )
    causal_names = [
        "QemuLink.MmioSubmit",
        "QemuLink.TaskHandoff",
        "EspNow.TxCallback",
        "EspNow.TxComplete",
        "EspNow.RxCallback",
        "EspNow.RxQueue",
        "EspNow.RxDispatch",
        "QemuLink.ServiceDispatch",
    ]
    positions = dict(
        zip(
            causal_names,
            (
                mmio,
                task_handoff,
                tx_callback,
                tx_complete,
                rx_callback,
                rx_queue,
                rx_dispatch,
                service_dispatch,
            ),
            strict=True,
        )
    )
    isr = [
        event
        for event in events
        if event["type"] in (22, 23) and event["token"] == token
    ]
    callbacks = [
        event
        for event in events
        if event["type"] in (28, 29) and event["token"] == token
    ]
    stages = {
        "mmio": positions["QemuLink.MmioSubmit"] is not None,
        "irq": any(event["type"] == 22 for event in isr)
        and any(event["type"] == 23 for event in isr),
        "task": positions["QemuLink.TaskHandoff"] is not None,
        "callback": len(callbacks) >= 4,
        "ring": positions["EspNow.RxQueue"] is not None,
        "semaphore": any(
            event["type"] == 13 for event in events if event["token"] == token
        )
        and any(event["type"] == 12 for event in events if event["token"] == token),
        "dequeue": positions["EspNow.RxDispatch"] is not None,
        "service_dispatch": any(event["name"] == "EspNow.RxBeacon" for event in events)
        and positions["QemuLink.ServiceDispatch"] is not None,
        "tx_complete": positions["EspNow.TxComplete"] is not None,
    }
    ordered = [positions[name] for name in causal_names if positions[name] is not None]
    passed = (
        result.group(1) == "PASS"
        and int(result.group(2), 0) == 0
        and int(result.group(4)) > 0
        and int(result.group(5)) == 0
        and int(result.group(6)) == 0
        and all(stages.values())
        and ordered == sorted(ordered)
    )
    return {
        "status": "PASS" if passed else "FAIL",
        "token": token,
        "stages": stages,
        "positions": positions,
        "event_count": len(events),
    }


def verify(
    output: Path | None, qemu_binary: Path | None, runtime_log: Path | None
) -> dict[str, object]:
    abi = json.loads(ABI.read_text())
    manifest = json.loads(MANIFEST.read_text())
    header = HEADER.read_text()
    patch = PATCH.read_text()
    failures: list[str] = []

    try:
        scalar_checks = {
            "mmio_base": (int(abi["mmio_base"], 0), symbol(header, "kMmioBase")),
            "mmio_window_size": (
                abi["mmio_window_size"],
                symbol(header, "kMmioWindowSize"),
            ),
            "capability_magic": (
                int(abi["capability_magic"], 0),
                symbol(header, "kCapabilityMagic"),
            ),
            "abi_version": (abi["abi_version"], symbol(header, "kAbiVersion")),
            "interrupt_source": (
                abi["interrupt_source"],
                symbol(header, "kInterruptSource"),
            ),
            "payload_window_size": (
                abi["payload_window_size"],
                symbol(header, "kPayloadWindowSize"),
            ),
            "maximum_payload": (
                abi["maximum_payload"],
                symbol(patch, "DOMES_LINK_MAX_PAYLOAD"),
            ),
        }
        for name, values in scalar_checks.items():
            if values[0] != values[1]:
                failures.append(f"ABI scalar mismatch: {name}: {values}")
        for manifest_name, (header_name, patch_name) in REGISTER_NAMES.items():
            expected = int(abi["registers"][manifest_name], 0)
            if expected != symbol(header, header_name) or expected != symbol(
                patch, patch_name
            ):
                failures.append(f"ABI register mismatch: {manifest_name}")
        for manifest_name, (header_name, patch_name) in INTERRUPT_NAMES.items():
            expected = abi["interrupt_bits"][manifest_name]
            if expected != symbol(header, header_name) or expected != symbol(
                patch, patch_name
            ):
                failures.append(f"ABI interrupt mismatch: {manifest_name}")
        for manifest_name, (header_name, patch_name) in STICKY_NAMES.items():
            expected = abi["sticky_bits"][manifest_name]
            if expected != symbol(header, header_name) or expected != symbol(
                patch, patch_name
            ):
                failures.append(f"ABI sticky mismatch: {manifest_name}")
        if abi["endianness"] != "little" or "DEVICE_LITTLE_ENDIAN" not in patch:
            failures.append("ABI endianness mismatch")
        mmio_mapping = (
            f"memory_region_add_subregion_overlap(sys_mem, {abi['mmio_base']}".upper()
        )
        if mmio_mapping not in patch.upper():
            failures.append("QEMU MMIO base mismatch")
        if abi["interrupt_name"] not in patch:
            failures.append("QEMU interrupt source mismatch")
    except (KeyError, ValueError) as error:
        failures.append(f"ABI cross-check failed: {error}")

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

    qtest = None
    actor_qtest = None
    if qemu_binary:
        qtest = run_qtest_rejections(qemu_binary.resolve(), abi)
        if not all(qtest.values()):
            failures.append("one or more patched-device qtest rejections failed")
        actor_qtest = run_qtest_functional_actor(qemu_binary.resolve(), abi)
        if not all(actor_qtest.values()):
            failures.append("one or more functional-actor qtest cases failed")
    runtime = None
    if runtime_log:
        runtime = validate_runtime_log(runtime_log)
        if runtime["status"] != "PASS":
            failures.append("QEMU runtime trace validation failed")

    report = {
        "schema_version": 2,
        "status": "PASS" if not failures else "FAIL",
        "abi_sha256": sha256(ABI),
        "patch_sha256": sha256(PATCH),
        "upstream_revision": manifest["upstream_revision"],
        "changed_paths": paths,
        "non_generated_file_count": len(paths),
        "changed_line_count": changed_lines,
        "prohibited_paths": prohibited,
        "qtest_rejection_cases": qtest,
        "qtest_functional_actor_cases": actor_qtest,
        "runtime_trace": runtime,
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
    parser.add_argument("--qemu-binary", type=Path)
    parser.add_argument("--runtime-log", type=Path)
    args = parser.parse_args()
    print(
        json.dumps(
            verify(args.output, args.qemu_binary, args.runtime_log),
            indent=2,
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
