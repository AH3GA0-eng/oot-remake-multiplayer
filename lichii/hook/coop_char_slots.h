// A pick in Snail's Character Select page whose model is not installed (owner, 0.11.9: his NPC characters are an
// opt-in download; without it his full picker stays). The mod's picker stores the pick (CharacterMenu_SelectedCharacter,
// the mod's own number of the character) and its model / cloth / voice hooks build a path from that number every time
// the game loads the player: a number whose files are not there would load missing files. So the hook reads the pick
// every frame and puts a dead one back at once (the last installed pick, or 0: Link), with one chat line.
// Installed: what the puppet pack found (tools/puppet_pack/build.py find_characters keeps only numbers whose
// romfs/Model/Player_<age>_C<file> file is there; coop_puppet.json "characters": {"numbers": ...}).
// Another player's pick we do not have: their puppet is Link already (Planner::ActorFor finds no file), and the
// same line is said once per player number (not per frame), again only when they pick another missing one.
// Header only, standard C++ only.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace CoopCharSlots {

// The chat line (plain words; the kit's start shows the NPC line and N switches the download on).
inline std::string MissingLine(bool own, const std::string& who = {}) {
    if (own) {
        return "This character is not downloaded: press N at the kit's start to get Snail's NPC characters.";
    }
    return (who.empty() ? std::string("A player") : who) +
           " plays a character you have not downloaded, shown as Link (press N at the kit's start to get Snail's NPC "
           "characters).";
}

// `pick` (the mod's stored number) can be loaded: 0 or less is Link; else the pack must have its file.
inline bool Installed(std::int32_t pick, const std::map<std::uint16_t, std::uint16_t>& files) {
    return pick <= 0 || (pick <= 0xFFFF && files.count(static_cast<std::uint16_t>(pick)) != 0);
}

// Our own pick.
class Own {
public:
    struct Step {
        std::optional<std::int32_t> set;   // the number to store at once in place of the pick
        bool say = false;                  // say MissingLine(true) now
    };
    // Each frame with the pick read (nothing: no such flag, no mod).
    Step Check(std::optional<std::int32_t> pick, const std::map<std::uint16_t, std::uint16_t>& files) {
        Step s;
        if (!pick) return s;
        if (Installed(*pick, files)) {
            last_good_ = *pick;
            told_ = 0;
            return s;
        }
        s.set = Installed(last_good_, files) ? last_good_ : 0;
        s.say = told_ != *pick;   // (the same missing pick again at once, as the page re-stores it: no second line)
        told_ = *pick;
        return s;
    }

private:
    std::int32_t last_good_ = 0;
    std::int32_t told_ = 0;
};

// Other players' picks: true when the line is to be said for `number` now.
class Others {
public:
    static constexpr std::size_t kNumbers = 17;   // player numbers 1..16
    bool Tell(std::uint8_t number, std::uint16_t character, const std::map<std::uint16_t, std::uint16_t>& files) {
        if (number >= kNumbers) return false;
        if (Installed(character, files)) {
            told_[number] = 0;
            return false;
        }
        if (told_[number] == character) return false;
        told_[number] = character;
        return true;
    }

private:
    std::array<std::uint16_t, kNumbers> told_{};
};

// Rows 239/240/241 (0.11.9, black-join-0119): a SAVE stored with a pick whose model is not here (a host's save worn
// with one of Snail's NPC characters, or the player's own save from before the NPC download was opt-in) never showed
// the world. The mod's load sequence starts building Link from the saved number as the load begins; Own's put-back
// came on that load's first frame, mid-sequence, and the sequence never finished: the same frame at 60 fps for good
// (E'tykalu's logs: "pick 93 ... back to 0" then ~106 draws a frame and one load revert a frame, 25 minutes). So:
// 1. the pick is put right in the save FILE before the game reads it (FixSavePick: the host's save a guest writes, and
//    the player's own slots each time the title comes up);
// 2. Own's put-back never runs from a save load's start until Link has been in play for LoadHold::kSettleFrames.

