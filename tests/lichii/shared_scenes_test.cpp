// Tests for shared story cutscenes (lichii/hook/coop_shared_scenes.cpp).
#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <ostream>   // (CHECK_MESSAGE prints a string_view: MSVC needs operator<< declared)
#include <string>
#include <string_view>
#include <vector>

#include "coop/host/flag_watcher.hpp"
#include "coop/protocol/codec.hpp"
#include "coop_live_events.h"
#include "coop_shared_scenes.h"

namespace {

using CoopSharedScenes::Here;
using CoopSharedScenes::Joiner;
using CoopSharedScenes::Vec3;

constexpr Vec3 kBridge{10.0f, 0.0f, 20.0f};

Here Free(std::uint64_t now_us, Vec3 link = {15.0f, 0.0f, 22.0f}, std::string scene = "KokiriForest") {
    Here h;
    h.scene = std::move(scene);
    h.can_start = true;
    h.link = link;
    h.now_us = now_us;
    return h;
}

const auto kNotPlayed = [](const std::string&) { return false; };

} // namespace

TEST_CASE("the story cutscene list is sorted, has the story scenes and leaves out the rest") {
    const auto names = CoopSharedScenes::Names();
    CHECK(names.size() > 150);
    CHECK(std::is_sorted(names.begin(), names.end()));
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());
    CHECK(CoopSharedScenes::IsShared("Dm_Kokiri_0030"));
    CHECK(CoopSharedScenes::IsShared("Meet_DekuTree"));
    CHECK(CoopSharedScenes::IsShared("Get_StoneOfForest"));
    CHECK_FALSE(CoopSharedScenes::IsShared("Dm_Kokiri_0000"));       // the opening
    CHECK_FALSE(CoopSharedScenes::IsShared("Dm_Deku_BossIn"));       // dungeons-boss
    CHECK_FALSE(CoopSharedScenes::IsShared("Dm_SY_BancChangeArrive"));
    CHECK_FALSE(CoopSharedScenes::IsShared("Dm_TimeTemple_0010"));   // time travel
    CHECK_FALSE(CoopSharedScenes::IsShared("Dm_Awake_Forest_Effect"));
    CHECK_FALSE(CoopSharedScenes::IsShared("Npc_Kokiri_Saria_Child"));  // talks stay local
    CHECK_FALSE(CoopSharedScenes::IsShared("InsideTheDekuTree_Gimmick_RoomD_PlatformUp"));
    // None of them is a gimmick event (those go by coop_live_events' rules).
    for (const auto& p : CoopLiveEvents::Places()) {
        CHECK_FALSE(CoopSharedScenes::IsShared(p.event));
    }
}

TEST_CASE("another player's story cutscene near us plays here once, at once") {
    Joiner j;
    CHECK_FALSE(j.Next(Free(0), kNotPlayed));
    j.Started(1, "Dm_Kokiri_0030", "KokiriForest", kBridge, 100);
    const auto got = j.Next(Free(120), kNotPlayed);
    REQUIRE(got);
    CHECK(*got == "Dm_Kokiri_0030");
    const auto notes = j.TakeNotes();
    REQUIRE(notes.size() == 1);
    CHECK(notes[0].find("playing it here too") != std::string::npos);
    // Our watch then finds it running here: not sent back.
    CHECK_FALSE(j.Ours("Dm_Kokiri_0030", "KokiriForest"));
    // The same start again this visit: not played twice.
    j.Started(2, "Dm_Kokiri_0030", "KokiriForest", kBridge, 200);
    CHECK_FALSE(j.Next(Free(220), kNotPlayed));
    CHECK(j.TakeNotes()[0].find("played here already") != std::string::npos);
}

TEST_CASE("a story cutscene our game starts goes out once, a later start from another player is not replayed") {
    Joiner j;
    CHECK(j.Ours("Meet_DekuTree", "KokiriForest"));
    CHECK_FALSE(j.Ours("Meet_DekuTree", "KokiriForest"));  // the watch finds it again: sent once
    j.Started(1, "Meet_DekuTree", "KokiriForest", kBridge, 10);  // both started it together
    CHECK_FALSE(j.Next(Free(20), kNotPlayed));
    // A new visit of the scene: it may go out again.
    CHECK_FALSE(j.Next(Free(30, kBridge, "LostWoods"), kNotPlayed));
    CHECK(j.Ours("Meet_DekuTree", "KokiriForest"));
}

