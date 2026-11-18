// SPDX-License-Identifier: GPL-3.0-or-later
// The pause menu's Multiplayer tab, its first step (coop_pause_tab.h): the
// probe's lines for the first game run, and the mouse's page.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "coop_pause_tab.h"

namespace {

using CoopPauseTab::Facts;
using CoopPauseTab::Kind;

Facts Strip(std::uint64_t screen, int tabs, int lit, int mode = 0, int state = 2) {
    Facts f;
    f.kind = Kind::kStrip;
    f.screen = screen;
    f.tabs = tabs, f.lit = lit, f.mode = mode, f.state = state;
    return f;
}

Facts Pages(std::uint64_t screen, int page, int asked, int state = 2) {
    Facts f;
    f.kind = Kind::kPages;
    f.screen = screen;
    f.page = page, f.asked = asked, f.state = state;
    return f;
}

bool Has(const std::vector<std::string>& lines, const std::string& part) {
    for (const auto& l : lines) {
        if (l.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("pause tab: the screens are known by their class's vtable") {
    CHECK(CoopPauseTab::KindOf(CoopPauseTab::kPagesVtable) == Kind::kPages);
    CHECK(CoopPauseTab::KindOf(CoopPauseTab::kStripVtable) == Kind::kStrip);
    CHECK(CoopPauseTab::KindOf(0x4bf06e8) == Kind::kOther);   // the title (UITitleScreen)
    CHECK(CoopPauseTab::KindOf(0) == Kind::kOther);
    // Guest addresses, main at 0x80020000 (docs/game-ui.md): UIPlusMenuScreen 0x84c0baa8, UIMenuOverlayScreen
    // 0x84c08aa0, the menu manager's global 0x84e9cfa8.
    CHECK(0x80020000 + CoopPauseTab::kPagesVtable == 0x84c0baa8);
    CHECK(0x80020000 + CoopPauseTab::kStripVtable == 0x84c08aa0);
    CHECK(0x80020000 + CoopPauseTab::kMenuManager == 0x84e9cfa8);
}

TEST_CASE("pause tab: the probe logs a screen's first update, its first second's count and each change") {
    CoopPauseTab::Probe p;
    auto lines = p.Seen(Strip(0x9000, 5, 5), 1000);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "the tab strip (MenuOverlay_00), screen 0x9000: its updates reach the module (patch 0030)"));
    CHECK(Has(lines, "5 tabs, the Plus menu, lights category 5, state 2"));
    // The same, once a frame: nothing until its first second is over, then the count.
    for (int i = 1; i < 60; ++i) {
        CHECK(p.Seen(Strip(0x9000, 5, 5), 1000 + 16 * static_cast<std::uint64_t>(i)).empty());
    }
    lines = p.Seen(Strip(0x9000, 5, 5), 2000);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "the tab strip (MenuOverlay_00): 61 updates in its first 1000 ms"));
    CHECK(p.Seen(Strip(0x9000, 5, 5), 3000).empty());   // counted once
    // A change: one line with the old and the new.
    lines = p.Seen(Strip(0x9000, 5, 4), 3016);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "lights category 4, state 2 (was 5 tabs, the Plus menu, lights category 5, state 2)"));
    CHECK(Has(p.Seen(Strip(0x9000, 5, 6, 1), 3032), "the Minus menu, lights category 6"));
    // The pages apart from the strip.
    lines = p.Seen(Pages(0xa000, 5, 5), 3048);
    CHECK(Has(lines, "the pause menu's pages (PlusMenu_00), screen 0xa000: its updates reach the module"));
    CHECK(Has(lines, "page 5, asked 5, state 2"));
    CHECK(Has(p.Seen(Pages(0xa000, 5, 4), 3064), "page 5, asked 4, state 2 (was page 5, asked 5, state 2)"));
    // A new screen object starts over.
    CHECK(Has(p.Seen(Strip(0x9100, 4, 5), 4000), "screen 0x9100: its updates reach the module"));
}

TEST_CASE("pause tab: other screens give nothing, and the mouse's page is the game's in this step") {
    CoopPauseTab::Probe p;
    Facts other;
    other.screen = 0x9000;
    CHECK(p.Seen(other, 0).empty());
    CHECK(p.Seen(Strip(0, 5, 5), 0).empty());
    CHECK(CoopPauseTab::MousePage() == -1);
}
