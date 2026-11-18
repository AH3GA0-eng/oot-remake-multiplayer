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

// The running event's own name-like strings (CoopEventMember::ReadRunning, read when it began).
inline void RunNames(const std::vector<std::string>& names, std::uint64_t now_us) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    b.tracker.RunNames(names, now_us);
}

// This frame's kind from the raw facts (the frame without a session reads no full reading).
inline coop::host::EventWhose Classify(bool running, bool link_bound, const std::optional<std::string_view>& anim,
                                       std::uint64_t now_us, coop::host::EventOwnerTracker::By* by = nullptr) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    const auto whose = b.tracker.Frame(running, link_bound, now_us, anim);
    if (by) *by = b.tracker.Decided();
    return whose;
}

// Sets reading.event_whose for this frame's reading. Returns it.
inline coop::host::EventWhose Apply(coop::host::PlayerReading& reading, std::uint64_t now_us) {
    Box& b = The();
    std::lock_guard lock(b.mutex);
    reading.event_whose = b.tracker.Frame(
        reading.event_running.value_or(false), reading.link_bound, now_us,
        coop::host::name_of(reading.animation));
    return reading.event_whose;
}

}  // namespace CoopEventOwner
