// SPDX-License-Identifier: GPL-3.0-or-later
// CoopEyeSwitch (deku-switches, #176 #129): another player's eye-switch event starts here by our eye's FirstOn,
// set through the game's set-bool, so our room's asker asks the event itself.
#include <doctest/doctest.h>

#include <map>

#include "coop/protocol/codec.hpp"
#include "coop_eye_switch.h"
#include "coop_live_events.h"
#include "coop_logic_probe.h"
#include "coop_logic_set.h"

namespace {
namespace a = CoopLogicSet::addr;
namespace p = CoopLogicProbe::addr;

class FakeGuest : public CoopPuppet::Guest {
public:
    bool Read(std::uint64_t address, void* out, std::size_t size) override {
        auto* b = static_cast<std::uint8_t*>(out);
        for (std::size_t i = 0; i < size; ++i) {
            const auto it = bytes.find(address + i);
            b[i] = it == bytes.end() ? 0 : it->second;
        }
        return true;
    }
    bool Write(std::uint64_t address, const void* in, std::size_t size) override {
        const auto* b = static_cast<const std::uint8_t*>(in);
        for (std::size_t i = 0; i < size; ++i) bytes[address + i] = b[i];
        return true;
    }
    template <typename T>
    void Put(std::uint64_t address, T v) {
        Write(address, &v, sizeof v);
    }
    template <typename T>
    T Get(std::uint64_t address) {
        T v{};
        Read(address, &v, sizeof v);
        return v;
    }
    std::map<std::uint64_t, std::uint8_t> bytes;
};

constexpr std::uint64_t kMain = 0x7100000000;
constexpr std::uint64_t kEye = 0x1000;
constexpr std::uint64_t kEye2 = 0x11000;
constexpr std::uint64_t kAsker = 0x20000;
constexpr std::uint64_t kBlock = 0x9000;
const std::string kLadder = "InsideTheDekuTree_Gimmick_FallingLadder";
const std::string kDeku = "InsideTheDekuTree";

// A live logic with one bool plug `key` (the plug keeps the name's key ^ 7) at `value`; `base` spaces the parts.
void Eye(FakeGuest& g, std::uint64_t eye, std::uint8_t value = 0, std::uint32_t key = CoopEyeSwitch::kFirstOn ^ 7u) {
    const std::uint64_t base = eye + 0x1000;
    g.Put<std::uint64_t>(eye + p::kActorLogic, base);
    g.Put<std::uint64_t>(base + p::kLogicInner, base + 0x1000);
    g.Put<std::uint64_t>(base + 0x1000 + p::kLogicState, 5);
    g.Put<std::uint64_t>(base + 0x1000 + p::kLogicTable, base + 0x2000);
    g.Put<std::uint64_t>(base + 0x2000 + p::kTableRootHolder, base + 0x3000);
    g.Put<std::uint64_t>(base + 0x3000 + p::kRoot, base + 0x4000);
    g.Put<std::uint32_t>(base + 0x4000 + p::kKey, key);
    g.Put<std::uint8_t>(base + 0x4000 + p::kType, CoopLogicProbe::kBool);
    g.Put<std::uint64_t>(base + 0x4000 + p::kValue, (base + 0x5000) | 2);
    g.Put<std::uint8_t>(base + 0x5000, value);
}
std::uint8_t FirstOn(FakeGuest& g, std::uint64_t eye) { return g.Get<std::uint8_t>(eye + 0x1000 + 0x5000); }

void EventRunning(FakeGuest& g, bool on) {
    g.Put<std::uint64_t>(kMain + CoopLiveEvents::addr::kEventStatePtr, 0x40000);
    g.Put<std::uint64_t>(0x40000 + CoopLiveEvents::addr::kEventSession, on ? 0x50000 : 0);
}

// The actor list, faked: the asker and the eyes are there when set.
struct World {
    std::uint64_t asker = kAsker;
    std::vector<std::uint64_t> eyes{kEye};
    std::vector<std::string> asked;  // the names looked for
    CoopEyeSwitch::Finder Finder() {
        return [this](CoopPuppet::Guest&, std::uint64_t, std::span<const std::string_view> names,
                      const std::array<float, 3>&, float) {
            asked.emplace_back(names.front());
            if (names.front() == "EventRequestActor") {
                return asker ? std::vector<std::uint64_t>{asker} : std::vector<std::uint64_t>{};
            }
            return eyes;
        };
    }
};

}  // namespace

