#!/usr/bin/env python3
"""Generate the host trace-name registry from firmware TRACE_ID literals."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

TRACE_ID_PATTERN = re.compile(r'TRACE_ID\s*\(\s*"([^"\\]+)"\s*\)')
SOURCE_SUFFIXES = {".cpp", ".hpp"}


def fnv1a_32(value: str) -> int:
    hash_value = 2_166_136_261
    for byte in value.encode("utf-8"):
        hash_value ^= byte
        hash_value = (hash_value * 16_777_619) & 0xFFFF_FFFF
    return hash_value


def strip_cpp_comments(source: str) -> str:
    """Remove C++ comments while preserving strings, characters, and line numbers."""

    output = list(source)
    cursor = 0
    state = "code"

    while cursor < len(source):
        character = source[cursor]
        following = source[cursor + 1] if cursor + 1 < len(source) else ""

        if state == "code":
            if character == "/" and following == "/":
                output[cursor] = " "
                output[cursor + 1] = " "
                cursor += 2
                state = "line-comment"
                continue
            if character == "/" and following == "*":
                output[cursor] = " "
                output[cursor + 1] = " "
                cursor += 2
                state = "block-comment"
                continue
            if character == '"':
                state = "string"
            elif character == "'":
                state = "character"
            cursor += 1
            continue

        if state == "line-comment":
            if character in "\r\n":
                state = "code"
            else:
                output[cursor] = " "
            cursor += 1
            continue

        if state == "block-comment":
            if character == "*" and following == "/":
                output[cursor] = " "
                output[cursor + 1] = " "
                cursor += 2
                state = "code"
                continue
            if character not in "\r\n":
                output[cursor] = " "
            cursor += 1
            continue

        if character == "\\" and cursor + 1 < len(source):
            cursor += 2
            continue
        if (state == "string" and character == '"') or (
            state == "character" and character == "'"
        ):
            state = "code"
        cursor += 1

    return "".join(output)


def collect_trace_names(source_root: Path) -> dict[str, str]:
    names: dict[str, str] = {}
    for source in sorted(source_root.rglob("*")):
        if source.suffix not in SOURCE_SUFFIXES:
            continue
        source_text = source.read_text(encoding="utf-8")
        for name in TRACE_ID_PATTERN.findall(strip_cpp_comments(source_text)):
            trace_id = str(fnv1a_32(name))
            previous = names.get(trace_id)
            if previous is not None and previous != name:
                raise ValueError(
                    f"FNV-1a collision for {trace_id}: {previous!r} and {name!r}"
                )
            names[trace_id] = name

    return dict(sorted(names.items(), key=lambda item: int(item[0])))


def render_registry(names: dict[str, str]) -> str:
    return json.dumps(names, indent=2) + "\n"


def check_registry(output: Path, expected: dict[str, str]) -> bool:
    try:
        current = json.loads(output.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError) as error:
        print(f"trace registry is missing or invalid: {error}", file=sys.stderr)
        return False

    if current == expected:
        return True

    current_keys = set(current)
    expected_keys = set(expected)
    missing = sorted(expected_keys - current_keys, key=int)
    stale = sorted(current_keys - expected_keys, key=int)
    changed = sorted(
        (key for key in current_keys & expected_keys if current[key] != expected[key]),
        key=int,
    )
    if missing:
        print(
            "missing trace names: " + ", ".join(expected[key] for key in missing),
            file=sys.stderr,
        )
    if stale:
        print(
            "stale trace names: " + ", ".join(str(current[key]) for key in stale),
            file=sys.stderr,
        )
    if changed:
        print("hash/name mismatches: " + ", ".join(changed), file=sys.stderr)
    return False


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-root",
        type=Path,
        default=repo_root / "firmware",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().with_name("trace_names.json"),
    )
    parser.add_argument(
        "--check", action="store_true", help="fail if the registry is stale"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    expected = collect_trace_names(args.source_root)
    if not expected:
        print(f"no TRACE_ID literals found under {args.source_root}", file=sys.stderr)
        return 1

    if args.check:
        if not check_registry(args.output, expected):
            print(
                "run tools/trace/generate_trace_names.py to refresh the registry",
                file=sys.stderr,
            )
            return 1
        print(f"trace registry is current ({len(expected)} names)")
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render_registry(expected), encoding="utf-8")
    print(f"wrote {len(expected)} trace names to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