TEST_CASE("players not in the room, in another scene, or with it played are not pulled in") {
    Joiner j;
    j.Started(1, "Dm_Kokiri_0030", "KokiriForest", Vec3{100.0f, 0.0f, 20.0f}, 0);
    CHECK_FALSE(j.Next(Free(10), kNotPlayed));
    CHECK(j.TakeNotes()[0].find("too far") != std::string::npos);

    j.Started(1, "Dm_Kokiri_0045", "KokiriForest", kBridge, 0);
    CHECK_FALSE(j.Next(Free(10, kBridge, "LostWoods"), kNotPlayed));
    CHECK(j.Waiting() == 0);  // another scene: dropped

    j.Started(1, "Dm_LostWood_0010", "LostWoods", kBridge, 20);
    CHECK_FALSE(j.Next(Free(30, kBridge, "LostWoods"), [](const std::string&) { return true; }));
    CHECK(j.TakeNotes().back().find("has it played already") != std::string::npos);

    j.Started(1, "Dm_LostWood_0015", "LostWoods", std::nullopt, 40);
    CHECK_FALSE(j.Next(Free(50, kBridge, "LostWoods"), kNotPlayed));
    CHECK(j.TakeNotes().back().find("not known") != std::string::npos);
}

TEST_CASE("a busy game waits up to 3 s, then skips the cutscene") {
    Joiner j;
    j.Started(1, "Dm_Kokiri_0030", "KokiriForest", kBridge, 1'000'000);
    Here busy = Free(1'500'000);
    busy.can_start = false;  // our own talk, a menu or a pause
    CHECK_FALSE(j.Next(busy, kNotPlayed));
    CHECK(j.Waiting() == 1);
    CHECK(j.TakeNotes().empty());
    // Free again within the time: joined late.
    REQUIRE(j.Next(Free(2'000'000), kNotPlayed));

    j.Started(1, "Dm_Kokiri_0045", "KokiriForest", kBridge, 5'000'000);
    Here loading;
    loading.now_us = 6'000'000;  // a load: no scene, no Link
    CHECK_FALSE(j.Next(loading, kNotPlayed));
    CHECK(j.Waiting() == 1);
    busy.now_us = 8'000'000;
    CHECK_FALSE(j.Next(busy, kNotPlayed));
    CHECK(j.Waiting() == 0);
    CHECK(j.TakeNotes().back().find("busy for 3 s") != std::string::npos);
}

TEST_CASE("LICHII_COOP_SHARED_CUTSCENES: on unless 0") {
    CHECK(CoopSharedScenes::OnFromEnv(""));
    CHECK(CoopSharedScenes::OnFromEnv("1"));
    CHECK_FALSE(CoopSharedScenes::OnFromEnv("0"));
}

TEST_CASE("in a dungeon a story cutscene started in another room is not played here") {
    // 12 m apart, but through a wall: the starter's room is not loaded in our
    // game, and the cutscene would show a black picture until it ends.
    Joiner j;
    Here h = Free(120, {15.0f, 0.0f, 22.0f}, "ForestTemple");
    h.dungeon = true;
    h.room = 4;
    j.Started(1, "Dm_ForestTemple_0010", "ForestTemple", Vec3{15.0f, 0.0f, 10.0f}, 100, 7);
    CHECK_FALSE(j.Next(h, kNotPlayed));
    CHECK(j.Waiting() == 0);  // skipped, not kept waiting
    const auto notes = j.TakeNotes();
    REQUIRE(notes.size() == 1);
    CHECK(notes[0].find("another room of this dungeon") != std::string::npos);
    // The same room: played.
    j.Started(1, "Dm_ForestTemple_0011", "ForestTemple", Vec3{15.0f, 0.0f, 10.0f}, 200, 4);
    const auto got = j.Next(h, kNotPlayed);
    REQUIRE(got);
    CHECK(*got == "Dm_ForestTemple_0011");
}

