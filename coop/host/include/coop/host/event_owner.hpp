// Whose event runs in our game (G1: issues 251, 243, 200, 144, 86; docs/scenes.md "Whose event").
//
// One rule for every site that holds, hides, freezes or hands over something
// because "an event runs". Before, all of them read one bit, "an event runs in
// our game" (PlayerReading::event_running), and so treated three different
// events the same:
//   ours    - an event our own Link is in: a talk, a cutscene our game started,
//             a shared story scene that binds our Link.
//   others  - another player's event that our co-op layer asked our game to
//             play (a live dungeon event, an eye switch's event, a shared
//             scene joined), and that does not bind our Link: our Link is not
//             in it.
//   ocarina - our Link playing the Ocarina freely (the game's event setting
//             Dm_SY_Ocarina / PlayOcarina_Base, read from RSDB EventSetting):
//             an event in the game's terms, but nothing is staged and our Link
//             stands in the open.
// What each kind means for a site:
//   - Hides the other players' puppets, tells the others "in a cutscene"
//     (their games hold our puppet), holds shared changes near our Link,
//     holds Deku Scrub flags and hits, claims townspeople and guessed actors:
//     only `ours` (our_link_in_event).
//   - The actors the running event binds in OUR game act for it here (no
//     owner's motion on them, their changes wait): any kind, since the event
//     in our game waits on them whoever asked for it (event_in_our_game).
//     Only `ours` also claims them from their owner (hands them over): a
//     replay of another player's event leaves them with that player.
//   - "The game is busy" (no second event asked, no quest catch-up, no time
//     nudge, no revive): any kind (event_in_our_game).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "coop/host/scene.hpp"

namespace coop::host {

enum class EventWhose : std::uint8_t { ours, others, ocarina };

// The game's own event settings for the Ocarina played freely (RSDB EventSetting, flow Dm_SY_Ocarina,
// base PlayerPos). A song's reaction (BlewOcarinaByPlayer, a warp's Dm_SY_Ocarina_move, a gimmick's
// *_Ocarina) is another setting: a real event, ours.
inline bool free_ocarina_event(std::string_view setting) {
  return setting == "Dm_SY_Ocarina" || setting == "PlayOcarina_Base";
}

inline const char* whose_name(EventWhose w) {
  switch (w) {
    case EventWhose::ours: return "ours";
    case EventWhose::others: return "another player's";
    case EventWhose::ocarina: return "our free Ocarina";
  }
  return "?";
}

// Our Link is in the event that runs: the hold/hide/tell sites.
inline bool our_link_in_event(bool event_running, EventWhose whose) {
  return event_running && whose == EventWhose::ours;
}

// The game runs an event of any kind: the "game busy" sites and the event's own actors here.
inline bool event_in_our_game(bool event_running) { return event_running; }

// local_event_hides with the owner: only our own event hides the others (not another player's
// replayed here, not the Ocarina by its event or by our Link's animation).
inline bool local_event_hides(bool event_running, EventWhose whose, std::optional<std::string_view> own_anim) {
  return our_link_in_event(event_running, whose) && local_event_hides(true, own_anim);
}

// Once a frame (the hook's present thread): which kind the running event is.
// - CoopAsked: our co-op layer asked the game for another player's event and the
//   manager took it. An event that starts within kReplayUs after it (with no
//   request of the game's own in between) is that replay: `others`, unless our
//   Link's own performer is bound into it (then the event holds our Link: ours).
// - GameAsked: the game's own request the manager took (Hookshot's request wrap,
//   CoopLiveEvents::GameRequests). The free Ocarina's setting makes the run
//   `ocarina` until another request is taken in it (a song's reaction, a warp).
//   Our own replay seen again through the wrap (the same name) changes nothing.
// - Frame: `running` is PlayerReading::event_running, `link_bound` our Link's
//   own performer bound, `own_anim` our Link's main AS command (the Ocarina's
//   animation counts as the Ocarina where the wrap gives no name: the emulator).
//   A run is classified when it starts and stays so until it ends.
// Pure: tested in tests/host/event_owner_test.cpp.
class EventOwnerTracker {
 public:
  static constexpr std::uint64_t kReplayUs = 3'000'000;

  void CoopAsked(std::string_view name, std::uint64_t now_us) {
    coop_name_ = std::string(name);
    coop_us_ = now_us;
    coop_set_ = true;
  }

  void GameAsked(std::string_view name, std::uint64_t now_us) {
    if (coop_set_ && name == coop_name_) return;  // our replay, seen by the wrap
    coop_set_ = false;                            // the game's own request after ours: the next run is ours
    game_ocarina_ = free_ocarina_event(name);
    game_us_ = now_us;
    game_set_ = true;
    if (running_) {
      ocarina_ = game_ocarina_;  // a song's reaction ends the free play; a new free play starts one
      replay_ = false;
    }
  }

  EventWhose Frame(bool running, bool link_bound, std::uint64_t now_us,
                   std::optional<std::string_view> own_anim = std::nullopt) {
    if (!running) {
      running_ = false;
      replay_ = false;
      ocarina_ = false;
      if (coop_set_ && now_us - coop_us_ > kReplayUs) coop_set_ = false;
      if (game_set_ && now_us - game_us_ > kReplayUs) game_set_ = false;
      return EventWhose::ours;
    }
    if (!running_) {  // the run starts
      running_ = true;
      replay_ = coop_set_ && now_us - coop_us_ <= kReplayUs;
      ocarina_ = !replay_ && game_set_ && game_ocarina_ && now_us - game_us_ <= kReplayUs;
      coop_set_ = false;
      game_set_ = false;
    }
    if (ocarina_ || activity_of(own_anim) == Activity::ocarina) return EventWhose::ocarina;
    if (replay_ && !link_bound) return EventWhose::others;
    return EventWhose::ours;
  }

  bool Replay() const { return running_ && replay_; }

 private:
  std::string coop_name_;
  std::uint64_t coop_us_ = 0;
  bool coop_set_ = false;
  bool game_ocarina_ = false;
  std::uint64_t game_us_ = 0;
  bool game_set_ = false;
  bool running_ = false;
  bool replay_ = false;
  bool ocarina_ = false;
};

}  // namespace coop::host
