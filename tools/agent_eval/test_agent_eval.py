import json
import tempfile
import unittest
from pathlib import Path

from tools.agent_eval.agent_eval import (
    EvaluationCase,
    load_cases,
    render_comparison,
    render_report,
    score_response,
    summarize,
)


class AgentEvaluationTest(unittest.TestCase):
    def test_checked_in_cases_are_valid_and_representative(self) -> None:
        cases = load_cases()
        categories = {case.category for case in cases}

        self.assertGreaterEqual(len(cases), 10)
        self.assertTrue({"protocol", "firmware", "flutter", "hardware"} <= categories)
        self.assertTrue(all(case.cleanup for case in cases))

    def test_required_paths_exist(self) -> None:
        root = Path(__file__).resolve().parents[2]
        missing = [
            relative
            for case in load_cases()
            for relative in case.required_files
            if not (root / relative).exists()
        ]

        self.assertEqual([], missing)

    def test_duplicate_ids_are_rejected(self) -> None:
        case = {
            "id": "duplicate",
            "title": "Duplicate",
            "category": "test",
            "prompt": "Inspect.",
            "required_files": [],
            "required_terms": [],
            "forbidden_terms": [],
            "sandbox": "read-only",
            "hardware": "not-required",
            "cleanup": "Remove worktree.",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cases.json"
            path.write_text(
                json.dumps({"schema_version": 1, "cases": [case, case]}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate case id"):
                load_cases(path)

    def test_score_includes_hardware_and_forbidden_claims(self) -> None:
        case = EvaluationCase(
            identifier="score",
            title="Score",
            category="test",
            prompt="Inspect.",
            required_files=("AGENTS.md",),
            required_terms=("hardware",),
            forbidden_terms=("hardware passed",),
            sandbox="read-only",
            hardware="unavailable-gate",
            cleanup="Remove worktree.",
        )
        response = {
            "summary": "Hardware remains unavailable.",
            "files": ["AGENTS.md"],
            "invariants": [],
            "verification": [],
            "hardware_status": "unavailable",
            "claims": [],
        }

        score = score_response(case, response)

        self.assertTrue(score["passed"])
        self.assertEqual((4, 4), (score["score"], score["possible"]))

    def test_summary_preserves_pending_cases_for_resume(self) -> None:
        result = {
            "id": "one",
            "status": "completed",
            "passed": True,
            "score": 3,
            "possible": 3,
        }

        summary = summarize([result], planned=3)

        self.assertEqual(2, summary["pending"])
        self.assertEqual(1, summary["passed"])

    def test_report_summarizes_results(self) -> None:
        document = {
            "run_id": "baseline",
            "revision": "abc123",
            "model": "gpt-5.6-sol",
            "reasoning_effort": "medium",
            "summary": {
                "total": 1,
                "recorded": 1,
                "pending": 0,
                "completed": 1,
                "passed": 1,
                "errors": 0,
                "skipped": 0,
                "score": 4,
                "possible": 4,
            },
            "results": [
                {
                    "id": "case-one",
                    "status": "completed",
                    "passed": True,
                    "score": 4,
                    "possible": 4,
                    "duration_seconds": 2.5,
                    "response": {"hardware_status": "not_required"},
                }
            ],
        }

        report = render_report(document)

        self.assertIn("# Agent Evaluation: baseline", report)
        self.assertIn("| case-one | pass | 4/4 | 2.5s | not_required |", report)

    def test_comparison_requires_matching_case_definitions(self) -> None:
        baseline = {
            "run_id": "baseline",
            "revision": "a" * 40,
            "model": "gpt-5.6-sol",
            "reasoning_effort": "medium",
            "case_definition_sha256": "same",
            "summary": {"passed": 1, "completed": 2, "score": 8, "possible": 10},
            "results": [{"duration_seconds": 2, "usage": {"input_tokens": 10}}],
        }
        optimized = {
            **baseline,
            "run_id": "optimized",
            "summary": {**baseline["summary"], "passed": 2},
        }

        report = render_comparison([baseline, optimized])

        self.assertIn("baseline", report)
        self.assertIn("optimized", report)
        self.assertIn("1/2", report)

        optimized["case_definition_sha256"] = "different"
        with self.assertRaisesRegex(ValueError, "different case definitions"):
            render_comparison([baseline, optimized])


if __name__ == "__main__":
    unittest.main()
