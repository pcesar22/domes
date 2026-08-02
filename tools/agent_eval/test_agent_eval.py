import copy
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch

from tools.agent_eval.agent_eval import (
    DEFAULT_RESPONSE_SCHEMA,
    EvaluationCase,
    EvaluationCriterion,
    EvaluationRuntime,
    PreparedCheckout,
    _assert_offline_git_operation,
    _case_prompt,
    _checkout_changes,
    _checkout_manifest,
    _codex_command,
    _codex_environment,
    _collect_submodule_plan,
    _containment_command,
    _containment_preflight,
    _copy_sanitized_checkout,
    _initialize_snapshot_repository,
    _local_git_environment,
    _manifest_digest,
    _materialize_git_tree,
    _permission_profile_config,
    _prepare_checkout,
    _prepare_codex_home,
    _resolve_commit,
    _resolve_runtime,
    _run_case,
    _run_codex_process,
    _shell_environment_config,
    _tracked_definition,
    assess_response_coverage,
    build_parser,
    evaluation_exit_code,
    load_cases,
    render_report,
    run_evaluations,
    validate_response_document,
    validate_response_schema,
)


def evaluation_case(**overrides) -> EvaluationCase:
    values = {
        "identifier": "case-one",
        "title": "Case one",
        "category": "test",
        "prompt": "Inspect the repository.",
        "reference_files": ("AGENTS.md",),
        "criteria": (
            EvaluationCriterion("repository-evidence", "Supply repository evidence."),
        ),
        "sandbox": "read-only",
        "hardware_requirement": "not_required",
        "cleanup": "Remove the disposable checkout.",
    }
    values.update(overrides)
    return EvaluationCase(**values)


def structured_response(**overrides):
    values = {
        "summary": "Repository evidence supports review.",
        "files": ["AGENTS.md"],
        "invariants": ["The repository contract remains authoritative."],
        "verification": ["Review the cited source."],
        "hardware_requirement": "not_required",
        "claims": ["The evidence requires human review."],
        "criterion_evidence": [
            {
                "criterion_id": "repository-evidence",
                "evidence": ["AGENTS.md defines the repository contract."],
                "files": ["AGENTS.md"],
            }
        ],
    }
    values.update(overrides)
    return values


def evaluation_runtime() -> EvaluationRuntime:
    return EvaluationRuntime(
        codex_executable=Path("/opt/codex/bin/codex"),
        codex_read_root=Path("/opt/codex"),
        bubblewrap_executable=Path("/usr/bin/bwrap"),
        shell_path="/opt/codex/rg:/usr/bin:/bin",
    )


def run_git(repository: Path, *args: str) -> str:
    process = subprocess.run(
        ["git", *args],
        cwd=repository,
        check=True,
        capture_output=True,
        text=True,
    )
    return process.stdout.strip()


def initialize_repository(repository: Path) -> None:
    repository.mkdir(parents=True)
    run_git(repository, "init", "--quiet")
    run_git(repository, "config", "user.name", "Agent Eval Test")
    run_git(repository, "config", "user.email", "agent-eval-test@invalid")


def commit_all(repository: Path, message: str) -> str:
    run_git(repository, "add", "--all")
    run_git(repository, "commit", "--quiet", "-m", message)
    return run_git(repository, "rev-parse", "HEAD")


def add_gitlink(repository: Path, relative: str, revision: str) -> str:
    modules = repository / ".gitmodules"
    modules.write_text(
        f'[submodule "{relative}"]\n\tpath = {relative}\n'
        f"\turl = https://example.invalid/{relative}.git\n",
        encoding="utf-8",
    )
    run_git(repository, "add", ".gitmodules")
    run_git(
        repository,
        "update-index",
        "--add",
        "--cacheinfo",
        f"160000,{revision},{relative}",
    )
    run_git(repository, "commit", "--quiet", "-m", "Add submodule gitlink")
    return run_git(repository, "rev-parse", "HEAD")


