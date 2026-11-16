"""Tests for room_objects.py (which moving room objects their own logic drives). Synthetic data only.

    python tools/puppet_pack/tests/test_room_objects.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from puppet_pack import room_objects as ro  # noqa: E402


def node(**inputs):
    """A logic node whose bool inputs are name -> source node (-1: a constant)."""
    empty = {t: [] for t in ("int", "bool", "float", "string", "vec3", "udt")}
    n = {"type": "ActorLogicX", "imm": {}, "in": dict(empty), "out": dict(empty)}
    n["in"]["bool"] = [(name, src, 0) for name, src in inputs.items()]
    return n


class RoomObjectsTest(unittest.TestCase):
    def test_platform_names_as_coop_platforms(self):
        self.assertTrue(ro.is_driver("GroundControlActor_WaveLoop"))
        self.assertTrue(ro.is_driver("GrouondControlActor_DiffentSpeedMoveTo"))
        self.assertTrue(ro.is_driver("Obj_InsideTheDekuTree_ElevatorLift_A_01"))
        self.assertTrue(ro.is_driver("Obj_ForestTemple_Elevator_A_01"))
        self.assertFalse(ro.is_driver("Npc_ElevatorLift_A_01"))
        self.assertFalse(ro.is_driver("Obj_MovableBox_A_01"))

    def test_wired_inputs_leave_out_constants_and_resets(self):
        n = node(IsOn=4, Logic_Create=2, RespawnTrigger=3, ForceWarp=5, IisForceWarp=6, Move=-1)
        self.assertEqual(ro.wired_inputs(n), {"IsOn"})
        self.assertEqual(ro.wired_inputs(node(IsInPlayer=-100)), {"IsInPlayer"})  # the multi-input list
        self.assertEqual(ro.wired_inputs(node(Move=-1)), set())


if __name__ == "__main__":
    unittest.main()