TEST_CASE("in a dungeon with no room known only a very near starter is joined") {
    Joiner j;
    Here h = Free(120, {15.0f, 0.0f, 22.0f}, "ForestTemple");
    h.dungeon = true;  // no room: no table
    j.Started(1, "Dm_ForestTemple_0010", "ForestTemple", Vec3{15.0f, 0.0f, 10.0f}, 100);  // 12 m
    CHECK_FALSE(j.Next(h, kNotPlayed));
    CHECK(j.TakeNotes().at(0).find("room in this dungeon is not known") != std::string::npos);
    j.Started(1, "Dm_ForestTemple_0011", "ForestTemple", Vec3{15.0f, 0.0f, 17.0f}, 100);  // 5 m
    CHECK(j.Next(h, kNotPlayed) == std::optional<std::string>("Dm_ForestTemple_0011"));
    // Our room known, theirs not (or the other way): the same short reach.
    h.room = 4;
    j.Started(1, "Dm_ForestTemple_0012", "ForestTemple", Vec3{15.0f, 0.0f, 10.0f}, 100);
    CHECK_FALSE(j.Next(h, kNotPlayed));
}

TEST_CASE("outside a dungeon rooms do not count: the 30 m rule alone") {
    Joiner j;
    Here h = Free(120);  // KokiriForest, not a dungeon
    h.room = 1;          // ignored
    j.Started(1, "Dm_Kokiri_0030", "KokiriForest", kBridge, 100, 2);
    CHECK(j.Next(h, kNotPlayed) == std::optional<std::string>("Dm_Kokiri_0030"));
}

TEST_CASE("intro-ocarina: Saria's gift is never started in a game still in the opening") {
    // A player's report (0.10.1, Longshot, hosting): a rupee taken in the intro gave the Fairy Ocarina and froze in
    // the cutscene: Dm_Kokiri_0150 (Saria on the bridge) asked in a game where Saria is not at the bridge yet.
    constexpr std::uint32_t kDekuTreeLast = 0xebaec07fu;  // HavePlayedEvent.Dm_Kokiri_0140
    constexpr std::uint32_t kExitBlocker = 0xe5dc5674u;   // KokiriForest_Npc_Kokiri_007_Child_Blocker_00
    std::optional<bool> deku_tree = false, blocker = true;
    const auto flag = [&](std::uint32_t h) -> std::optional<bool> {
        if (h == kDekuTreeLast) return deku_tree;
        if (h == kExitBlocker) return blocker;
        return std::nullopt;
    };
    CHECK(coop::protocol::flag_hash("HavePlayedEvent.Dm_Kokiri_0140") == kDekuTreeLast);
    CHECK(coop::protocol::flag_hash("KokiriForest_Npc_Kokiri_007_Child_Blocker_00") == kExitBlocker);
    CHECK(CoopSharedScenes::Needs("Dm_Kokiri_0150").size() == 2);
    for (const auto& n : CoopSharedScenes::Needs("Dm_Kokiri_0150")) {
        CHECK_MESSAGE(coop::protocol::flag_hash(n.what) == n.hash, n.what);
    }
    CHECK(CoopSharedScenes::Needs("Dm_Kokiri_0030").empty());
    CHECK_FALSE(CoopSharedScenes::GateHolds("Dm_Kokiri_0150", flag));
    CHECK(CoopSharedScenes::GateHolds("Dm_Kokiri_0030", flag));  // no gate

    // The opening: another player's (wrongly found) Saria's gift is skipped, not waited for.
    Joiner j;
    j.Started(1, "Dm_Kokiri_0150", "KokiriForest", kBridge, 100);
    CHECK_FALSE(j.Next(Free(120), kNotPlayed, flag));
    CHECK(j.Waiting() == 0);
    CHECK(j.TakeNotes().back().find("our story is not there yet") != std::string::npos);
    // Unreadable flags hold nothing either.
    deku_tree.reset();
    blocker.reset();
    CHECK_FALSE(CoopSharedScenes::GateHolds("Dm_Kokiri_0150", flag));

    // After the Deku Tree's last scene: played here too (a new visit of the forest).
    deku_tree = true;
    Joiner after;
    after.Started(1, "Dm_Kokiri_0150", "KokiriForest", kBridge, 100);
    const auto got = after.Next(Free(120), kNotPlayed, flag);
    REQUIRE(got);
    CHECK(*got == "Dm_Kokiri_0150");
    // Or the exit blocker gone (an open forest): it holds as well.
    deku_tree = false;
    blocker = false;
    CHECK(CoopSharedScenes::GateHolds("Dm_Kokiri_0150", flag));
    // Other scenes are not gated.
    blocker = true;
    Joiner other;
    other.Started(1, "Dm_Kokiri_0050", "KokiriForest", kBridge, 100);
    CHECK(other.Next(Free(120), kNotPlayed, flag) == std::optional<std::string>("Dm_Kokiri_0050"));
}

