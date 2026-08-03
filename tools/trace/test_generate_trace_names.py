import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

from tools.trace.generate_trace_names import (
    check_registry,
    collect_trace_names,
    fnv1a_32,
    render_registry,
    strip_cpp_comments,
)


class TraceNameGeneratorTest(unittest.TestCase):
    def test_fnv1a_matches_firmware_known_value(self) -> None:
        self.assertEqual(fnv1a_32("Game.Tick"), 19_912_354)

    def test_collects_unique_production_literals(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "example.cpp").write_text(
                'TRACE_SCOPE(TRACE_ID("Example.Scope"), category);\n'
                'TRACE_INSTANT(TRACE_ID("Example.Event"), category);\n'
                'TRACE_SCOPE(TRACE_ID("Example.Scope"), category);\n',
                encoding="utf-8",
            )
            names = collect_trace_names(root)

        self.assertEqual(len(names), 2)
        self.assertEqual(names[str(fnv1a_32("Example.Scope"))], "Example.Scope")
        self.assertEqual(names[str(fnv1a_32("Example.Event"))], "Example.Event")

    def test_comment_examples_are_not_collected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "example.cpp").write_text(
                'TRACE_INSTANT(TRACE_ID("Production.Event"), category);\n'
                '// TRACE_INSTANT(TRACE_ID("Comment.Line"), category);\n'
                '/* TRACE_SCOPE(TRACE_ID("Comment.Block"), category); */\n'
                'const char* url = "https://example.com/path";\n',
                encoding="utf-8",
            )

            names = collect_trace_names(root)

        self.assertEqual(names, {str(fnv1a_32("Production.Event")): "Production.Event"})

    def test_comment_markers_inside_strings_are_preserved(self) -> None:
        source = (
            'TRACE_INSTANT(TRACE_ID("Url.http://request"), category);\n'
            "const char slash = '/'; // real comment\n"
        )

        stripped = strip_cpp_comments(source)

        self.assertIn('TRACE_ID("Url.http://request")', stripped)
        self.assertIn("const char slash = '/';", stripped)
        self.assertNotIn("real comment", stripped)

    def test_check_detects_stale_registry(self) -> None:
        expected = {str(fnv1a_32("Current")): "Current"}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "trace_names.json"
            output.write_text(json.dumps({"1": "Stale"}), encoding="utf-8")
            with redirect_stderr(io.StringIO()):
                self.assertFalse(check_registry(output, expected))

            output.write_text(render_registry(expected), encoding="utf-8")
            self.assertTrue(check_registry(output, expected))


if __name__ == "__main__":
    unittest.main()
