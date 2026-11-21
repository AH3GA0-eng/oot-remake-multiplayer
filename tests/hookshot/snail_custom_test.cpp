// SPDX-License-Identifier: GPL-3.0-or-later
// Co-op's customization in Character Select's Customize screen (hookshot/src/coop_snail_custom.h).
#include <doctest/doctest.h>

#include <string>

#include "coop_colors.h"
#include "coop_snail_custom.h"

using CoopLook::Look;
using CoopLook::Part;
using namespace CoopSnailCustom;

TEST_CASE("snail customize: his focus rule, two columns of three, no wrap") {
    CHECK(Neighbour(0, Dir::kDown) == 1);
    CHECK(Neighbour(2, Dir::kDown) == -1);
    CHECK(Neighbour(0, Dir::kUp) == -1);
    CHECK(Neighbour(1, Dir::kRight) == 4);
    CHECK(Neighbour(4, Dir::kLeft) == 1);
    CHECK(Neighbour(4, Dir::kRight) == -1);
    CHECK(Neighbour(0, Dir::kLeft) == -1);
    CHECK(Neighbour(-1, Dir::kDown) == -1);
}

TEST_CASE("snail customize: his six options show co-op's parts, ZL / ZR step the focused one, Y turns the page") {
    Driver d;
    CHECK(d.Pad(kZR) == kZR);   // (closed: nothing of co-op's)
    Look start;
    start[Part::zora_tunic] = *CoopColors::Parse("red");
    d.Open(start, 0);
    REQUIRE(d.Active());
    auto t = d.Shown();
    CHECK(t.title == "Link (page 1 of 4)   Y: next page");
    CHECK(t.labels[0] == "Eye Colour");
    CHECK(t.labels[1] == "Kokiri Tunic");
    CHECK(t.labels[3] == "Goron Tunic");   // (his right column)
    CHECK(t.labels[4] == "Zora Tunic");
    CHECK(t.labels[5].empty());            // (five on this page: his sixth option says nothing)
    CHECK(t.values[4] == "red");
    CHECK(t.values[1] == "Game's own");
    // The d-pad is his (his marker moves); co-op follows: right to Goron Tunic, down to Zora Tunic.
    CHECK(d.Pad(kRight) == kRight);
    CHECK(d.Pad(kDown) == kDown);
    CHECK(d.Focus() == 4);
    // ZR: Zora Tunic to the next preset; never his (his option Int stays 0).
    CHECK(d.Pad(kZR) == 0);
    const auto change = d.TakeChange();
    REQUIRE(change);
    CHECK((*change)[Part::zora_tunic] != CoopColors::Parse("red"));
    // Y: the Shields page, the same six places.
    CHECK(d.Pad(kY) == 0);
    t = d.Shown();
    CHECK(t.title == "Shields (page 2 of 4)   Y: next page");
    CHECK(t.labels[0] == "Hylian Shield Frame");
    CHECK(t.labels[5] == "Deku Shield");
    // B and -: his (back to his picker).
    CHECK(d.Pad(kB) == kB);
    CHECK(d.Pad(kMinus) == kMinus);
}

TEST_CASE("snail customize: A opens co-op's any-colour mixer in his six places, B and Done leave it") {
    Driver d;
    d.Open(Look{}, 0);
    CHECK(d.Pad(kA) == 0);   // (Kokiri Tunic, the first option)
    REQUIRE(d.Editing());
    auto t = d.Shown();
    CHECK(t.labels[0] == "Hue");
    CHECK(t.labels[1] == "Saturation");
    CHECK(t.labels[2] == "Brightness");
    CHECK(t.labels[3] == "Preset");
    CHECK(t.labels[4] == "Game's own colour");
    CHECK(t.values[4] == "A");
    CHECK(t.labels[5] == "Done");
    CHECK(t.title.find("Y:") == std::string::npos);   // (no page turn in the mixer)
    d.Pad(kZR);   // (Hue round the wheel)
    REQUIRE(d.TakeChange());
    CHECK(d.Pad(kB) == 0);   // (B: back to the parts, his screen stays)
    CHECK_FALSE(d.Editing());
    d.Pad(kA);
    REQUIRE(d.Editing());
    d.Pad(kRight), d.Pad(kDown), d.Pad(kDown);   // (Done: the right column's last)
    CHECK(d.Focus() == 5);
    d.Pad(kA);
    CHECK_FALSE(d.Editing());
}

TEST_CASE("snail customize: a character with no colour options shows that, and its keys change nothing") {
    Driver d;
    d.Open(Look{}, 9);
    const auto t = d.Shown();
    CHECK(t.labels[0] == CoopLookMenu::kNoColourOptions);
    CHECK(t.title.find("Y:") == std::string::npos);
    d.Pad(kZR), d.Pad(kA), d.Pad(kY);
    CHECK_FALSE(d.TakeChange());
    CHECK_FALSE(d.Editing());
}

TEST_CASE("snail customize: his option past a page's parts does nothing") {
    Driver d;
    Look start;
    start[Part::zora_tunic] = *CoopColors::Parse("red");
    d.Open(start, 0);
    d.Pad(kRight), d.Pad(kDown), d.Pad(kDown);   // (his sixth option: the Link page has five)
    REQUIRE(d.Focus() == 5);
    d.Pad(kZR);
    d.Pad(kA);
    CHECK_FALSE(d.TakeChange());
    CHECK_FALSE(d.Editing());
}
