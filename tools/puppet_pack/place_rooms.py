"""Adds each live-event place's room to lichii/hook/coop_live_places.cpp (the last field of a Place).

Run by a developer against their own romfs dump; nothing from the game is in this tool:

    python -m puppet_pack.place_rooms --romfs <romfs> --places lichii/hook/coop_live_places.cpp [--write]

Why (deku-ladder, LANE1 run hs-and-gate 2026-11-16): the live-event decider starts another player's room event
here only when our Link is in the room of the event's place, so a place near our Link "through a wall" waits. It
guessed the place's room like a player's: the room of the nearest placement that has a room index
(coop_rooms.h). An asker (EventRequestActor) has no room index of its own and often stands high on a wall, nearer
another room's placements: Jabu-Jabu's room-1 switch asker (7.3 109.7 -48.1) was guessed in room 5 while it, its
switch and the players at the switch are in room 17, so the event never started. 46 of the 204 places were guessed
in another room than their own.

Their own room: the asker belongs to its room's logic graph (an AiGroup in Banc/<scene>/*.bcett), and the graph's
other members carry `Dynamic.DefaultDungeonRoomIndex`. The room most of them carry is the asker's room (doors and
shutters left out: they stand between two rooms). The group's own `Dynamic.DungeonRoomID` is not used: it differs
from its members' rooms in 79 of 383 cases (Ganon's Castle's room-12 chest graph says 1). A place with no such
group keeps -1: the hook guesses it as before.
"""
import argparse
import collections
import glob
import os
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "puppet_pack"  # noqa: A001

from . import byml, zs  # noqa: E402

LINE = re.compile(r'^(\s*\{"(\w+)", "(\w+)", ([-\d.]+)f, ([-\d.]+)f, ([-\d.]+)f)(?:, -?\d+)?(\},.*)$')


def _int(v):
    return int(v.v if hasattr(v, "v") else v)


def _place(actor):
    return tuple(round(float(x), 1) for x in (actor.get("Translate") or [0, 0, 0]))


def placement_rooms(romfs, dicts=None):
    """{(scene, (x, y, z)): room} for every placement without a room index of its own that is in a room's graph."""
    if dicts is None:
        dicts = zs.Dicts.from_romfs(romfs)
    out = {}
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
            members = [by_hash.get(_int(r["Reference"])) for r in g.get("References", []) or [] if "Reference" in r]
            members = [a for a in members if a is not None]
            votes = collections.Counter(
                int(a["Dynamic"]["DefaultDungeonRoomIndex"]) for a in members
                if (a.get("Dynamic") or {}).get("DefaultDungeonRoomIndex") is not None
                and "Shutter" not in str(a.get("Gyaml")) and "Door" not in str(a.get("Gyaml")))
            if not votes:
                continue
            room = votes.most_common(1)[0][0]
            for a in members:
                if (a.get("Dynamic") or {}).get("DefaultDungeonRoomIndex") is None:
                    out.setdefault((scene, _place(a)), room)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--romfs", required=True)
    ap.add_argument("--places", required=True)
    ap.add_argument("--write", action="store_true", help="rewrite the places file (else print it)")
    args = ap.parse_args(argv)
    rooms = placement_rooms(args.romfs)
    lines = open(args.places, encoding="utf-8").read().split("\n")
    known = 0
    for i, line in enumerate(lines):
        m = LINE.match(line)
        if not m:
            continue
        head, _, scene, x, y, z, tail = m.groups()
        room = rooms.get((scene, (round(float(x), 1), round(float(y), 1), round(float(z), 1))), -1)
        known += room >= 0
        lines[i] = f"{head}, {room}{tail}"
    text = "\n".join(lines)
    if args.write:
        open(args.places, "w", encoding="utf-8", newline="\n").write(text)
    else:
        print(text)
    print(f"place_rooms: {known} places with their own room", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