// The saved game's records (tools/autotest/save_flags.py, all little-endian): magic 0x01020304, the pool's offset at
// 8, from 0x20 a table of 8-byte records. A record whose first word is 0 opens a type (its second word the type id,
// 0 to 0x1f in order, then the end record 0x20); any other first word is a value's hash, and for the inline types
// (Int is type 2) the second word is the value. There is no checksum. The offset of the Int `hash`'s value word, or
// nothing (not a save, a table out of order, or no such Int).
inline std::optional<std::size_t> SaveIntAt(const std::uint8_t* data, std::size_t size, std::uint32_t hash) {
    constexpr std::uint32_t kMagic = 0x01020304;
    constexpr std::size_t kTable = 0x20;
    constexpr std::uint32_t kInt = 2;
    constexpr std::uint32_t kEnd = 0x20;
    const auto word = [&](std::size_t at) {
        return static_cast<std::uint32_t>(data[at]) | static_cast<std::uint32_t>(data[at + 1]) << 8 |
               static_cast<std::uint32_t>(data[at + 2]) << 16 | static_cast<std::uint32_t>(data[at + 3]) << 24;
    };
    if (data == nullptr || size < kTable || word(0) != kMagic) return std::nullopt;
    const std::size_t pool = word(8);
    if (pool < kTable || pool > size) return std::nullopt;
    std::optional<std::uint32_t> kind;
    for (std::size_t at = kTable; at + 8 <= pool; at += 8) {
        const std::uint32_t first = word(at), second = word(at + 4);
        if (first == 0) {
            if (second != (kind ? *kind + 1 : 0)) return std::nullopt;
            if (second == kEnd) return std::nullopt;
            kind = second;
        } else if (!kind) {
            return std::nullopt;
        } else if (*kind == kInt && first == hash) {
            return at + 4;
        }
    }
    return std::nullopt;
}

// What FixSavePick did: the pick the save had (nothing: no such value, no mod's layout) and whether it was changed.
struct SaveFix {
    std::optional<std::int32_t> was;
    bool changed = false;
};
// A progress.sav's bytes: a stored pick whose model is not installed becomes 0 (Link), in place. `pick_hash`: the
// flag_hash of CharacterMenu_SelectedCharacter.
inline SaveFix FixSavePick(std::uint8_t* data, std::size_t size, std::uint32_t pick_hash,
                           const std::map<std::uint16_t, std::uint16_t>& files) {
    SaveFix out;
    const auto at = SaveIntAt(data, size, pick_hash);
    if (!at) return out;
    std::uint32_t raw = 0;
    for (int i = 3; i >= 0; --i) raw = raw << 8 | data[*at + static_cast<std::size_t>(i)];
    out.was = static_cast<std::int32_t>(raw);
    if (Installed(*out.was, files)) return out;
    for (int i = 0; i < 4; ++i) data[*at + static_cast<std::size_t>(i)] = 0;
    out.changed = true;
    return out;
}

// Own's put-back held from a save load's start until Link has been in play for kSettleFrames frames in a row after it
// (the mod's load sequence done). Also the watchdog: a load whose Link never comes for kStuckFrames says so once.
class LoadHold {
public:
    static constexpr std::uint32_t kSettleFrames = 60;
    static constexpr std::uint32_t kStuckFrames = 60 * 60;   // a minute at 60 fps
    void LoadStarted() {
        held_ = true;
        settled_ = 0;
        waiting_ = 0;
        told_stuck_ = false;
    }
    // Each frame: `link` is Link in play (the player's reading). True when Own may put a pick back this frame.
    bool Frame(bool link) {
        if (!held_) return true;
        if (link) {
            waiting_ = 0;
            if (++settled_ >= kSettleFrames) held_ = false;
        } else {
            settled_ = 0;
            ++waiting_;
        }
        return !held_;
    }
    // Once per load: Link has not come for kStuckFrames since the load started (or since it last went).
    bool Stuck() {
        if (!held_ || told_stuck_ || waiting_ < kStuckFrames) return false;
        told_stuck_ = true;
        return true;
    }
    bool held() const { return held_; }

private:
    bool held_ = false;
    bool told_stuck_ = false;
    std::uint32_t settled_ = 0;
    std::uint32_t waiting_ = 0;
};

// The chat line when a load has not shown Link for a minute (LoadHold::Stuck).
inline std::string StuckLine() {
    return "The game has been loading for a minute. If the screen stays black, close the game, start it again with "
           "Start Squadala and send /bugreport.";
}

}  // namespace CoopCharSlots
