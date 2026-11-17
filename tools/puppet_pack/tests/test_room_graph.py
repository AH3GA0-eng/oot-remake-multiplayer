"""Tests for eye_switches.read_graph and switch_events on a tiny synthetic room (row 98(2)): an unwired UpdateOnce
input of a gate means the game reads that gate once, when the room loads, so a flag behind it is "flag-once": it
deletes our asker only at a load, never live, and is no guard. No game files.

    python tools/puppet_pack/tests/test_room_graph.py   (CI runs it through test_puppet_pack.py)
"""
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from puppet_pack import byml, eye_switches  # noqa: E402

TYPES = eye_switches.TYPES


def ainb(nodes, multi=()):
    """The bytes of an AI file as read_graph reads it. nodes: [{"type", "imm": {name: value}, "bool": [(name, src,
    out, value)], "string": [(name, default)], "out": [bool output names]}]; multi: [(node, output)]."""
    pool, at = bytearray(b"\0"), {"": 0}

    def s(t):
        if t not in at:
            at[t] = len(pool)
            pool.extend(t.encode() + b"\0")
        return at[t]

    head = 0x74
    bodies = head + 0x3C * len(nodes)
    body_size = 0x30 + 16 * len(TYPES)
    imm = bodies + body_size * len(nodes)
    str_imm = imm + 0x10
    n_imm = sum(len(n.get("imm", {})) for n in nodes)
    io = str_imm + 0x0C * n_imm
    lists = io + 4 * 12
    entries = {t: [] for t in TYPES}  # (input entries bytes, output entries bytes)
    outs = {t: [] for t in TYPES}
    node_bytes, body_bytes, imm_bytes = bytearray(), bytearray(), bytearray()
    k = 0
    for n in nodes:
        node = bytearray(0x3C)
        struct.pack_into("<II", node, 8, s(n.get("type", "")), 0)
        struct.pack_into("<I", node, 0x14, bodies + len(body_bytes))
        node_bytes += node
        body = bytearray(body_size)
        struct.pack_into("<II", body, 0x18, k, len(n.get("imm", {})))
        for name, value in n.get("imm", {}).items():
            imm_bytes += struct.pack("<III", s(name), 0, s(value))
            k += 1
        for t, name in enumerate(TYPES):
            ins = []
            if name == "bool":
                ins = [struct.pack("<IhhII", s(nm), src, o, 0, v) for nm, src, o, v in n.get("bool", [])]
            elif name == "string":
                ins = [struct.pack("<IhhII", s(nm), -1, 0, 0, s(d)) for nm, d in n.get("string", [])]
            o_names = [struct.pack("<I", s(x)) for x in (n.get("out", []) if name == "bool" else [])]
            struct.pack_into("<IIII", body, 0x30 + 16 * t, len(entries[name]), len(ins), len(outs[name]),
                             len(o_names))
            entries[name] += ins
            outs[name] += o_names
        body_bytes += body
    starts, o = [], lists
    blob = bytearray()
    for name in TYPES:
        starts.append(o)
        for e in entries[name]:
            blob += e
        o += sum(len(e) for e in entries[name])
        starts.append(o)
        for e in outs[name]:
            blob += e
        o += sum(len(e) for e in outs[name])
    multi_at = o
    for node, output in multi:
        blob += struct.pack("<HHI", node, output, 0)
    pool_at = multi_at + 8 * len(multi)
    out = bytearray(head)
    struct.pack_into("<II", out, 0x0C, 0, len(nodes))
    struct.pack_into("<I", out, 0x24, pool_at)
    struct.pack_into("<I", out, 0x2C, imm)
    struct.pack_into("<II", out, 0x34, io, multi_at)
    out += node_bytes + body_bytes
    out += struct.pack("<IIII", 0, 0, 0, str_imm) + imm_bytes
    out += struct.pack("<12I", *starts) + blob + pool
    return bytes(out)


def room(once):
    """An asker started by a step switch; deleted by Room_Flag through an AND whose UpdateOnce is `once`."""
    return [
        {"type": "ActorLogicEventRequestActor", "imm": {"InstanceName": "Asker"},
         "bool": [("EventRequestTrigger", 1, 1, 0), ("Logic_Delete", 3, 1, 0), ("Logic_Create", -1, 0, 1)],
         "out": ["Dead", "EventBool"]},
        {"type": "ActorLogicBootUpAndForceTrunOffControlActor", "imm": {"InstanceName": "Switch"},
         "out": ["OffState", "OnState"]},
        {"type": "HoldGameDataGetBoolSimple", "string": [("GameDataNameBool", "Room_Flag")], "out": ["Bool"]},
        {"type": "OperateAND", "bool": [("BoolMulti", 2, 0, 0), ("UpdateOnce", -1, 0, once)], "out": ["False", "True"]},
    ]


class _Plain:
    @staticmethod
    def decompress(raw):
        return raw


