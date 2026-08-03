import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class PlansContractTest(unittest.TestCase):
    def test_root_guidance_routes_substantial_work_to_contract(self) -> None:
        agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")

        self.assertIn("PLANS.md", agents)

    def test_contract_contains_resume_and_evidence_boundaries(self) -> None:
        contract = (ROOT / "PLANS.md").read_text(encoding="utf-8").lower()

        required = {
            "objective and observable outcome",
            "authorities and contracts",
            "affected components and generated consumers",
            "stages and dependencies",
            "automated",
            "accepted command",
            "physical confirmation",
            "decisions, discoveries, and deviations",
            "resume checkpoint",
            "remaining unverified behavior",
            "representative example",
        }

        missing = {term for term in required if term not in contract}

        self.assertEqual(set(), missing)
        self.assertIn("unavailable is not passed", contract)
        self.assertIn("illustrative cross-component plan", contract)


if __name__ == "__main__":
    unittest.main()
