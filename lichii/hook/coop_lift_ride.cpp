// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_lift_ride.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "coop_fairy.h"
#include "coop_logic_probe.h"

namespace CoopLiftRide {

bool IsRideLift(std::string_view name) {
    if (!name.starts_with("Obj_")) return false;
    if (name.starts_with("Obj_Deku_") || name.starts_with("Obj_InsideTheDekuTree_")) return false;
    return name.find("ElevatorLift_") != std::string_view::npos || name.find("MoveFloor_") != std::string_view::npos;
}

std::vector<std::uint64_t> Ridden(const std::vector<Lift>& lifts, const std::vector<coop::protocol::Vec3>& others) {
    std::vector<std::uint64_t> out;
    for (const Lift& l : lifts) {
        for (const auto& p : others) {
            const float dx = p[0] - l.position[0];
            const float dz = p[2] - l.position[2];
            const float dy = p[1] - l.position[1];
            if (dx * dx + dz * dz <= kRideRadiusM * kRideRadiusM && dy >= -kRideBelowM && dy <= kRideAboveM) {
                out.push_back(l.address);
                break;
            }
        }
    }
    return out;
}

void Rider::Want(const std::vector<std::uint64_t>& lifts, std::uint64_t main, std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    main_ = main;
    for (const std::uint64_t lift : lifts) {
        auto it = std::find_if(wanted_.begin(), wanted_.end(), [lift](const auto& w) { return w.first == lift; });
        if (it == wanted_.end()) {
            wanted_.emplace_back(lift, now_ms + kHoldMs);
        } else {
            it->second = now_ms + kHoldMs;
        }
    }
}

void Rider::Clear() {
    std::lock_guard lock(mutex_);
    wanted_.clear();
    told_.clear();
}

std::vector<std::string> Rider::TakeNotes() {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out;
    out.swap(notes_);
    return out;
}

std::optional<CoopPuppet::Call> Rider::Step(CoopPuppet::Guest& guest, bool after_call, std::uint64_t result,
                                            std::uint64_t now_ms) {
    std::uint64_t main = 0;
    {
        std::lock_guard lock(mutex_);
        main = main_;
        if (after_call && asked_malloc_) {
            asked_malloc_ = false;
            if (result == 0) {
                failed_ = true;
                notes_.push_back("lift ride: the game's malloc gave nothing; lift rides off");
                return std::nullopt;
            }
            block_ = result;
        }
        if (!after_call) {
            // A new frame: every lift still wanted, once.
            wanted_.erase(std::remove_if(wanted_.begin(), wanted_.end(),
                                         [now_ms](const auto& w) { return w.second < now_ms; }),
                          wanted_.end());
            frame_.clear();
            for (const auto& w : wanted_) frame_.push_back(w.first);
        }
    }
    if (failed_ || main == 0 || frame_.empty()) return std::nullopt;
    if (block_ == 0) {
        if (after_call) return std::nullopt;  // the malloc is asked at the start of a frame only
        std::lock_guard lock(mutex_);
        asked_malloc_ = true;
        CoopPuppet::Call c;
        c.function = main + addr::kMalloc;
        c.x[0] = addr::kBlockSize;
        return c;
    }
    return Next(guest, main);
}

std::optional<CoopPuppet::Call> Rider::Next(CoopPuppet::Guest& guest, std::uint64_t main) {
    while (!frame_.empty()) {
        const std::uint64_t lift = frame_.front();
        frame_.pop_front();
        if (!CoopFairy::ActorPresent(guest, lift)) continue;
        // The execute reads [actor+0x270] without a check: only a lift whose logic has a live bool RideOnPlayer.
        const auto plugs = CoopLogicProbe::Read(
            [&guest](std::uint64_t at) -> std::optional<std::uint64_t> {
                std::uint64_t v = 0;
                if (!guest.Read(at, &v, 8)) return std::nullopt;
                return v;
            },
            lift);
        if (!plugs) continue;
        const auto plug = std::find_if(plugs->begin(), plugs->end(), [](const CoopLogicProbe::Plug& p) {
            return p.key == (kRideOnPlayer ^ 7u) && p.type == CoopLogicProbe::kBool;
        });
        if (plug == plugs->end()) continue;
        if (plug->value == std::uint8_t{1}) continue;  // on already (our Link, or set this frame)
        const std::uint64_t vtable = main + addr::kVtable;
        const std::uint64_t owner = block_ + addr::kOwner;
        const std::uint64_t holder = block_ + addr::kHolder;
        const std::uint64_t value_at = block_ + addr::kValueByte;
        const std::uint8_t flags = 2;
        const std::uint32_t key = kRideOnPlayer;
        const std::uint8_t one = 1;
        if (!guest.Write(block_, &vtable, 8) || !guest.Write(block_ + addr::kNodeOwner, &owner, 8) ||
            !guest.Write(block_ + addr::kNodeFlags, &flags, 1) || !guest.Write(block_ + addr::kNodeKey, &key, 4) ||
            !guest.Write(block_ + addr::kNodeValue, &value_at, 8) || !guest.Write(value_at, &one, 1) ||
            !guest.Write(owner + 8, &holder, 8) || !guest.Write(holder + addr::kHolderActor, &lift, 8)) {
            continue;
        }
        {
            std::lock_guard lock(mutex_);
            if (std::find(told_.begin(), told_.end(), lift) == told_.end()) {
                told_.push_back(lift);
                char line[160];
                std::snprintf(line, sizeof line,
                              "lift ride: another player is on our lift 0x%llx: its RideOnPlayer set through the "
                              "game's set-bool",
                              static_cast<unsigned long long>(lift));
                notes_.emplace_back(line);
            }
        }
        CoopPuppet::Call c;
        c.function = main + addr::kExecute;
        c.x[0] = block_;
        return c;
    }
    return std::nullopt;
}

Rider& TheRider() {
    static Rider rider;
    return rider;
}

}  // namespace CoopLiftRide
