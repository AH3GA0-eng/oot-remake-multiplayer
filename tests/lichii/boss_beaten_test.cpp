// SPDX-License-Identifier: GPL-3.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "coop/protocol/codec.hpp"
#include "coop_boss.h"
#include "coop_boss_beaten.h"
#include "coop_boss_scenes.h"
#include "coop_dungeon.h"
#include "coop_quests.h"

namespace {

using CoopBossBeaten::Apply;
using CoopBossBeaten::BeatenSet;
using CoopBossBeaten::Write;

// A game's flags: unset ones read as nothing.
struct Flags {
    std::map<std::uint32_t, std::uint32_t> v;
    std::optional<std::uint32_t> operator()(std::uint32_t hash) const {
        const auto it = v.find(hash);
        return it == v.end() ? std::nullopt : std::optional<std::uint32_t>(it->second);
    }
};

// A game that never saw the boss die: every bool of the set at the other value, the quest at the boss step.
Flags Alive(const BeatenSet& s) {
    Flags f;
    for (const auto& b : s.bools) {
        f.v[b.hash] = b.value ? 0 : 1;
    }
    f.v[s.quest] = s.below.back();
    return f;
}

const BeatenSet& Gohma() {
    const BeatenSet* s = CoopBossBeaten::ForRoom(0);
    REQUIRE(s != nullptr);
    return *s;
}

bool HasBool(const BeatenSet& s, std::string_view name, bool value) {
    return std::any_of(s.bools.begin(), s.bools.end(),
                       [&](const auto& b) { return b.name == name && b.value == value; });
}

} // namespace

TEST_CASE("boss beaten: the generated sets, one per boss (eight rooms, Ganondorf, Ganon), hashes from their names") {
    const auto sets = CoopBossBeaten::Sets();
    REQUIRE(sets.size() == 10);
    for (std::uint8_t room = 0; room < 10; ++room) {
        const BeatenSet* s = CoopBossBeaten::ForRoom(room);
        REQUIRE(s != nullptr);
        CAPTURE(s->boss);
        CHECK(std::string_view(s->boss) == CoopBossScenes::Bosses()[room].name);
        CHECK((CoopBoss::RoomByIndex(room) != nullptr) == (room < 8));
        REQUIRE(CoopBossBeaten::DeathScene(room).has_value());
        CHECK(s->step == coop::protocol::flag_hash(s->step_name));
        CHECK(!s->below.empty());
        CHECK(s->below.front() == coop::protocol::flag_hash("NotReady"));
        CHECK(std::find(s->below.begin(), s->below.end(), s->step) == s->below.end());
        CHECK(std::find(s->below.begin(), s->below.end(), coop::protocol::flag_hash("Complete")) == s->below.end());
        CHECK(!s->bools.empty());
        for (const auto& b : s->bools) {
            CAPTURE(b.name);
            CHECK(b.hash == coop::protocol::flag_hash(b.name));
            // the fight's own state is never given: the stale-flag mend clears it outside a boss room (issue 148)
            const auto ns = CoopDungeon::NotShared();
            CHECK(std::find(ns.begin(), ns.end(), std::string_view(b.name)) == ns.end());
        }
    }
    CHECK(CoopBossBeaten::ForRoom(10) == nullptr);
    CHECK_FALSE(CoopBossBeaten::DeathScene(10).has_value());
}

namespace {

const BeatenSet& Ganondorf() {
    const BeatenSet* s = CoopBossBeaten::ForRoom(8);
    REQUIRE(s != nullptr);
    return *s;
}

const BeatenSet& Ganon() {
    const BeatenSet* s = CoopBossBeaten::ForRoom(9);
    REQUIRE(s != nullptr);
    return *s;
}

} // namespace

TEST_CASE("boss beaten: Ganondorf and Ganon move Defeat_Ganon, each game's own quest (not shared)") {
    const BeatenSet& dorf = Ganondorf();
    const BeatenSet& ganon = Ganon();
    CHECK(std::string_view(dorf.quest_name) == "Defeat_Ganon");
    CHECK(dorf.quest == ganon.quest);
    CHECK(CoopQuests::Find(dorf.quest) == nullptr);  // coop_quests.cpp leaves the final battle out
    CHECK(std::string_view(dorf.step_name) == "Step06");
    CHECK(dorf.below.back() == coop::protocol::flag_hash("Step05"));
    CHECK(std::string_view(ganon.step_name) == "Complete");
    CHECK(ganon.below.back() == coop::protocol::flag_hash("Step09"));
    // Ganondorf's step is below Ganon's: a save past Ganondorf is still given Ganon's.
    CHECK(std::find(ganon.below.begin(), ganon.below.end(), dorf.step) != ganon.below.end());
    CHECK(HasBool(dorf, "HavePlayedEvent.Dm_InsideGanonCastle_BossOut", true));
    CHECK(HasBool(ganon, "FinalBattleField_DefeatGanonStart", true));
    CHECK(HasBool(ganon, "HavePlayedEvent.Dm_FinalBattle_BossOut", true));
}

