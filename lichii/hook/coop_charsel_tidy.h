// SPDX-License-Identifier: GPL-3.0-or-later
// Character Select's page flags put back when no menu is up (player report 202: "the game was stuck in a first person
// view").
//
// OoT Character Select (snaildev99, MIT; mods/build.py) swaps the − menu's 3D character in a sequence of its page's
// graph: it sets its Bool CharacterMenu_KeepUICamera, the game's "delete the menu Link" runs (its exe patch then skips
// that function's camera part, so the menu's 3D UI camera stays alive for the new character), and clears the Bool as
// the sequence's last step. CharacterMenu_Slide / CharacterMenu_SlideOut carry the swap's crossfade the same way (its
// exe patch parks or deletes menu slot 4 while Slide is not 0), and CharacterMenu_IsPageOpen says his page shows.
// None of the four is saved and none is reset by the game (ResetTypeValue 0): a sequence cut short (the menu closed
// in the middle of a swap) leaves them set until the game is closed. With KeepUICamera left set, every later "delete
// the menu Link" (each menu close, each area load) keeps the 3D UI camera: the menu's camera outlives the menu.
//
// Each of them is 0 whenever no game menu is up (the swap runs only while his page shows). So, with no menu in front
// for kQuietFrames in a row, any of them read as not 0 is written back to 0 through the game's own setter, once per
// time it is found set, and said in the log. Nothing is done while a menu is up, while the menu state is not known,
// or without the mod (the flags do not exist).
//
// Logic only: the hook (coop_hook.cpp) gives the reads, the writes and the menu state; tests/lichii/
// charsel_tidy_test.cpp drives it.
#pragma once

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
// Frames with no menu in front before the flags are looked at (half a second: a menu closing has run its graph).
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
        if (!menu_up || *menu_up) {
            quiet_ = 0;
            return out;
        }
        if (++quiet_ < kQuietFrames) return out;
        quiet_ = kQuietFrames;   // (no overflow while the field plays)
        for (const Flag& f : kFlags) {
            const auto v = read(f);
            if (!v || *v == 0) continue;
            if (!write(f, 0)) continue;   // tried again next frame
            ++cleared_;
            out.push_back("characters: Character Select's " + std::string(f.name) + " was left at " +
                          std::to_string(*v) + " with no menu up (his page closed in the middle of a swap): set " +
                          "back to 0 (player report 202: the menu's camera stays otherwise)");
        }
        return out;
    }
    std::uint64_t cleared() const { return cleared_; }

private:
    int quiet_ = 0;
    std::uint64_t cleared_ = 0;
};

}  // namespace CoopCharselTidy
