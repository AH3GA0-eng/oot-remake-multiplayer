#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "coop/overlay/map_icons.hpp"

using namespace coop::overlay::map;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// A quaternion turning about +Y by `a` radians.
Quat yaw(float a) { return {0.0f, std::sin(a / 2), 0.0f, std::cos(a / 2)}; }

Player at(std::uint8_t n, float x, float y, float z, std::string scene = "HyruleField") {
  Player p;
  p.number = n;
  p.position = {x, y, z};
  p.scene = std::move(scene);
  p.rgb = 0x102030u * n;
  return p;
}

const char* kText =
    "# made by map_captures.py\n"
    "map KokiriForest -5 -12 120 300 0 KokiriForest_Child,KokiriForest_Adult -95:- -20:-\n"
    "map HyruleField -23 197 200 736 0 HyruleField_Child,HyruleField_Adult\n"
    "map InsideJabuJabuBelly -1 -92 200 328 1 InsideJabuJabuBelly -:- 93:105 128:133\n"
    "map HyruleCastle 0 -80 200 242 0 HyruleCastle,GanonCastle\n"
    "map GanonCastle 61 297 200 268 1 GanonCastle 55:216\n";

// A pane at (x, y) in the layout's units from its middle (y up), scaled by `k` and turned by `turn` radians.
Pane pane(float w, float h, float x = 0, float y = 0, float k = 1, float turn = 0) {
  Pane p;
  p.w = w;
  p.h = h;
  p.m = {k * std::cos(turn), k * std::sin(turn), 0, -k * std::sin(turn), k * std::cos(turn), 0, 0, 0, 1, x, y, 0};
  return p;
}

// A 100 m map of "Room"; its picture 1000 x 1000 in the layout's middle: 10 layout units a metre.
// The window 400 x 400 over it.
Capture room() {
  Capture c;
  c.location = "Room";
  c.scenes = {"Room_Child"};
  c.height = 100;
  c.floors.push_back({});
  return c;
}
MinimapPanes panes() {
  MinimapPanes mm;
  mm.picture = pane(1000, 1000);
  mm.window = pane(400, 400);
  return mm;
}

}  // namespace


TEST_CASE("coop_maps.txt lines parse, bad ones are counted and skipped") {
  int bad = -1;
  const auto caps = parse_captures(std::string(kText) +
                                       "map Broken 1 2 3\n"                 // too short
                                       "map Zero 0 0 200 0 0 Zero\n"         // no height
                                       "map BadFloor 0 0 200 10 1 X 3\n"     // floor without ':'
                                       "map NaN nan 0 200 10 0 X\n"          // not finite
                                       "nope 1 2 3 4 5 6 7\n",
                                   &bad);
  CHECK(bad == 5);
  REQUIRE(caps.size() == 5);
  CHECK(caps[0].location == "KokiriForest");
  CHECK(caps[0].scenes == std::vector<std::string>{"KokiriForest_Child", "KokiriForest_Adult"});
  CHECK(caps[0].height == 300.0f);
  CHECK_FALSE(caps[0].dungeon);
  REQUIRE(caps[0].floors.size() == 2);
  CHECK(*caps[0].floors[0].near_m == -95.0f);
  CHECK_FALSE(caps[0].floors[0].far_m);
  CHECK(caps[1].floors.size() == 1);  // none listed: one uncut floor
  CHECK(caps[2].dungeon);
  REQUIRE(caps[2].floors.size() == 3);
  CHECK_FALSE(caps[2].floors[0].near_m);
  CHECK(*caps[2].floors[2].far_m == 133.0f);
}

TEST_CASE("a scene finds its map by location, scene name, or without its era") {
  const Table t(parse_captures(kText));
  REQUIRE(t.size() == 5);
  CHECK(t.find("GanonCastle")->dungeon);  // its own map, not HyruleCastle's that lists it
  CHECK(t.find("HyruleField")->location == "HyruleField");
  CHECK(t.find("KokiriForest_Adult")->location == "KokiriForest");
  CHECK(t.find("HyruleField_Child")->location == "HyruleField");
  CHECK(t.find("InsideJabuJabuBelly")->dungeon);
  CHECK(t.find("LakeHylia") == nullptr);
  CHECK(t.find("") == nullptr);
  CHECK(Table().find("HyruleField") == nullptr);
}