TEST_CASE("eye switch: the table, its places and FirstOn's key") {
    CHECK(CoopEyeSwitch::kFirstOn == coop::protocol::flag_hash("FirstOn"));
    CHECK(CoopEyeSwitch::Find(kLadder, kDeku) != nullptr);
    CHECK(CoopEyeSwitch::Find(kLadder, "DodongoCavern") == nullptr);
    CHECK(CoopEyeSwitch::Find("InsideTheDekuTree_Gimmick_RoomD_PlatformUp", kDeku) == nullptr);  // a plate, not an eye
    for (const auto& s : CoopEyeSwitch::Switches()) {  // each has a place: its asker is found there
        const auto places = CoopLiveEvents::Places();
        CHECK(std::any_of(places.begin(), places.end(),
                          [&](const CoopLiveEvents::Place& pl) { return pl.event == s.event && pl.scene == s.scene; }));
    }
    CHECK(CoopEyeSwitch::OnFromEnv("1"));
    CHECK_FALSE(CoopEyeSwitch::OnFromEnv(""));
    CHECK_FALSE(CoopEyeSwitch::OnFromEnv("0"));
}

TEST_CASE("eye switch: one malloc, then FirstOn through the game's set-bool until the asker's event runs") {
    FakeGuest g;
    World w;
    Eye(g, kEye);
    CoopEyeSwitch::Switcher s(w.Finder());
    CHECK_FALSE(s.Step(g, false, 0, kMain, 100));  // nothing wanted
    CHECK_FALSE(s.Want("InsideTheDekuTree_Gimmick_CandleAndSpiderWeb", kDeku));  // not an eye: the old way
    REQUIRE(s.Want(kLadder, kDeku));
    CHECK(s.Want(kLadder, kDeku));  // once
    auto c = s.Step(g, false, 0, kMain, 100);
    REQUIRE(c);
    CHECK(c->function == kMain + a::kMalloc);
    CHECK(c->x[0] == a::kBlockSize);
    c = s.Step(g, true, kBlock, kMain, 100);
    REQUIRE(c);
    CHECK(c->function == kMain + a::kExecute);
    CHECK(c->x[0] == kBlock);
    // The node names our eye and FirstOn, value 1.
    CHECK(g.Get<std::uint32_t>(kBlock + a::kNodeKey) == CoopEyeSwitch::kFirstOn);
    const auto owner = g.Get<std::uint64_t>(kBlock + a::kNodeOwner);
    CHECK(g.Get<std::uint64_t>(g.Get<std::uint64_t>(owner + 8) + a::kHolderActor) == kEye);
    CHECK(g.Get<std::uint8_t>(g.Get<std::uint64_t>(kBlock + a::kNodeValue)) == 1);
    CHECK_FALSE(s.Step(g, true, 0, kMain, 100));  // one set a frame
    // The eye's AI put 0 back (a pulse): set again the next frame, no new malloc.
    c = s.Step(g, false, 0, kMain, 116);
    REQUIRE(c);
    CHECK(c->function == kMain + a::kExecute);
    CHECK_FALSE(s.Step(g, true, 0, kMain, 116));
    // The asker asked its event: done, not the old way.
    EventRunning(g, true);
    CHECK_FALSE(s.Step(g, false, 0, kMain, 132));
    CHECK(s.TakeFallbacks().empty());
    const auto notes = s.TakeNotes();
    REQUIRE(notes.size() == 2);
    CHECK(notes[0].find("FirstOn set") != std::string::npos);
    CHECK(notes[1].find("event started") != std::string::npos);
    CHECK_FALSE(s.Step(g, false, 0, kMain, 148));  // nothing left
}

TEST_CASE("eye switch: a latched FirstOn is not set again, no event in time goes the old way") {
    FakeGuest g;
    World w;
    Eye(g, kEye);
    CoopEyeSwitch::Switcher s(w.Finder());
    REQUIRE(s.Want(kLadder, kDeku));
    REQUIRE(s.Step(g, false, 0, kMain, 1000));  // malloc
    REQUIRE(s.Step(g, true, kBlock, kMain, 1000));  // execute
    s.Step(g, true, 0, kMain, 1000);
    g.Put<std::uint8_t>(kEye + 0x1000 + 0x5000, 1);  // the execute latched it
    CHECK_FALSE(s.Step(g, false, 0, kMain, 1016));  // on: waits for the asker
    CHECK_FALSE(s.Step(g, false, 0, kMain, 1000 + CoopEyeSwitch::kTryMs));
    CHECK_FALSE(s.Step(g, false, 0, kMain, 1001 + CoopEyeSwitch::kTryMs));  // too late
    const auto f = s.TakeFallbacks();
    REQUIRE(f.size() == 1);
    CHECK(f[0] == std::make_pair(kLadder, kDeku));
    CHECK(s.TakeNotes().back().find("plan (b)") != std::string::npos);
}

