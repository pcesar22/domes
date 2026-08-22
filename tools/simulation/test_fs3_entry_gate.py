#!/usr/bin/env python3
"""Tests for the fail-closed FS-WP-002F entry gate."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).with_name("fs3_entry_gate.py")
SPEC = importlib.util.spec_from_file_location("fs3_entry_gate", MODULE_PATH)
assert SPEC and SPEC.loader
gate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = gate
SPEC.loader.exec_module(gate)


def check(
    name: str, *, workflow: str = "Software CI", conclusion: str = "SUCCESS"
) -> dict:
    return {
        "workflowName": workflow,
        "name": name,
        "status": "COMPLETED",
        "conclusion": conclusion,
        "detailsUrl": f"https://checks.example/{name}",
    }


class EntryGateTests(unittest.TestCase):
    def test_software_ci_requires_successful_gate(self) -> None:
        passed, checks = gate._software_ci(
            {
                "statusCheckRollup": [
                    check(name) for name in gate.REQUIRED_SOFTWARE_CHECKS
                ]
            }
        )
        self.assertTrue(passed)
        self.assertEqual(len(checks), len(gate.REQUIRED_SOFTWARE_CHECKS))
        failed, _ = gate._software_ci(
            {"statusCheckRollup": [check("CI Gate", conclusion="FAILURE")]}
        )
        self.assertFalse(failed)

    def test_patch_budget_is_empty_and_bounded(self) -> None:
        report, blockers = gate._patch_budget_report()
        self.assertEqual(blockers, [])
        self.assertEqual(report["anticipated_qemu_patch"]["paths"], [])
        self.assertEqual(report["adopted_limits"]["non_generated_files"], 10)
        self.assertEqual(report["adopted_limits"]["changed_lines"], 2500)

    def test_skipped_engine_validation_fails_closed(self) -> None:
        report, blockers = gate._engine_report(False)
        self.assertEqual(report["result"], "BLOCKED")
        self.assertTrue(blockers)

    def test_fs3_open_legacy_pr_and_missing_physical_evidence_block(self) -> None:
        prs = {
            107: {
                "number": 107,
                "title": "legacy",
                "state": "OPEN",
                "isDraft": False,
                "baseRefName": "main",
                "headRefOid": "1" * 40,
                "mergeCommit": None,
                "statusCheckRollup": [],
                "url": "https://example/pr/107",
                "mergedAt": None,
            },
            115: {
                "number": 115,
                "title": "integrated",
                "state": "MERGED",
                "isDraft": False,
                "baseRefName": "main",
                "headRefOid": "2" * 40,
                "mergeCommit": {"oid": "3" * 40},
                "statusCheckRollup": [
                    check(name) for name in gate.REQUIRED_SOFTWARE_CHECKS
                ],
                "url": "https://example/pr/115",
                "mergedAt": "2026-01-01T00:00:00Z",
            },
        }
        issue = {
            "number": 114,
            "state": "CLOSED",
            "url": "https://example/issues/114",
            "comments": [{"body": "Physical verification is pending."}],
        }
        with mock.patch.object(gate, "_is_ancestor", return_value=True):
            report, blockers = gate._fs3_report(prs, issue)
        self.assertEqual(report["result"], "BLOCKED")
        self.assertFalse(report["legacy_pr_107"]["disposition_resolved"])
        self.assertEqual(
            [item["result"] for item in report["evidence_matrix"]],
            ["PASS", "MISSING", "MISSING", "MISSING", "MISSING"],
        )
        self.assertEqual(len(blockers), 5)

    def test_physical_evidence_requires_exact_commit_and_artifact(self) -> None:
        commit = "2" * 40
        issue = {
            "comments": [
                {
                    "author": {"login": "verifier"},
                    "authorAssociation": "OWNER",
                    "url": "https://example/comment",
                    "body": """```json
{"commit":"2222222222222222222222222222222222222222","verification":[{"level":"physical","command_or_observation":"two-board discovery and complementary-role result","status":"passed","artifact":"artifact.json"}]}
```""",
                }
            ]
        }
        evidence = gate._physical_evidence(issue, commit)
        self.assertEqual(evidence["two_board_discovery"]["result"], "PASS")
        self.assertEqual(evidence["complementary_roles"]["artifact"], "artifact.json")
        self.assertEqual(
            evidence["two_board_discovery"]["acceptance_authority"],
            "verifier (OWNER)",
        )

    def test_tracker_failure_cannot_produce_pass(self) -> None:
        repository = ({"head": "a" * 40}, [])
        passing_section = ({"result": "PASS"}, [])
        with (
            mock.patch.object(gate, "_git", return_value="a" * 40),
            mock.patch.object(gate, "_repository_report", return_value=repository),
            mock.patch.object(gate, "_gh_json", side_effect=gate.GateError("offline")),
            mock.patch.object(gate, "_seam_report", return_value=passing_section),
            mock.patch.object(
                gate, "_fidelity_schema_report", return_value=passing_section
            ),
            mock.patch.object(gate, "_engine_report", return_value=passing_section),
            mock.patch.object(
                gate, "_patch_budget_report", return_value=passing_section
            ),
        ):
            report = gate.build_report()
        self.assertEqual(report["verdict"], "BLOCKED")
        self.assertFalse(report["tracker"]["access_resolved"])
        self.assertIn("live tracker access is unresolved", report["blockers"][0])


if __name__ == "__main__":
    unittest.main()