TEST_CASE("facing: +Z is down the map, +X right") {
  CHECK(map_angle(yaw(0)).value() == doctest::Approx(kPi / 2));        // looks +Z: down
  CHECK(map_angle(yaw(kPi / 2)).value() == doctest::Approx(0.0f));     // +X: right
  CHECK(std::fabs(map_angle(yaw(kPi)).value()) == doctest::Approx(kPi / 2));  // -Z: up
  CHECK(map_angle(yaw(-kPi / 2)).value() == doctest::Approx(kPi).epsilon(1e-4));  // -X: left
  // Looking straight up (turned 90 degrees about X): no facing on a map.
  CHECK_FALSE(map_angle({std::sin(-kPi / 4), 0, 0, std::cos(-kPi / 4)}));
  CHECK_FALSE(map_angle({kNaN, 0, 0, 1}));
}


TEST_CASE("pane: a point in its own units on the layout, and what is not shown") {
  const Pane p = pane(100, 50, 1536, -690, 2);
  const auto [x, y] = pane_point(p, 10, 5);
  CHECK(x == doctest::Approx(1920 + 1536 + 20));
  CHECK(y == doctest::Approx(1080 + 690 - 10));
  CHECK(pane_usable(p));
  CHECK_FALSE(pane_usable(Pane{}));       // a pane never placed: all 0
  CHECK_FALSE(pane_usable(pane(0, 50)));  // no size
  Pane nan = p;
  nan.m[9] = kNaN;
  CHECK_FALSE(pane_usable(nan));
}

TEST_CASE("minimap: everyone at their own spot on the game's picture, in the map's places only") {
  std::vector<Icon> out;
  std::vector<Player> ps{at(2, 10, 0, -5, "Room_Child"), at(3, 0, 5, 12, "Room"), at(4, 0, 0, 0, "LakeHylia"),
                         at(5, 1, 1, 1, "Room")};
  ps[0].rotation = yaw(kPi / 2);
  ps[3].scene.reset();
  minimap_icons(panes(), room(), 0, ps, out);
  REQUIRE(out.size() == 2);
  CHECK(out[0].number == 2);
  CHECK(out[0].x == doctest::Approx(2020));  // 10 m east: 100 right of the middle
  CHECK(out[0].y == doctest::Approx(1030));  // 5 m north: up
  CHECK(out[0].angle.value() == doctest::Approx(0.0f));
  CHECK_FALSE(out[0].clamped);
  CHECK(out[0].rgb == 0x204060u);
  CHECK(out[1].x == doctest::Approx(1920));
  CHECK(out[1].y == doctest::Approx(1200));
  CHECK_FALSE(out[1].angle);  // no rotation sent: a dot
}

TEST_CASE("minimap: a player standing still keeps their spot while we move, and moves only with the picture") {
  std::vector<Icon> out;
  const std::vector<Player> them{at(2, 10, 0, -5, "Room")};
  MinimapPanes mm = panes();
  minimap_icons(mm, room(), 0, them, out);  // (our own place is not an input at all)
  REQUIRE(out.size() == 1);
  const float x0 = out[0].x, y0 = out[0].y;
  // The game scrolls its picture 30 right and 20 down (its y is up: -20): the icon goes with it, exactly.
  mm.picture.m[9] += 30;
  mm.picture.m[10] -= 20;
  minimap_icons(mm, room(), 0, them, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].x == doctest::Approx(x0 + 30));
  CHECK(out[0].y == doctest::Approx(y0 + 20));
  // Zoomed in twice: twice as far from the picture's centre.
  mm.picture = pane(1000, 1000, 0, 0, 2);
  minimap_icons(mm, room(), 0, them, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].x == doctest::Approx(1920 + 200));
  CHECK(out[0].y == doctest::Approx(1080 - 100));
}

