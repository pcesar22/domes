#!/usr/bin/env python3
"""Tests for the fail-closed FS-WP-002F entry gate."""

from __future__ import annotations

import hashlib
import importlib.util
import json
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
REAL_ARTIFACT_DIGEST_MATCHES = gate._artifact_digest_matches


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


def physical_record(evidence_id: str, details: dict, commit: str = "2" * 40) -> dict:
    boards = details["board_ids"]
    return {
        "evidence_id": evidence_id,
        "level": "physical",
        "status": "passed",
        "configuration": {
            "firmware_commit": commit,
            "board_image_sha256": {
                boards[0]: "a" * 64,
                boards[1]: "b" * 64,
            },
            "lifecycle_initial_state": "disabled",
            "simulation_enabled": evidence_id == "traced_drill",
        },
        "procedure": f"controlled {evidence_id} verification",
        "details": details,
        "artifact": {
            "url": f"https://evidence.example/{evidence_id}.json",
            "sha256": hashlib.sha256(evidence_id.encode()).hexdigest(),
        },
    }


def physical_issue(
    records: list[dict], commit: str = "2" * 40, actor: str = gate.TRACKER_ACTOR
) -> dict:
    url = "https://github.com/pcesar22/domes/issues/114#issuecomment-1"
    payload = {
        "schema_version": 1,
        "kind": "fs-wp-003a-physical-acceptance",
        "issue": 114,
        "pull_request": 115,
        "spec_revision": gate.FS3_JUDGMENT["spec_revision"],
        "commit": commit,
        "acceptance_authority": {
            "actor": gate.TRACKER_ACTOR,
            "role": "controller",
            "decision": "accepted",
        },
        "verification": records,
    }
    return {
        "comments": [
            {
                "author": {"login": actor},
                "authorAssociation": "OWNER",
                "url": url,
                "body": f"```json\n{json.dumps(payload)}\n```",
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


def judgment(
    *,
    issue: int = 101,
    pull_request: int = 105,
    spec_revision: str = "224c22931311e763db3ba304d780daa78552db41",
    commit: str = "1" * 40,
    verdict: str = "approve",
) -> dict:
    return {
        "claim_boundary": "Exact-head independent review.",
        "commit": commit,
        "criteria": [
            {
                "criterion": "Required behavior",
                "evidence": ["Exact evidence"],
                "status": "met",
            }
        ],
        "issue": issue,
        "pull_request": pull_request,
        "required_rework": [],
        "spec_revision": spec_revision,
        "verdict": verdict,
    }


class EntryGateTests(unittest.TestCase):
    def setUp(self) -> None:
        artifact_patch = mock.patch.object(
            gate, "_artifact_digest_matches", return_value=True
        )
        artifact_patch.start()
        self.addCleanup(artifact_patch.stop)

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

    def test_judgment_schema_binds_every_acceptance_identity(self) -> None:
        valid = judgment()
        self.assertTrue(
            gate._valid_judgment(
                valid,
                issue=101,
                pull_request=105,
                spec_revision=valid["spec_revision"],
                commit=valid["commit"],
            )
        )
        mutations = {
            "issue": {**valid, "issue": 999},
            "pull_request": {**valid, "pull_request": 999},
            "spec_revision": {**valid, "spec_revision": "f" * 40},
            "commit": {**valid, "commit": "f" * 40},
            "criteria": {**valid, "criteria": []},
            "rework": {**valid, "required_rework": ["still required"]},
            "extra_field": {**valid, "schema_version": 1},
        }
        for name, candidate in mutations.items():
            with self.subTest(name=name):
                self.assertFalse(
                    gate._valid_judgment(
                        candidate,
                        issue=101,
                        pull_request=105,
                        spec_revision=valid["spec_revision"],
                        commit=valid["commit"],
                    )
                )

    def test_runtime_artifact_must_be_distinct_and_digest_bound(self) -> None:
        content = b'{"runtime":"evidence"}'
        artifact = {
            "url": "https://github.com/pcesar22/domes/runtime.json",
            "sha256": hashlib.sha256(content).hexdigest(),
        }
        response = mock.MagicMock()
        response.__enter__.return_value.geturl.return_value = artifact["url"]
        response.__enter__.return_value.read.return_value = content
        with mock.patch.object(gate.urllib.request, "urlopen", return_value=response):
            self.assertTrue(
                REAL_ARTIFACT_DIGEST_MATCHES(
                    artifact, "https://github.com/pcesar22/domes/issues/114#comment"
                )
            )
            self.assertFalse(REAL_ARTIFACT_DIGEST_MATCHES(artifact, artifact["url"]))
            self.assertFalse(
                REAL_ARTIFACT_DIGEST_MATCHES(
                    {**artifact, "sha256": "0" * 64}, "https://tracker.example/comment"
                )
            )

    def test_patch_budget_is_empty_and_bounded(self) -> None:
        def git_result(*args: str) -> str:
            if args[:2] == ("status", "--porcelain=v1"):
                return ""
            if args[:2] == ("diff", "--name-only"):
                return "tools/simulation/fs3_entry_gate.py"
            if args[:2] == ("diff", "--numstat"):
                return "10\t2\ttools/simulation/fs3_entry_gate.py"
            raise AssertionError(args)

        with mock.patch.object(gate, "_git", side_effect=git_result):
            report, blockers = gate._patch_budget_report()
        self.assertEqual(blockers, [])
        self.assertEqual(report["anticipated_qemu_patch"]["paths"], [])
        self.assertEqual(report["adopted_limits"]["non_generated_files"], 10)
        self.assertEqual(report["adopted_limits"]["changed_lines"], 2500)

    def test_seam_checks_reject_marker_only_mutations(self) -> None:
        paths = {
            "header": "firmware/domes/main/transport/espNowTransport.hpp",
            "source": "firmware/domes/main/transport/espNowTransport.cpp",
            "interface": "firmware/domes/main/transport/iEspNowRadio.hpp",
            "regression": "firmware/test_app/main/test_esp_now_transport.cpp",
            "cmake": "firmware/domes/main/CMakeLists.txt",
        }
        originals = {
            name: gate._git_file(gate.SPEC_REVISION, path)
            for name, path in paths.items()
        }
        path_to_name = {path: name for name, path in paths.items()}

        def run(files: dict[str, str]) -> dict:
            with mock.patch.object(
                gate,
                "_git_file",
                side_effect=lambda _revision, path: files[path_to_name[path]],
            ):
                return gate._seam_report()[0]["checks"]

        self.assertTrue(all(run(originals).values()))
        mutations = {
            "production_transport_owns_radio_seam": {
                "header": originals["header"].replace(
                    "IEspNowRadio& radio_;", "int radio_; // IEspNowRadio& radio_;"
                )
            },
            "seven_maximum_size_pending_frames": {
                "regression": originals["regression"].replace(
                    "static_assert(kEspNowRxMaxFrames == kEspNowRxBaselineMaxFrames);",
                    "static_assert(kEspNowRxMaxFrames >= kEspNowRxBaselineMaxFrames); "
                    "// static_assert(kEspNowRxMaxFrames == kEspNowRxBaselineMaxFrames);",
                )
            },
            "bounded_correlation_tokens": {
                "interface": originals["interface"].replace(
                    "using EspNowCorrelationToken = uint32_t;",
                    "using EspNowCorrelationToken = uint64_t; "
                    "// using EspNowCorrelationToken = uint32_t;",
                )
            },
            "physical_image_isolation": {
                "cmake": originals["cmake"].replace(
                    '        "transport/espNowTransport.cpp"\n',
                    '        # "transport/espNowTransport.cpp"\n',
                )
            },
        }
        for check_name, changed in mutations.items():
            with self.subTest(check=check_name):
                self.assertFalse(run({**originals, **changed})[check_name])

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
        self.assertEqual(len(blockers), 6)

    def test_pr107_disposition_is_explicit_and_identity_bound(self) -> None:
        old_head = "1" * 40
        replacement_head = "2" * 40
        payload = {
            "schema_version": 1,
            "kind": "fs-wp-003a-pr-disposition",
            "issue": 114,
            "spec_revision": gate.FS3_JUDGMENT["spec_revision"],
            "legacy_pull_request": 107,
            "legacy_head": old_head,
            "disposition": "superseded",
            "replacement_pull_request": 115,
            "replacement_head": replacement_head,
            "acceptance_authority": {
                "actor": gate.TRACKER_ACTOR,
                "role": "controller",
                "decision": "accepted",
            },
        }
        issue = {
            "comments": [
                {
                    "author": {"login": gate.TRACKER_ACTOR},
                    "url": "https://tracker.example/disposition",
                    "body": f"```json\n{json.dumps(payload)}\n```",
                }
            ]
        }
        self.assertIsNotNone(gate._pr107_disposition(issue, old_head, replacement_head))
        self.assertIsNone(gate._pr107_disposition(issue, old_head, "3" * 40))

    def test_non_integrated_ci_head_is_not_reported_as_pass(self) -> None:
        prs = {
            107: {"number": 107, "state": "CLOSED", "headRefOid": "1" * 40},
            115: {
                "number": 115,
                "state": "MERGED",
                "headRefOid": "2" * 40,
                "mergeCommit": {"oid": "3" * 40},
                "statusCheckRollup": [
                    check(name) for name in gate.REQUIRED_SOFTWARE_CHECKS
                ],
            },
        }
        with mock.patch.object(gate, "_is_ancestor", return_value=False):
            report, _ = gate._fs3_report(prs, {"comments": []})
        self.assertFalse(
            report["accepted_integration_candidate"]["integrated_in_pinned_revision"]
        )
        self.assertEqual(report["evidence_matrix"][0]["result"], "MISSING")

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
            {"actor": "pcesar22", "role": "controller", "decision": "accepted"},
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

    def test_physical_evidence_rejects_untrusted_actor_and_tampered_digest(
        self,
    ) -> None:
        untrusted, errors = gate._physical_evidence(
            physical_issue(valid_physical_records(), actor="collaborator"), "2" * 40
        )
        self.assertEqual(untrusted, {})
        self.assertEqual(errors, [])

        tampered = physical_issue(valid_physical_records())
        tampered["comments"][0]["body"] = tampered["comments"][0]["body"].replace(
            '"schema_version": 1', '"schema_version": 2', 1
        )
        evidence, errors = gate._physical_evidence(tampered, "2" * 40)
        self.assertEqual(evidence, {})
        self.assertEqual(len(errors), len(gate.PHYSICAL_EVIDENCE_IDS))

        unresolved = physical_issue(valid_physical_records())
        unresolved["comments"][0]["body"] = unresolved["comments"][0]["body"].replace(
            '"pull_request": 115', '"pull_request": 999', 1
        )
        evidence, errors = gate._physical_evidence(unresolved, "2" * 40)
        self.assertEqual(evidence, {})
        self.assertEqual(len(errors), len(gate.PHYSICAL_EVIDENCE_IDS))

    def test_patch_audit_blocks_dirty_or_protected_diff(self) -> None:
        outputs = {
            "status": "1 .M N... tools/simulation/fs3_entry_gate.py",
            "names": "firmware/domes/main/main.cpp",
            "numstat": "1\t0\tfirmware/domes/main/main.cpp",
        }

        def git_result(*args: str) -> str:
            if args[0] == "status":
                return outputs["status"]
            if args[1] == "--name-only":
                return outputs["names"]
            return outputs["numstat"]

        with mock.patch.object(gate, "_git", side_effect=git_result):
            report, blockers = gate._patch_budget_report()
        self.assertEqual(report["result"], "BLOCKED")
        self.assertEqual(
            report["reviewed_diff"]["protected_paths"], ["firmware/domes/main/main.cpp"]
        )
        self.assertTrue(blockers)

    def test_ledger_status_is_bound_to_exact_package_row(self) -> None:
        files = {
            "PROGRAM_STATUS.md": (
                "PR 105 passes review and merge\n"
                "| FS-WP-002C | package | Active / Amber |\n"
                "| FS-WP-002E | package | Complete / Green |\n"
                "| UNRELATED | package | `Not due` / `Not rated` |\n"
            ),
            "docs/plans/scheduler-trace-observability.md": (
                "Reviewed PR 105 candidate `abcdef0`"
            ),
            "docs/plans/esp-now-radio-seam.md": (
                "Issue #123 implements FS-WP-002E at specification revision\n"
                "`" + "a" * 40 + "`."
            ),
        }
        prs = {
            105: {
                "state": "MERGED",
                "headRefOid": "1" * 40,
                "mergeCommit": {"oid": "a" * 40},
            },
            130: {
                "state": "MERGED",
                "headRefOid": "2" * 40,
                "mergeCommit": {"oid": "b" * 40},
            },
        }
        issues = {101: {"comments": []}, 123: {"comments": []}}
        with (
            mock.patch.object(
                gate, "_git_file", side_effect=lambda _revision, path: files[path]
            ),
            mock.patch.object(gate, "_is_ancestor", return_value=True),
        ):
            report, blockers = gate._ledger_and_integration(prs, issues)
        self.assertTrue(report["program_status_pr105_pointer_stale"])
        self.assertFalse(report["program_status_fs_wp_002e_pointer_stale"])
        self.assertTrue(blockers)

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
            ("issue", 101): {"number": 101, "comments": []},
            ("issue", 114): unsupported,
            ("issue", 123): {"number": 123, "comments": []},
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
