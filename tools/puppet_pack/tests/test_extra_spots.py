"""Tests for extra_spots.py (where difficulty's extra enemies may stand). Synthetic data only.

    python tools/puppet_pack/tests/test_extra_spots.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from puppet_pack import extra_spots as xs  # noqa: E402


def actor(gyaml, h, x, y, z):
    return {"Gyaml": gyaml, "Hash": h, "Translate": [x, y, z]}


class SpotsTest(unittest.TestCase):
    def test_markers_near_enemies_only(self):
        scenes = {
            "Room": [
                actor("Enemy_Keese", 2, 0.0, 0.0, 0.0),
                actor("Enemy_Moblin", 1, 20.0, 0.0, 0.0),
                actor("Enemy_Amos_Dictionary", 3, 0.0, 0.0, 0.0),  # a picture-book stand-in: not an enemy
                actor("DestinationAnchor", 10, 3.0, 0.5, 0.0),        # near the Keese, same floor
                actor("WarpSafePosAnchor", 11, 23.0, 0.0, 0.0),       # near the Moblin
                actor("Obj_Plant_Grass_A_01", 12, 0.0, 0.0, 7.0),     # within reach
                actor("StartPos", 13, 0.0, 2.0, 3.0),                 # another floor (2 m up)
                actor("DestinationAnchor", 14, 50.0, 0.0, 0.0),       # near no enemy
                actor("Obj_Pot_A_01", 15, 1.0, 0.0, 1.0),             # not a floor marker
            ],
            "Empty": [actor("DestinationAnchor", 20, 0.0, 0.0, 0.0)],
        }
        found = xs.areas(scenes)
        self.assertEqual([a[0] for a in found], ["Room"])  # an area with no enemies is left out
        _, enemies, ground = found[0]
        self.assertEqual([e[0] for e in enemies], [1, 2])  # in placement id order
        self.assertEqual([m[0] for m in ground], sorted([(3.0, 0.5, 0.0), (23.0, 0.0, 0.0), (0.0, 0.0, 7.0)]))

    def test_door_facing(self):
        door = {"Gyaml": "DestinationAnchor", "Rotate": [3.1415927, 1.2217305, 3.1415927]}
        fx, fz = xs.facing(door)
        self.assertAlmostEqual(fx, 0.9397, places=3)  # the other landing of that door, (0, -1.22, 0), faces back
        self.assertAlmostEqual(fz, -0.3420, places=3)
        bx, bz = xs.facing({"Gyaml": "DestinationAnchor", "Rotate": [0.0, -1.2217305, 0.0]})
        self.assertAlmostEqual(bx, -fx, places=3)
        self.assertAlmostEqual(bz, -fz, places=3)
        self.assertEqual(xs.facing({"Gyaml": "DestinationAnchor"}), (0.0, 1.0))  # no Rotate: +z
        self.assertEqual(xs.facing({"Gyaml": "Obj_Plant_Grass_A_01", "Rotate": [0.0, 1.0, 0.0]}), (0.0, 0.0))

    def test_render(self):
        found = xs.areas({"Room": [actor("Enemy_Keese", 2, 0.0, 0.0, 0.0),
                                   actor("DestinationAnchor", 10, 3.0, 0.0, 0.0)]})
        text = xs.render(found)
        self.assertIn('{0x0000000000000002ull, "Enemy_Keese", {0.000f, 0.000f, 0.000f}},', text)
        self.assertIn("{3.000f, 0.000f, 0.000f, 0.000f, 1.000f},", text)  # a door landing with no Rotate faces +z
        self.assertIn('{"Room", 0, 1, 0, 1},', text)


if __name__ == "__main__":
    unittest.main()
