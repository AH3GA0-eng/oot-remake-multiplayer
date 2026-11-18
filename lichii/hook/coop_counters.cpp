// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_counters.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace CoopCounters {
namespace {

constexpr std::uint64_t kSlotLimit = 0x100000;  // node - graph larger than this: not a slot

std::uint64_t Mix(std::uint64_t h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (8 * i)) & 0xff;
        h *= 0x100000001b3ull;
    }
    return h;
}

bool Close(std::uint64_t a, std::uint64_t b, float slack) {
    const double hi = static_cast<double>(std::max(a, b));
    const double lo = static_cast<double>(std::min(a, b));
    return hi - lo <= slack * hi;
}

}  // namespace

std::vector<std::uint64_t> RingNodes(const std::array<std::uint64_t, kRingNodes>& entries) {
    std::vector<std::uint64_t> out;
    for (const std::uint64_t e : entries) {
        if (e != 0 && std::find(out.begin(), out.end(), e) == out.end()) {
            out.push_back(e);
        }
    }
    return out;
}

std::uint64_t KeyOf(std::uint64_t placement_hash, float duration_frames, std::uint64_t node,
                    std::uint64_t graph) {
    const auto tenths = static_cast<std::int64_t>(std::lround(static_cast<double>(duration_frames) * 10.0));
    const std::uint64_t slot =
        graph != 0 && node > graph && node - graph < kSlotLimit ? node - graph : 0;
    std::uint64_t h = 0xcbf29ce484222325ull;
    h = Mix(h, placement_hash);
    h = Mix(h, static_cast<std::uint64_t>(tenths));
    h = Mix(h, slot);
    return h == 0 ? 1 : h;
}

bool IsPlatformOwner(std::string_view name) {
    return name.starts_with("Obj_");  // not enemies, NPCs, items or Link (Hot Onion)
}

void OtherNodes::Add(std::uint64_t node, std::uint64_t actor) {
    if (nodes_.size() >= kMax) {
        nodes_.clear();
    }
    nodes_[node] = actor;
}

bool OtherNodes::Skip(std::uint64_t node, std::uint64_t actor_now) {
    const auto it = nodes_.find(node);
    if (it == nodes_.end()) {
        return false;
    }
    if (it->second != actor_now) {
        nodes_.erase(it);  // reused: someone else's node now
        return false;
    }
    return true;
}

void OtherNodes::Keep(const std::function<bool(std::uint64_t actor)>& alive) {
    std::erase_if(nodes_, [&](const auto& p) { return !alive(p.second); });
}

std::uint64_t LoopOf(const std::deque<std::uint64_t>& periods, float slack) {
    const std::size_t n = periods.size();
    if (n >= 2 && Close(periods[n - 1], periods[n - 2], slack)) {
        return periods[n - 1];
    }
    if (n >= 4 && Close(periods[n - 1], periods[n - 3], slack) &&
        Close(periods[n - 2], periods[n - 4], slack)) {
        return periods[n - 1] + periods[n - 2];
    }
    return 0;
}

float PhaseGap(std::uint64_t ours_us, std::uint64_t theirs_us, std::uint64_t period_us) {
    if (period_us == 0) {
        return 0.0f;
    }
    const auto p = static_cast<std::int64_t>(period_us);
    std::int64_t d = static_cast<std::int64_t>(ours_us) - static_cast<std::int64_t>(theirs_us);
    d %= p;
    if (d > p / 2) {
        d -= p;
    } else if (d <= -p / 2) {
        d += p;
    }
    return static_cast<float>(static_cast<double>(d) / 1e6);
}

void CounterSync::Receive(const coop::protocol::PlatformCounter& message) {
    if (message.scene != scene_ || message.player_number == 0) {
        return;
    }
    for (const auto& t : message.ticks) {
        Theirs& th = theirs_[t.key][message.player_number];
        if (th.got_us != 0 && message.server_time_us <= th.got_us) {
            continue;  // older than one we have
        }
        th.got_us = message.server_time_us;
        th.start_us = message.server_time_us > t.since_us ? message.server_time_us - t.since_us : 0;
        th.period_us = t.period_us;
        th.starts = t.starts;
    }
}

