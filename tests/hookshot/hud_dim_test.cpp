// SPDX-License-Identifier: GPL-3.0-or-later
// The HUD blocks' dim look held steady (hookshot/src/coop_hud_dim.h).
#include <doctest/doctest.h>

#include <cstdint>

#include "coop_hud_dim.h"

using CoopHudDim::Hold;
using CoopHudDim::kSteadyUs;

TEST_CASE("hud dim: a new player takes its state at once") {
    Hold h;
    CHECK_FALSE(h.Dim(2, true, 0));
    CHECK(h.Dim(3, false, 0));
}

TEST_CASE("hud dim: a state flipping every frame never flashes the block") {
    Hold h;
    CHECK_FALSE(h.Dim(2, true, 0));
    // A slow game: "quiet" and "connected" by turns, 16 ms apart, for 5 s.
    for (std::uint64_t t = 16'000, i = 0; t < 5'000'000; t += 16'000, ++i) {
        CHECK_FALSE(h.Dim(2, i % 2 == 0 ? false : true, t));
    }
}

TEST_CASE("hud dim: flips a few frames long, under the steady time, keep the look") {
    Hold h;
    CHECK_FALSE(h.Dim(2, true, 0));
    std::uint64_t t = 0;
    for (int round = 0; round < 10; ++round) {
        for (int f = 0; f < 20; ++f) {  // 320 ms quiet
            t += 16'000;
            CHECK_FALSE(h.Dim(2, false, t));
        }
        t += 16'000;
        CHECK_FALSE(h.Dim(2, true, t));  // one sample came in
    }
}

TEST_CASE("hud dim: a lasting change shows after the steady time, both ways") {
    Hold h;
    CHECK_FALSE(h.Dim(2, true, 0));
    CHECK_FALSE(h.Dim(2, false, 1'000'000));
    CHECK_FALSE(h.Dim(2, false, 1'000'000 + kSteadyUs - 1));
    CHECK(h.Dim(2, false, 1'000'000 + kSteadyUs));
    CHECK(h.Dim(2, true, 3'000'000));
    CHECK(h.Dim(2, true, 3'000'000 + kSteadyUs - 1));
    CHECK_FALSE(h.Dim(2, true, 3'000'000 + kSteadyUs));
}

TEST_CASE("hud dim: players are held apart, and a forgotten one starts fresh") {
    Hold h;
    CHECK_FALSE(h.Dim(2, true, 0));
    CHECK(h.Dim(3, false, 0));
    CHECK(h.Dim(3, true, 100));  // 3 back, not yet steady: still dim
    CHECK_FALSE(h.Dim(2, false, 100));
    h.Forget(3);
    CHECK_FALSE(h.Dim(3, true, 200));  // fresh: at once
    CHECK(h.Dim(Hold::kSlots + 1, false, 0));  // out of range: as given
    CHECK_FALSE(h.Dim(0, true, 0));
}
