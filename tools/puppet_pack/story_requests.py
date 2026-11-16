"""Writes lichii/hook/coop_story_requests.inc: which event settings start a shared story cutscene.

    python -m puppet_pack.story_requests --romfs <romfs> --out lichii/hook/coop_story_requests.inc

The game asks for an event by its event setting (RSDB/EventSetting: a name, the flow and the entry
it plays) and keeps "already played" per setting (`HavePlayedEvent.<setting>`). Most shared story
cutscenes (`kNames` in lichii/hook/coop_shared_scenes.cpp) are a setting of their own name. Many
settings play a story flow under another name: the Great Deku Tree's meeting is the setting
`KokiriForest_Meet_DekuTree_MeetDekuTree_01` of the flow Meet_DekuTree (player report 110,
docs/scenes.md "Shared story cutscenes"), Darunia's dance `Meet_Darunia_PlayOcarina` one of
Get_StoneOfFire, the Fire Temple's freed Gorons `Dm_FireTemple_0014`..`_0022` entries of
Dm_FireTemple_0020.

Every named setting (not a numeric row id: those are talks) whose flow is a story name and whose own
name is not that flow's is classified here from the game's data, first rule that holds:
  boss     its components are BossDie, or `Boss` is in its name or entry: the boss scenes have it
           (lichii/hook/coop_boss_scenes.cpp, docs/dungeons-boss.md);
  gimmick  a dungeon live event (a `HavePlayedEvent.<name>` of lichii/hook/coop_dungeon.cpp or a
           place of lichii/hook/coop_live_places.cpp): the dungeon path shares it;
  own      started inside a talk of the player's own (components NPCTalk*, or an entry Talk*):
           another game has no talk to continue;
  own      played where the player is, on that player's own arrival (base PlayerPos);
  own      an item each player gets for themself (`_Get<Item>`, not a spiritual stone);
  story    a `Dm_` cutscene entry of a `Dm_` story flow, or a name that carries its flow's name as
           a whole part (`Get_StoneOfWater_ShowLetter`): shared, sent and joined by its name;
  never    anything else: a room or object event the flow's file also holds (a door, a hole).
The `story` ones go in kStorySettings, the rest in comments with their reason. It also writes
`kNoSetting`, the story names with no setting: no game can be asked for them, so they are neither
sent nor joined. Every story setting and every other story name must have its played flag in
GameData/GameDataList.Product.*.byml.zs, else the tool stops.
Names only: nothing of the game's data but its own event and scene names.
"""
import argparse
import glob
import os
import re
import sys
from collections import namedtuple

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "puppet_pack"  # noqa: A001

from . import byml, zs  # noqa: E402

HOOK = os.path.join(os.path.dirname(__file__), "..", "..", "lichii", "hook")

Setting = namedtuple("Setting", "flow entry base components")


def story_names(cpp_text):
    """The quoted names of `kNames[] = { ... };` in coop_shared_scenes.cpp."""
    m = re.search(r"kNames\[\]\s*=\s*\{(.*?)\};", cpp_text, re.S)
    if not m:
        raise ValueError("no kNames table")
    return set(re.findall(r'"([^"]+)"', m.group(1)))


def gimmick_names(dungeon_cpp, live_places_cpp):
    """The dungeon live events: `HavePlayedEvent.<name>` in coop_dungeon.cpp, places' names in coop_live_places.cpp."""
    out = set(re.findall(r'"HavePlayedEvent\.([A-Za-z0-9_]+)"', dungeon_cpp))
    out |= set(re.findall(r'\{"([A-Za-z0-9_]+)",\s*"[A-Za-z0-9_]+",\s*-?[0-9]', live_places_cpp))
    return out


def _stem(path):
    return os.path.basename(path or "").split(".")[0]


def event_settings(rows):
    """{setting name: Setting} from the rows of RSDB/EventSetting (a setting with no flow: flow "")."""
    return {
        r["__RowId"]: Setting(_stem(r.get("FlowchartFileName")), r.get("EntryPointName", "") or "",
                              _stem(r.get("EventBaseSetting")), _stem(r.get("EventSettingComponentList")))
        for r in rows
    }


def carries(name, flow):
    """Whether `name` holds `flow` as whole parts between underscores."""
    return f"_{flow}_" in f"_{name}_"


def classify(name, s, gimmicks):
    """(class, reason) of a named setting `s` of a story flow (module docstring)."""
    if s.components == "BossDie" or "Boss" in name or "Boss" in s.entry:
        return "boss", "a boss event: the boss scenes have it"
    if name in gimmicks:
        return "gimmick", "a dungeon live event"
    if s.components.startswith("NPCTalk") or s.entry.startswith("Talk"):
        return "own", "started in the player's own talk"
    if s.base == "PlayerPos":
        return "own", "played on the player's own arrival"
    if re.search(r"_Get(?!Stone)[A-Z]", name):
        return "own", "an item each player gets"
    if (name.startswith("Dm_") and s.flow.startswith("Dm_")) or carries(name, s.flow):
        return "story", ""
    return "never", "no story name in it: a room or object event"


