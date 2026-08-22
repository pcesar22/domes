import importlib.util
import unittest
from pathlib import Path

PATH = Path(__file__).parent / "qemu_link" / "verify.py"
SPEC = importlib.util.spec_from_file_location("qemu_link_verify", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class QemuLinkModelTest(unittest.TestCase):
    def test_valid_submission_and_consumption(self):
        model = MODULE.LinkModel()
        model.length(250)
        model.token(0x1234)
        model.submit()
        model.complete()
        self.assertEqual(model.sticky, 0)
        self.assertTrue(model.rx_ready)
        model.consume()
        self.assertFalse(model.rx_ready)

    def test_unknown_version_fails_closed(self):
        model = MODULE.LinkModel()
        model.version(2)
        model.length(1)
        model.token(1)
        model.submit()
        self.assertTrue(model.sticky & model.ST_UNKNOWN_VERSION)
        self.assertNotEqual(model.tx_status, model.TX_PENDING)

    def test_invalid_access_overlength_sequence_overflow_and_overwrite(self):
        model = MODULE.LinkModel()
        model.access(1, 4)
        self.assertTrue(model.sticky & 2)
        model = MODULE.LinkModel()
        model.length(251)
        self.assertTrue(model.sticky & 32)
        model = MODULE.LinkModel()
        model.submit()
        self.assertTrue(model.sticky & 16)
        model = MODULE.LinkModel()
        model.length(1)
        model.token(1)
        model.submit()
        model.submit()
        self.assertTrue(model.sticky & 64)
        model = MODULE.LinkModel()
        model.length(1)
        model.token(1)
        model.submit()
        model.rx_ready = True
        model.complete()
        self.assertTrue(model.sticky & 1)


if __name__ == "__main__":
    unittest.main()