TEST_CASE("minimap: a turned picture turns the spots and the facings with it") {
  std::vector<Icon> out;
  MinimapPanes mm = panes();
  mm.picture = pane(1000, 1000, 0, 0, 1, kPi / 2);  // a quarter turn left: east is up
  auto east = at(2, 10, 0, 0, "Room");
  east.rotation = yaw(kPi / 2);  // facing east (+X)
  minimap_icons(mm, room(), 0, {east}, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].x == doctest::Approx(1920));
  CHECK(out[0].y == doctest::Approx(980));                   // up
  CHECK(out[0].angle.value() == doctest::Approx(-kPi / 2));  // pointing up
}

TEST_CASE("minimap: past the window, held at its edge on the line from its centre") {
  std::vector<Icon> out;
  MinimapPanes mm = panes();
  mm.edge_inset = 10;
  minimap_icons(mm, room(), 0, {at(2, 40, 0, 10, "Room"), at(3, 0, 0, 15, "Room")}, out);
  REQUIRE(out.size() == 2);
  CHECK(out[0].clamped);
  CHECK(out[0].x == doctest::Approx(1920 + 190));    // 400 right: the edge less the inset
  CHECK(out[0].y == doctest::Approx(1080 + 47.5f));  // 400:100 scaled to 190:47.5
  CHECK_FALSE(out[1].clamped);                       // 150 down: inside
  CHECK(out[1].y == doctest::Approx(1230));
  // The window drawn at half size: the inset stays in layout units.
  mm.window = pane(400, 400, 0, 0, 0.5f);
  minimap_icons(mm, room(), 0, {at(2, 40, 0, 0, "Room")}, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].clamped);
  CHECK(out[0].x == doctest::Approx(1920 + 90));
}

TEST_CASE("minimap: panes the game does not show, or bad numbers, draw nothing, and the list is reused") {
  std::vector<Icon> out;
  out.reserve(16);
  const auto* before = out.data();
  MinimapPanes mm = panes();
  mm.picture = Pane{};
  minimap_icons(mm, room(), 0, {at(2, 1, 0, 1, "Room")}, out);
  CHECK(out.empty());
  mm = panes();
  mm.window.w = 0;
  minimap_icons(mm, room(), 0, {at(2, 1, 0, 1, "Room")}, out);
  CHECK(out.empty());
  Capture flat = room();
  flat.height = 0;
  minimap_icons(panes(), flat, 0, {at(2, 1, 0, 1, "Room")}, out);
  CHECK(out.empty());
  std::vector<Player> sixteen;
  for (int i = 1; i <= 16; ++i) sixteen.push_back(at(static_cast<std::uint8_t>(i), 1.0f * i, 0, 0, "Room"));
  sixteen[3].position[0] = kNaN;
  minimap_icons(panes(), room(), 0, sixteen, out);
  CHECK(out.size() == 15);
  CHECK(out.data() == before);  // no new allocation
}

TEST_CASE("minimap: the fade carries over and is kept in 0..1") {
  std::vector<Icon> out;
  auto p = at(2, 1, 0, 1, "Room");
  p.fade = 0.25f;
  auto q = at(3, 1, 0, 1, "Room");
  q.fade = 7.0f;
  auto r = at(4, 1, 0, 1, "Room");
  r.fade = kNaN;
  minimap_icons(panes(), room(), 0, {p, q, r}, out);
  REQUIRE(out.size() == 3);
  CHECK(out[0].alpha == doctest::Approx(0.25f));
  CHECK(out[1].alpha == 1.0f);
  CHECK(out[2].alpha == 0.0f);
}