class AgentEvaluationContractTest(unittest.TestCase):
    def test_checked_in_cases_are_valid_and_representative(self) -> None:
        cases = load_cases()

        self.assertGreaterEqual(len(cases), 10)
        self.assertIn("protocol", {case.category for case in cases})
        self.assertIn("firmware", {case.category for case in cases})
        self.assertIn("flutter", {case.category for case in cases})
        self.assertIn("hardware", {case.category for case in cases})
        self.assertTrue(all(case.cleanup and case.criteria for case in cases))
        self.assertTrue(all(case.sandbox == "read-only" for case in cases))
        self.assertEqual(
            {"not_required", "required"},
            {case.hardware_requirement for case in cases},
        )

    def test_reference_paths_exist_in_the_repository(self) -> None:
        root = Path(__file__).resolve().parents[2]
        missing = [
            relative
            for case in load_cases()
            for relative in case.reference_files
            if not (root / relative).exists()
        ]

        self.assertEqual([], missing)

    def test_duplicate_case_and_criterion_ids_are_rejected(self) -> None:
        case = {
            "id": "duplicate",
            "title": "Duplicate",
            "category": "test",
            "prompt": "Inspect the repository.",
            "reference_files": [],
            "criteria": [
                {"id": "same-id", "description": "First."},
                {"id": "same-id", "description": "Second."},
            ],
            "sandbox": "read-only",
            "hardware_requirement": "not_required",
            "cleanup": "Remove the temporary checkout.",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cases.json"
            path.write_text(
                json.dumps({"schema_version": 3, "cases": [case]}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate criterion id"):
                load_cases(path)

            case["criteria"] = [{"id": "one", "description": "One."}]
            path.write_text(
                json.dumps({"schema_version": 3, "cases": [case, case]}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate case id"):
                load_cases(path)

    def test_unknown_case_fields_are_rejected(self) -> None:
        case = {
            "id": "unknown",
            "title": "Unknown",
            "category": "test",
            "prompt": "Inspect.",
            "reference_files": [],
            "criteria": [{"id": "one", "description": "One."}],
            "sandbox": "read-only",
            "hardware_requirement": "not_required",
            "cleanup": "Clean up.",
            "required_terms": ["legacy"],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cases.json"
            path.write_text(
                json.dumps({"schema_version": 3, "cases": [case]}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "unknown fields"):
                load_cases(path)

    def test_workspace_write_cases_are_rejected(self) -> None:
        case = {
            "id": "write-case",
            "title": "Write case",
            "category": "test",
            "prompt": "Edit a file.",
            "reference_files": [],
            "criteria": [{"id": "one", "description": "One."}],
            "sandbox": "workspace-write",
            "hardware_requirement": "not_required",
            "cleanup": "Clean up.",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cases.json"
            path.write_text(
                json.dumps({"schema_version": 3, "cases": [case]}),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "invalid sandbox"):
                load_cases(path)

    def test_checked_in_response_schema_has_expected_structure(self) -> None:
        schema = json.loads(DEFAULT_RESPONSE_SCHEMA.read_text(encoding="utf-8"))
        validate_response_schema(schema)

        for mutation, message in (
            (
                lambda value: value.__setitem__("additionalProperties", True),
                "additional",
            ),
            (lambda value: value["required"].remove("claims"), "required"),
            (
                lambda value: value["properties"]["criterion_evidence"][
                    "items"
                ].__setitem__("additionalProperties", True),
                "criterion evidence",
            ),
            (
                lambda value: value["properties"]["files"].__setitem__(
                    "items", {"type": "integer"}
                ),
                "items",
            ),
            (
                lambda value: value["properties"]["hardware_requirement"].__setitem__(
                    "enum", ["not_required"]
                ),
                "enum",
            ),
            (
                lambda value: value["properties"]["hardware_requirement"].pop(
                    "description"
                ),
                "ambiguous",
            ),
        ):
            invalid = copy.deepcopy(schema)
            mutation(invalid)
            with self.assertRaisesRegex(ValueError, message):
                validate_response_schema(invalid)

    def test_malformed_hardware_type_is_a_validation_error(self) -> None:
        with self.assertRaisesRegex(ValueError, "hardware_requirement"):
            validate_response_document(structured_response(hardware_requirement=[]))
        with self.assertRaisesRegex(ValueError, "hardware_requirement"):
            validate_response_document(
                structured_response(hardware_requirement="unavailable")
            )

    def test_duplicate_criterion_evidence_is_rejected(self) -> None:
        response = structured_response()
        response["criterion_evidence"].append(response["criterion_evidence"][0])
        with self.assertRaisesRegex(ValueError, "duplicate criterion evidence"):
            validate_response_document(response)


class EvidenceCoverageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.checkout = Path(self.temp_dir.name)
        (self.checkout / "AGENTS.md").write_text("contract\n", encoding="utf-8")
        self.case = evaluation_case()

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_complete_evidence_is_review_ready_but_never_marked_correct(self) -> None:
        response = structured_response(
            claims=[
                'The statement "unit tests prove BLE hardware" is false.',
                "Evidence evidence evidence.",
            ]
        )

        assessment = assess_response_coverage(self.case, response, self.checkout)

        self.assertTrue(assessment["review_ready"])
        self.assertTrue(assessment["review_required"])
        self.assertNotIn("passed", assessment)
        self.assertNotIn("score", assessment)

    def test_missing_criterion_evidence_is_incomplete(self) -> None:
        response = structured_response(criterion_evidence=[])

        assessment = assess_response_coverage(self.case, response, self.checkout)

        self.assertFalse(assessment["review_ready"])
        self.assertEqual(0, assessment["coverage"]["covered"])
        self.assertIn(
            "missing criterion evidence",
            assessment["coverage"]["criteria"][0]["coverage_issues"],
        )

    def test_nonexistent_evidence_path_is_incomplete(self) -> None:
        response = structured_response(files=["AGENTS.md", "missing.md"])
        response["criterion_evidence"][0]["files"] = ["missing.md"]

        assessment = assess_response_coverage(self.case, response, self.checkout)

        self.assertFalse(assessment["review_ready"])
        self.assertIn(
            "evidence path is not in the checkout: missing.md",
            assessment["coverage"]["criteria"][0]["coverage_issues"],
        )

    def test_hardware_requirement_is_not_an_execution_status(self) -> None:
        case = evaluation_case(hardware_requirement="required")

        matching = assess_response_coverage(
            case,
            structured_response(hardware_requirement="required"),
            self.checkout,
        )
        mismatched = assess_response_coverage(
            case,
            structured_response(hardware_requirement="not_required"),
            self.checkout,
        )

        self.assertTrue(matching["review_ready"])
        self.assertFalse(mismatched["review_ready"])
        hardware_check = next(
            check
            for check in mismatched["contract_checks"]
            if check["kind"] == "hardware_requirement"
        )
        self.assertEqual("required", hardware_check["value"])

    def test_evaluation_exit_policy_is_structural_only(self) -> None:
        ready = {"completed": 2, "review_ready": 2, "errors": 0}
        incomplete = {"completed": 2, "review_ready": 1, "errors": 0}
        errored = {"completed": 1, "review_ready": 1, "errors": 1}

        self.assertEqual(0, evaluation_exit_code(ready))
        self.assertEqual(1, evaluation_exit_code(incomplete))
        self.assertEqual(1, evaluation_exit_code(errored))


class IsolationTest(unittest.TestCase):
    def test_codex_environment_uses_isolated_paths_and_scrubs_credentials(self) -> None:
        environment = _codex_environment(
            Path("/tmp/isolated-home"),
            Path("/tmp/isolated-tmp"),
            Path("/tmp/auth-only-codex-home"),
            {
                "PATH": "/usr/bin",
                "HOME": "/home/test",
                "CODEX_HOME": "/auth/codex",
                "HTTPS_PROXY": "https://user:secret@proxy.invalid",
                "GH_TOKEN": "secret",
                "OPENAI_API_KEY": "secret",
                "SSH_AUTH_SOCK": "/tmp/agent.sock",
                "USER": "private-user",
                "UNRELATED_HOST_STATE": "private",
            },
            executable_path="/usr/bin",
        )

        self.assertEqual("/usr/bin", environment["PATH"])
        self.assertEqual("/tmp/auth-only-codex-home", environment["CODEX_HOME"])
        self.assertEqual("/tmp/isolated-home", environment["HOME"])
        self.assertEqual("/tmp/isolated-tmp", environment["TMPDIR"])
        for key in (
            "HTTPS_PROXY",
            "GH_TOKEN",
            "OPENAI_API_KEY",
            "SSH_AUTH_SOCK",
            "USER",
            "UNRELATED_HOST_STATE",
        ):
            self.assertNotIn(key, environment)

    def test_auth_only_codex_home_does_not_copy_user_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            destination = root / "isolated"
            source.mkdir()
            (source / "auth.json").write_text('{"token":"secret"}\n')
            (source / "config.toml").write_text("model = 'user-model'\n")

            source_auth = _prepare_codex_home(
                destination,
                {"HOME": str(root), "CODEX_HOME": str(source)},
            )

            self.assertEqual(source / "auth.json", source_auth)
            self.assertEqual(
                ["auth.json"], sorted(path.name for path in destination.iterdir())
            )
            self.assertEqual(0o600, (destination / "auth.json").stat().st_mode & 0o777)

    def test_permission_profile_is_default_deny_and_uses_exact_roots(self) -> None:
        case = evaluation_case()
        profile = _permission_profile_config(
            case,
            Path("/tmp/case/checkout"),
            Path("/tmp/case/shell-home"),
            Path("/tmp/case/command-tmp"),
            evaluation_runtime(),
        )
        shell = _shell_environment_config(
            Path("/tmp/case/shell-home"),
            Path("/tmp/case/command-tmp"),
            evaluation_runtime().shell_path,
        )

        self.assertIn('":minimal"="read"', profile)
        self.assertNotIn('":root"', profile)
        self.assertIn('"/tmp/case/checkout"="read"', profile)
        self.assertNotIn('"/tmp/case/checkout/.git"', profile)
        self.assertNotIn('"/tmp"=', profile)
        self.assertIn('inherit="none"', shell)
        self.assertNotIn("CODEX_HOME", shell)

        with self.assertRaisesRegex(ValueError, "read-only cases only"):
            _permission_profile_config(
                evaluation_case(sandbox="workspace-write"),
                Path("/tmp/case/checkout"),
                Path("/tmp/case/shell-home"),
                Path("/tmp/case/command-tmp"),
                evaluation_runtime(),
            )

    def test_outer_containment_uses_read_only_root_and_pid_namespace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            wrapped = _containment_command(
                ["/usr/bin/true"], evaluation_runtime(), (root,)
            )

        self.assertEqual("/usr/bin/bwrap", wrapped[0])
        self.assertIn("--ro-bind", wrapped)
        self.assertIn("--unshare-user", wrapped)
        self.assertIn("--unshare-pid", wrapped)
        self.assertIn("--die-with-parent", wrapped)

    @unittest.skipUnless(
        sys.platform == "linux" and shutil.which("codex") and shutil.which("bwrap"),
        "Codex and bubblewrap are required for the no-model containment probe",
    )
    def test_no_model_containment_preflight_denies_host_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            codex_home = root / "codex-home"
            codex_home.mkdir()
            source_auth = root / "source-auth.json"
            isolated_auth = codex_home / "auth.json"
            schema = root / "response.schema.json"
            source_auth.write_text("{}\n")
            isolated_auth.write_text("{}\n")
            schema.write_text("{}\n")

            result = _containment_preflight(
                _resolve_runtime(), codex_home, source_auth, schema
            )

        self.assertTrue(result["passed"])
        self.assertEqual(["read-only"], result["modes"])
        self.assertTrue(result["exec_configuration_validated"])

    def test_case_prompt_and_command_include_evidence_and_safety_contracts(
        self,
    ) -> None:
        case = evaluation_case()
        prompt = _case_prompt(case)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkout = root / "checkout"
            shell_home = root / "shell-home"
            command_tmp = root / "command-tmp"
            codex_home = root / "codex-home"
            for path in (checkout, shell_home, command_tmp, codex_home):
                path.mkdir()
            schema = root / "immutable-response.schema.json"
            command = _codex_command(
                case,
                "gpt-5.6-sol",
                "medium",
                checkout,
                root / "response.json",
                shell_home,
                command_tmp,
                codex_home,
                evaluation_runtime(),
                schema,
            )

        self.assertIn("Cleanup contract:", prompt)
        self.assertIn("repository-evidence", prompt)
        self.assertIn("reviewer decides correctness", prompt)
        self.assertIn("describes a requirement, not execution status", prompt)
        self.assertIn("--ignore-user-config", command)
        self.assertIn("--ignore-rules", command)
        self.assertTrue(
            any(
                value.startswith('shell_environment_policy={inherit="none"')
                for value in command
            )
        )
        self.assertTrue(
            any(value.startswith("permissions.agent-eval=") for value in command)
        )
        self.assertNotIn("--sandbox", command)
        self.assertIn("--unshare-pid", command)
        self.assertEqual(str(schema), command[command.index("--output-schema") + 1])
        self.assertEqual(prompt, command[-1])

    @patch("tools.agent_eval.agent_eval._git", return_value="a" * 40)
    def test_revision_is_verified_as_a_commit(self, git_mock) -> None:
        revision = _resolve_commit("feature/ref")

        self.assertEqual("a" * 40, revision)
        self.assertEqual(
            ("rev-parse", "--verify", "--end-of-options", "feature/ref^{commit}"),
            git_mock.call_args.args,
        )

    def test_external_case_definition_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            external = Path(directory) / "cases.json"
            external.write_text("{}\n")

            with self.assertRaisesRegex(ValueError, "tracked repository file"):
                _tracked_definition(external)

    def test_sanitized_copy_excludes_git_history_and_evaluator(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            destination = root / "checkout"
            (source / "tools/agent_eval").mkdir(parents=True)
            (source / "tools/agent_eval/cases.json").write_text("hidden\n")
            (source / "tools/README.md").write_text("visible\n")
            (source / ".git").write_text("gitdir: elsewhere\n")
            (source / "firmware/third_party/nanopb").mkdir(parents=True)
            (source / "firmware/third_party/nanopb/source.c").write_text("exact\n")
            (source / "firmware/third_party/nanopb/.git").write_text("hidden\n")

            _copy_sanitized_checkout(source, destination)

            self.assertFalse((destination / ".git").exists())
            self.assertFalse((destination / "tools/agent_eval").exists())
            self.assertTrue((destination / "tools/README.md").exists())
            self.assertTrue(
                (destination / "firmware/third_party/nanopb/source.c").exists()
            )
            self.assertFalse(
                (destination / "firmware/third_party/nanopb/.git").exists()
            )

    def test_snapshot_repository_has_one_clean_commit_without_hidden_rubric(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkout = root / "checkout"
            home = root / "home"
            checkout.mkdir()
            home.mkdir()
            (checkout / "AGENTS.md").write_text("contract\n")

            revision = _initialize_snapshot_repository(checkout, home)

            self.assertEqual(40, len(revision))
            count = subprocess.run(
                ["git", "rev-list", "--count", "HEAD"],
                cwd=checkout,
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertEqual("1", count.stdout.strip())
            hidden = subprocess.run(
                ["git", "show", "HEAD:tools/agent_eval/cases.json"],
                cwd=checkout,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(0, hidden.returncode)

    def test_snapshot_revision_is_deterministic_for_identical_content(self) -> None:
        revisions = []
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index in range(2):
                checkout = root / f"checkout-{index}"
                home = root / f"home-{index}"
                checkout.mkdir()
                home.mkdir()
                (checkout / "AGENTS.md").write_text("contract\n")
                revisions.append(_initialize_snapshot_repository(checkout, home))

        self.assertEqual(revisions[0], revisions[1])

    def test_manifest_and_git_status_detect_ignored_writes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkout = root / "checkout"
            home = root / "home"
            checkout.mkdir()
            home.mkdir()
            (checkout / ".gitignore").write_text("build/\n")
            (checkout / "AGENTS.md").write_text("contract\n")
            _initialize_snapshot_repository(checkout, home)
            baseline = _checkout_manifest(checkout)

            (checkout / "build").mkdir()
            (checkout / "build/output.bin").write_bytes(b"ignored write")
            changes = _checkout_changes(checkout, baseline)

            self.assertTrue(
                any("created: build/output.bin" in change for change in changes)
            )
            self.assertTrue(
                any("git-status: !! build/" in change for change in changes)
            )
            self.assertNotEqual(
                _manifest_digest(baseline),
                _manifest_digest(_checkout_manifest(checkout)),
            )


class OfflineSubmoduleSnapshotTest(unittest.TestCase):
    def test_missing_submodule_worktree_fails_before_source_creation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            temporary = root / "evaluation"
            initialize_repository(repository)
            revision = add_gitlink(repository, "deps/sub", "1" * 40)
            temporary.mkdir()

            with self.assertRaisesRegex(ValueError, "worktree is not initialized"):
                _prepare_checkout(revision, temporary, repository)

            self.assertFalse((temporary / "source").exists())

    def test_missing_recorded_submodule_object_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            submodule = repository / "deps/sub"
            initialize_repository(repository)
            initialize_repository(submodule)
            (submodule / "tracked.txt").write_text("available\n", encoding="utf-8")
            commit_all(submodule, "Available object")
            revision = add_gitlink(repository, "deps/sub", "2" * 40)

            with self.assertRaisesRegex(ValueError, "unavailable locally"):
                _collect_submodule_plan(revision, repository)

    def test_nested_missing_submodule_worktree_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            parent = repository / "deps/parent"
            initialize_repository(repository)
            initialize_repository(parent)
            parent_revision = add_gitlink(parent, "nested", "3" * 40)
            revision = add_gitlink(repository, "deps/parent", parent_revision)

            with self.assertRaisesRegex(
                ValueError, "not initialized: deps/parent/nested"
            ):
                _collect_submodule_plan(revision, repository)

    def test_snapshot_uses_recorded_objects_not_dirty_submodule_worktree(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            submodule = repository / "deps/sub"
            temporary = root / "evaluation"
            initialize_repository(repository)
            initialize_repository(submodule)
            (submodule / ".gitignore").write_text("ignored.bin\n", encoding="utf-8")
            (submodule / "tracked.txt").write_text("committed\n", encoding="utf-8")
            executable = submodule / "tool.sh"
            executable.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            executable.chmod(0o755)
            os.symlink("tracked.txt", submodule / "tracked-link")
            submodule_revision = commit_all(submodule, "Committed submodule tree")
            revision = add_gitlink(repository, "deps/sub", submodule_revision)

            (submodule / "tracked.txt").write_text("dirty\n", encoding="utf-8")
            (submodule / "untracked.txt").write_text("untracked\n", encoding="utf-8")
            (submodule / "ignored.bin").write_bytes(b"ignored")
            temporary.mkdir()

            prepared = _prepare_checkout(revision, temporary, repository)
            captured = prepared.path / "deps/sub"

            self.assertEqual("committed\n", (captured / "tracked.txt").read_text())
            self.assertTrue((captured / "tracked-link").is_symlink())
            self.assertEqual("tracked.txt", os.readlink(captured / "tracked-link"))
            self.assertTrue((captured / "tool.sh").stat().st_mode & stat.S_IXUSR)
            self.assertFalse((captured / "untracked.txt").exists())
            self.assertFalse((captured / "ignored.bin").exists())
            self.assertEqual(
                ({"path": "deps/sub", "revision": submodule_revision},),
                prepared.submodules,
            )

    def test_local_git_environment_disables_implicit_network_reads(self) -> None:
        environment = _local_git_environment()

        self.assertEqual("", environment["GIT_ALLOW_PROTOCOL"])
        self.assertEqual("1", environment["GIT_NO_LAZY_FETCH"])
        self.assertEqual("1", environment["GIT_NO_REPLACE_OBJECTS"])
        self.assertEqual("0", environment["GIT_TERMINAL_PROMPT"])

    def test_network_capable_git_operations_are_rejected(self) -> None:
        for operation in (
            ("clone", "https://example.invalid/repository.git"),
            ("fetch", "origin"),
            ("-c", "protocol.file.allow=always", "fetch", "origin"),
            ("pull",),
            ("push", "origin"),
            ("remote", "update"),
            ("submodule", "update", "--init"),
        ):
            with self.subTest(operation=operation):
                with self.assertRaisesRegex(ValueError, "prohibited"):
                    _assert_offline_git_operation(operation)


class ProcessLifecycleTest(unittest.TestCase):
    def test_codex_process_timeout_terminates_process(self) -> None:
        with self.assertRaises(subprocess.TimeoutExpired):
            _run_codex_process(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                timeout_seconds=1,
                environment=os.environ.copy(),
            )

    def test_timeout_kills_descendants_even_after_group_leader_exits(self) -> None:
        script = (
            "import subprocess, sys; "
            "subprocess.Popen([sys.executable, '-c', "
            "'import time; time.sleep(30)']); sys.exit(0)"
        )
        started = time.monotonic()

        with self.assertRaises(subprocess.TimeoutExpired):
            _run_codex_process(
                [sys.executable, "-c", script],
                timeout_seconds=0.2,
                environment=os.environ.copy(),
            )

        self.assertLess(time.monotonic() - started, 2.0)

    @unittest.skipUnless(os.name == "posix", "process-group test requires POSIX")
    def test_successful_leader_cannot_leave_a_background_descendant(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / "survived"
            child = (
                "import pathlib, signal, time; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "time.sleep(0.4); "
                f"pathlib.Path({str(marker)!r}).write_text('survived')"
            )
            leader = (
                "import subprocess, sys; "
                "subprocess.Popen([sys.executable, '-c', "
                f"{child!r}], stdin=subprocess.DEVNULL, "
                "stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)"
            )

            result = _run_codex_process(
                [sys.executable, "-c", leader],
                timeout_seconds=2,
                environment=os.environ.copy(),
            )
            time.sleep(0.6)

            self.assertEqual(0, result.returncode)
            self.assertFalse(marker.exists())

    @unittest.skipUnless(
        sys.platform == "linux" and Path("/usr/bin/bwrap").is_file(),
        "PID-namespace regression requires bubblewrap",
    )
    def test_pid_namespace_kills_setsid_descendant_on_timeout(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / "survived"
            child = (
                "import pathlib, signal, time; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "time.sleep(0.6); "
                f"pathlib.Path({str(marker)!r}).write_text('survived')"
            )
            leader = (
                "import subprocess, sys, time; "
                "subprocess.Popen([sys.executable, '-c', "
                f"{child!r}], start_new_session=True, stdin=subprocess.DEVNULL, "
                "stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL); "
                "time.sleep(30)"
            )
            wrapped = _containment_command(
                [sys.executable, "-c", leader], evaluation_runtime(), (root,)
            )

            with self.assertRaises(subprocess.TimeoutExpired):
                _run_codex_process(
                    wrapped,
                    timeout_seconds=0.2,
                    environment=os.environ.copy(),
                )
            time.sleep(0.8)

            self.assertFalse(marker.exists())

    @unittest.skipUnless(
        sys.platform == "linux" and Path("/usr/bin/bwrap").is_file(),
        "PID-namespace regression requires bubblewrap",
    )
    def test_pid_namespace_kills_setsid_descendant_after_success(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / "survived"
            child = (
                "import pathlib, time; time.sleep(0.4); "
                f"pathlib.Path({str(marker)!r}).write_text('survived')"
            )
            leader = (
                "import subprocess, sys; "
                "subprocess.Popen([sys.executable, '-c', "
                f"{child!r}], start_new_session=True, stdin=subprocess.DEVNULL, "
                "stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)"
            )
            wrapped = _containment_command(
                [sys.executable, "-c", leader], evaluation_runtime(), (root,)
            )

            result = _run_codex_process(
                wrapped,
                timeout_seconds=2,
                environment=os.environ.copy(),
            )
            time.sleep(0.6)

            self.assertEqual(0, result.returncode)
            self.assertFalse(marker.exists())


class CaseExecutionTest(unittest.TestCase):
    def prepared_checkout(self, _revision: str, temporary: Path) -> PreparedCheckout:
        checkout = temporary / "checkout"
        home = temporary / "home"
        checkout.mkdir()
        home.mkdir(exist_ok=True)
        (checkout / "AGENTS.md").write_text("contract\n")
        manifest = _checkout_manifest(checkout)
        return PreparedCheckout(checkout, "b" * 40, (), manifest)

    @patch("tools.agent_eval.agent_eval._checkout_changes", return_value=[])
    @patch("tools.agent_eval.agent_eval._run_codex_process")
    @patch("tools.agent_eval.agent_eval._prepare_checkout")
    def test_run_case_retains_evidence_and_requires_review(
        self, prepare_mock, process_mock, _changes_mock
    ) -> None:
        prepare_mock.side_effect = self.prepared_checkout
        response = structured_response()

        def complete(command, _timeout_seconds, _environment):
            response_path = Path(command[command.index("--output-last-message") + 1])
            response_path.write_text(json.dumps(response), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0, "", "")

        process_mock.side_effect = complete
        result = _run_case(
            evaluation_case(),
            model="gpt-5.6-sol",
            effort="medium",
            revision="a" * 40,
            timeout_seconds=30,
            response_schema=DEFAULT_RESPONSE_SCHEMA,
            codex_home=Path("/tmp"),
            runtime=evaluation_runtime(),
        )

        self.assertEqual("completed", result["status"])
        self.assertTrue(result["review_ready"])
        self.assertTrue(result["review_required"])
        self.assertEqual(response, result["response"])
        self.assertFalse(result["checkout_snapshot"]["evaluator_material_present"])
        self.assertNotIn("passed", result)

    @patch(
        "tools.agent_eval.agent_eval._checkout_changes",
        return_value=["created: build/ignored-output.bin"],
    )
    @patch("tools.agent_eval.agent_eval._run_codex_process")
    @patch("tools.agent_eval.agent_eval._prepare_checkout")
    def test_malformed_response_becomes_a_per_case_error(
        self, prepare_mock, process_mock, _changes_mock
    ) -> None:
        prepare_mock.side_effect = self.prepared_checkout

        def complete(command, _timeout_seconds, _environment):
            response_path = Path(command[command.index("--output-last-message") + 1])
            response_path.write_text(
                json.dumps(structured_response(hardware_requirement=[])),
                encoding="utf-8",
            )
            return subprocess.CompletedProcess(command, 0, "", "")

        process_mock.side_effect = complete
        result = _run_case(
            evaluation_case(),
            model="gpt-5.6-sol",
            effort="medium",
            revision="a" * 40,
            timeout_seconds=30,
            response_schema=DEFAULT_RESPONSE_SCHEMA,
            codex_home=Path("/tmp"),
            runtime=evaluation_runtime(),
        )

        self.assertEqual("error", result["status"])
        self.assertFalse(result["review_ready"])
        self.assertIn("hardware_requirement", result["error"])
        self.assertEqual(
            ["created: build/ignored-output.bin"], result["checkout_changes"]
        )
        self.assertTrue(result["read_only_violation"])

    @patch(
        "tools.agent_eval.agent_eval._git",
        return_value=" M tools/agent_eval/cases.json",
    )
    def test_run_rejects_every_dirty_checkout_without_override(self, _git_mock) -> None:
        args = Namespace(timeout=30)
        with self.assertRaisesRegex(ValueError, "working tree is dirty"):
            run_evaluations(args)

    def test_parser_has_no_dirty_or_failure_override(self) -> None:
        help_text = build_parser().format_help()
        run_parser = next(
            action
            for action in build_parser()._actions
            if getattr(action, "choices", None)
        ).choices["run"]
        run_help = run_parser.format_help()

        self.assertNotIn("--allow-dirty", help_text + run_help)
        self.assertNotIn("--allow-failures", help_text + run_help)
        self.assertNotIn("--allow-write-cases", help_text + run_help)


class ReportTest(unittest.TestCase):
    def test_report_marks_coverage_as_requiring_human_review(self) -> None:
        document = {
            "schema_version": 3,
            "run_id": "baseline",
            "revision": "abc123",
            "model": "gpt-5.6-sol",
            "reasoning_effort": "medium",
            "execution": {
                "containment": "bubblewrap-test",
                "containment_preflight": {
                    "passed": True,
                    "modes": ["read-only"],
                },
                "workspace_access": "read-only",
                "hardware_execution": "prohibited",
                "auth": "isolated-file-copy",
                "timeout_seconds": 600,
            },
            "summary": {
                "total": 1,
                "completed": 1,
                "review_ready": 1,
                "errors": 0,
                "criteria_covered": 1,
                "criteria_possible": 1,
            },
            "results": [
                {
                    "id": "case-one",
                    "status": "completed",
                    "review_ready": True,
                    "duration_seconds": 2.5,
                    "hardware_requirement": "not_required",
                    "usage": {"input_tokens": 100, "output_tokens": 20},
                    "response": {
                        "summary": "The repository contract is authoritative.",
                        "files": ["AGENTS.md"],
                        "claims": ["The change requires human review."],
                        "invariants": ["AGENTS.md remains authoritative."],
                        "verification": ["Review AGENTS.md."],
                    },
                    "contract_checks": [],
                    "coverage": {
                        "covered": 1,
                        "possible": 1,
                        "criteria": [
                            {
                                "id": "repository-evidence",
                                "description": "Supply repository evidence.",
                                "covered": True,
                                "evidence": ["AGENTS.md defines the contract."],
                                "files": ["AGENTS.md"],
                                "coverage_issues": [],
                            }
                        ],
                    },
                }
            ],
        }

        report = render_report(document)

        self.assertIn("Structurally review-ready: 1 / 1", report)
        self.assertIn("review required", report)
        self.assertIn("not a correctness score or approval", report)
        self.assertIn("### Agent claims", report)
        self.assertIn("### Agent-reported files", report)
        self.assertIn("`AGENTS.md`", report)
        self.assertIn("The change requires human review.", report)
        self.assertIn("### Agent invariants", report)
        self.assertIn("AGENTS.md remains authoritative.", report)
        self.assertIn("### Agent verification plan", report)
        self.assertIn("Review AGENTS.md.", report)
        self.assertIn("`input_tokens`: 100", report)
        self.assertIn("Containment: `bubblewrap-test`", report)
        self.assertIn('"modes": ["read-only"]', report)
        self.assertIn("Workspace access: `read-only`", report)
        self.assertIn("Hardware execution: `prohibited`", report)
        self.assertIn("Authentication isolation: `isolated-file-copy`", report)
        self.assertIn("Case timeout: 600 seconds", report)
        self.assertNotIn("Passed cases", report)
        self.assertNotIn("Criteria score", report)

    def test_report_retains_failed_case_write_audit(self) -> None:
        document = {
            "schema_version": 3,
            "run_id": "failure-audit",
            "revision": "abc123",
            "model": "gpt-5.6-sol",
            "reasoning_effort": "medium",
            "summary": {
                "total": 1,
                "completed": 0,
                "review_ready": 0,
                "errors": 1,
                "criteria_covered": 0,
                "criteria_possible": 0,
            },
            "results": [
                {
                    "id": "failed-case",
                    "status": "error",
                    "review_ready": False,
                    "error": "malformed response",
                    "read_only_violation": True,
                    "checkout_changes": ["created: build/ignored.bin"],
                }
            ],
        }

        report = render_report(document)

        self.assertIn("malformed response", report)
        self.assertIn("Read-only checkout violation: detected", report)
        self.assertIn("created: build/ignored.bin", report)


if __name__ == "__main__":
    unittest.main()
