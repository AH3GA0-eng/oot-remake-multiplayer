"""Prints the eye-switch table of lichii/hook/coop_eye_switch.cpp (kSwitches).

Run by a developer against their own romfs dump; nothing from the game is in
this tool:

    python -m puppet_pack.eye_switches --romfs <romfs>

The rooms whose event starts from an eye switch (deku-ladder, tracker rows 90
and 102): the room's logic graph (an AiGroup in Banc/<scene>/*.bcett, its own
`Logic` or a nested one in its References) has an
`ActorLogicBootUpActorIsFirstOnEventTrigger` node, and the graph's
`EventRequestActor` (its `RequestEventName`) has a Reference link to the
graph's `Obj_EyeSwitch_*`. That eye's output plug FirstOn raises the asker's
EventRequestTrigger. Askers of the same graph without a link to the eye are
other events (enemies, chests) and are left out, as are eyes whose asker asks
the generic FocusOnGimmick_Basic (Dodongo's Cavern fire walls: the event has no
place of its own, coop_live_places.cpp).

One line per (event, eye): the event, its scene, the eye's actor name and
placement (x, height, z) and the asker's placement.
"""
import argparse
import glob
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "puppet_pack"  # noqa: A001

from . import byml, zs  # noqa: E402

TRIGGER = b"ActorLogicBootUpActorIsFirstOnEventTrigger"
SKIP_EVENTS = {"FocusOnGimmick_Basic"}


def _int(v):
    return int(v.v if hasattr(v, "v") else v)


def _place(actor):
    return tuple(round(float(x), 1) for x in (actor.get("Translate") or [0, 0, 0]))


def eye_switches(romfs, dicts=None):
    """[(event, scene, eye name, eye place, asker place)], sorted."""
    if dicts is None:
        dicts = zs.Dicts.from_romfs(romfs)
    graphs = set()
    for path in glob.glob(os.path.join(romfs, "Logic", "*.ainb")):
        with open(path, "rb") as f:
            if TRIGGER in f.read():
                graphs.add(os.path.basename(path)[: -len(".ainb")])
    out = set()
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
            logics = [g.get("Logic", "")] + [r.get("Logic", "") for r in refs if "Logic" in r]
            if not any(os.path.basename(str(lg))[: -len(".ain")] in graphs for lg in logics if lg):
                continue
            members = [by_hash.get(_int(r["Reference"])) for r in refs if "Reference" in r]
            members = [m for m in members if m is not None]
            eyes = {_int(m["Hash"]): m for m in members if str(m.get("Gyaml", "")).startswith("Obj_EyeSwitch_")}
            for asker in members:
                if str(asker.get("Gyaml", "")) != "EventRequestActor":
                    continue
                event = (asker.get("Dynamic") or {}).get("RequestEventName")
                if not event or event in SKIP_EVENTS:
                    continue
                for link in asker.get("Links", []) or []:
                    eye = eyes.get(_int(link.get("Dst", 0)))
                    if eye is not None:
                        out.add((str(event), scene, str(eye["Gyaml"]), _place(eye), _place(asker)))
    return sorted(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--romfs", required=True)
    args = ap.parse_args(argv)
    for event, scene, eye, e, a in eye_switches(args.romfs):
        print(f'    {{"{event}", "{scene}", "{eye}", {{{e[0]}f, {e[1]}f, {e[2]}f}}, {{{a[0]}f, {a[1]}f, {a[2]}f}}}},')
    return 0


if __name__ == "__main__":
    sys.exit(main())