TEST_CASE("minimap: each map by its own capture (field, town, dungeon): its centre at the picture's centre") {
  const Table t(parse_captures(kText));
  std::vector<Icon> out;
  for (const char* name : {"HyruleField", "KokiriForest", "HyruleCastle", "InsideJabuJabuBelly"}) {
    const Capture& c = *t.find(name);
    MinimapPanes mm;
    mm.picture = pane(1300, 1300, 1536, -690, 0.5f);
    mm.window = pane(552, 552, 1536, -690);
    minimap_icons(mm, c, 0, {at(2, c.centre_x, 100, c.centre_z, name)}, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].x == doctest::Approx(1920 + 1536));
    CHECK(out[0].y == doctest::Approx(1080 + 690));
    // A tenth of the map's height north: a tenth of the drawn picture (650) up.
    minimap_icons(mm, c, 0, {at(2, c.centre_x, 100, c.centre_z - c.height / 10, name)}, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].y == doctest::Approx(1080 + 690 - 65));
  }
}

TEST_CASE("picture: the capture's centre is the picture's centre, its height the picture's height") {
  const Table t(parse_captures(kText));
  const Capture& field = *t.find("HyruleField");
  const Box pic{100, 50, 1000, 500};  // 2:1, so 1472 m across
  auto c = to_picture(pic, field, {-23, 0, 197});
  CHECK(c[0] == doctest::Approx(600));
  CHECK(c[1] == doctest::Approx(300));
  c = to_picture(pic, field, {-23 + 368, 0, 197 - 184});  // a quarter across right, a quarter up
  CHECK(c[0] == doctest::Approx(850));
  CHECK(c[1] == doctest::Approx(175));

  std::vector<Icon> out;
  picture_icons(pic, field, 0,
                {at(1, -23, 0, 197, "HyruleField_Adult"), at(2, 5000, 0, 197), at(3, 0, 0, 0, "LakeHylia")}, out);
  REQUIRE(out.size() == 2);
  CHECK(out[0].x == doctest::Approx(600));
  CHECK_FALSE(out[0].clamped);
  CHECK(out[1].clamped);
  CHECK(out[1].x == doctest::Approx(1100));  // the right edge
  CHECK(out[1].floor_delta == 0);
}

TEST_CASE("dungeon floors: the floor whose cut is nearest above the player") {
  const Table t(parse_captures(kText));
  const Capture& jabu = *t.find("InsideJabuJabuBelly");
  // Camera at 200: floor 1 holds 95..107, floor 2 holds 67..72, floor 0 everything.
  CHECK(floor_of(jabu, 100) == 1);
  CHECK(floor_of(jabu, 68) == 2);
  CHECK(floor_of(jabu, 150) == 0);
  CHECK(floor_of(jabu, kNaN) == 0);
  std::vector<Icon> out;
  picture_icons({0, 0, 400, 400}, jabu, 1, {at(1, -1, 100, -92, "InsideJabuJabuBelly"),
                                            at(2, -1, 68, -92, "InsideJabuJabuBelly")}, out);
  REQUIRE(out.size() == 2);
  CHECK(out[0].floor == 1);
  CHECK(out[0].floor_delta == 0);
  CHECK(out[1].floor == 2);
  CHECK(out[1].floor_delta == 1);
}

TEST_CASE("minimap in a dungeon: a player on another floor is marked by how many floors from the one shown") {
  const Table t(parse_captures(kText));
  const Capture& jabu = *t.find("InsideJabuJabuBelly");
  std::vector<Icon> out;
  std::vector<Player> others{at(2, 1, 100, 0, "InsideJabuJabuBelly"), at(3, 1, 68, 0, "InsideJabuJabuBelly")};
  minimap_icons(panes(), jabu, floor_of(jabu, 100), others, out);
  REQUIRE(out.size() == 2);
  CHECK(out[0].floor_delta == 0);
  CHECK(out[1].floor == 2);
  CHECK(out[1].floor_delta == 1);
  // An area: no floors counted.
  const Capture& field = *t.find("HyruleField");
  others = {at(2, 1, 100, 0, "HyruleField"), at(3, 1, 68, 0, "HyruleField")};
  minimap_icons(panes(), field, 0, others, out);
  REQUIRE(out.size() == 2);
  CHECK(out[1].floor_delta == 0);
}
