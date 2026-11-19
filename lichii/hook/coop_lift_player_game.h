// SPDX-License-Identifier: GPL-3.0-or-later
//
// Native puppets phase 3: lifting another player, the hook's side (coop_lift_player.h has the rules;
// docs/native-puppets.md "Lifting another player"). Off unless LICHII_COOP_LIFT_PLAYER=1: off, nothing here
// runs, nothing is sent, and the director, the object walk and the input check are as before.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "coop/host/player_reader.hpp"

namespace coop::host {
class Session;
}
namespace coop::net {
class Client;
struct Event;
}

namespace CoopLiftPlayerGame {

// Any thread: the switch (LICHII_COOP_LIFT_PLAYER=1, read once).
bool On();

// Network thread, from the session's on_event (the network lock is held): LIFT_ASK, LIFT_ANSWER and LIFT_PLACE.
void OnEvent(const coop::net::Event& event, coop::net::Client& client);

// Present thread, once per frame, after the puppets and the object walk. `session` null when not connected.
void Frame(const std::optional<std::string>& scene, const std::optional<coop::host::PlayerReading>& reading,
           coop::host::Session* session, std::uint64_t server_now_us);

// Any thread (the input-off wrap): another player's Link holds ours, so our Link's own input check answers
// "off" (the game's check main+0x3025850, wrap 18).
bool OurInputOff();

} // namespace CoopLiftPlayerGame
