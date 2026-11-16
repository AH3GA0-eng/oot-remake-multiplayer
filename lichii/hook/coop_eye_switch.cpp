// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_eye_switch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "coop_fairy.h"
#include "coop_live_events.h"
#include "coop_logic_set.h"

namespace CoopEyeSwitch {

namespace {
// reports/deku-ladder-a.md: the same pattern (eye + IsFirstOnEventTrigger + the room asker) in these rooms.
constexpr Switch kSwitches[] = {
    {"InsideTheDekuTree_Gimmick_FallingLadder", "InsideTheDekuTree", "Obj_EyeSwitch_A_02", false},  // slingshot room C
    {"InsideTheDekuTree_Gimmick_RoomF_EyeSwitch", "InsideTheDekuTree", "Obj_EyeSwitch_A_02", false},
    {"DodongoCavern_Gimmick_EyeSwitch", "DodongoCavern", "Obj_EyeSwitch_A_05", true},  // EyeSwitch_01 and _02
};

std::optional<std::array<float, 3>> PlaceOf(std::string_view event, std::string_view scene) {
    for (const auto& p : CoopLiveEvents::Places()) {
        if (p.event == event && p.scene == scene) return std::array<float, 3>{p.x, p.y, p.z};
    }
    return std::nullopt;
}
}  // namespace

std::span<const Switch> Switches() { return kSwitches; }

const Switch* Find(std::string_view event, std::string_view scene) {
    for (const auto& s : kSwitches) {
        if (s.event == event && s.scene == scene) return &s;
    }
    return nullptr;
}

std::vector<std::uint64_t> FindActors(CoopPuppet::Guest& guest, std::uint64_t main,
                                      std::span<const std::string_view> names, const std::array<float, 3>& at,
                                      float max_m) {
    std::vector<std::pair<float, std::uint64_t>> near;
    CoopFairy::WalkActors(guest, main, [&](std::uint64_t actor) {
        std::array<float, 3> p{};
        if (CoopFairy::ActorPresent(guest, actor) &&
            std::any_of(names.begin(), names.end(),
                        [&](std::string_view n) { return CoopFairy::ActorNamed(guest, actor, n); }) &&
            guest.Read(actor + CoopPuppet::addr::kActorPosition, p.data(), sizeof p)) {
            const float dx = p[0] - at[0], dy = p[1] - at[1], dz = p[2] - at[2];
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 <= max_m * max_m) near.emplace_back(d2, actor);
        }
        return false;  // the whole list
    });
    std::sort(near.begin(), near.end());
    std::vector<std::uint64_t> out;
    for (const auto& n : near) out.push_back(n.second);
    return out;
}

bool Switcher::Want(const std::string& event, const std::string& scene) {
    if (Find(event, scene) == nullptr) return false;
    std::lock_guard lock(mutex_);
    if (failed_) return false;
    if (std::none_of(wanted_.begin(), wanted_.end(), [&](const Try& t) { return t.event == event; })) {
        Try t;
        t.event = event;
        t.scene = scene;
        wanted_.push_back(std::move(t));
    }
    return true;
}

