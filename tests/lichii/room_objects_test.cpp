// Tests for which room objects their own logic drives (lichii/hook/coop_room_objects.cpp, room-objects).
#include <doctest/doctest.h>

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include "coop_dungeon.h"
#include "coop_room_objects.h"

using namespace CoopRoomObjects;

TEST_CASE("room objects: the generated table is whole and the issues' objects are in it") {
    const auto all = Placements();
    CHECK(all.size() == 132);
    std::size_t driven = 0;
    std::set<std::string> scenes;
    for (const Placement& p : all) {
        driven += p.why.empty() ? 0 : 1;
        scenes.emplace(p.scene);
    }
    CHECK(driven == 24);
    CHECK(scenes.contains("InsideTheDekuTree"));
    CHECK(scenes.contains("InsideJabuJabuBelly"));
    CHECK(scenes.contains("ForestTemple"));
}

TEST_CASE("room objects: switch-, sensor- and rider-driven objects are driven, free movers are not") {
    // #194 / tracker 90: the Deku Tree compass-room lifts rise on their plate's OnState (IsOn).
    const Placement* lift = Nearest("InsideTheDekuTree", "Obj_InsideTheDekuTree_ElevatorLift_A_01", {-25.4f, 118.0f, -1.6f});
    REQUIRE(lift != nullptr);
    CHECK(lift->why == "IsOn");
    // Its limit part has no logic of its own; it is placed on the lift and moves with it.
    CHECK(Driven("InsideTheDekuTree", "Obj_InsideTheDekuTree_ElevatorLiftLim_A_01", {-25.4f, 114.4f, -1.6f}));
    // #249: Jabu-Jabu's moving floor starts on a sensor and its rider plug.
    const Placement* floor = Nearest("InsideJabuJabuBelly", "Obj_InsideJabu_Jabu_MoveFloor_A_01", {0.3f, 95.0f, -42.6f});
    REQUIRE(floor != nullptr);
    CHECK(floor->why == "IsInPlayer+IsTargetFloor+RideOnPlayer");
    // #196: the water room's wave and its floor run free (their phase is the only difference).
    CHECK_FALSE(Driven("InsideTheDekuTree", "GroundControlActor_WaveLoop", {-26.2f, 82.3f, 38.0f}));
    CHECK_FALSE(Driven("InsideTheDekuTree", "Obj_Deku_MoveFloor_A_01", {-28.0f, 75.4f, 37.0f}));
    // A reset-only input (ForceWarp) does not drive a mover.
    CHECK_FALSE(Driven("InsideTheDekuTree", "GroundControlActor_ResidentMoveTo", {-27.5f, 87.5f, 28.1f}));
    // Not in the table: free.
    CHECK_FALSE(Driven("Kokiri", "Obj_InsideTheDekuTree_ElevatorLift_A_01", {0.0f, 0.0f, 0.0f}));
    CHECK(Nearest("InsideTheDekuTree", "Obj_NotAPlatform", {0.0f, 0.0f, 0.0f}) == nullptr);
}

TEST_CASE("room objects: one name, two placements: the nearest decides") {
    // Dodongo's Cavern: of three MoveFloor_A_01, only the shortcut one has a wired input.
    CHECK(Driven("DodongoCavern", "Obj_DodongoCave_MoveFloor_A_01", {10.5f, 99.0f, -23.0f}));
    CHECK_FALSE(Driven("DodongoCavern", "Obj_DodongoCave_MoveFloor_A_01", {-10.5f, 99.0f, -23.0f}));
}

TEST_CASE("room objects: the rider names are the logic types with RideOnPlayer") {
    CHECK(RiderName("Obj_ForestTemple_Elevator_A_01"));
    CHECK(RiderName("Obj_InsideJabu_Jabu_ElevatorLift_A_01"));
    CHECK(RiderName("Obj_InsideJabu_Jabu_ElevatorLift_A_02"));
    CHECK(RiderName("Obj_InsideJabu_Jabu_MoveFloor_A_01"));
    // fable-g3-objects: by the logic, whatever the name (no platform name part in these two).
    CHECK(RiderName("Obj_FireTemple_RiseFloor_A_02"));
    CHECK(RiderName("Obj_InsideGanonCastle_ElevatedFloor_A_01"));
    CHECK_FALSE(RiderName("Obj_FireTemple_RiseFloor_A_01"));  // GimmickOn, no rider plug
    CHECK_FALSE(RiderName("Obj_InsideTheDekuTree_ElevatorLift_A_01"));  // OnPlayer, no RideOnPlayer
    CHECK_FALSE(RiderName("GroundControlActor_WaveLoop"));
}

TEST_CASE("room objects: every dungeon mover group's flag is shared, or left out on purpose (issue 249)") {
    // The flag is the group's only input (its ResidentMoveTo has none in the room graph), so a group whose flag does
    // not reach the other games never moves there: Jabu-Jabu's room 14 water, before this.
    std::set<std::string> shared;
    for (const auto& f : CoopDungeon::Flags()) shared.insert(f.name);
    std::set<std::string> own;
    for (const std::string_view n : CoopDungeon::NotShared()) own.insert(std::string(n));
    std::size_t dungeon_groups = 0;
    for (const CoopRoomObjects::GroupFlag& g : CoopRoomObjects::GroupFlags()) {
        if (!CoopDungeon::IsDungeonScene(g.scene)) continue;  // overworld groups: docs/dungeon.md
        ++dungeon_groups;
        CAPTURE(std::string(g.scene));
        CAPTURE(std::string(g.flag));
        const std::string flag(g.flag);
        CHECK((shared.contains(flag) || own.contains(flag)));
        CHECK(shared.contains(flag) != own.contains(flag));
    }
    CHECK(dungeon_groups >= 30);
    CHECK(shared.contains("Gimmick_Insidejabujabu_Updown_MoveWater"));
    CHECK(shared.contains("Gimmick_Insidejabujabu_down_MoveWater"));
    CHECK(own.contains("InsideTheDekuTree_BossArea_ShutIn"));  // where one player is in the boss fight (issue 148)
    // The Water Temple's level is the one Int.
    std::size_t ints = 0;
    for (const CoopRoomObjects::GroupFlag& g : CoopRoomObjects::GroupFlags()) {
        if (!g.is_int) continue;
        ++ints;
        CHECK(g.flag == "WaterTemple_Room00_WaterLevel");
    }
    CHECK(ints == 4);
}

TEST_CASE("room objects: a switch plug that also raises an event asker is not held by SWITCH_STATE (g3-objects)") {
    // room_objects.py lists 9 such plugs apart (coop_room_objects.cpp, after kSwitchPlugs): holding one would raise
    // the follower's own asker before the presser's played flag arrives (a replay; owner 07:40 UTC 11-17: result only).
    CHECK(CoopRoomObjects::SwitchPlugsOf("InsideJabuJabuBelly", "Obj_InsideJabu_Jabu_StepSwitch_A_03",
                                         {8.5f, 102.5f, -51.4f}).empty());
    CHECK(CoopRoomObjects::SwitchPlugsOf("InsideTheDekuTree", "Obj_StepSwitch_A_02", {-17.5f, 119.6f, -4.9f}).empty());
    CHECK(CoopRoomObjects::SwitchPlugsOf("SpiritTemple", "Obj_SolarSwitch_A_02", {28.0f, 121.5f, -40.1f}).empty());
    CHECK(CoopRoomObjects::SwitchPlugs().size() == 12);
}
