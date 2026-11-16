"""Tests for story_requests.py (which placed event request starts a shared story cutscene). Synthetic data only.

    python tools/puppet_pack/tests/test_story_requests.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from puppet_pack import story_requests as sr  # noqa: E402


def setting(name, flow, entry="", base="StarterPos", comp="FlowchartEvent"):
    return {"__RowId": name, "FlowchartFileName": f"Work/Event/EventFlow/{flow}.evfc", "EntryPointName": entry,
            "EventBaseSetting": f"Work/Event/EventSetting/X/{base}.game__event__GameEventBaseSetting.gyml",
            "EventSettingComponentList": f"Work/Event/EventSetting/Y/{comp}.engine__event__EventSettingComponentList.gyml"}


class StoryRequestsTest(unittest.TestCase):
    def test_story_names_from_the_cpp_table(self):
        text = 'constexpr std::string_view kNames[] = {\n    "Dm_Kokiri_0070", "Meet_DekuTree",\n};\n'
        self.assertEqual(sr.story_names(text), {"Dm_Kokiri_0070", "Meet_DekuTree"})
        with self.assertRaises(ValueError):
            sr.story_names("nothing here")

    def test_every_named_setting_of_a_story_flow_is_classified(self):
        settings = sr.event_settings([
            setting("KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"),
            setting("Dm_Kokiri_0070", "Dm_Kokiri_0070"),                           # a story name: not in the table
            setting("Dm_FireTemple_0020", "Dm_FireTemple_0020"),
            setting("Get_StoneOfWater_ShowLetter", "Get_StoneOfWater"),             # placed or not: shared
            setting("Dm_FireTemple_0015", "Dm_FireTemple_0020", "EntryPoint1"),    # an entry of a Dm_ cutscene
            setting("Get_StoneOfWater_DestroyBoss", "Get_StoneOfWater", comp="BossDie"),
            setting("Get_StoneOfFire_AppearBoss", "Get_StoneOfFire"),
            setting("StoneOfWater_ElevatedFloor", "Get_StoneOfWater"),
            setting("Meet_Darunia_PlayOcarina", "Get_StoneOfFire", comp="NPCTalk"),
            setting("Talk_Deku_01_Attacked", "Get_StoneOfForest", "Talk_Deku_01_Attacked"),
            setting("Get_StoneOfWater_ArriveJabuJabuBelly", "Get_StoneOfWater", base="PlayerPos"),
            setting("Get_StoneOfForest_GetSword", "Get_StoneOfForest", "GetSword"),
            setting("Get_StoneOfWater_RutoGetStone", "Get_StoneOfWater"),          # a stone: shared
            setting("Check_DaruniaDoor", "Get_StoneOfFire"),
            setting("Room_Gimmick_Open", "Room_Gimmick"),                           # not a story flow
            setting("3838149862_1053658113_1523149261_D_T", "Get_StoneOfFire"),    # a talk row id
        ])
        self.assertEqual(settings["Meet_Darunia_PlayOcarina"].components, "NPCTalk")
        self.assertEqual(settings["Get_StoneOfWater_ArriveJabuJabuBelly"].base, "PlayerPos")
        story = {"Meet_DekuTree", "Get_StoneOfFire", "Get_StoneOfWater", "Get_StoneOfForest", "Dm_Kokiri_0070",
                 "Dm_FireTemple_0020", "Meet_GreatFairy"}
        rows = {r[0]: r[2] for r in sr.table(settings, story, {"StoneOfWater_ElevatedFloor"})}
        self.assertEqual(rows, {
            "KokiriForest_Meet_DekuTree_MeetDekuTree_01": "story",
            "Get_StoneOfWater_ShowLetter": "story",
            "Dm_FireTemple_0015": "story",
            "Get_StoneOfWater_RutoGetStone": "story",
            "Get_StoneOfWater_DestroyBoss": "boss",
            "Get_StoneOfFire_AppearBoss": "boss",
            "StoneOfWater_ElevatedFloor": "gimmick",
            "Meet_Darunia_PlayOcarina": "own",
            "Talk_Deku_01_Attacked": "own",
            "Get_StoneOfWater_ArriveJabuJabuBelly": "own",
            "Get_StoneOfForest_GetSword": "own",
            "Check_DaruniaDoor": "never",
        })
        self.assertEqual(sr.no_setting(story, settings),
                         ["Get_StoneOfFire", "Get_StoneOfForest", "Get_StoneOfWater", "Meet_DekuTree",
                          "Meet_GreatFairy"])

    def test_gimmick_names_from_the_hook(self):
        dungeon = '{0xfd1a276a, false, "HavePlayedEvent.StoneOfWater_ElevatedFloor"}, {1, true, "Other.Flag"},'
        places = '{"Dm_FireTemple_0029", "FireTemple", -46.9f, 186.5f, -22.4f},'
        self.assertEqual(sr.gimmick_names(dungeon, places), {"StoneOfWater_ElevatedFloor", "Dm_FireTemple_0029"})

    def test_carries_whole_parts(self):
        self.assertTrue(sr.carries("KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"))
        self.assertTrue(sr.carries("Get_StoneOfFire_MeetDarnia", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("Get_StoneOfFireX_Meet", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("GoronCity_SecretHole_AppearTreasureBox", "Get_StoneOfFire"))

    def test_render(self):
        text = sr.render([("KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree", "story", ""),
                          ("Get_StoneOfForest_GetSword", "Get_StoneOfForest", "own", "an item each player gets")],
                         ["Meet_GreatFairy"])
        self.assertIn('{"KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"},', text)
        self.assertIn("constexpr StorySetting kStorySettings[] = {", text)
        self.assertIn("//   own     Get_StoneOfForest_GetSword (Get_StoneOfForest): an item each player gets", text)
        self.assertIn("constexpr std::string_view kNoSetting[] = {", text)
        self.assertIn('    "Meet_GreatFairy",', text)

    def test_played_flag_is_the_setting_asked(self):
        # The game's own hash of a flag name (coop_shared_scenes.cpp has HavePlayedEvent.Dm_Kokiri_0140 so).
        self.assertEqual(sr.murmur3("HavePlayedEvent.Dm_Kokiri_0140"), 0xEBAEC07F)
        self.assertEqual(sr.played_flag("Get_StoneOfFire_MeetDarnia"), "HavePlayedEvent.Get_StoneOfFire_MeetDarnia")
        names = ["Get_StoneOfFire_MeetDarnia", "Meet_DekuTree"]
        gamedata = bytes(8) + sr.murmur3(sr.played_flag(names[0])).to_bytes(4, "big") + bytes(8)
        self.assertEqual(sr.missing_played_flags(names, gamedata), ["Meet_DekuTree"])
        self.assertEqual(sr.missing_played_flags(names[:1], gamedata), [])


if __name__ == "__main__":
    unittest.main()
