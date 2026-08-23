#!/usr/bin/env python3
"""Focused fail-closed tests for terminal FS4 software acceptance."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).with_name("fs4_terminal_acceptance.py")
SPEC = importlib.util.spec_from_file_location("fs4_terminal_acceptance", MODULE_PATH)
assert SPEC and SPEC.loader
acceptance = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = acceptance
SPEC.loader.exec_module(acceptance)

TESTED_SHA = "a" * 40
SOURCE_SHAS = {issue: str(issue % 10) * 40 for issue in acceptance.REQUIRED_ISSUES}


def digest(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self._counter = 0

    def file(self, name: str, content: bytes) -> dict[str, str]:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return {"path": name, "sha256": digest(content)}

    def artifact(
        self, issue: int, tool_sha256: str, lockfile_sha256s: list[str]
    ) -> dict[str, str]:
        return self.file(
            f"issue-{issue}.json",
            acceptance.canonical(
                {
                    "tested_git_sha": SOURCE_SHAS[issue],
                    "tool_sha256": tool_sha256,
                    "lockfile_sha256s": lockfile_sha256s,
                    "result": "accepted",
                }
            ),
        )

    def manifest(self) -> tuple[Path, dict]:
        targets = [f"alpha-{number}" for number in range(1, 7)]
        prerequisites = []
        for issue in acceptance.REQUIRED_ISSUES:
            tool_sha256 = str(issue % 10) * 64
            lockfiles = [
                self.file(f"locks/{issue}.lock", f"lock {issue}\n".encode())
            ]
            prerequisites.append(
                {
                    "issue": issue,
                    "tracker_url": f"https://github.com/pcesar22/domes/issues/{issue}",
                    "source_git_sha": SOURCE_SHAS[issue],
                    "status": "accepted",
                    "tool": {
                        "path": f"tools/upstream/{issue}.py",
                        "version": "1",
                        "sha256": tool_sha256,
                    },
                    "lockfiles": lockfiles,
                    "artifacts": [
                        self.artifact(
                            issue,
                            tool_sha256,
                            [item["sha256"] for item in lockfiles],
                        )
                    ],
                }
            )
        raw_logs = [
            self.file("logs/flutter.log", b"flutter tests passed\n"),
            self.file("logs/diagnostics.log", b"six target diagnostics passed\n"),
            self.file("logs/qualification.log", b"mobile qualification passed\n"),
            self.file("logs/soak.log", b"1000 recovery cycles passed\n"),
        ]
        document = {
            "schema_version": 1,
            "specification_revision": acceptance.SPECIFICATION_REVISION,
            "tested_git_sha": TESTED_SHA,
            "targets": targets,
            "prerequisites": prerequisites,
            "coverage": [
                {
                    "area": area,
                    "tracker_issues": list(owners),
                    "implementation_paths": [f"implementation/{area}"],
                }
                for area, owners in acceptance.REQUIRED_COVERAGE.items()
            ],
            "duplication_audit": {
                "consumed": [154, 155, 174, 175, 176],
                "excluded": list(acceptance.EXCLUDED_ISSUES[1:]),
                "blocked_issue_116": "not_resumed_or_replaced",
            },
            "execution": {
                "tested_git_sha": TESTED_SHA,
                "tool_versions": {
                    "python": "3.12",
                    "flutter": "3.32",
                    "dart": "3.8",
                    "rust": "1.89",
                },
                "lockfiles": [self.file("locks/execution.lock", b"execution lock\n")],
                "raw_logs": raw_logs,
                "targets": targets,
                "stages": list(acceptance.REQUIRED_STAGES),
                "cycles": 1000,
                "per_target": {target: 1000 for target in targets},
                "per_stage": {stage: 1000 for stage in acceptance.REQUIRED_STAGES},
                "invariant_counters": {
                    counter: 0 for counter in acceptance.REQUIRED_COUNTERS
                },
                "terminal_states": {target: "disconnected" for target in targets},
                "software_result": "passed",
                "simulation_result": "passed",
            },
        }
        path = self.root / "input.json"
        path.write_bytes(acceptance.canonical(document))
        return path, document

    def rewrite(self, path: Path, document: dict) -> None:
        path.write_bytes(acceptance.canonical(document))


class AcceptanceTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.fixture = Fixture(Path(temporary.name))
        self.path, self.document = self.fixture.manifest()
        ancestor = mock.patch.object(
            acceptance.subprocess,
            "run",
            return_value=mock.Mock(returncode=0, stdout="", stderr=""),
        )
        ancestor.start()
        self.addCleanup(ancestor.stop)

    def build(self, document: dict | None = None) -> dict:
        if document is not None:
            self.fixture.rewrite(self.path, document)
        return acceptance.build_verdict(self.path, expected_git_sha=TESTED_SHA)

    def test_accepts_complete_digest_bound_evidence(self) -> None:
        verdict = self.build()
        self.assertEqual(verdict["result"], "passed")
        self.assertEqual(verdict["claim_boundaries"]["physical_validation"], "unverified")
        self.assertEqual(verdict["claim_boundaries"]["additional_alpha_nodes_unavailable"], 4)
        self.assertRegex(verdict["canonical_verdict_sha256"], r"^[0-9a-f]{64}$")

    def test_missing_prerequisite_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["prerequisites"].pop()
        with self.assertRaisesRegex(acceptance.AcceptanceError, "prerequisites"):
            self.build(candidate)

    def test_duplicate_and_reordered_prerequisites_stop(self) -> None:
        for mutate in (
            lambda records: records.__setitem__(1, copy.deepcopy(records[0])),
            lambda records: records.reverse(),
        ):
            with self.subTest(mutate=mutate):
                candidate = copy.deepcopy(self.document)
                mutate(candidate["prerequisites"])
                with self.assertRaisesRegex(acceptance.AcceptanceError, "prerequisites"):
                    self.build(candidate)

    def test_unaccepted_prerequisite_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["prerequisites"][0]["status"] = "pending"
        with self.assertRaisesRegex(acceptance.AcceptanceError, "unavailable or not accepted"):
            self.build(candidate)

    def test_foreign_revision_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["specification_revision"] = "f" * 40
        with self.assertRaisesRegex(acceptance.AcceptanceError, "foreign specification"):
            self.build(candidate)

    def test_foreign_tested_commit_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["tested_git_sha"] = "b" * 40
        with self.assertRaisesRegex(acceptance.AcceptanceError, "another tested Git"):
            self.build(candidate)

    def test_unintegrated_source_stops(self) -> None:
        acceptance.subprocess.run.return_value.returncode = 1
        with self.assertRaisesRegex(acceptance.AcceptanceError, "not integrated"):
            self.build()

    def test_foreign_tool_identity_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["prerequisites"][0]["tool"]["sha256"] = "f" * 64
        with self.assertRaisesRegex(acceptance.AcceptanceError, "another toolchain"):
            self.build(candidate)

    def test_foreign_lockfile_identity_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        item = candidate["prerequisites"][0]["artifacts"][0]
        foreign = self.fixture.file(
            item["path"],
            acceptance.canonical(
                {
                    "tested_git_sha": SOURCE_SHAS[154],
                    "tool_sha256": candidate["prerequisites"][0]["tool"]["sha256"],
                    "lockfile_sha256s": ["f" * 64],
                }
            ),
        )
        candidate["prerequisites"][0]["artifacts"][0] = foreign
        with self.assertRaisesRegex(acceptance.AcceptanceError, "foreign lockfile"):
            self.build(candidate)

    def test_hash_mismatch_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["raw_logs"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(acceptance.AcceptanceError, "hash mismatch"):
            self.build(candidate)

    def test_malformed_artifact_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        item = candidate["prerequisites"][0]["artifacts"][0]
        malformed = self.fixture.file(item["path"], b"not json\n")
        candidate["prerequisites"][0]["artifacts"][0] = malformed
        with self.assertRaisesRegex(acceptance.AcceptanceError, "malformed"):
            self.build(candidate)

    def test_foreign_artifact_source_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        item = candidate["prerequisites"][0]["artifacts"][0]
        foreign = self.fixture.file(
            item["path"],
            acceptance.canonical(
                {
                    "tested_git_sha": "f" * 40,
                    "tool_sha256": candidate["prerequisites"][0]["tool"]["sha256"],
                    "lockfile_sha256s": [
                        candidate["prerequisites"][0]["lockfiles"][0]["sha256"]
                    ],
                }
            ),
        )
        candidate["prerequisites"][0]["artifacts"][0] = foreign
        with self.assertRaisesRegex(acceptance.AcceptanceError, "another source"):
            self.build(candidate)

    def test_missing_or_duplicate_target_stops(self) -> None:
        for targets in (
            self.document["targets"][:-1],
            [*self.document["targets"][:-1], self.document["targets"][0]],
        ):
            with self.subTest(targets=targets):
                candidate = copy.deepcopy(self.document)
                candidate["targets"] = targets
                with self.assertRaisesRegex(acceptance.AcceptanceError, "six unique"):
                    self.build(candidate)

    def test_reordered_targets_stop(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["targets"] = list(reversed(candidate["targets"]))
        with self.assertRaisesRegex(acceptance.AcceptanceError, "target inventory changed"):
            self.build(candidate)

    def test_incomplete_lifecycle_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["stages"].pop()
        with self.assertRaisesRegex(acceptance.AcceptanceError, "coverage is incomplete"):
            self.build(candidate)

    def test_short_soak_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["cycles"] = 999
        with self.assertRaisesRegex(acceptance.AcceptanceError, "fewer than 1,000"):
            self.build(candidate)

    def test_every_invariant_stops(self) -> None:
        for counter in acceptance.REQUIRED_COUNTERS:
            with self.subTest(counter=counter):
                candidate = copy.deepcopy(self.document)
                candidate["execution"]["invariant_counters"][counter] = 1
                with self.assertRaisesRegex(acceptance.AcceptanceError, "invariants are nonzero"):
                    self.build(candidate)

    def test_unexplained_divergence_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["per_stage"][acceptance.REQUIRED_STAGES[0]] = 999
        with self.assertRaisesRegex(acceptance.AcceptanceError, "divergent execution"):
            self.build(candidate)

    def test_terminal_state_mismatch_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["execution"]["terminal_states"][candidate["targets"][0]] = "connected"
        with self.assertRaisesRegex(acceptance.AcceptanceError, "terminal states"):
            self.build(candidate)

    def test_coverage_owner_change_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["coverage"][0]["tracker_issues"] = [143]
        with self.assertRaisesRegex(acceptance.AcceptanceError, "foreign owner"):
            self.build(candidate)

    def test_duplication_audit_change_stops(self) -> None:
        candidate = copy.deepcopy(self.document)
        candidate["duplication_audit"]["blocked_issue_116"] = "resumed"
        with self.assertRaisesRegex(acceptance.AcceptanceError, "duplication audit"):
            self.build(candidate)

    def test_repeated_verdict_is_byte_identical(self) -> None:
        first = self.build()
        second = self.build()
        self.assertEqual(acceptance.canonical(first), acceptance.canonical(second))
        verdict_path = self.fixture.root / "verdict.json"
        verdict_path.write_bytes(acceptance.canonical(first))
        self.assertEqual(
            acceptance.verify_verdict(
                self.path, verdict_path, expected_git_sha=TESTED_SHA
            ),
            first,
        )

    def test_changed_verdict_is_non_reproducible(self) -> None:
        verdict = self.build()
        verdict["result"] = "failed"
        verdict_path = self.fixture.root / "verdict.json"
        verdict_path.write_bytes(acceptance.canonical(verdict))
        with self.assertRaisesRegex(acceptance.AcceptanceError, "non-reproducible"):
            acceptance.verify_verdict(
                self.path, verdict_path, expected_git_sha=TESTED_SHA
            )


if __name__ == "__main__":
    unittest.main()