TEST_CASE("eye switch: our own shot, no asker, no eye, an event running, another scene") {
    SUBCASE("FirstOn 1 already: our game did it") {
        FakeGuest g;
        World w;
        Eye(g, kEye, 1);
        CoopEyeSwitch::Switcher s(w.Finder());
        REQUIRE(s.Want(kLadder, kDeku));
        REQUIRE(s.Step(g, false, 0, kMain, 100));  // malloc
        CHECK_FALSE(s.Step(g, true, kBlock, kMain, 100));
        CHECK(s.TakeFallbacks().empty());
        CHECK(s.TakeNotes().back().find("already") != std::string::npos);
    }
    SUBCASE("no asker: the room is cleared here, not started") {
        FakeGuest g;
        World w;
        w.asker = 0;
        Eye(g, kEye);
        CoopEyeSwitch::Switcher s(w.Finder());
        REQUIRE(s.Want(kLadder, kDeku));
        s.Step(g, false, 0, kMain, 100);
        CHECK_FALSE(s.Step(g, true, kBlock, kMain, 100));
        CHECK(s.TakeFallbacks().empty());
        CHECK(FirstOn(g, kEye) == 0);
    }
    SUBCASE("an eye without a live FirstOn: never set, the old way after kTryMs") {
        FakeGuest g;
        World w;
        Eye(g, kEye, 0, 0x1234);  // another plug only
        CoopEyeSwitch::Switcher s(w.Finder());
        REQUIRE(s.Want(kLadder, kDeku));
        s.Step(g, false, 0, kMain, 100);
        CHECK_FALSE(s.Step(g, true, kBlock, kMain, 100));
        g.Put<std::uint64_t>(kEye + p::kActorLogic, 0);  // no logic: the execute would read 0+0x118
        CHECK_FALSE(s.Step(g, false, 0, kMain, 200));
        CHECK_FALSE(s.Step(g, false, 0, kMain, 101 + CoopEyeSwitch::kTryMs));
        CHECK(s.TakeFallbacks().size() == 1);
        CHECK(g.Get<std::uint64_t>(kBlock) == 0);  // no node ever written
    }
    SUBCASE("another event runs: waits, sets after it") {
        FakeGuest g;
        World w;
        Eye(g, kEye);
        EventRunning(g, true);
        CoopEyeSwitch::Switcher s(w.Finder());
        REQUIRE(s.Want(kLadder, kDeku));
        s.Step(g, false, 0, kMain, 100);
        CHECK_FALSE(s.Step(g, true, kBlock, kMain, 100));
        EventRunning(g, false);
        CHECK(s.Step(g, false, 0, kMain, 500));
    }
    SUBCASE("we left the scene: dropped") {
        FakeGuest g;
        World w;
        CoopEyeSwitch::Switcher s(w.Finder());
        REQUIRE(s.Want(kLadder, kDeku));
        s.Expire(kDeku);
        s.Expire("DekuForest");
        s.Step(g, false, 0, kMain, 100);
        CHECK_FALSE(s.Step(g, true, kBlock, kMain, 100));
        CHECK(s.TakeFallbacks().empty());
        CHECK(s.TakeNotes().front().find("left its scene") != std::string::npos);
    }
}

TEST_CASE("eye switch: Dodongo's Cavern sets each of its eyes, the Deku Tree only the nearest") {
    FakeGuest g;
    World w;
    w.eyes = {kEye, kEye2};
    Eye(g, kEye);
    Eye(g, kEye2);
    CoopEyeSwitch::Switcher dc(w.Finder());
    REQUIRE(dc.Want("DodongoCavern_Gimmick_EyeSwitch", "DodongoCavern"));
    dc.Step(g, false, 0, kMain, 100);
    REQUIRE(dc.Step(g, true, kBlock, kMain, 100));
    g.Put<std::uint8_t>(kEye + 0x1000 + 0x5000, 1);  // the first latched
    dc.Step(g, true, 0, kMain, 100);
    REQUIRE(dc.Step(g, false, 0, kMain, 116));
    CHECK(g.Get<std::uint64_t>(g.Get<std::uint64_t>(g.Get<std::uint64_t>(kBlock + a::kNodeOwner) + 8) +
                               a::kHolderActor) == kEye2);

    FakeGuest g2;
    Eye(g2, kEye, 1);
    Eye(g2, kEye2);
    CoopEyeSwitch::Switcher deku(w.Finder());
    REQUIRE(deku.Want(kLadder, kDeku));
    deku.Step(g2, false, 0, kMain, 100);
    CHECK_FALSE(deku.Step(g2, true, kBlock, kMain, 100));  // the nearest is on: our own shot; the far one untouched
    CHECK(deku.TakeNotes().back().find("already") != std::string::npos);
}