TEST_CASE("boss beaten: Ganondorf and Ganon are given outside the end sequence's scenes, the furthest step once") {
    const BeatenSet& dorf = Ganondorf();
    const BeatenSet& ganon = Ganon();
    const std::uint32_t step05 = coop::protocol::flag_hash("Step05");
    const auto quest_writes = [&](const std::vector<Write>& w) {
        std::vector<std::uint32_t> v;
        for (const Write& x : w) {
            if (x.is_enum) {
                CHECK(x.hash == dorf.quest);
                v.push_back(x.value);
            }
        }
        return v;
    };
    // Ganondorf alone: in his castle, the escape or Ganon's field nothing; at the next load elsewhere Step06.
    for (const char* kept : {"InsideGanonCastle", "CrumblingGanonCastle", "FinalBattleField"}) {
        CAPTURE(kept);
        CHECK(CoopBossBeaten::KeptScene(8, kept));
        CHECK(CoopBossBeaten::KeptScene(9, kept));
        Flags f = Alive(dorf);
        Apply a;
        CHECK(a.Check(std::string(kept), true, std::vector<std::uint8_t>{8}, std::ref(f)).empty());
    }
    CHECK_FALSE(CoopBossBeaten::KeptScene(8, "GanonCastle"));  // the castle grounds: no fight there
    CHECK_FALSE(CoopBossBeaten::KeptScene(0, "InsideGanonCastle"));
    {
        Flags f = Alive(dorf);
        Apply a;
        const auto w = a.Check(std::string("GanonCastle"), true, std::vector<std::uint8_t>{8}, std::ref(f));
        CHECK(w.size() == dorf.bools.size() + 1);
        CHECK((quest_writes(w) == std::vector<std::uint32_t>{dorf.step}));
    }
    // Both beaten (a joiner told both, in either order): one Defeat_Ganon write, Complete.
    for (const std::vector<std::uint8_t>& beaten : {std::vector<std::uint8_t>{8, 9}, std::vector<std::uint8_t>{9, 8}}) {
        Flags f = Alive(dorf);
        for (const auto& b : ganon.bools) {
            f.v[b.hash] = b.value ? 0 : 1;
        }
        f.v[dorf.quest] = step05;
        Apply a;
        const auto w = a.Check(std::string("HyruleField"), true, beaten, std::ref(f));
        CHECK((quest_writes(w) == std::vector<std::uint32_t>{ganon.step}));
        CHECK(w.size() == dorf.bools.size() + ganon.bools.size() + 1);
    }
    // Past Ganondorf already (Step06, Step07): only Ganon's step; a finished game (Complete): no step at all.
    for (const char* at : {"Step06", "Step07"}) {
        Flags f;
        f.v[dorf.quest] = coop::protocol::flag_hash(at);
        Apply a;
        CHECK((quest_writes(a.Check(std::string("HyruleField"), true, std::vector<std::uint8_t>{8, 9}, std::ref(f))) ==
               std::vector<std::uint32_t>{ganon.step}));
    }
    Flags done;
    done.v[dorf.quest] = coop::protocol::flag_hash("Complete");
    Apply a;
    CHECK(quest_writes(a.Check(std::string("HyruleField"), true, std::vector<std::uint8_t>{8, 9}, std::ref(done)))
              .empty());
}

TEST_CASE("boss beaten: Gohma's set is what a save that beat her holds (hs-gohma-death's save prep)") {
    const BeatenSet& s = Gohma();
    CHECK(std::string_view(s.quest_name) == "Get_StoneOfForest");
    CHECK(std::string_view(s.step_name) == "GoToWarpPoint");
    CHECK(HasBool(s, "Gohma_DefeatBoss", true));
    CHECK(HasBool(s, "HavePlayedEvent.Dm_Deku_BossIn", true));
    CHECK(HasBool(s, "HavePlayedEvent.Dm_Deku_BossOut", true));
    CHECK(HasBool(s, "HavePlayedEvent.InsideTheDekuTree_BossDie", true));
    CHECK(!HasBool(s, "InsideTheDekuTree_BossArea_ShutIn", true));
    CHECK(HasBool(*CoopBossBeaten::ForRoom(1), "Enemy_DungeonBoss_Dodongo_BossAppear", false));
    CHECK(HasBool(*CoopBossBeaten::ForRoom(5), "Morpha_DeleteWater", true));
}

