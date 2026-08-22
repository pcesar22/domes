#!/usr/bin/env python3
"""Deterministic one-DUT peer backplane and replay contract.

This module is deliberately host-I/O free.  The QEMU device supplies virtual
nanoseconds and production ESP-NOW payloads; this model resolves an immutable
scenario, orders bounded deliveries, drives functional peers, and emits a
canonical replay record.  Functional peers implement scenario transitions
only.  Retry, timeout, recovery, role election, and game state stay in the DUT.
"""

from __future__ import annotations

import ast
import dataclasses
import hashlib
import heapq
import json
import struct
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any

SCHEMA_VERSION = 1
BROADCAST_ID = 0xFFFF
PRODUCTION_CODEC = "peer-drill-legacy-v1"
IMPORTANT_ROLES = ("master", "slave")


class BackplaneFailure(RuntimeError):
    """A fail-closed deterministic-model or replay violation."""


class EventClass(IntEnum):
    ASSERTION = 0
    DUT_DELIVERY = 1
    ACTOR_TRANSITION = 2


class MessageType(IntEnum):
    BEACON = 0x01
    PING = 0x02
    PONG = 0x03
    JOIN_GAME = 0x10
    ARM_TOUCH = 0x11
    SET_COLOR = 0x12
    STOP_ALL = 0x13
    SIMULATE_TOUCH = 0x14
    TOUCH_EVENT = 0x20
    TIMEOUT_EVENT = 0x21


WIRE_SIZES = {
    MessageType.BEACON: 11,
    MessageType.PING: 11,
    MessageType.PONG: 11,
    MessageType.JOIN_GAME: 11,
    MessageType.ARM_TOUCH: 20,
    MessageType.SET_COLOR: 14,
    MessageType.STOP_ALL: 11,
    MessageType.SIMULATE_TOUCH: 16,
    MessageType.TOUCH_EVENT: 20,
    MessageType.TIMEOUT_EVENT: 15,
}


def _canonical(value: object) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _require_exact_keys(value: Mapping[str, Any], keys: set[str], name: str) -> None:
    actual = set(value)
    if actual != keys:
        raise BackplaneFailure(
            f"{name} keys differ: missing={sorted(keys - actual)} extra={sorted(actual - keys)}"
        )


@dataclass(frozen=True, order=True)
class EventKey:
    deadline_ns: int
    event_class_priority: int
    source_id: int
    destination_id: int
    sequence: int


@dataclass(order=True)
class Event:
    key: EventKey
    payload: bytes = field(compare=False)
    kind: str = field(compare=False, default="delivery")


class BoundedEventQueue:
    def __init__(self, capacity: int):
        if not isinstance(capacity, int) or isinstance(capacity, bool) or capacity <= 0:
            raise BackplaneFailure("event queue capacity must be a positive integer")
        self.capacity = capacity
        self._events: list[Event] = []
        self._keys: set[EventKey] = set()

    def push(self, event: Event) -> None:
        if event.key.deadline_ns < 0 or event.key.sequence < 0:
            raise BackplaneFailure("event key contains a negative value")
        if event.key in self._keys:
            raise BackplaneFailure("duplicate total-order event key")
        if len(self._events) >= self.capacity:
            raise BackplaneFailure("bounded event queue overflow")
        heapq.heappush(self._events, event)
        self._keys.add(event.key)

    def pop(self) -> Event:
        if not self._events:
            raise BackplaneFailure("event queue exhausted")
        event = heapq.heappop(self._events)
        self._keys.remove(event.key)
        return event

    def __len__(self) -> int:
        return len(self._events)


