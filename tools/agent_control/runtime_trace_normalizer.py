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
NORMALIZER_VERSION = "1.1.0"
MAX_TRACE_DUMP_BYTES = 32 * 1024
ESP_NOW_TRACE_NAMES = {
    0x59FB7823: "EspNow.CausalQueue",
    0xEF2DD8BB: "EspNow.CausalReady",
    0x35800DA2: "EspNow.Callback",
    0xF1F4511E: "EspNow.Complete",
}
ESP_NOW_OBJECT_NAMES = {
    0x59FB7823: "espnow_queue",
    0xEF2DD8BB: "espnow_ready",
    0x35800DA2: "espnow_cb",
    0xF1F4511E: "espnow_done",
}


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


def _catalog(
    session: Mapping[str, Any],
) -> tuple[dict[int, dict[str, Any]], dict[int, dict[str, Any]]]:
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


def _validate_scheduler_contract(
    events: list[dict[str, Any]], event_types: Mapping[str, int]
) -> None:
    allowed_contexts = {
        event_types["EVENT_TYPE_SCHED_QUEUE_SEND"]: {0, 1, 2},
        event_types["EVENT_TYPE_SCHED_QUEUE_RECEIVE"]: {0, 1, 2},
        event_types["EVENT_TYPE_CALLBACK_BEGIN"]: {2},
        event_types["EVENT_TYPE_CALLBACK_END"]: {2},
        event_types["EVENT_TYPE_CAUSAL_COMPLETE"]: {0},
        event_types["EVENT_TYPE_SEM_TAKE"]: {0, 1, 2},
        event_types["EVENT_TYPE_SEM_GIVE"]: {0, 1, 2},
    }
    taskless = {
        event_types["EVENT_TYPE_CALLBACK_BEGIN"],
        event_types["EVENT_TYPE_CALLBACK_END"],
    }
    synchronized = {
        event_types["EVENT_TYPE_SCHED_QUEUE_SEND"],
        event_types["EVENT_TYPE_SCHED_QUEUE_RECEIVE"],
        event_types["EVENT_TYPE_SEM_TAKE"],
        event_types["EVENT_TYPE_SEM_GIVE"],
    }
    for event in events:
        allowed = allowed_contexts.get(event["type"])
        if allowed is None:
            continue
        if event["context"] not in allowed or event["category"] != 0:
            raise RuntimeTraceError(
                "ESP-NOW scheduler boundary has an invalid context or category"
            )
        if event["type"] in taskless and event["task_id"] != 0:
            raise RuntimeTraceError(
                "ESP-NOW callback boundary unexpectedly owns a task"
            )
        if event["type"] in synchronized and (
            (event["context"] == 0 and event["task_id"] == 0)
            or (event["context"] != 0 and event["task_id"] != 0)
        ):
            raise RuntimeTraceError(
                "ESP-NOW synchronization boundary has invalid task ownership"
            )


