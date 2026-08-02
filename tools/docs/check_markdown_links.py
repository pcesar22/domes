#!/usr/bin/env python3
"""Check tracked Markdown files for broken repository-relative links."""

from __future__ import annotations

import argparse
import html
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import unquote

FENCE_RE = re.compile(r"^[ ]{0,3}(?P<marker>`{3,}|~{3,})")
REFERENCE_RE = re.compile(
    r"(?m)^[ ]{0,3}\[[^\]\n]+\]:[ \t]*"
    r"(?P<destination><(?:\\.|[^>\n])*>|(?:\\.|[^\s])+)",
)
SCHEME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")
MARKDOWN_ESCAPE_RE = re.compile(r"\\([ !\"#$%&'()*+,\-./:;<=>?@\[\]\\^_`{|}~])")


@dataclass(frozen=True)
class LinkIssue:
    document: Path
    line: int
    target: str
    resolved: Path
    reason: str


def _blank(value: str) -> str:
    return "".join(character if character in "\r\n" else " " for character in value)


def _strip_fenced_blocks(text: str) -> str:
    output: list[str] = []
    fence_character: str | None = None
    fence_length = 0

    for line in text.splitlines(keepends=True):
        match = FENCE_RE.match(line)
        if fence_character is None:
            if match is None:
                output.append(line)
                continue
            marker = match.group("marker")
            fence_character = marker[0]
            fence_length = len(marker)
            output.append(_blank(line))
            continue

        output.append(_blank(line))
        if match is None:
            continue
        marker = match.group("marker")
        remainder = line[match.end() :].strip()
        if (
            marker[0] == fence_character
            and len(marker) >= fence_length
            and not remainder
        ):
            fence_character = None
            fence_length = 0

    return "".join(output)


def _strip_html_comments(text: str) -> str:
    output = list(text)
    cursor = 0
    while True:
        start = text.find("<!--", cursor)
        if start < 0:
            break
        end = text.find("-->", start + 4)
        end = len(text) if end < 0 else end + 3
        for index in range(start, end):
            if output[index] not in "\r\n":
                output[index] = " "
        cursor = end
    return "".join(output)


def _strip_inline_code(text: str) -> str:
    output = list(text)
    cursor = 0
    while cursor < len(text):
        if text[cursor] != "`":
            cursor += 1
            continue

        run_end = cursor
        while run_end < len(text) and text[run_end] == "`":
            run_end += 1
        marker = text[cursor:run_end]
        closing = text.find(marker, run_end)
        if closing < 0:
            cursor = run_end
            continue

        end = closing + len(marker)
        for index in range(cursor, end):
            if output[index] not in "\r\n":
                output[index] = " "
        cursor = end
    return "".join(output)


def sanitize_markdown(text: str) -> str:
    """Blank content that Markdown renders as code or an HTML comment."""

    return _strip_inline_code(_strip_html_comments(_strip_fenced_blocks(text)))


def _is_escaped(text: str, index: int) -> bool:
    backslashes = 0
    index -= 1
    while index >= 0 and text[index] == "\\":
        backslashes += 1
        index -= 1
    return backslashes % 2 == 1


def _find_inline_link_end(text: str, opening: int) -> int | None:
    depth = 1
    cursor = opening + 1
    angle_destination = False
    quote: str | None = None
    seen_non_space = False
    title_can_start = False

    while cursor < len(text):
        character = text[cursor]
        if character in "\r\n" and depth == 1:
            return None
        if _is_escaped(text, cursor):
            cursor += 1
            continue
        if angle_destination:
            if character == ">":
                angle_destination = False
            cursor += 1
            continue
        if quote is not None:
            if character == quote:
                quote = None
            cursor += 1
            continue
        if not seen_non_space and character.isspace():
            cursor += 1
            continue
        if not seen_non_space and character == "<":
            angle_destination = True
            seen_non_space = True
            cursor += 1
            continue

        if character.isspace() and depth == 1:
            title_can_start = seen_non_space
            cursor += 1
            continue
        seen_non_space = True
        if title_can_start and character in "\"'":
            quote = character
            title_can_start = False
        elif character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
            if depth == 0:
                return cursor
        else:
            title_can_start = False
        cursor += 1

    return None


def _parse_destination(contents: str) -> str:
    contents = contents.lstrip()
    if not contents:
        return ""
    if contents.startswith("<"):
        closing = contents.find(">", 1)
        return "" if closing < 0 else contents[1:closing]

    cursor = 0
    while cursor < len(contents):
        if contents[cursor].isspace() and not _is_escaped(contents, cursor):
            break
        cursor += 1
    return contents[:cursor]