@dataclass(frozen=True)
class WireMessage:
    message_type: MessageType
    sender_mac: bytes
    timestamp_us: int
    round_token: int = 0
    timeout_ms: int = 0
    feedback_mode: int = 0
    red: int = 0
    green: int = 0
    blue: int = 0
    reaction_time_us: int = 0
    pad_index: int = 0

    def encode(self) -> bytes:
        if len(self.sender_mac) != 6 or not 0 <= self.timestamp_us <= 0xFFFFFFFF:
            raise BackplaneFailure("invalid production wire header")
        payload = struct.pack(
            "<B6sI", self.message_type, self.sender_mac, self.timestamp_us
        )
        if self.message_type == MessageType.ARM_TOUCH:
            payload += struct.pack(
                "<IIB", self.round_token, self.timeout_ms, self.feedback_mode
            )
        elif self.message_type == MessageType.SET_COLOR:
            if any(
                not 0 <= value <= 255 for value in (self.red, self.green, self.blue)
            ):
                raise BackplaneFailure("invalid SET_COLOR fields")
            payload += bytes((self.red, self.green, self.blue))
        elif self.message_type == MessageType.SIMULATE_TOUCH:
            payload += struct.pack("<IB", self.round_token, self.pad_index)
        elif self.message_type == MessageType.TOUCH_EVENT:
            payload += struct.pack(
                "<IIB", self.round_token, self.reaction_time_us, self.pad_index
            )
        elif self.message_type == MessageType.TIMEOUT_EVENT:
            payload += struct.pack("<I", self.round_token)
        if len(payload) != WIRE_SIZES[self.message_type]:
            raise BackplaneFailure("production codec emitted an invalid wire size")
        return payload

    @classmethod
    def decode(cls, payload: bytes) -> "WireMessage":
        if len(payload) < 11:
            raise BackplaneFailure("truncated production wire message")
        try:
            message_type = MessageType(payload[0])
        except ValueError as error:
            raise BackplaneFailure("unknown production wire message") from error
        if len(payload) != WIRE_SIZES[message_type]:
            raise BackplaneFailure("non-canonical production wire size")
        _, sender_mac, timestamp_us = struct.unpack_from("<B6sI", payload)
        values: dict[str, int] = {}
        if message_type == MessageType.ARM_TOUCH:
            values["round_token"], values["timeout_ms"], values["feedback_mode"] = (
                struct.unpack_from("<IIB", payload, 11)
            )
            if values["round_token"] == 0 or not 0 < values["timeout_ms"] <= 60_000:
                raise BackplaneFailure("invalid ARM_TOUCH fields")
        elif message_type == MessageType.SET_COLOR:
            values["red"], values["green"], values["blue"] = payload[11:14]
        elif message_type == MessageType.SIMULATE_TOUCH:
            values["round_token"], values["pad_index"] = struct.unpack_from(
                "<IB", payload, 11
            )
        elif message_type == MessageType.TOUCH_EVENT:
            values["round_token"], values["reaction_time_us"], values["pad_index"] = (
                struct.unpack_from("<IIB", payload, 11)
            )
        elif message_type == MessageType.TIMEOUT_EVENT:
            (values["round_token"],) = struct.unpack_from("<I", payload, 11)
        if values.get("round_token", 1) == 0 or values.get("pad_index", 0) > 3:
            raise BackplaneFailure("invalid round-scoped production wire fields")
        return cls(message_type, sender_mac, timestamp_us, **values)


@dataclass(frozen=True)
class ActorConfig:
    pod_id: int
    role: str
    mac: bytes
    peer_delay_ns: int
    reaction_time_us: int


@dataclass(frozen=True)
class ResolvedScenario:
    name: str
    model: str
    seed: int
    dut_role: str
    dut_id: int
    dut_mac: bytes
    queue_capacity: int
    termination_ns: int
    actors: tuple[ActorConfig, ...]
    expected_dut_types: tuple[MessageType, ...]
    resolved_sha256: str


SCENARIO_KEYS = {
    "schema_version",
    "name",
    "model",
    "seed",
    "dut_role",
    "dut_id",
    "dut_mac",
    "queue_capacity",
    "termination_ns",
    "actors",
    "expected_dut_types",
}
ACTOR_KEYS = {"pod_id", "role", "mac", "peer_delay_ns", "reaction_time_us"}


