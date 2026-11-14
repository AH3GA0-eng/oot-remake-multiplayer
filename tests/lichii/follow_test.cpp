// Tests for the even-step follower of puppets and townspeople (lichii/hook/coop_follow.h, jitter-b).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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

TEST_CASE("follow: ahead is capped, a target from the future is not run back, bad input passes through") {
    Follower f;
    // Worked out 500 ms ago (a stall): taken forward by at most kAheadMaxMs.
    const Vec3 first = f.Step({0.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, 1000, 1500);
    CHECK(first[0] == doctest::Approx(6.0f * CoopFollow::kAheadMaxMs / 1000.0f));
    Follower g;
    const Vec3 at{1.0f, 0.0f, 0.0f};
    CHECK(g.Step(at, {6.0f, 0.0f, 0.0f}, 2000, 1990) == at);  // made after now: no ahead
    CHECK(g.Step(at, {6.0f, 0.0f, 0.0f}, 0, 1990) == at);     // same frame twice: unchanged
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
