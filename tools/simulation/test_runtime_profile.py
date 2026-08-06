from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("generate_runtime_profile.py")
SPEC = importlib.util.spec_from_file_location("generate_runtime_profile", MODULE_PATH)
assert SPEC and SPEC.loader
profile = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profile)

REPO_ROOT = Path(__file__).resolve().parents[2]
PROFILE_SPEC = REPO_ROOT / "firmware" / "domes" / "profiles" / "runtime_profiles.json"


class RuntimeProfileTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.sdkconfig = self.root / "sdkconfig"
        self.sdkconfig.write_text(
            "\n".join(
                (
                    'CONFIG_IDF_TARGET="esp32s3"',
                    "CONFIG_DOMES_RUNTIME_PROFILE_QEMU=y",
                    "CONFIG_APP_REPRODUCIBLE_BUILD=y",
                    "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                    "CONFIG_ESP_CONSOLE_UART_DEFAULT=y",
                    "CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096",
                    "CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y",
                    "CONFIG_FREERTOS_HZ=1000",
                    "# CONFIG_FREERTOS_UNICORE is not set",
                    "# CONFIG_BT_ENABLED is not set",
                    "# CONFIG_DOMES_OTA_AUTO_CHECK is not set",
                    "# CONFIG_DOMES_WIFI_AUTO_CONNECT is not set",
                    "# CONFIG_ESP_COEX_SW_COEXIST_ENABLE is not set",
                    "# CONFIG_ESP_TASK_WDT_EN is not set",
                    "# CONFIG_SPIRAM is not set",
                    "# CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH is not set",
                    "",
                )
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def _mutated_spec(self, mutate) -> Path:
        value = json.loads(PROFILE_SPEC.read_text(encoding="utf-8"))
        mutate(value)
        path = self.root / "runtime_profiles.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def _write_physical_sdkconfig(
        self, *, include_ota: bool = True, wifi_enabled: bool = True
    ) -> None:
        lines = [
            'CONFIG_IDF_TARGET="esp32s3"',
            "CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL=y",
            "# CONFIG_DOMES_RUNTIME_PROFILE_QEMU is not set",
            (
                "CONFIG_DOMES_WIFI_AUTO_CONNECT=y"
                if wifi_enabled
                else "# CONFIG_DOMES_WIFI_AUTO_CONNECT is not set"
            ),
        ]
        if include_ota:
            lines.append("CONFIG_DOMES_OTA_AUTO_CHECK=y")
        lines.extend(
            (
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y",
                "CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096",
                "CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0=y",
                "CONFIG_FREERTOS_HZ=1000",
                "CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y",
                "CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y",
                "# CONFIG_FREERTOS_UNICORE is not set",
                "",
            )
        )
        self.sdkconfig.write_text("\n".join(lines), encoding="utf-8")

    def test_qemu_profile_is_complete_and_stable(self) -> None:
        first = profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)
        second = profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)
        self.assertEqual(first["manifest_bytes"], second["manifest_bytes"])
        self.assertEqual(first["manifest_sha256"], second["manifest_sha256"])
        self.assertEqual(first["supported_mask"], 0xE2)
        self.assertEqual(first["ready_mask"], 0x02)
        self.assertEqual(len(first["required_tasks"]), 9)
        self.assertEqual(len(first["absent_tasks"]), 6)
        self.assertEqual(first["required_tasks"][0]["state"], "synthetic-load")
        self.assertEqual(
            first["required_tasks"][0]["contract"], "qemu.service_ready_workload"
        )
        self.assertTrue(first["required_tasks"][0]["timing"])
        self.assertEqual(first["readiness_scenario"]["dwell_ms"], 350)
        self.assertEqual(
            sum(task["evidence_mask"] for task in first["required_tasks"]), 0x307F
        )
        for component in first["manifest"]["components"]:
            self.assertTrue(component["implementation"])
            self.assertTrue(component["limitations"])

    def test_missing_component_classification_fails(self) -> None:
        path = self._mutated_spec(
            lambda value: value["profiles"]["qemu"]["components"].pop("vendor.wifi")
        )
        with self.assertRaisesRegex(
            profile.ProfileError, "classify every catalog component"
        ):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_unknown_profile_key_fails(self) -> None:
        path = self._mutated_spec(
            lambda value: value["profiles"]["qemu"].update({"comment": "not schema"})
        )
        with self.assertRaisesRegex(profile.ProfileError, r"unknown=\['comment'\]"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_duplicate_json_key_fails_before_schema_validation(self) -> None:
        path = self.root / "runtime_profiles.json"
        path.write_text(
            '{"schema_version":1,"profiles":{"qemu":{},"qemu":{}}}',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(profile.ProfileError, "duplicate JSON key 'qemu'"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_duplicate_task_name_fails(self) -> None:
        def mutate(value) -> None:
            value["task_catalog"][1]["name"] = value["task_catalog"][0]["name"]

        path = self._mutated_spec(mutate)
        with self.assertRaisesRegex(profile.ProfileError, "duplicate task names"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_duplicate_task_trace_id_fails(self) -> None:
        def mutate(value) -> None:
            value["task_catalog"][1]["trace_id"] = value["task_catalog"][0]["trace_id"]

        path = self._mutated_spec(mutate)
        with self.assertRaisesRegex(profile.ProfileError, "duplicate task trace IDs"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_task_trace_id_above_active_bitmap_fails(self) -> None:
        def mutate(value) -> None:
            value["task_catalog"][0]["trace_id"] = 32

        path = self._mutated_spec(mutate)
        with self.assertRaisesRegex(
            profile.ProfileError, r"trace_id must be in \[1, 31\]"
        ):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_prohibited_qemu_vendor_config_fails(self) -> None:
        self.sdkconfig.write_text(self.sdkconfig.read_text() + "CONFIG_BT_ENABLED=y\n")
        with self.assertRaisesRegex(
            profile.ProfileError, "duplicate CONFIG_BT_ENABLED"
        ):
            profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)

    def test_enabled_then_disabled_sdkconfig_duplicate_fails(self) -> None:
        self.sdkconfig.write_text(
            self.sdkconfig.read_text()
            + "# CONFIG_DOMES_RUNTIME_PROFILE_QEMU is not set\n"
        )
        with self.assertRaisesRegex(
            profile.ProfileError, "duplicate CONFIG_DOMES_RUNTIME_PROFILE_QEMU"
        ):
            profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)

    def test_disabled_task_cannot_claim_presence(self) -> None:
        def mutate(value) -> None:
            value["profiles"]["qemu"]["tasks"]["esp_now_service"][
                "presence"
            ] = "required"

        path = self._mutated_spec(mutate)
        with self.assertRaisesRegex(
            profile.ProfileError, "disabled exactly when absent"
        ):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_unknown_fidelity_contract_fails(self) -> None:
        path = self._mutated_spec(
            lambda value: value["profiles"]["qemu"]["component_contracts"].update(
                {"peripheral.touch": "missing.contract"}
            )
        )
        with self.assertRaisesRegex(profile.ProfileError, "unknown fidelity contract"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_missing_task_state_contract_fails(self) -> None:
        path = self._mutated_spec(
            lambda value: value["profiles"]["qemu"]["task_state_contracts"].pop(
                "synthetic-load"
            )
        )
        with self.assertRaisesRegex(profile.ProfileError, "unknown fidelity contract"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_readiness_scenario_must_be_bounded(self) -> None:
        path = self._mutated_spec(
            lambda value: value["profiles"]["qemu"]["readiness_scenario"].update(
                {"touch_release_ms": 350}
            )
        )
        with self.assertRaisesRegex(profile.ProfileError, "inside the dwell"):
            profile.resolve_profile(path, "qemu", self.sdkconfig)

    def test_main_stack_is_bound_to_sdkconfig(self) -> None:
        self.sdkconfig.write_text(
            self.sdkconfig.read_text().replace(
                "CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096",
                "CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584",
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(profile.ProfileError, "ESP_MAIN_TASK_STACK_SIZE"):
            profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)

    def test_missing_required_false_sdkconfig_key_fails(self) -> None:
        self.sdkconfig.write_text(
            self.sdkconfig.read_text().replace(
                "# CONFIG_FREERTOS_UNICORE is not set\n", ""
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            profile.ProfileError, "CONFIG_FREERTOS_UNICORE must be defined"
        ):
            profile.resolve_profile(PROFILE_SPEC, "qemu", self.sdkconfig)

    def test_removed_project_option_fails_resolved_profile(self) -> None:
        self._write_physical_sdkconfig(include_ota=False)
        with self.assertRaisesRegex(
            profile.ProfileError, "CONFIG_DOMES_OTA_AUTO_CHECK must be defined"
        ):
            profile.resolve_profile(PROFILE_SPEC, "physical", self.sdkconfig)

    def test_physical_profile_resolves_wifi_and_ota_options_enabled(self) -> None:
        self._write_physical_sdkconfig()
        resolved = profile.resolve_profile(PROFILE_SPEC, "physical", self.sdkconfig)
        self.assertEqual(resolved["supported_mask"], 0xFE)
        self.assertEqual(resolved["ready_mask"], 0x0E)

    def test_physical_profile_resolves_wifi_disabled_ready_mask(self) -> None:
        self._write_physical_sdkconfig(wifi_enabled=False)
        resolved = profile.resolve_profile(PROFILE_SPEC, "physical", self.sdkconfig)
        self.assertEqual(resolved["supported_mask"], 0xF6)
        self.assertEqual(resolved["ready_mask"], 0x06)


if __name__ == "__main__":
    unittest.main()
