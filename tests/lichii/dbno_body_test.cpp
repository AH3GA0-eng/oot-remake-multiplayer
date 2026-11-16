// Tests for our downed Link's body: crawl, shield, heal (lichii/hook/coop_dbno_body.cpp).
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <map>

#include "coop_dbno_body.h"

using namespace CoopDbno::Body;
namespace bn = CoopBossNative;

namespace {

class Guest : public CoopPuppet::Guest {
public:
    bool Read(std::uint64_t address, void* out, std::size_t size) override {
        auto* o = static_cast<std::uint8_t*>(out);
        for (std::size_t i = 0; i < size; ++i) {
            const auto it = bytes.find(address + i);
            o[i] = it == bytes.end() ? 0 : it->second;
        }
        return true;
    }
    bool Write(std::uint64_t address, const void* in, std::size_t size) override {
        const auto* b = static_cast<const std::uint8_t*>(in);
        for (std::size_t i = 0; i < size; ++i) {
            bytes[address + i] = b[i];
        }
        return true;
    }
    std::map<std::uint64_t, std::uint8_t> bytes;
};

constexpr std::uint64_t kBsa = 0x1000;
constexpr std::uint64_t kLife = 0x2000;
constexpr std::uint64_t kNode = 0x3000;
constexpr std::uint64_t kMain = 0x80000000;
constexpr std::uint64_t kBlock = 0x9000;

// The flags word behind the fake compare-and-swap.
std::uint32_t g_flags = 0;
int g_cas_calls = 0;
bool Cas(std::uint64_t address, std::uint32_t expected, std::uint32_t desired, std::uint32_t& seen) {
    ++g_cas_calls;
    if (address != kLife + addr::kLifeFlags) {
        return false;
    }
    seen = g_flags;
    if (g_flags == expected) {
        g_flags = desired;
    }
    return true;
}

bn::Request Req(std::int32_t verb, std::int32_t slot = 0, std::uint64_t node = kNode) {
    coop::protocol::BossAction a;
    a.verb = verb;
    a.slot = slot;
    a.flags = 3;
    return bn::RequestFromAction(a, node);
}

Crawl::Wanted Want(bool lying, bool event = false) {
    Crawl::Wanted w;
    w.lying = lying;
    w.event = event;
    w.bsa = kBsa;
    w.life = kLife;
    return w;
}

std::int32_t VerbOf(Guest& g) {
    std::int32_t verb = 0;
    g.Read(kBlock + bn::addr::kReqVerb, &verb, sizeof verb);
    return verb;
}

// Runs one frame of the caller: every call made is answered `result`
// (the malloc with kBlock). Returns the action verbs asked this frame.
std::vector<std::int32_t> RunFrame(Crawl& c, Guest& g, std::uint64_t now_ms, std::uint64_t result = 0) {
    std::vector<std::int32_t> asked;
    auto call = c.Next(g, false, 0, now_ms, kMain, &Cas);
    while (call) {
        std::uint64_t r = result;
        if (call->function == kMain + bn::addr::kMalloc) {
            r = kBlock;
        } else {
            CHECK(call->function == kMain + bn::addr::kRequestActionVerb);
            CHECK(call->x[0] == kBsa);
            CHECK(call->x[1] == kBlock);
            asked.push_back(VerbOf(g));
        }
        call = c.Next(g, true, r, now_ms, kMain, &Cas);
    }
    return asked;
}

}  // namespace

TEST_CASE("dbno body: a raise goes into the Life step as a negative sum") {
    CHECK(Raised(1, 0, 12) == -11);    // 1 -> 12: the step heals 11
    CHECK(Raised(1, 2, 12) == -11);    // a hit the same frame does not keep it lower
    CHECK(Raised(12, 0, 12) == 0);     // reached
    CHECK(Raised(16, 0, 12) == 0);     // above: nothing taken off
    CHECK(Raised(16, 2, 12) == 2);     // a hit that stays above the raise is the game's
    CHECK(Raised(5, 3, -1) == 3);      // no raise asked
}

TEST_CASE("dbno body: Raising waits for the step, then is written once") {
    Raising r;
    CHECK(r.Peek() == -1);
    r.Start(12, 1'000'000);
    CHECK(r.Peek() == 12);
    CHECK_FALSE(r.Frame(1, 1'100'000));
    const auto late = r.Frame(1, 1'000'000 + kHealWaitUs);
    REQUIRE(late);
    CHECK(*late == 12);
    CHECK(r.Peek() == -1);
    CHECK_FALSE(r.Frame(1, 2'000'000));  // once

    r.Start(12, 3'000'000);
    CHECK_FALSE(r.Frame(12, 3'100'000));  // the step took it
    CHECK(r.Peek() == -1);
    CHECK_FALSE(r.Frame(1, 4'000'000));
}

TEST_CASE("dbno body: the invincible bit") {
    CHECK(WithInvincible(0x1008, true) == 0x1048);
    CHECK(WithInvincible(0x1048, false) == 0x1008);
    CHECK(WithInvincible(0, false) == 0);
}

