// SPDX-License-Identifier: GPL-3.0-or-later
//
// jitter-b: the time of the game frame whose state a present reads. Co-op reads the player and
// the objects at each present (the present thread), but the game moves them on its own frames
// (the game thread, 30 a second while presents run at 60): a reading taken 0 to 16 ms after
// the frame that made it was stamped with the present's time, so the samples' times were off
// by a different amount each time and the other games drew uneven steps. Hookshot calls
// Note() at each game frame (HOS_COOP_SITE_FRAME); a present takes the newest frame's time
// (BeginPresent), and the samples are stamped with it (coop::host::frame_aligned_us).
// Where Note() is never called (the emulator), the age is 0 and the present's time is used,
// as before. No emulator or game types here; tests/lichii/follow_test.cpp.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace CoopFrameClock {

// A frame older than this is not the one read (a pause, a load): the present's own time. 250 ms
// keeps 4 fps games on frame stamps (review-jitter: 50 ms turned the fix off under 20 fps).
inline constexpr std::uint64_t kFreshUs = 250'000;

inline std::uint64_t SteadyUs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

// How long before `now_us` the frame at `frame_us` ran; 0 none, or not fresh.
inline std::uint64_t Age(std::uint64_t now_us, std::uint64_t frame_us) {
    if (frame_us == 0 || now_us < frame_us || now_us - frame_us > kFreshUs) {
        return 0;
    }
    return now_us - frame_us;
}

inline std::atomic<std::uint64_t> g_frame_us{0};    // the newest game frame (steady us)
inline std::atomic<std::uint64_t> g_present_age_us{0};
inline std::atomic<std::uint64_t> g_present_at_us{0};  // the steady time the present's readings are for

// Game thread, at each game frame.
inline void Note() { g_frame_us.store(SteadyUs()); }

// Present thread, before the readings of this present.
inline void BeginPresent() {
    const std::uint64_t now = SteadyUs();
    const std::uint64_t age = Age(now, g_frame_us.load());
    g_present_age_us.store(age);
    g_present_at_us.store(now - age);
}

// This present's age (0: the present's own time) and the steady ms its readings are for.
inline std::uint64_t PresentAgeUs() { return g_present_age_us.load(); }
inline std::uint64_t PresentAtMs() { return g_present_at_us.load() / 1000; }

}  // namespace CoopFrameClock
