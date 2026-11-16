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
// tools/puppet_pack/eye_switches.py on the romfs of build e0f3ccfaf50065ae (1.1.1), 2026-11-16.
constexpr Switch kSwitches[] = {
    {"DodongoCavern_Gimmick_EyeSwitch", "DodongoCavern", "Obj_EyeSwitch_A_05", {80.5f, 121.0f, -6.6f}, {68.2f, 127.3f, -6.6f}},
    {"FireTemple_06_EyeSwitch", "FireTemple", "Obj_EyeSwitch_A_02", {34.2f, 151.6f, 0.1f}, {36.7f, 150.8f, 3.9f}},
    {"ForestTemple_Room11_TBoxAppear", "ForestTemple", "Obj_EyeSwitch_A_02", {-52.8f, 119.5f, 5.9f}, {-52.8f, 115.0f, 2.0f}},
    {"ForestTemple_Room17_ShutterOpen", "ForestTemple", "Obj_EyeSwitch_A_02", {-13.8f, 118.9f, -59.5f}, {-7.5f, 121.5f, -59.5f}},
    {"ForestTemple_Room27_ShutterOpen", "ForestTemple", "Obj_EyeSwitch_A_02", {24.9f, 62.1f, -3.5f}, {0.0f, 65.7f, -19.6f}},
    {"GerudoTrainingGround_00_FallTreasureBox", "GerudoTrainingGround", "Obj_EyeSwitch_A_02", {-1.8f, 109.8f, 10.8f}, {2.5f, 99.5f, -9.8f}},
    {"GerudoTrainingGround_03_ShutterOpen", "GerudoTrainingGround", "Obj_EyeSwitch_GlassTruth_A_02", {-47.3f, 109.2f, -110.6f}, {-36.3f, 119.5f, -92.9f}},
    {"GerudoTrainingGround_04_ActStoneGimmick", "GerudoTrainingGround", "Obj_EyeSwitch_A_05", {24.1f, 102.5f, -82.4f}, {13.1f, 99.9f, -87.5f}},
    {"GerudoTrainingGround_05_ShutterOpen", "GerudoTrainingGround", "Obj_EyeSwitch_A_03", {46.9f, 101.9f, -92.4f}, {34.4f, 108.5f, -89.8f}},
    {"GerudoTrainingGround_10_FallTreasureBox", "GerudoTrainingGround", "Obj_EyeSwitch_A_02", {-54.2f, 109.1f, -131.7f}, {-51.4f, 103.5f, -131.8f}},
    {"InsideTheDekuTree_Gimmick_FallingLadder", "InsideTheDekuTree", "Obj_EyeSwitch_A_02", {-35.6f, 114.3f, 10.7f}, {-37.5f, 117.8f, 12.5f}},
    {"InsideTheDekuTree_Gimmick_RoomF_EyeSwitch", "InsideTheDekuTree", "Obj_EyeSwitch_A_02", {-17.1f, 83.0f, 29.6f}, {-14.5f, 82.7f, 27.8f}},
    {"SpiritTemple_Level_Room_04_EyeSwitchOn", "SpiritTemple", "Obj_EyeSwitch_A_02", {-33.6f, 104.0f, -86.0f}, {-31.3f, 107.9f, -81.7f}},
    {"SpiritTemple_Level_Room_30_BoardNetFall", "SpiritTemple", "Obj_EyeSwitch_A_03", {14.4f, 152.7f, -76.7f}, {19.8f, 159.5f, -77.8f}},
    {"WaterTemple_EyeSwitch_Focus", "WaterTemple", "Obj_EyeSwitch_A_05", {-33.2f, 115.8f, -40.2f}, {-33.5f, 118.9f, -40.7f}},
    {"WaterTemple_EyeSwitch_Focus", "WaterTemple", "Obj_EyeSwitch_A_05", {-4.5f, 116.6f, -26.3f}, {0.7f, 116.1f, -29.3f}},
};

float Dist2(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}
}  // namespace

std::span<const Switch> Switches() { return kSwitches; }

const Switch* Find(std::string_view event, std::string_view scene) {
    for (const auto& s : kSwitches) {
        if (s.event == event && s.scene == scene) return &s;
    }
    return nullptr;
}

