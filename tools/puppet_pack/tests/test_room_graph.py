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
        self.assertEqual(wiring[lines[0]]["guards"], ["Room_Flag"])  # g3-objects: a live guard

    def test_a_delete_flag_read_once_at_load_is_no_guard(self):
        lines, guards, wiring = self.run_room(1)
        self.assertEqual(len(lines), 1)
        self.assertEqual(guards, [])
        self.assertEqual(wiring[lines[0]]["delete"], [])
        # objects-next-a-6: for g3-objects' hold it is a LOAD guard (on at our room's load: our asker is gone)
        self.assertEqual(wiring[lines[0]]["guards"], ["load:Room_Flag"])


if __name__ == "__main__":
    unittest.main()
