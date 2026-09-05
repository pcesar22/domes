import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import control
import operator_config


class OperatorConfigTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "operator.json"
        self.value = {
            "schema_version": 1,
            "scheduler_host": "domes-runner",
            "registered_cp2102n_serials": ["testboard00000001", "testboard00000002"],
        }
        self.write()
        self.environment = mock.patch.dict(
            os.environ, {"DOMES_OPERATOR_CONFIG": str(self.path)}
        )
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def write(self):
        self.path.write_text(json.dumps(self.value))
        self.path.chmod(0o600)

    def test_private_config_and_matching_host_are_accepted(self):
        self.assertEqual(self.value, operator_config.load_operator_config())
        with mock.patch.object(
            control.socket, "gethostname", return_value="domes-runner"
        ):
            control.enforce_scheduler_host(control.load_workflow())

    def test_missing_config_fails_before_any_device_probe(self):
        self.path.unlink()
        with mock.patch.object(control.subprocess, "run") as run:
            with self.assertRaises(control.ControlError):
                control.registered_hardware_preflight()
            run.assert_not_called()

    def test_shared_config_is_rejected(self):
        self.path.chmod(0o644)
        with self.assertRaises(operator_config.OperatorConfigError):
            operator_config.load_operator_config()

    def test_duplicate_boards_are_rejected(self):
        self.value["registered_cp2102n_serials"] = ["testboard00000001"] * 2
        self.write()
        with self.assertRaises(operator_config.OperatorConfigError):
            operator_config.load_operator_config()

    def test_symlink_config_is_rejected(self):
        link = self.path.with_name("link.json")
        link.symlink_to(self.path)
        with mock.patch.dict(os.environ, {"DOMES_OPERATOR_CONFIG": str(link)}):
            with self.assertRaises(operator_config.OperatorConfigError):
                operator_config.load_operator_config()

    def test_software_only_config_cannot_authorize_hardware(self):
        self.value["registered_cp2102n_serials"] = []
        self.write()
        with mock.patch.object(control.subprocess, "run") as run:
            with self.assertRaises(control.ControlError):
                control.registered_hardware_preflight()
            run.assert_not_called()
