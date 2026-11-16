"""Tests for puppet_pack.boss_beaten (G2). No romfs here: the walk's rules on made-up actions, and the committed
coop_boss_beaten.inc and the tool's lists checked against the C++ they mirror."""
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))

from puppet_pack import boss_beaten as bb  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
HOOK = os.path.join(REPO, "lichii", "hook")


def read(name):
    with open(os.path.join(HOOK, name), encoding="utf-8") as f:
        return f.read()


GOHMA = bb.BOSSES[0]
STEPS = ["NotReady", "Ready", "GoInside", "MeetBoss", "GoToBossRoom", "DefeatBoss", "GoToWarpPoint", "Complete"]
QUESTS = {"Get_StoneOfForest": (0x7d776b61, STEPS, 5)}


def saved(*names, reset=bb.SAVED):
    return {bb.murmur3(n): reset for n in names}


def act(action, **params):
    return ("Get_StoneOfForest", "Actor", action, params)


class Murmur(unittest.TestCase):
    def test_known_hashes(self):
        # coop_dungeon.cpp's table and the game's GameDataList
        self.assertEqual(bb.murmur3("InsideTheDekuTree_Heart_Container_Obtained"), 0x98ca7aab)
        self.assertEqual(bb.murmur3("ForestTemple_SageOfForest_BossAppearance"), 0xd99e718a)


class BeatenSet(unittest.TestCase):
    def actions(self):
        return [
            act("EventTriggerSetGameDataBool", GameDataName="Gohma_DefeatBoss", Value=True, Index=-1),
            act("EventTriggerSetGameDataBool", GameDataName="Gohma_Temporary", Value=True, Index=-1),
            act("EventTriggerSetGameDataBool", GameDataName="InsideTheDekuTree_BossArea_ShutIn", Value=False),
            act("EventTransition", EventName="Dm_Deku_BossOut"),
            act("EventExecuteProgressChallengeStep", ChallengeName="Get_StoneOfForest", StepName="GoToWarpPoint"),
        ]

    def flags(self):
        f = saved("Gohma_DefeatBoss", "InsideTheDekuTree_BossArea_ShutIn", "HavePlayedEvent.InsideTheDekuTree_BossDie",
                  "HavePlayedEvent.Dm_Deku_BossIn", "HavePlayedEvent.Dm_Deku_BossOut")
        f.update(saved("Gohma_Temporary", reset=1))
        return f

    def test_saved_writes_played_events_and_the_step(self):
        s = bb.beaten_set(GOHMA, self.actions(), self.flags(), QUESTS)
        self.assertEqual(s["bools"], [("Gohma_DefeatBoss", True), ("HavePlayedEvent.InsideTheDekuTree_BossDie", True),
                                      ("HavePlayedEvent.Dm_Deku_BossIn", True),
                                      ("HavePlayedEvent.Dm_Deku_BossOut", True)])
        self.assertEqual((s["quest"], s["step"]), ("Get_StoneOfForest", "GoToWarpPoint"))
        self.assertEqual(s["below"], STEPS[:6])  # never Complete: a finished dungeon is not stepped back

    def test_no_boss_out_or_no_step_or_a_wrong_step_is_an_error(self):
        acts = [a for a in self.actions() if a[2] != "EventTransition"]
        with self.assertRaises(ValueError):
            bb.beaten_set(GOHMA, acts, self.flags(), QUESTS)
        acts = [a for a in self.actions() if a[2] != "EventExecuteProgressChallengeStep"]
        with self.assertRaises(ValueError):
            bb.beaten_set(GOHMA, acts, self.flags(), QUESTS)
        acts = self.actions()[:-1] + [act("EventExecuteProgressChallengeStep", ChallengeName="Get_StoneOfForest",
                                          StepName="DefeatBoss")]
        with self.assertRaises(ValueError):
            bb.beaten_set(GOHMA, acts, self.flags(), QUESTS)

    def test_normal_world_answers(self):
        self.assertEqual(bb.normal_world("f", "GameSystemActor", "EventQueryIsDreamWorld", {}), 1)
        self.assertEqual(bb.normal_world("f", "GameDataActor", "EventQueryIsGameDataBool",
                                         {"GameDataName": "DreamWorld_BossRush"}), 1)
        self.assertIsNone(bb.normal_world("f", "Player_Event", "EventQueryPlayerIsChild", {}))


class AgainstTheCode(unittest.TestCase):
    def test_bosses_match_coop_boss_scenes(self):
        text = read("coop_boss_scenes.cpp")
        for room, name, death, boss_in, boss_out in bb.BOSSES:
            m = re.search(r'\{"' + re.escape(name) + r'", "(\w+)", "(\w+)", (-?\d+), \{[^}]*\},\s*\{"(\w+)"', text)
            self.assertIsNotNone(m, name)
            self.assertEqual(m.groups(), (boss_in, boss_out, str(room), death), name)

    def test_not_shared_matches_coop_dungeon(self):
        text = read("coop_dungeon.cpp")
        block = text[text.index("kNotShared{"):]
        names = set(re.findall(r'"(\w+)"', block[:block.index("};")]))
        self.assertEqual(names, bb.NOT_SHARED)

    def test_the_committed_table_hashes_its_names(self):
        text = read("coop_boss_beaten.inc")
        rows = re.findall(r"\{0x([0-9a-f]{8})u, (?:true|false), \"([\w.]+)\"\}", text)
        self.assertGreater(len(rows), 8 * 3)
        for h, name in rows:
            self.assertEqual(int(h, 16), bb.murmur3(name), name)
        sets = re.findall(r"\{(\d), \"[^\"]+\", 0x[0-9a-f]{8}u, \"\w+\", 0x([0-9a-f]{8})u, \"(\w+)\", kBelow\1, "
                          r"kBools\1\}", text)
        self.assertEqual([int(r) for r, _, _ in sets], list(range(8)))
        for _, h, step in sets:
            self.assertEqual(int(h, 16), bb.murmur3(step), step)


if __name__ == "__main__":
    unittest.main()
