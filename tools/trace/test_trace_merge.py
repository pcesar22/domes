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
        return next(
            event
            for event in events
            if event.get("name") == name and event.get("pid") == pid
        )

    def merge(self, paths, mode, names=None):
        pod_names = [f"pod-{index}" for index in range(len(paths))]
        with contextlib.redirect_stderr(io.StringIO()):
            return merge_traces(
                paths,
                pod_names,
                names or {},
                mode,
            )

    def test_metadata_only_input_is_preserved_in_every_alignment_mode(self):
        metadata = {
            "ph": "M",
            "pid": 99,
            "tid": 37,
            "name": "source_metadata",
            "args": {"name": "original thread"},
        }
        path = self.write_trace("metadata.json", [metadata])

        for mode in ("zero", "raw"):
            with self.subTest(mode=mode):
                merged = self.merge([path], mode)
                preserved = self.event_named(merged, "source_metadata")
                self.assertNotIn("ts", preserved)
                self.assertEqual(preserved["pid"], 0)
                self.assertEqual(preserved["tid"], 37)
                self.assertEqual(preserved["args"], metadata["args"])

    def test_zero_alignment_uses_only_timestamped_events(self):
        path = self.write_trace(
            "zero.json",
            [
                {"ph": "M", "name": "source_metadata", "args": {"name": "pod"}},
                {"ph": "i", "name": "first", "ts": 25, "cat": "game"},
                {"ph": "i", "name": "second", "ts": 40, "cat": "game"},
            ],
        )

        merged = self.merge([path], "zero")

        self.assertEqual(self.event_named(merged, "first")["ts"], 0)
        self.assertEqual(self.event_named(merged, "second")["ts"], 15)
        self.assertNotIn("ts", self.event_named(merged, "source_metadata"))

    def test_raw_alignment_leaves_timestamps_unchanged(self):
        path = self.write_trace(
            "raw.json",
            [
                {"ph": "M", "tid": 4, "name": "source_metadata", "args": {}},
                {"ph": "i", "name": "sample", "ts": 1234, "cat": "touch"},
            ],
        )

        merged = self.merge([path], "raw")

        self.assertEqual(self.event_named(merged, "sample")["ts"], 1234)
        self.assertEqual(self.event_named(merged, "source_metadata")["tid"], 4)

    def test_sync_category_has_a_dedicated_lane(self):
        path = self.write_trace(
            "sync.json",
            [{"ph": "i", "name": "mutex", "ts": 10, "cat": "sync"}],
        )

        merged = self.merge([path], "zero")

        self.assertEqual(self.event_named(merged, "mutex")["tid"], 14)


if __name__ == "__main__":
    unittest.main()
