// Tests for the even-step follower of puppets and townspeople (lichii/hook/coop_follow.h, jitter-b).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "coop_follow.h"
#include "coop_frame_clock.h"

using CoopFollow::Follower;
using CoopFollow::Vec3;

namespace {

constexpr float kSpeed = 6.0f;  // a run, metres a second

// Where a runner along x is at `ms`.
Vec3 RunnerAt(std::uint64_t ms) {
    return {kSpeed * static_cast<float>(ms) / 1000.0f, 0.0f, 0.0f};
}

}  // namespace

TEST_CASE("follow: steady running is drawn in even steps although the place is worked out at uneven times") {
    // The place is worked out at the last present before each game frame: 0 to 16 ms before it,
    // changing from frame to frame (the 0.11 jitter). Drawn at 33 ms game frames.
    Follower f;
    const std::uint64_t made_before[] = {0, 16, 3, 12, 16, 0, 8, 15, 1, 10};
    std::vector<float> steps;
    float last = 0.0f;
    for (int i = 0; i < 40; ++i) {
        const std::uint64_t now = 1000 + 33 * static_cast<std::uint64_t>(i);
        const std::uint64_t made = now - made_before[i % 10];
        const Vec3 drawn = f.Step(RunnerAt(made), {kSpeed, 0.0f, 0.0f}, made, now);
        if (i > 0) {
            steps.push_back(drawn[0] - last);
        }
        last = drawn[0];
        // No lag while running: drawn where the runner is at this frame.
        CHECK(drawn[0] == doctest::Approx(RunnerAt(now)[0]).epsilon(0.001));
    }
    const auto [lo, hi] = std::minmax_element(steps.begin(), steps.end());
    CHECK(*hi - *lo < 0.01f);  // even steps (raw: 0.1 to 0.29 m)
}

