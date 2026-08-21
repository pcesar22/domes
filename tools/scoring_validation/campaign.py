#!/usr/bin/env python3
"""Deterministic fixed-drill/mobile scoring differential campaign."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
from typing import Any


class FixtureError(ValueError):
    """The fixture cannot be compared without guessing."""


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
            clock.get(k) for k in ("kind", "origin", "unit")
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


def _score(fixture: dict[str, Any], path_name: str) -> dict[str, Any]:
    resolution = fixture["paths"][path_name]["result"]["reaction_resolution_us"]
    rounds = []
    hit_reactions = []
    for source in fixture["rounds"]:
        reaction = source["reaction_time_us"]
        normalized_reaction = (
            None if reaction is None else (reaction // resolution) * resolution
        )
        if source["hit"]:
            hit_reactions.append(normalized_reaction)
        rounds.append(
            {
                "hit": source["hit"],
                "index": source["index"],
                "reaction_time_us": normalized_reaction,
                "round_token": source["round_token"],
                "target_identity": source["target_identity"],
            }
        )
    hit_count = len(hit_reactions)
    aggregate = {
        "average_reaction_us": sum(hit_reactions) // hit_count if hit_count else None,
        "best_reaction_us": min(hit_reactions) if hit_count else None,
        "hits": hit_count,
        "misses": len(rounds) - hit_count,
        "worst_reaction_us": max(hit_reactions) if hit_count else None,
    }
    return {
        "aggregate": aggregate,
        "clock_provenance": fixture["paths"][path_name]["clock"],
        "path": path_name,
        "result_provenance": fixture["paths"][path_name]["result"],
        "rounds": rounds,
    }


def compare(
    fixture: dict[str, Any], fixture_digest: str
) -> tuple[dict[str, Any], dict[str, Any]]:
    fixed = _score(fixture, "fixed")
    mobile = _score(fixture, "mobile")
    matches: list[str] = []
    divergences: list[dict[str, Any]] = []
    for field in (
        "hits",
        "misses",
        "average_reaction_us",
        "best_reaction_us",
        "worst_reaction_us",
    ):
        left = fixed["aggregate"][field]
        right = mobile["aggregate"][field]
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
            if left[field] == right[field]:
                matches.append(f"rounds[{index}].{field}")
            else:
                divergences.append(
                    {
                        "field": f"rounds[{index}].{field}",
                        "fixed": left[field],
                        "mobile": right[field],
                    }
                )
    normalized = {
        "fixture_id": fixture["fixture_id"],
        "fixture_sha256": fixture_digest,
        "paths": {"fixed": fixed, "mobile": mobile},
        "schema_version": 1,
    }
    limitations = [
        "simulated monotonic and host wall clocks have different origins and are not physically equivalent",
        "mobile DrillResult does not retain round tokens; campaign tokens come from the validated orchestration fixture",
        "BLE, ESP-NOW, touch, and wall-clock equivalence are unverified",
    ]
    verdict = {
        "divergences": divergences,
        "fixture_id": fixture["fixture_id"],
        "fixture_sha256": fixture_digest,
        "matches": matches,
        "provenance_limitations": limitations,
        "schema_version": 1,
        "status": "diverged" if divergences else "match_with_provenance_limitations",
    }
    return normalized, verdict


def run_campaign(fixture_path: Path, output_dir: Path) -> dict[str, Any]:
    fixture, digest = load_fixture(fixture_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    run_hashes = []
    verdict: dict[str, Any] | None = None
    for repetition in (1, 2):
        normalized, current_verdict = compare(copy.deepcopy(fixture), digest)
        output = canonical_bytes(normalized)
        (output_dir / f"normalized-run-{repetition}.json").write_bytes(output)
        run_hashes.append(sha256_bytes(output))
        if verdict is None:
            verdict = current_verdict
        elif canonical_bytes(verdict) != canonical_bytes(current_verdict):
            raise RuntimeError("verdict changed across repetitions")
    if run_hashes[0] != run_hashes[1]:
        raise RuntimeError("normalized output changed across repetitions")
    assert verdict is not None
    verdict["normalized_output_sha256"] = run_hashes
    (output_dir / "verdict.json").write_bytes(canonical_bytes(verdict))
    return verdict


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    verdict = run_campaign(args.fixture, args.output_dir)
    print(json.dumps(verdict, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
