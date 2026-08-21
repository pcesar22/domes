#!/usr/bin/env python3
"""Compare independently emitted fixed-simulator and mobile scoring results."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


class FixtureError(ValueError):
    """The fixture cannot be compared without guessing."""


class ResultError(ValueError):
    """A path result cannot be attributed to the immutable fixture."""


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def load_fixture(path: Path) -> tuple[dict[str, Any], str]:
    raw = path.read_bytes()
    try:
        fixture = json.loads(raw)
    except json.JSONDecodeError as error:
        raise FixtureError(f"malformed JSON: {error.msg}") from error
    if not isinstance(fixture, dict):
        raise FixtureError("fixture root must be an object")
    validate_fixture(fixture)
    return fixture, sha256_bytes(raw)


def _required(mapping: dict[str, Any], key: str, context: str) -> Any:
    if key not in mapping:
        raise FixtureError(f"{context} missing {key}")
    return mapping[key]


def validate_fixture(fixture: dict[str, Any]) -> None:
    if fixture.get("schema_version") != 1:
        raise FixtureError("unsupported or missing schema_version")
    for field in ("fixture_id", "specification_revision", "paths", "pods", "rounds"):
        _required(fixture, field, "fixture")

    paths = fixture["paths"]
    if not isinstance(paths, dict):
        raise FixtureError("paths must be an object")
    for name in ("fixed", "mobile"):
        path = _required(paths, name, "paths")
        if not isinstance(path, dict):
            raise FixtureError(f"paths.{name} must be an object")
        clock = _required(path, "clock", f"paths.{name}")
        result = _required(path, "result", f"paths.{name}")
        if not isinstance(clock, dict) or not all(
            clock.get(key) for key in ("kind", "origin", "unit")
        ):
            raise FixtureError(f"paths.{name} clock provenance is incomplete")
        if not isinstance(result, dict) or not result.get("origin"):
            raise FixtureError(f"paths.{name} result provenance is incomplete")
        resolution = result.get("reaction_resolution_us")
        if not isinstance(resolution, int) or resolution <= 0:
            raise FixtureError(
                f"paths.{name} reaction resolution is missing or invalid"
            )

    pods = fixture["pods"]
    if not isinstance(pods, list) or len(pods) != 2:
        raise FixtureError("pods must identify exactly two participants")
    identities: set[str] = set()
    fixed_ids: set[int] = set()
    addresses: set[str] = set()
    roles: set[str] = set()
    for index, pod in enumerate(pods):
        if not isinstance(pod, dict):
            raise FixtureError(f"pod {index} must be an object")
        identity = _required(pod, "identity", f"pod {index}")
        fixed_id = _required(pod, "fixed_id", f"pod {index}")
        address = _required(pod, "mobile_address", f"pod {index}")
        role = _required(pod, "role", f"pod {index}")
        if not isinstance(identity, str) or not identity or identity in identities:
            raise FixtureError("pod identity is missing or ambiguous")
        if not isinstance(fixed_id, int) or fixed_id in fixed_ids:
            raise FixtureError("fixed pod identity is missing or ambiguous")
        if not isinstance(address, str) or not address or address in addresses:
            raise FixtureError("mobile pod identity is missing or ambiguous")
        if role not in ("local", "peer") or role in roles:
            raise FixtureError("pod roles must uniquely identify local and peer")
        identities.add(identity)
        fixed_ids.add(fixed_id)
        addresses.add(address)
        roles.add(role)

    rounds = fixture["rounds"]
    if not isinstance(rounds, list) or not rounds:
        raise FixtureError("rounds must be a non-empty list")
    tokens: set[int] = set()
    seen_targets: set[str] = set()
    seen_hits: set[bool] = set()
    for index, round_data in enumerate(rounds):
        if not isinstance(round_data, dict):
            raise FixtureError(f"round {index} must be an object")
        if round_data.get("index") != index:
            raise FixtureError(f"round {index} has an ambiguous index")
        token = _required(round_data, "round_token", f"round {index}")
        if not isinstance(token, int) or token <= 0:
            raise FixtureError(f"round {index} has a missing or zero token")
        if token in tokens:
            raise FixtureError(f"duplicate round token {token}")
        tokens.add(token)
        target = _required(round_data, "target_identity", f"round {index}")
        if target not in identities:
            raise FixtureError(f"round {index} targets unknown pod {target}")
        seen_targets.add(target)
        hit = _required(round_data, "hit", f"round {index}")
        if not isinstance(hit, bool):
            raise FixtureError(f"round {index} hit must be boolean")
        seen_hits.add(hit)
        reaction = _required(round_data, "reaction_time_us", f"round {index}")
        timeout = _required(round_data, "timeout_us", f"round {index}")
        if not isinstance(timeout, int) or timeout <= 0:
            raise FixtureError(f"round {index} timeout is missing or invalid")
        if hit:
            if not isinstance(reaction, int) or reaction <= 0 or reaction >= timeout:
                raise FixtureError(
                    f"round {index} hit timing is missing or outside its boundary"
                )
        elif reaction is not None:
            raise FixtureError(f"round {index} miss has ambiguous reaction timing")
    if seen_targets != identities or seen_hits != {False, True}:
        raise FixtureError(
            "fixture must cover hits and misses for both target identities"
        )


def load_result(
    path: Path, expected_path: str, fixture: dict[str, Any], digest: str
) -> dict[str, Any]:
    try:
        result = json.loads(path.read_bytes())
    except (OSError, json.JSONDecodeError) as error:
        raise ResultError(
            f"{expected_path} result is missing or malformed: {error}"
        ) from error
    if not isinstance(result, dict) or result.get("schema_version") != 1:
        raise ResultError(f"{expected_path} result schema is missing or unsupported")
    if result.get("path") != expected_path:
        raise ResultError(f"expected {expected_path} result provenance")
    if (
        result.get("fixture_id") != fixture["fixture_id"]
        or result.get("fixture_sha256") != digest
    ):
        raise ResultError(
            f"{expected_path} result does not identify the immutable fixture"
        )
    if result.get("clock_provenance") != fixture["paths"][expected_path]["clock"]:
        raise ResultError(f"{expected_path} clock provenance is missing or ambiguous")
    if result.get("result_provenance") != fixture["paths"][expected_path]["result"]:
        raise ResultError(f"{expected_path} result provenance is missing or ambiguous")
    rounds = result.get("rounds")
    aggregate = result.get("aggregate")
    if not isinstance(rounds, list) or not isinstance(aggregate, dict):
        raise ResultError(f"{expected_path} result is incomplete")
    if len(rounds) != len(fixture["rounds"]):
        raise ResultError(f"{expected_path} result round count is incomplete")
    required_aggregate = {
        "hits",
        "misses",
        "average_reaction_us",
        "best_reaction_us",
        "worst_reaction_us",
    }
    if not required_aggregate.issubset(aggregate):
        raise ResultError(f"{expected_path} aggregate result is incomplete")
    identities = {pod["identity"] for pod in fixture["pods"]}
    tokens: set[int] = set()
    for index, (actual, source) in enumerate(
        zip(rounds, fixture["rounds"], strict=True)
    ):
        if not isinstance(actual, dict) or actual.get("index") != index:
            raise ResultError(f"{expected_path} round {index} is missing or ambiguous")
        if not {"target_identity", "round_token", "hit", "reaction_time_us"}.issubset(
            actual
        ):
            raise ResultError(f"{expected_path} round {index} is incomplete")
        if actual.get("target_identity") not in identities:
            raise ResultError(f"{expected_path} round {index} has wrong pod identity")
        token = actual.get("round_token")
        if expected_path == "fixed" and (not isinstance(token, int) or token <= 0):
            raise ResultError(f"fixed round {index} token is missing or ambiguous")
        if expected_path == "fixed" and token in tokens:
            raise ResultError(f"fixed round {index} token is duplicated")
        if expected_path == "fixed":
            tokens.add(token)
        if expected_path == "mobile" and token is not None:
            raise ResultError(f"mobile round {index} claims an unavailable round token")
    return result


def compare_results(
    fixture: dict[str, Any], digest: str, fixed: dict[str, Any], mobile: dict[str, Any]
) -> tuple[dict[str, Any], dict[str, Any]]:
    matches: list[str] = []
    divergences: list[dict[str, Any]] = []
    for field in (
        "hits",
        "misses",
        "average_reaction_us",
        "best_reaction_us",
        "worst_reaction_us",
    ):
        left = fixed["aggregate"].get(field)
        right = mobile["aggregate"].get(field)
        if left == right:
            matches.append(f"aggregate.{field}")
        else:
            divergences.append(
                {"field": f"aggregate.{field}", "fixed": left, "mobile": right}
            )
    for index, (left, right) in enumerate(
        zip(fixed["rounds"], mobile["rounds"], strict=True)
    ):
        for field in ("round_token", "target_identity", "hit", "reaction_time_us"):
            left_value = left.get(field)
            right_value = right.get(field)
            if left_value == right_value:
                matches.append(f"rounds[{index}].{field}")
            else:
                divergence = {
                    "field": f"rounds[{index}].{field}",
                    "fixed": left_value,
                    "mobile": right_value,
                }
                if field == "round_token" and right_value is None:
                    divergence["reason"] = "unavailable_in_mobile_result"
                divergences.append(divergence)
    normalized = {
        "fixture_id": fixture["fixture_id"],
        "fixture_sha256": digest,
        "paths": {"fixed": fixed, "mobile": mobile},
        "schema_version": 1,
    }
    verdict = {
        "divergences": divergences,
        "fixture_id": fixture["fixture_id"],
        "fixture_sha256": digest,
        "matches": matches,
        "provenance_limitations": [
            "mobile production results do not retain round tokens",
            "mobile fixture durations exercise DrillResult scoring but not DateTime.now capture",
            "simulated monotonic and host wall clocks have different origins and are not physically equivalent",
            "BLE, ESP-NOW, touch, and wall-clock equivalence are unverified",
        ],
        "schema_version": 1,
        "status": "diverged" if divergences else "match_with_provenance_limitations",
    }
    return normalized, verdict


def run_campaign(
    fixture_path: Path,
    fixed_paths: list[Path],
    mobile_paths: list[Path],
    output_dir: Path,
) -> dict[str, Any]:
    if len(fixed_paths) != 2 or len(mobile_paths) != 2:
        raise ResultError("exactly two independent outputs per path are required")
    fixture, digest = load_fixture(fixture_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    run_hashes: list[str] = []
    verdict_hashes: list[str] = []
    verdict: dict[str, Any] | None = None
    for repetition, (fixed_path, mobile_path) in enumerate(
        zip(fixed_paths, mobile_paths, strict=True), 1
    ):
        fixed = load_result(fixed_path, "fixed", fixture, digest)
        mobile = load_result(mobile_path, "mobile", fixture, digest)
        normalized, current_verdict = compare_results(fixture, digest, fixed, mobile)
        output = canonical_bytes(normalized)
        verdict_output = canonical_bytes(current_verdict)
        (output_dir / f"normalized-run-{repetition}.json").write_bytes(output)
        run_hashes.append(sha256_bytes(output))
        verdict_hashes.append(sha256_bytes(verdict_output))
        verdict = current_verdict
    if run_hashes[0] != run_hashes[1] or verdict_hashes[0] != verdict_hashes[1]:
        raise ResultError("normalized output or verdict changed across repetitions")
    assert verdict is not None
    verdict["normalized_output_sha256"] = run_hashes
    verdict["comparison_verdict_sha256"] = verdict_hashes
    (output_dir / "verdict.json").write_bytes(canonical_bytes(verdict))
    return verdict


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--fixed-result", type=Path, action="append", required=True)
    parser.add_argument("--mobile-result", type=Path, action="append", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    verdict = run_campaign(
        args.fixture, args.fixed_result, args.mobile_result, args.output_dir
    )
    print(json.dumps(verdict, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
