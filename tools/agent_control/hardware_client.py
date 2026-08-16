#!/usr/bin/env python3
"""Sandbox-safe hardware-broker client; it never learns a device path."""

from __future__ import annotations

import argparse
import json
import os
import secrets
import sys
import time
from pathlib import Path
from typing import Any


def submit(capability_dir: Path, request: dict[str, Any]) -> tuple[str, Path]:
    cap = json.loads((capability_dir / "capability.json").read_text(encoding="utf-8"))
    request = {
        **request,
        "token": cap["token"],
        "issue": cap["issue"],
        "spec_revision": cap["spec_revision"],
        "pr_head": cap["pr_head"],
    }
    request_id = secrets.token_hex(16)
    queue = capability_dir / "requests"
    queue.mkdir(mode=0o700, exist_ok=True)
    target = queue / f"request-{request_id}.json"
    temporary = queue / f".request-{request_id}.tmp"
    encoded = json.dumps(request, sort_keys=True) + "\n"
    if len(encoded.encode("utf-8")) > 16_384:
        raise ValueError("hardware broker request exceeds 16 KiB")
    temporary.write_text(encoded, encoding="utf-8")
    os.chmod(temporary, 0o600)
    temporary.replace(target)
    return request_id, target


def request(
    capability_dir: Path, payload: dict[str, Any], timeout: float
) -> dict[str, Any]:
    request_id, _ = submit(capability_dir, payload)
    result = capability_dir / "results" / f"result-{request_id}.json"
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if result.is_file():
            return json.loads(result.read_text(encoding="utf-8"))
        time.sleep(0.05)
    raise TimeoutError("hardware broker response timed out")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capability-dir", type=Path, required=True)
    parser.add_argument("--operation", required=True)
    parser.add_argument("--board", type=int)
    parser.add_argument("--path")
    parser.add_argument("--timeout", type=float)
    args = parser.parse_args(argv)
    payload: dict[str, Any] = {"operation": args.operation}
    if args.board is not None:
        payload["board"] = args.board
    if args.path is not None:
        payload["path"] = args.path
    timeout = (
        args.timeout
        if args.timeout is not None
        else (
            1800.0
            if args.operation in {"flash", "flash-trace-acceptance", "ota"}
            else 60.0
        )
    )
    try:
        answer = request(args.capability_dir, payload, timeout)
    except (OSError, ValueError, TimeoutError) as error:
        print(json.dumps({"error": str(error)}))
        return 1
    print(json.dumps(answer, sort_keys=True))
    return 0 if not answer.get("error") and answer.get("returncode", 0) == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