def _correlation_chains(
    events: list[dict[str, Any]], event_types: Mapping[str, int]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    queue_send = event_types["EVENT_TYPE_SCHED_QUEUE_SEND"]
    queue_receive = event_types["EVENT_TYPE_SCHED_QUEUE_RECEIVE"]
    callback_begin = event_types["EVENT_TYPE_CALLBACK_BEGIN"]
    callback_end = event_types["EVENT_TYPE_CALLBACK_END"]
    causal_complete = event_types["EVENT_TYPE_CAUSAL_COMPLETE"]
    sem_take = event_types["EVENT_TYPE_SEM_TAKE"]
    sem_give = event_types["EVENT_TYPE_SEM_GIVE"]
    queue_id, ready_id, callback_id, complete_id = ESP_NOW_TRACE_NAMES
    relevant = {
        queue_send: queue_id,
        queue_receive: queue_id,
        sem_give: ready_id,
        sem_take: ready_id,
        callback_begin: callback_id,
        callback_end: callback_id,
        causal_complete: complete_id,
    }
    token_events: dict[int, list[dict[str, Any]]] = {}
    for event in events:
        if relevant.get(event["type"]) != event["arg1"]:
            continue
        token = int(event["arg2"])
        if token == 0:
            raise RuntimeTraceError("ESP-NOW causal boundary has a zero token")
        token_events.setdefault(token, []).append(event)

    tx_chains: list[dict[str, Any]] = []
    rx_chains: list[dict[str, Any]] = []
    for token, correlated in token_events.items():
        submissions: list[int] = []
        callbacks: dict[str, list[list[int]]] = {"rx": [], "tx": []}
        completions: dict[str, list[list[int]]] = {"rx": [], "tx": []}
        index = 0
        while index < len(correlated):
            event = correlated[index]
            if event["type"] == queue_send and event["context"] == 0:
                submissions.append(event["sequence"])
                index += 1
                continue
            if event["type"] == callback_begin:
                end = index + 1
                while end < len(correlated) and correlated[end]["type"] != callback_end:
                    end += 1
                if end == len(correlated):
                    raise RuntimeTraceError("ESP-NOW callback chain is incomplete")
                event_sequence = [item["type"] for item in correlated[index : end + 1]]
                direction = (
                    "rx"
                    if event_sequence
                    == [callback_begin, queue_send, sem_give, callback_end]
                    else "tx"
                )
                if direction == "tx" and event_sequence != [
                    callback_begin,
                    sem_give,
                    callback_end,
                ]:
                    raise RuntimeTraceError("ESP-NOW callback chain is malformed")
                callbacks[direction].append(
                    [item["sequence"] for item in correlated[index : end + 1]]
                )
                index = end + 1
                continue
            if event["type"] == sem_take:
                end = index + 1
                while (
                    end < len(correlated) and correlated[end]["type"] != causal_complete
                ):
                    end += 1
                if end == len(correlated):
                    raise RuntimeTraceError("ESP-NOW task chain is incomplete")
                event_sequence = [item["type"] for item in correlated[index : end + 1]]
                direction = (
                    "rx"
                    if event_sequence == [sem_take, queue_receive, causal_complete]
                    else "tx"
                )
                if direction == "tx" and event_sequence != [
                    sem_take,
                    causal_complete,
                ]:
                    raise RuntimeTraceError("ESP-NOW task chain is malformed")
                completions[direction].append(
                    [item["sequence"] for item in correlated[index : end + 1]]
                )
                index = end + 1
                continue
            raise RuntimeTraceError("ESP-NOW causal chain has an unexpected boundary")

        if len(submissions) != len(callbacks["tx"]) or len(callbacks["tx"]) != len(
            completions["tx"]
        ):
            raise RuntimeTraceError("ESP-NOW TX correlation chain is incomplete")
        if len(callbacks["rx"]) != len(completions["rx"]):
            raise RuntimeTraceError("ESP-NOW RX correlation chain is incomplete")
        for start, callback, complete in zip(
            submissions, callbacks["tx"], completions["tx"]
        ):
            if not start < callback[0] < complete[0]:
                raise RuntimeTraceError("ESP-NOW TX correlation chain is reordered")
            tx_chains.append(
                {
                    "kind": "tx",
                    "token": token,
                    "positions": [start, *callback, *complete],
                }
            )
        for callback, complete in zip(callbacks["rx"], completions["rx"]):
            if callback[0] >= complete[0]:
                raise RuntimeTraceError("ESP-NOW RX correlation chain is reordered")
            rx_chains.append(
                {
                    "kind": "rx",
                    "token": token,
                    "positions": [*callback, *complete],
                }
            )
    return (
        sorted(tx_chains, key=lambda chain: chain["positions"][0]),
        sorted(rx_chains, key=lambda chain: chain["positions"][0]),
    )


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
    if any(
        names.get(identifier) != name
        for identifier, name in ESP_NOW_TRACE_NAMES.items()
    ):
        raise RuntimeTraceError("trace name map omits ESP-NOW correlation boundaries")

    tasks, objects = _catalog(session)
    object_kinds = _enum(proto, "ObjectKind")
    expected_objects = {
        0x59FB7823: {
            "kind": object_kinds["OBJECT_KIND_QUEUE"],
            "name": ESP_NOW_OBJECT_NAMES[0x59FB7823],
        },
        0xEF2DD8BB: {
            "kind": object_kinds["OBJECT_KIND_SEMAPHORE"],
            "name": ESP_NOW_OBJECT_NAMES[0xEF2DD8BB],
        },
        0x35800DA2: {
            "kind": object_kinds["OBJECT_KIND_CALLBACK"],
            "name": ESP_NOW_OBJECT_NAMES[0x35800DA2],
        },
        0xF1F4511E: {
            "kind": object_kinds["OBJECT_KIND_ACTION"],
            "name": ESP_NOW_OBJECT_NAMES[0xF1F4511E],
        },
    }
    if objects != expected_objects:
        raise RuntimeTraceError("session ESP-NOW object catalog is unresolved")
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
    _validate_scheduler_contract(events, event_types)
    if (
        int(session.get("start_timestamp_us", -1)) != events[0]["timestamp_us"]
        or int(session.get("end_timestamp_us", -1)) != events[-1]["timestamp_us"]
    ):
        raise RuntimeTraceError("session timestamps do not bound raw evidence")

    tx_chains, rx_chains = _correlation_chains(events, event_types)
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