TEST_CASE("a story cutscene asked by an entry of its flow counts as that story cutscene (player report 110)") {
    using CoopSharedScenes::RequestFor;
    using CoopSharedScenes::StoryOfRequest;
    // The Deku Tree's meeting, as the EventRequestActor in KokiriForest_Child asks it.
    constexpr std::string_view kEntry = "KokiriForest_Meet_DekuTree_MeetDekuTree_01";
    CHECK(StoryOfRequest(kEntry) == std::optional<std::string_view>("Meet_DekuTree"));
    CHECK(StoryOfRequest("Meet_DekuTree") == std::optional<std::string_view>("Meet_DekuTree"));
    CHECK(StoryOfRequest("Get_StoneOfFire_MeetDarnia") == std::optional<std::string_view>("Get_StoneOfFire"));
    CHECK_FALSE(StoryOfRequest("Npc_Kokiri_Mido_Child").has_value());          // a talk
    CHECK_FALSE(StoryOfRequest("GoronCity_SecretHole_AppearTreasureBox").has_value());  // a gimmick
    // A game that joins asks as the starter's did: the placed entry of that scene, else the flow's name.
    CHECK(RequestFor("Meet_DekuTree", "KokiriForest_Child") == kEntry);
    CHECK(RequestFor("Meet_DekuTree", "LostWoods") == "Meet_DekuTree");
    CHECK(RequestFor("Dm_Kokiri_0070", "KokiriForest_Child") == "Dm_Kokiri_0070");
    for (const auto& r : CoopSharedScenes::StoryRequests()) {
        CHECK_MESSAGE(CoopSharedScenes::IsShared(r.flow), std::string(r.flow));
        CHECK(StoryOfRequest(r.request) == std::optional<std::string_view>(r.flow));
        CHECK(RequestFor(r.flow, r.scene) == r.request);
    }

    // The guard (intro-ocarina) with the game's request as it came, and as the hook now maps it.
    auto request = [](std::string_view name) {
        CoopLiveEvents::GameRequest r;
        std::copy(name.begin(), name.end(), r.name);
        r.taken = true;
        return r;
    };
    const auto names_span = CoopSharedScenes::Names();
    const std::vector<std::string_view> names(names_span.begin(), names_span.end());
    const std::vector<CoopLiveEvents::GameRequest> as_asked{request(kEntry)};
    CHECK(CoopLiveEvents::RequestedOtherOf(as_asked, names));     // before: "another event", its search refused
    CHECK_FALSE(CoopLiveEvents::RequestedOf(as_asked, names).has_value());
    const std::vector<CoopLiveEvents::GameRequest> mapped{request(*StoryOfRequest(kEntry))};
    CHECK_FALSE(CoopLiveEvents::RequestedOtherOf(mapped, names));
    CHECK(CoopLiveEvents::RequestedOf(mapped, names) == std::optional<std::string>("Meet_DekuTree"));

    // Through the start source: given once, by the request, while the search keeps finding it.
    CoopLiveEvents::StartSource source;
    source.Scene("KokiriForest_Child");
    const auto find = [] { return std::optional<std::string>("Meet_DekuTree"); };
    CHECK(source.Frame(CoopLiveEvents::RequestedOf(mapped, names), CoopLiveEvents::RequestedOtherOf(mapped, names),
                       true, true, find) == std::optional<std::string>("Meet_DekuTree"));
    CHECK_FALSE(source.Frame(std::nullopt, false, true, true, find).has_value());
    CHECK(source.Refused() == 0);
}

