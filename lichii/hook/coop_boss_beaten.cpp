// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_boss_beaten.h"

#include <algorithm>
#include <variant>

#include "coop_boss.h"

namespace CoopBossBeaten {

namespace {

#include "coop_boss_beaten.inc"

bool Has(std::span<const std::uint8_t> rooms, std::uint8_t room) {
    return std::find(rooms.begin(), rooms.end(), room) != rooms.end();
}

} // namespace

std::span<const BeatenSet> Sets() {
    return kSets;
}

const BeatenSet* ForRoom(std::uint8_t room) {
    for (const BeatenSet& s : kSets) {
        if (s.room == room) {
            return &s;
        }
    }
    return nullptr;
}

std::optional<std::string_view> DeathScene(std::uint8_t room) {
    if (ForRoom(room) == nullptr) {
        return std::nullopt;
    }
    if (const CoopBoss::Room* where = CoopBoss::RoomByIndex(room)) {
        return std::string_view(where->scene);
    }
    switch (room) {
    case 8:
        return std::string_view("InsideGanonCastle");  // Ganondorf's room (Room_26, Create_Ganondorf)
    case 9:
        return std::string_view("FinalBattleField");  // Ganon (FinalBattleField_* logic, Banc GanonCastle/)
    default:
        return std::nullopt;
    }
}

bool KeptScene(std::uint8_t room, std::string_view scene) {
    const std::optional<std::string_view> death = DeathScene(room);
    if (!death) {
        return true;  // no set: never given anywhere
    }
    if (room == 8 || room == 9) {
        return scene == "InsideGanonCastle" || scene == "CrumblingGanonCastle" || scene == "FinalBattleField";
    }
    return scene == *death;
}

bool Record::Add(std::uint8_t room) {
    if (ForRoom(room) == nullptr) {
        return false;
    }
    std::lock_guard lock(mutex_);
    if (Has(rooms_, room)) {
        return false;
    }
    rooms_.push_back(room);
    return true;
}

void Record::Clear() {
    std::lock_guard lock(mutex_);
    rooms_.clear();
}

std::vector<std::uint8_t> Record::Rooms() const {
    std::lock_guard lock(mutex_);
    return rooms_;
}

std::optional<std::uint8_t> ProvenDeath(const coop::protocol::Message& m) {
    namespace proto = coop::protocol;
    const auto in_its_dungeon = [](std::uint8_t room, const std::string& scene) -> std::optional<std::uint8_t> {
        const std::optional<std::string_view> where = DeathScene(room);
        if (!where || *where != scene) {
            return std::nullopt;
        }
        return room;
    };
    const auto* s = std::get_if<proto::BossScene>(&m);
    if (s == nullptr ||
        (s->kind != proto::BossSceneKind::outro_start && s->kind != proto::BossSceneKind::outro_end)) {
        return std::nullopt;  // never BOSS_ROOM defeated: a game also sends it on a guess
    }
    return in_its_dungeon(s->boss, s->scene);
}

Record& TheRecord() {
    static Record record;
    return record;
}

std::vector<Write> Apply::Check(const std::optional<std::string>& scene, bool place_known,
                                std::span<const std::uint8_t> beaten,
                                const std::function<std::optional<std::uint32_t>(std::uint32_t)>& read) {
    if (!scene) {
        scene_.reset();  // loading: the scene after it (the same one too) checks again
        loaded_.clear();
        checked_.clear();
        return {};
    }
    if (!scene_ || *scene_ != *scene) {
        scene_ = scene;
        loaded_.assign(beaten.begin(), beaten.end());  // a later record change waits for the next load
        checked_.clear();
    }
    if (!place_known) {
        return {};
    }
    std::vector<Write> out;
    for (const std::uint8_t room : loaded_) {
        if (!Has(beaten, room)) {
            continue;  // the record was cleared since (the host loaded a save): that boss fights again
        }
        const BeatenSet* set = ForRoom(room);
        if (set == nullptr || Has(checked_, room) || KeptScene(room, *scene)) {
            continue;  // in the boss's own scene: its room's logic is loaded (maybe a fight); the next scene gives it
        }
        checked_.push_back(room);
        for (const BeatenBool& b : set->bools) {
            const std::optional<std::uint32_t> now = read(b.hash);
            if (now && (*now != 0) != b.value) {
                out.push_back(Write{room, b.hash, false, b.value ? 1u : 0u, b.name});
            }
        }
        const std::optional<std::uint32_t> step = read(set->quest);
        if (step && std::find(set->below.begin(), set->below.end(), *step) != set->below.end()) {
            // Two sets of one quest (Ganondorf's Step06, Ganon's Complete), both read before either is written: one
            // write, the furthest step (the one the other's step is below), whatever order the record holds them in.
            const auto same = std::find_if(out.begin(), out.end(),
                                           [&](const Write& w) { return w.is_enum && w.hash == set->quest; });
            if (same == out.end()) {
                out.push_back(Write{room, set->quest, true, set->step, set->quest_name});
            } else if (std::find(set->below.begin(), set->below.end(), same->value) != set->below.end()) {
                *same = Write{room, set->quest, true, set->step, set->quest_name};
            }
        }
    }
    return out;
}

void Apply::Reset() {
    scene_.reset();
    loaded_.clear();
    checked_.clear();
}

} // namespace CoopBossBeaten