TEST_CASE("follow: no overshoot and no lag on a stop") {
    Follower f;
    std::uint64_t now = 1000;
    for (int i = 0; i < 10; ++i, now += 33) {
        f.Step(RunnerAt(now), {kSpeed, 0.0f, 0.0f}, now, now);
    }
    // They stop dead: the place stays, velocity 0.
    const Vec3 stop = RunnerAt(now - 33);
    // Behind it by a late sample: the follower closes the gap without passing it.
    Vec3 drawn{};
    int frames = 0;
    for (; frames < 10; ++frames, now += 33) {
        drawn = f.Step(stop, {0.0f, 0.0f, 0.0f}, now, now);
        CHECK(drawn[0] <= stop[0] + 1e-5f);
        if (drawn == stop) {
            break;
        }
    }
    CHECK(drawn == stop);  // exactly there
    CHECK(frames <= 2);

    // From behind (a gap left by a late sample), still: settles on it in a few frames, never past it.
    Follower g;
    now = 5000;
    g.Step({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, now, now);
    const Vec3 to{0.5f, 0.0f, 0.0f};
    frames = 0;
    for (now += 33; frames < 20; ++frames, now += 33) {
        drawn = g.Step(to, {0.0f, 0.0f, 0.0f}, now, now);
        CHECK(drawn[0] <= to[0]);
        if (drawn == to) {
            break;
        }
    }
    CHECK(drawn == to);
    CHECK(frames <= 8);  // under 0.3 s
}

TEST_CASE("follow: velocity carrying it past the place is cut at the place") {
    Follower f;
    f.Step({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1000, 1000);
    // 2 cm ahead, but a velocity that would carry it 20 cm in a frame.
    const Vec3 drawn = f.Step({0.02f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 1033, 1033);
    CHECK(drawn[0] <= 0.02f + 1e-6f);
}

TEST_CASE("follow: a big jump or teleport is taken at once") {
    Follower f;
    f.Step({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1000, 1000);
    const Vec3 far{30.0f, 2.0f, -4.0f};
    CHECK(f.Step(far, {0.0f, 0.0f, 0.0f}, 1033, 1033) == far);
    const Vec3 near_jump{31.0f, 2.0f, -4.0f};  // past kJumpM: the caller eases it (coop_snap.h)
    CHECK(f.Step(near_jump, {0.0f, 0.0f, 0.0f}, 1066, 1066) == near_jump);
}

TEST_CASE("follow: ahead is capped both ways, bad input passes through") {
    Follower f;
    // Worked out 500 ms ago (a stall): taken forward by at most kAheadMaxMs.
    const Vec3 first = f.Step({0.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 1000, 1500);
    CHECK(first[0] == doctest::Approx(6.0f * CoopFollow::kAheadMaxMs / 1000.0f));
    Follower g;
    const Vec3 at{1.0f, 0.0f, 0.0f};
    // Made after now (an even Cadence time runs a little behind): taken back by the velocity.
    const Vec3 back = g.Step(at, {6.0f, 0.0f, 0.0f}, 2000, 1990);
    CHECK(back[0] == doctest::Approx(0.94f));
    CHECK(g.Step(at, {6.0f, 0.0f, 0.0f}, 0, 1990) == back);  // same frame twice: unchanged
    Follower h;
    const Vec3 far = h.Step({0.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 2000, 1000);
    CHECK(far[0] == doctest::Approx(-6.0f * CoopFollow::kAheadMaxMs / 1000.0f));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Vec3 bad{nan, 0.0f, 0.0f};
    CHECK(std::isnan(g.Step(bad, {0.0f, 0.0f, 0.0f}, 0, 3000)[0]));
    CHECK_FALSE(g.Drawn().has_value());
}

TEST_CASE("frame clock: age of the newest game frame") {
    CHECK(CoopFrameClock::Age(1'000'000, 0) == 0);           // no game frame seen (the emulator)
    CHECK(CoopFrameClock::Age(1'000'000, 990'000) == 10'000);
    CHECK(CoopFrameClock::Age(1'000'000, 1'000'500) == 0);   // noted after now
    CHECK(CoopFrameClock::Age(1'000'000, 1'000'000 - CoopFrameClock::kFreshUs - 1) == 0);  // a pause
}

// review-jitter: even steps at 60 fps (16 ms frames, a place per present) and at 10 fps (100 ms
// frames, each place worked out at one of the six presents before the frame).
TEST_CASE("follow: even steps at 60 fps and at 10 fps") {
    for (const std::uint64_t frame_ms : {16ull, 100ull}) {
        CAPTURE(frame_ms);
        Follower f;
        float last = 0.0f;
        std::vector<float> steps;
        for (int i = 0; i < 30; ++i) {
            const std::uint64_t now = 1000 + frame_ms * static_cast<std::uint64_t>(i);
            const std::uint64_t made = now - (frame_ms == 16 ? (i % 2) * 8 : (i * 37) % 95);
            const Vec3 drawn = f.Step(RunnerAt(made), {kSpeed, 0.0f, 0.0f}, made, now);
            CHECK(drawn[0] == doctest::Approx(RunnerAt(now)[0]).epsilon(0.001));
            if (i > 0) {
                steps.push_back(drawn[0] - last);
            }
            last = drawn[0];
        }
        const auto [lo, hi] = std::minmax_element(steps.begin(), steps.end());
        CHECK(*hi - *lo < 0.01f);
    }
}

namespace {

// The owner's B game (owner-retest-20261114): ~28 fps, frames 22 to 40 ms apart, now and then
// one over 40 ms. A fixed pattern, so the test is the same every run.
std::vector<std::uint64_t> OwnerFrames(int count) {
    static constexpr std::uint64_t kGaps[] = {24, 38, 27, 33, 23, 41, 30, 36, 22, 35, 29, 45, 26, 31, 39, 25};
    std::vector<std::uint64_t> at;
    std::uint64_t t = 10'000;
    for (int i = 0; i < count; ++i) {
        at.push_back(t);
        t += kGaps[i % 16];
    }
    return at;
}

// The Steps metric of a runner followed at those frames, on the raw or the Cadence time. The
// place is worked out at a present up to 16 ms before the frame (as on the game).
CoopFollow::StepsReport Run(bool even) {
    Follower f;
    CoopFollow::Cadence cadence;
    CoopFollow::Steps steps;
    std::optional<CoopFollow::StepsReport> last;
    int i = 0;
    for (const std::uint64_t now : OwnerFrames(400)) {
        const std::uint64_t made = now - static_cast<std::uint64_t>((i++ * 7) % 17);
        const double t = even ? cadence.At(now) : static_cast<double>(now);
        const Vec3 drawn = f.Step(RunnerAt(made), {kSpeed, 0.0f, 0.0f}, made, t);
        if (auto r = steps.Step(std::nullopt, drawn, now)) {
            last = r;
        }
    }
    REQUIRE(last.has_value());
    return *last;
}

}  // namespace

TEST_CASE("cadence: uneven game frames become even, it stays near the real time") {
    CoopFollow::Cadence c;
    const auto frames = OwnerFrames(300);
    double last = 0.0;
    std::vector<double> gaps;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const double t = c.At(frames[i]);
        CHECK(t <= static_cast<double>(frames[i]) + CoopFollow::kResyncMs);
        CHECK(std::fabs(t - static_cast<double>(frames[i])) < 40.0);
        if (i >= 60) {
            gaps.push_back(t - last);
        }
        last = t;
    }
    const auto [lo, hi] = std::minmax_element(gaps.begin(), gaps.end());
    CHECK(*lo > 25.0);  // raw: 22 to 45 ms
    CHECK(*hi < 36.0);
    CHECK(c.PeriodMs() == doctest::Approx(31.5).epsilon(0.08));
}

TEST_CASE("cadence: the same frame twice, a hitch and a clock going back") {
    CoopFollow::Cadence c;
    CHECK(c.At(1000) == 1000.0);
    CHECK(c.At(1033) > 1000.0);
    const double t = c.At(1066);
    CHECK(c.At(1066) == t);
    CHECK(c.At(1067) == t);  // a caller reading the clock per call, same frame
    CHECK(c.At(2000) == 2000.0);  // a load: from the real time again
    CHECK(c.At(500) == 500.0);    // back: afresh
}

TEST_CASE("cadence: slow even frames stay smooth, a resync never goes back") {
    for (const std::uint64_t frame_ms : {143ull, 250ull}) {  // 7 and 4 fps
        CAPTURE(frame_ms);
        CoopFollow::Cadence c;
        double last = c.At(1000);
        for (std::uint64_t i = 1; i < 40; ++i) {
            const std::uint64_t jitter = (i % 3) * 20;  // uneven too
            const double t = c.At(1000 + i * frame_ms + jitter);
            if (i > 10) {
                CHECK(t - last > 0.6 * static_cast<double>(frame_ms));
                CHECK(t - last < 1.4 * static_cast<double>(frame_ms));
            }
            last = t;
        }
    }
    // Slow frames, then a burst of fast ones, a hitch and slow again: the time never goes back.
    CoopFollow::Cadence c;
    std::uint64_t now = 1000;
    double last = c.At(now);
    const std::uint64_t gaps[] = {140, 150, 145, 10, 8, 9, 7, 400, 33, 33, 200, 5, 140, 6, 6, 6, 6, 300};
    for (int round = 0; round < 5; ++round) {
        for (const std::uint64_t g : gaps) {
            now += g;
            const double t = c.At(now);
            CHECK(t >= last);
            CHECK(t <= static_cast<double>(now) + 2.5 * 150.0);
            last = t;
        }
    }
}

TEST_CASE("follow: the owner's uneven frames: even steps on the Cadence time, not on the raw time") {
    const auto raw = Run(false);
    const auto even = Run(true);
    MESSAGE("uneven raw " << raw.uneven << " cadence " << even.uneven);
    CHECK(raw.uneven > 0.2f);   // the owner's "jitter a little, a little choppy"
    CHECK(even.uneven < 0.08f);
    CHECK(even.mean_step == doctest::Approx(raw.mean_step).epsilon(0.05));  // no slower
}

TEST_CASE("steps: the metric reads even steps, a wobble and a second writer") {
    CoopFollow::Steps s;
    std::optional<CoopFollow::StepsReport> r;
    for (std::uint64_t i = 0; i <= 160 && !r; ++i) {
        const float x = 0.2f * static_cast<float>(i) + (i % 2 ? 0.05f : 0.0f);  // 0.25 / 0.15 steps
        const Vec3 drawn{x, 0.0f, 0.0f};
        const Vec3 read{x - 0.15f, 0.0f, 0.0f};  // 0.15 m after our last write: the game moved it
        r = s.Step(i == 0 ? std::nullopt : std::optional<Vec3>(read), drawn, 1000 + i * 33);
    }
    REQUIRE(r.has_value());
    CHECK(r->frames > 100);
    CHECK(r->mean_step == doctest::Approx(0.2f).epsilon(0.02));
    CHECK(r->uneven > 0.4f);
    CHECK(r->uneven_max < 0.7f);
    CHECK(r->game_moved > 0.0f);
    CoopFollow::Steps even;
    std::optional<CoopFollow::StepsReport> e;
    for (std::uint64_t i = 0; i <= 160 && !e; ++i) {
        const Vec3 drawn{0.2f * static_cast<float>(i), 0.0f, 0.0f};
        e = even.Step(drawn, drawn, 1000 + i * 33);
    }
    REQUIRE(e.has_value());
    CHECK(e->uneven < 0.01f);
    CHECK(e->game_moved == doctest::Approx(0.2f).epsilon(0.02));  // read = this frame's draw
}