def iter_link_targets(text: str) -> list[tuple[str, int]]:
    """Return Markdown link destinations and their one-based line numbers."""

    sanitized = sanitize_markdown(text)
    links: list[tuple[str, int]] = []

    cursor = 0
    while True:
        closing_label = sanitized.find("](", cursor)
        if closing_label < 0:
            break
        cursor = closing_label + 2
        if _is_escaped(sanitized, closing_label):
            continue
        line_start = sanitized.rfind("\n", 0, closing_label) + 1
        if sanitized.rfind("[", line_start, closing_label) < 0:
            continue

        closing_link = _find_inline_link_end(sanitized, closing_label + 1)
        if closing_link is None:
            continue
        destination = _parse_destination(sanitized[closing_label + 2 : closing_link])
        if destination:
            line = sanitized.count("\n", 0, closing_label) + 1
            links.append((destination, line))

    for match in REFERENCE_RE.finditer(sanitized):
        destination = match.group("destination")
        if destination.startswith("<") and destination.endswith(">"):
            destination = destination[1:-1]
        line = sanitized.count("\n", 0, match.start()) + 1
        links.append((destination, line))

    return links


def _resolve_local_target(
    repo_root: Path, document: Path, target: str
) -> tuple[Path, str] | None:
    target = html.unescape(target.strip())
    target = MARKDOWN_ESCAPE_RE.sub(r"\1", target)
    if not target or target.startswith("#") or target.startswith("//"):
        return None
    if SCHEME_RE.match(target):
        return None

    path_text = target.split("#", 1)[0].split("?", 1)[0]
    if not path_text:
        return None
    path_text = unquote(path_text)
    if path_text.startswith("/"):
        resolved = (repo_root / path_text.lstrip("/")).resolve()
    else:
        resolved = (document.parent / path_text).resolve()

    try:
        resolved.relative_to(repo_root)
    except ValueError:
        return resolved, "target escapes the repository"
    if not resolved.exists():
        return resolved, "target does not exist"
    return None


def check_documents(
    repo_root: Path, documents: list[Path]
) -> tuple[list[LinkIssue], int]:
    repo_root = repo_root.resolve()
    issues: list[LinkIssue] = []
    checked_links = 0

    for document in sorted(path.resolve() for path in documents):
        text = document.read_text(encoding="utf-8")
        for target, line in iter_link_targets(text):
            problem = _resolve_local_target(repo_root, document, target)
            if problem is None:
                if not (
                    target.startswith("#")
                    or target.startswith("//")
                    or SCHEME_RE.match(html.unescape(target.strip()))
                ):
                    checked_links += 1
                continue
            checked_links += 1
            resolved, reason = problem
            issues.append(LinkIssue(document, line, target, resolved, reason))

    return issues, checked_links


def tracked_markdown_files(repo_root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(repo_root), "ls-files", "-z", "--", "*.md"],
        check=True,
        capture_output=True,
    )
    paths = result.stdout.decode("utf-8", errors="surrogateescape").split("\0")
    return [repo_root / path for path in paths if path]


def selected_markdown_files(repo_root: Path, values: list[str]) -> list[Path]:
    if not values:
        return tracked_markdown_files(repo_root)

    documents: set[Path] = set()
    for value in values:
        path = Path(value)
        path = path if path.is_absolute() else repo_root / path
        if path.is_dir():
            documents.update(path.rglob("*.md"))
        else:
            documents.add(path)
    return sorted(documents)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "paths",
        nargs="*",
        help="Markdown files or directories; defaults to tracked Markdown files",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    documents = selected_markdown_files(repo_root, args.paths)
    missing_documents = [path for path in documents if not path.is_file()]
    if missing_documents:
        for document in missing_documents:
            print(f"missing Markdown document: {document}", file=sys.stderr)
        return 1

    issues, checked_links = check_documents(repo_root, documents)
    for issue in issues:
        try:
            document = issue.document.relative_to(repo_root)
        except ValueError:
            document = issue.document
        try:
            resolved = issue.resolved.relative_to(repo_root)
        except ValueError:
            resolved = issue.resolved
        print(
            f"{document}:{issue.line}: broken relative link {issue.target!r}: "
            f"{issue.reason} ({resolved})",
            file=sys.stderr,
        )

    if issues:
        print(
            f"found {len(issues)} broken relative link(s) in "
            f"{len(documents)} Markdown file(s)",
            file=sys.stderr,
        )
        return 1

    print(
        f"Markdown links are current ({len(documents)} files, "
        f"{checked_links} relative links)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
