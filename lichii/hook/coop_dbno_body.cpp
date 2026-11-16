// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_dbno_body.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace CoopDbno::Body {

// --- Raising ---

void Raising::Start(std::int32_t to, std::uint64_t now_us) noexcept {
    to_.store(to, std::memory_order_release);
    since_us_ = now_us == 0 ? 1 : now_us;
}

std::optional<std::int32_t> Raising::Frame(std::optional<std::int32_t> life, std::uint64_t now_us) noexcept {
    const std::int32_t to = to_.load(std::memory_order_acquire);
    if (to < 0 || since_us_ == 0) {
        return std::nullopt;
    }
    if (life && *life >= to) {
        Drop();  // the step took it (or something raised it further)
        return std::nullopt;
    }
    if (now_us < since_us_ + kHealWaitUs) {
        return std::nullopt;
    }
    Drop();
    return to;
}

void Raising::Drop() noexcept {
    to_.store(-1, std::memory_order_release);
    since_us_ = 0;
}

// --- Crawl ---

void Crawl::Want(const Wanted& w) {
    std::lock_guard lock(mutex_);
    want_ = w;
    bsa_.store(w.bsa, std::memory_order_release);
}

bool Crawl::Skip(std::uint64_t bsa, const CoopBossNative::Request& request, bool own) {
    if (bsa == 0 || own || bsa != bsa_.load(std::memory_order_acquire)) {
        return false;  // (every other actor's request: no lock)
    }
    std::lock_guard lock(mutex_);
    if (bsa != want_.bsa) {
        return false;
    }
    const auto a = CoopBossNative::ActionFromRequest(request);
    if (!a) {
        return false;
    }
    std::uint64_t node = 0;
    std::memcpy(&node, request.data() + CoopBossNative::addr::kReqNode, sizeof node);
    if ((a->verb == addr::kVerbWalk || a->verb == addr::kVerbWait) && node != 0) {
        node_ = node;  // the scheduler's own node, on its main slot
        slot_ = a->slot;
        flags_ = a->flags;
        learnt_ = true;
    }
    if (!want_.lying || want_.event || a->verb == addr::kVerbCrawl) {
        return false;
    }
    ++counts_.skipped;
    if (counts_.skipped_verbs.size() < 16 &&
        std::find(counts_.skipped_verbs.begin(), counts_.skipped_verbs.end(), a->verb) ==
            counts_.skipped_verbs.end()) {
        counts_.skipped_verbs.push_back(a->verb);
        Note("our downed Link: the game's action verb " + std::to_string(a->verb) + " (slot " +
             std::to_string(a->slot) + ") skipped");
    }
    return true;
}

std::optional<CoopPuppet::Call> Crawl::Ask(CoopPuppet::Guest& guest, std::int32_t verb, std::uint64_t main_base) {
    // (the lock is held)
    if (block_ == 0) {
        CoopPuppet::Call c;
        c.function = main_base + CoopBossNative::addr::kMalloc;
        c.x[0] = CoopBossNative::addr::kRequestSize;
        pending_ = Pending::malloc;
        return c;
    }
    coop::protocol::BossAction action;
    action.slot = slot_;
    action.verb = verb;
    action.flags = flags_;
    action.changed = true;
    const CoopBossNative::Request r = CoopBossNative::RequestFromAction(action, node_);
    if (!guest.Write(block_, r.data(), r.size())) {
        return std::nullopt;
    }
    CoopPuppet::Call c;
    c.function = main_base + CoopBossNative::addr::kRequestActionVerb;
    c.x[0] = want_.bsa;
    c.x[1] = block_;
    c.x[2] = 1;
    pending_ = verb == addr::kVerbCrawl ? Pending::crawl : Pending::stand;
    ++counts_.asked;
    return c;
}

std::optional<CoopPuppet::Call> Crawl::Next(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                            std::uint64_t now_ms, std::uint64_t main_base, Cas cas) {
    std::lock_guard lock(mutex_);
    if (after_call) {
        const Pending was = std::exchange(pending_, Pending::none);
        if (was == Pending::malloc) {
            block_ = result;
            if (block_ == 0) {
                return std::nullopt;
            }
            // (on to the ask below, in the same frame)
        } else {
            const bool taken = result == CoopBossNative::addr::kActionTaken;
            if (!taken) {
                ++counts_.refused;
            }
            if (was == Pending::crawl && taken) {
                crawling_ = true;
                Note("our downed Link: Crawl (234) asked and taken");
            } else if (was == Pending::crawl) {
                Note("our downed Link: Crawl (234) refused (" + std::to_string(result) + ")");
            } else if (was == Pending::stand) {
                Note(taken ? "our Link up: Wait (40) asked and taken" : "our Link up: Wait (40) refused");
            }
            return std::nullopt;
        }
    }

    // The shield, each frame (the game may clear the bit itself: set again).
    if (cas != nullptr && want_.life != 0 && !after_call) {
        const std::uint64_t at = want_.life + addr::kLifeFlags;
        const bool on = want_.lying;
        if (on || shield_ours_) {
            std::uint32_t seen = 0;
            if (cas(at, 0, 0, seen)) {  // (reads the word: 0 is swapped for 0 only)
                for (int i = 0; i < 4; ++i) {
                    const std::uint32_t desired = WithInvincible(seen, on);
                    if (desired == seen) {
                        break;
                    }
                    std::uint32_t now = 0;
                    if (!cas(at, seen, desired, now)) {
                        break;  // (not mapped any more)
                    }
                    if (now == seen) {  // swapped
                        if (on) {
                            ++counts_.shield_on;
                            if (!shield_ours_) {
                                Note("our downed Link: the game's invincible flag set");
                            }
                        } else {
                            Note("our Link up: the game's invincible flag cleared");
                        }
                        shield_ours_ = on;
                        break;
                    }
                    seen = now;  // the game changed another bit meanwhile: again
                }
            }
        }
    }

    // The crawl.
    if (want_.lying != was_lying_) {
        was_lying_ = want_.lying;
        asks_ = 0;
        next_ask_ms_ = 0;
        stand_due_ = !want_.lying && crawling_;
        crawling_ = false;
    }
    if (want_.event) {
        crawling_ = false;  // the event moves our Link; asked again after it
        asks_ = 0;
        next_ask_ms_ = 0;
        return std::nullopt;
    }
    if (want_.bsa == 0 || !learnt_) {
        return std::nullopt;
    }
    if (stand_due_) {
        if (block_ != 0) {
            stand_due_ = false;  // (the first time the block is made first, then this again)
        }
        return Ask(guest, addr::kVerbWait, main_base);
    }
    if (want_.lying && !crawling_ && asks_ < kMaxAsks && now_ms >= next_ask_ms_) {
        if (pending_ == Pending::none && block_ != 0) {
            ++asks_;
            next_ask_ms_ = now_ms + kAskEveryMs;
        }
        return Ask(guest, addr::kVerbCrawl, main_base);
    }
    return std::nullopt;
}

Crawl::Counts Crawl::Totals() const {
    std::lock_guard lock(mutex_);
    return counts_;
}

std::vector<std::string> Crawl::TakeNotes() {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out;
    out.swap(notes_);
    return out;
}

void Crawl::Note(std::string line) {
    if (notes_.size() < 32) {
        notes_.push_back(std::move(line));
    }
}

Crawl& TheCrawl() {
    static Crawl crawl;
    return crawl;
}

Raising& TheRaising() {
    static Raising raising;
    return raising;
}

}  // namespace CoopDbno::Body
