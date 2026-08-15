import json
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


class WorkflowTest(unittest.TestCase):
    def test_checked_in_workflow_loads(self) -> None:
        workflow = control.load_workflow()
        self.assertEqual("pcesar22/domes", workflow.repository)
        self.assertEqual("ministrom", workflow.scheduler_host)
        self.assertEqual(3, workflow.max_concurrent_workers)
        self.assertEqual("main", workflow.base_branch)

    def test_repository_contracts_validate(self) -> None:
        self.assertEqual([], control.validate_repository())

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
