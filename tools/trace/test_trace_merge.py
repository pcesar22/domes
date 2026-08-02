#!/usr/bin/env python3
"""Focused tests for Perfetto trace merging and timestamp alignment."""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from trace_merge import merge_traces


class TraceMergeTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)

    def write_trace(self, name, events):
        path = Path(self.temp_dir.name) / name
        path.write_text(json.dumps({"traceEvents": events}), encoding="utf-8")
        return path

    @staticmethod
    def event_named(events, name, pid=0):
        return next(event for event in events
                    if event.get("name") == name and event.get("pid") == pid)

    def merge(self, paths, mode, names=None):
        pod_names = [f"pod-{index}" for index in range(len(paths))]
        with contextlib.redirect_stderr(io.StringIO()):
            return merge_traces(paths, pod_names, names or {}, mode)

    def test_metadata_only_input_is_preserved_in_every_alignment_mode(self):
        metadata = {
            "ph": "M",
            "pid": 99,
            "tid": 37,
            "name": "source_metadata",
            "args": {"name": "original thread"},
        }
        path = self.write_trace("metadata.json", [metadata])

        for mode in ("zero", "raw", "beacon"):
            with self.subTest(mode=mode):
                merged = self.merge([path], mode)
                preserved = self.event_named(merged, "source_metadata")
                self.assertNotIn("ts", preserved)
                self.assertEqual(preserved["pid"], 0)
                self.assertEqual(preserved["tid"], 37)
                self.assertEqual(preserved["args"], metadata["args"])

    def test_zero_alignment_uses_only_timestamped_events(self):
        path = self.write_trace("zero.json", [
            {"ph": "M", "name": "source_metadata", "args": {"name": "pod"}},
            {"ph": "i", "name": "first", "ts": 25, "cat": "game"},
            {"ph": "i", "name": "second", "ts": 40, "cat": "game"},
        ])

        merged = self.merge([path], "zero")

        self.assertEqual(self.event_named(merged, "first")["ts"], 0)
        self.assertEqual(self.event_named(merged, "second")["ts"], 15)
        self.assertNotIn("ts", self.event_named(merged, "source_metadata"))

    def test_raw_alignment_leaves_timestamps_unchanged(self):
        path = self.write_trace("raw.json", [
            {"ph": "M", "tid": 4, "name": "source_metadata", "args": {}},
            {"ph": "i", "name": "sample", "ts": 1234, "cat": "touch"},
        ])

        merged = self.merge([path], "raw")

        self.assertEqual(self.event_named(merged, "sample")["ts"], 1234)
        self.assertEqual(self.event_named(merged, "source_metadata")["tid"], 4)

    def test_beacon_alignment_ignores_metadata_and_aligns_first_beacon(self):
        first = self.write_trace("beacon-first.json", [
            {"ph": "M", "tid": 8, "name": "first_metadata", "args": {}},
            {"ph": "B", "name": "span:7", "ts": 100, "cat": "espnow"},
            {"ph": "i", "name": "first_after", "ts": 130, "cat": "game"},
        ])
        second = self.write_trace("beacon-second.json", [
            {"ph": "M", "tid": 9, "name": "second_metadata", "args": {}},
            {"ph": "B", "name": "EspNow.SendBeacon", "ts": 250, "cat": "espnow"},
            {"ph": "i", "name": "second_after", "ts": 300, "cat": "game"},
        ])

        merged = self.merge([first, second], "beacon", {"7": "EspNow.SendBeacon"})

        self.assertEqual(self.event_named(merged, "EspNow.SendBeacon", 0)["ts"], 100)
        self.assertEqual(self.event_named(merged, "EspNow.SendBeacon", 1)["ts"], 100)
        self.assertEqual(self.event_named(merged, "first_after", 0)["ts"], 130)
        self.assertEqual(self.event_named(merged, "second_after", 1)["ts"], 150)
        self.assertNotIn("ts", self.event_named(merged, "second_metadata", 1))

    def test_beacon_fallback_zero_aligns_mixed_and_metadata_only_inputs(self):
        metadata_only = self.write_trace("fallback-metadata.json", [
            {"ph": "M", "tid": 2, "name": "metadata_only", "args": {}},
        ])
        mixed = self.write_trace("fallback-mixed.json", [
            {"ph": "M", "name": "mixed_metadata", "args": {}},
            {"ph": "i", "name": "mixed_sample", "ts": 75, "cat": "kernel"},
        ])

        merged = self.merge([metadata_only, mixed], "beacon")

        self.assertNotIn("ts", self.event_named(merged, "metadata_only", 0))
        self.assertNotIn("ts", self.event_named(merged, "mixed_metadata", 1))
        self.assertEqual(self.event_named(merged, "mixed_sample", 1)["ts"], 0)


if __name__ == "__main__":
    unittest.main()