def table(settings, story, gimmicks):
    """[(setting, flow, class, reason)] sorted by flow and name: every named setting of a story flow
    under another name."""
    rows = []
    for name, s in settings.items():
        if s.flow in story and name != s.flow and not name[:1].isdigit():
            rows.append((name, s.flow) + classify(name, s, gimmicks))
    return sorted(rows, key=lambda r: (r[1], r[0]))


def no_setting(story, settings):
    """The story names the game has no event setting of."""
    return sorted(n for n in story if n not in settings)


def murmur3(text, seed=0):
    """MurmurHash3 x86 32-bit, the game's hash of a flag name (coop::host::flag_hash)."""
    data = text.encode()
    h, c1, c2, m = seed, 0xCC9E2D51, 0x1B873593, 0xFFFFFFFF
    n = len(data) // 4
    for i in range(n):
        k = int.from_bytes(data[4 * i:4 * i + 4], "little")
        k = (k * c1) & m
        k = ((k << 15) | (k >> 17)) & m
        h ^= (k * c2) & m
        h = ((h << 13) | (h >> 19)) & m
        h = (h * 5 + 0xE6546B64) & m
    tail = data[4 * n:]
    if tail:
        k = int.from_bytes(tail, "little")
        k = (k * c1) & m
        k = ((k << 15) | (k >> 17)) & m
        h ^= (k * c2) & m
    h ^= len(data)
    h ^= h >> 16
    h = (h * 0x85EBCA6B) & m
    h ^= h >> 13
    h = (h * 0xC2B2AE35) & m
    h ^= h >> 16
    return h


def played_flag(name):
    """The flag the game sets once the setting `name` has played."""
    return "HavePlayedEvent." + name


def missing_played_flags(names, gamedata):
    """The names whose played flag's hash is not in `gamedata` (the decompressed, big-endian GameDataList)."""
    return [n for n in names if murmur3(played_flag(n)).to_bytes(4, "big") not in gamedata]


def render(rows, nosetting):
    lines = [
        "// Generated by tools/puppet_pack/story_requests.py from the game's RSDB EventSetting and GameDataList",
        "// and the hook's own lists; do not edit. Every named event setting that plays a shared story flow under",
        "// another name: the shared ones (sent and joined by that name, coop_shared_scenes.h StorySetting), then",
        "// the others with why, then the story names the game has no setting of.",
        "constexpr StorySetting kStorySettings[] = {",
    ]
    lines += [f'    {{"{name}", "{flow}"}},' for name, flow, cls, _ in rows if cls == "story"]
    lines.append("};")
    lines.append("// Not shared (a request of one is another event: the story search's find is refused):")
    lines += [f"//   {cls:7} {name} ({flow}): {why}" for name, flow, cls, why in rows if cls != "story"]
    lines.append("constexpr std::string_view kNoSetting[] = {")
    lines += [f'    "{n}",' for n in nosetting]
    lines.append("};")
    return "\n".join(lines) + "\n"


def _text(name):
    with open(os.path.join(HOOK, name), encoding="utf-8") as fh:
        return fh.read()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--romfs", required=True, help="your own extracted game (holds RSDB and GameData)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args(argv)
    story = story_names(_text("coop_shared_scenes.cpp"))
    gimmicks = gimmick_names(_text("coop_dungeon.cpp"), _text("coop_live_places.cpp"))
    dicts = zs.Dicts.from_romfs(args.romfs)

    def read(*parts):
        paths = sorted(glob.glob(os.path.join(args.romfs, *parts)))
        if not paths:
            raise SystemExit(f"no {'/'.join(parts)} in the romfs")
        with open(paths[-1], "rb") as fh:
            return zs.maybe_decompress(dicts, fh.read())

    settings = event_settings(byml.loads(read("RSDB", "EventSetting.Product.*.rstbl.byml.zs")))
    rows = table(settings, story, gimmicks)
    nosetting = no_setting(story, settings)
    gamedata = read("GameData", "GameDataList.Product.*.byml.zs")
    asked = [r[0] for r in rows if r[2] == "story"] + sorted(story - set(nosetting))
    missing = missing_played_flags(asked, gamedata)
    if missing:
        raise SystemExit("no played flag in the game for: " + ", ".join(played_flag(n) for n in missing))
    with open(args.out, "w", newline="\n") as fh:
        fh.write(render(rows, nosetting))
    shared = sum(1 for r in rows if r[2] == "story")
    print(f"{shared} story settings, {len(rows) - shared} not shared, {len(nosetting)} story names with no setting")


if __name__ == "__main__":
    main()
