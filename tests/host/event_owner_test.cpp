#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "coop/host/event_owner.hpp"
#include "coop/host/sampler.hpp"
#include "coop/host/scene.hpp"
#include "coop/protocol/codec.hpp"

using namespace coop::host;
namespace proto = coop::protocol;

namespace {

constexpr std::uint64_t kMs = 1'000;
constexpr std::uint64_t kStart = 10'000'000;

PlayerReading reading(bool event, EventWhose whose) {
  PlayerReading r;
  r.position = {1.0f, 0.0f, 0.0f};
  r.rotation = Quat{0, 0, 0, 1};
  r.age = proto::Age::adult;
  r.scene = "spot00";
  r.event_running = event;
  r.event_whose = whose;
  return r;
}

// The PLAYER_STATUS flags the sampler sends for one reading (after a first, event-free frame).
std::uint8_t sent_flags(const PlayerReading& r) {
  Sampler s;
  s.frame(reading(false, EventWhose::ours), kStart);
  s.frame(r, kStart + 17 * kMs);
  return s.status_flags();
}

}  // namespace

TEST_CASE("event owner: no event, our own event, a request of the game's own") {
  EventOwnerTracker t;
  CHECK(t.Frame(false, false, kStart) == EventWhose::ours);
  CHECK(t.Frame(true, false, kStart + 16 * kMs) == EventWhose::ours);  // nobody marked it: our game's own
  CHECK_FALSE(t.Replay());
  t.Frame(false, false, kStart + 100 * kMs);
  t.GameAsked("Npc_Talk_Something", kStart + 200 * kMs);
  CHECK(t.Frame(true, true, kStart + 216 * kMs) == EventWhose::ours);
}