std::vector<const Switch*> FindAll(std::string_view event, std::string_view scene,
                                   const std::optional<std::array<float, 3>>& link) {
    std::vector<const Switch*> out;
    for (const auto& s : kSwitches) {
        if (s.event == event && s.scene == scene) out.push_back(&s);
    }
    if (link) {
        std::stable_sort(out.begin(), out.end(), [&](const Switch* x, const Switch* y) {
            return Dist2(x->eye_at, *link) < Dist2(y->eye_at, *link);
        });
    }
    return out;
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

bool Switcher::Want(const std::string& event, const std::string& scene, std::optional<std::array<float, 3>> link) {
    if (Find(event, scene) == nullptr) return false;
    std::lock_guard lock(mutex_);
    if (failed_) return false;
    if (std::none_of(wanted_.begin(), wanted_.end(), [&](const Try& t) { return t.event == event; })) {
        Try t;
        t.event = event;
        t.scene = scene;
        t.link = link;
        wanted_.push_back(std::move(t));
    }
    return true;
}

void Switcher::Expire(const std::string& scene, const std::function<bool(const std::string&)>& played) {
    std::lock_guard lock(mutex_);
    for (auto it = wanted_.begin(); it != wanted_.end();) {
        const bool busy = it == wanted_.begin() && next_ != Next::none;  // a call of ours is out: next Step ends it
        if (!busy && !scene.empty() && it->scene != scene) {
            notes_.push_back("eye switch: " + it->event + " dropped (we left its scene)");
            it = wanted_.erase(it);
        } else if (!busy && played && played(it->event)) {
            notes_.push_back("eye switch: " + it->event + " dropped (our game has it played)");
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
    const auto lines = FindAll(t.event, t.scene, t.link);
    if (lines.empty()) {
        End("not in the eye table: asked the old way", true);
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
    // The first line (nearest our Link) whose asker is here; then only its own eye, at its placement.
    const Switch* line = nullptr;
    for (const Switch* l : lines) {
        if (!find_(guest, main, CoopLiveEvents::kAskerNames, l->asker_at, CoopLiveEvents::kAskerNearM).empty()) {
            line = l;
            break;
        }
    }
    if (line == nullptr) {
        // No asker: the room is cleared here (its asker deleted itself) or not loaded; the old way would not ask either.
        End(t.set ? "our room's asker is gone after FirstOn (its event ran)" : "no room asker here: not started",
            false);
        return std::nullopt;
    }
    const std::string_view eye_name[] = {line->eye};
    const auto eyes = find_(guest, main, eye_name, line->eye_at, kEyeAtM);
    std::uint64_t target = 0;
    bool any_plug = false;
    for (const std::uint64_t eye : eyes) {
        const auto plug = CoopLogicSet::BoolPlug(guest, eye, kFirstOn);
        if (!plug) continue;
        any_plug = true;
        if (plug->value != std::optional<std::uint8_t>(1)) target = eye;  // 1: latched, or our own shot
        break;
    }
    if (target == 0) {
        if (any_plug && !t.set) {
            End("our eye's FirstOn is 1 already (our own shot): nothing to do", false);
        } else if (!t.told_waiting) {
            t.told_waiting = true;
            notes_.push_back("eye switch: " + t.event + ": waiting (" + std::to_string(eyes.size()) + " " +
                             std::string(line->eye) + " at its place, " +
                             (t.set ? "FirstOn on" : "no live FirstOn plug yet") + ")");
        }
        return std::nullopt;
    }
    if (!CoopLogicSet::WriteNode(guest, block_, main, target, kFirstOn)) return std::nullopt;
    if (!t.set) {
        std::array<float, 3> p{};
        guest.Read(target + CoopPuppet::addr::kActorPosition, p.data(), sizeof p);
        const float d = std::sqrt(Dist2(p, line->asker_at));
        char text[256];
        std::snprintf(text, sizeof text,
                      "eye switch: %s: our eye 0x%llx (%.1f m from the asker) FirstOn set through the game's set-bool",
                      t.event.c_str(), static_cast<unsigned long long>(target), static_cast<double>(d));
        notes_.emplace_back(text);
    }
    t.set = true;
    next_ = Next::execute;
    return CoopLogicSet::Execute(main, block_);
}

}  // namespace CoopEyeSwitch
