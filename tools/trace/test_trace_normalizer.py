#!/usr/bin/env python3

import hashlib
import struct
import unittest

from tools.trace.trace_normalizer import (
    PROBE_OBJECTS,
    TraceNormalizationError,
    normalize_trace,
    object_map_from_qemu_log,
    raw_from_qemu_log,
    validate_session,
)


def event(sequence, event_type, arg1, arg2=0, task=1, flags=1):
    return struct.pack("<IHBBII", 100 + sequence, task, event_type, flags, arg1, arg2)


class TraceNormalizerTests(unittest.TestCase):
    def setUp(self):
        self.manifest = {
            "tasks": [
                {
                    "presence": "required",
                    "trace_id": 1,
                    "name": "main",
                    "priority": 1,
                    "core_affinity": 0,
                }
            ]
        }
        chain = [
            (0x1B, 6),
            (0x16, 3),
            (0x1C, 4),
            (0x19, 1),
            (0x0D, 2),
            (0x1D, 4),
            (0x17, 3),
            (0x1A, 1),
            (0x0C, 2),
            (0x1E, 5),
        ]
        self.raw = (
            event(0, 0x10, 1, 1)
            + b"".join(
                event(
                    index + 1,
                    kind,
                    object_id,
                    1,
                    task=0 if kind in (0x16, 0x17, 0x1C, 0x1D, 0x19, 0x0D) else 1,
                    flags=(
                        5
                        if kind in (0x16, 0x17, 0x19, 0x0D)
                        else (9 if kind in (0x1C, 0x1D) else 1)
                    ),
                )
                for index, (kind, object_id) in enumerate(chain)
            )
            + event(11, 0x25, 4, 20)
        )

    def session(self, raw=None):
        raw = self.raw if raw is None else raw
        return {
            "format_version": 1,
            "event_count": len(raw) // 16,
            "start_timestamp_us": struct.unpack_from("<I", raw, 0)[0],
            "end_timestamp_us": struct.unpack_from("<I", raw, len(raw) - 16)[0],
            "buffer_size_bytes": 16 * 1024,
            "firmware_version": "host-test",
            "app_elf_sha256": "a5" * 32,
            "app_image_sha256": "5a" * 32,
            "device_uid": "020000000001",
            "transport": {
                "device_name": "test-pod",
                "type": "serial",
                "address": "/dev/serial/by-id/test-pod",
            },
            "candidate_image": None,
            "received_raw_bytes": len(raw),
            "raw_sha256": hashlib.sha256(raw).hexdigest(),
            "integrity_error": None,
            "tasks": [
                {"task_id": 1, "name": "main", "priority": 1, "core_affinity_mask": 1}
            ],
            "objects": [
                {"object_id": 1, "kind": 1, "name": "probe_queue"},
                {"object_id": 2, "kind": 2, "name": "probe_sem"},
                {"object_id": 3, "kind": 3, "name": "probe_irq"},
                {"object_id": 4, "kind": 4, "name": "probe_callback"},
                {"object_id": 5, "kind": 5, "name": "probe_action"},
                {"object_id": 6, "kind": 7, "name": "probe_timeout"},
            ],
        }

    def test_decodes_little_endian_and_validates_causal_chain(self):
        normalized = normalize_trace(self.raw, self.manifest, objects=PROBE_OBJECTS)
        self.assertEqual(normalized["events"][0]["relative_us"], 0)
        self.assertEqual(normalized["causal_positions"], list(range(1, 11)))
        self.assertEqual(normalized["normalizer"]["version"], "1.0.1")

    def test_rejects_overflow_unresolved_ids_and_truncation(self):
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(self.raw, self.manifest, objects=PROBE_OBJECTS, dropped=1)
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(
                event(0, 0x1E, 5, 1, task=99), self.manifest, objects=PROBE_OBJECTS
            )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(self.raw[:-1], self.manifest, objects=PROBE_OBJECTS)

    def test_extracts_contiguous_raw_log_rows(self):
        first, second = self.raw[:16], self.raw[16:32]
        text = (
            f"I trace: DOMES_QEMU_TRACE schema=1 index=0 raw={first.hex()}\n"
            f"I trace: DOMES_QEMU_TRACE schema=1 index=1 raw={second.hex()}\n"
        )
        self.assertEqual(raw_from_qemu_log(text), first + second)
        with self.assertRaises(TraceNormalizationError):
            raw_from_qemu_log(text.replace("index=1", "index=2"))
        with self.assertRaises(TraceNormalizationError):
            raw_from_qemu_log("\n".join(reversed(text.splitlines())))
        with self.assertRaises(TraceNormalizationError):
            raw_from_qemu_log(
                text + "I trace: DOMES_QEMU_TRACE schema=2 index=2 raw=" + first.hex()
            )

    def test_requires_explicit_stable_object_mapping(self):
        marker = (
            "I trace: DOMES_QEMU_TRACE_SESSION schema=1 "
            "objects=1:1:probe_queue,2:2:probe_sem,3:3:probe_irq,"
            "4:4:probe_callback,5:5:probe_action,6:7:probe_timeout\n"
        )
        self.assertEqual(object_map_from_qemu_log(marker), PROBE_OBJECTS)
        with self.assertRaises(TraceNormalizationError):
            object_map_from_qemu_log(marker.replace("1:1:probe_queue", "1:6:mutex"))
        with self.assertRaises(TraceNormalizationError):
            object_map_from_qemu_log(
                marker.replace("1:1:probe_queue", "1:2:probe_queue")
            )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(self.raw, self.manifest, objects={1: "probe_queue"})

    def test_rejects_unbalanced_switch_affinity_and_unresolved_interrupt(self):
        switch_out = self.raw + event(20, 0x15, 0, task=1)
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(switch_out, self.manifest, objects=PROBE_OBJECTS)
        wrong_core = bytearray(self.raw)
        wrong_core[-9] = 2  # flags byte of the final task-owned completion event
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(bytes(wrong_core), self.manifest, objects=PROBE_OBJECTS)
        unresolved = self.raw.replace(
            event(2, 0x16, 3, 1, task=0, flags=5),
            event(2, 0x16, 999, 1, task=0, flags=5),
        )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(unresolved, self.manifest, objects=PROBE_OBJECTS)

    def test_rejects_semaphore_take_before_give(self):
        rows = [self.raw[index : index + 16] for index in range(0, len(self.raw), 16)]
        rows[5], rows[9] = rows[9], rows[5]
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(b"".join(rows), self.manifest, objects=PROBE_OBJECTS)

    def test_rejects_duplicate_or_extra_causal_edges(self):
        duplicate = (
            self.raw[:-16] + event(12, 0x0D, 2, 1, task=0, flags=5) + self.raw[-16:]
        )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(duplicate, self.manifest, objects=PROBE_OBJECTS)

    def test_rejects_zero_or_inverted_overhead_measurement(self):
        for disabled, enabled in ((0, 20), (4, 0), (20, 4), (4, 4)):
            raw = self.raw[:-16] + event(11, 0x25, disabled, enabled)
            with self.subTest(disabled=disabled, enabled=enabled):
                with self.assertRaises(TraceNormalizationError):
                    normalize_trace(raw, self.manifest, objects=PROBE_OBJECTS)

    def test_rejects_timestamp_regression_and_accepts_one_wrap(self):
        regressed = bytearray(self.raw)
        struct.pack_into("<I", regressed, 5 * 16, 50)
        with self.assertRaisesRegex(TraceNormalizationError, "timestamp regression"):
            normalize_trace(bytes(regressed), self.manifest, objects=PROBE_OBJECTS)

        wrapped = bytearray(self.raw)
        start = 0xFFFFFFF9
        for index in range(len(wrapped) // 16):
            struct.pack_into("<I", wrapped, index * 16, (start + index) & 0xFFFFFFFF)
        normalized = normalize_trace(
            bytes(wrapped), self.manifest, objects=PROBE_OBJECTS
        )
        self.assertEqual(
            [event["relative_us"] for event in normalized["events"]],
            list(range(len(wrapped) // 16)),
        )

    def test_rejects_unknown_category_use_after_delete_and_wrong_isr_context(self):
        unknown_category = bytearray(self.raw)
        unknown_category[7] = 0xF1
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(
                bytes(unknown_category), self.manifest, objects=PROBE_OBJECTS
            )
        use_after_delete = self.raw + event(12, 0x11, 0) + event(13, 0x23, 1)
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(use_after_delete, self.manifest, objects=PROBE_OBJECTS)
        wrong_context = self.raw + event(12, 0x16, 3, 0, task=0, flags=1)
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(wrong_context, self.manifest, objects=PROBE_OBJECTS)

    def test_rejects_switch_below_highest_ready_priority(self):
        manifest = {
            "tasks": self.manifest["tasks"]
            + [
                {
                    "presence": "required",
                    "trace_id": 2,
                    "name": "high",
                    "priority": 10,
                    "core_affinity": 0,
                }
            ]
        }
        second_create = event(1, 0x10, 10, 1, task=2)
        raw = self.raw[:16] + second_create + self.raw[16:]
        raw += (
            event(20, 0x12, 0, task=2)
            + event(21, 0x14, 0, task=1)
            + event(22, 0x15, 0, task=1)
        )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(raw, manifest, objects=PROBE_OBJECTS)

    def test_rejects_unresolved_block_mutex_and_nonrunning_block(self):
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(
                self.raw + event(20, 0x13, 999), self.manifest, objects=PROBE_OBJECTS
            )
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(
                self.raw + event(20, 0x09, 0xDEADBEEF),
                self.manifest,
                objects=PROBE_OBJECTS,
            )
        manifest = {
            "tasks": self.manifest["tasks"]
            + [
                {
                    "presence": "required",
                    "trace_id": 2,
                    "name": "other",
                    "priority": 1,
                    "core_affinity": 0,
                }
            ]
        }
        raw = self.raw[:16] + event(1, 0x10, 1, 1, task=2) + self.raw[16:]
        raw += event(20, 0x14, 0, task=1) + event(21, 0x13, 1, task=2)
        with self.assertRaises(TraceNormalizationError):
            normalize_trace(raw, manifest, objects=PROBE_OBJECTS)

    def test_validates_session_binding_and_full_object_catalog(self):
        session = self.session()
        self.assertEqual(validate_session(self.raw, session), PROBE_OBJECTS)
        bound_candidate = self.session()
        bound_candidate["candidate_image"] = {
            "path": "/tmp/domes.bin",
            "file_sha256": "12" * 32,
            "app_image_sha256": bound_candidate["app_image_sha256"],
            "app_elf_sha256": bound_candidate["app_elf_sha256"],
            "firmware_version": bound_candidate["firmware_version"],
            "binding_verified": True,
        }
        self.assertEqual(validate_session(self.raw, bound_candidate), PROBE_OBJECTS)
        mutations = []
        for key, value in (
            ("format_version", 2),
            ("event_count", 1),
            ("start_timestamp_us", 0),
            ("end_timestamp_us", 0),
            ("raw_sha256", "0" * 64),
            ("app_elf_sha256", "0" * 64),
            ("device_uid", "03" + "00" * 5),
        ):
            mutated = self.session()
            mutated[key] = value
            mutations.append(mutated)
        duplicate = self.session()
        duplicate["objects"].append(dict(duplicate["objects"][0]))
        mutations.append(duplicate)
        wrong_kind = self.session()
        wrong_kind["objects"][0]["kind"] = 6
        mutations.append(wrong_kind)
        duplicate_task = self.session()
        duplicate_task["tasks"].append(dict(duplicate_task["tasks"][0]))
        mutations.append(duplicate_task)
        bad_received_size = self.session()
        bad_received_size["received_raw_bytes"] -= 1
        mutations.append(bad_received_size)
        integrity_error = self.session()
        integrity_error["integrity_error"] = "offset mismatch"
        mutations.append(integrity_error)
        invalid_candidate = bound_candidate
        invalid_candidate["candidate_image"]["binding_verified"] = False
        mutations.append(invalid_candidate)
        oversized_buffer = self.session()
        oversized_buffer["buffer_size_bytes"] = 32 * 1024 + 1
        mutations.append(oversized_buffer)
        long_task_name = self.session()
        long_task_name["tasks"][0]["name"] = "1234567890abcdef"
        mutations.append(long_task_name)
        for mutated in mutations:
            with self.subTest(mutated=mutated):
                with self.assertRaises(TraceNormalizationError):
                    validate_session(self.raw, mutated)


if __name__ == "__main__":
    unittest.main()
