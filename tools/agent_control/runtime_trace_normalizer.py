#!/usr/bin/env python3
"""Fail-closed normalization for bounded production-runtime trace captures."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from typing import Any, Mapping

EVENT_SIZE = 16
FORMAT_VERSION = 1
NORMALIZER_VERSION = "1.0.0"
MAX_TRACE_DUMP_BYTES = 32 * 1024


class RuntimeTraceError(ValueError):
    pass


def _enum(proto: Path, name: str) -> dict[str, int]:
    text = proto.read_text(encoding="utf-8")
    match = re.search(rf"enum\s+{re.escape(name)}\s*\{{(.*?)\n\}}", text, re.DOTALL)
    if match is None:
        raise RuntimeTraceError(f"trace schema omits enum {name}")
    values = {
        key: int(value, 0)
        for key, value in re.findall(
            r"^\s*([A-Z][A-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;",
            match.group(1),
            re.MULTILINE,
        )
    }
    if not values:
        raise RuntimeTraceError(f"trace schema enum {name} has no values")
    return values


def _canonical(value: Mapping[str, Any]) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode() + b"\n"


def _timestamp_order(timestamps: list[int]) -> None:
    wraps = 0
    for previous, current in zip(timestamps, timestamps[1:]):
        delta = (current - previous) & 0xFFFF_FFFF
        if delta > 0x7FFF_FFFF:
            raise RuntimeTraceError("trace timestamps regress")
        if current < previous:
            wraps += 1
            if wraps > 1:
                raise RuntimeTraceError("trace timestamps wrap more than once")


def _catalog(session: Mapping[str, Any]) -> tuple[dict[int, dict[str, Any]], dict[int, dict[str, Any]]]:
    tasks: dict[int, dict[str, Any]] = {}
    task_names: set[str] = set()
    raw_tasks = session.get("tasks")
    if not isinstance(raw_tasks, list):
        raise RuntimeTraceError("session task catalog is missing")
    for item in raw_tasks:
        if not isinstance(item, dict):
            raise RuntimeTraceError("session task catalog is malformed")
        task_id = int(item.get("task_id", 0))
        name = str(item.get("name", ""))
        priority = int(item.get("priority", -1))
        mask = int(item.get("core_affinity_mask", 0))
        if (
            task_id <= 0
            or task_id > 31
            or task_id in tasks
            or not name
            or len(name.encode()) >= 16
            or name in task_names
            or not 0 <= priority <= 0xFF
            or mask not in (1, 2, 3)
        ):
            raise RuntimeTraceError("session has duplicate or invalid task mappings")
        task_names.add(name)
        tasks[task_id] = {
            "name": name,
            "priority": priority,
            "core_affinity": {1: 0, 2: 1, 3: -1}[mask],
        }

    objects: dict[int, dict[str, Any]] = {}
    object_names: set[str] = set()
    raw_objects = session.get("objects")
    if not isinstance(raw_objects, list):
        raise RuntimeTraceError("session object catalog is missing")
    for item in raw_objects:
        if not isinstance(item, dict):
            raise RuntimeTraceError("session object catalog is malformed")
        object_id = int(item.get("object_id", 0))
        kind = int(item.get("kind", 0))
        name = str(item.get("name", ""))
        if (
            object_id <= 0
            or object_id in objects
            or kind <= 0
            or not name
            or len(name.encode()) >= 16
            or name in object_names
        ):
            raise RuntimeTraceError("session has duplicate or invalid object mappings")
        object_names.add(name)
        objects[object_id] = {"kind": kind, "name": name}
    return tasks, objects


def _complete_chains(
    events: list[dict[str, Any]], expected: list[tuple[int, int]], kind: str
) -> list[dict[str, Any]]:
    by_token: dict[int, list[dict[str, Any]]] = {}
    expected_ids = {identifier for _event_type, identifier in expected}
    for event in events:
        token = event["arg2"]
        if token and event["arg1"] in expected_ids:
            by_token.setdefault(token, []).append(event)
    complete: list[dict[str, Any]] = []
    for token, candidates in by_token.items():
        cursor = 0
        positions: list[int] = []
        for event in candidates:
            if cursor < len(expected) and (
                event["type"], event["arg1"]
            ) == expected[cursor]:
                positions.append(event["sequence"])
                cursor += 1
        if cursor == len(expected):
            complete.append(
                {
                    "kind": kind,
                    "token": token,
                    "positions": positions,
                }
            )
    return sorted(complete, key=lambda chain: chain["positions"][0])


def normalize_runtime(
    raw: bytes, session: Mapping[str, Any], proto: Path, trace_names: Path
) -> tuple[dict[str, Any], dict[str, Any]]:
    if not raw or len(raw) % EVENT_SIZE or len(raw) > MAX_TRACE_DUMP_BYTES:
        raise RuntimeTraceError("raw trace size is invalid")
    if session.get("format_version") != FORMAT_VERSION:
        raise RuntimeTraceError("session format version is unsupported")
    if session.get("integrity_error") is not None:
        raise RuntimeTraceError("session records a trace integrity error")
    if session.get("event_count") != len(raw) // EVENT_SIZE:
        raise RuntimeTraceError("session event count does not match raw evidence")
    if session.get("received_raw_bytes") != len(raw):
        raise RuntimeTraceError("session byte count does not match raw evidence")
    if session.get("dropped_count") != 0 or session.get("discontinuity_count") != 0:
        raise RuntimeTraceError("trace overflow or discontinuity invalidates evidence")
    raw_sha256 = hashlib.sha256(raw).hexdigest()
    if session.get("raw_sha256") != raw_sha256:
        raise RuntimeTraceError("session hash does not match raw evidence")

    event_types = _enum(proto, "EventType")
    known_types = set(event_types.values()) - {event_types["EVENT_TYPE_UNKNOWN"]}
    known_categories = set(_enum(proto, "Category").values())
    names_document = json.loads(trace_names.read_text(encoding="utf-8"))
    if not isinstance(names_document, dict) or not all(
        isinstance(key, str) and key.isdigit() and isinstance(value, str) and value
        for key, value in names_document.items()
    ):
        raise RuntimeTraceError("trace name map is malformed")
    names = {int(key): value for key, value in names_document.items()}
    reverse_names = {value: key for key, value in names.items()}
    required_names = {
        "EspNow.TxSubmit",
        "EspNow.TxCallback",
        "EspNow.TxComplete",
        "EspNow.RxCallback",
        "EspNow.RxQueue",
        "EspNow.RxReady",
        "EspNow.RxDispatch",
    }
    if not required_names <= set(reverse_names):
        raise RuntimeTraceError("trace name map omits ESP-NOW correlation boundaries")

    tasks, objects = _catalog(session)
    events: list[dict[str, Any]] = []
    for sequence, offset in enumerate(range(0, len(raw), EVENT_SIZE)):
        timestamp, task_id, event_type, flags, arg1, arg2 = struct.unpack_from(
            "<IHBBII", raw, offset
        )
        core_marker = flags & 0x03
        context = (flags >> 2) & 0x03
        category = (flags >> 4) & 0x0F
        if (
            event_type not in known_types
            or category not in known_categories
            or core_marker not in (1, 2)
            or context > 2
            or (task_id != 0 and task_id not in tasks)
        ):
            raise RuntimeTraceError(
                f"event {sequence} has an unresolved type, context, core, category, or task"
            )
        events.append(
            {
                "sequence": sequence,
                "timestamp_us": timestamp,
                "task_id": task_id,
                "task": tasks.get(task_id, {}).get("name"),
                "type": event_type,
                "type_name": next(
                    name for name, value in event_types.items() if value == event_type
                ),
                "category": category,
                "core": core_marker - 1,
                "context": context,
                "arg1": arg1,
                "arg1_name": names.get(arg1),
                "arg2": arg2,
            }
        )
    _timestamp_order([event["timestamp_us"] for event in events])
    if (
        int(session.get("start_timestamp_us", -1)) != events[0]["timestamp_us"]
        or int(session.get("end_timestamp_us", -1)) != events[-1]["timestamp_us"]
    ):
        raise RuntimeTraceError("session timestamps do not bound raw evidence")

    tx_expected = [
        (event_types["EVENT_TYPE_SCHED_QUEUE_SEND"], reverse_names["EspNow.TxSubmit"]),
        (event_types["EVENT_TYPE_CALLBACK_BEGIN"], reverse_names["EspNow.TxCallback"]),
        (event_types["EVENT_TYPE_CALLBACK_END"], reverse_names["EspNow.TxCallback"]),
        (event_types["EVENT_TYPE_CAUSAL_COMPLETE"], reverse_names["EspNow.TxComplete"]),
    ]
    rx_expected = [
        (event_types["EVENT_TYPE_CALLBACK_BEGIN"], reverse_names["EspNow.RxCallback"]),
        (event_types["EVENT_TYPE_SCHED_QUEUE_SEND"], reverse_names["EspNow.RxQueue"]),
        (event_types["EVENT_TYPE_SEM_GIVE"], reverse_names["EspNow.RxReady"]),
        (event_types["EVENT_TYPE_CALLBACK_END"], reverse_names["EspNow.RxCallback"]),
        (event_types["EVENT_TYPE_SEM_TAKE"], reverse_names["EspNow.RxReady"]),
        (event_types["EVENT_TYPE_SCHED_QUEUE_RECEIVE"], reverse_names["EspNow.RxQueue"]),
        (event_types["EVENT_TYPE_CAUSAL_COMPLETE"], reverse_names["EspNow.RxDispatch"]),
    ]
    tx_chains = _complete_chains(events, tx_expected, "tx")
    rx_chains = _complete_chains(events, rx_expected, "rx")
    if not tx_chains or not rx_chains:
        raise RuntimeTraceError(
            "runtime trace lacks complete ESP-NOW TX and RX correlation chains"
        )

    first_timestamp = events[0]["timestamp_us"]
    duration_us = (events[-1]["timestamp_us"] - first_timestamp) & 0xFFFF_FFFF
    for event in events:
        event["relative_us"] = (
            event.pop("timestamp_us") - first_timestamp
        ) & 0xFFFF_FFFF
    replay: dict[str, Any] = {
        "artifact_kind": "replay-normalized-runtime-trace",
        "format_version": FORMAT_VERSION,
        "event_size": EVENT_SIZE,
        "normalizer": {
            "name": "domes-runtime-trace-normalizer",
            "version": NORMALIZER_VERSION,
            "input_schema": "domes.trace.TraceEvent/le16/v1",
            "output_schema": "domes.trace.runtime-replay/v1",
        },
        "tasks": {str(key): tasks[key] for key in sorted(tasks)},
        "objects": {str(key): objects[key] for key in sorted(objects)},
        "dropped_count": 0,
        "discontinuity_count": 0,
        "duration_us": duration_us,
        "correlation": {
            "tx_complete_count": len(tx_chains),
            "rx_complete_count": len(rx_chains),
            "tx_chains": tx_chains,
            "rx_chains": rx_chains,
        },
        "events": events,
        "raw_sha256": raw_sha256,
    }
    replay["normalized_sha256"] = hashlib.sha256(_canonical(replay)).hexdigest()
    semantic = {
        "artifact_kind": "runtime-correlation-semantic-projection",
        "format_version": FORMAT_VERSION,
        "normalizer": {
            **replay["normalizer"],
            "output_schema": "domes.trace.runtime-correlation/v1",
        },
        "raw_sha256": raw_sha256,
        "event_count": len(events),
        "duration_us": duration_us,
        "tx_complete_count": len(tx_chains),
        "rx_complete_count": len(rx_chains),
        "tx_positions": [chain["positions"] for chain in tx_chains],
        "rx_positions": [chain["positions"] for chain in rx_chains],
    }
    return replay, semantic


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--session", type=Path, required=True)
    parser.add_argument("--trace-proto", type=Path, required=True)
    parser.add_argument("--trace-names", type=Path, required=True)
    parser.add_argument("--output-prefix", type=Path, required=True)
    args = parser.parse_args()
    replay, semantic = normalize_runtime(
        args.raw.read_bytes(),
        json.loads(args.session.read_text(encoding="utf-8")),
        args.trace_proto,
        args.trace_names,
    )
    args.output_prefix.with_suffix(".replay.json").write_bytes(_canonical(replay))
    args.output_prefix.with_suffix(".semantic.json").write_bytes(_canonical(semantic))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
