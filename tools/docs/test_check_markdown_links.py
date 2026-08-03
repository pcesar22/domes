import tempfile
import unittest
from pathlib import Path

from tools.docs.check_markdown_links import check_documents


class MarkdownLinkCheckerTest(unittest.TestCase):
    def _write(self, root: Path, relative: str, contents: str = "") -> Path:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def test_accepts_existing_relative_root_and_encoded_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._write(root, "README.md", "# Root\n")
            self._write(root, "assets/my file.txt")
            self._write(root, "assets/maintainer's-notes.md")
            document = self._write(
                root,
                "docs/guide.md",
                "[parent](../README.md)\n"
                "[root](/README.md#root)\n"
                "[directory](../assets/)\n"
                "[encoded](../assets/my%20file.txt)\n"
                "[angle](<../assets/my file.txt>)\n"
                "[query](../README.md?plain=1#root)\n"
                "[apostrophe](../assets/maintainer's-notes.md)\n"
                "[anchor](#section)\n"
                "[web](https://example.com/missing)\n"
                "[mail](mailto:maintainer@example.com)\n",
            )

            issues, checked = check_documents(root, [document])

        self.assertEqual(issues, [])
        self.assertEqual(checked, 7)

    def test_reports_broken_link_with_source_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = self._write(
                root, "docs/guide.md", "# Guide\n[missing](../missing.md)\n"
            )

            issues, checked = check_documents(root, [document])

        self.assertEqual(checked, 1)
        self.assertEqual(len(issues), 1)
        self.assertEqual(issues[0].line, 2)
        self.assertEqual(issues[0].target, "../missing.md")
        self.assertEqual(issues[0].reason, "target does not exist")

    def test_ignores_fenced_inline_and_commented_examples(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = self._write(
                root,
                "guide.md",
                "`[inline](missing-inline.md)`\n"
                "<!-- [comment](missing-comment.md) -->\n"
                "```markdown\n"
                "[fenced](missing-fenced.md)\n"
                "```\n",
            )

            issues, checked = check_documents(root, [document])

        self.assertEqual(issues, [])
        self.assertEqual(checked, 0)

    def test_checks_reference_definitions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._write(root, "specs/design.md")
            document = self._write(
                root,
                "docs/guide.md",
                "See [the design][design].\n\n"
                '[design]: ../specs/design.md "Design"\n',
            )

            issues, checked = check_documents(root, [document])

        self.assertEqual(issues, [])
        self.assertEqual(checked, 1)

    def test_rejects_targets_outside_repository(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = self._write(
                root, "docs/guide.md", "[outside](../../outside.md)\n"
            )

            issues, checked = check_documents(root, [document])

        self.assertEqual(checked, 1)
        self.assertEqual(len(issues), 1)
        self.assertEqual(issues[0].reason, "target escapes the repository")


if __name__ == "__main__":
    unittest.main()
