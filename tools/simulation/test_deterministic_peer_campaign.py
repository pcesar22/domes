import importlib.util
import json
import sys
import unittest
from pathlib import Path

PATH = Path(__file__).with_name("deterministic_peer_campaign.py")
SPEC = importlib.util.spec_from_file_location("deterministic_peer_campaign", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def delivery(sequence=0, payload="0102000000000201000000"):
    return (
        "DOMES_PEER_DELIVERY schema=1 deadline_ns=1000 class=1 source=2 "
        f"destination=1 sequence={sequence} correlation=7 payload={payload}\n"
    )


def trace(index=0, timestamp=100):
    return (
        f"DOMES_QEMU_LINK_TRACE schema=1 index={index} timestamp={timestamp} "
        "task=2 type=28 arg1=7 token=9\n"
    )


class CampaignContractTest(unittest.TestCase):
    def test_role_rotation_resolves_complementary_production_actors(self):
        matrix = []
        for role in MODULE.ROLES:
            resolved = MODULE.resolve_scenario(MODULE.scenario(role))
            matrix.append((role, resolved.actors[0].role, MODULE.PRODUCTION_CODEC))
        self.assertEqual(
            matrix,
            [
                ("master", "slave", "peer-drill-legacy-v1"),
                ("slave", "master", "peer-drill-legacy-v1"),
            ],
        )

    def test_delivery_and_trace_records_are_canonical_and_fail_closed(self):
        records = MODULE.parse_delivery_records(delivery())
        self.assertEqual(records[0]["payload_hex"], "0102000000000201000000")
        normalized = MODULE.normalized_trace(trace(0, 100) + trace(1, 125))
        self.assertTrue(all("timestamp" not in record for record in normalized))
        with self.assertRaisesRegex(MODULE.CampaignFailure, "no functional-peer"):
            MODULE.parse_delivery_records("")
        with self.assertRaisesRegex(MODULE.CampaignFailure, "non-contiguous"):
            MODULE.normalized_trace(trace(1, 100))

    def test_fresh_execution_and_identity_mismatch_rejections(self):
        run = {
            "delivery_records_sha256": "a" * 64,
            "trace_sha256": "b" * 64,
            "flash_sha256": "c" * 64,
        }
        MODULE.require_identical("master", [run, dict(run)])
        changed = dict(run, trace_sha256="d" * 64)
        with self.assertRaisesRegex(MODULE.CampaignFailure, "trace_sha256"):
            MODULE.require_identical("master", [run, changed])
        with self.assertRaisesRegex(MODULE.CampaignFailure, "at least two"):
            MODULE.require_identical("master", [run])

    def test_manifest_is_self_contained_json_data(self):
        raw = MODULE.scenario("master")
        self.assertEqual(json.loads(MODULE.canonical(raw)), raw)
        self.assertNotIn("path", raw)


if __name__ == "__main__":
    unittest.main()