def _decode_mac(value: object, name: str) -> bytes:
    if not isinstance(value, str):
        raise BackplaneFailure(f"{name} must be a colon-separated MAC")
    try:
        result = bytes(int(part, 16) for part in value.split(":"))
    except ValueError as error:
        raise BackplaneFailure(f"{name} is invalid") from error
    if len(result) != 6:
        raise BackplaneFailure(f"{name} must contain six octets")
    return result


def resolve_scenario(raw: Mapping[str, Any]) -> ResolvedScenario:
    _require_exact_keys(raw, SCENARIO_KEYS, "scenario")
    if raw["schema_version"] != SCHEMA_VERSION or raw["model"] != "functional-peer-v1":
        raise BackplaneFailure("unsupported scenario schema or model")
    if raw["dut_role"] not in IMPORTANT_ROLES:
        raise BackplaneFailure("DUT role must be master or slave")
    integers = ("seed", "dut_id", "queue_capacity", "termination_ns")
    if any(
        not isinstance(raw[key], int) or isinstance(raw[key], bool) for key in integers
    ):
        raise BackplaneFailure("scenario integer field has the wrong type")
    if raw["queue_capacity"] <= 0 or raw["termination_ns"] <= 0:
        raise BackplaneFailure("scenario bounds must be positive")
    if not isinstance(raw["actors"], Sequence) or isinstance(
        raw["actors"], (str, bytes)
    ):
        raise BackplaneFailure("scenario actors must be an ordered array")
    actors: list[ActorConfig] = []
    actor_ids: set[int] = set()
    for index, value in enumerate(raw["actors"]):
        if not isinstance(value, Mapping):
            raise BackplaneFailure("actor must be an object")
        _require_exact_keys(value, ACTOR_KEYS, f"actor[{index}]")
        if value["role"] not in IMPORTANT_ROLES or value["role"] == raw["dut_role"]:
            raise BackplaneFailure("functional actor role must complement the DUT")
        numeric = ("pod_id", "peer_delay_ns", "reaction_time_us")
        if any(
            not isinstance(value[key], int) or isinstance(value[key], bool)
            for key in numeric
        ):
            raise BackplaneFailure("actor integer field has the wrong type")
        if value["pod_id"] in actor_ids or value["pod_id"] == raw["dut_id"]:
            raise BackplaneFailure("actor pod IDs must be unique and distinct from DUT")
        if value["peer_delay_ns"] < 0 or value["reaction_time_us"] < 0:
            raise BackplaneFailure("actor timing cannot be negative")
        actor_ids.add(value["pod_id"])
        actors.append(
            ActorConfig(
                value["pod_id"],
                value["role"],
                _decode_mac(value["mac"], f"actor[{index}].mac"),
                value["peer_delay_ns"],
                value["reaction_time_us"],
            )
        )
    if not actors:
        raise BackplaneFailure("scenario requires at least one functional actor")
    try:
        expected = tuple(MessageType[value] for value in raw["expected_dut_types"])
    except (KeyError, TypeError) as error:
        raise BackplaneFailure(
            "expected_dut_types contains an unknown message"
        ) from error
    resolved = dict(raw)
    resolved["actors"] = sorted(
        (dict(actor) for actor in raw["actors"]), key=lambda item: item["pod_id"]
    )
    return ResolvedScenario(
        str(raw["name"]),
        str(raw["model"]),
        raw["seed"],
        raw["dut_role"],
        raw["dut_id"],
        _decode_mac(raw["dut_mac"], "dut_mac"),
        raw["queue_capacity"],
        raw["termination_ns"],
        tuple(sorted(actors, key=lambda actor: actor.pod_id)),
        expected,
        sha256_bytes(_canonical(resolved)),
    )


IDENTITY_KEYS = {
    "schema_version",
    "firmware_sha256",
    "flash_sha256",
    "toolchain_identity",
    "qemu_revision",
    "qemu_patch_sha256",
    "profile_sha256",
    "fidelity_manifest_sha256",
    "scenario_schema",
    "scenario_model",
    "scenario_seed",
    "resolved_scenario_sha256",
    "icount_shift",
    "vcpu_count",
    "input_records_sha256",
    "trace_sha256",
    "assertions",
    "termination",
    "unconsumed_events",
    "delivery_records_sha256",
}
TRACE_RECORD_KEYS = {
    "timestamp_ns",
    "event_id",
    "task_id",
    "core_id",
    "correlation_token",
}