TEST_CASE("boss beaten: the shared quests' steps agree with CoopQuests (the step after the boss, the ones before)") {
    int checked = 0;
    for (const BeatenSet& s : CoopBossBeaten::Sets()) {
        const CoopQuests::Quest* q = CoopQuests::Find(s.quest);
        if (q == nullptr) {
            continue;  // SageOfWater, SageOfShadow, SageOfSpirit are not shared (coop_quests.cpp)
        }
        CAPTURE(s.quest_name);
        CHECK(std::string_view(q->name) == s.quest_name);
        const auto at = CoopQuests::Rank(*q, s.step);
        REQUIRE(at.has_value());
        REQUIRE(s.below.size() == static_cast<std::size_t>(*at));
        for (std::size_t i = 0; i < s.below.size(); ++i) {
            CHECK(s.below[i] == coop::protocol::flag_hash(q->steps[i]));
        }
        ++checked;
    }
    CHECK(checked == 5);
}

TEST_CASE("boss beaten: record") {
    CoopBossBeaten::Record r;
    CHECK(r.Add(0));
    CHECK(!r.Add(0));
    CHECK(r.Add(9));    // Ganon: no dungeon room, a set of its own
    CHECK(!r.Add(10));  // no boss
    CHECK(r.Add(3));
    CHECK((r.Rooms() == std::vector<std::uint8_t>{0, 9, 3}));
    r.Clear();
    CHECK(r.Rooms().empty());
}

TEST_CASE("boss beaten: given one way, once per scene load, never in the boss's own scene") {
    const BeatenSet& s = Gohma();
    const std::vector<std::uint8_t> beaten{0};
    Flags f = Alive(s);
    Apply a;
    const std::string deku(CoopBoss::RoomByIndex(0)->scene);

    // In the Deku Tree (the boss room or anywhere in it): its room's logic is loaded, nothing given.
    CHECK(a.Check(deku, true, beaten, std::ref(f)).empty());
    // Loading, then a field: nothing before Link's place reads.
    CHECK(a.Check(std::nullopt, false, beaten, std::ref(f)).empty());
    CHECK(a.Check(std::string("KokiriForest"), false, beaten, std::ref(f)).empty());
    std::vector<Write> w = a.Check(std::string("KokiriForest"), true, beaten, std::ref(f));
    REQUIRE(w.size() == s.bools.size() + 1);
    for (const auto& b : s.bools) {
        CHECK(std::any_of(w.begin(), w.end(), [&](const Write& x) {
            return !x.is_enum && x.hash == b.hash && x.value == (b.value ? 1u : 0u);
        }));
    }
    CHECK(w.back().is_enum);
    CHECK(w.back().hash == s.quest);
    CHECK(w.back().value == s.step);
    // Once per scene load.
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    // The writes took: the next scene load finds nothing to give.
    for (const Write& x : w) {
        f.v[x.hash] = x.value;
    }
    CHECK(a.Check(std::nullopt, false, beaten, std::ref(f)).empty());
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    // A reload of an older save of ours (the boss alive in it again): the next scene load gives it again (#241).
    f = Alive(s);
    CHECK(a.Check(std::nullopt, false, beaten, std::ref(f)).empty());
    CHECK(a.Check(std::string("HyruleField"), true, beaten, std::ref(f)).size() == s.bools.size() + 1);
}

TEST_CASE("boss beaten: a finished dungeon is never stepped back, unread flags are left alone") {
    const BeatenSet& s = Gohma();
    const std::vector<std::uint8_t> beaten{0};
    Flags f = Alive(s);
    f.v[s.quest] = coop::protocol::flag_hash("Complete");
    f.v.erase(s.bools.front().hash);
    Apply a;
    const std::vector<Write> w = a.Check(std::string("KokiriForest"), true, beaten, std::ref(f));
    CHECK(w.size() == s.bools.size() - 1);
    CHECK(std::none_of(w.begin(), w.end(), [](const Write& x) { return x.is_enum; }));
    CHECK(std::none_of(w.begin(), w.end(), [&](const Write& x) { return x.hash == s.bools.front().hash; }));
}

