// SPDX-License-Identifier: GPL-3.0-or-later
//
// Each game's own quests catch up with the shared story (issue 173, docs/flags.md
// "Story and cutscenes"). The story scenes are shared flags (coop_story), but the
// game's quests ("Challenge" table: Goto_HyruleCastle, DoorOfTime, ...) are saved
// Enums each game keeps for itself: their values are hashes of step names, not
// ordered, so they cannot be shared as a max. A scene that moves a quest on
// (Dm_HrlCastle_0020 sets Goto_HyruleCastle Step04, the meeting with Zelda sets
// Goto_HyruleCastle Complete and DoorOfTime Step01) is skipped in a game where
// another player already played it, and that game's quest stays behind: after one
// player met Zelda, the other player's Impa never came (Impa's courtyard schedule
// and her "can be stopped" event belong to DoorOfTime Step01). So each game raises
// its own quests to what the shared scenes say. No emulator code: tested on its
// own (tests/lichii/quest_test.cpp).
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "coop_puppet.h"

namespace CoopQuest {

// A shared story flag (a saved Bool) that, once 1, means the quest is at least
// at `step`.
struct Rule {
    std::uint32_t flag;
    std::uint32_t step;  // Murmur3 of the step name
    const char* why;
};

// One quest: its Enum (hash from GameDataList, matched by its value list), its
// steps in the game's order (GameDataList Values = the Challenge table's Steps,
// NotReady first, Complete last), and its rules.
struct Quest {
    std::uint32_t hash;
    const char* name;
    std::span<const std::uint32_t> steps;
    std::span<const Rule> rules;
    // The lowest step our own value must already have for a raise (index into
    // steps): a game that has not got this far in its own story is left alone.
    std::size_t floor;
    // Raised only when this other quest of ours is at `needs_step` or later
    // (0 = no such need).
    std::uint32_t needs_quest = 0;
    std::uint32_t needs_step = 0;
};

std::span<const Quest> Quests();

// Index of `value` in `quest.steps`, or nullopt for a value we do not know.
std::optional<std::size_t> Rank(const Quest& quest, std::uint32_t value);

using ReadBool = std::function<std::optional<bool>(std::uint32_t)>;
using ReadEnum = std::function<std::optional<std::uint32_t>(std::uint32_t)>;

// The step to write into `quest`, or nothing: the highest step a set rule
// flag gives, if it is above our value. Never lowers; an unread or unknown
// value, a value under `floor` or an unmet need leaves the quest alone.
std::optional<std::uint32_t> CatchUp(const Quest& quest, const ReadBool& flag,
                                     const ReadEnum& value);

// The game's own quest state (issue 171). The game keeps each quest twice: the
// saved Enum (GameData, what co-op writes and shares) and ChallengeMgr's entry,
// which the game's logic reads: the Kakariko gate's shutter
// (Logic/KakarikoVillage_Child_GateClose_Prefab_6b8b: node
// HoldChallengeCurrentStepCheckOrder, Get_StoneOfFire against
// KakarikoVillageLetter) reads the manager, never GameData. A step co-op wrote
// into GameData therefore also goes to the manager, through the game's own
// ChallengeMgr::setStep, on the game's calc thread (a CoopCallers caller).
//
// Build e0f3ccfa (main offsets; read from the game's code):
//   ChallengeMgr = [main + 0x4e29950] (the game loads it through its GOT slot
//     main+0x4c7f9f0, as setStep's callers at main+0x31efbe0 and +0x32af0f8 do);
//     three entry arrays: count (i32) at +0x40/+0x50/+0x60, pointer at
//     +0x48/+0x58/+0x68, entries 0x160 bytes: +0x08 the challenge's hash
//     (Murmur3 of its name), +0x10 the step it is at (a step name's hash),
//     +0x16 flags (0x400: a step was asked), +0xa4 the step asked.
//   setStep = main + 0x2cf54a0 (mgr, challenge, step, w3, w4, force): finds
//     the entry, refuses from NotReady or Complete unless force, does nothing
//     when the step is the one it is at, else asks for the step (+0xa4, flag
//     0x400) for the manager's own update to apply. The game's own calls pass
//     w3 = w4 = 0.
inline constexpr std::uint64_t kChallengeMgr = 0x4e29950;
inline constexpr std::uint64_t kSetStep = 0x2cf54a0;

// One quest whose GameData step co-op holds: where the manager should be.
struct StepWant {
    std::uint32_t challenge;            // Murmur3 of the quest's name (Get_StoneOfFire)
    std::uint32_t step;                 // the step GameData holds (a step name's hash)
    std::vector<std::uint32_t> steps;   // the quest's steps in order, NotReady first
    std::string name;                   // for the log
};

// A boss fight or minigame step (RSDB Challenge IsBossBattle / IsMiniGame):
// the manager is never moved into one (the player gets there by playing).
bool HeldStep(std::uint32_t challenge, std::uint32_t step);

// Moves the manager's entries up to the steps GameData holds. Never lowers,
// never into a held step, an unknown step or a quest the manager does not have
// is left alone, and a step already asked is not asked again.
class Challenges {
public:
    // Any thread: the quests to check on the next frame (replaces any waiting).
    void Want(std::vector<StepWant> wants);
    // The calc thread, as a CoopCallers caller: the next setStep call, or
    // nothing. `main` is main's guest address.
    std::optional<CoopPuppet::Call> Step(CoopPuppet::Guest& guest, bool after_call,
                                         std::uint64_t result, std::uint64_t main);
    // Lines for the log ("Get_StoneOfFire 0x... -> 0x... asked").
    std::vector<std::string> TakeNotes();

private:
    std::mutex mutex_;
    std::vector<StepWant> waiting_;
    std::vector<StepWant> work_;  // calc thread only
    std::size_t next_ = 0;
    std::uint32_t asked_from_ = 0;
    std::vector<std::string> notes_;
};

} // namespace CoopQuest
