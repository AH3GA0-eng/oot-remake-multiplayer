"""Prints the room-object tables of lichii/hook/coop_room_objects.cpp (kPlacements, kRiderNames).

Run by a developer against their own romfs dump; nothing from the game is in
this tool:

    python -m puppet_pack.room_objects --romfs <romfs>

The rule (room-objects, issues 196, 249, 194): in a game that follows another,
a room object moves only through its own logic, the input its own switch,
sensor or rider would set (CoopEyeSwitch's switch plug, CoopLiftRide's
RideOnPlayer, the push block's node), never by a word written into it from
outside. The clock path (coop_platforms.h) is for free-running movers only:
whose logic node in the room graph has no wired control input, and whose
logic type has no rider plug.

For every placement of a platform name (CoopPlatforms::IsDriver, the same
name rule as below) this prints one line: scene, name, placement, and whether
it is driven by its own logic and why: the wired input names (the room graph
feeds them from a switch, a flag, a sensor or an event) or `RideOnPlayer` (its
logic type has that plug: a rider lift). Inputs that only reset or warp it
(Logic_Create, Logic_Delete, RespawnTrigger, ForceWarp, IisForceWarp) do not
drive it. A placement with no logic node is free.

Then the names whose logic type has the bool output RideOnPlayer (the rider
lifts CoopLiftRide drives).
"""
import argparse
import glob
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "puppet_pack"  # noqa: A001

from . import byml, zs  # noqa: E402
from .eye_switches import _int, _place, _prefix, read_graph  # noqa: E402

PARTS = ("ElevatorLift", "_Elevator_", "MoveFloor", "RotateFloor", "RotateBlade", "MoveSpike",
         "ExtendingPlatform", "_lift_", "ElevatorMirror", "ElevatorPit", "UpDown")
NOT_DRIVING = {"Logic_Create", "Logic_Delete", "RespawnTrigger", "ForceWarp", "IisForceWarp"}
RIDER_PLUG = "RideOnPlayer"


def is_driver(name):
    """CoopPlatforms::IsDriver."""
    if name.startswith(("GroundControlActor_", "GrouondControlActor_")):
        return True
    if not name.startswith(("Obj_", "Land_")):
        return False
    return any(p in name for p in PARTS)


def wired_inputs(node):
    """The input names of a logic node that the graph feeds (not a constant, not reset-only)."""
    out = set()
    for t in ("bool", "int", "float", "vec3"):
        for name, src, _ in node["in"][t]:
            if name not in NOT_DRIVING and (src >= 0 or src <= -100):
                out.add(name)
    return out


def room_objects(romfs, dicts=None):
    """([(scene, name, place, why)], rider names): why is '' for a free placement."""
    if dicts is None:
        dicts = zs.Dicts.from_romfs(romfs)
    graphs = {}

    def graph(logic):
        path = os.path.join(romfs, "Logic", os.path.basename(str(logic)) + "b")
        if path not in graphs:
            graphs[path] = read_graph(open(path, "rb").read()) if os.path.exists(path) else None
        return graphs[path]

    rows, riders = {}, set()
    for folder in sorted(glob.glob(os.path.join(romfs, "Banc", "*"))):
        scene = os.path.basename(folder)
        actors, groups = [], []
        for path in sorted(glob.glob(os.path.join(folder, "*.bcett.byml.zs"))):
            with open(path, "rb") as f:
                root = byml.loads(dicts.decompress(f.read()))
            actors += root.get("Actors", []) or []
            groups += root.get("AiGroups", []) or []
        why = {}
        for a in actors:
            name = str(a.get("Gyaml", ""))
            if is_driver(name) and "Hash" in a:
                why.setdefault(_int(a["Hash"]), set())
        by_hash = {_int(a["Hash"]): a for a in actors if "Hash" in a}
        for g in groups:
            refs = g.get("References", []) or []
            mods = [("", g.get("Logic", ""))] + [(_prefix(r["Path"]), r["Logic"]) for r in refs if "Logic" in r]
            by_id = {(_prefix(r["Path"]), r["Id"]): _int(r["Reference"])
                     for r in refs if "Reference" in r and _int(r["Reference"]) in by_hash}
            for pre, logic in mods:
                g_ = graph(logic) if logic else None
                if g_ is None:
                    continue
                for node in g_[0]:
                    h = by_id.get((pre, node["imm"].get("InstanceName")))
                    if h is None or h not in why:
                        continue
                    why[h] |= wired_inputs(node)
                    if RIDER_PLUG in node["out"]["bool"]:
                        why[h].add(RIDER_PLUG)
                        riders.add(str(by_hash[h]["Gyaml"]))
        # A part with no logic of its own placed where a driven one is (the Deku Tree's ElevatorLiftLim_A_0n on its
        # ElevatorLift_A_0n) moves with it: driven too.
        driven = [(by_hash[h], r) for h, r in why.items() if r]
        for h, reasons in why.items():
            if reasons:
                continue
            p = _place(by_hash[h])
            for other, r in driven:
                q = _place(other)
                if sum((p[i] - q[i]) ** 2 for i in range(3)) <= 0.25:
                    reasons.add("with " + str(other["Gyaml"]))
        for h, reasons in why.items():
            a = by_hash[h]
            key = (scene, str(a["Gyaml"]), _place(a))
            rows[key] = "+".join(sorted(reasons | set(rows.get(key, "").split("+")) - {""}))
    return sorted((s, n, p, w) for (s, n, p), w in rows.items()), sorted(riders)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--romfs", required=True)
    args = ap.parse_args(argv)
    rows, riders = room_objects(args.romfs)
    for scene, name, p, why in rows:
        print(f'    {{"{scene}", "{name}", {{{p[0]}f, {p[1]}f, {p[2]}f}}, "{why}"}},')
    print()
    for name in riders:
        print(f'    "{name}",')
    return 0


if __name__ == "__main__":
    sys.exit(main())
