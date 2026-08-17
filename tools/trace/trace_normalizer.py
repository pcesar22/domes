#!/usr/bin/env python3
"""Versioned, fail-closed normalization for the DOMES 16-byte trace ABI."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from typing import Any, Mapping

FORMAT_VERSION = 1
NORMALIZER_VERSION = "1.0.1"
EVENT_SIZE = 16
MAX_TRACE_DUMP_BYTES = 32 * 1024
TRACE_LINE = re.compile(r"DOMES_QEMU_TRACE schema=(\d+) index=(\d+) raw=([0-9a-f]{32})")
SESSION_LINE = re.compile(
    r"DOMES_QEMU_TRACE_SESSION schema=(\d+) objects=([0-9a-z_:,]+)"
)
TRACE_PROTO = Path(__file__).resolve().parents[2] / "firmware/common/proto/trace.proto"


def _proto_enum(name: str) -> dict[str, int]:
    text = TRACE_PROTO.read_text(encoding="utf-8")
    match = re.search(rf"enum\s+{re.escape(name)}\s*\{{(.*?)\n\}}", text, re.DOTALL)
    if match is None:
        raise RuntimeError(f"trace schema omits enum {name}")
    values = {
        key: int(value, 0)
        for key, value in re.findall(
            r"^\s*([A-Z][A-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;",
            match.group(1),
            re.MULTILINE,
        )
    }
    if not values:
        raise RuntimeError(f"trace schema enum {name} has no values")
    return values


EVENT_TYPES = _proto_enum("EventType")
KNOWN_EVENT_TYPES = frozenset(EVENT_TYPES.values()) - {
    EVENT_TYPES["EVENT_TYPE_UNKNOWN"]
}
KNOWN_CATEGORIES = frozenset(_proto_enum("Category").values())
OBJECT_KINDS = _proto_enum("ObjectKind")

MUTEX_LOCK = EVENT_TYPES["EVENT_TYPE_MUTEX_LOCK"]
MUTEX_UNLOCK = EVENT_TYPES["EVENT_TYPE_MUTEX_UNLOCK"]
MUTEX_CONTENTION = EVENT_TYPES["EVENT_TYPE_MUTEX_CONTENTION"]
TASK_CREATE = EVENT_TYPES["EVENT_TYPE_SCHED_TASK_CREATE"]
TASK_DELETE = EVENT_TYPES["EVENT_TYPE_SCHED_TASK_DELETE"]
TASK_READY = EVENT_TYPES["EVENT_TYPE_SCHED_TASK_READY"]
TASK_BLOCK = EVENT_TYPES["EVENT_TYPE_SCHED_TASK_BLOCK"]
SWITCH_IN = EVENT_TYPES["EVENT_TYPE_SCHED_SWITCH_IN"]
SWITCH_OUT = EVENT_TYPES["EVENT_TYPE_SCHED_SWITCH_OUT"]
ISR_ENTER = EVENT_TYPES["EVENT_TYPE_SCHED_ISR_ENTER"]
ISR_EXIT = EVENT_TYPES["EVENT_TYPE_SCHED_ISR_EXIT"]
QUEUE_SEND = EVENT_TYPES["EVENT_TYPE_SCHED_QUEUE_SEND"]
QUEUE_RECEIVE = EVENT_TYPES["EVENT_TYPE_SCHED_QUEUE_RECEIVE"]
TIMEOUT = EVENT_TYPES["EVENT_TYPE_SCHED_TIMEOUT"]
CALLBACK_BEGIN = EVENT_TYPES["EVENT_TYPE_CALLBACK_BEGIN"]
CALLBACK_END = EVENT_TYPES["EVENT_TYPE_CALLBACK_END"]
CAUSAL_COMPLETE = EVENT_TYPES["EVENT_TYPE_CAUSAL_COMPLETE"]
SEM_TAKE = EVENT_TYPES["EVENT_TYPE_SEM_TAKE"]
SEM_GIVE = EVENT_TYPES["EVENT_TYPE_SEM_GIVE"]
TRACE_OVERHEAD = EVENT_TYPES["EVENT_TYPE_TRACE_OVERHEAD"]
PROBE_OBJECT_NAMES = {
    1: "probe_queue",
    2: "probe_sem",
    3: "probe_irq",
    4: "probe_callback",
    5: "probe_action",
    6: "probe_timeout",
}
PROBE_OBJECT_KINDS = {
    1: OBJECT_KINDS["OBJECT_KIND_QUEUE"],
    2: OBJECT_KINDS["OBJECT_KIND_SEMAPHORE"],
    3: OBJECT_KINDS["OBJECT_KIND_INTERRUPT"],
    4: OBJECT_KINDS["OBJECT_KIND_CALLBACK"],
    5: OBJECT_KINDS["OBJECT_KIND_ACTION"],
    6: OBJECT_KINDS["OBJECT_KIND_TIMEOUT"],
}
PROBE_OBJECTS = {
    object_id: {"kind": PROBE_OBJECT_KINDS[object_id], "name": name}
    for object_id, name in PROBE_OBJECT_NAMES.items()
}


class TraceNormalizationError(ValueError):
    pass


def raw_from_qemu_log(text: str) -> bytes:
    marker_lines = [line for line in text.splitlines() if "DOMES_QEMU_TRACE " in line]
    matches = [TRACE_LINE.search(line) for line in marker_lines]
    if any(match is None for match in matches):
        raise TraceNormalizationError("QEMU log contains a malformed raw trace marker")
    rows = []
    for match in matches:
        assert match is not None
        schema, index, raw = match.groups()
        if int(schema) != FORMAT_VERSION:
            raise TraceNormalizationError(f"unsupported QEMU trace schema {schema}")
        rows.append((int(index), bytes.fromhex(raw)))
    if not rows:
        raise TraceNormalizationError("QEMU log contains no format-v1 raw trace events")
    if [index for index, _ in rows] != list(range(len(rows))):
        raise TraceNormalizationError(
            "raw trace event indexes are reordered, missing, duplicated, or nonzero-based"
        )
    return b"".join(raw for _, raw in rows)


def object_map_from_qemu_log(text: str) -> dict[int, dict[str, Any]]:
    marker_lines = [
        line for line in text.splitlines() if "DOMES_QEMU_TRACE_SESSION " in line
    ]
    matches = [SESSION_LINE.search(line) for line in marker_lines]
    if len(matches) != 1 or matches[0] is None:
        raise TraceNormalizationError(
            "QEMU log must contain one valid trace session marker"
        )
    match = matches[0]
    assert match is not None
    schema, encoded = match.groups()
    if int(schema) != FORMAT_VERSION:
        raise TraceNormalizationError(f"unsupported QEMU trace session schema {schema}")
    objects: dict[int, dict[str, Any]] = {}
    for item in encoded.split(","):
        parts = item.split(":")
        if (
            len(parts) != 3
            or not parts[0].isdigit()
            or not parts[1].isdigit()
            or not parts[2]
        ):
            raise TraceNormalizationError(
                "QEMU trace session has a malformed object mapping"
            )
        object_id = int(parts[0])
        if object_id == 0 or object_id in objects:
            raise TraceNormalizationError(
                "QEMU trace session has duplicate or zero object IDs"
            )
        objects[object_id] = {"kind": int(parts[1]), "name": parts[2]}
    if objects != PROBE_OBJECTS:
        raise TraceNormalizationError("QEMU trace session object mapping is unresolved")
    return objects


def _canonical(value: Any) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8") + b"\n"
    )


def _validate_timestamp_order(timestamps: list[int]) -> None:
    wraps = 0
    for previous, current in zip(timestamps, timestamps[1:]):
        delta = (current - previous) & 0xFFFFFFFF
        if delta > 0x7FFFFFFF:
            raise TraceNormalizationError(
                f"trace timestamp regression: {previous} followed by {current}"
            )
        if current < previous:
            wraps += 1
            if wraps > 1:
                raise TraceNormalizationError("trace timestamps wrap more than once")


def normalize_trace(
    raw: bytes,
    manifest: Mapping[str, Any],
    *,
    objects: Mapping[int, Any],
    dropped: int = 0,
    discontinuities: int = 0,
) -> dict[str, Any]:
    if not raw or len(raw) % EVENT_SIZE:
        raise TraceNormalizationError(
            "raw trace length is not a nonzero multiple of 16"
        )
    if dropped or discontinuities:
        raise TraceNormalizationError(
            "trace overflow or discontinuity invalidates evidence"
        )
    try:
        object_ids = {
            int(key): {"kind": int(value["kind"]), "name": str(value["name"])}
            for key, value in objects.items()
        }
    except (KeyError, TypeError, ValueError) as error:
        raise TraceNormalizationError(
            "trace has malformed stable object mappings"
        ) from error
    if object_ids != PROBE_OBJECTS:
        raise TraceNormalizationError("trace has invalid stable object mappings")
    required_tasks = [
        task for task in manifest.get("tasks", []) if task.get("presence") == "required"
    ]
    task_info = {
        int(task["trace_id"]): {
            "name": str(task["name"]),
            "priority": int(task["priority"]),
            "core_affinity": int(task["core_affinity"]),
        }
        for task in required_tasks
    }
    if (
        not task_info
        or 0 in task_info
        or len(task_info) != len(required_tasks)
        or any(
            task["priority"] < 0 or task["core_affinity"] not in (-1, 0, 1)
            for task in task_info.values()
        )
    ):
        raise TraceNormalizationError("manifest has invalid stable task mappings")

    events = []
    for sequence, offset in enumerate(range(0, len(raw), EVENT_SIZE)):
        timestamp, task_id, event_type, flags, arg1, arg2 = struct.unpack_from(
            "<IHBBII", raw, offset
        )
        core = flags & 0x03
        context = (flags >> 2) & 0x03
        category = (flags >> 4) & 0x0F
        if (
            event_type not in KNOWN_EVENT_TYPES
            or category not in KNOWN_CATEGORIES
            or core not in (1, 2)
            or context > 2
        ):
            raise TraceNormalizationError(
                f"event {sequence} has an unknown type/category/core/context"
            )
        if task_id and task_id not in task_info:
            raise TraceNormalizationError(
                f"event {sequence} uses unresolved task ID {task_id}"
            )
        if task_id and event_type not in (TASK_CREATE, TASK_DELETE, TASK_READY):
            affinity = task_info[task_id]["core_affinity"]
            if affinity != -1 and affinity != core - 1:
                raise TraceNormalizationError(
                    f"event {sequence} violates task affinity"
                )
        events.append(
            {
                "sequence": sequence,
                "timestamp_us": timestamp,
                "task_id": task_id,
                "task": task_info.get(task_id, {}).get("name"),
                "type": event_type,
                "category": category,
                "core": core - 1,
                "context": context,
                "arg1": arg1,
                "arg2": arg2,
            }
        )

    _validate_timestamp_order([event["timestamp_us"] for event in events])

    causal = [
        event
        for event in events
        if event["arg2"] == 1
        and event["type"]
        in {
            SEM_TAKE,
            SEM_GIVE,
            ISR_ENTER,
            ISR_EXIT,
            QUEUE_SEND,
            QUEUE_RECEIVE,
            TIMEOUT,
            CALLBACK_BEGIN,
            CALLBACK_END,
            CAUSAL_COMPLETE,
        }
    ]
    required = [
        (TIMEOUT, 6),
        (ISR_ENTER, 3),
        (CALLBACK_BEGIN, 4),
        (QUEUE_SEND, 1),
        (SEM_GIVE, 2),
        (CALLBACK_END, 4),
        (ISR_EXIT, 3),
        (QUEUE_RECEIVE, 1),
        (SEM_TAKE, 2),
        (CAUSAL_COMPLETE, 5),
    ]
    actual_causal = [(event["type"], event["arg1"]) for event in causal]
    if actual_causal != required:
        raise TraceNormalizationError(
            "causal chain is missing, duplicated, reordered, or contains extra edges"
        )
    positions = [event["sequence"] for event in causal]
    for event in events:
        if event["context"] == 1 and event["type"] == TASK_BLOCK:
            raise TraceNormalizationError("ISR context contains a blocking event")
        object_entry = object_ids.get(event["arg1"])
        if event["type"] == TASK_BLOCK and (
            object_entry is None
            or object_entry["kind"]
            not in {
                OBJECT_KINDS["OBJECT_KIND_QUEUE"],
                OBJECT_KINDS["OBJECT_KIND_SEMAPHORE"],
                OBJECT_KINDS["OBJECT_KIND_MUTEX"],
            }
        ):
            raise TraceNormalizationError(
                "block event uses an unresolved object ID or kind"
            )
        if event["type"] in (MUTEX_LOCK, MUTEX_UNLOCK, MUTEX_CONTENTION) and (
            object_entry is None
            or object_entry["kind"] != OBJECT_KINDS["OBJECT_KIND_MUTEX"]
        ):
            raise TraceNormalizationError(
                "mutex event uses an unresolved object ID or kind"
            )
        if event["type"] in (QUEUE_SEND, QUEUE_RECEIVE) and (
            event["arg1"] != 1
            or object_entry is None
            or object_entry["kind"] != OBJECT_KINDS["OBJECT_KIND_QUEUE"]
        ):
            raise TraceNormalizationError("queue event uses an unresolved object ID")
        if event["type"] in (SEM_TAKE, SEM_GIVE) and (
            event["arg1"] != 2
            or object_entry is None
            or object_entry["kind"] != OBJECT_KINDS["OBJECT_KIND_SEMAPHORE"]
        ):
            raise TraceNormalizationError(
                "semaphore event uses an unresolved object ID"
            )
        expected_object = {
            ISR_ENTER: 3,
            ISR_EXIT: 3,
            TIMEOUT: 6,
            CALLBACK_BEGIN: 4,
            CALLBACK_END: 4,
            CAUSAL_COMPLETE: 5,
        }.get(event["type"])
        if expected_object is not None and event["arg1"] != expected_object:
            raise TraceNormalizationError("causal event uses an unresolved object ID")

    create_events = [event for event in events if event["type"] == TASK_CREATE]
    if len(create_events) != len(task_info) or {
        event["task_id"] for event in create_events
    } != set(task_info):
        raise TraceNormalizationError(
            "task catalog preamble is incomplete or duplicated"
        )
    first_non_create = next(
        (event["sequence"] for event in events if event["type"] != TASK_CREATE),
        len(events),
    )
    if any(event["sequence"] >= first_non_create for event in create_events):
        raise TraceNormalizationError("task catalog preamble is not ordered first")
    for event in create_events:
        task = task_info[event["task_id"]]
        expected_mask = 3 if task["core_affinity"] == -1 else 1 << task["core_affinity"]
        if event["arg1"] != task["priority"] or event["arg2"] != expected_mask:
            raise TraceNormalizationError(
                "task catalog priority or affinity mapping is invalid"
            )

    scheduler_contexts = {
        TASK_CREATE: {0},
        TASK_DELETE: {0},
        TASK_READY: {0, 1},
        TASK_BLOCK: {0},
        SWITCH_IN: {0},
        SWITCH_OUT: {0},
        ISR_ENTER: {1},
        ISR_EXIT: {1},
        QUEUE_SEND: {0, 1},
        QUEUE_RECEIVE: {0, 1},
        TIMEOUT: {0},
        CALLBACK_BEGIN: {2},
        CALLBACK_END: {2},
        CAUSAL_COMPLETE: {0},
        SEM_TAKE: {0, 1},
        SEM_GIVE: {0, 1},
        TRACE_OVERHEAD: {0},
    }
    taskless_contexts = {ISR_ENTER, ISR_EXIT, CALLBACK_BEGIN, CALLBACK_END}
    for event in events:
        allowed = scheduler_contexts.get(event["type"])
        if allowed is not None and event["context"] not in allowed:
            raise TraceNormalizationError(
                "scheduler event has an invalid execution context"
            )
        if allowed is not None and event["category"] != 0:
            raise TraceNormalizationError(
                "scheduler evidence event does not use the kernel category"
            )
        if event["type"] in taskless_contexts and event["task_id"] != 0:
            raise TraceNormalizationError("ISR/callback event unexpectedly owns a task")
        if event["type"] in (QUEUE_SEND, QUEUE_RECEIVE, SEM_TAKE, SEM_GIVE):
            if (event["context"] == 0 and event["task_id"] == 0) or (
                event["context"] == 1 and event["task_id"] != 0
            ):
                raise TraceNormalizationError(
                    "synchronization event ownership/context is invalid"
                )

    lifecycle_stacks = {
        (core, kind): [] for core in (0, 1) for kind in ("isr", "callback")
    }
    scheduled_tasks = {0: None, 1: None}
    scheduler_known = {0: False, 1: False}
    active_tasks = set(task_info)
    ready_tasks: set[int] = set()
    for event in events:
        task_id = event["task_id"]
        if task_id and event["type"] != TASK_CREATE and task_id not in active_tasks:
            raise TraceNormalizationError(
                "task activity occurs before create or after delete"
            )
        if event["type"] == TASK_DELETE:
            if (
                task_id == 0
                or task_id not in active_tasks
                or task_id in scheduled_tasks.values()
            ):
                raise TraceNormalizationError("task delete lifecycle is inconsistent")
            active_tasks.remove(task_id)
            ready_tasks.discard(task_id)
        elif event["type"] == TASK_READY:
            if task_id == 0:
                raise TraceNormalizationError("ready event omits its task")
            ready_tasks.add(task_id)
        elif event["type"] == TASK_BLOCK:
            if task_id == 0:
                raise TraceNormalizationError("block event omits its task")
            if (
                scheduler_known[event["core"]]
                and scheduled_tasks[event["core"]] != task_id
            ):
                raise TraceNormalizationError("non-running task emitted a block event")
            ready_tasks.discard(task_id)

        if event["type"] == SWITCH_IN:
            if event["task_id"] == 0 or scheduled_tasks[event["core"]] is not None:
                raise TraceNormalizationError(
                    "scheduler switch-in lifecycle is inconsistent"
                )
            eligible = [
                candidate
                for candidate in ready_tasks
                if task_info[candidate]["core_affinity"] in (-1, event["core"])
            ]
            if eligible and task_info[event["task_id"]]["priority"] < max(
                task_info[candidate]["priority"] for candidate in eligible
            ):
                raise TraceNormalizationError(
                    "scheduler selected below the highest ready priority"
                )
            scheduled_tasks[event["core"]] = event["task_id"]
            scheduler_known[event["core"]] = True
            ready_tasks.discard(event["task_id"])
        elif event["type"] == SWITCH_OUT:
            current = scheduled_tasks[event["core"]]
            if event["task_id"] == 0 or current != event["task_id"]:
                raise TraceNormalizationError(
                    "scheduler switch-out lifecycle is inconsistent"
                )
            scheduled_tasks[event["core"]] = None
        if event["type"] in (ISR_ENTER, ISR_EXIT):
            stack = lifecycle_stacks[(event["core"], "isr")]
            if event["type"] == ISR_ENTER:
                stack.append((event["arg1"], event["arg2"]))
            elif not stack or stack.pop() != (event["arg1"], event["arg2"]):
                raise TraceNormalizationError(
                    "ISR lifecycle is out of order or mismatched"
                )
        if event["type"] in (CALLBACK_BEGIN, CALLBACK_END):
            stack = lifecycle_stacks[(event["core"], "callback")]
            if event["type"] == CALLBACK_BEGIN:
                stack.append((event["arg1"], event["arg2"]))
            elif not stack or stack.pop() != (event["arg1"], event["arg2"]):
                raise TraceNormalizationError(
                    "callback lifecycle is out of order or mismatched"
                )
    if any(stack for stack in lifecycle_stacks.values()) or any(
        task is not None for task in scheduled_tasks.values()
    ):
        raise TraceNormalizationError(
            "trace has an unbalanced ISR, callback, or switch lifecycle"
        )

    explicit = causal
    irq_cores = {
        event["core"]
        for event in explicit
        if event["type"] in (ISR_ENTER, ISR_EXIT, CALLBACK_BEGIN, CALLBACK_END)
    }
    completions = [event for event in explicit if event["type"] == CAUSAL_COMPLETE]
    if len(irq_cores) != 1 or len(completions) != 1 or completions[0]["task_id"] != 1:
        raise TraceNormalizationError(
            "causal ownership does not resolve to one ISR core and main task"
        )
    for event in explicit:
        expected_context = {
            SEM_GIVE: 1,
            ISR_ENTER: 1,
            ISR_EXIT: 1,
            QUEUE_SEND: 1,
            CALLBACK_BEGIN: 2,
            CALLBACK_END: 2,
            SEM_TAKE: 0,
            QUEUE_RECEIVE: 0,
            TIMEOUT: 0,
            CAUSAL_COMPLETE: 0,
        }.get(event["type"])
        if expected_context is not None and event["context"] != expected_context:
            raise TraceNormalizationError(
                "causal event has an invalid execution context"
            )
        if (
            event["type"] in (SEM_TAKE, QUEUE_RECEIVE, TIMEOUT, CAUSAL_COMPLETE)
            and event["task_id"] != 1
        ):
            raise TraceNormalizationError("task-side causal event is not owned by main")
    overhead_events = [event for event in events if event["type"] == TRACE_OVERHEAD]
    if (
        len(overhead_events) != 1
        or overhead_events[0]["task_id"] != 1
        or overhead_events[0]["context"] != 0
        or overhead_events[0]["arg1"] == 0
        or overhead_events[0]["arg2"] == 0
        or overhead_events[0]["arg2"] <= overhead_events[0]["arg1"]
    ):
        raise TraceNormalizationError(
            "trace must contain one positive main-task overhead measurement"
        )

    first_timestamp = events[0]["timestamp_us"]
    for event in events:
        event["relative_us"] = (
            event.pop("timestamp_us") - first_timestamp
        ) & 0xFFFFFFFF
    normalized = {
        "artifact_kind": "replay-normalized-trace",
        "format_version": FORMAT_VERSION,
        "event_size": EVENT_SIZE,
        "normalizer": {
            "name": "domes-trace-normalizer",
            "version": NORMALIZER_VERSION,
            "input_schema": "domes.trace.TraceEvent/le16/v1",
            "output_schema": "domes.trace.replay-normalized/v1",
            "ordered_transforms": [
                "decode-le16",
                "resolve-stable-ids",
                "subtract-session-start",
            ],
            "field_mapping": "all event fields retained; timestamp_us becomes relative_us",
            "exclusions": [],
        },
        "tasks": {str(key): task_info[key] for key in sorted(task_info)},
        "objects": {str(key): object_ids[key] for key in sorted(object_ids)},
        "dropped_count": dropped,
        "discontinuity_count": discontinuities,
        "causal_id": 1,
        "causal_positions": positions,
        "overhead_us": {
            "disabled_32_records": overhead_events[0]["arg1"],
            "enabled_32_records": overhead_events[0]["arg2"],
        },
        "events": events,
    }
    normalized["normalized_sha256"] = hashlib.sha256(_canonical(normalized)).hexdigest()
    normalized["raw_sha256"] = hashlib.sha256(raw).hexdigest()
    return normalized


def semantic_projection(replay: Mapping[str, Any]) -> dict[str, Any]:
    projection = dict(replay)
    projection.pop("normalized_sha256", None)
    projection["artifact_kind"] = "cross-target-semantic-projection"
    projection["normalizer"] = dict(replay["normalizer"])
    projection["normalizer"]["output_schema"] = "domes.trace.cross-target-semantic/v1"
    projection["comparison"] = "thresholded; whole-file hash comparison forbidden"
    return projection


def canonical_json(value: Mapping[str, Any]) -> bytes:
    return _canonical(value)


def _manifest_from_session(session: Mapping[str, Any]) -> dict[str, Any]:
    tasks = []
    task_ids: set[int] = set()
    task_names: set[str] = set()
    for task in session.get("tasks", []):
        task_id = int(task["task_id"])
        name = str(task["name"])
        priority = int(task["priority"])
        if (
            task_id <= 0
            or task_id > 31
            or task_id in task_ids
            or not name
            or len(name.encode("utf-8")) >= 16
            or name in task_names
            or priority < 0
            or priority > 0xFF
        ):
            raise TraceNormalizationError(
                "session has duplicate or invalid task mappings"
            )
        task_ids.add(task_id)
        task_names.add(name)
        mask = int(task["core_affinity_mask"])
        affinity = {1: 0, 2: 1, 3: -1}.get(mask)
        if affinity is None:
            raise TraceNormalizationError("session has invalid task affinity mask")
        tasks.append(
            {
                "presence": "required",
                "trace_id": task_id,
                "name": name,
                "priority": priority,
                "core_affinity": affinity,
            }
        )
    return {"tasks": tasks}


def validate_session(
    raw: bytes, session: Mapping[str, Any]
) -> dict[int, dict[str, Any]]:
    if len(raw) < EVENT_SIZE or len(raw) % EVENT_SIZE:
        raise TraceNormalizationError(
            "raw trace length is not a nonzero multiple of 16"
        )
    if int(session.get("format_version", -1)) != FORMAT_VERSION:
        raise TraceNormalizationError(
            "session format version does not match the trace ABI"
        )
    firmware_version = session.get("firmware_version")
    if (
        not isinstance(firmware_version, str)
        or not firmware_version
        or len(firmware_version.encode("utf-8")) >= 32
    ):
        raise TraceNormalizationError("session firmware version is missing or invalid")

    def validated_hex(name: str, size: int) -> str:
        value = session.get(name)
        if not isinstance(value, str) or len(value) != size * 2:
            raise TraceNormalizationError(f"session {name} is missing or invalid")
        try:
            decoded = bytes.fromhex(value)
        except ValueError as error:
            raise TraceNormalizationError(
                f"session {name} is not hexadecimal"
            ) from error
        if len(decoded) != size or all(byte == 0 for byte in decoded):
            raise TraceNormalizationError(f"session {name} is missing or invalid")
        return value

    app_elf_sha256 = validated_hex("app_elf_sha256", 32)
    app_image_sha256 = validated_hex("app_image_sha256", 32)
    device_uid = bytes.fromhex(validated_hex("device_uid", 6))
    if all(byte == 0xFF for byte in device_uid) or device_uid[0] & 0x01:
        raise TraceNormalizationError("session device_uid is not a factory unicast MAC")

    transport = session.get("transport")
    if (
        not isinstance(transport, dict)
        or transport.get("type") not in {"serial", "wifi", "tcp", "ble"}
        or not isinstance(transport.get("device_name"), str)
        or not transport["device_name"]
        or not isinstance(transport.get("address"), str)
        or not transport["address"]
    ):
        raise TraceNormalizationError(
            "session transport identity is missing or invalid"
        )

    candidate = session.get("candidate_image")
    if candidate is not None:
        if (
            not isinstance(candidate, dict)
            or candidate.get("binding_verified") is not True
            or candidate.get("firmware_version") != firmware_version
            or candidate.get("app_elf_sha256") != app_elf_sha256
            or candidate.get("app_image_sha256") != app_image_sha256
        ):
            raise TraceNormalizationError("candidate image binding is invalid")
        file_hash = candidate.get("file_sha256")
        if not isinstance(file_hash, str) or len(file_hash) != 64:
            raise TraceNormalizationError("candidate image file hash is invalid")
        try:
            decoded_file_hash = bytes.fromhex(file_hash)
            if len(decoded_file_hash) != 32 or all(
                byte == 0 for byte in decoded_file_hash
            ):
                raise ValueError
        except ValueError as error:
            raise TraceNormalizationError(
                "candidate image file hash is invalid"
            ) from error
    if int(session.get("event_count", -1)) != len(raw) // EVENT_SIZE:
        raise TraceNormalizationError("session event count does not match raw evidence")
    if int(session.get("received_raw_bytes", -1)) != len(raw):
        raise TraceNormalizationError(
            "session received byte count does not match raw evidence"
        )
    buffer_size = int(session.get("buffer_size_bytes", -1))
    if buffer_size <= 0 or buffer_size > MAX_TRACE_DUMP_BYTES or len(raw) > buffer_size:
        raise TraceNormalizationError("session buffer capacity is invalid")
    if session.get("integrity_error") is not None:
        raise TraceNormalizationError("session records a trace integrity error")
    raw_sha256 = hashlib.sha256(raw).hexdigest()
    if session.get("raw_sha256") != raw_sha256:
        raise TraceNormalizationError("session hash does not match raw evidence")
    first_timestamp = struct.unpack_from("<I", raw, 0)[0]
    last_timestamp = struct.unpack_from("<I", raw, len(raw) - EVENT_SIZE)[0]
    if (
        int(session.get("start_timestamp_us", -1)) != first_timestamp
        or int(session.get("end_timestamp_us", -1)) != last_timestamp
    ):
        raise TraceNormalizationError(
            "session timestamps do not bound the raw evidence"
        )
    _validate_timestamp_order(
        [
            struct.unpack_from("<I", raw, offset)[0]
            for offset in range(0, len(raw), EVENT_SIZE)
        ]
    )

    object_names: dict[int, str] = {}
    object_kinds: dict[int, int] = {}
    objects = session.get("objects")
    if not isinstance(objects, list):
        raise TraceNormalizationError("session object catalog is missing")
    for item in objects:
        object_id = int(item["object_id"])
        name = str(item["name"])
        kind = int(item["kind"])
        if (
            object_id == 0
            or object_id in object_names
            or kind not in range(1, 8)
            or not name
            or len(name.encode("utf-8")) >= 16
            or name in object_names.values()
        ):
            raise TraceNormalizationError(
                "session has duplicate or invalid object mappings"
            )
        object_names[object_id] = name
        object_kinds[object_id] = kind
    if object_names != PROBE_OBJECT_NAMES or object_kinds != PROBE_OBJECT_KINDS:
        raise TraceNormalizationError(
            "session object IDs, kinds, or names are unresolved"
        )
    _manifest_from_session(session)
    return {
        object_id: {"kind": object_kinds[object_id], "name": object_names[object_id]}
        for object_id in sorted(object_names)
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--session", type=Path, required=True)
    parser.add_argument("--output-prefix", type=Path, required=True)
    args = parser.parse_args()
    raw = args.raw.read_bytes()
    session = json.loads(args.session.read_text(encoding="utf-8"))
    objects = validate_session(raw, session)
    replay = normalize_trace(
        raw,
        _manifest_from_session(session),
        objects=objects,
        dropped=int(session.get("dropped_count", 0)),
        discontinuities=int(session.get("discontinuity_count", 0)),
    )
    args.output_prefix.with_suffix(".replay.json").write_bytes(canonical_json(replay))
    args.output_prefix.with_suffix(".semantic.json").write_bytes(
        canonical_json(semantic_projection(replay))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