class ReadGraphTest(unittest.TestCase):
    def test_unwired_bool_inputs_keep_their_constant(self):
        nodes, _ = eye_switches.read_graph(ainb(room(1)))
        self.assertEqual(nodes[3]["type"], "OperateAND")
        self.assertEqual(nodes[3]["bdef"], {"UpdateOnce": 1})
        self.assertEqual(nodes[0]["bdef"], {"Logic_Create": 1})  # wired inputs have no constant
        self.assertEqual(nodes[2]["sdef"], {"GameDataNameBool": "Room_Flag"})
        self.assertEqual(eye_switches.read_graph(ainb(room(0)))[0][3]["bdef"], {"UpdateOnce": 0})

    def test_multi_input_list(self):
        nodes, multi = eye_switches.read_graph(ainb(room(0), multi=[(2, 0), (1, 1)]))
        self.assertEqual(multi(0, 2), [(2, 0), (1, 1)])


class SwitchEventsTest(unittest.TestCase):
    def run_room(self, once):
        with tempfile.TemporaryDirectory() as romfs:
            os.makedirs(os.path.join(romfs, "Logic"))
            os.makedirs(os.path.join(romfs, "Banc", "Scene"))
            with open(os.path.join(romfs, "Logic", "Room.logic.module.ainb"), "wb") as f:
                f.write(ainb(room(once)))
            actors = [{"Hash": 1, "Gyaml": "EventRequestActor", "Dynamic": {"RequestEventName": "Ev"},
                       "Translate": [1.0, 0.0, 0.0]},
                      {"Hash": 2, "Gyaml": "Obj_StepSwitch_A_01", "Translate": [0.0, 0.0, 0.0]}]
            groups = [{"Logic": "Work/Logic/Room.logic.module.ain",
                       "References": [{"Path": "Asker", "Id": "Asker", "Reference": 1},
                                      {"Path": "Switch", "Id": "Switch", "Reference": 2}]}]
            with open(os.path.join(romfs, "Banc", "Scene", "Room.bcett.byml.zs"), "wb") as f:
                f.write(byml.dumps({"Actors": actors, "AiGroups": groups}))
            wiring = {}
            lines, guards, _, _ = eye_switches.switch_events(romfs, dicts=_Plain(), wiring=wiring)
            return lines, guards, wiring

    def test_a_live_delete_flag_is_a_guard(self):
        lines, guards, wiring = self.run_room(0)
        self.assertEqual(len(lines), 1)
        self.assertEqual(lines[0][:3], ("Ev", "Scene", "Obj_StepSwitch_A_01"))
        self.assertEqual(guards, [("Room_Flag", "Scene", "Ev", "deletes the asker")])
        self.assertEqual(wiring[lines[0]]["delete"], ["Room_Flag"])

    def test_a_delete_flag_read_once_at_load_is_no_guard(self):
        lines, guards, wiring = self.run_room(1)
        self.assertEqual(len(lines), 1)
        self.assertEqual(guards, [])
        self.assertEqual(wiring[lines[0]]["delete"], [])


def lift_room(flag_alone):
    """A lift whose IsOn ORs the asker's EventBool with Room_Flag: alone (flag_alone) or ANDed with the switch (the
    Deku Tree compass lifts' OR(asker, AND(RoomD_StepSwitch_On_01, plate)))."""
    nodes = [
        {"type": "ActorLogicEventRequestActor", "imm": {"InstanceName": "Asker"},
         "bool": [("EventRequestTrigger", 1, 1, 0)], "out": ["Dead", "EventBool"]},
        {"type": "ActorLogicBootUpAndForceTrunOffControlActor", "imm": {"InstanceName": "Switch"},
         "out": ["OffState", "OnState"]},
        {"type": "HoldGameDataGetBoolSimple", "string": [("GameDataNameBool", "Room_Flag")], "out": ["Bool"]},
        {"type": "OperateAND", "bool": [("BoolMulti", -100, 2, 0), ("UpdateOnce", -1, 0, 0)], "out": ["False", "True"]},
        {"type": "OperateOR", "bool": [("BoolMulti", -102, 2, 0), ("UpdateOnce", -1, 0, 0)], "out": ["False", "True"]},
        {"type": "ActorLogicLift", "imm": {"InstanceName": "Lift"}, "bool": [("IsOn", 4, 1, 0)], "out": ["Dead"]},
    ]
    multi = [(2, 0), (1, 1), (0, 1), (2, 0) if flag_alone else (3, 1)]
    return nodes, multi


