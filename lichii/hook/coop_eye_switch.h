// SPDX-License-Identifier: GPL-3.0-or-later
//
// deku-switches (#176, #129; tracker #90, #38; plan reports/deku-ladder-a.md option (c)): another player shot an eye
// switch, so our copy of its room event runs the room's own way.
//
// The Deku Tree's slingshot-room eye (Obj_EyeSwitch_A_02) has an AI that, on a slingshot hit by its own player, sets
// its logic output plug FirstOn. The room graph (Logic/InsideTheDekuTree_Level_Room_C) reads FirstOn in an
// IsFirstOnEventTrigger node and raises the room asker's (EventRequestActor) EventRequestTrigger; the asker's AI
// then asks InsideTheDekuTree_Gimmick_FallingLadder ITSELF, and that event's EventTriggerSetEventBool drops the
// ladder; its end sets RoomC_Cleared, deletes the asker and moves the pillar. Asking the event from outside (by name,
// with the asker linked as starter, or through the asker's performer: coop_live_events.h) plays the cutscene only:
// the asker's AI is still waiting for its trigger and writes EventBool 0 every frame (deku-platforms-b2).
//
// So for these events we do not ask the event: we set our eye's FirstOn to 1 with the game's own set-bool
// (coop_logic_set.h), as the eye's AI would after our own shot, and the room and its asker do the rest. Only our
// eye's model stays open (its blackboard isActive is not ours to set). Each frame up to kTryMs until an event runs
// (the asker asked it) or the asker is gone; FirstOn already 1 when we come: our game did it, nothing to do. When
// the eye or its FirstOn plug is never found, or no event starts in time, the event goes back to the old way
// (Requester::WantFromRoom, TakeFallbacks): no worse than before.
//
// OFF unless LICHII_COOP_EYE_SWITCH=1 until the probe run (scenario hs-eye-probe) shows FirstOn's key on the eye,
// whether it latches, and that the asker's trigger follows it. No emulator code (tests/lichii/eye_switch_test.cpp).
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "coop_puppet.h"

namespace CoopEyeSwitch {

inline constexpr std::uint32_t kFirstOn = 0x2d9754fd;  // Murmur3("FirstOn"); the plug's key is ^ 7

// A room event started by an eye switch's FirstOn (romfs Logic/*_Room_*.logic.module.ainb).
struct Switch {
    std::string_view event;  // without "HavePlayedEvent."
    std::string_view scene;
    std::string_view eye;    // the eye's actor name
    bool every_eye = false;  // set every such eye near the asker (a room with two eyes for one event)
};
std::span<const Switch> Switches();
// The switch event `event` in `scene`, if it is one.
const Switch* Find(std::string_view event, std::string_view scene);

// The eye(s) are looked for this near the event's place (its asker, coop_live_places.cpp).
inline constexpr float kEyeNearM = 25.0f;
// How long the set is tried (and then waited for the asker's event) before the old way is taken.
inline constexpr std::uint64_t kTryMs = 2000;

// LICHII_COOP_EYE_SWITCH: "1" on; anything else off (the default until the probe run).
inline bool OnFromEnv(std::string_view value) { return value == "1"; }

// The present actors named one of `names` within `max_m` of `at` (x, height, z), nearest first. The calc thread.
using Finder = std::function<std::vector<std::uint64_t>(CoopPuppet::Guest&, std::uint64_t main,
                                                        std::span<const std::string_view> names,
                                                        const std::array<float, 3>& at, float max_m)>;
// The game's actor list (CoopFairy::WalkActors).
std::vector<std::uint64_t> FindActors(CoopPuppet::Guest& guest, std::uint64_t main,
                                      std::span<const std::string_view> names, const std::array<float, 3>& at,
                                      float max_m);

// The svc caller: one switch event at a time.
class Switcher {
public:
    explicit Switcher(Finder find = FindActors) : find_(std::move(find)) {}
    // The hook thread: start `event` in `scene` by its eye. False: not a switch event (ask it the old way).
    bool Want(const std::string& event, const std::string& scene);
    // The hook thread, once a frame: drops what was wanted in another scene than `scene`.
    void Expire(const std::string& scene);
    // The calc thread (CoopSvc caller contract).
    std::optional<CoopPuppet::Call> Step(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                         std::uint64_t main, std::uint64_t now_ms);
    std::vector<std::string> TakeNotes();
    // (event, scene) to ask the old way (Requester::WantFromRoom).
    std::vector<std::pair<std::string, std::string>> TakeFallbacks();

private:
    struct Try {
        std::string event;
        std::string scene;
        std::uint64_t until_ms = 0;  // 0: not begun
        bool set = false;            // FirstOn written at least once
        bool told_waiting = false;
    };
    std::optional<CoopPuppet::Call> Frame(CoopPuppet::Guest& guest, std::uint64_t main, std::uint64_t now_ms);
    void End(const std::string& why, bool fallback);
    static bool EventRunning(CoopPuppet::Guest& guest, std::uint64_t main);

    Finder find_;
    std::mutex mutex_;
    std::deque<Try> wanted_;  // front: the one being tried
    std::vector<std::string> notes_;
    std::vector<std::pair<std::string, std::string>> fallbacks_;
    std::uint64_t block_ = 0;
    enum class Next { none, malloc, execute };
    Next next_ = Next::none;
    bool failed_ = false;  // the malloc gave nothing: every event the old way
};

}  // namespace CoopEyeSwitch
