"""Prints the switch table of lichii/hook/coop_eye_switch.cpp (kSwitches).

Run by a developer against their own romfs dump; nothing from the game is in
this tool:

    python -m puppet_pack.eye_switches --romfs <romfs> [--places lichii/hook/coop_live_places.cpp]
                                       [--dungeon lichii/hook/coop_dungeon.cpp]

A room's gimmick event is asked by its EventRequestActor when the actor's
input `EventRequestTrigger` rises. That input is wired in the room's logic
graph (an AiGroup in Banc/<scene>/*.bcett: its own `Logic` and the nested
modules in its References), straight or through AND/OR gates and module
inputs, to a switch's output plug: `OnState` (eye, step, hit, hammer and dive
switches), `FirstOn`, `isHit` (the Forest Temple's silver eyes) or
`isLightMax` (sun switches). Another player's copy of the event, asked from
outside, plays its camera only: our asker never saw its trigger (deku-ladder,
tracker rows 90 and 102). So the hook sets that plug on OUR switch instead.

This tool follows the wiring (the graph's input links, its multi-input list,
and module inputs up to the calling graph) from every asker's trigger to the
switch plugs at its leaves, and prints one line per (event, switch): the event,
its scene, the switch's actor name and placement, the plug, and the asker's
placement. With --places only the events that have a live place (shared,
startable) are printed.

AI file layout read here (version 0x408), as far as needed:
  header +0x0c commands, +0x10 nodes, +0x24 string pool, +0x2c immediate
  parameters, +0x34 input/output parameters, +0x38 the multi-input list
  node 0x3c each after the commands (0x18 each): +0x08 type name, +0x14 body
  body: +0x18 the string immediates (index, count); +0x30 + 16*t the inputs
  (index, count) and outputs (index, count) of type t (int, bool, float,
  string, vec3, user)
  input entry: +0x00 name, then source node (s16) and its output (s16) at +4
  (+8 for user types); a source <= -100 is the multi-input list from entry
  (-100 - source), count = the output field; multi entry 8 bytes: node, output
  string immediate 0x0c: +0x00 name, +0x08 value (InstanceName, Name)
  string input: +0x0c its default value (HoldGameDataGetBool's GameDataNameBool: the flag's name)
"""
import argparse
import glob
import os
import re
import struct
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "puppet_pack"  # noqa: A001

from . import byml, zs  # noqa: E402

TYPES = ("int", "bool", "float", "string", "vec3", "udt")
IN_SIZE = {"int": 0x10, "bool": 0x10, "float": 0x10, "string": 0x10, "vec3": 0x18, "udt": 0x14}
OUT_SIZE = {"int": 4, "bool": 4, "float": 4, "string": 4, "vec3": 4, "udt": 8}
# Switch actors whose output starts an event (not the shutters they open).
SWITCH = re.compile(r"^Obj_(?:[A-Za-z]+_)*(?:EyeSwitch|StepSwitch|SwitchHit|HammerSwitch|DiveSwitch|SolarSwitch)(?:_|$)")


def read_graph(data):
    """[node]: {"type", "imm": {name: str}, "in": {type: [(name, src, out)]}, "out": {type: [name]}}, multi(base, n)."""
    u32 = lambda o: struct.unpack_from("<I", data, o)[0]  # noqa: E731
    s16 = lambda o: struct.unpack_from("<h", data, o)[0]  # noqa: E731
    u16 = lambda o: struct.unpack_from("<H", data, o)[0]  # noqa: E731
    pool = u32(0x24)

    def text(o):
        return data[pool + o: data.index(b"\0", pool + o)].decode("utf-8", "replace")

    commands, count = u32(0x0C), u32(0x10)
    io, multi_at, imm = u32(0x34), u32(0x38), u32(0x2C)
    starts = [u32(io + 4 * i) for i in range(12)] + [multi_at]
    ins, outs, defaults = {}, {}, []
    for t, name in enumerate(TYPES):
        lst, o = [], starts[2 * t]
        while o < starts[2 * t + 1]:
            at = o + (8 if name == "udt" else 4)
            lst.append((text(u32(o) & 0x3FFFFFFF), s16(at), s16(at + 2)))
            if name == "string":  # its default value (a flag's name) at +0x0c
                defaults.append(text(u32(o + 12)))
            o += IN_SIZE[name]
        ins[name] = lst
        lst, o = [], starts[2 * t + 1]
        while o < starts[2 * t + 2]:
            lst.append(text(u32(o) & 0x3FFFFFFF))
            o += OUT_SIZE[name]
        outs[name] = lst
    string_imm = u32(imm + 12)
    nodes = []
    for i in range(count):
        n = 0x74 + commands * 0x18 + i * 0x3C
        body = u32(n + 0x14)
        node = {"type": text(u32(n + 8)), "in": {}, "out": {}, "imm": {}, "sdef": {}}
        for t, name in enumerate(TYPES):
            a, b = u32(body + 0x30 + 16 * t), u32(body + 0x34 + 16 * t)
            c, d = u32(body + 0x38 + 16 * t), u32(body + 0x3C + 16 * t)
            node["in"][name] = ins[name][a: a + b]
            node["out"][name] = outs[name][c: c + d]
            if name == "string":
                node["sdef"] = {k[0]: v for k, v in zip(ins[name][a: a + b], defaults[a: a + b])}
        first, n_str = u32(body + 0x18), u32(body + 0x1C)
        for k in range(first, first + n_str):
            o = string_imm + 0x0C * k
            node["imm"][text(u32(o))] = text(u32(o + 8))
        nodes.append(node)

    def multi(base, n):
        return [(u16(multi_at + 8 * (base + k)), u16(multi_at + 8 * (base + k) + 2)) for k in range(n)]

    return nodes, multi


