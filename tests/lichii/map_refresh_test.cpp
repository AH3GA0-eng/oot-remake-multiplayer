// SPDX-License-Identifier: GPL-3.0-or-later
// CoopMapRefresh (dungeon-sync-b, #63/#76): the game's own HUD refresh after another player's map arrives.
#include <doctest/doctest.h>

#include <map>

#include "coop_map_refresh.h"

namespace {
namespace a = CoopMapRefresh::addr;

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
    std::map<std::uint64_t, std::uint8_t> bytes;
};

constexpr std::uint64_t kMain = 0x7100000000;
constexpr std::uint64_t kScreen = 0x5000;

}  // namespace

TEST_CASE("map refresh: one call of the game's refresh on the HUD screen, a moment after a map arrives") {
    FakeGuest g;
    CoopMapRefresh::Refresher r;
    r.SetInPlay(true);
    CHECK_FALSE(r.Step(g, false, 100));  // nothing wanted
    r.Want(100);
    r.SetScreen(kScreen, kMain);
    CHECK_FALSE(r.Step(g, false, 100 + CoopMapRefresh::kDelayMs - 1));  // the game's setter first
    g.bytes[kScreen + a::kFloors] = 0x4;
    auto c = r.Step(g, false, 100 + CoopMapRefresh::kDelayMs);
    REQUIRE(c);
    CHECK(c->function == kMain + a::kRefresh);
    CHECK(c->x[0] == kScreen);
    // The game's refresh read the flags in: the log has the copy before and after; nothing of ours was written.
    g.bytes[kScreen + a::kFloors] = 0x1f;
    g.bytes[kScreen + a::kHeld] = 1;
    const auto before = g.bytes;
    CHECK_FALSE(r.Step(g, true, 0));
    CHECK(g.bytes == before);
    const auto notes = r.TakeNotes();
    REQUIRE(notes.size() == 1);
    CHECK(notes[0].find("floors 0x04, map 0") != std::string::npos);
    CHECK(notes[0].find("floors 0x1f, map 1") != std::string::npos);
    CHECK_FALSE(r.Step(g, false, 1000));  // once
}

TEST_CASE("map refresh: no running HUD screen (an older patch 0018, a screen not run lately): nothing called, kept until one runs") {
    FakeGuest g;
    CoopMapRefresh::Refresher r;
    r.SetInPlay(true);
    r.Want(100);
    CHECK_FALSE(r.Step(g, false, 1000));
    r.SetScreen(kScreen, kMain);
    CHECK(r.Step(g, false, 1100));
}

TEST_CASE("map refresh: not in play (the title, a load, a reload): nothing called, kept until Link is back") {
    FakeGuest g;
    CoopMapRefresh::Refresher r;
    r.SetScreen(kScreen, kMain);
    r.Want(100);
    r.SetInPlay(false);
    CHECK_FALSE(r.Step(g, false, 1000));
    r.SetInPlay(true);
    CHECK(r.Step(g, false, 1100));
}

TEST_CASE("map refresh: the HUD screen stops running before the call: none made on it") {
    FakeGuest g;
    CoopMapRefresh::Refresher r;
    r.SetInPlay(true);
    r.SetScreen(kScreen, kMain);
    r.Want(100);
    r.SetScreen(0, kMain);  // patch 0018: not run for 100 ms, or its slot reused (the title, a new HUD)
    CHECK_FALSE(r.Step(g, false, 1000));
    r.SetScreen(kScreen + 0x100, kMain);  // a new HUD screen that runs
    const auto c = r.Step(g, false, 1100);
    REQUIRE(c);
    CHECK(c->x[0] == kScreen + 0x100);
}