TEST_CASE("dbno body: only our downed Link's own non-crawl actions are skipped") {
    Crawl c;
    c.Want(Want(false));
    CHECK_FALSE(c.Skip(kBsa, Req(addr::kVerbWalk), false));  // up: the game's
    c.Want(Want(true));
    CHECK(c.Skip(kBsa, Req(134), false));                      // a hit's reaction (row 34)
    CHECK(c.Skip(kBsa, Req(addr::kVerbWalk), false));
    CHECK_FALSE(c.Skip(kBsa, Req(addr::kVerbCrawl), false));   // the crawl itself
    CHECK_FALSE(c.Skip(kBsa, Req(134), true));                 // our own call
    CHECK_FALSE(c.Skip(0x5000, Req(134), false));              // another actor (an enemy)
    CHECK_FALSE(c.Skip(0, Req(134), false));
    c.Want(Want(true, true));
    CHECK_FALSE(c.Skip(kBsa, Req(134), false));                // our event drives our Link
    const auto t = c.Totals();
    CHECK(t.skipped == 2);
    CHECK(t.skipped_verbs.size() == 2);
}

TEST_CASE("dbno body: nothing is asked before our Link's own node is known, nor when up") {
    Guest g;
    Crawl c;
    c.Want(Want(true));
    CHECK(RunFrame(c, g, 1000).empty());  // no node learnt yet
    c.Want(Want(false));
    c.Skip(kBsa, Req(addr::kVerbWait, 2), false);  // learnt (up: not skipped)
    CHECK(RunFrame(c, g, 1100).empty());           // up and never crawled: nothing
    CHECK(c.Totals().asked == 0);
}

TEST_CASE("dbno body: down -> Crawl once with the scheduler's node; up -> Wait once") {
    Guest g;
    Crawl c;
    c.Want(Want(false));
    c.Skip(kBsa, Req(77, 5, 0x7777), false);              // not Walk/Wait: not learnt from
    c.Skip(kBsa, Req(addr::kVerbWalk, 2), false);         // the scheduler's main slot
    c.Want(Want(true));
    const auto first = RunFrame(c, g, 1000);
    REQUIRE(first.size() == 1);
    CHECK(first[0] == addr::kVerbCrawl);
    std::uint64_t node = 0;
    g.Read(kBlock + bn::addr::kReqNode, &node, sizeof node);
    CHECK(node == kNode);
    std::int32_t slot = -1;
    g.Read(kBlock + bn::addr::kReqSlot, &slot, sizeof slot);
    CHECK(slot == 2);
    CHECK(RunFrame(c, g, 1100).empty());  // taken: not asked again
    CHECK(RunFrame(c, g, 5000).empty());

    c.Want(Want(false));
    const auto up = RunFrame(c, g, 6000);
    REQUIRE(up.size() == 1);
    CHECK(up[0] == addr::kVerbWait);
    CHECK(RunFrame(c, g, 6100).empty());
    CHECK(c.Totals().asked == 2);
}

TEST_CASE("dbno body: a refused crawl is asked again, spaced, at most kMaxAsks times") {
    Guest g;
    Crawl c;
    c.Want(Want(false));
    c.Skip(kBsa, Req(addr::kVerbWait), false);
    c.Want(Want(true));
    int asks = 0;
    for (std::uint64_t ms = 1000; ms < 20000; ms += 100) {
        asks += static_cast<int>(RunFrame(c, g, ms, 2).size());  // 2: the game's refusal
    }
    CHECK(asks == Crawl::kMaxAsks);
    CHECK(c.Totals().refused == static_cast<std::uint64_t>(Crawl::kMaxAsks));
    // Up and down again: asked anew (refused crawl: no Wait needed).
    c.Want(Want(false));
    CHECK(RunFrame(c, g, 21000).empty());
    c.Want(Want(true));
    CHECK(RunFrame(c, g, 21100).size() == 1);
}

TEST_CASE("dbno body: an event of ours pauses the crawl; it is asked again after") {
    Guest g;
    Crawl c;
    c.Want(Want(false));
    c.Skip(kBsa, Req(addr::kVerbWait), false);
    c.Want(Want(true));
    CHECK(RunFrame(c, g, 1000).size() == 1);
    c.Want(Want(true, true));
    CHECK(RunFrame(c, g, 2000).empty());
    c.Want(Want(true));
    const auto again = RunFrame(c, g, 3000);
    REQUIRE(again.size() == 1);
    CHECK(again[0] == addr::kVerbCrawl);
}

TEST_CASE("dbno body: the shield sets the game's invincible bit, and clears only its own") {
    Guest g;
    Crawl c;
    g_flags = 0x1008;  // the game's other bits stay
    c.Want(Want(true));
    RunFrame(c, g, 1000);
    CHECK(g_flags == 0x1048);
    CHECK(c.Totals().shield_on == 1);
    g_flags = 0x1008;  // the game cleared it: set again
    RunFrame(c, g, 1100);
    CHECK(g_flags == 0x1048);
    c.Want(Want(false));
    RunFrame(c, g, 1200);
    CHECK(g_flags == 0x1008);

    // The game's own invincibility (its bit already set when we went down): left set.
    Crawl d;
    g_flags = 0x0040;
    d.Want(Want(true));
    RunFrame(d, g, 1000);
    CHECK(g_flags == 0x0040);
    d.Want(Want(false));
    RunFrame(d, g, 1100);
    CHECK(g_flags == 0x0040);

    // Never down: the word is not touched at all.
    Crawl e;
    g_cas_calls = 0;
    e.Want(Want(false));
    RunFrame(e, g, 1000);
    CHECK(g_cas_calls == 0);
}
