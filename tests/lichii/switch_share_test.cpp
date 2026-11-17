// Tests for the timed switches' presses (lichii/hook/coop_switch_share.cpp, room-objects).
#include <doctest/doctest.h>

#include <array>
#include <string>
#include <vector>

#include "coop_room_objects.h"
#include "coop_switch_share.h"

using namespace CoopSwitchShare;
namespace proto = coop::protocol;

namespace {

constexpr const char* kScene = "DodongoCavern";

// Dodongo's Cavern's moving-ground plate: Obj_StepSwitch_A_01 OnState through a timer (GroundControlActor_SimpleMoveTo).
const CoopRoomObjects::SwitchPlug* Plate() {
    const auto lines = CoopRoomObjects::SwitchPlugsOf(kScene, "Obj_StepSwitch_A_01", {17.2f, 112.3f, -23.0f});
    REQUIRE(lines.size() == 1);
    return lines[0];
}

struct Game {
    Sharer sharer;
    std::uint8_t self = 1;
    bool on = false;  // our switch's plug as the game has it
    std::vector<proto::SwitchState> sent;
    std::vector<Hold> holds;
    std::vector<std::string> notes;

    void Tick(std::uint64_t now_ms, bool others = true, const std::string& scene = kScene) {
        std::vector<proto::SwitchState> out;
        holds = sharer.Tick(scene, {Seen{Plate(), 0x1000, on, true}}, others, now_ms, out, notes);
        for (auto& m : out) {
            m.player_number = self;  // the server's relay writes the sender
            sent.push_back(m);
        }
    }
};

bool Has(const std::vector<std::string>& notes, const char* text) {
    for (const auto& n : notes) {
        if (n.find(text) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("switch share: the generated lines are the timed switches no CoopEyeSwitch line covers") {
    CHECK(CoopRoomObjects::SwitchPlugs().size() == 16);  // 12 timed + 4 behind an asker guard (g3-objects)
    CHECK(Plate()->plug == "OnState");
    // The Water Temple fountain reads one switch's OffState.
    CHECK(CoopRoomObjects::SwitchPlugsOf("WaterTemple", "Obj_SwitchHit_A_06", {52.1f, 110.0f, -23.2f}).size() == 1);
    // A switch elsewhere, or another scene: no line.
    CHECK(CoopRoomObjects::SwitchPlugsOf(kScene, "Obj_StepSwitch_A_01", {0.0f, 0.0f, 0.0f}).empty());
    CHECK(CoopRoomObjects::SwitchPlugsOf("ForestTemple", "Obj_StepSwitch_A_01", {17.2f, 112.3f, -23.0f}).empty());
    CHECK(CoopRoomObjects::SharedSwitchName("Obj_SwitchHit_A_06"));
    CHECK_FALSE(CoopRoomObjects::SharedSwitchName("Obj_DiveSwitch_A_01"));  // a CoopEyeSwitch line only
}

TEST_CASE("switch share: our press is sent, held on the other game's switch while on, let go when off") {
    Game a;
    Game b;
    b.self = 2;
    std::uint64_t t = 1000;
    a.Tick(t);
    b.Tick(t);
    CHECK(a.sent.empty());
    // A's player steps on the plate.
    a.on = true;
    a.Tick(t += 100);
    REQUIRE(a.sent.size() == 1);
    CHECK(a.sent[0].on);
    CHECK(a.sent[0].name == "Obj_StepSwitch_A_01");
    CHECK(a.sent[0].plug == PlugKey("OnState"));
    CHECK(Has(a.notes, "our player's press: sent"));
    b.sharer.Receive(a.sent[0], t);
    b.Tick(t += 50);
    REQUIRE(b.holds.size() == 1);
    CHECK(b.holds[0] == Hold{0x1000, PlugKey("OnState")});
    CHECK(Has(b.notes, "another player's is on"));
    // Our set made B's plug 1: still held (nothing to set while 1), and never sent back.
    b.on = true;
    for (int i = 0; i < 30; ++i) b.Tick(t += 100);
    CHECK(b.sent.empty());
    // A sends again while on (a later player gets it).
    for (int i = 0; i < 25; ++i) a.Tick(t += 100);
    CHECK(a.sent.size() >= 2);
    // A's plate goes off: off sent, B lets go; B's own 0 comes back and B sends nothing.
    a.on = false;
    a.Tick(t += 100);
    REQUIRE(!a.sent.empty());
    CHECK_FALSE(a.sent.back().on);
    b.sharer.Receive(a.sent.back(), t);
    b.Tick(t += 50);
    CHECK(b.holds.empty());
    CHECK(Has(b.notes, "let go after"));
    CHECK(Has(a.notes, "our player's press ended after"));
    b.on = false;
    for (int i = 0; i < 20; ++i) b.Tick(t += 100);
    CHECK(b.sent.empty());
}

TEST_CASE("switch share: our plug still 1 just after we held it is not a press of ours (echo)") {
    Game b;
    b.self = 2;
    std::uint64_t t = 1000;
    proto::SwitchState on;
    on.player_number = 1;
    on.scene = kScene;
    on.name = "Obj_StepSwitch_A_01";
    on.place = {17.2f, 112.3f, -23.0f};
    on.plug = PlugKey("OnState");
    on.on = true;
    b.sharer.Receive(on, t);
    b.on = true;
    b.Tick(t += 50);
    auto off = on;
    off.on = false;
    b.sharer.Receive(off, t);
    b.Tick(t += 50);  // our plug says 1 for a moment more
    b.Tick(t += 300);
    CHECK(b.sent.empty());
    // Later our own player presses it: that is ours.
    b.on = false;
    b.Tick(t += kEchoMs);
    b.on = true;
    b.Tick(t += 100);
    CHECK(b.sent.size() == 1);
}

TEST_CASE("switch share: solo sends nothing and holds nothing, and a remote on not sent again is let go") {
    Game a;
    std::uint64_t t = 1000;
    a.on = true;
    for (int i = 0; i < 30; ++i) a.Tick(t += 100, false);
    CHECK(a.sent.empty());
    CHECK(a.holds.empty());
    Game b;
    b.self = 2;
    proto::SwitchState on;
    on.player_number = 1;
    on.scene = kScene;
    on.name = "Obj_StepSwitch_A_01";
    on.place = {17.2f, 112.3f, -23.0f};
    on.plug = PlugKey("OnState");
    on.on = true;
    b.sharer.Receive(on, t);
    b.Tick(t += 100);
    CHECK(b.holds.size() == 1);
    b.Tick(t += kRemoteHoldMs + 100);  // the sender left without an off
    CHECK(b.holds.empty());
    // Another plug key or a far place is no line of ours.
    auto other = on;
    other.plug = PlugKey("FirstOn");
    b.sharer.Receive(other, t);
    other = on;
    other.place = {0.0f, 0.0f, 0.0f};
    b.sharer.Receive(other, t);
    b.Tick(t += 100);
    CHECK(b.holds.empty());
}

TEST_CASE("switch share: a player who leaves is let go at once, not after the timeout") {
    Game b;
    b.self = 2;
    std::uint64_t t = 1000;
    proto::SwitchState on;
    on.player_number = 1;
    on.scene = kScene;
    on.name = "Obj_StepSwitch_A_01";
    on.place = {17.2f, 112.3f, -23.0f};
    on.plug = PlugKey("OnState");
    on.on = true;
    b.sharer.Receive(on, t);
    auto three = on;
    three.player_number = 3;
    b.sharer.Receive(three, t);
    b.Tick(t += 100);
    REQUIRE(b.holds.size() == 1);
    b.sharer.Forget(1);
    b.Tick(t += 100);
    CHECK(b.holds.size() == 1);  // player 3's is still on
    b.sharer.Forget(3);
    b.Tick(t += 100);  // well before kRemoteHoldMs
    CHECK(b.holds.empty());
    CHECK(Has(b.notes, "let go after"));
}

TEST_CASE("switch share: a new scene forgets what was held") {
    Game b;
    b.self = 2;
    std::uint64_t t = 1000;
    proto::SwitchState on;
    on.player_number = 1;
    on.scene = kScene;
    on.name = "Obj_StepSwitch_A_01";
    on.place = {17.2f, 112.3f, -23.0f};
    on.plug = PlugKey("OnState");
    on.on = true;
    b.Tick(t);
    b.sharer.Receive(on, t);
    b.Tick(t += 100, true, "KokiriForest");
    b.Tick(t += 100);
    CHECK(b.holds.empty());
}

TEST_CASE("switch share: a plug that also raises our event asker is held only once its guard flags are on here") {
    // g3-objects, one line per class: a step switch's OnState read by shutters (Jabu-Jabu room 1), by lifts (Deku Tree
    // room D) and by a chest (GTG room 5), and a sun switch's isLightMax (Spirit room 19). Before the presser's guard
    // flag reaches our game our own asker could still fire on the plug (a replay): no hold, and a note.
    struct Line {
        const char* scene;
        const char* name;
        std::array<float, 3> at;
        const char* plug;
    };
    const Line lines[] = {
        {"InsideJabuJabuBelly", "Obj_InsideJabu_Jabu_StepSwitch_A_03", {8.5f, 102.5f, -51.4f}, "OnState"},
        {"InsideTheDekuTree", "Obj_StepSwitch_A_02", {-17.5f, 119.6f, -4.9f}, "OnState"},
        {"GerudoTrainingGround", "Obj_StepSwitch_A_04", {52.2f, 97.5f, -86.0f}, "OnState"},
        {"SpiritTemple", "Obj_SolarSwitch_A_02", {45.8f, 121.5f, -47.5f}, "isLightMax"},
    };
    for (const Line& l : lines) {
        CAPTURE(l.name);
        const auto found = CoopRoomObjects::SwitchPlugsOf(l.scene, l.name, l.at);
        REQUIRE(found.size() == 1);
        REQUIRE_FALSE(found[0]->guard.empty());
        Sharer b;
        std::vector<proto::SwitchState> out;
        std::vector<std::string> notes;
        std::uint64_t t = 1000;
        proto::SwitchState on;
        on.player_number = 1;
        on.scene = l.scene;
        on.name = l.name;
        on.place = l.at;
        on.plug = PlugKey(l.plug);
        on.on = true;
        b.Receive(on, t);
        // Not armed: nothing held, nothing sent (our plug is 0), the wait is logged once.
        for (int i = 0; i < 5; ++i) {
            const auto holds = b.Tick(l.scene, {Seen{found[0], 0x2000, false, false}}, true, t += 100, out, notes);
            CHECK(holds.empty());
        }
        CHECK(out.empty());
        CHECK(Has(notes, "waiting for"));
        // The guard flag came (our asker is gone or its trigger is false): the hold runs the room objects only.
        b.Receive(on, t);
        auto holds = b.Tick(l.scene, {Seen{found[0], 0x2000, false, true}}, true, t += 100, out, notes);
        REQUIRE(holds.size() == 1);
        CHECK(holds[0] == Hold{0x2000, PlugKey(l.plug)});
        CHECK(Has(notes, "ours held on"));
        // Our plug now 1 from that hold is not sent back as a press of ours.
        holds = b.Tick(l.scene, {Seen{found[0], 0x2000, true, true}}, true, t += 100, out, notes);
        CHECK(out.empty());
    }
}

TEST_CASE("switch share: a guarded line not armed is still our own: our press is sent") {
    // The presser's game: its own guard flag is not on yet when its player steps on (its asker plays the event).
    const auto found = CoopRoomObjects::SwitchPlugsOf("InsideTheDekuTree", "Obj_StepSwitch_A_02", {-17.5f, 119.6f, -4.9f});
    REQUIRE(found.size() == 1);
    Sharer a;
    std::vector<proto::SwitchState> out;
    std::vector<std::string> notes;
    const auto holds = a.Tick("InsideTheDekuTree", {Seen{found[0], 0x3000, true, false}}, true, 1000, out, notes);
    CHECK(holds.empty());
    REQUIRE(out.size() == 1);
    CHECK(out[0].on);
    CHECK(out[0].plug == PlugKey("OnState"));
}