class AskerClassTest(unittest.TestCase):
    def run_room(self, flag_alone):
        with tempfile.TemporaryDirectory() as romfs:
            os.makedirs(os.path.join(romfs, "Logic"))
            os.makedirs(os.path.join(romfs, "Banc", "Scene"))
            nodes, multi = lift_room(flag_alone)
            with open(os.path.join(romfs, "Logic", "Room.logic.module.ainb"), "wb") as f:
                f.write(ainb(nodes, multi))
            actors = [{"Hash": 1, "Gyaml": "EventRequestActor", "Dynamic": {"RequestEventName": "Ev"},
                       "Translate": [1.0, 0.0, 0.0]},
                      {"Hash": 2, "Gyaml": "Obj_StepSwitch_A_01", "Translate": [0.0, 0.0, 0.0]},
                      {"Hash": 3, "Gyaml": "Obj_Lift", "Translate": [5.0, 0.0, 0.0]}]
            groups = [{"Logic": "Work/Logic/Room.logic.module.ain",
                       "References": [{"Path": n, "Id": n, "Reference": h}
                                      for n, h in (("Asker", 1), ("Switch", 2), ("Lift", 3))]}]
            with open(os.path.join(romfs, "Banc", "Scene", "Room.bcett.byml.zs"), "wb") as f:
                f.write(byml.dumps({"Actors": actors, "AiGroups": groups}))
            askers = {}
            eye_switches.switch_events(romfs, dicts=_Plain(), askers=askers)
            return askers

    def test_a_shared_flag_alone_in_the_or_is_live(self):
        askers = self.run_room(True)
        key = ("Ev", "Scene", (1.0, 0.0, 0.0))
        self.assertEqual(list(askers), [key])
        self.assertEqual(list(askers[key]["outputs"]), ["EventBool"])
        self.assertEqual(eye_switches.asker_class(key, askers[key], {"Room_Flag"})[0], "live")
        self.assertEqual(eye_switches.asker_class(key, askers[key], set())[0], "asker")  # not shared

    def test_a_shared_flag_anded_in_the_or_is_no_live_result(self):
        askers = self.run_room(False)
        key = ("Ev", "Scene", (1.0, 0.0, 0.0))
        cls, why = eye_switches.asker_class(key, askers[key], {"Room_Flag"})
        self.assertEqual(cls, "asker")
        self.assertIn("Obj_Lift.IsOn", why)
        (_, _, (_, also)), = askers[key]["outputs"]["EventBool"]
        self.assertIn(("Room_Flag", False, 2), also)  # in an OR, behind an AND
        self.assertIn(("EventRequestActor", False, 1), also)  # alone in the OR


class ResultsOutputTest(unittest.TestCase):
    """--results (row 98(2)): a class-asker line names its outputs; a shared live flag that deletes its asker is a
    result guard (coop_event_result.cpp kGuards); a flag read once at load is none."""

    def run_main(self, once):
        from unittest import mock
        import contextlib
        import io
        with tempfile.TemporaryDirectory() as romfs:
            os.makedirs(os.path.join(romfs, "Logic"))
            os.makedirs(os.path.join(romfs, "Banc", "Scene"))
            nodes = room(once) + [{"type": "ActorLogicLift", "imm": {"InstanceName": "Lift"},
                                   "bool": [("IsOn", 0, 1, 0)], "out": ["Dead"]}]
            with open(os.path.join(romfs, "Logic", "Room.logic.module.ainb"), "wb") as f:
                f.write(ainb(nodes))
            actors = [{"Hash": 1, "Gyaml": "EventRequestActor", "Dynamic": {"RequestEventName": "Ev"},
                       "Translate": [1.0, 0.0, 0.0]},
                      {"Hash": 2, "Gyaml": "Obj_StepSwitch_A_01", "Translate": [0.0, 0.0, 0.0]},
                      {"Hash": 3, "Gyaml": "Obj_Lift", "Translate": [5.0, 0.0, 0.0]}]
            groups = [{"Logic": "Work/Logic/Room.logic.module.ain",
                       "References": [{"Path": n, "Id": n, "Reference": h}
                                      for n, h in (("Asker", 1), ("Switch", 2), ("Lift", 3))]}]
            with open(os.path.join(romfs, "Banc", "Scene", "Room.bcett.byml.zs"), "wb") as f:
                f.write(byml.dumps({"Actors": actors, "AiGroups": groups}))
            dungeon = os.path.join(romfs, "coop_dungeon.cpp")
            with open(dungeon, "w", encoding="utf-8") as f:
                f.write('    {0x12345678, false, "Room_Flag"},\n')
            out = io.StringIO()
            with mock.patch.object(eye_switches.zs.Dicts, "from_romfs", return_value=_Plain()), \
                    contextlib.redirect_stdout(out):
                self.assertEqual(eye_switches.main(["--romfs", romfs, "--dungeon", dungeon, "--results"]), 0)
            return out.getvalue()

    def test_a_live_delete_of_a_class_asker_asker_is_a_result_guard(self):
        text = self.run_main(0)
        self.assertIn('{"Ev", "Scene", {1.0f, 0.0f, 0.0f}, "EventBool"},  // asker', text)
        guards = text.split("// kResultGuards", 1)[1]
        self.assertIn('{"Room_Flag", "Scene", "Ev"},', guards)

    def test_a_delete_read_once_at_load_is_no_result_guard(self):
        text = self.run_main(1)
        guards = text.split("// kResultGuards", 1)[1]
        self.assertNotIn("Room_Flag", guards)


if __name__ == "__main__":
    unittest.main()
