import copy
import dataclasses
import importlib.util
import json
import sys
import unittest
from pathlib import Path

PATH = Path(__file__).parent / "deterministic_peer_backplane.py"
SPEC = importlib.util.spec_from_file_location("deterministic_peer_backplane", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def scenario(role="master"):
    complement = "slave" if role == "master" else "master"
    expected = ["PING", "ARM_TOUCH"] if role == "master" else ["PING", "BEACON"]
    return {
        "schema_version": 1,
        "name": "role_rotation_v1",
        "model": "functional-peer-v1",
        "seed": 17,
        "dut_role": role,
        "dut_id": 1,
        "dut_mac": "02:00:00:00:00:01",
        "queue_capacity": 8,
        "termination_ns": 10_000_000,
        "actors": [
            {
                "pod_id": 2,
                "role": complement,
                "mac": "02:00:00:00:00:02",
                "peer_delay_ns": 1_000,
                "reaction_time_us": 500,
            }
        ],
        "expected_dut_types": expected,
    }


def wire(message_type, **values):
    return MODULE.WireMessage(
        message_type,
        b"\x02\x00\x00\x00\x00\x01",
        values.pop("timestamp_us", 1),
        **values,
    ).encode()


def identity(**changes):
    digest = "0" * 64
    values = {
        "schema_version": 1,
        "firmware_sha256": digest,
        "flash_sha256": digest,
        "toolchain_identity": "esp-idf-v5.4.4",
        "qemu_revision": "4f4148e2f68689eb8861bf9fce0b46ada9200fef",
        "qemu_patch_sha256": digest,
        "profile_sha256": digest,
        "fidelity_manifest_sha256": digest,
        "scenario_schema": 1,
        "scenario_model": "functional-peer-v1",
        "scenario_seed": 17,
        "resolved_scenario_sha256": digest,
        "icount_shift": 3,
        "vcpu_count": 1,
        "input_records_sha256": digest,
        "trace_sha256": digest,
        "assertions": ["production_codec", "single_dut"],
        "termination": "assertions_passed",
        "unconsumed_events": 0,
        "delivery_records_sha256": digest,
    }
    values.update(changes)
    return MODULE.ReplayIdentity.create(values)


class EventQueueTest(unittest.TestCase):
    def test_total_order_covers_equal_and_unequal_deadlines(self):
        queue = MODULE.BoundedEventQueue(8)
        keys = [
            MODULE.EventKey(20, 0, 0, 0, 0),
            MODULE.EventKey(10, 2, 1, 1, 2),
            MODULE.EventKey(10, 1, 2, 1, 1),
            MODULE.EventKey(10, 1, 1, 2, 1),
            MODULE.EventKey(10, 1, 1, 2, 0),
        ]
        for key in reversed(keys):
            queue.push(MODULE.Event(key, b"x"))
        self.assertEqual([queue.pop().key for _ in keys], sorted(keys))

    def test_capacity_duplicate_and_exhaustion_fail_closed(self):
        queue = MODULE.BoundedEventQueue(1)
        event = MODULE.Event(MODULE.EventKey(0, 0, 0, 0, 0), b"x")
        queue.push(event)
        distinct = MODULE.Event(MODULE.EventKey(0, 0, 0, 0, 1), b"y")
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "overflow"):
            queue.push(distinct)
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "duplicate"):
            queue.push(event)
        queue.pop()
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "exhausted"):
            queue.pop()


class ScenarioTest(unittest.TestCase):
    def test_resolution_is_canonical_and_input_order_independent(self):
        raw = scenario()
        raw["actors"].append(
            {
                "pod_id": 3,
                "role": "slave",
                "mac": "02:00:00:00:00:03",
                "peer_delay_ns": 2_000,
                "reaction_time_us": 700,
            }
        )
        reversed_raw = copy.deepcopy(raw)
        reversed_raw["actors"].reverse()
        self.assertEqual(
            MODULE.resolve_scenario(raw), MODULE.resolve_scenario(reversed_raw)
        )

    def test_schema_model_and_ambiguous_actor_fail_closed(self):
        for mutation in (
            lambda value: value.update(model="unknown"),
            lambda value: value.update(extra=True),
            lambda value: value["actors"][0].update(role="master"),
        ):
            raw = scenario()
            mutation(raw)
            with self.assertRaises(MODULE.BackplaneFailure):
                MODULE.resolve_scenario(raw)


