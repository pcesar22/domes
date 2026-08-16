import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

import control


def ticket_body(revision: str, dependencies: str = "None") -> str:
    values = {
        "Specification revision": revision,
        "Parent objective": "Deliver the parent outcome.",
        "Goal": "Implement one bounded change.",
        "Non-goals": "Do not merge or release.",
        "Required behavior": "The observable behavior is deterministic.",
        "Acceptance checks": "Run the focused automated tests.",
        "Allowed architectural surfaces": "tools/agent_control/",
        "Dependencies": dependencies,
        "Required proof": "Command, exit status, and retained result.",
    }
    return "\n\n".join(f"### {name}\n\n{value}" for name, value in values.items())


def make_ticket(
    number: int,
    revision: str,
    *,
    label: str = "agent:ready",
    dependencies: str = "None",
    state: str = "OPEN",
    extra_labels: tuple[str, ...] = (),
) -> control.Ticket:
    return control.Ticket(
        number=number,
        title=f"Issue {number}",
        body=ticket_body(revision, dependencies),
        state=state,
        labels=(label, *extra_labels),
        url=f"https://example.invalid/issues/{number}",
    )


def automated_ticket(
    number: int,
    revision: str,
    *,
    label: str = "agent:ci-pending",
    surfaces: tuple[str, ...] = ("firmware/domes/main/",),
    proof: tuple[str, ...] = ("Focused software verification output.",),
) -> control.Ticket:
    contract = control.render_ticket_contract(
        spec_revision=revision,
        parent_objective="Deliver the parent outcome.",
        goal="Implement one bounded change.",
        non_goals=["Do not merge or release."],
        required_behavior="The observable behavior is deterministic.",
        acceptance_checks=["Run the focused automated tests."],
        allowed_surface_values=list(surfaces),
        dependencies=(),
        required_proof=list(proof),
        work_package="FS-WP-TEST",
        work_class="software",
        selected_policy="software-review-required",
        pull_request=77,
    )
    body = control.with_autopilot_contract("", contract)
    return control.Ticket(
        number=number,
        title=f"Autopilot issue {number}",
        body=body,
        state="OPEN",
        labels=(label,),
        url=f"https://example.invalid/issues/{number}",
    )


def pull_request(
    policy: control.AutopilotPolicy,
    *,
    state: str = "OPEN",
    head: str = "c" * 40,
    files: tuple[str, ...] = ("firmware/domes/main/app_main.cpp",),
    checks: tuple[dict[str, str], ...] | None = None,
) -> control.PullRequest:
    if checks is None:
        checks = tuple(
            {"name": name, "state": "SUCCESS", "url": ""}
            for name in policy.required_ci_checks
        )
    return control.PullRequest(
        number=77,
        state=state,
        is_draft=False,
        base_ref="main",
        base_oid="a" * 40,
        head_ref="codex/feat/test",
        head_oid=head,
        mergeable="MERGEABLE",
        merge_state="CLEAN",
        review_decision="",
        files=files,
        checks=checks,
        merge_commit="d" * 40 if state == "MERGED" else "",
    )


