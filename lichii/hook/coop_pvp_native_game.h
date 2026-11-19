// SPDX-License-Identifier: GPL-3.0-or-later
//
// PvP on real bodies, the hook's side (coop_pvp_native.h has the rules;
// docs/native-puppets.md section 5). Off unless LICHII_COOP_PVP_NATIVE=1.
//
// Built so far: our PVP_NATIVE (glue on and a pvp pack), the other games'
// PVP_NATIVE, and which players' copies are the _Pvp actor (Wants: PvP on,
// the glue on, a pvp pack, that player native). The director asks Wants for
// each copy and keeps a lifted or holding copy as it is (CoopPvpNative::
// RemakeNow). Strikes and results that reach us are logged and counted only:
// reading the copy's hit record, sending strikes, path B on our Link and the
// third-game replay wait for the reads U2/U3/U5 (docs 5.8, 5.10).
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

namespace CoopPvpNativeGame {

// LICHII_COOP_PVP_NATIVE=1, read once.
bool On();
// Once, at configure time: the puppet pack's manifests say "pvp": true.
void SetPackPvp(bool on);
// Network thread, from the session's on_event (the network lock is held).
void OnEvent(const coop::net::Event& event, coop::net::Client& client);
// Present thread, once per frame. `session` null when not connected.
void Frame(const std::optional<std::string>& scene, const std::optional<coop::host::PlayerReading>& reading,
           coop::host::Session* session, std::uint64_t server_now_us);
// Any thread: player `number`'s copy is to be the _Pvp actor now.
bool Wants(std::uint8_t number);

} // namespace CoopPvpNativeGame