class ProductionActorTest(unittest.TestCase):
    def test_qemu_device_embeds_model_in_virtual_clock_domain(self):
        patch = (
            Path(__file__).parent
            / "qemu_link"
            / "patches"
            / "0001-domes-link-device.patch"
        ).read_text()
        required = (
            "SCENARIO_FUNCTIONAL_PEER_V1",
            "actor_transition",
            "QEMU_CLOCK_VIRTUAL",
            "DOMES_LINK_EVENT_CAPACITY",
            "event_class_priority",
            'DEFINE_PROP_UINT32("scenario-model"',
            'DEFINE_PROP_UINT32("scenario-seed"',
            'DEFINE_PROP_UINT32("dut-role"',
            'DEFINE_PROP_UINT64("peer-delay-ns"',
            "DOMES_PEER_DELIVERY schema=1",
            "s->tx_status = TX_FAILURE",
        )
        self.assertTrue(all(token in patch for token in required))
        self.assertNotIn("QEMU_CLOCK_REALTIME", patch)
        self.assertNotIn("QEMU_CLOCK_HOST", patch)
        self.assertIn("if (!enqueue_event(s, event))", patch)

    def test_active_production_wire_variants_are_exchanged(self):
        actor = MODULE.FunctionalActor(MODULE.resolve_scenario(scenario()).actors[0])
        pong = MODULE.WireMessage.decode(
            actor.consume(wire(MODULE.MessageType.PING), 5_000)
        )
        self.assertEqual(pong.message_type, MODULE.MessageType.PONG)
        touch = MODULE.WireMessage.decode(
            actor.consume(
                wire(
                    MODULE.MessageType.ARM_TOUCH,
                    round_token=7,
                    timeout_ms=3_000,
                    feedback_mode=3,
                ),
                6_000,
            )
        )
        self.assertEqual(
            (touch.message_type, touch.round_token, touch.reaction_time_us),
            (MODULE.MessageType.TOUCH_EVENT, 7, 500),
        )

    def test_malformed_and_unexpected_traffic_fail_closed(self):
        actor = MODULE.FunctionalActor(MODULE.resolve_scenario(scenario()).actors[0])
        with self.assertRaises(MODULE.BackplaneFailure):
            actor.consume(b"\xff", 0)
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "unexpected"):
            actor.consume(wire(MODULE.MessageType.TIMEOUT_EVENT, round_token=1), 0)


class RoleRotationReplayTest(unittest.TestCase):
    def run_role(self, role):
        resolved = MODULE.resolve_scenario(scenario(role))
        backplane = MODULE.DeterministicPeerBackplane(resolved)
        backplane.submit_from_dut(2, wire(MODULE.MessageType.PING), 100)
        if role == "master":
            backplane.submit_from_dut(
                2,
                wire(
                    MODULE.MessageType.ARM_TOUCH,
                    round_token=9,
                    timeout_ms=3_000,
                    feedback_mode=3,
                ),
                200,
            )
        else:
            backplane.submit_from_dut(2, wire(MODULE.MessageType.BEACON), 200)
        while backplane.pop_due(2_000):
            pass
        return resolved, backplane.finish("assertions_passed")

    def test_each_important_role_rotates_through_exactly_one_dut(self):
        matrix = []
        for role in MODULE.IMPORTANT_ROLES:
            resolved, record = self.run_role(role)
            matrix.append(
                (
                    role,
                    len(resolved.actors),
                    MODULE.PRODUCTION_CODEC,
                    MODULE.sha256_bytes(record),
                )
            )
        self.assertEqual([row[0] for row in matrix], ["master", "slave"])
        self.assertTrue(
            all(row[1] == 1 and row[2] == "peer-drill-legacy-v1" for row in matrix)
        )

    def test_two_fresh_runs_are_byte_identical(self):
        _, first = self.run_role("master")
        _, second = self.run_role("master")
        self.assertEqual(first, second)
        self.assertEqual(MODULE.sha256_bytes(first), MODULE.sha256_bytes(second))

        trace = [
            {
                "timestamp_ns": 1_000,
                "event_id": 7,
                "task_id": 2,
                "core_id": 0,
                "correlation_token": 9,
            }
        ]
        self.assertEqual(
            MODULE.replay_normalized_trace_hash(trace),
            MODULE.replay_normalized_trace_hash(copy.deepcopy(trace)),
        )
        changed = copy.deepcopy(trace)
        changed[0]["correlation_token"] = 10
        self.assertNotEqual(
            MODULE.replay_normalized_trace_hash(trace),
            MODULE.replay_normalized_trace_hash(changed),
        )

    def test_unexpected_exhausted_model_failure_and_unconsumed_fail_closed(self):
        backplane = MODULE.DeterministicPeerBackplane(
            MODULE.resolve_scenario(scenario())
        )
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "unexpected DUT"):
            backplane.submit_from_dut(2, wire(MODULE.MessageType.BEACON), 0)
        incomplete = MODULE.DeterministicPeerBackplane(
            MODULE.resolve_scenario(scenario())
        )
        incomplete.submit_from_dut(2, wire(MODULE.MessageType.PING), 0)
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "unconsumed"):
            incomplete.finish("assertions_passed")
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "model_failure"):
            incomplete.finish("model_failure")

    def test_identity_or_input_change_is_rejected(self):
        baseline = identity()
        baseline.require_match(identity())
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "scenario_seed"):
            baseline.require_match(identity(scenario_seed=18))
        with self.assertRaisesRegex(MODULE.BackplaneFailure, "input_records_sha256"):
            baseline.require_match(identity(input_records_sha256="1" * 64))

    def test_virtual_time_audit_has_no_host_or_ordering_sources(self):
        audit = MODULE.audit_virtual_time_sources(PATH.read_text())
        self.assertTrue(all(audit.values()), audit)


if __name__ == "__main__":
    unittest.main()
