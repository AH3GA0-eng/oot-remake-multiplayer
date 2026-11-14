// SPDX-License-Identifier: GPL-3.0-or-later
#include <doctest/doctest.h>

#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "coop_hit_replay.h"

namespace {

// Guest memory as a byte map: reads of unwritten bytes give 0.
class FakeGuest : public CoopPuppet::Guest {
public:
    bool Read(std::uint64_t address, void* out, std::size_t size) override {
        auto* p = static_cast<std::uint8_t*>(out);
        for (std::size_t i = 0; i < size; ++i) {
            const auto it = bytes.find(address + i);
            p[i] = it == bytes.end() ? 0 : it->second;
        }
        return true;
    }
    bool Write(std::uint64_t address, const void* in, std::size_t size) override {
        const auto* p = static_cast<const std::uint8_t*>(in);
        for (std::size_t i = 0; i < size; ++i) bytes[address + i] = p[i];
        return true;
    }
    template <typename T>
    void Put(std::uint64_t address, T v) {
        Write(address, &v, sizeof v);
    }
    template <typename T>
    T Get(std::uint64_t address) {
        T v{};
        Read(address, &v, sizeof v);
        return v;
    }
    std::map<std::uint64_t, std::uint8_t> bytes;
};

constexpr std::uint64_t kActor = 0x1000'0000;
constexpr std::uint64_t kTable = 0x2000'0000;
constexpr std::uint64_t kLife = 0x3000'0000;

FakeGuest WithCopy() {
    FakeGuest g;
    g.Put<std::uint64_t>(kActor + CoopHitReplay::kActorComponents, kTable);
    g.Put<std::uint64_t>(kTable + CoopHitReplay::kComponentLife, kLife);
    g.Put<std::uint32_t>(kLife + CoopHitReplay::kLastHitId, 0x20);
    return g;
}

} // namespace

TEST_CASE("hit replay: run under the caller's own lock, as the Mover's Step does, never takes it again") {
    // boss-intro run 080505: Replay locked the Mover's mutex_ inside Step, which holds it: the game ended
    // (resource_deadlock_would_occur). The replay takes only its own locks, so it runs fine under any outer one,
    // and its ids are read from another thread's call while it is held.
    CoopHitReplay::Replayer r;
    FakeGuest g = WithCopy();
    CoopHitReplay::Hit h;
    h.actor = kActor;
    h.attacker = 0x5000;
    h.amount = 3;
    h.at = {18.5f, 61.5f, 67.8f};
    h.exact = true;
    r.Add(h, 1000);
    CHECK(r.Waiting() == 1);
    std::mutex step_lock;  // stands for the Mover's mutex_
    std::vector<CoopHitReplay::Written> written;
    {
        std::lock_guard lock(step_lock);
        written = r.Run(g, 1050, [&](std::uint64_t a) {
            CHECK(r.ReplayedId(kLife) == 0xffffffffu);  // (its ids, read inside the run: no deadlock either)
            return a == kActor;
        });
    }
    REQUIRE(written.size() == 1);
    CHECK(written[0].id == 0x22);
    CHECK(written[0].life == kLife);
    CHECK(r.ReplayedId(kLife) == 0x22);
    CHECK(r.Waiting() == 0);
    // The record: our id, present, the hit landed exactly where it was asked (no +1 for an exact place).
    CHECK(g.Get<std::uint32_t>(kLife + CoopHitReplay::kHitRecord + 8) == 0x22);
    CHECK(g.Get<std::uint32_t>(kLife + CoopHitReplay::kHitRecord + 0xc) == 1);
    CHECK(g.Get<float>(kLife + 0x27c + 4) == doctest::Approx(61.5f));
    CHECK(g.Get<std::int32_t>(kLife + CoopHitReplay::kHitAmount + 4) == 3);
    r.Forget();
    CHECK(r.ReplayedId(kLife) == 0xffffffffu);
}

TEST_CASE("hit replay: a late hit or a gone actor is dropped, nothing written") {
    CoopHitReplay::Replayer r;
    FakeGuest g = WithCopy();
    CoopHitReplay::Hit h;
    h.actor = kActor;
    h.amount = 1;
    r.Add(h, 1000);
    CHECK(r.Run(g, 1000 + CoopHitReplay::kHitLateMs, [](std::uint64_t) { return true; }).empty());
    r.Add(h, 2000);
    CHECK(r.Run(g, 2010, [](std::uint64_t) { return false; }).empty());
    CHECK(r.ReplayedId(kLife) == 0xffffffffu);
    CHECK(g.Get<std::uint32_t>(kLife + CoopHitReplay::kHitRecord + 0xc) == 0);
    for (int i = 0; i < 70; ++i) r.Add(h, 3000);
    CHECK(r.Waiting() == CoopHitReplay::Replayer::kQueued);
}
