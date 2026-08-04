import json
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CODEX_DIR = ROOT / ".codex"


class CodexConfigTest(unittest.TestCase):
    def test_primary_defaults_and_bounded_concurrency(self) -> None:
        with (CODEX_DIR / "config.toml").open("rb") as stream:
            config = tomllib.load(stream)

        self.assertEqual("gpt-5.6-sol", config["model"])
        self.assertEqual("medium", config["model_reasoning_effort"])
        self.assertTrue(config["agents"]["enabled"])
        self.assertEqual(2, config["agents"]["max_concurrent_threads_per_session"])
        self.assertEqual("gpt-5.6-terra", config["agents"]["default_subagent_model"])
        self.assertEqual(
            "medium", config["agents"]["default_subagent_reasoning_effort"]
        )
        self.assertNotIn("approval_policy", config)
        self.assertNotIn("sandbox_mode", config)
        self.assertNotIn("network_access", config)

    def test_specialists_are_narrow_read_only_roles(self) -> None:
        expected = {
            "firmware_reviewer": ("gpt-5.6-sol", "high"),
            "protocol_reviewer": ("gpt-5.6-sol", "high"),
            "repo_explorer": ("gpt-5.6-terra", "medium"),
            "test_triage": ("gpt-5.6-terra", "medium"),
            "hardware_verifier": ("gpt-5.6-sol", "high"),
        }

        for name, (model, effort) in expected.items():
            with self.subTest(name=name):
                with (CODEX_DIR / "agents" / f"{name}.toml").open("rb") as stream:
                    agent = tomllib.load(stream)
                self.assertEqual(name, agent["name"])
                self.assertEqual(model, agent["model"])
                self.assertEqual(effort, agent["model_reasoning_effort"])
                self.assertEqual("read-only", agent["sandbox_mode"])
                self.assertTrue(agent["description"].strip())
                self.assertTrue(agent["developer_instructions"].strip())
                self.assertNotIn("approval_policy", agent)
                self.assertNotIn("network_access", agent)

    def test_agent_eval_has_representative_cases_for_every_role(self) -> None:
        with (ROOT / "tools" / "agent_eval" / "cases.json").open(
            encoding="utf-8"
        ) as stream:
            case_ids = {case["id"] for case in json.load(stream)["cases"]}

        self.assertIn("autonomous-continuation-selection", case_ids)

        coverage = {
            "firmware_reviewer": {"freertos-isr-review", "firmware-patch-review"},
            "protocol_reviewer": {
                "protobuf-cross-language-change",
                "frame-envelope-review",
            },
            "repo_explorer": {"documentation-authority", "scoped-change-review"},
            "test_triage": {"release-verification-map"},
            "hardware_verifier": {
                "ota-rollback-claims",
                "multi-device-port-routing",
            },
        }

        for name, representative_cases in coverage.items():
            with self.subTest(name=name):
                self.assertTrue(representative_cases <= case_ids)


if __name__ == "__main__":
    unittest.main()