void Switcher::Expire(const std::string& scene) {
    std::lock_guard lock(mutex_);
    for (auto it = wanted_.begin(); it != wanted_.end();) {
        if (!scene.empty() && it->scene != scene && !(it == wanted_.begin() && next_ != Next::none)) {
            notes_.push_back("eye switch: " + it->event + " dropped (we left its scene)");
            it = wanted_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> Switcher::TakeNotes() {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out;
    out.swap(notes_);
    return out;
}

std::vector<std::pair<std::string, std::string>> Switcher::TakeFallbacks() {
    std::lock_guard lock(mutex_);
    std::vector<std::pair<std::string, std::string>> out;
    out.swap(fallbacks_);
    return out;
}

bool Switcher::EventRunning(CoopPuppet::Guest& guest, std::uint64_t main) {
    namespace le = CoopLiveEvents::addr;
    std::uint64_t manager = 0;
    std::uint64_t session = 0;
    if (!guest.Read(main + le::kEventStatePtr, &manager, 8) || manager == 0) return false;
    return guest.Read(manager + le::kEventSession, &session, 8) && session != 0;
}

void Switcher::End(const std::string& why, bool fallback) {
    const Try& t = wanted_.front();
    notes_.push_back("eye switch: " + t.event + ": " + why);
    if (fallback) fallbacks_.emplace_back(t.event, t.scene);
    wanted_.pop_front();
}

std::optional<CoopPuppet::Call> Switcher::Step(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                               std::uint64_t main, std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    if (after_call) {
        const Next was = next_;
        next_ = Next::none;
        if (was != Next::malloc) return std::nullopt;  // the execute: one set a frame
        if (result == 0) {
            failed_ = true;
            while (!wanted_.empty()) End("the game's malloc gave nothing: asked the old way", true);
            return std::nullopt;
        }
        block_ = result;
    }
    if (failed_ || main == 0 || wanted_.empty()) return std::nullopt;
    if (block_ == 0) {
        if (after_call) return std::nullopt;
        next_ = Next::malloc;
        return CoopLogicSet::Malloc(main);
    }
    return Frame(guest, main, now_ms);
}

std::optional<CoopPuppet::Call> Switcher::Frame(CoopPuppet::Guest& guest, std::uint64_t main, std::uint64_t now_ms) {
    Try& t = wanted_.front();
    if (t.until_ms == 0) t.until_ms = now_ms + kTryMs;
    const Switch* s = Find(t.event, t.scene);
    const auto place = PlaceOf(t.event, t.scene);
    if (s == nullptr || !place) {
        End("no place known: asked the old way", true);
        return std::nullopt;
    }
    const bool running = EventRunning(guest, main);
    if (t.set && running) {
        End("an event started after our eye's FirstOn (the room's asker asked it)", false);
        return std::nullopt;
    }
    if (now_ms > t.until_ms) {
        End(t.set ? "FirstOn set but no event started in time: asked the old way (cutscene only; plan (b) needed)"
                  : "our eye or its FirstOn plug not found in time: asked the old way",
            true);
        return std::nullopt;
    }
    if (running) return std::nullopt;  // another event: wait (not set while one runs)
    if (find_(guest, main, CoopLiveEvents::kAskerNames, *place, CoopLiveEvents::kAskerNearM).empty()) {
        // No asker: the room is cleared here (its asker deleted itself) or not loaded; the old way would not ask either.
        End(t.set ? "our room's asker is gone after FirstOn (its event ran)" : "no room asker here: not started",
            false);
        return std::nullopt;
    }
    const std::string_view eye_name[] = {s->eye};
    auto eyes = find_(guest, main, eye_name, *place, kEyeNearM);
    if (!s->every_eye && eyes.size() > 1) eyes.resize(1);
    std::uint64_t target = 0;
    bool any_plug = false;
    for (const std::uint64_t eye : eyes) {
        const auto plug = CoopLogicSet::BoolPlug(guest, eye, kFirstOn);
        if (!plug) continue;
        any_plug = true;
        if (plug->value == std::optional<std::uint8_t>(1)) continue;  // on (latched, or our own shot)
        target = eye;
        break;
    }
    if (target == 0) {
        if (any_plug && !t.set) {
            End("our eye's FirstOn is 1 already (our own shot): nothing to do", false);
        } else if (!t.told_waiting) {
            t.told_waiting = true;
            notes_.push_back("eye switch: " + t.event + ": waiting (" + std::to_string(eyes.size()) + " eye(s) near, " +
                             (t.set ? "FirstOn on" : "no live FirstOn plug yet") + ")");
        }
        return std::nullopt;
    }
    if (!CoopLogicSet::WriteNode(guest, block_, main, target, kFirstOn)) return std::nullopt;
    if (!t.set) {
        std::array<float, 3> p{};
        guest.Read(target + CoopPuppet::addr::kActorPosition, p.data(), sizeof p);
        const float d = std::sqrt((p[0] - (*place)[0]) * (p[0] - (*place)[0]) + (p[1] - (*place)[1]) * (p[1] - (*place)[1]) +
                                  (p[2] - (*place)[2]) * (p[2] - (*place)[2]));
        char line[256];
        std::snprintf(line, sizeof line,
                      "eye switch: %s: our eye 0x%llx (%.1f m from the asker) FirstOn set through the game's set-bool",
                      t.event.c_str(), static_cast<unsigned long long>(target), static_cast<double>(d));
        notes_.emplace_back(line);
    }
    t.set = true;
    next_ = Next::execute;
    return CoopLogicSet::Execute(main, block_);
}

}  // namespace CoopEyeSwitch
