#!/usr/bin/env python3
"""Fail-closed tests for the FS4 command-recovery soak runner."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).with_name("fs4_command_recovery_soak.py")
SPEC = importlib.util.spec_from_file_location("fs4_command_recovery_soak", MODULE_PATH)
assert SPEC and SPEC.loader
soak = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = soak
SPEC.loader.exec_module(soak)


def summary(cycles: int = 1000) -> dict:
    return {
        "schema_version": 1,
        "scenario": "fs4_command_recovery_soak",
        "identities": list(soak.IDENTITIES),
        "stages": list(soak.STAGES),
        "cycles": cycles,
        "faults": cycles,
        "reconnects": cycles,
        "completed_results": cycles,
        "per_identity": soak._expected_totals(soak.IDENTITIES, cycles),
        "per_stage": soak._expected_totals(soak.STAGES, cycles),
        "terminal_state": "disconnected",
        "invariant_counters": {field: 0 for field in soak.COUNTER_FIELDS},
    }


def log_bytes(value: dict | None = None) -> bytes:
    value = summary() if value is None else value
    return f"test output\n{soak.SUMMARY_PREFIX}{json.dumps(value)}\nAll tests passed\n".encode()


def manifest(log: bytes | None = None) -> dict:
    log = log_bytes() if log is None else log
    value = summary()
    result = {
        "schema_version": 1,
        "campaign": "fs4_command_recovery_soak",
        "specification_revision": soak.SPECIFICATION_REVISION,
        "tested_git_sha": "a" * 40,
        "predecessor_git_sha": soak.PREDECESSOR_REVISION,
        "tool_versions": soak._tool_versions(),
        "flutter_lockfile": {
            "path": str(soak.FLUTTER_LOCKFILE.relative_to(soak.REPO_ROOT)),
            "sha256": hashlib.sha256(soak.FLUTTER_LOCKFILE.read_bytes()).hexdigest(),
        },
        "invocation": "python3 tools/simulation/fs4_command_recovery_soak.py --cycles 1000",
        "scenario": {
            "path": str(soak.SCENARIO.relative_to(soak.REPO_ROOT)),
            "sha256": hashlib.sha256(soak.SCENARIO.read_bytes()).hexdigest(),
            "inventory": list(soak.STAGES),
        },
        "counts": {
            key: value[key]
            for key in (
                "cycles",
                "faults",
                "reconnects",
                "completed_results",
                "per_identity",
                "per_stage",
            )
        },
        "invariant_counters": value["invariant_counters"],
        "terminal_state": "disconnected",
        "artifact_hashes": {"raw_flutter_log_sha256": hashlib.sha256(log).hexdigest()},
        "physical_validation": "unverified",
        "claim_boundary": "deterministic software regression evidence only",
        "ownership_and_gaps": {
            "owned": "command recovery",
            "excluded": ["normal qualification", "diagnostics", "bundling"],
            "unverified": "physical validation unverified",
        },
        "predecessor_reconciliation": [
            {
                "issue": issue,
                "state": "OPEN",
                "title": f"issue {issue}",
                "url": f"https://example.test/{issue}",
                "artifacts_consumed": False,
            }
            for issue in (174, 175, 176)
        ],
    }
    verdict = {
        key: result[key]
        for key in (
            "campaign",
            "specification_revision",
            "tested_git_sha",
            "predecessor_git_sha",
            "tool_versions",
            "flutter_lockfile",
            "scenario",
            "counts",
            "invariant_counters",
            "terminal_state",
            "physical_validation",
        )
    }
    result["canonical_verdict_digest"] = soak._sha256(soak._canonical(verdict))
    return result


class SummaryTests(unittest.TestCase):
    def test_accepts_complete_summary(self) -> None:
        self.assertEqual(soak.parse_summary(log_bytes().decode(), 1000)["cycles"], 1000)

    def test_malformed_summary_fails_closed(self) -> None:
        with self.assertRaisesRegex(soak.SoakError, "malformed"):
            soak.parse_summary(f"{soak.SUMMARY_PREFIX}{{", 1000)

    def test_incomplete_summary_fails_closed(self) -> None:
        candidate = summary()
        del candidate["terminal_state"]
        with self.assertRaisesRegex(soak.SoakError, "missing or unexpected"):
            soak.parse_summary(log_bytes(candidate).decode(), 1000)

    def test_duplicate_identity_fails_closed(self) -> None:
        candidate = summary()
        candidate["identities"][-1] = candidate["identities"][0]
        with self.assertRaisesRegex(soak.SoakError, "identities"):
            soak.parse_summary(log_bytes(candidate).decode(), 1000)

    def test_counter_mismatch_fails_closed(self) -> None:
        candidate = summary()
        candidate["per_stage"][soak.STAGES[0]] += 1
        with self.assertRaisesRegex(soak.SoakError, "per-stage"):
            soak.parse_summary(log_bytes(candidate).decode(), 1000)

    def test_nonzero_invariant_fails_closed(self) -> None:
        candidate = summary()
        candidate["invariant_counters"]["stale_mutations"] = 1
        with self.assertRaisesRegex(soak.SoakError, "nonzero"):
            soak.parse_summary(log_bytes(candidate).decode(), 1000)


class ManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        toolchain = mock.patch.object(
            soak,
            "_tool_version_tuple",
            return_value=("3.44.9", "3.12.2", sys.version.split()[0]),
        )
        toolchain.start()
        self.addCleanup(toolchain.stop)

    def test_accepts_complete_manifest_and_recomputes_log_hash(self) -> None:
        log = log_bytes()
        soak.validate_manifest(manifest(log), log)

    def test_revision_mismatch_fails_closed(self) -> None:
        candidate = manifest()
        candidate["specification_revision"] = "f" * 40
        with self.assertRaisesRegex(soak.SoakError, "revision"):
            soak.validate_manifest(candidate, log_bytes())

    def test_nondeterministic_digest_fails_closed(self) -> None:
        candidate = manifest()
        candidate["canonical_verdict_digest"] = "0" * 64
        with self.assertRaisesRegex(soak.SoakError, "verdict digest"):
            soak.validate_manifest(candidate, log_bytes())

    def test_toolchain_mismatch_fails_closed(self) -> None:
        candidate = manifest()
        candidate["tool_versions"]["flutter"] = "different"
        with self.assertRaisesRegex(soak.SoakError, "toolchain"):
            soak.validate_manifest(candidate, log_bytes())

    def test_lockfile_mismatch_fails_closed(self) -> None:
        candidate = manifest()
        candidate["flutter_lockfile"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(soak.SoakError, "lockfile"):
            soak.validate_manifest(candidate, log_bytes())

    def test_physical_claim_fails_closed(self) -> None:
        candidate = manifest()
        candidate["physical_validation"] = "passed"
        with self.assertRaisesRegex(soak.SoakError, "physical"):
            soak.validate_manifest(candidate, log_bytes())

    def test_predictive_field_fails_closed(self) -> None:
        candidate = manifest()
        candidate["ownership_and_gaps"]["owned"] = "predictive verdict passed"
        with self.assertRaisesRegex(soak.SoakError, "physical or predictive"):
            soak.validate_manifest(candidate, log_bytes())

    def test_claim_value_fails_closed(self) -> None:
        for claim in (
            "physical validation passed",
            "physical validation passed but unverified",
            "predictive verdict passed",
        ):
            with self.subTest(claim=claim):
                candidate = manifest()
                candidate["claim_boundary"] = claim
                with self.assertRaisesRegex(soak.SoakError, "physical or predictive"):
                    soak.validate_manifest(candidate, log_bytes())

    def test_negative_predecessor_title_is_not_a_claim(self) -> None:
        candidate = manifest()
        candidate["predecessor_reconciliation"][2][
            "title"
        ] = "bundle evidence without issuing a physical or predictive trust verdict"
        soak.validate_manifest(candidate, log_bytes())

    def test_empty_structured_fields_fail_closed(self) -> None:
        for field, replacement in (
            ("tool_versions", {}),
            ("invocation", ""),
            ("claim_boundary", ""),
            ("ownership_and_gaps", {}),
            ("predecessor_reconciliation", []),
        ):
            with self.subTest(field=field):
                candidate = manifest()
                candidate[field] = replacement
                with self.assertRaises(soak.SoakError):
                    soak.validate_manifest(candidate, log_bytes())

    def test_unexpected_manifest_field_fails_closed(self) -> None:
        candidate = manifest()
        candidate["duration_seconds"] = 1.0
        with self.assertRaisesRegex(soak.SoakError, "unexpected"):
            soak.validate_manifest(candidate, log_bytes())

    def test_retained_hash_mismatch_fails_closed(self) -> None:
        candidate = manifest()
        candidate["artifact_hashes"]["raw_flutter_log_sha256"] = "0" * 64
        with self.assertRaisesRegex(soak.SoakError, "artifact hash"):
            soak.validate_manifest(candidate, log_bytes())

    def test_count_change_invalidates_deterministic_digest(self) -> None:
        candidate = copy.deepcopy(manifest())
        candidate["counts"]["cycles"] = 1001
        with self.assertRaises(soak.SoakError):
            soak.validate_manifest(candidate, log_bytes())


if __name__ == "__main__":
    unittest.main()
