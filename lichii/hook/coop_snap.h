// SPDX-License-Identifier: GPL-3.0-or-later
//
// Issue 194 (docs/puppet.md "Cutscenes without snaps"): the other player's puppet snapped
// after cutscenes. Two causes, both in coop_puppet.cpp:
// - While their cutscene ran the puppet was held where the cutscene began (Show::hold), so
//   when it ended the puppet jumped to where the event had moved them (through a door, down a
//   ladder). Now a held puppet keeps its frozen frame but follows their place.
// - Any other jump in the place we are given (a late sample after a stall) was drawn in one
//   frame. Now a move of the target of more than kSnapM in one frame and less than kWarpM is
//   eased out (EaseMs): the drawn place stays where the puppet was and closes the gap while
//   it follows the new place. Longer jumps are real warps and are drawn at once; so is any
//   jump while their cutscene runs (a real teleport then).
// Logs (coop_puppet.cpp, Director::DrawAt): every target jump ("P<n> target jump <m> m
// (<show|hold>, eased|at once)"), and the Meter on the puppet's own place read back from the
// game each frame ("P<n> snap <m> m (<show|hold|warp>)"), so a run's log says both what we
// were handed and what the game drew.
// No emulator code: tested in tests/lichii/snap_test.cpp.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace CoopSnap {

using Vec3 = std::array<float, 3>;

// A drawn step longer than this in one frame is a snap. Running is about 0.2 m a frame at
// 30 frames a second, a roll about 0.3 m.
inline constexpr float kSnapM = 0.75f;
// Longer than this is a warp (a scene's spawn point, a teleport): drawn at once.
inline constexpr float kWarpM = 20.0f;
// How long a jump takes to close: at least kEaseMs, longer gaps at kEaseSpeed, so no drawn
// frame steps past kSnapM at 20 frames a second or more.
inline constexpr std::uint64_t kEaseMs = 250;
inline constexpr float kEaseSpeed = 15.0f;  // metres a second
// A new puppet is made where its player was a few frames before it shows: at most this far.
inline constexpr float kSpawnLagM = 3.0f;
// jitter-b (owner-deku 20261114-100649: "target jump 0.8-2.3 m (show, eased)" while the game ran at
// 7-18 fps): a frame that took long moves a running player well past kSnapM in that one frame,
// and each such frame began a new ease, so the puppet was held back and let go over and over.
// A move counts as a jump only past kSnapM plus what kMoveSpeed covers in the frame's time
// (at most kMoveFrameMs); the first Step after Set or Spawned has no frame time: kSnapM alone.
inline constexpr float kMoveSpeed = 10.0f;  // metres a second: a run, a roll, a horse
inline constexpr std::uint64_t kMoveFrameMs = 250;

inline std::uint64_t EaseMs(float gap) {
    const float ms = gap / kEaseSpeed * 1000.0f;
    if (!std::isfinite(ms) || ms <= static_cast<float>(kEaseMs)) {
        return kEaseMs;
    }
    return static_cast<std::uint64_t>(ms);
}

inline float Distance(const Vec3& a, const Vec3& b) {
    float d2 = 0.0f;
    for (std::size_t i = 0; i < 3; ++i) {
        const float d = a[i] - b[i];
        d2 += d * d;
    }
    return std::sqrt(d2);
}