void CounterSync::Tick(const std::string& scene, std::uint8_t self, std::uint64_t server_us,
                       const std::vector<Seen>& seen, bool probe_only, Out& out) {
    if (scene != scene_) {
        Reset();
        scene_ = scene;
    }
    for (auto it = theirs_.begin(); it != theirs_.end();) {
        std::erase_if(it->second, [&](const auto& p) { return server_us > p.second.got_us + kStaleUs; });
        it = it->second.empty() ? theirs_.erase(it) : std::next(it);
    }
    std::set<std::uint64_t> here;
    std::map<std::uint64_t, int> per_key;  // our nodes of each key: two is no key
    for (const Seen& s : seen) {
        if (IsPlatformOwner(s.owner_name)) {
            ++per_key[s.key];
        }
    }
    for (const Seen& s : seen) {
        if (!IsPlatformOwner(s.owner_name) || s.node == 0 || !std::isfinite(s.remaining) ||
            !std::isfinite(s.duration) || s.duration <= 0.0f || s.remaining > s.duration + 1.0f) {
            continue;
        }
        here.insert(s.node);
        if (s.owner_hash != 0) {
            counter_owners_.insert(s.owner_hash);  // the clock-word path leaves it alone
        }
        Node& n = nodes_[s.node];
        if (n.key != s.key) {
            n = Node{};  // a new node at that address
            n.key = s.key;
        }
        n.owner_hash = s.owner_hash;
        n.owner_name = s.owner_name;
        const float elapsed_frames = std::max(0.0f, s.duration - std::max(s.remaining, 0.0f));
        const auto began = [&] {
            const auto back = static_cast<std::uint64_t>(elapsed_frames / kFramesPerSecond * 1e6f);
            return server_us > back ? server_us - back : 0;
        };
        if (!n.looked) {
            n.looked = true;
            if (s.remaining > 0.0f) {
                n.start_us = began();  // seen in the middle of a pause
                n.starts = 1;
            }
        } else if (s.remaining > n.last_remaining + 0.5f) {
            const std::uint64_t start = began();
            n.started_now = true;
            if (n.start_us != 0 && start > n.start_us) {
                const std::uint64_t period = start - n.start_us;
                if (period >= kMinPeriodUs && period <= kMaxPeriodUs) {
                    n.periods.push_back(period);
                    while (n.periods.size() > 4) {
                        n.periods.pop_front();
                    }
                } else {
                    n.periods.clear();
                }
            }
            n.start_us = start;
            ++n.starts;
            const bool was = n.looping;
            const std::uint64_t loop = LoopOf(n.periods, kPeriodSlack);
            if (loop != 0) {
                n.cycle_us = loop;
                n.looping = true;
                n.misses = 0;
                const std::size_t np = n.periods.size();
                const bool two = loop != n.periods.back();
                if (!two || n.periods[np - 1] >= n.periods[np - 2]) {
                    n.ref_us = start;  // ends the longer period (or the only one)
                }
            } else if (n.looping && ++n.misses >= kLoopMisses) {
                n.looping = false;  // it stopped looping (a switch, a broken lift)
                n.cycle_us = 0;
                n.ref_us = 0;
            }
            if (n.looping && !was && !n.logged) {
                n.logged = true;
                char text[200];
                std::snprintf(text, sizeof text, "%016llx %s: pause counter %016llx loops every %.2f s%s",
                              static_cast<unsigned long long>(n.owner_hash), n.owner_name.c_str(),
                              static_cast<unsigned long long>(n.key),
                              static_cast<double>(n.cycle_us) / 1e6,
                              n.cycle_us != n.periods.back() ? " (two waits)" : "");
                out.notes.emplace_back(text);
            }
        }
        n.last_remaining = s.remaining;
        if (per_key[n.key] != 1 || s.remaining <= 0.0f || n.written_start_us == n.start_us) {
            continue;
        }
        const auto tk = theirs_.find(n.key);
        if (tk == theirs_.end()) {
            continue;
        }
        if (!n.looping || n.ref_us == 0) {
            AlignPause(n, s, self, tk->second, probe_only, out);  // no loop of ours: this pause alone
            continue;
        }
        // The one to follow: the game that has seen the most starts (then
        // the lowest number), with the same period.
        const std::uint64_t period = n.cycle_us;
        const Theirs* best = nullptr;
        std::uint8_t best_player = 0;
        for (const auto& [player, th] : tk->second) {
            if (player == self || th.start_us == 0 || !Close(th.period_us, period, kPeriodSlack)) {
                continue;
            }
            if (best == nullptr || th.starts > best->starts) {
                best = &th;
                best_player = player;
            }
        }
        if (best == nullptr) {
            AlignPause(n, s, self, tk->second, probe_only, out);  // no game with our period: this pause alone
            continue;
        }
        const float gap = PhaseGap(n.ref_us, best->start_us, period);
        if (std::fabs(gap) <= kOffS) {
            continue;  // in phase: nothing to write
        }
        const bool we_move = n.starts < best->starts || (n.starts == best->starts && self > best_player);
        if (!we_move) {
            continue;
        }
        // Began later (gap > 0): end the pause sooner, if it has that long
        // left; else, and when it began earlier, make it longer (by the
        // rest of the period when it began later). One write either way.
        float target = s.remaining - gap * kFramesPerSecond;
        if (target < 0.0f) {
            target = s.remaining + (static_cast<float>(period) / 1e6f - gap) * kFramesPerSecond;
        }
        if (probe_only) {
            n.written_start_us = n.start_us;  // said once a pause; nothing changes
        } else {
            out.writes.push_back({s.node, n.key, target, s.remaining});
            ++writes_;
            // The pause now ends as if it began this much earlier (negative: later).
            const auto shift = static_cast<std::int64_t>(static_cast<double>(s.remaining - target) /
                                                         kFramesPerSecond * 1e6);
            n.start_us = static_cast<std::uint64_t>(static_cast<std::int64_t>(n.start_us) - shift);
            n.ref_us = static_cast<std::uint64_t>(static_cast<std::int64_t>(n.ref_us) - shift);
            n.written_start_us = n.start_us;
            n.last_remaining = target;
        }
        char text[220];
        std::snprintf(text, sizeof text,
                      "%016llx %s: pause counter %016llx %.2f s off player %u's: frames left %.1f -> %.1f%s",
                      static_cast<unsigned long long>(n.owner_hash), n.owner_name.c_str(),
                      static_cast<unsigned long long>(n.key), static_cast<double>(gap),
                      static_cast<unsigned>(best_player), static_cast<double>(s.remaining),
                      static_cast<double>(target), probe_only ? " (probe: not written)" : "");
        out.notes.emplace_back(text);
    }
    std::erase_if(nodes_, [&](const auto& p) { return !here.contains(p.first); });
    bool fresh = false;  // a shared node started this tick: its start goes at once, not in up to 3 s
    for (auto& [address, n] : nodes_) {
        fresh = fresh || (n.started_now && Shared(n) && per_key[n.key] == 1);
        n.started_now = false;
    }
    if (!fresh && last_send_us_ != 0 && server_us < last_send_us_ + kSendEveryUs) {
        return;
    }
    coop::protocol::PlatformCounter m;
    m.scene = scene;
    m.server_time_us = server_us;
    std::set<std::uint64_t> sent_keys;
    for (const auto& [address, n] : nodes_) {
        // A looping node: its loop and the start every game compares. One that started kPauseStarts
        // times but keeps no steady period (a physics lift): its last period and its last start.
        const std::uint64_t ref = n.looping ? n.ref_us : n.start_us;
        const std::uint64_t cycle = n.looping ? n.cycle_us : (n.periods.empty() ? 0 : n.periods.back());
        if (!Shared(n) || per_key[n.key] != 1 || ref == 0 || ref > server_us ||
            !sent_keys.insert(n.key).second || m.ticks.size() >= coop::protocol::kMaxPlatformCounters) {
            continue;
        }
        const std::uint64_t since = server_us - ref;  // since the start every game compares
        if (since > 0xffffffffull || cycle > 0xffffffffull || cycle == 0) {
            continue;
        }
        m.ticks.push_back({n.key, static_cast<std::uint32_t>(cycle), static_cast<std::uint32_t>(since), n.starts});
    }
    last_send_us_ = server_us;
    if (!m.ticks.empty()) {
        out.send.emplace_back(std::move(m));
    }
}

