// SPDX-License-Identifier: GPL-3.0-or-later
//
// G1 (coop/host/event_owner.hpp): the one EventOwnerTracker of this game. The
// co-op layer's own requests for another player's event mark it (CoopAsked: the
// live-event Requester's taken asks, an eye switch set for another game) from
// the game's calc thread; the hook's frame feeds the game's own taken requests
// (GameAsked, Hookshot's wrap) and classifies each reading (Apply). One lock,
// held only for those few lines.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

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

inline void CoopAsked(std::string_view name, std::uint64_t now_us) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    b.tracker.CoopAsked(name, now_us);
}

inline void GameAsked(std::string_view name, std::uint64_t now_us) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    b.tracker.GameAsked(name, now_us);
}

// Sets reading.event_whose for this frame's reading. Returns it.
inline coop::host::EventWhose Apply(coop::host::PlayerReading& reading, std::uint64_t now_us) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    reading.event_whose = b.tracker.Frame(
        reading.event_running.value_or(false), reading.link_bound, now_us,
        reading.animation ? std::optional<std::string_view>(reading.animation->name) : std::nullopt);
    return reading.event_whose;
}

}  // namespace CoopEventOwner