def replay_normalized_trace_hash(records: Sequence[Mapping[str, Any]]) -> str:
    """Hash stable causal trace fields without host or raw-address metadata."""
    normalized = []
    previous_timestamp = -1
    for index, record in enumerate(records):
        _require_exact_keys(record, TRACE_RECORD_KEYS, f"trace[{index}]")
        if any(
            not isinstance(record[key], int) or isinstance(record[key], bool)
            for key in TRACE_RECORD_KEYS
        ):
            raise BackplaneFailure("normalized trace fields must be integers")
        if record["timestamp_ns"] < previous_timestamp:
            raise BackplaneFailure("normalized trace virtual time regressed")
        previous_timestamp = record["timestamp_ns"]
        normalized.append(dict(record))
    return sha256_bytes(_canonical(normalized))


@dataclass(frozen=True)
class ReplayIdentity:
    values: Mapping[str, Any]

    @classmethod
    def create(cls, values: Mapping[str, Any]) -> "ReplayIdentity":
        _require_exact_keys(values, IDENTITY_KEYS, "replay identity")
        if (
            values["schema_version"] != SCHEMA_VERSION
            or values["scenario_schema"] != SCHEMA_VERSION
        ):
            raise BackplaneFailure("unsupported replay identity schema")
        if (
            not isinstance(values["vcpu_count"], int)
            or isinstance(values["vcpu_count"], bool)
            or values["vcpu_count"] <= 0
            or not isinstance(values["icount_shift"], int)
            or isinstance(values["icount_shift"], bool)
        ):
            raise BackplaneFailure(
                "one-DUT replay requires positive vCPU count and integer icount shift"
            )
        if values["unconsumed_events"] != 0:
            raise BackplaneFailure("replay identity contains unconsumed events")
        for key in (field for field in IDENTITY_KEYS if field.endswith("sha256")):
            value = values[key]
            if (
                not isinstance(value, str)
                or len(value) != 64
                or any(c not in "0123456789abcdef" for c in value)
            ):
                raise BackplaneFailure(f"{key} must be a lowercase SHA-256")
        return cls(dict(values))

    @property
    def digest(self) -> str:
        return sha256_bytes(_canonical(self.values))

    def require_match(self, other: "ReplayIdentity") -> None:
        if self.digest != other.digest:
            changed = sorted(
                key for key in IDENTITY_KEYS if self.values[key] != other.values[key]
            )
            raise BackplaneFailure(f"replay identity mismatch: {','.join(changed)}")


@dataclass(frozen=True)
class DeliveryRecord:
    deadline_ns: int
    event_class_priority: int
    source_id: int
    destination_id: int
    sequence: int
    codec: str
    payload_hex: str


class FunctionalActor:
    def __init__(self, config: ActorConfig):
        self.config = config

    def consume(self, payload: bytes, virtual_now_ns: int) -> bytes | None:
        message = WireMessage.decode(payload)
        # Actors only map an immutable scenario transition to a production wire response.
        response: MessageType | None = None
        values: dict[str, int] = {}
        if message.message_type == MessageType.PING:
            response = MessageType.PONG
        elif self.config.role == "slave" and message.message_type in (
            MessageType.ARM_TOUCH,
            MessageType.SIMULATE_TOUCH,
        ):
            response = MessageType.TOUCH_EVENT
            values = {
                "round_token": message.round_token,
                "reaction_time_us": self.config.reaction_time_us,
                "pad_index": message.pad_index,
            }
        elif message.message_type == MessageType.BEACON:
            response = MessageType.BEACON
        elif message.message_type in (
            MessageType.JOIN_GAME,
            MessageType.SET_COLOR,
            MessageType.STOP_ALL,
        ):
            return None
        else:
            raise BackplaneFailure(
                f"unexpected {message.message_type.name} traffic for {self.config.role} actor"
            )
        return WireMessage(
            response, self.config.mac, virtual_now_ns // 1000, **values
        ).encode()