TEST_CASE("boss beaten: a boss beaten while we stand in a scene is given at its next load, never mid-scene") {
    // boss-away-crash (review of c78d17c4): Kokiri Forest's Npc_Kokiri_011 reads Gohma_DefeatBoss live, and in the game
    // alone it never changes while Kokiri Forest is loaded (the kill is in the Deku Tree).
    const BeatenSet& s = Gohma();
    Flags f = Alive(s);
    Apply a;
    CHECK(a.Check(std::string("KokiriForest"), true, {}, std::ref(f)).empty());  // co-op idle or nothing beaten
    const std::vector<std::uint8_t> beaten{0};
    // The guests beat Gohma while we stand in Kokiri Forest: nothing written in this scene, however long we stay.
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    // The next load (the same scene again too): given then, once.
    CHECK(a.Check(std::nullopt, false, beaten, std::ref(f)).empty());
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).size() == s.bools.size() + 1);
    CHECK(a.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    // A scene reached without a loading frame between (a new scene name): that is a load too.
    Apply b;
    CHECK(b.Check(std::string("KokiriForest"), true, {}, std::ref(f)).empty());
    CHECK(b.Check(std::string("KokiriForest"), true, beaten, std::ref(f)).empty());
    CHECK(b.Check(std::string("LostWoods"), true, beaten, std::ref(f)).size() == s.bools.size() + 1);
}

TEST_CASE("boss beaten: every boss's set waits for the next load when beaten mid-scene") {
    // Every set, not only Gohma's: Dodongo's BossOut is read by Death Mountain Trail, Phantom Ganon's
    // SageOfForest_IsBefore_Step08 by Kokiri Forest. A field none of the bosses is in.
    for (const BeatenSet& set : CoopBossBeaten::Sets()) {
        CAPTURE(set.boss);
        Flags f = Alive(set);
        Apply a;
        const std::vector<std::uint8_t> beaten{set.room};
        CHECK(a.Check(std::string("HyruleField"), true, {}, std::ref(f)).empty());
        CHECK(a.Check(std::string("HyruleField"), true, beaten, std::ref(f)).empty());
        CHECK(a.Check(std::nullopt, false, beaten, std::ref(f)).empty());
        CHECK_FALSE(a.Check(std::string("HyruleField"), true, beaten, std::ref(f)).empty());
    }
}

TEST_CASE("boss beaten: a record cleared mid-scene (the host loaded a save) gives nothing in that scene") {
    const BeatenSet& s = Gohma();
    Flags f = Alive(s);
    Apply a;
    const std::vector<std::uint8_t> beaten{0};
    CHECK(a.Check(std::string("KokiriForest"), false, beaten, std::ref(f)).empty());  // loaded with it, place unread
    CHECK(a.Check(std::string("KokiriForest"), true, {}, std::ref(f)).empty());       // cleared before it was given
}

TEST_CASE("boss beaten: only a proven death is one (a game took the boss's death event itself)") {
    namespace proto = coop::protocol;
    const std::string deku(CoopBoss::RoomByIndex(0)->scene);
    const auto scene = [](std::uint8_t boss, const std::string& where, proto::BossSceneKind kind) {
        return proto::Message{proto::BossScene{2, where, boss, kind, {}}};
    };
    CHECK(CoopBossBeaten::ProvenDeath(scene(0, deku, proto::BossSceneKind::outro_start)) == std::uint8_t{0});
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(0, "GohmaStage", proto::BossSceneKind::outro_start)));  // boss rush
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(0, deku, proto::BossSceneKind::intro_start)));
    CHECK(CoopBossBeaten::ProvenDeath(scene(0, deku, proto::BossSceneKind::outro_end)) == std::uint8_t{0});
    // Ganondorf in his castle, Ganon on his field; never their boss rush stages or another scene.
    CHECK(CoopBossBeaten::ProvenDeath(scene(8, "InsideGanonCastle", proto::BossSceneKind::outro_start)) ==
          std::uint8_t{8});
    CHECK(CoopBossBeaten::ProvenDeath(scene(9, "FinalBattleField", proto::BossSceneKind::outro_end)) == std::uint8_t{9});
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(8, "GanondorfStage", proto::BossSceneKind::outro_start)));
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(9, "GanonStage", proto::BossSceneKind::outro_start)));
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(9, "InsideGanonCastle", proto::BossSceneKind::outro_start)));
    CHECK_FALSE(CoopBossBeaten::ProvenDeath(scene(10, "InsideGanonCastle", proto::BossSceneKind::outro_start)));
    // BOSS_ROOM defeated is maybe a guess (a long event, no reading): never one, from anyone.
    for (const std::uint8_t player : {std::uint8_t{0}, std::uint8_t{2}}) {
        CHECK_FALSE(
            CoopBossBeaten::ProvenDeath(proto::Message{proto::BossRoom{player, deku, 0, proto::BossState::defeated}}));
    }
}

TEST_CASE("boss beaten: BOSS_SCENE's boss index is the room for the eight dungeon bosses") {
    const auto bosses = CoopBossScenes::Bosses();
    for (std::uint8_t i = 0; i < 8; ++i) {
        CHECK(bosses[i].room == static_cast<int>(i));
    }
}
