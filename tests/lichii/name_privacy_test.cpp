// SPDX-License-Identifier: GPL-3.0-or-later
// The co-op name never names the player (lichii/hook/coop_name_privacy.*, 0.11.1 player
// report): random names for new players, and the PC's own names told apart from a chosen one.
#include <doctest/doctest.h>

#include <set>
#include <string>

#include "coop_name_privacy.h"

using namespace CoopNamePrivacy;

TEST_CASE("name privacy: random names are friendly, valid and differ") {
    std::set<std::string> seen;
    for (std::uint32_t r = 0; r < 2000; r += 7) {
        const std::string n = RandomName(r * 2654435761u);
        CHECK(n.size() >= 6);
        CHECK(n.size() <= 14);   // a co-op name may have 16
        const auto dash = n.rfind('-');
        REQUIRE(dash != std::string::npos);
        CHECK(n.size() - dash - 1 == 4);
        CHECK(n[dash + 1] != '0');
        seen.insert(n);
    }
    CHECK(seen.size() > 250);
    CHECK(RandomName(12345) == RandomName(12345));
    CHECK(NewName().find('-') != std::string::npos);
}

TEST_CASE("name privacy: the PC's user name and PC name are found, a chosen name is not") {
    CHECK(Matches("Bob", "bob", "DESKTOP-1") == Source::user);
    CHECK(Matches("  BOB ", "Bob", "") == Source::user);
    CHECK(Matches("desktop-1", "bob", "DESKTOP-1") == Source::pc);
    CHECK(Matches("Saria", "bob", "DESKTOP-1") == Source::none);
    CHECK(Matches("Bobby", "Bob", "PC") == Source::none);
    CHECK(Matches("", "", "") == Source::none);
    CHECK(Matches("Saria", "", "") == Source::none);
    // The old default cut a longer user name to 16 characters.
    CHECK(Matches("abcdefghijklmnop", "abcdefghijklmnopqrs", "") == Source::user);
}

TEST_CASE("name privacy: the chat line and its queue") {
    CHECK(Notice(Source::user, "Goron-4821", true) ==
          "Your co-op name was your Windows user name; it is now Goron-4821. Change it in the Multiplayer tab.");
    CHECK(Notice(Source::user, "Zora-1000", false).find("your user name;") != std::string::npos);
    CHECK(Notice(Source::pc, "Zora-1000", true).find("your PC's name;") != std::string::npos);
    CHECK(TakeLines().empty());
    Say("one");
    Say("");
    const auto lines = TakeLines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "one");
    CHECK(TakeLines().empty());
}
