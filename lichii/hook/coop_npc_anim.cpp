// SPDX-License-Identifier: GPL-3.0-or-later

#include "coop_npc_anim.h"

#include <algorithm>
#include <cmath>

namespace CoopNpcAnim {
namespace {

constexpr std::uint64_t kRateMinUs = 20'000;   // nearer samples: too noisy
constexpr std::uint64_t kRateMaxUs = 500'000;  // further apart: too old
constexpr float kRateMax = 4.0f;
constexpr float kGameFps = 30.0f;

} // namespace

float SeenRate(float before_frame, std::uint64_t before_us, float last_frame,
               std::uint64_t last_us, float nominal) {
    if (!std::isfinite(before_frame) || !std::isfinite(last_frame) || last_us <= before_us ||
        last_us - before_us < kRateMinUs || last_us - before_us > kRateMaxUs ||
        last_frame < before_frame) {
        return nominal;
    }
    const float game_frames = static_cast<float>(last_us - before_us) * kGameFps / 1e6f;
    return std::min((last_frame - before_frame) / game_frames, kRateMax);
}

void Loop::See(std::string_view name, float frame) {
    if (!std::isfinite(frame) || frame < 0.0f) {
        return;
    }
    if (name != name_) {
        name_ = std::string(name);
        last_ = frame;
        top_ = frame;
        length_ = 0.0f;
        return;
    }
    if (frame < last_ - kLoopDrop) {
        length_ = std::max(length_, top_);
        top_ = frame;
    } else {
        top_ = std::max(top_, frame);
    }
    last_ = frame;
}

float Wrapped(float frame, float length) {
    if (!std::isfinite(frame) || frame < 0.0f) {
        return 0.0f;
    }
    if (!(length > 0.0f) || frame < length) {
        return frame;
    }
    return std::fmod(frame, length);
}

float FrameGap(float now, float want, float length) {
    const float gap = std::fabs(now - want);
    if (!(length > 0.0f) || gap >= length) {
        return gap;
    }
    return std::min(gap, length - gap);
}

bool Aliases::Shows(std::uint64_t actor, std::string_view wanted, std::uint64_t slot_name_at,
                    std::string_view slot_name) {
    const auto it = entries_.find(actor);
    if (slot_name == wanted) {
        // Its own AI plays the owner's animation too (or the alias is gone
        // and our play shows): nothing is hidden any more.
        if (it != entries_.end()) {
            entries_.erase(it);
        }
        return true;
    }
    return it != entries_.end() && it->second.played == wanted &&
           it->second.name_at == slot_name_at && it->second.name == slot_name;
}

void Aliases::Played(std::uint64_t actor, std::string_view wanted, std::uint64_t alias_at,
                     std::string_view alias) {
    entries_[actor] = Entry{std::string(wanted), alias_at, std::string(alias)};
}

std::optional<Aliases::Entry> Aliases::Drop(std::uint64_t actor) {
    const auto it = entries_.find(actor);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    Entry entry = std::move(it->second);
    entries_.erase(it);
    return entry;
}

std::vector<std::pair<std::uint64_t, Aliases::Entry>> Aliases::DropAllBut(
    const std::unordered_set<std::uint64_t>& wished) {
    std::vector<std::pair<std::uint64_t, Entry>> out;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (wished.contains(it->first)) {
            ++it;
            continue;
        }
        out.emplace_back(it->first, std::move(it->second));
        it = entries_.erase(it);
    }
    return out;
}

// jitter-b: Resync (coop_npc_anim.h).
Resync::Verdict Resync::Check(std::uint64_t key, std::string_view name, float gap, std::uint64_t now_ms) {
    Entry& e = entries_[key];
    if (e.name != name) {
        e = Entry{};
        e.name = std::string(name);
    }
    if (!(gap > kFrameSlack)) {
        e.misses = 0;  // in step: the game's own playback runs
        return Verdict::in_step;
    }
    if (e.given_up) {
        return Verdict::given_up;
    }
    if (e.set_ms != 0) {
        if (now_ms - e.set_ms < kResyncMs) {
            return Verdict::wait;
        }
        // Still about as far off as before the last set: that set did not take.
        if (gap >= e.gap_at_set * 0.5f && ++e.misses >= kMaxMisses) {
            e.given_up = true;
            return Verdict::give_up;
        }
    }
    e.set_ms = now_ms == 0 ? 1 : now_ms;
    e.gap_at_set = gap;
    return Verdict::set;
}

void Resync::Keep(const std::unordered_set<std::uint64_t>& keys) {
    std::erase_if(entries_, [&keys](const auto& e) { return !keys.contains(e.first); });
}

} // namespace CoopNpcAnim
