// SPDX-License-Identifier: GPL-3.0-or-later
//
// jitter-b (0.11 playtest, 2026-11-14: "players are always constantly jittering a little on other
// players' screens", townspeople "extremely jittery and choppy"). The place of another player's
// puppet and of a townsperson copy is worked out on the present thread (presence / the object
// samples, interpolated at that moment) and written on the next game frame. The game runs its
// frames on its own thread, so the time between the two changes from frame to frame (0 to one
// present): each frame's step was a whole, half or one-and-a-half frame of their movement. The
// owner's samples are also stamped at their present, not at their game frame.
//
// A Follower per puppet or copy, stepped once a game frame, takes that place forward to the
// frame's own time (by the velocity it came with, at most kAheadMaxMs) and follows it with the
// velocity fed forward, closing what is left of the gap with a short exponential (kTauMs). So:
// - steady movement is drawn in even steps, with no lag (the velocity carries it);
// - it never passes the place it follows (no overshoot), and on a stop it settles there in a
//   few frames (kRestM: exactly there);
// - a gap past kJumpM (a late sample, a teleport) is taken at once: the caller's own easing
//   (coop_snap.h for puppets) or the game's warp handles it, as before.
// The result is written through the same game call as before (the setter the puppet and copy
// already use); nothing new is written. No emulator code: tests/lichii/follow_test.cpp.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace CoopFollow {

using Vec3 = std::array<float, 3>;

// The place is taken forward from when it was worked out by at most this.
inline constexpr std::uint64_t kAheadMaxMs = 50;
// What is left of the gap closes by 1 - e^(-dt/kTauMs) a frame (a 30 fps frame: 56 %).
inline constexpr float kTauMs = 40.0f;
// A gap past this (metres) is taken at once.
inline constexpr float kJumpM = 0.75f;
// Still and this close: exactly there.
inline constexpr float kRestM = 0.005f;
// A frame counts as at most this (a hitch, a load).
inline constexpr std::uint64_t kMaxStepMs = 100;

class Follower {
public:
    // Where to draw it this game frame (`now_ms`, steady clock). `target` is where it is as of
    // `target_ms` (the same clock; 0: now), moving at `velocity` metres a second.
    Vec3 Step(const Vec3& target, const Vec3& velocity, std::uint64_t target_ms, std::uint64_t now_ms) {
        if (!Finite(target) || !Finite(velocity)) {
            Reset();
            return target;
        }
        const std::uint64_t ahead_ms =
            target_ms == 0 || now_ms <= target_ms ? 0 : std::min(now_ms - target_ms, kAheadMaxMs);
        Vec3 want = target;
        for (std::size_t i = 0; i < 3; ++i) {
            want[i] += velocity[i] * static_cast<float>(ahead_ms) / 1000.0f;
        }
        if (!drawn_ || now_ms < last_ms_) {
            return Set(want, now_ms);
        }
        const std::uint64_t dt_ms = std::min(now_ms - last_ms_, kMaxStepMs);
        if (dt_ms == 0) {
            return *drawn_;  // twice in one frame
        }
        Vec3 drawn = *drawn_;
        if (Distance(drawn, want) > kJumpM) {
            return Set(want, now_ms);
        }
        // The velocity carries it; it never passes the place it follows.
        Vec3 next = drawn;
        float along = 0.0f;  // (want - next) . (want - drawn)
        for (std::size_t i = 0; i < 3; ++i) {
            next[i] += velocity[i] * static_cast<float>(dt_ms) / 1000.0f;
            along += (want[i] - next[i]) * (want[i] - drawn[i]);
        }
        if (along < 0.0f) {
            next = want;
        }
        const float close = 1.0f - std::exp(-static_cast<float>(dt_ms) / kTauMs);
        for (std::size_t i = 0; i < 3; ++i) {
            drawn[i] = next[i] + (want[i] - next[i]) * close;
        }
        if (Still(velocity) && Distance(drawn, want) <= kRestM) {
            drawn = want;
        }
        drawn_ = drawn;
        last_ms_ = now_ms;
        return drawn;
    }

    void Reset() { drawn_.reset(); }
    const std::optional<Vec3>& Drawn() const { return drawn_; }

    static float Distance(const Vec3& a, const Vec3& b) {
        float d2 = 0.0f;
        for (std::size_t i = 0; i < 3; ++i) {
            d2 += (a[i] - b[i]) * (a[i] - b[i]);
        }
        return std::sqrt(d2);
    }

private:
    Vec3 Set(const Vec3& at, std::uint64_t now_ms) {
        drawn_ = at;
        last_ms_ = now_ms;
        return at;
    }
    static bool Finite(const Vec3& v) {
        return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
    }
    static bool Still(const Vec3& v) {
        return std::fabs(v[0]) < 1e-3f && std::fabs(v[1]) < 1e-3f && std::fabs(v[2]) < 1e-3f;
    }

    std::optional<Vec3> drawn_;
    std::uint64_t last_ms_ = 0;
};

}  // namespace CoopFollow
