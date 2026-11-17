// Tests for which room objects their own logic drives (lichii/hook/coop_room_objects.cpp, room-objects).
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include "coop/protocol/codec.hpp"
#include "coop_dungeon.h"
#include "coop_eye_switch.h"
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

TEST_CASE("room objects: a switch plug that also raises an event asker is held only behind its asker's guard (g3-objects)") {
    // room_objects.py: such a plug (a CoopEyeSwitch line the switch's AI rewrites, so it is not driven) is a line only
    // when shared flags stop EVERY asker it raises; the hold waits for them (owner 07:40 UTC 11-17: result only).
    std::set<std::string> shared;
    for (const auto& f : CoopDungeon::Flags()) shared.insert(f.name);
    struct Want {
        std::string_view scene, name;
        std::array<float, 3> at;
        std::string_view plug, guard, event;
    };
    const Want held[] = {
        // a step switch's OnState: Jabu-Jabu room 1's shutters (the trigger reads the flag negated)
        {"InsideJabuJabuBelly", "Obj_InsideJabu_Jabu_StepSwitch_A_03", {8.5f, 102.5f, -51.4f}, "OnState",
         "InsideJabuJabuBelly_Gimmick_Onswitch_first", "InsideJabuJabuBellyRoom01StepSwitch"},
        // a step switch's OnState: the Deku Tree room D lifts (the flag deletes the asker)
        {"InsideTheDekuTree", "Obj_StepSwitch_A_02", {-17.5f, 119.6f, -4.9f}, "OnState",
         "InsideTheDekuTree_RoomD_StepSwitch_On_01", "InsideTheDekuTree_Gimmick_RoomD_PlatformUp"},
        // a step switch's OnState read by a chest: GTG room 5
        {"GerudoTrainingGround", "Obj_StepSwitch_A_04", {52.2f, 97.5f, -86.0f}, "OnState",
         "GerudoTrainingGround_Dungeon_Room05_StepSwitch", "GerudoTrainingGround_05_DisappearFire"},
        // a sun switch's isLightMax: Spirit room 19's mirror (this asker of the event is deleted by the flag)
        {"SpiritTemple", "Obj_SolarSwitch_A_02", {45.8f, 121.5f, -47.5f}, "isLightMax",
         "SpiritTemple_Room19_LightSwitch_On_01", "SpiritTemple_Level_Room_19_SwitchOn_01"},
    };
    for (const Want& w : held) {
        CAPTURE(w.name);
        const auto lines = SwitchPlugsOf(w.scene, w.name, w.at);
        REQUIRE(lines.size() == 1);
        CHECK(lines[0]->plug == w.plug);
        CHECK(lines[0]->guard == w.guard);
        const auto guards = GuardFlags(*lines[0]);
        REQUIRE(guards.size() == 1);
        CHECK(guards[0] == w.guard);
        CHECK(shared.contains(std::string(w.guard)));  // the presser's flag reaches us live
        // the same flag is a guard of that event in the eye-switch table (the tool reads the same wiring)
        bool listed = false;
        for (const CoopEyeSwitch::Guard& g : CoopEyeSwitch::Guards()) {
            if (g.flag == w.guard && g.scene == w.scene && g.event == w.event) listed = true;
        }
        CHECK(listed);
        // not held in the dungeon (coop_dungeon HeldIn): else the hold would never be armed while we are there
        CHECK_FALSE(CoopDungeon::HeldIn(coop::protocol::flag_hash(w.guard)).has_value());
    }
    // A hammer or hit switch's OnState and Spirit room 19's other two suns: no shared flag stops their askers, so they
    // are never held; the event's result flag turns the follower's switch on through its own ON input (ForceControl,
    // isCleared), the native path. That flag must be shared.
    const Want not_held[] = {
        {"SpiritTemple", "Obj_HammerSwitch_A_01", {18.0f, 129.1f, -68.5f}, "OnState", "SpiritTemple_Room08_Unlocked_01", ""},
        {"SpiritTemple", "Obj_SwitchHit_A_02", {33.6f, 111.8f, -30.7f}, "OnState", "SpiritTemple_Room14_Unlocked_01", ""},
        {"SpiritTemple", "Obj_SwitchHit_A_05", {0.0f, 153.1f, -46.0f}, "OnState", "SpiritTemple_Room31_Unlocked", ""},
        {"SpiritTemple", "Obj_SolarSwitch_A_02", {28.0f, 121.5f, -40.1f}, "isLightMax", "SpiritTemple_Room19_LightSwitch_On_02", ""},
        {"SpiritTemple", "Obj_SolarSwitch_A_02", {35.4f, 121.5f, -37.2f}, "isLightMax", "SpiritTemple_Room19_LightSwitch_On_03", ""},
    };
    for (const Want& w : not_held) {
        CAPTURE(w.at[0]);
        CHECK(SwitchPlugsOf(w.scene, w.name, w.at).empty());
        CHECK(shared.contains(std::string(w.guard)));
    }
    // Every guarded line is one of the four; the timed switches keep no guard.
    std::size_t guarded = 0;
    for (const SwitchPlug& s : SwitchPlugs()) {
        if (!s.guard.empty()) ++guarded;
    }
    CHECK(guarded == 4);
    CHECK(SwitchPlugs().size() == 16);
    CHECK(GuardFlags(SwitchPlug{"S", "N", {0.0f, 0.0f, 0.0f}, "OnState", ""}).empty());
    const auto two = GuardFlags(SwitchPlug{"S", "N", {0.0f, 0.0f, 0.0f}, "OnState", "A+B"});
    REQUIRE(two.size() == 2);
    CHECK(two[0] == "A");
    CHECK(two[1] == "B");
}