def _int(v):
    return int(v.v if hasattr(v, "v") else v)


def _place(actor):
    return tuple(round(float(x), 1) for x in (actor.get("Translate") or [0, 0, 0]))


def _prefix(path):
    return path.rsplit("/", 1)[0] + "/" if "/" in path else ""


def switch_events(romfs, dicts=None):
    """(lines, guards, extras), each sorted.

    lines:  (event, scene, switch name, switch place, plug, asker place)
    guards: (flag, scene, event, why) - a saved flag that keeps the event from starting in our game when it comes
            from another player first: one that deletes the asker (why "deletes the asker"), or one the trigger reads
            negated (why "the trigger reads it negated")
    extras: (event, scene, actor name, actor place, plug) - another actor's plug the trigger ANDs with the switch
    others: (event, scene, what) - any other input on the trigger (a positive flag, a player state, ...), for notes
    """
    if dicts is None:
        dicts = zs.Dicts.from_romfs(romfs)
    graphs = {}

    def graph(logic):
        path = os.path.join(romfs, "Logic", os.path.basename(str(logic)) + "b")
        if path not in graphs:
            graphs[path] = read_graph(open(path, "rb").read()) if os.path.exists(path) else None
        return graphs[path]

    lines, guards, extras, others = set(), set(), set(), set()
    for folder in sorted(glob.glob(os.path.join(romfs, "Banc", "*"))):
        scene = os.path.basename(folder)
        actors, groups = [], []
        for path in glob.glob(os.path.join(folder, "*.bcett.byml.zs")):
            with open(path, "rb") as f:
                root = byml.loads(dicts.decompress(f.read()))
            actors += root.get("Actors", []) or []
            groups += root.get("AiGroups", []) or []
        by_hash = {_int(a["Hash"]): a for a in actors if "Hash" in a}
        for g in groups:
            refs = g.get("References", []) or []
            mods = [("", g.get("Logic", ""), None)]
            mods += [(_prefix(r["Path"]), r["Logic"], r.get("InstanceName")) for r in refs if "Logic" in r]
            by_id = {(_prefix(r["Path"]), r["Id"]): by_hash[_int(r["Reference"])]
                     for r in refs if "Reference" in r and _int(r["Reference"]) in by_hash}

            def leaves(pre, g_, src, outp, depth, seen, neg=False, under_or=False):
                """[(kind, actor or flag name, plug, negated, under an OR)], kind "actor", "flag" or "node"."""
                nodes, multi = g_
                if src <= -100:
                    found = []
                    for a, b in multi(-100 - src, outp):
                        found += leaves(pre, g_, a, b, depth + 1, seen, neg, under_or)
                    return found
                if src < 0 or src >= len(nodes) or depth > 12 or (id(g_), src, outp) in seen:
                    return []
                seen.add((id(g_), src, outp))
                node = nodes[src]
                plugs = node["out"]["bool"]
                plug = plugs[outp] if 0 <= outp < len(plugs) else ""
                inst = node["imm"].get("InstanceName")
                if inst and (pre, inst) in by_id:
                    return [("actor", by_id[(pre, inst)], plug, neg, under_or)]
                if not node["type"] and "Name" in node["imm"]:  # a module input: follow it in the calling graph
                    found = []
                    for mpre, logic, minst in mods:
                        if mpre != pre or minst is None:
                            continue
                        mtype = os.path.basename(str(logic))[: -len(".ain")]
                        for ppre, plogic, _ in mods:
                            parent = graph(plogic) if plogic and ppre != pre and pre.startswith(ppre) else None
                            if parent is None:
                                continue
                            for call in parent[0]:
                                if call["type"] == mtype and call["imm"].get("InstanceName") == minst:
                                    for name, a, b in call["in"]["bool"]:
                                        if name == node["imm"]["Name"]:
                                            found += leaves(ppre, parent, a, b, depth + 1, seen, neg, under_or)
                    return found
                if node["type"].startswith("HoldGameDataGetBool"):
                    name = node["sdef"].get("GameDataNameBool", "")
                    return [("flag", name, plug, neg, under_or)] if name else []
                if not node["in"]["bool"]:
                    return [("node", node["type"], plug, neg, under_or)]
                # A gate, or a module call (InvBool, ...): its False output (or an inverter) negates.
                flip = plug == "False" or "InvBool" in node["type"]
                is_or = "OR" in node["type"].upper().replace("OPERATE", "")[:2] or node["type"].startswith("OperateOR")
                found = []
                for _, a, b in node["in"]["bool"]:
                    found += leaves(pre, g_, a, b, depth + 1, seen, neg != flip, under_or or is_or)
                return found

            for pre, logic, _ in mods:
                g_ = graph(logic) if logic else None
                if g_ is None:
                    continue
                for node in g_[0]:
                    if node["type"] != "ActorLogicEventRequestActor":
                        continue
                    asker = by_id.get((pre, node["imm"].get("InstanceName")))
                    event = (asker or {}).get("Dynamic", {}).get("RequestEventName") if asker else None
                    if not event:
                        continue
                    trigger, delete = [], []
                    for name, a, b in node["in"]["bool"]:
                        if name == "EventRequestTrigger":
                            trigger = leaves(pre, g_, a, b, 0, set())
                        elif name == "Logic_Delete":
                            delete = leaves(pre, g_, a, b, 0, set())
                    switches = [t for t in trigger if t[0] == "actor" and SWITCH.match(str(t[1].get("Gyaml", ""))) and t[2]]
                    if not switches:
                        continue
                    event = str(event)
                    for _, actor, plug, _, _ in switches:
                        lines.add((event, scene, str(actor["Gyaml"]), _place(actor), plug, _place(asker)))
                    for kind, what, plug, neg, under_or in trigger:
                        if kind == "actor" and SWITCH.match(str(what.get("Gyaml", ""))):
                            continue
                        if kind == "flag" and neg:
                            guards.add((what, scene, event, "the trigger reads it negated"))
                        elif kind == "actor" and not neg and not under_or and plug:
                            extras.add((event, scene, str(what["Gyaml"]), _place(what), plug))
                        else:
                            label = str(what.get("Gyaml")) if kind == "actor" else what
                            others.add((event, scene, f"{'not ' if neg else ''}{label}.{plug}{' (in an OR)' if under_or else ''}"))
                    for kind, what, plug, neg, under_or in delete:
                        if kind == "flag" and not neg and not what.startswith("HavePlayedEvent."):
                            guards.add((what, scene, event, "deletes the asker"))
    return sorted(lines), sorted(guards), sorted(extras), sorted(others)


