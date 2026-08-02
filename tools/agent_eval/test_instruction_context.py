import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAX_ROOT_BYTES = 8 * 1024
MAX_CHAIN_BYTES = 20 * 1024


def instruction_chain(relative: str) -> list[Path]:
    target = ROOT / relative
    directory = target if target.is_dir() else target.parent
    chain = [ROOT / "AGENTS.md"]
    current = ROOT
    for part in directory.relative_to(ROOT).parts:
        current /= part
        candidate = current / "AGENTS.md"
        if candidate.exists():
            chain.append(candidate)
    return chain


class InstructionContextTest(unittest.TestCase):
    def test_root_and_scoped_chains_stay_bounded(self) -> None:
        root = ROOT / "AGENTS.md"
        self.assertLessEqual(root.stat().st_size, MAX_ROOT_BYTES)

        for relative in (
            "firmware/domes/main",
            "tools/domes-cli/src",
            "ios/domes_app/lib",
            "hardware",
        ):
            chain = instruction_chain(relative)
            size = sum(path.stat().st_size for path in chain)
            self.assertLessEqual(size, MAX_CHAIN_BYTES, relative)

    def test_representative_directories_resolve_expected_instructions(self) -> None:
        expected = {
            "firmware/domes/main": ["AGENTS.md", "firmware/AGENTS.md"],
            "tools/domes-cli/src": ["AGENTS.md", "tools/domes-cli/AGENTS.md"],
            "ios/domes_app/lib": ["AGENTS.md", "ios/domes_app/AGENTS.md"],
            "hardware": ["AGENTS.md", "hardware/AGENTS.md"],
        }
        for relative, paths in expected.items():
            actual = [
                str(path.relative_to(ROOT)) for path in instruction_chain(relative)
            ]
            self.assertEqual(paths, actual)

    def test_progressive_disclosure_targets_exist(self) -> None:
        targets = (
            "firmware/AGENTS.md",
            "tools/domes-cli/AGENTS.md",
            "ios/domes_app/AGENTS.md",
            "hardware/AGENTS.md",
            "docs/TESTING.md",
            ".codex/PLATFORM.md",
            ".codex/skills/domes-esp32-firmware/SKILL.md",
            ".codex/skills/domes-esp32-firmware/references/runbooks.md",
            ".codex/skills/domes-debug-esp32/SKILL.md",
            ".codex/skills/domes-github-workflow/SKILL.md",
        )
        missing = [relative for relative in targets if not (ROOT / relative).exists()]
        self.assertEqual([], missing)

    def test_operational_runbooks_are_not_duplicated_in_instruction_files(self) -> None:
        root = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
        firmware = (ROOT / "firmware/AGENTS.md").read_text(encoding="utf-8")
        for heading in ("## OTA Updates", "## Tracing", "## Multi-Device Testing"):
            self.assertNotIn(heading, root)
            self.assertNotIn(heading, firmware)


if __name__ == "__main__":
    unittest.main()