TEST_CASE("event owner: issue 251, another player's event replayed here is not ours") {
  EventOwnerTracker t;
  t.Frame(false, false, kStart);
  t.CoopAsked("InsideTheDekuTree_Room_Ladder", kStart + 10 * kMs);  // a held live event, taken
  CHECK(t.Frame(true, false, kStart + 26 * kMs) == EventWhose::others);
  CHECK(t.Replay());
  // The whole run stays another player's.
  CHECK(t.Frame(true, false, kStart + 2'000 * kMs) == EventWhose::others);
  // The event ends; the next one (our own talk) is ours.
  CHECK(t.Frame(false, false, kStart + 3'000 * kMs) == EventWhose::ours);
  CHECK(t.Frame(true, true, kStart + 4'000 * kMs) == EventWhose::ours);
  // The sites: the replay hides nobody, tells nobody "in a cutscene", holds nothing near our Link.
  CHECK_FALSE(our_link_in_event(true, EventWhose::others));
  CHECK(event_in_our_game(true));  // the game is busy all the same: no second event asked
  CHECK_FALSE(local_event_hides(true, EventWhose::others, std::string_view("Wait")));
  CHECK(local_event_hides(true, EventWhose::ours, std::string_view("EvtWait")));
  CHECK((sent_flags(reading(true, EventWhose::others)) & proto::status::in_cutscene) == 0);
  CHECK((sent_flags(reading(true, EventWhose::ours)) & proto::status::in_cutscene) != 0);
}

TEST_CASE("event owner: a replay that binds our Link holds it, so it is ours") {
  EventOwnerTracker t;
  t.CoopAsked("Dm_LostWood_0000", kStart);  // a shared story scene joined
  CHECK(t.Frame(true, false, kStart + 16 * kMs) == EventWhose::others);
  CHECK(t.Frame(true, true, kStart + 32 * kMs) == EventWhose::ours);  // our Link's performer is bound now
}

TEST_CASE("event owner: a mark too old, or a request of the game's own after it, is not a replay") {
  EventOwnerTracker t;
  t.CoopAsked("Room_Event", kStart);
  CHECK(t.Frame(true, false, kStart + EventOwnerTracker::kReplayUs + kMs) == EventWhose::ours);
  t.Frame(false, false, kStart + 5'000'000);
  t.CoopAsked("Room_Event", kStart + 6'000'000);
  t.GameAsked("Room_Event", kStart + 6'001'000);        // our own ask seen again by the wrap: still a replay
  t.GameAsked("Npc_Kokiri_Talk", kStart + 6'002'000);   // the game's own request after it: ours
  CHECK(t.Frame(true, false, kStart + 6'016'000) == EventWhose::ours);
}

TEST_CASE("event owner: issue 86, the free Ocarina's take-out holds and hides nobody") {
  EventOwnerTracker t;
  t.Frame(false, false, kStart);
  t.GameAsked("Dm_SY_Ocarina", kStart + 5 * kMs);  // the game's own setting for the free Ocarina
  // Its first ~4 s our Link's animation is a plain EvtWait; the event's name says it all the same.
  CHECK(t.Frame(true, true, kStart + 16 * kMs, std::string_view("EvtWait")) == EventWhose::ocarina);
  CHECK(t.Frame(true, true, kStart + 4'000 * kMs, std::string_view("DemoOcarina")) == EventWhose::ocarina);
  CHECK((sent_flags(reading(true, EventWhose::ocarina)) & proto::status::in_cutscene) == 0);
  CHECK_FALSE(local_event_hides(true, EventWhose::ocarina, std::string_view("EvtWait")));
  // A song's reaction is a real event: ours from its request on.
  t.GameAsked("Npc_Bonooru_BlewOcarinaByPlayer", kStart + 6'000 * kMs);
  CHECK(t.Frame(true, true, kStart + 6'016 * kMs, std::string_view("EvtWait")) == EventWhose::ours);
  // Without the wrap (the emulator): our Link's Ocarina animation alone, as before.
  EventOwnerTracker e;
  CHECK(e.Frame(true, true, kStart, std::string_view("DemoOcarina")) == EventWhose::ocarina);
  CHECK(e.Frame(true, true, kStart + 16 * kMs, std::string_view("DemoOcarinaMelodyGet_Saria")) == EventWhose::ours);
  CHECK(free_ocarina_event("PlayOcarina_Base"));
  CHECK_FALSE(free_ocarina_event("Dm_SY_Ocarina_move"));  // a warp song's travel: a cutscene
}

TEST_CASE("event owner: issues 144 and 243, the scrub holds follow only our Link's own event") {
  // The hook's scrub talk hold, sale hold and Scrub Brothers' hit hold take our_link_in_event:
  // a scrub's talk of ours holds; another player's dungeon event replayed here does not delay
  // another game's scrub flags or hits (they come at once, as without any event).
  CHECK(our_link_in_event(true, EventWhose::ours));
  CHECK_FALSE(our_link_in_event(true, EventWhose::others));
  CHECK_FALSE(our_link_in_event(true, EventWhose::ocarina));
  CHECK_FALSE(our_link_in_event(false, EventWhose::ours));
}

TEST_CASE("event owner: issue 200, only the game whose event it is hides others for it") {
  // Another player's event here: the other players stay drawn as usual (show), our own hides them.
  CHECK(visibility(std::string_view("spot00"), local_event_hides(true, EventWhose::others, std::nullopt),
                   std::string_view("spot00"), 0) == Visibility::show);
  CHECK(visibility(std::string_view("spot00"), local_event_hides(true, EventWhose::ours, std::nullopt),
                   std::string_view("spot00"), 0) == Visibility::hide);
}

TEST_CASE("event owner: the event's own name decides, not the time since the ask (review)") {
  using By = EventOwnerTracker::By;
  // A replay that starts late (beyond the window): the time guess says ours, its name says another player's.
  EventOwnerTracker t;
  t.CoopAsked("InsideTheDekuTree_Room_Ladder", kStart);
  CHECK(t.Frame(true, false, kStart + EventOwnerTracker::kReplayUs + 500 * kMs) == EventWhose::ours);
  t.RunNames({"Event_Common", "InsideTheDekuTree_Room_Ladder"}, kStart + EventOwnerTracker::kReplayUs + 520 * kMs);
  CHECK(t.Frame(true, false, kStart + EventOwnerTracker::kReplayUs + 532 * kMs) == EventWhose::others);
  CHECK(t.Decided() == By::name);
  t.Frame(false, false, kStart + 10'000'000);
  // Our own event inside the window: the time guess says a replay, its name (not ours asked) says ours.
  t.CoopAsked("InsideTheDekuTree_Room_Ladder", kStart + 11'000'000);
  CHECK(t.Frame(true, false, kStart + 11'500'000) == EventWhose::others);
  CHECK(t.Decided() == By::time);
  t.RunNames({"Npc_Kokiri_Talk"}, kStart + 11'520'000);
  CHECK(t.Frame(true, false, kStart + 11'532'000) == EventWhose::ours);
  CHECK(t.Decided() == By::name);
  t.Frame(false, false, kStart + 20'000'000);
  // The free Ocarina named by the event itself (the emulator: no request wrap), even in its EvtWait take-out.
  CHECK(t.Frame(true, true, kStart + 21'000'000, std::string_view("EvtWait")) == EventWhose::ours);
  t.RunNames({"Dm_SY_Ocarina"}, kStart + 21'020'000);
  CHECK(t.Frame(true, true, kStart + 21'032'000, std::string_view("EvtWait")) == EventWhose::ocarina);
  // No names: nothing changes.
  t.RunNames({}, kStart + 21'040'000);
  CHECK(t.Frame(true, true, kStart + 21'050'000, std::string_view("EvtWait")) == EventWhose::ocarina);
}
