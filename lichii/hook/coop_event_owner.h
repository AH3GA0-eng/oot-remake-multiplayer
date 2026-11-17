// SPDX-License-Identifier: GPL-3.0-or-later
//
// G1 (coop/host/event_owner.hpp): the one EventOwnerTracker of this game. The
// hook's frame feeds the game's own taken requests (GameAsked, Hookshot's wrap)
// and classifies each reading (Apply) from our Link's event membership. One
// lock, held only for those few lines.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "coop/host/event_owner.hpp"
#include "coop/host/player_reader.hpp"

namespace CoopEventOwner {

struct Box {
    std::mutex mutex;
    coop::host::EventOwnerTracker tracker;
};

inline Box& The() {
    static Box box;
    return box;
}

inline void GameAsked(std::string_view name) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    b.tracker.GameAsked(name);
}

// This frame's kind from the raw facts (the frame without a session reads no full reading).
inline coop::host::EventWhose Classify(bool running, bool link_bound, std::optional<std::string_view> anim,
                                       coop::host::EventOwnerTracker::By* by = nullptr) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    const auto whose = b.tracker.Frame(running, link_bound, anim);
    if (by) *by = b.tracker.Decided();
    return whose;
}

// Sets reading.event_whose for this frame's reading. Returns it.
inline coop::host::EventWhose Apply(coop::host::PlayerReading& reading) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    reading.event_whose = b.tracker.Frame(
        reading.event_running.value_or(false), reading.link_bound,
        reading.animation ? std::optional<std::string_view>(reading.animation->name) : std::nullopt);
    return reading.event_whose;
}

}  // namespace CoopEventOwner
