import importlib.util
import os
import tempfile
import unittest
from pathlib import Path

PATH = Path(__file__).parent / "qemu_link" / "verify.py"
SPEC = importlib.util.spec_from_file_location("qemu_link_verify", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class QemuLinkVerificationTest(unittest.TestCase):
    def test_patch_hunk_counts_include_type_registration(self):
        patch = MODULE.PATCH.read_text()
        self.assertTrue(MODULE.unified_diff_hunks_are_well_formed(patch))
        malformed = patch.replace("@@ -0,0 +1,623 @@", "@@ -0,0 +1,622 @@", 1)
        self.assertFalse(MODULE.unified_diff_hunks_are_well_formed(malformed))

    def test_complete_manifest_cross_check_and_patch_audit(self):
        report = MODULE.verify(None, None, None)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["prohibited_paths"], [])
        self.assertEqual(report["physical_source_closure"], "denied")

    @unittest.skipUnless(
        os.environ.get("DOMES_QEMU_TEST_BINARY"),
        "set DOMES_QEMU_TEST_BINARY to exercise the compiled patch",
    )
    def test_compiled_qemu_device_rejections(self):
        binary = Path(os.environ["DOMES_QEMU_TEST_BINARY"])
        abi = MODULE.json.loads(MODULE.ABI.read_text())
        cases = MODULE.run_qtest_rejections(binary, abi)
        self.assertTrue(all(cases.values()), cases)
        actor_cases = MODULE.run_qtest_functional_actor(binary, abi)
        self.assertTrue(all(actor_cases.values()), actor_cases)

    def test_runtime_validator_rejects_hard_coded_result_without_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "runtime.log"
            log.write_text(
                "DOMES_QEMU_LINK_RESULT schema=2 status=PASS failure_mask=0x00000000 "
                "token=1 service_dispatches=1 trace_drops=0 trace_discontinuities=0\n"
            )
            report = MODULE.validate_runtime_log(log)
            self.assertEqual(report["status"], "FAIL")
            self.assertFalse(all(report["stages"].values()))


if __name__ == "__main__":
    unittest.main()
