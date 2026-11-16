// SPDX-License-Identifier: GPL-3.0-or-later
//
// Player reports #63/#76 (dungeon-sync-b, dungeon-maps-a; on unless LICHII_COOP_MAP_REFRESH=0): a dungeon map, compass,
// boss key or floors written by co-op (another player's, the join snapshot, a reset, a put-back, the solo keeper) shows on
// our Map page and minimap.
//
// The Map page and the minimap do not read the save's flags live. The HUD screen (Longshot patch 0018's
// hud_screen, CoopMapPage::MenuState) keeps its own copy: the floors at +0x1500 and the map, compass and boss
// key at +0x1538..0x153b. The game reads that copy in only at the screen's init (main+0x33e12c0) and refresh
// (main+0x33e4200), which its HUD state machine (main+0x33e4bf0) calls. On each floor change the game writes the
// copy back to DungeonInfo.<dungeon>.EnteredFloorBit (main+0x5851ac), so a shared IsGetMap or floors write
// alone does not show until the game's next refresh: the Map page, a room or scene load, an event's end
// (reports/dungeon-sync-b.md, "What the Map page draws from"; reports/dungeon-maps-a.md).
//
// So when a shared map or floors value comes from another player, this calls the game's own refresh on the HUD
// screen, once, a moment later (after the game's setter has taken the flag). It never writes the screen's copy.
// Each call logs the copy before and after. Proven: lane1-a hs-map-deku-walkin 20261116-003904 (guest), hs-map-opener
// 20261116-013911 (without it the receiver's minimap stayed blank until the Map page or a room load).
//
// LICHII_COOP_MAP_REFRESH=0 turns it off. No emulator code (tests/lichii/map_refresh_test.cpp).
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "coop_puppet.h"

namespace CoopMapRefresh {

namespace addr {
inline constexpr std::uint64_t kRefresh = 0x33e4200;  // the HUD screen's refresh: x0 = the screen
inline constexpr std::uint64_t kFloors = 0x1500;      // the screen's floors byte (read for the log only)
inline constexpr std::uint64_t kHeld = 0x1538;        // map, compass, boss key, 0x251d4c2a (bytes; log only)
}  // namespace addr

// A shared value lands through the game's own setter a frame or more later: the refresh waits this long.
inline constexpr std::uint64_t kDelayMs = 300;

// The svc caller (CoopCallers::Caller contract).
class Refresher {
public:
    // The hook's flag report (any thread): another player's map or floors arrived.
    void Want(std::uint64_t now_ms);
    // The Hookshot frame (any thread): the HUD screen that runs now (0: none) and main's base.
    // `screen` only when the HUD screen ran a moment ago (patch 0018's hud_ran_ns_ago, CoopMapPage::Menu::HudRuns),
    // else 0: patch 0018 forgets a HUD screen whose slot was reused, so no call is made on one that may be freed.
    void SetScreen(std::uint64_t screen, std::uint64_t main);
    // The hook's frame (calc thread): Link is in play (a scene loaded, the first Link there). Not at the title, a
    // load or a reload: the refresh waits.
    void SetInPlay(bool in_play);
    // The calc thread.
    std::optional<CoopPuppet::Call> Step(CoopPuppet::Guest& guest, bool after_call, std::uint64_t now_ms);
    std::vector<std::string> TakeNotes();

private:
    std::string Copy(CoopPuppet::Guest& guest, std::uint64_t screen);

    std::mutex mutex_;
    std::uint64_t due_ms_ = 0;  // 0: nothing wanted
    std::uint64_t screen_ = 0;
    bool in_play_ = false;
    std::uint64_t main_ = 0;
    std::uint64_t called_on_ = 0;  // the calc thread's: the screen of the call made, until its after_call
    std::string before_;
    std::vector<std::string> notes_;
};

Refresher& TheRefresher();

}  // namespace CoopMapRefresh
