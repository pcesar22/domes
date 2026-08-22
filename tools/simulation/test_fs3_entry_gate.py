#!/usr/bin/env python3
"""Tests for the fail-closed FS-WP-002F entry gate."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
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


def physical_record(evidence_id: str, details: dict) -> dict:
    return {
        "evidence_id": evidence_id,
        "level": "physical",
        "status": "passed",
        "artifact": f"https://evidence.example/{evidence_id}.json",
        "details": details,
    }


def physical_issue(records: list[dict], commit: str = "2" * 40) -> dict:
    import json

    return {
        "comments": [
            {
                "author": {"login": "verifier"},
                "authorAssociation": "OWNER",
                "url": "https://example/comment",
                "body": f"```json\n{json.dumps({'commit': commit, 'verification': records})}\n```",
            }
        ]
    }


def valid_physical_records() -> list[dict]:
    boards = ["pod-a", "pod-b"]
    return [
        physical_record(
            "two_board_discovery",
            {"board_ids": boards, "peer_counts": {"pod-a": 1, "pod-b": 1}},
        ),
        physical_record(
            "complementary_roles",
            {
                "board_ids": boards,
                "roles": {"pod-a": "initiator", "pod-b": "responder"},
            },
        ),
        physical_record(
            "bidirectional_benchmark_simulation_off",
            {
                "board_ids": boards,
                "simulation_enabled": False,
                "directions": [
                    {"from": "pod-a", "to": "pod-b", "result": "passed"},
                    {"from": "pod-b", "to": "pod-a", "result": "passed"},
                ],
            },
        ),
        physical_record(
            "traced_drill",
            {
                "board_ids": boards,
                "simulation_enabled": True,
                "trace_enabled": True,
                "drill_result": "passed",
            },
        ),
    ]


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

    def test_physical_evidence_requires_explicit_complete_records(self) -> None:
        commit = "2" * 40
        evidence, errors = gate._physical_evidence(
            physical_issue(valid_physical_records()), commit
        )
        self.assertEqual(errors, [])
        self.assertEqual(set(evidence), set(gate.PHYSICAL_EVIDENCE_IDS))
        self.assertEqual(evidence["two_board_discovery"]["result"], "PASS")
        self.assertEqual(
            evidence["two_board_discovery"]["acceptance_authority"],
            "verifier (OWNER)",
        )

    def test_free_text_cannot_satisfy_multiple_physical_categories(self) -> None:
        record = {
            "level": "physical",
            "command_or_observation": (
                "two-board discovery, complementary roles, bidirectional benchmark "
                "with simulation off, and traced drill all passed"
            ),
            "status": "passed",
            "artifact": "https://evidence.example/all.json",
        }
        evidence, errors = gate._physical_evidence(physical_issue([record]), "2" * 40)
        self.assertEqual(evidence, {})
        self.assertEqual(errors, [])

    def test_unsupported_physical_claims_cannot_produce_overall_pass(self) -> None:
        old_pr = {
            "number": 107,
            "state": "CLOSED",
            "headRefOid": "1" * 40,
            "mergeCommit": None,
            "statusCheckRollup": [],
        }
        integrated_pr = {
            "number": 115,
            "state": "MERGED",
            "headRefOid": "2" * 40,
            "mergeCommit": {"oid": "3" * 40},
            "statusCheckRollup": [
                check(name) for name in gate.REQUIRED_SOFTWARE_CHECKS
            ],
        }
        placeholder_pr = {
            "number": 0,
            "state": "MERGED",
            "headRefOid": "4" * 40,
            "mergeCommit": {"oid": "5" * 40},
            "statusCheckRollup": [],
        }
        unsupported = {
            "number": 114,
            "state": "CLOSED",
            **physical_issue(
                [
                    {
                        "level": "physical",
                        "command_or_observation": (
                            "discovery complementary roles bidirectional benchmark "
                            "simulation traced drill"
                        ),
                        "status": "passed",
                        "artifact": "https://evidence.example/unsupported.json",
                    }
                ]
            ),
        }
        tracker = {
            ("pr", 105): {**placeholder_pr, "number": 105},
            ("pr", 107): old_pr,
            ("pr", 115): integrated_pr,
            ("pr", 130): {**placeholder_pr, "number": 130},
            ("issue", 114): unsupported,
        }
        passing = ({"result": "PASS"}, [])
        with (
            mock.patch.object(gate, "_git", return_value="6" * 40),
            mock.patch.object(
                gate,
                "_repository_report",
                return_value=({"head": "6" * 40}, []),
            ),
            mock.patch.object(
                gate,
                "_gh_json",
                side_effect=lambda kind, number: tracker[(kind, number)],
            ),
            mock.patch.object(gate, "_is_ancestor", return_value=True),
            mock.patch.object(gate, "_ledger_and_integration", return_value=passing),
            mock.patch.object(gate, "_seam_report", return_value=passing),
            mock.patch.object(gate, "_fidelity_schema_report", return_value=passing),
            mock.patch.object(gate, "_engine_report", return_value=passing),
            mock.patch.object(gate, "_patch_budget_report", return_value=passing),
        ):
            report = gate.build_report()
        self.assertEqual(report["verdict"], "BLOCKED")
        self.assertEqual(
            [item["result"] for item in report["fs_wp_003a"]["evidence_matrix"]],
            ["PASS", "MISSING", "MISSING", "MISSING", "MISSING"],
        )

    def test_adversarial_physical_records_fail_closed(self) -> None:
        cases = {}
        negative = valid_physical_records()[0]
        negative["status"] = "failed"
        cases["negative"] = [negative]
        simulation_on = valid_physical_records()[2]
        simulation_on["details"]["simulation_enabled"] = True
        cases["simulation_on_benchmark"] = [simulation_on]
        incomplete = valid_physical_records()[0]
        incomplete["details"]["board_ids"] = ["pod-a"]
        cases["incomplete"] = [incomplete]
        contradictory = valid_physical_records()[3]
        contradictory["details"]["trace_enabled"] = False
        cases["contradictory"] = [contradictory]
        duplicate = valid_physical_records()[0]
        cases["duplicate"] = [duplicate, duplicate]
        for name, records in cases.items():
            with self.subTest(name=name):
                evidence, errors = gate._physical_evidence(
                    physical_issue(records), "2" * 40
                )
                self.assertEqual(evidence, {})
                self.assertTrue(errors)

    def test_engine_requires_cached_archives(self) -> None:
        toolchain = SimpleNamespace(
            idf_version=gate.feasibility.EXPECTED_IDF_VERSION,
            idf_revision=gate.feasibility.EXPECTED_IDF_REVISION,
            compiler_version=gate.feasibility.EXPECTED_COMPILER_VERSION,
            compiler_sha256=gate.feasibility.EXPECTED_COMPILER_SHA256,
            compiler_archive=None,
            compiler_archive_sha256=None,
            qemu_version=gate.feasibility.EXPECTED_QEMU_VERSION,
            qemu_sha256=gate.feasibility.EXPECTED_QEMU_SHA256,
            qemu_archive=None,
            qemu_archive_sha256=None,
        )
        refs = (
            f"{gate.feasibility.EXPECTED_QEMU_TAG_OBJECT}\trefs/tags/"
            f"{gate.feasibility.EXPECTED_QEMU_RELEASE_TAG}\n"
            f"{gate.feasibility.EXPECTED_QEMU_SOURCE_REVISION}\trefs/tags/"
            f"{gate.feasibility.EXPECTED_QEMU_RELEASE_TAG}^{{}}"
        )
        with (
            mock.patch.object(
                gate.feasibility, "discover_toolchain", return_value=toolchain
            ),
            mock.patch.object(gate.feasibility, "_toolchain_identity", return_value={}),
            mock.patch.object(gate, "_run", return_value=refs),
        ):
            report, blockers = gate._engine_report(True)
        self.assertEqual(report["result"], "BLOCKED")
        self.assertFalse(report["identity_checks"]["compiler_archive_sha256"])
        self.assertFalse(report["identity_checks"]["qemu_archive_sha256"])
        self.assertTrue(blockers)

    def test_engine_requires_resolved_qemu_source_identity(self) -> None:
        archive = Path("/cached/archive")
        toolchain = SimpleNamespace(
            idf_version=gate.feasibility.EXPECTED_IDF_VERSION,
            idf_revision=gate.feasibility.EXPECTED_IDF_REVISION,
            compiler_version=gate.feasibility.EXPECTED_COMPILER_VERSION,
            compiler_sha256=gate.feasibility.EXPECTED_COMPILER_SHA256,
            compiler_archive=archive,
            compiler_archive_sha256=gate.feasibility.EXPECTED_COMPILER_ARCHIVE_SHA256,
            qemu_version=gate.feasibility.EXPECTED_QEMU_VERSION,
            qemu_sha256=gate.feasibility.EXPECTED_QEMU_SHA256,
            qemu_archive=archive,
            qemu_archive_sha256=gate.feasibility.EXPECTED_QEMU_ARCHIVE_SHA256,
        )
        with (
            mock.patch.object(
                gate.feasibility, "discover_toolchain", return_value=toolchain
            ),
            mock.patch.object(gate.feasibility, "_toolchain_identity", return_value={}),
            mock.patch.object(gate, "_run", return_value=""),
        ):
            report, blockers = gate._engine_report(True)
        self.assertEqual(report["result"], "BLOCKED")
        self.assertFalse(report["identity_checks"]["qemu_tag_object"])
        self.assertFalse(report["identity_checks"]["qemu_source_revision"])
        self.assertTrue(blockers)

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