class WorkflowTest(unittest.TestCase):
    def test_checked_in_workflow_loads(self) -> None:
        workflow = control.load_workflow()
        self.assertEqual("pcesar22/domes", workflow.repository)
        self.assertEqual("ministrom", workflow.scheduler_host)
        self.assertEqual(3, workflow.max_concurrent_workers)
        self.assertEqual("main", workflow.base_branch)

    def test_repository_contracts_validate(self) -> None:
        self.assertEqual([], control.validate_repository())

    def test_checked_in_autopilot_policy_loads(self) -> None:
        policy = control.load_autopilot_policy()
        self.assertEqual("domes-human-review-autopilot-v2", policy.policy_name)
        self.assertEqual("human", policy.review_authority)
        self.assertEqual(
            ("software", "executed-validation"), policy.allowed_work_classes
        )
        self.assertEqual(3, policy.max_ci_repair_cycles)

    def test_output_schema_requires_explicit_property_types(self) -> None:
        document = {
            "type": "object",
            "additionalProperties": False,
            "required": ["state"],
            "properties": {"state": {"enum": ["ready"]}},
        }
        self.assertEqual(
            ["$.properties.state must declare a type"],
            control.output_schema_contract_errors(document),
        )

    def test_front_matter_rejects_unsupported_tracker(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "WORKFLOW.md"
            text = control.WORKFLOW_PATH.read_text(encoding="utf-8").replace(
                "tracker_kind: github", "tracker_kind: conversation"
            )
            path.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(control.ControlError, "unsupported"):
                control.load_workflow(path)


class TicketValidationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.revision = "a" * 40

    def test_complete_ticket_is_valid(self) -> None:
        validation = control.validate_ticket(
            make_ticket(10, self.revision), check_revision=False
        )
        self.assertTrue(validation.valid)
        self.assertEqual((), validation.dependencies)

    def test_ticket_requires_one_state_and_full_revision(self) -> None:
        ticket = make_ticket(10, "abc123", extra_labels=("agent:plan",))
        validation = control.validate_ticket(ticket, check_revision=False)
        self.assertIn(
            "expected exactly one agent: state label; found 2", validation.errors
        )
        self.assertIn(
            "Specification revision must be one full lowercase 40-character commit SHA",
            validation.errors,
        )

    def test_dependency_parser_rejects_free_form_text(self) -> None:
        ticket = make_ticket(10, self.revision, dependencies="Do the other task first")
        validation = control.validate_ticket(ticket, check_revision=False)
        self.assertIn(
            "Dependencies must be `None` or contain GitHub issue references such as #123",
            validation.errors,
        )

    def test_allowed_surfaces_reject_prose_and_parent_paths(self) -> None:
        with self.assertRaises(control.ControlError):
            control.allowed_surfaces("firmware and CLI")
        with self.assertRaises(control.ControlError):
            control.allowed_surfaces("../outside")
        self.assertEqual(
            (".codex/orchestration/**", "tools/agent_control"),
            control.allowed_surfaces(
                "- `tools/agent_control/`\n- `.codex/orchestration/**`"
            ),
        )

    def test_queue_orders_priority_then_issue_and_blocks_dependencies(self) -> None:
        done = make_ticket(1, self.revision, label="agent:done", state="CLOSED")
        blocked_dependency = make_ticket(2, self.revision, label="agent:ready")
        low = make_ticket(
            20, self.revision, dependencies="#1", extra_labels=("priority:p2",)
        )
        high = make_ticket(
            30, self.revision, dependencies="#1", extra_labels=("priority:p0",)
        )
        blocked = make_ticket(40, self.revision, dependencies="#2")
        eligible, blockers = control.eligible_queue(
            [low, blocked, high, done, blocked_dependency], check_revision=False
        )
        self.assertEqual([30, 20, 2], [item.ticket.number for item in eligible])
        self.assertEqual(["dependency #2 is not terminal"], blockers[40])

    def test_dependency_cycle_blocks_every_member(self) -> None:
        first = make_ticket(1, self.revision, dependencies="#2")
        second = make_ticket(2, self.revision, dependencies="#1")
        eligible, blockers = control.eligible_queue(
            [first, second], check_revision=False
        )
        self.assertEqual([], eligible)
        self.assertIn("dependency cycle", blockers[1])
        self.assertIn("dependency cycle", blockers[2])

    def test_parallel_selection_skips_overlapping_surfaces(self) -> None:
        first = make_ticket(1, self.revision)
        overlapping = make_ticket(2, self.revision)
        separate = make_ticket(3, self.revision)
        separate_body = separate.body.replace(
            "tools/agent_control/", "docs/agent-system/"
        )
        separate = control.Ticket(
            separate.number,
            separate.title,
            separate_body,
            separate.state,
            separate.labels,
            separate.url,
        )
        eligible, blockers = control.eligible_queue(
            [first, overlapping, separate], check_revision=False
        )
        self.assertEqual({}, blockers)
        selected = control.select_non_overlapping(eligible, 3)
        self.assertEqual([1, 3], [item.ticket.number for item in selected])

    def test_parallel_selection_respects_running_worker_surfaces(self) -> None:
        first = make_ticket(1, self.revision)
        second = make_ticket(2, self.revision)
        eligible, blockers = control.eligible_queue(
            [first, second], check_revision=False
        )
        self.assertEqual({}, blockers)
        selected = control.select_non_overlapping(
            eligible, 1, (("tools/agent_control",),)
        )
        self.assertEqual([], selected)

    def test_rendered_queue_contains_no_ticket_body(self) -> None:
        ticket = make_ticket(7, self.revision)
        eligible, blockers = control.eligible_queue([ticket], check_revision=False)
        rendered = json.dumps(control.render_queue(eligible, blockers))
        self.assertNotIn("Deliver the parent outcome", rendered)
        self.assertIn('"role": "worker"', rendered)

    def test_autopilot_contract_marker_detects_tampering(self) -> None:
        ticket = automated_ticket(11, self.revision)
        sections = control.parse_sections(ticket.body)
        self.assertTrue(control.has_valid_autopilot_marker(ticket, sections))
        self.assertTrue(
            control.validate_ticket(ticket, check_revision=False).valid,
        )
        tampered = control.Ticket(
            ticket.number,
            ticket.title,
            ticket.body.replace("bounded change", "different change"),
            ticket.state,
            ticket.labels,
            ticket.url,
        )
        validation = control.validate_ticket(tampered, check_revision=False)
        self.assertIn(
            "software-review-required requires a valid controller contract marker",
            validation.errors,
        )

    def test_forbidden_paths_and_surface_narrowing(self) -> None:
        policy = control.load_autopilot_policy()
        self.assertEqual(
            [".github/workflows/ci.yml", "tools/agent_control/control.py"],
            control.protected_autonomous_paths(
                (
                    "firmware/domes/main/app_main.cpp",
                    ".github/workflows/ci.yml",
                    "tools/agent_control/control.py",
                ),
                policy,
            ),
        )
        self.assertEqual(
            [".github/**", "firmware/domes/**"],
            control.protected_autonomous_surfaces(
                (".github/**", "firmware/domes/**"), policy
            ),
        )
        self.assertTrue(
            control.surfaces_within(("firmware/domes/main/**",), ("firmware/domes/**",))
        )
        self.assertFalse(
            control.surfaces_within(("tools/agent_control/**",), ("firmware/domes/**",))
        )

    def test_autopilot_idle_ignores_human_and_blocked_but_not_ci(self) -> None:
        human = make_ticket(20, self.revision, label="agent:human-review")
        blocked = make_ticket(21, self.revision, label="agent:blocked")
        self.assertTrue(control.autopilot_queue_idle((human, blocked)))
        ci_pending = automated_ticket(22, self.revision)
        self.assertFalse(control.autopilot_queue_idle((human, ci_pending)))

    def test_dependency_blocked_ready_work_does_not_block_selector(self) -> None:
        dependency = make_ticket(23, self.revision, label="agent:blocked")
        waiting = make_ticket(24, self.revision, dependencies="#23")
        self.assertTrue(control.autopilot_queue_idle((dependency, waiting)))
        runnable = control.validate_ticket(
            make_ticket(25, self.revision), check_revision=False
        )
        self.assertFalse(control.autopilot_queue_idle((waiting,), (runnable,)))


class CommandLineTest(unittest.TestCase):
    def test_labels_are_complete_and_state_prefixed(self) -> None:
        expected_states = {
            "agent:needs-specification",
            "agent:plan",
            "agent:plan-review",
            "agent:ready",
            "agent:running",
            "agent:rework",
            "agent:agent-review",
            "agent:ci-pending",
            "agent:verification",
            "agent:human-review",
            "agent:blocked",
            "agent:done",
        }
        self.assertEqual(
            expected_states,
            {name for name in control.MANAGED_LABELS if name.startswith("agent:")},
        )

    def test_run_requires_explicit_execute(self) -> None:
        self.assertEqual(2, control.main(["run"]))

    def test_autopilot_requires_watch(self) -> None:
        self.assertEqual(2, control.main(["run", "--execute", "--autopilot"]))

    def test_dashboard_requires_watch(self) -> None:
        self.assertEqual(2, control.main(["run", "--execute", "--dashboard"]))

    def test_only_new_work_is_claimed_as_running(self) -> None:
        revision = "b" * 40
        workflow = control.load_workflow()
        ready = control.validate_ticket(make_ticket(1, revision), check_revision=False)
        rework = control.validate_ticket(
            make_ticket(2, revision, label="agent:rework"), check_revision=False
        )
        with mock.patch.object(control, "transition") as transition:
            claimed = control.claim_for_dispatch(workflow, ready)
            retained = control.claim_for_dispatch(workflow, rework)
        transition.assert_called_once_with(workflow, ready.ticket, "agent:running")
        self.assertEqual("agent:running", claimed.ticket.agent_state)
        self.assertEqual("agent:ready", claimed.source_state)
        self.assertIs(retained, rework)

    def test_mutating_run_is_pinned_to_reviewed_host(self) -> None:
        workflow = control.load_workflow()
        with mock.patch.object(
            control.socket, "gethostname", return_value="other-host"
        ):
            with self.assertRaisesRegex(control.ControlError, "pinned to ministrom"):
                control.enforce_scheduler_host(workflow)


class ResultSemanticsTest(unittest.TestCase):
    def test_judge_cannot_approve_unmet_criteria(self) -> None:
        result = {
            "verdict": "approve",
            "criteria": [{"criterion": "tests", "status": "not_met", "evidence": []}],
            "required_rework": [],
        }
        with self.assertRaisesRegex(control.ControlError, "every criterion met"):
            control.validate_result_semantics("judge", result)

    def test_rejected_worker_returns_to_rework(self) -> None:
        result = {
            "verdict": "reject",
            "criteria": [{"criterion": "tests", "status": "not_met", "evidence": []}],
            "required_rework": ["Add the missing test."],
        }
        control.validate_result_semantics("judge", result)
        self.assertEqual("agent:rework", control.result_state("judge", result))

    def test_worker_blocker_never_advances_to_review(self) -> None:
        result = {"state": "agent_review", "blockers": ["external input"]}
        self.assertEqual("agent:blocked", control.result_state("worker", result))

    def test_planner_requires_contract_and_digest_is_order_independent(self) -> None:
        first = {
            "issue": 10,
            "spec_revision": "a" * 40,
            "blockers": [],
            "tasks": [
                {
                    "key": "implementation",
                    "goal": "Implement it.",
                    "non_goals": ["No release."],
                    "required_behavior": "Feature behaves deterministically.",
                    "acceptance_checks": ["Run unit tests."],
                    "allowed_surfaces": ["firmware/domes/main/**"],
                    "dependencies": [],
                    "required_proof": ["Test log."],
                    "autonomy_policy": "software-review-required",
                },
                {
                    "key": "tests",
                    "goal": "Test it.",
                    "non_goals": ["No redesign."],
                    "required_behavior": "Tests cover the change.",
                    "acceptance_checks": ["Run unit tests."],
                    "allowed_surfaces": ["firmware/domes/tests/**"],
                    "dependencies": ["implementation"],
                    "required_proof": ["Test log."],
                    "autonomy_policy": "software-review-required",
                },
            ],
        }
        control.validate_result_semantics("planner", first)
        reversed_tasks = {**first, "tasks": list(reversed(first["tasks"]))}
        self.assertEqual(
            control.plan_digest(first), control.plan_digest(reversed_tasks)
        )
        invalid = {
            **first,
            "tasks": [{**first["tasks"][0], "dependencies": ["missing"]}],
        }
        with self.assertRaisesRegex(control.ControlError, "unknown task dependencies"):
            control.validate_result_semantics("planner", invalid)


class AutopilotReviewTest(unittest.TestCase):
    revision = "a" * 40

    def test_required_ci_reports_green_pending_failed_and_missing(self) -> None:
        policy = control.load_autopilot_policy()
        green = pull_request(policy)
        self.assertEqual("passed", control.required_check_summary(green, policy)[0])
        pending = pull_request(
            policy,
            checks=tuple(
                {
                    "name": name,
                    "state": "PENDING" if index == 0 else "SUCCESS",
                    "url": "",
                }
                for index, name in enumerate(policy.required_ci_checks)
            ),
        )
        self.assertEqual("pending", control.required_check_summary(pending, policy)[0])
        failed = pull_request(
            policy,
            checks=tuple(
                {
                    "name": name,
                    "state": "FAILURE" if index == 1 else "SUCCESS",
                    "url": "",
                }
                for index, name in enumerate(policy.required_ci_checks)
            ),
        )
        self.assertEqual("failed", control.required_check_summary(failed, policy)[0])
        missing = pull_request(policy, checks=())
        self.assertEqual("pending", control.required_check_summary(missing, policy)[0])

    def test_physical_proof_helpers_require_current_passed_artifact(self) -> None:
        ticket = automated_ticket(
            31, self.revision, proof=("Physical hardware verification artifact.",)
        )
        self.assertTrue(
            control.requires_physical_proof(control.parse_sections(ticket.body))
        )
        self.assertFalse(control.has_current_physical_proof({"verification": []}))
        self.assertTrue(
            control.has_current_physical_proof(
                {
                    "verification": [
                        {"level": "physical", "status": "passed", "artifact": "run-1"}
                    ]
                }
            )
        )

    def test_passing_ci_stops_at_human_review_without_pr_mutation(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(32, self.revision, label="agent:ci-pending")
        pr = pull_request(policy)
        artifact = {
            "commit": pr.head_oid,
            "pull_request": pr.number,
            "verification": [],
        }
        judge = {"verdict": "approve", "commit": pr.head_oid, "pull_request": pr.number}
        validation = control.TicketValidation(
            ticket, control.parse_sections(ticket.body), (), ()
        )
        with (
            mock.patch.object(control, "validate_ticket", return_value=validation),
            mock.patch.object(
                control, "load_latest_artifact_handoff", return_value=artifact
            ),
            mock.patch.object(control, "load_exact_role_handoff", return_value=judge),
            mock.patch.object(control, "load_pull_request", return_value=pr),
            mock.patch.object(control, "transition") as transition,
            mock.patch.object(control, "post_controller_comment") as comment,
            mock.patch.object(control.subprocess, "run") as run,
        ):
            result = control.reconcile_ci_ticket(workflow, policy, ticket)
        self.assertEqual(pr.number, result["pull_request"])
        self.assertEqual("agent:human-review", result["state"])
        self.assertEqual("human", result["review_authority"])
        comment.assert_called_once()
        transition.assert_called_once_with(workflow, ticket, "agent:human-review")
        run.assert_not_called()

    def test_complete_issue_closes_and_sets_done_in_one_request(self) -> None:
        workflow = control.load_workflow()
        ticket = automated_ticket(36, self.revision)
        completed = subprocess.CompletedProcess([], 0, "", "")
        with mock.patch.object(
            control.subprocess, "run", return_value=completed
        ) as run:
            control.complete_issue(workflow, ticket)
        command = run.call_args.args[0]
        payload = json.loads(run.call_args.kwargs["input"])
        self.assertEqual("PATCH", command[command.index("--method") + 1])
        self.assertEqual("closed", payload["state"])
        self.assertEqual("completed", payload["state_reason"])
        self.assertIn("agent:done", payload["labels"])
        self.assertNotIn("agent:ci-pending", payload["labels"])

    def test_ci_tracker_failure_retries_without_changing_ticket_state(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(34, self.revision)
        with (
            mock.patch.object(
                control,
                "reconcile_ci_ticket",
                side_effect=control.TrackerError("temporary GitHub outage"),
            ),
            mock.patch.object(control, "transition") as transition,
            mock.patch.object(control, "post_controller_comment") as comment,
        ):
            results = control.reconcile_ci(workflow, policy, (ticket,))
        self.assertEqual("agent:ci-pending", results[0]["state"])
        self.assertEqual("temporary GitHub outage", results[0]["retryable_error"])
        transition.assert_not_called()
        comment.assert_not_called()

    def test_human_merge_refresh_failure_precedes_issue_completion(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(35, self.revision, label="agent:human-review")
        merged = pull_request(policy, state="MERGED")
        failed = subprocess.CompletedProcess([], 1, "", "temporary fetch failure")
        with (
            mock.patch.object(control, "_git", return_value=failed),
            mock.patch.object(control, "complete_issue") as complete_issue,
            mock.patch.object(control, "post_controller_comment") as comment,
        ):
            with self.assertRaisesRegex(
                control.TrackerError, "human-merge bookkeeping must be retried"
            ):
                control.finalize_human_merged_ticket(workflow, ticket, merged)
        complete_issue.assert_not_called()
        comment.assert_not_called()

    def test_human_merge_completion_failure_remains_retryable(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(37, self.revision, label="agent:human-review")
        merged = pull_request(policy, state="MERGED")
        completed = subprocess.CompletedProcess([], 0, "", "")
        with (
            mock.patch.object(control, "_git", return_value=completed),
            mock.patch.object(control, "post_controller_comment") as comment,
            mock.patch.object(
                control,
                "complete_issue",
                side_effect=control.ControlError("temporary update failure"),
            ) as complete_issue,
        ):
            with self.assertRaisesRegex(
                control.TrackerError, "human-merge bookkeeping must be retried"
            ):
                control.finalize_human_merged_ticket(workflow, ticket, merged)
        comment.assert_called_once()
        complete_issue.assert_called_once_with(workflow, ticket)

    def test_open_human_review_does_not_block_or_mutate(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(38, self.revision, label="agent:human-review")
        pr = pull_request(policy)
        artifact = {"commit": pr.head_oid, "pull_request": pr.number}
        validation = control.TicketValidation(
            ticket, control.parse_sections(ticket.body), (), ()
        )
        with (
            mock.patch.object(control, "validate_ticket", return_value=validation),
            mock.patch.object(
                control, "load_latest_artifact_handoff", return_value=artifact
            ),
            mock.patch.object(control, "load_pull_request", return_value=pr),
            mock.patch.object(control, "transition") as transition,
            mock.patch.object(control, "post_controller_comment") as comment,
        ):
            result = control.reconcile_human_reviews(workflow, (ticket,))
        self.assertEqual([], result)
        transition.assert_not_called()
        comment.assert_not_called()

    def test_manual_human_review_without_pull_request_is_not_reconciled(self) -> None:
        workflow = control.load_workflow()
        ticket = make_ticket(41, self.revision, label="agent:human-review")
        with (
            mock.patch.object(control, "validate_ticket") as validate,
            mock.patch.object(control, "load_latest_artifact_handoff") as artifact,
            mock.patch.object(control, "load_pull_request") as pull_request_loader,
            mock.patch.object(control, "transition") as transition,
            mock.patch.object(control, "post_controller_comment") as comment,
        ):
            result = control.reconcile_human_reviews(workflow, (ticket,))
        self.assertEqual([], result)
        validate.assert_not_called()
        artifact.assert_not_called()
        pull_request_loader.assert_not_called()
        transition.assert_not_called()
        comment.assert_not_called()

    def test_observed_human_merge_completes_issue_bookkeeping(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        ticket = automated_ticket(39, self.revision, label="agent:human-review")
        merged = pull_request(policy, state="MERGED")
        artifact = {"commit": merged.head_oid, "pull_request": merged.number}
        validation = control.TicketValidation(
            ticket, control.parse_sections(ticket.body), (), ()
        )
        completed = subprocess.CompletedProcess([], 0, "", "")
        with (
            mock.patch.object(control, "validate_ticket", return_value=validation),
            mock.patch.object(
                control, "load_latest_artifact_handoff", return_value=artifact
            ),
            mock.patch.object(control, "load_pull_request", return_value=merged),
            mock.patch.object(control, "_git", return_value=completed),
            mock.patch.object(control, "complete_issue") as complete_issue,
            mock.patch.object(control, "post_controller_comment") as comment,
        ):
            result = control.reconcile_human_reviews(workflow, (ticket,))
        self.assertEqual("agent:done", result[0]["state"])
        self.assertEqual("human", result[0]["merged_by"])
        comment.assert_called_once()
        complete_issue.assert_called_once_with(workflow, ticket)


class SelectorAndPlanTest(unittest.TestCase):
    revision = "a" * 40

    def selector_result(
        self,
        *,
        revision: str | None = None,
        existing_issue: int = 0,
        existing_pull_request: int = 0,
        surfaces: list[str] | None = None,
    ) -> dict[str, object]:
        return {
            "state": "selected",
            "spec_revision": revision or self.revision,
            "mode": "execute",
            "existing_issue": existing_issue,
            "existing_pull_request": existing_pull_request,
            "work_package": "FS-WP-TEST",
            "work_class": "software",
            "title": "Autopilot test work",
            "parent_objective": "Deliver the test outcome.",
            "goal": "Implement the test change.",
            "non_goals": ["No release."],
            "required_behavior": "The test behavior is deterministic.",
            "acceptance_checks": ["Run tests."],
            "allowed_surfaces": surfaces or ["firmware/domes/main/trace/**"],
            "dependencies": [],
            "required_proof": ["Test log."],
            "priority": "p1",
            "autonomy_policy": "software-review-required",
            "rationale": "Current milestone names this package.",
            "blockers": [],
        }

    def test_selector_pins_exact_main_revision_and_rejects_protected_surface(
        self,
    ) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        with mock.patch.object(
            control, "origin_main_revision", return_value=self.revision
        ):
            with self.assertRaisesRegex(control.ControlError, "pin the current"):
                control.validate_selector_result(
                    self.selector_result(revision="b" * 40), workflow, policy, (), ()
                )
            with self.assertRaisesRegex(control.ControlError, "protected surfaces"):
                control.validate_selector_result(
                    self.selector_result(surfaces=[".github/**"]),
                    workflow,
                    policy,
                    (),
                    (),
                )

    def test_selector_rejects_unavailable_existing_issue_or_pull_request(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        with mock.patch.object(
            control, "origin_main_revision", return_value=self.revision
        ):
            with self.assertRaisesRegex(
                control.ControlError, "unavailable existing issue"
            ):
                control.validate_selector_result(
                    self.selector_result(existing_issue=91), workflow, policy, (), ()
                )
            with self.assertRaisesRegex(
                control.ControlError, "unavailable existing pull request"
            ):
                control.validate_selector_result(
                    self.selector_result(existing_issue=7, existing_pull_request=77),
                    workflow,
                    policy,
                    (make_ticket(7, self.revision),),
                    (),
                )

    def test_selector_retries_semantically_invalid_result(self) -> None:
        workflow = control.load_workflow()
        policy = control.load_autopilot_policy()
        prompts: list[str] = []

        def run_attempt(command, prompt, *_args):
            prompts.append(prompt)
            result_path = Path(command[command.index("--output-last-message") + 1])
            result_path.write_text(
                json.dumps({"state": "idle", "work_package": ""}),
                encoding="utf-8",
            )
            return 0, ""

        with tempfile.TemporaryDirectory() as directory:
            with (
                mock.patch.dict(os.environ, {"XDG_STATE_HOME": directory}),
                mock.patch.object(
                    control, "build_selector_prompt", return_value="prompt"
                ),
                mock.patch.object(
                    control, "run_codex_attempt", side_effect=run_attempt
                ),
                mock.patch.object(
                    control,
                    "validate_selector_result",
                    side_effect=[control.ControlError("bad issue/PR pairing"), None],
                ) as validate,
                mock.patch.object(control, "apply_selector_result", return_value=None),
                mock.patch.object(control.time, "sleep") as sleep,
            ):
                result = control.run_selector(workflow, policy, (), ())
        self.assertEqual("idle", result["state"])
        self.assertEqual(2, validate.call_count)
        self.assertEqual(2, len(prompts))
        self.assertIn("bad issue/PR pairing", prompts[1])
        sleep.assert_called_once_with(10)

    def test_materialize_plan_reuses_matching_child_on_retry(self) -> None:
        workflow = control.load_workflow()
        parent_ticket = automated_ticket(
            40,
            self.revision,
            label="agent:plan",
            surfaces=("firmware/domes/main/trace/**",),
        )
        parent = control.validate_ticket(parent_ticket, check_revision=False)
        task = {
            "key": "implementation",
            "goal": "Implement the bounded task.",
            "non_goals": ["No release."],
            "required_behavior": "The bounded behavior is deterministic.",
            "acceptance_checks": ["Run focused tests."],
            "allowed_surfaces": ["firmware/domes/main/trace/**"],
            "dependencies": [],
            "required_proof": ["Focused test output."],
            "autonomy_policy": "software-review-required",
        }
        result = {
            "issue": parent.ticket.number,
            "spec_revision": self.revision,
            "tasks": [task],
            "blockers": [],
        }
        plan_hash = control.plan_digest(result)
        marker = (
            "<!-- domes-autopilot-task:v1 "
            f"parent={parent.ticket.number} plan={plan_hash} key=implementation "
            f"uid={control.task_uid(parent, plan_hash, task)} -->"
        )
        contract = control.render_ticket_contract(
            spec_revision=self.revision,
            parent_objective=f"{parent.sections['Parent objective']} Parent planning issue #40.",
            goal=task["goal"],
            non_goals=task["non_goals"],
            required_behavior=task["required_behavior"],
            acceptance_checks=task["acceptance_checks"],
            allowed_surface_values=task["allowed_surfaces"],
            dependencies=(40,),
            required_proof=task["required_proof"],
            work_package="FS-WP-TEST",
            work_class="software",
            selected_policy="software-review-required",
        )
        body = marker + "\n\n" + control.with_autopilot_contract("", contract)
        child = control.Ticket(
            55, "[Agent] implementation", body, "OPEN", ("agent:ready",), ""
        )
        valid_child = control.TicketValidation(
            child, control.parse_sections(body), (), (40,)
        )
        with (
            mock.patch.object(control, "load_live_tickets", return_value=[child]),
            mock.patch.object(control, "create_issue") as create_issue,
            mock.patch.object(control, "update_issue_body") as update_body,
            mock.patch.object(control, "transition") as transition,
            mock.patch.object(control, "validate_ticket", return_value=valid_child),
        ):
            self.assertEqual([55], control.materialize_plan(workflow, parent, result))
        create_issue.assert_not_called()
        update_body.assert_not_called()
        transition.assert_not_called()


class ReviewFixRegressionTest(unittest.TestCase):
    revision = "a" * 40

    def test_exact_handoff_loads_local_evidence_and_checks_specification(self) -> None:
        workflow = control.load_workflow()
        ticket = make_ticket(60, self.revision)
        handoff = {
            "issue": ticket.number,
            "spec_revision": self.revision,
            "state": "agent_review",
            "commit": "b" * 40,
            "pull_request": 77,
            "verification": [],
            "blockers": [],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "handoff-worker.json").write_text(
                json.dumps(handoff), encoding="utf-8"
            )
            self.assertEqual(
                handoff,
                control.load_exact_role_handoff(workflow, ticket, "worker", root),
            )
            handoff["spec_revision"] = "c" * 40
            (root / "handoff-worker.json").write_text(
                json.dumps(handoff), encoding="utf-8"
            )
            with self.assertRaisesRegex(control.ControlError, "handoff spec mismatch"):
                control.load_exact_role_handoff(workflow, ticket, "worker", root)

    def _run_result(self, result: dict[str, object]):
        def fake_run(command, *_args):
            output = Path(command[command.index("--output-last-message") + 1])
            output.write_text(json.dumps(result), encoding="utf-8")
            return 0, ""

        return fake_run

    def test_judge_rejects_result_not_bound_to_worker_artifact(self) -> None:
        workflow = control.load_workflow()
        ticket = make_ticket(61, self.revision, label="agent:agent-review")
        item = control.validate_ticket(ticket, check_revision=False)
        worker = {
            "issue": 61,
            "spec_revision": self.revision,
            "commit": "b" * 40,
            "pull_request": 77,
        }
        result = {
            "issue": 61,
            "spec_revision": self.revision,
            "verdict": "approve",
            "criteria": [{"criterion": "tests", "status": "met", "evidence": ["log"]}],
            "required_rework": [],
            "commit": "c" * 40,
            "pull_request": 77,
        }
        with tempfile.TemporaryDirectory() as directory:
            with (
                mock.patch.dict(os.environ, {"XDG_STATE_HOME": directory}),
                mock.patch.object(
                    control, "ensure_workspace", return_value=Path(directory)
                ),
                mock.patch.object(
                    control, "required_prior_handoff", return_value=worker
                ),
                mock.patch.object(
                    control, "run_codex_attempt", side_effect=self._run_result(result)
                ) as run,
            ):
                with self.assertRaisesRegex(
                    control.ControlError, "not bound to the reviewed artifact"
                ):
                    control.execute_one(workflow, item)
        run.assert_called_once()

    def test_verification_repair_must_return_to_independent_review(self) -> None:
        workflow = control.load_workflow()
        ticket = make_ticket(62, self.revision, label="agent:verification")
        item = control.validate_ticket(ticket, check_revision=False)
        judge = {
            "issue": 62,
            "spec_revision": self.revision,
            "commit": "b" * 40,
            "pull_request": 77,
        }
        result = {
            "issue": 62,
            "spec_revision": self.revision,
            "state": "human_review",
            "checks": [{"name": "unit", "status": "passed", "evidence": ["log"]}],
            "repairs": ["Fixed CI configuration."],
            "blockers": [],
            "commit": "b" * 40,
            "pull_request": 77,
            "verification": [],
        }
        with tempfile.TemporaryDirectory() as directory:
            with (
                mock.patch.dict(os.environ, {"XDG_STATE_HOME": directory}),
                mock.patch.object(
                    control, "ensure_workspace", return_value=Path(directory)
                ),
                mock.patch.object(
                    control, "required_prior_handoff", return_value=judge
                ),
                mock.patch.object(
                    control, "run_codex_attempt", side_effect=self._run_result(result)
                ),
            ):
                with self.assertRaisesRegex(
                    control.ControlError, "every CI repair must return"
                ):
                    control.execute_one(workflow, item)

    def test_pending_planner_journal_recovers_without_starting_codex(self) -> None:
        workflow = control.load_workflow()
        ticket = automated_ticket(63, self.revision, label="agent:plan")
        item = control.validate_ticket(ticket, check_revision=False)
        plan = {
            "issue": 63,
            "spec_revision": self.revision,
            "blockers": [],
            "tasks": [
                {
                    "key": "recover",
                    "goal": "Recover the planned task.",
                    "non_goals": ["No release."],
                    "required_behavior": "Recovery is deterministic.",
                    "acceptance_checks": ["Run focused tests."],
                    "allowed_surfaces": ["firmware/domes/main/trace/**"],
                    "dependencies": [],
                    "required_proof": ["Focused log."],
                    "autonomy_policy": "software-review-required",
                }
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            run_root = Path(directory) / "domes-agent-control" / "issue-63"
            run_root.mkdir(parents=True)
            (run_root / "pending-plan.json").write_text(
                json.dumps(plan), encoding="utf-8"
            )
            with (
                mock.patch.dict(os.environ, {"XDG_STATE_HOME": directory}),
                mock.patch.object(
                    control, "ensure_workspace", return_value=Path(directory)
                ),
                mock.patch.object(
                    control, "materialize_plan", return_value=[88]
                ) as materialize,
                mock.patch.object(
                    control,
                    "run_codex_attempt",
                    side_effect=AssertionError("must not run"),
                ),
                mock.patch.object(control, "post_result"),
                mock.patch.object(control, "transition") as transition,
                mock.patch.object(control, "close_issue") as close_issue,
            ):
                recovered = control.execute_one(workflow, item, autopilot=True)
        self.assertTrue(recovered["recovered"])
        self.assertEqual([88], recovered["materialized"])
        materialize.assert_called_once_with(workflow, item, plan)
        transition.assert_called_once_with(workflow, ticket, "agent:done")
        close_issue.assert_called_once_with(workflow, ticket.number)

    def test_dashboard_shows_active_work_and_human_review_without_raw_paths(
        self,
    ) -> None:
        workflow = control.load_workflow()
        active_ticket = automated_ticket(64, self.revision, label="agent:running")
        review_ticket = automated_ticket(65, self.revision, label="agent:human-review")
        active = control.TicketValidation(
            active_ticket, control.parse_sections(active_ticket.body), (), ()
        )
        rendered = control.render_dashboard(
            workflow,
            (active_ticket, review_ticket),
            (active,),
            (),
            {},
            {
                "runs": [
                    {
                        "issue": 63,
                        "role": "judge",
                        "state": "agent:ci-pending",
                        "events": "/private/raw-worker-events.jsonl",
                    }
                ]
            },
            phase="working",
        )
        self.assertIn("HUMAN REVIEW AND MERGE REQUIRED", rendered)
        self.assertIn("#64 worker | PR #77", rendered)
        self.assertIn("PR #77 | issue #65", rendered)
        self.assertIn("#63 judge → ci-pending", rendered)
        self.assertNotIn("raw-worker-events", rendered)

    def test_dashboard_identifies_manual_review_without_pull_request(self) -> None:
        workflow = control.load_workflow()
        ticket = make_ticket(66, self.revision, label="agent:human-review")
        rendered = control.render_dashboard(
            workflow,
            (ticket,),
            (),
            (),
            {},
            {"ci": [{"issue": 66, "state": "agent:human-review"}]},
        )
        self.assertIn("manual review (no PR) | issue #66", rendered)

    def test_pull_request_loader_rejects_incomplete_paginated_file_list(self) -> None:
        workflow = control.load_workflow()
        document = {
            "number": 77,
            "state": "OPEN",
            "isDraft": False,
            "baseRefName": "main",
            "baseRefOid": "a" * 40,
            "headRefName": "codex/test",
            "headRefOid": "b" * 40,
            "mergeable": "MERGEABLE",
            "mergeStateStatus": "CLEAN",
            "reviewDecision": "",
            "statusCheckRollup": [],
            "mergeCommit": {},
            "changedFiles": 2,
        }
        with mock.patch.object(
            control, "_run_json", side_effect=(document, [[{"filename": "one.py"}]])
        ):
            with self.assertRaisesRegex(
                control.ControlError, "changed-file list is incomplete"
            ):
                control.load_pull_request(workflow, 77)


class ProcessLifecycleTest(unittest.TestCase):
    def test_inactive_agent_attempt_is_terminated(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lease = root / "active-process.json"
            code, reason = control.run_codex_attempt(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                "",
                root / "events.jsonl",
                root / "stderr.log",
                1,
                lease,
            )
            self.assertFalse(lease.exists())
            self.assertNotEqual(0, code)
            self.assertIn("no Codex event activity", reason)

    def test_active_agent_attempt_can_complete(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lease = root / "active-process.json"
            code, reason = control.run_codex_attempt(
                [sys.executable, "-c", "print('event', flush=True)"],
                "",
                root / "events.jsonl",
                root / "stderr.log",
                5,
                lease,
            )
            events = (root / "events.jsonl").read_text(encoding="utf-8")
            self.assertFalse(lease.exists())
            self.assertEqual(0, code)
            self.assertEqual("", reason)
            self.assertEqual("event\n", events)

    def test_recorded_orphan_is_terminated_before_restart(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            lease = Path(directory) / "active-process.json"
            process = subprocess.Popen(
                [
                    sys.executable,
                    "-c",
                    "import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    "time.sleep(30)",
                ],
                start_new_session=True,
            )
            control.write_process_lease(lease, process.pid)
            time.sleep(0.1)
            control.terminate_recorded_process_group(lease)
            process.wait(timeout=5)
            self.assertFalse(lease.exists())
            self.assertFalse(control.process_group_exists(process.pid))
            self.assertNotEqual(0, process.returncode)

    def test_recorded_process_group_survives_leader_exit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            lease = Path(directory) / "active-process.json"
            process = subprocess.Popen(
                [
                    sys.executable,
                    "-c",
                    "import os,time; time.sleep(.2); "
                    "child=os.fork(); time.sleep(30) if child == 0 else os._exit(0)",
                ],
                start_new_session=True,
            )
            control.write_process_lease(lease, process.pid)
            process.wait(timeout=5)
            self.assertTrue(control.process_group_exists(process.pid))
            control.terminate_recorded_process_group(lease)
            self.assertFalse(lease.exists())

    def test_cleanup_rejects_reused_pid_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            lease = Path(directory) / "active-process.json"
            lease.write_text(
                json.dumps({"pid": 12345, "start_ticks": 100}) + "\n",
                encoding="utf-8",
            )
            with (
                mock.patch.object(control, "process_start_ticks", return_value=200),
                mock.patch.object(control, "process_group_exists", return_value=True),
                mock.patch.object(control, "terminate_process_group") as terminate,
            ):
                control.terminate_recorded_process_group(lease, 12345)
            terminate.assert_not_called()
            self.assertFalse(lease.exists())

    def test_failed_lease_write_never_releases_agent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / "agent-started"
            with mock.patch.object(
                control, "write_process_lease", side_effect=OSError("write failed")
            ):
                with self.assertRaisesRegex(OSError, "write failed"):
                    control.start_leased_process(
                        [
                            sys.executable,
                            "-c",
                            "from pathlib import Path; Path(r'%s').touch()" % marker,
                        ],
                        root / "active-process.json",
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
            self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()
