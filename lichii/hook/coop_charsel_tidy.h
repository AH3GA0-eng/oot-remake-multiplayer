// SPDX-License-Identifier: GPL-3.0-or-later
// Character Select's page flags put back when no menu is up (player report 202: "the game was stuck in a first person
// view").
//
// OoT Character Select (snaildev99, MIT; mods/build.py) swaps the − menu's 3D character in a sequence of its page's
// graph: it sets its Bool CharacterMenu_KeepUICamera, the game's "delete the menu Link" runs (its exe patch then skips
// that function's camera part, so the menu's 3D UI camera stays alive for the new character), and clears the Bool as
// the sequence's last step. CharacterMenu_Slide / CharacterMenu_SlideOut carry the swap's crossfade the same way (its
// exe patch parks or deletes menu slot 4 while Slide is not 0), and CharacterMenu_IsPageOpen says his page shows.
// None of the four is saved and none is reset by the game (ResetTypeValue 0): a swap cut short leaves them set until
// the game is closed. With KeepUICamera set, the next "delete the menu Link" keeps the 3D UI camera alive. That
// function has one caller, the SeqOneShotDeletePouchPlayer node (his build.py: calls_to(0x80c35c30) == [0x80c35c08]),
// which runs in an area load (LoadBancImpl) and in his own swap; a menu close does not run it. So the camera
// outlives its menu when an area load (or a later swap) comes while the flag is still set.
//
// His page's exits all clear IsPageOpen: B and - (MinusMenuToMain, his close_page_with_menu), R and ZR (his exit to
// Key Items). So the rule:
// - IsPageOpen 0 (his page not shown, menu up or not): KeepUICamera, Slide and SlideOut not 0 are a swap cut short.
//   They are put back to 0 at once, on that frame, before any later area load or menu step can delete the menu Link
//   with them set;
// - IsPageOpen itself left at 1 (a path that skipped his exits) with no game menu in front for kQuietFrames: all four
//   are put back to 0;
// - IsPageOpen 1 while a menu is up (his page showing, a swap may be running): nothing is touched.
// Each one found set is written back through the game's own setter, said once in the log. Without the mod the
// flags do not exist: nothing.
//
// Logic only: the hook (coop_hook.cpp) gives the reads, the writes and the menu state; tests/lichii/
// charsel_tidy_test.cpp drives it.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CoopCharselTidy {

struct Flag {
    std::string_view name;
    bool is_bool;   // else an Int
};
// His page's flags that are 0 whenever no menu shows (build.py GAMEDATA_FLAGS, ResetTypeValue 0, SaveFileIndex -1).
inline constexpr std::array<Flag, 4> kFlags{{
    {"CharacterMenu_KeepUICamera", true},
    {"CharacterMenu_IsPageOpen", true},
    {"CharacterMenu_Slide", false},
    {"CharacterMenu_SlideOut", false},
}};
// Frames with no menu in front before an IsPageOpen still at 1 counts as left set (half a second: a menu
// closing has run its graph, MinusMenuToMain included).
inline constexpr int kQuietFrames = 30;

class Tidy {
public:
    // read: the flag's value (a Bool as 0/1), nothing when the game has no such flag (no mod).
    // write: the game's setter, true when queued.
    using Read = std::function<std::optional<std::int32_t>(const Flag&)>;
    using Write = std::function<bool(const Flag&, std::int32_t)>;

    // One game frame. `menu_up`: a game menu is in front (nothing: not known). Returns the log lines.
    std::vector<std::string> Frame(std::optional<bool> menu_up, const Read& read, const Write& write) {
        std::vector<std::string> out;
        quiet_ = (menu_up && !*menu_up) ? std::min(quiet_ + 1, kQuietFrames) : 0;
        const auto page = read(kFlags[1]);
        if (!page) return out;   // no mod
        const bool page_left = *page == 0;                    // his exits cleared it: what is left set is a cut swap
        const bool stuck_page = !page_left && quiet_ >= kQuietFrames;   // no menu for a while, still "open"
        if (!page_left && !stuck_page) return out;            // his page shows: his flags
        for (const Flag& f : kFlags) {
            if (page_left && &f == &kFlags[1]) continue;      // (0 already)
            const auto v = read(f);
            if (!v || *v == 0) continue;
            if (!write(f, 0)) continue;   // tried again next frame
            ++cleared_;
            out.push_back("characters: Character Select's " + std::string(f.name) + " was left at " +
                          std::to_string(*v) + (page_left ? " after his page closed" : " with no menu up") +
                          " (a swap cut short): set back to 0 (player report 202: the menu's camera outlives the "
                          "menu otherwise)");
        }
        return out;
    }
    std::uint64_t cleared() const { return cleared_; }

private:
    int quiet_ = 0;
    std::uint64_t cleared_ = 0;
};

}  // namespace CoopCharselTidy