class DeterministicPeerBackplane:
    def __init__(self, scenario: ResolvedScenario):
        self.scenario = scenario
        self.queue = BoundedEventQueue(scenario.queue_capacity)
        self.actors = {
            actor.pod_id: FunctionalActor(actor) for actor in scenario.actors
        }
        self.records: list[DeliveryRecord] = []
        self.sequence = 0
        self.virtual_now_ns = 0
        self.expected_cursor = 0
        self.failed = False

    def _schedule(
        self,
        deadline_ns: int,
        event_class: EventClass,
        source_id: int,
        destination_id: int,
        payload: bytes,
    ) -> None:
        key = EventKey(
            deadline_ns, int(event_class), source_id, destination_id, self.sequence
        )
        self.sequence += 1
        self.queue.push(Event(key, payload))

    def submit_from_dut(
        self, destination_id: int, payload: bytes, virtual_now_ns: int
    ) -> None:
        if self.failed or virtual_now_ns < self.virtual_now_ns:
            raise BackplaneFailure("virtual time regressed or model already failed")
        self.virtual_now_ns = virtual_now_ns
        message = WireMessage.decode(payload)
        if self.expected_cursor >= len(self.scenario.expected_dut_types):
            self.failed = True
            raise BackplaneFailure(
                "unexpected DUT traffic after scenario replay exhausted"
            )
        expected = self.scenario.expected_dut_types[self.expected_cursor]
        if message.message_type != expected:
            self.failed = True
            raise BackplaneFailure(
                f"unexpected DUT traffic: expected {expected.name}, got {message.message_type.name}"
            )
        self.expected_cursor += 1
        destinations = (
            sorted(self.actors) if destination_id == BROADCAST_ID else [destination_id]
        )
        if any(destination not in self.actors for destination in destinations):
            self.failed = True
            raise BackplaneFailure("DUT addressed an unknown functional actor")
        for destination in destinations:
            actor = self.actors[destination]
            response = actor.consume(payload, virtual_now_ns)
            if response is not None:
                self._schedule(
                    virtual_now_ns + actor.config.peer_delay_ns,
                    EventClass.DUT_DELIVERY,
                    destination,
                    self.scenario.dut_id,
                    response,
                )

    def pop_due(self, virtual_now_ns: int) -> DeliveryRecord | None:
        if self.failed or virtual_now_ns < self.virtual_now_ns:
            raise BackplaneFailure("virtual time regressed or model already failed")
        self.virtual_now_ns = virtual_now_ns
        if (
            not self.queue._events
            or self.queue._events[0].key.deadline_ns > virtual_now_ns
        ):
            return None
        event = self.queue.pop()
        record = DeliveryRecord(
            *dataclasses.astuple(event.key), PRODUCTION_CODEC, event.payload.hex()
        )
        self.records.append(record)
        return record

    def finish(self, termination: str) -> bytes:
        unconsumed = (
            len(self.queue)
            + len(self.scenario.expected_dut_types)
            - self.expected_cursor
        )
        if self.failed or termination != "assertions_passed" or unconsumed != 0:
            raise BackplaneFailure(
                f"model did not terminate cleanly: termination={termination} unconsumed={unconsumed}"
            )
        return _canonical([dataclasses.asdict(record) for record in self.records])


def audit_virtual_time_sources(module_text: str) -> dict[str, bool]:
    prohibited = (
        "time.time",
        "time.monotonic",
        "datetime.now",
        "socket",
        "input",
        "id",
        "os.listdir",
        "Path.iterdir",
    )
    tree = ast.parse(module_text)
    calls = []
    imports = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            calls.append(ast.unparse(node.func))
        elif isinstance(node, (ast.Import, ast.ImportFrom)):
            imports.extend(alias.name for alias in node.names)
    return {
        token: token not in imports
        and all(call != token and not call.startswith(f"{token}.") for call in calls)
        for token in prohibited
    }