TEST_CASE("the played check of a story cutscene asked by an entry reads that entry's flag (review of report 110)") {
    using CoopSharedScenes::PlayedFlagsFor;
    // The game has HavePlayedEvent per entry asked, none for these flow names (GameDataList.Product.110):
    // the hashes below are the entries' flags found there; HavePlayedEvent.Meet_DekuTree is not in it.
    struct Row {
        std::string_view scene, flow;
        std::uint32_t hash;
    };
    constexpr Row kRows[] = {
        {"KokiriForest_Child", "Meet_DekuTree", 0xef98d829u},
        {"GoronCity_Child", "Get_StoneOfFire", 0x507d99c9u},
        {"DeathMountainTrail_Child", "Get_StoneOfFire", 0x516ed011u},
        {"InsideJabuJabuBelly", "Get_StoneOfWater", 0x64d3877fu},
        {"ZoraFountain_Child", "Get_StoneOfWater", 0xfb7c9b2cu},
    };
    CHECK(CoopSharedScenes::StoryRequests().size() == std::size(kRows));
    for (const Row& r : kRows) {
        const auto flags = PlayedFlagsFor(r.flow, r.scene);
        REQUIRE(flags.size() == 2);
        CHECK_MESSAGE(coop::host::flag_hash(flags[0]) == r.hash, flags[0]);
        CHECK(flags[0] == "HavePlayedEvent." + std::string(CoopSharedScenes::RequestFor(r.flow, r.scene)));
        CHECK(flags[1] == "HavePlayedEvent." + std::string(r.flow));
    }
    CHECK(PlayedFlagsFor("Meet_DekuTree", "KokiriForest_Child").front() ==
          "HavePlayedEvent.KokiriForest_Meet_DekuTree_MeetDekuTree_01");
    // A story name asked by itself keeps its one flag (the story order's Dm_Kokiri_0140 is in the game).
    CHECK(PlayedFlagsFor("Dm_Kokiri_0140", "KokiriForest_Child") ==
          std::vector<std::string>{"HavePlayedEvent.Dm_Kokiri_0140"});
    CHECK(coop::host::flag_hash("HavePlayedEvent.Dm_Kokiri_0140") == 0xebaec07fu);
    CHECK(PlayedFlagsFor("Meet_DekuTree", "LostWoods") == std::vector<std::string>{"HavePlayedEvent.Meet_DekuTree"});

    // Played, read as the hook reads it: a save's flags by name, nothing for a flag the game does not have.
    const std::string kEntryFlag = "HavePlayedEvent.KokiriForest_Meet_DekuTree_MeetDekuTree_01";
    const auto game = [&](bool met) {
        return [&, met](const std::string& f) -> std::optional<bool> {
            if (f == kEntryFlag) {
                return met;
            }
            if (f == "HavePlayedEvent.Dm_Kokiri_0140") {
                return false;
            }
            return std::nullopt;  // HavePlayedEvent.Meet_DekuTree, .Get_StoneOfForest, .Meet_GreatFairy: not in the game
        };
    };
    using CoopSharedScenes::Played;
    CHECK(Played("Meet_DekuTree", "KokiriForest_Child", game(true)));
    CHECK_FALSE(Played("Meet_DekuTree", "KokiriForest_Child", game(false)));
    CHECK_FALSE(Played("Dm_Kokiri_0140", "KokiriForest_Child", game(false)));
    // No flag the game has (quests asked by their own name): counted as played, never replayed.
    CHECK(Played("Get_StoneOfForest", "InsideTheDekuTree", game(false)));
    CHECK(Played("Meet_GreatFairy", "FairyFountain", game(false)));
    CHECK(Played("Meet_DekuTree", "LostWoods", game(false)));

    // Through the joiner: B has met the Deku Tree (only its entry flag set, as the game keeps it); A starts
    // the meeting near B: B is not asked to play it again. A B that has not met it is.
    const auto in_forest = [&](bool met) {
        return [&, met](const std::string& n) { return Played(n, "KokiriForest_Child", game(met)); };
    };
    CoopSharedScenes::Joiner seen;
    seen.Started(1, "Meet_DekuTree", "KokiriForest_Child", kBridge, 100);
    CHECK_FALSE(seen.Next(Free(120, {15.0f, 0.0f, 22.0f}, "KokiriForest_Child"), in_forest(true)).has_value());
    CoopSharedScenes::Joiner fresh;
    fresh.Started(1, "Meet_DekuTree", "KokiriForest_Child", kBridge, 100);
    CHECK(fresh.Next(Free(120, {15.0f, 0.0f, 22.0f}, "KokiriForest_Child"), in_forest(false)) ==
          std::optional<std::string>("Meet_DekuTree"));
}
