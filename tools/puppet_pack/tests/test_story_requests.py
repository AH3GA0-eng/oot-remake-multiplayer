"""Tests for story_requests.py (which placed event request starts a shared story cutscene). Synthetic data only.

    python tools/puppet_pack/tests/test_story_requests.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from puppet_pack import story_requests as sr  # noqa: E402


def request(name):
    return {"Gyaml": "EventRequestActor", "Dynamic": {"RequestEventName": name}}


class StoryRequestsTest(unittest.TestCase):
    def test_story_names_from_the_cpp_table(self):
        text = 'constexpr std::string_view kNames[] = {\n    "Dm_Kokiri_0070", "Meet_DekuTree",\n};\n'
        self.assertEqual(sr.story_names(text), {"Dm_Kokiri_0070", "Meet_DekuTree"})
        with self.assertRaises(ValueError):
            sr.story_names("nothing here")

    def test_settings_of_story_flows_only(self):
        scenes = {
            "KokiriForest_Child": [request("KokiriForest_Meet_DekuTree_MeetDekuTree_01"), request("Dm_Kokiri_0070"),
                                   {"Gyaml": "Obj_Pot_A_01"}],
            "InsideJabuJabuBelly": [request("Get_StoneOfWater_MeetRuto"), request("Get_StoneOfWater_RutoGetStone"),
                                    request("StoneOfWater_ElevatedFloor")],
            "DodongoCavern": [request("Get_StoneOfFire_AppearBoss")],
            "Room": [request("Room_Gimmick_Open")],
        }
        settings = sr.event_settings([
            {"__RowId": "KokiriForest_Meet_DekuTree_MeetDekuTree_01",
             "FlowchartFileName": "Work/Event/EventFlow/Meet_DekuTree.evfc"},
            {"__RowId": "Dm_Kokiri_0070", "FlowchartFileName": "Work/Event/EventFlow/Dm_Kokiri_0070.evfc"},
            {"__RowId": "Get_StoneOfWater_MeetRuto", "FlowchartFileName": "Work/Event/EventFlow/Get_StoneOfWater.evfc"},
            {"__RowId": "Get_StoneOfWater_RutoGetStone",
             "FlowchartFileName": "Work/Event/EventFlow/Get_StoneOfWater.evfc"},
            {"__RowId": "StoneOfWater_ElevatedFloor", "FlowchartFileName": "Work/Event/EventFlow/Get_StoneOfWater.evfc"},
            {"__RowId": "Get_StoneOfFire_AppearBoss", "FlowchartFileName": "Work/Event/EventFlow/Get_StoneOfFire.evfc"},
            {"__RowId": "Room_Gimmick_Open", "FlowchartFileName": "Work/Event/EventFlow/Room_Gimmick.evfc"},
            {"__RowId": "123_456_D_T"},
        ])
        self.assertEqual(settings["Get_StoneOfWater_MeetRuto"], "Get_StoneOfWater")
        self.assertEqual(settings["123_456_D_T"], "")
        story = {"Meet_DekuTree", "Get_StoneOfFire", "Get_StoneOfWater", "Dm_Kokiri_0070", "Meet_GreatFairy"}
        rows = sr.table(sr.requests(scenes), settings, story)
        self.assertEqual(rows, [
            ("InsideJabuJabuBelly", "Get_StoneOfWater_MeetRuto", "Get_StoneOfWater"),
            ("InsideJabuJabuBelly", "Get_StoneOfWater_RutoGetStone", "Get_StoneOfWater"),  # two in one scene
            ("KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"),
        ])  # a story name that is a setting itself, a gimmick, a boss event and a non-story flow are left out
        self.assertEqual(sr.no_setting(story, settings),
                         ["Get_StoneOfFire", "Get_StoneOfWater", "Meet_DekuTree", "Meet_GreatFairy"])

    def test_carries_whole_parts(self):
        self.assertTrue(sr.carries("KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"))
        self.assertTrue(sr.carries("Get_StoneOfFire_MeetDarnia", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("Get_StoneOfFireX_Meet", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("GoronCity_SecretHole_AppearTreasureBox", "Get_StoneOfFire"))

    def test_render(self):
        text = sr.render([("KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree")],
                         ["Meet_GreatFairy"])
        self.assertIn('{"KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"},', text)
        self.assertIn("constexpr StoryRequest kStoryRequests[] = {", text)
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
