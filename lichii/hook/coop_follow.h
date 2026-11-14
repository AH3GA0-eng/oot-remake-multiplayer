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
inline constexpr std::uint64_t kAheadMaxMs = 150;  // a 7 fps frame (review-jitter)
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
    // `now_ms` may be a Cadence time (fractional, and up to kAheadMaxMs before `target_ms`:
    // the place is then taken back by the velocity).
    Vec3 Step(const Vec3& target, const Vec3& velocity, std::uint64_t target_ms, double now_ms) {
        if (!Finite(target) || !Finite(velocity)) {
            Reset();
            return target;
        }
        const double ahead_ms =
            target_ms == 0 ? 0.0
                           : std::clamp(now_ms - static_cast<double>(target_ms),
                                        -static_cast<double>(kAheadMaxMs), static_cast<double>(kAheadMaxMs));
        Vec3 want = target;
        for (std::size_t i = 0; i < 3; ++i) {
            want[i] += velocity[i] * static_cast<float>(ahead_ms / 1000.0);
        }
        if (!drawn_ || now_ms < last_ms_) {
            return Set(want, now_ms);
        }
        const double dt_ms = std::min(now_ms - last_ms_, static_cast<double>(kMaxStepMs));
        if (dt_ms <= 0.0) {
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
    Vec3 Set(const Vec3& at, double now_ms) {
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
    double last_ms_ = 0.0;
};

// jitter-2 (owner re-test on 975a0258: "still jitter a little and seem a little choppy"). The
// game moves its world one even step a frame, but its frames come 22 to over 40 ms apart (the
// owner's B log: 'present intervals' at ~28 fps), and the Follower stepped by that raw time:
// the puppet's steps went 0.7x-1.5x frame to frame while the ground, the camera and the
// player's own Link moved evenly. The Cadence is the game frame's time made even: it goes on
// by the mean frame time and closes kPull of its distance to the real time each frame, so
// steps are even and it never drifts. A frame past kResyncMs off (a load, a hitch) starts it
// again from the real time. Once a game frame (several puppets in one frame: the same time).
inline constexpr double kPull = 0.1;
inline constexpr double kPeriodBlend = 0.1;
inline constexpr double kResyncMs = 150.0;
inline constexpr double kResyncFrames = 2.5;   // or this many mean frames, if more
inline constexpr double kSlowestMs = 500.0;    // a first frame time up to this is taken
inline constexpr std::uint64_t kSameFrameMs = 3;  // game frames are further apart than this

class Cadence {
public:
    double At(std::uint64_t now_ms) {
        const double now = static_cast<double>(now_ms);
        if (have_ && now_ms >= raw_ms_ && now_ms - raw_ms_ < kSameFrameMs) {
            return smooth_ms_;  // the same frame (a caller that reads the clock per call)
        }
        if (!have_ || now_ms < raw_ms_) {
            have_ = true;
            raw_ms_ = now_ms;
            period_ms_ = 0.0;
            return smooth_ms_ = now;
        }
        const double dt = now - static_cast<double>(raw_ms_);
        raw_ms_ = now_ms;
        // review-jitter2: the limit grows with the frame time, so slow but even frames (the
        // owner's 7 fps) stay smooth; a first frame up to kSlowestMs sets the frame time.
        const double base = period_ms_ != 0.0 ? period_ms_ : (dt <= kSlowestMs ? dt : 0.0);
        const double limit = std::max(kResyncMs, kResyncFrames * base);
        if (dt > limit) {
            return Resync(now);  // a hitch: the mean frame time stays
        }
        period_ms_ = period_ms_ == 0.0 ? dt : period_ms_ + (dt - period_ms_) * kPeriodBlend;
        const double guess = smooth_ms_ + period_ms_;
        const double off = now - guess;
        if (off > limit || off < -limit) {
            return Resync(now);
        }
        smooth_ms_ = std::max(guess + off * kPull, smooth_ms_);
        return smooth_ms_;
    }
    double PeriodMs() const { return period_ms_; }

private:
    // From the real time again, never back (review-jitter2: a step back made the Follower start
    // afresh, one frame unsmoothed); a time ahead of the real one waits for it.
    double Resync(double now) { return smooth_ms_ = std::max(now, smooth_ms_); }

    bool have_ = false;
    std::uint64_t raw_ms_ = 0;
    double smooth_ms_ = 0.0;
    double period_ms_ = 0.0;
};

// jitter-2: the proof metric. Each game frame the place drawn and the place the game had
// (read back before the write). Report() every kReportMs: frames, the mean step, how uneven
// the steps are (the mean |1 - step/last step| over frames that both moved, so a steady run
// reads 0 and a 0.5x/1.5x wobble 0.5), and how far the game itself moved the puppet between
// our writes (a second writer, its own root motion).
inline constexpr std::uint64_t kReportMs = 5000;
inline constexpr float kMovingM = 0.01f;

struct StepsReport {
    int frames = 0;
    int moving = 0;
    float mean_step = 0.0f;
    float uneven = 0.0f;      // mean |1 - step / last step|
    float uneven_max = 0.0f;  // the worst frame
    float game_moved = 0.0f;  // mean distance the game moved it between our writes
    float game_moved_max = 0.0f;
};

class Steps {
public:
    // `read` is where the game had it this frame (before the write), `drawn` what is written.
    std::optional<StepsReport> Step(const std::optional<Vec3>& read, const Vec3& drawn, std::uint64_t now_ms) {
        if (start_ms_ == 0 || now_ms < start_ms_) {
            start_ms_ = now_ms;
        }
        if (last_drawn_) {
            const float step = Follower::Distance(drawn, *last_drawn_);
            ++frames_;
            sum_step_ += step;
            if (step > kMovingM && last_step_ > kMovingM) {
                const float u = std::fabs(1.0f - step / last_step_);
                ++moving_;
                sum_uneven_ += u;
                uneven_max_ = std::max(uneven_max_, u);
            }
            last_step_ = step;
            if (read) {
                const float g = Follower::Distance(*read, *last_drawn_);
                ++reads_;
                sum_game_ += g;
                game_max_ = std::max(game_max_, g);
            }
        }
        last_drawn_ = drawn;
        if (now_ms - start_ms_ < kReportMs) {
            return std::nullopt;
        }
        StepsReport r;
        r.frames = frames_;
        r.moving = moving_;
        r.mean_step = frames_ ? sum_step_ / static_cast<float>(frames_) : 0.0f;
        r.uneven = moving_ ? sum_uneven_ / static_cast<float>(moving_) : 0.0f;
        r.uneven_max = uneven_max_;
        r.game_moved = reads_ ? sum_game_ / static_cast<float>(reads_) : 0.0f;
        r.game_moved_max = game_max_;
        const auto keep = last_drawn_;
        *this = Steps{};
        last_drawn_ = keep;
        start_ms_ = now_ms;
        return r;
    }
    // A jump, a hide, a new actor: the next frame starts afresh.
    void Break() {
        last_drawn_.reset();
        last_step_ = 0.0f;
    }

private:
    std::optional<Vec3> last_drawn_;
    float last_step_ = 0.0f;
    std::uint64_t start_ms_ = 0;
    int frames_ = 0, moving_ = 0, reads_ = 0;
    float sum_step_ = 0.0f, sum_uneven_ = 0.0f, uneven_max_ = 0.0f, sum_game_ = 0.0f, game_max_ = 0.0f;
};

}  // namespace CoopFollow
