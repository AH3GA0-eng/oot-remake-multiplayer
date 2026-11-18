// SPDX-License-Identifier: GPL-3.0-or-later
// The pause menu's Multiplayer tab (issue 264, piece 4; docs/hookshot.md "The
// pause menu's Multiplayer tab"): a fifth tab after System in the game's own
// pause menu, and its page. The pack adds the layouts (tools/puppet_pack/
// pause_menu.py); patch 0030 hands the module every menu screen's update
// (hos_coop_game_screen) and asks it which page the mouse's tab strip should
// count (hos_coop_game_pause_page).
//
// This step is the probe the first game run needs: which screens reach the
// module, how often, and what the tab strip and the pages say (the strip's tab
// count, its mode and lit category, the menu manager's page). It shows no page
// yet, so the mouse's page is always the game's (-1).
//
// Logic only (no game reads, no log): coop_pause_tab_module.cpp reads the game
// and logs; tests/hookshot/pause_tab_test.cpp drives Probe.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace CoopPauseTab {

// The game's screens this tab works with, by their class's vtable (offsets in
// main, build e0f3ccfaf50065ae; read from main: each class's name getter, then
// its factory's constructor).
inline constexpr std::uint64_t kPagesVtable = 0x4bebaa8;   // main+: UIPlusMenuScreen (PlusMenu_00, the four pages)
inline constexpr std::uint64_t kStripVtable = 0x4be8aa0;   // main+: UIMenuOverlayScreen (MenuOverlay_00, the tabs)
// Fields read (docs/game-ui.md, "The pause menu"):
inline constexpr std::uint64_t kScreenState = 0x1a6;   // u8, every screen: 2 open
inline constexpr std::uint64_t kStripTabs = 0x328;     // u32: its Pa_Category_%02d parts (5 with the pack's tab)
inline constexpr std::uint64_t kStripMode = 0x350;     // u8: the Plus menu's tabs or the Minus menu's
inline constexpr std::uint64_t kStripLit = 0x358;      // s32: the category it lights (0, 3, 4, 5 the Plus pages)
inline constexpr std::uint64_t kMenuManager = 0x4e7cfa8;   // main+: the global holding the menu manager
inline constexpr std::uint64_t kManagerPage = 0x530;   // s32: the Plus menu's current page (a category)
inline constexpr std::uint64_t kManagerAsked = 0x538;  // s32: the page asked for (the step's)

enum class Kind { kOther, kPages, kStrip };
Kind KindOf(std::uint64_t vtable_offset);

// What one update of a screen shows (read by the module from the game).
struct Facts {
    Kind kind = Kind::kOther;
    std::uint64_t screen = 0;
    int state = 0;
    int tabs = 0, mode = 0, lit = 0;   // the strip's
    int page = -1, asked = -1;         // the pages' (the menu manager's)
};

// The log lines a run needs, from the updates as they come (the game's threads:
// the caller holds a lock). A screen object seen for the first time, its
// updates counted over its first second (once a frame is expected), and every
// change of what it shows. Nothing for other screens.
class Probe {
public:
    std::vector<std::string> Seen(const Facts& f, std::uint64_t now_ms);

private:
    struct Last {
        std::uint64_t screen = 0;
        Facts facts;
        std::uint64_t first_ms = 0;
        unsigned updates = 0;
        bool counted = false;
    };
    Last pages_, strip_;
};

// The page the mouse's tab strip should count (patch 0030): 4 while the module
// shows its page over System's; -1 (the game's own) in this step.
int MousePage();

// The module's part (coop_pause_tab_module.cpp), hos_coop_game_screen: every
// menu screen's update, on the game's threads; ctx may call() (patch 0030).
void OnScreen(void* ctx, std::uint64_t screen) noexcept;

}  // namespace CoopPauseTab