def live_places(path):
    """{(event, scene)} of lichii/hook/coop_live_places.cpp."""
    text = open(path, encoding="utf-8").read()
    return set(re.findall(r'\{"(\w+)", "(\w+)", [-\d.]+f', text))


def shared_flags(path):
    """The flag names of lichii/hook/coop_dungeon.cpp's tables (the flags another player's game sends us)."""
    text = open(path, encoding="utf-8").read()
    return set(re.findall(r'\{0x[0-9a-f]+, (?:true|false), "([A-Za-z0-9_.]+)"\}', text))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--romfs", required=True)
    ap.add_argument("--places", help="coop_live_places.cpp: print only events that have a live place")
    ap.add_argument("--dungeon", help="coop_dungeon.cpp: print only guard flags that are shared")
    args = ap.parse_args(argv)
    keep = live_places(args.places) if args.places else None
    shared = shared_flags(args.dungeon) if args.dungeon else None
    lines, guards, extras, others = switch_events(args.romfs)
    wanted = {(e, s) for e, s, *_ in lines if keep is None or (e, s) in keep}
    print("// kSwitches")
    for event, scene, switch, s, plug, a in lines:
        if (event, scene) in wanted:
            print(f'    {{"{event}", "{scene}", "{switch}", {{{s[0]}f, {s[1]}f, {s[2]}f}}, "{plug}", '
                  f'{{{a[0]}f, {a[1]}f, {a[2]}f}}}},')
    print("// kExtras")
    for event, scene, actor, p, plug in extras:
        if (event, scene) in wanted:
            print(f'    {{"{event}", "{scene}", "{actor}", {{{p[0]}f, {p[1]}f, {p[2]}f}}, "{plug}"}},')
    print("// kGuards")
    whys = {}
    for flag, scene, event, why in guards:
        if (event, scene) in wanted and (shared is None or flag in shared):
            whys.setdefault((flag, scene, event), set()).add(why)
    for (flag, scene, event), why in sorted(whys.items()):
        print(f'    {{"{flag}", "{scene}", "{event}"}},  // {" + ".join(sorted(why))}')
    print("// not covered (other inputs on a trigger: a guard flag that is not shared, a player state, an OR branch)")
    for event, scene, what in others:
        if (event, scene) in wanted:
            print(f"//   {event}: {what}")
    for flag, scene, event, why in guards:
        if (event, scene) in wanted and shared is not None and flag not in shared:
            print(f"//   {event}: {flag} ({why}; not shared: each game's own)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