bool CounterSync::Shared(const Node& n) {
    return n.looping || n.starts >= kPauseStarts;
}

bool CounterSync::AlignPause(Node& n, const Seen& s, std::uint8_t self,
                             const std::map<std::uint8_t, Theirs>& theirs, bool probe_only, Out& out) {
    if (n.starts < kAlignStarts || n.start_us == 0) {
        return false;
    }
    // The leader: the game that has seen the most starts (a tie: the lower number), whose last start is
    // this same pause (within one pause length of ours: never the lift's other wait, a move away).
    const auto pause_us = static_cast<std::int64_t>(static_cast<double>(s.duration) / kFramesPerSecond * 1e6);
    const Theirs* best = nullptr;
    std::uint8_t best_player = 0;
    for (const auto& [player, th] : theirs) {
        if (player == self || th.start_us == 0 || th.starts < kAlignStarts) {
            continue;
        }
        const std::int64_t d = static_cast<std::int64_t>(n.start_us) - static_cast<std::int64_t>(th.start_us);
        if (d >= pause_us || d <= -pause_us) {
            continue;
        }
        if (best == nullptr || th.starts > best->starts || (th.starts == best->starts && player < best_player)) {
            best = &th;
            best_player = player;
        }
    }
    if (best == nullptr) {
        return false;
    }
    const bool we_move = n.starts < best->starts || (n.starts == best->starts && self > best_player);
    const float gap = static_cast<float>(
        static_cast<double>(static_cast<std::int64_t>(n.start_us) - static_cast<std::int64_t>(best->start_us)) / 1e6);
    if (!we_move || std::fabs(gap) <= kOffS) {
        return false;
    }
    // Began later (gap > 0): the pause ends sooner, when the leader's does; began earlier: it waits for
    // the leader's end. Never below zero.
    const float target = std::max(0.0f, s.remaining - gap * kFramesPerSecond);
    if (probe_only) {
        n.written_start_us = n.start_us;
    } else {
        out.writes.push_back({s.node, n.key, target, s.remaining});
        ++writes_;
        const auto shift =
            static_cast<std::int64_t>(static_cast<double>(s.remaining - target) / kFramesPerSecond * 1e6);
        n.start_us = static_cast<std::uint64_t>(static_cast<std::int64_t>(n.start_us) - shift);
        n.written_start_us = n.start_us;
        n.last_remaining = target;
    }
    char text[230];
    std::snprintf(text, sizeof text,
                  "%016llx %s: pause counter %016llx %.2f s off player %u's (this pause): frames left %.1f -> %.1f%s",
                  static_cast<unsigned long long>(n.owner_hash), n.owner_name.c_str(),
                  static_cast<unsigned long long>(n.key), static_cast<double>(gap),
                  static_cast<unsigned>(best_player), static_cast<double>(s.remaining),
                  static_cast<double>(target), probe_only ? " (probe: not written)" : "");
    out.notes.emplace_back(text);
    return true;
}

void CounterSync::Reset() {
    scene_.clear();
    nodes_.clear();
    theirs_.clear();
    last_send_us_ = 0;
    counter_owners_.clear();
}

std::set<std::uint64_t> CounterSync::LoopingOwners() const {
    std::set<std::uint64_t> out;
    for (const auto& [address, n] : nodes_) {
        if (n.looping && n.owner_hash != 0) {
            out.insert(n.owner_hash);
        }
    }
    return out;
}

std::uint64_t CounterSync::CycleOf(std::uint64_t key) const {
    for (const auto& [address, n] : nodes_) {
        if (n.key == key && n.looping) {
            return n.cycle_us;
        }
    }
    return 0;
}

bool CounterSync::Looping(std::uint64_t key) const {
    return std::any_of(nodes_.begin(), nodes_.end(),
                       [key](const auto& p) { return p.second.key == key && p.second.looping; });
}

}  // namespace CoopCounters
