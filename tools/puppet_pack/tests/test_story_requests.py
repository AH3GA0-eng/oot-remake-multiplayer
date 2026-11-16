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

    def test_entries_of_story_flows_only(self):
        scenes = {
            "KokiriForest_Child": [request("KokiriForest_Meet_DekuTree_MeetDekuTree_01"), request("Meet_DekuTree"),
                                   {"Gyaml": "Obj_Pot_A_01"}],
            "GoronCity_Child": [request("Get_StoneOfFire_MeetDarnia"), request("Get_StoneOfFire_MeetDarnia_2"),
                                request("GoronCity_SecretHole_AppearTreasureBox")],
            "Room": [request("Room_Gimmick_Open"), request("Shared_Entry")],
        }
        flows = {
            "Meet_DekuTree": b"..KokiriForest_Meet_DekuTree_MeetDekuTree_01..",
            "Get_StoneOfFire": b"Get_StoneOfFire_MeetDarnia Get_StoneOfFire_MeetDarnia_2 "
                               b"GoronCity_SecretHole_AppearTreasureBox",
            "Room_Gimmick": b"Room_Gimmick_Open Shared_Entry",
            "Other": b"Shared_Entry",
        }
        story = {"Meet_DekuTree", "Get_StoneOfFire"}
        rows = sr.table(sr.requests(scenes), flows, story)
        self.assertEqual(rows, [
            ("GoronCity_Child", "Get_StoneOfFire_MeetDarnia", "Get_StoneOfFire"),  # one per scene and flow
            ("KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"),
        ])  # the flow's own name, a gimmick in a story file, a non-story flow and a two-file name are left out

    def test_carries_whole_parts(self):
        self.assertTrue(sr.carries("KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"))
        self.assertTrue(sr.carries("Get_StoneOfFire_MeetDarnia", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("Get_StoneOfFireX_Meet", "Get_StoneOfFire"))
        self.assertFalse(sr.carries("GoronCity_SecretHole_AppearTreasureBox", "Get_StoneOfFire"))

    def test_render(self):
        text = sr.render([("KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree")])
        self.assertIn('{"KokiriForest_Child", "KokiriForest_Meet_DekuTree_MeetDekuTree_01", "Meet_DekuTree"},', text)
        self.assertIn("constexpr StoryRequest kStoryRequests[] = {", text)


if __name__ == "__main__":
    unittest.main()
