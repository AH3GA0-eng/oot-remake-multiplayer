// SPDX-License-Identifier: GPL-3.0-or-later
// Character Select's page flags put back when no menu is up (lichii/hook/coop_charsel_tidy.h, player report 202).
#include <doctest/doctest.h>

#include <map>
#include <optional>
#include <string>

#include "../../lichii/hook/coop_charsel_tidy.h"

using namespace CoopCharselTidy;

namespace {
struct Game {
    std::map<std::string, std::int32_t> flags;   // the mod's flags (none: no mod)
    bool refuse = false;
    int writes = 0;
    Tidy::Read read() {
        return [this](const Flag& f) -> std::optional<std::int32_t> {
            const auto it = flags.find(std::string(f.name));
            if (it == flags.end()) return std::nullopt;
            return it->second;
        };
    }
    Tidy::Write write() {
        return [this](const Flag& f, std::int32_t v) {
            if (refuse) return false;
            ++writes;
            flags[std::string(f.name)] = v;
            return true;
        };
    }
};
Game WithMod() {
    Game g;
    for (const Flag& f : kFlags) g.flags[std::string(f.name)] = 0;
    return g;
}
std::vector<std::string> Run(Tidy& t, Game& g, std::optional<bool> menu_up, int frames) {
    std::vector<std::string> all;
    for (int i = 0; i < frames; ++i) {
        for (auto& l : t.Frame(menu_up, g.read(), g.write())) all.push_back(l);
    }
    return all;
}
}  // namespace

TEST_CASE("charsel tidy: the keep-camera flag left set after the menu closed goes back to 0 (row 202)") {
    Game g = WithMod();
    Tidy t;
    // His page swapping: the flags set while the menu is up are his to clear, never touched.
    g.flags["CharacterMenu_KeepUICamera"] = 1;
    g.flags["CharacterMenu_Slide"] = 200;
    g.flags["CharacterMenu_IsPageOpen"] = 1;
    CHECK(Run(t, g, true, 600).empty());
    CHECK(g.writes == 0);
    // The menu closed in the middle of the swap: half a second later, each one back to 0, said once.
    CHECK(Run(t, g, false, kQuietFrames - 1).empty());
    const auto lines = Run(t, g, false, 1);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0].find("CharacterMenu_KeepUICamera was left at 1") != std::string::npos);
    CHECK(g.flags["CharacterMenu_KeepUICamera"] == 0);
    CHECK(g.flags["CharacterMenu_Slide"] == 0);
    CHECK(g.flags["CharacterMenu_IsPageOpen"] == 0);
    CHECK(t.cleared() == 3);
    CHECK(Run(t, g, false, 600).empty());   // nothing more while they stay 0
    CHECK(g.writes == 3);
}

TEST_CASE("charsel tidy: a short look away from the menu, an unknown menu state or no mod change nothing") {
    Game g = WithMod();
    g.flags["CharacterMenu_SlideOut"] = 3;
    Tidy t;
    for (int n = 0; n < 20; ++n) {   // the menu flickering shut for fewer than kQuietFrames each time
        Run(t, g, false, kQuietFrames - 1);
        Run(t, g, true, 1);
    }
    CHECK(g.writes == 0);
    CHECK(Run(t, g, std::nullopt, 600).empty());   // not known: not touched
    CHECK(g.writes == 0);
    Game none;   // no Character Select: no such flags
    Tidy t2;
    CHECK(Run(t2, none, false, 600).empty());
    CHECK(none.writes == 0);
}

TEST_CASE("charsel tidy: a refused write is tried again, and a flag set again later is put back again") {
    Game g = WithMod();
    g.flags["CharacterMenu_KeepUICamera"] = 1;
    g.refuse = true;
    Tidy t;
    CHECK(Run(t, g, false, kQuietFrames + 5).empty());
    g.refuse = false;
    CHECK(Run(t, g, false, 1).size() == 1);
    CHECK(g.flags["CharacterMenu_KeepUICamera"] == 0);
    // Another swap cut short later: the menu up, set, closed again.
    Run(t, g, true, 10);
    g.flags["CharacterMenu_KeepUICamera"] = 1;
    CHECK(Run(t, g, false, kQuietFrames).size() == 1);
    CHECK(t.cleared() == 2);
}