// The place a puppet is drawn at, one per puppet.
class Smoother {
public:
    // Where to draw it this frame, for `target` (where its player is now). A jump is the
    // target's own move since the last frame (not the drawn step, so a long ease plus their
    // running never starts a new ease). With `ease` a jump of kSnapM to kWarpM is eased;
    // without (their cutscene: samples move smoothly then, so a jump is a real teleport, a
    // void-out or the event's start mark) and past kWarpM it is drawn at once.
    // `jump` gets the target's move when it was a jump (0 none).
    Vec3 Step(const Vec3& target, std::uint64_t now_ms, bool ease = true, float* jump = nullptr) {
        if (jump != nullptr) {
            *jump = 0.0f;
        }
        for (const float c : target) {
            if (!std::isfinite(c)) {
                return target;
            }
        }
        from_spawn_ = std::exchange(spawned_, false);
        if (!drawn_ || !last_target_) {
            Set(target);
            last_ms_ = now_ms;
            return target;
        }
        const float moved = Distance(target, *last_target_);
        last_target_ = target;
        float allowed = kSnapM;
        if (last_ms_ && now_ms >= *last_ms_) {
            allowed += kMoveSpeed * static_cast<float>(std::min(now_ms - *last_ms_, kMoveFrameMs)) / 1000.0f;
        }
        last_ms_ = now_ms;
        // Where the current ease draws it now.
        Vec3 now = target;
        if (offset_) {
            const std::uint64_t gone = now_ms - start_ms_;
            if (now_ms < start_ms_ || gone >= ease_ms_) {
                offset_.reset();
            } else {
                const float left = 1.0f - static_cast<float>(gone) / static_cast<float>(ease_ms_);
                for (std::size_t i = 0; i < 3; ++i) {
                    now[i] = target[i] + (*offset_)[i] * left;
                }
            }
        }
        if (moved > allowed && std::isfinite(moved)) {
            if (jump != nullptr) {
                *jump = moved;
            }
            if (ease && moved < kWarpM) {
                // Drawn where it was this frame; the gap closes over EaseMs.
                Vec3 off{};
                for (std::size_t i = 0; i < 3; ++i) {
                    off[i] = (*drawn_)[i] - target[i];
                }
                offset_ = off;
                start_ms_ = now_ms;
                ease_ms_ = EaseMs(Distance(*drawn_, target));
                now = *drawn_;
            } else {
                offset_.reset();
                now = target;
            }
        }
        drawn_ = now;
        return now;
    }

    // A new puppet the game made at `made_at` (where its player was when it was asked for, a
    // few frames ago): drawn from there, so the first Step eases to where the player is now
    // instead of a one-frame step (the 0.8 m spawn pop of cutscene-snap, 2026-11-12). Only
    // before the first Step, and only when `made_at` is within kSpawnLagM of `target` (a few
    // frames of running): a place read from a half-made actor is not eased from.
    void Spawned(const Vec3& made_at, const Vec3& target) {
        if (!drawn_ && Distance(made_at, target) <= kSpawnLagM) {
            Set(made_at);
            spawned_ = true;
        }
    }
    // The last Step was a new puppet's first, eased from where the game made it: its jump is
    // that catch-up, not a move of the player (DrawAt does not log it as a target jump).
    bool FromSpawn() const { return from_spawn_; }

    // Drawn here from now on, no ease (a new puppet; a hide: it is out of sight).
    void Set(const Vec3& at) {
        drawn_ = at;
        last_target_ = at;
        last_ms_.reset();
        offset_.reset();
    }
    void Reset() {
        drawn_.reset();
        last_ms_.reset();
        last_target_.reset();
        offset_.reset();
        spawned_ = from_spawn_ = false;
    }
    bool Easing() const { return offset_.has_value(); }
    const std::optional<Vec3>& Drawn() const { return drawn_; }

private:
    std::optional<Vec3> drawn_;
    std::optional<Vec3> last_target_;
    std::optional<Vec3> offset_;  // drawn minus target when the ease began
    std::optional<std::uint64_t> last_ms_;  // the last Step's time; unset after Set
    std::uint64_t start_ms_ = 0;
    std::uint64_t ease_ms_ = kEaseMs;
    bool spawned_ = false;     // Spawned, no Step since
    bool from_spawn_ = false;  // the last Step was the first after Spawned
};

// The drawn steps of one puppet: Step returns the snap to log, if any.
class Meter {
public:
    // `drawn` is where it was drawn this frame. A step past kSnapM is returned.
    std::optional<float> Step(const Vec3& drawn) {
        std::optional<float> snap;
        if (last_) {
            const float d = Distance(drawn, *last_);
            if (std::isfinite(d) && d > kSnapM) {
                snap = d;
                ++snaps_;
                if (d < kWarpM) {
                    ++near_snaps_;
                }
            }
        }
        last_ = drawn;
        return snap;
    }
    void Reset() { last_.reset(); }
    int Snaps() const { return snaps_; }
    int NearSnaps() const { return near_snaps_; }  // not warps

private:
    std::optional<Vec3> last_;
    int snaps_ = 0;
    int near_snaps_ = 0;
};

}  // namespace CoopSnap
