// SPDX-License-Identifier: GPL-3.0-or-later
//
// Down but not out: our downed Link's body, through the game's own systems
// (docs/play-online.md "Down but not out"; coop_dbno.h has the rules).
//
// - Crawl. The player's scheduler AI (Player.scheduler.*.module.ainb) enters a
//   crawlspace by asking its BSA for action verb 234 ("Crawl", the value the
//   walk, wait and lock-on modules' ExecuteBSASetActionVerb nodes give it);
//   that runs Player.action.crawl (ExecutePlayerCrawl) with its own animation
//   and the PlayerCamera's "Crawl" camera. A downed Link is asked for that
//   verb through the game's own "request action verb" (main+0xc08f0, the
//   wrap of hos_coop_bosses.h), with the scheduler's own asking node, and
//   every other action the game asks for it meanwhile is skipped at that wrap
//   (so a hit's reaction, row 34, no longer stands it up). Up again (revived,
//   respawned, the rules off): the game's "Wait" (40) is asked once, and the
//   game's own requests go through again.
// - Shield. The game's own invincible flag: bit 0x40 of the Life component's
//   flags word (Life+0x1d18), which the AI node OneShotLifeSetInvincible sets
//   and clears atomically (its run, main+0x28e7da0). Set (compare-and-swap)
//   while downed; cleared again only if we set it.
// - Heal. The revive's life goes in through the game's own Life step (patch
//   0007) as a negative sum, the game's heal (as LifeWrite::heal for bosses);
//   written directly only when the step has not taken it in kHealWaitUs.
//
// The crawl and the shield need Hookshot (the wrap, a calc-thread caller and
// compare-and-swap); elsewhere nothing is asked and nothing changes. Solo
// play never gets here: downed exists only in a session.
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "coop_boss_native.h"
#include "coop_puppet.h"

namespace CoopDbno::Body {

namespace addr {
inline constexpr std::uint64_t kComponentBsa = CoopBossNative::addr::kComponentBsa;  // [actor+0x270]+0x188
inline constexpr std::int32_t kVerbCrawl = 234;  // Player ActionVerb "Crawl"
inline constexpr std::int32_t kVerbWait = 40;    // "Wait"
inline constexpr std::int32_t kVerbWalk = 1;     // "Walk"
inline constexpr std::uint64_t kLifeFlags = 0x1d18;       // Life component: u32 flags
inline constexpr std::uint32_t kLifeInvincible = 0x40;    // OneShotLifeSetInvincible's bit
}  // namespace addr

// The game's flags word with the invincible bit on or off.
constexpr std::uint32_t WithInvincible(std::uint32_t flags, bool on) {
    return on ? (flags | addr::kLifeInvincible) : (flags & ~addr::kLifeInvincible);
}

// A raise of our Link's life (a revive, a respawn) for the Life step: the
// step takes it as a negative sum; the frame writes it directly when the step
// has not within kHealWaitUs (it skips frames with no life value).
inline constexpr std::uint64_t kHealWaitUs = 250'000;

constexpr std::int32_t Raised(std::int32_t life, std::int32_t sum, std::int32_t to) {
    return to >= 0 && life - sum < to ? life - to : sum;
}

class Raising {
public:
    // The frame: raise to `to` from now (a new ask replaces an old one).
    void Start(std::int32_t to, std::uint64_t now_us) noexcept;
    // The frame, each frame: the life to write directly now (the step did not
    // take it in time), once; nothing while waiting or when done.
    std::optional<std::int32_t> Frame(std::optional<std::int32_t> life, std::uint64_t now_us) noexcept;
    void Drop() noexcept;
    // The Life step (any game thread): the life to raise to, or -1.
    std::int32_t Peek() const noexcept { return to_.load(std::memory_order_acquire); }

private:
    std::atomic<std::int32_t> to_{-1};
    std::uint64_t since_us_ = 0;
};

// Our Link's crawl and shield. Want() from the frame; Skip() from the action
// verb wrap (a game thread); Next() from the calc-thread caller.
class Crawl {
public:
    struct Wanted {
        bool lying = false;          // downed (or spectating): crawl, shielded
        bool event = false;          // our Link in an event: the event drives it
        std::uint64_t bsa = 0;       // our Link's BSA component (0: none)
        std::uint64_t life = 0;      // our Link's Life component (0: none)
    };
    void Want(const Wanted& w);

    // A request the game makes on any BSA; true: skip it (our downed Link,
    // any verb but Crawl, not our own call). Learns our Link's own asking node.
    bool Skip(std::uint64_t bsa, const CoopBossNative::Request& request, bool own);
    // Whether `bsa` is our Link's (no lock; the wrap reads a request only then).
    bool IsOurs(std::uint64_t bsa) const noexcept {
        return bsa != 0 && bsa == bsa_.load(std::memory_order_acquire);
    }

    using Cas = bool (*)(std::uint64_t address, std::uint32_t expected, std::uint32_t desired, std::uint32_t& seen);
    // The next game call (CoopCallers::Caller). `cas` may be null (no shield).
    std::optional<CoopPuppet::Call> Next(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                         std::uint64_t now_ms, std::uint64_t main_base, Cas cas);

    struct Counts {
        std::uint64_t skipped = 0;     // the game's own requests skipped while lying
        std::uint64_t asked = 0;       // our Crawl / Wait requests made
        std::uint64_t refused = 0;     // ... that the game refused (result not 0)
        std::uint64_t shield_on = 0;   // times we set the invincible bit
        std::vector<std::int32_t> skipped_verbs;  // the first distinct ones (the log)
    };
    Counts Totals() const;
    // Log lines since the last call.
    std::vector<std::string> TakeNotes();

    // Ask again at most this often while lying and not known to crawl.
    static constexpr std::uint64_t kAskEveryMs = 500;
    static constexpr int kMaxAsks = 6;  // then left alone until up again

private:
    enum class Pending : std::uint8_t { none, malloc, crawl, stand };
    std::optional<CoopPuppet::Call> Ask(CoopPuppet::Guest& guest, std::int32_t verb, std::uint64_t main_base);
    void Note(std::string line);

    mutable std::mutex mutex_;
    Wanted want_;
    std::atomic<std::uint64_t> bsa_{0};  // want_.bsa, read without the lock by Skip
    // Learnt from our Link's own scheduler requests (Walk / Wait: its main slot).
    std::uint64_t node_ = 0;
    std::int32_t slot_ = 0;
    std::uint16_t flags_ = 0;
    bool learnt_ = false;
    // The caller's (calc thread).
    bool was_lying_ = false;
    bool crawling_ = false;      // our Crawl was taken (until up, or an event)
    bool stand_due_ = false;     // up again: ask Wait once
    int asks_ = 0;
    std::uint64_t next_ask_ms_ = 0;
    bool shield_ours_ = false;   // we set the invincible bit
    std::uint64_t block_ = 0;    // our request block in guest memory
    Pending pending_ = Pending::none;
    Counts counts_;
    std::vector<std::string> notes_;
};

Crawl& TheCrawl();
Raising& TheRaising();

}  // namespace CoopDbno::Body
