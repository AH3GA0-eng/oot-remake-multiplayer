// SPDX-License-Identifier: GPL-3.0-or-later
//
// PvP on real bodies, the hook's side (coop_pvp_native_game.h).

#include "coop_pvp_native_game.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

#include "common/logging.h"
#include "coop/host/session.hpp"
#include "coop/net/client.hpp"
#include "coop_puppet.h"
#include "coop_pvp_game.h"
#include "coop_pvp_native.h"

namespace CoopPvpNativeGame {
namespace {

namespace proto = coop::protocol;

constexpr std::uint64_t kNativeEveryUs = 5'000'000;  // PVP_NATIVE again (docs/protocol.md)

std::uint64_t SteadyUs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct Glue {
    std::mutex mutex;
    bool say_now = true;            // a new connection: say PVP_NATIVE at once
    std::uint32_t native_peers = 0; // bit n: player n said PVP_NATIVE 1 (network thread, under mutex)
    std::uint64_t strikes_in = 0;   // logged only, until the strike glue lands
    std::uint64_t results_in = 0;
    std::atomic<bool> pack_pvp{false};
    std::uint64_t say_at_us = 0;    // present thread
    std::uint32_t wants_logged = 0; // present thread
};

Glue& TheGlue() {
    static Glue g;
    return g;
}

std::uint32_t Bit(std::uint8_t number) {
    return number >= 1 && number <= proto::kMaxPlayers ? 1u << number : 0u;
}

} // namespace

bool On() {
    static const bool on = [] {
        const char* v = std::getenv("LICHII_COOP_PVP_NATIVE");
        return v != nullptr && std::strcmp(v, "1") == 0;
    }();
    return on;
}

void SetPackPvp(bool on) {
    TheGlue().pack_pvp.store(on, std::memory_order_relaxed);
    if (On()) {
        LOG_INFO(Core, "coop: pvp native: on; the puppet pack {} _Pvp copies{}", on ? "has" : "has no",
                 on ? "" : " (rebuild it with --puppet-pvp): today's PvP is kept");
    }
}

bool Wants(std::uint8_t number) {
    return On() && (CoopPuppet::PvpCopies() & Bit(number)) != 0;
}

void OnEvent(const coop::net::Event& event, coop::net::Client& /*client*/) {
    if (!On()) {
        return;
    }
    using coop::net::EventKind;
    Glue& g = TheGlue();
    std::lock_guard lock(g.mutex);
    if (event.kind == EventKind::welcomed || event.kind == EventKind::reconnected ||
        event.kind == EventKind::disconnected) {
        g.native_peers = 0;
        g.say_now = true;
        return;
    }
    if (event.kind != EventKind::message) {
        return;
    }
    if (const auto* n = std::get_if<proto::PvpNative>(&event.message)) {
        g.native_peers = n->on ? g.native_peers | Bit(n->player_number) : g.native_peers & ~Bit(n->player_number);
    } else if (const auto* left = std::get_if<proto::PlayerLeft>(&event.message)) {
        // A player who comes back says PVP_NATIVE again; until then its copy is the plain one.
        g.native_peers &= ~Bit(left->player_number);
    } else if (const auto* s = std::get_if<proto::PvpStrike>(&event.message)) {
        ++g.strikes_in;
        LOG_INFO(Core, "coop: pvp native: strike {} from P{} (source {}): not taken yet (path B waits for U5)",
                 s->seq, s->player_number, static_cast<int>(s->source));
    } else if (const auto* r = std::get_if<proto::PvpResult>(&event.message)) {
        ++g.results_in;
        LOG_INFO(Core, "coop: pvp native: P{} answered P{}'s strike {}: outcome {}", r->player_number,
                 r->attacker, r->seq, static_cast<int>(r->outcome));
    }
}

void Frame(const std::optional<std::string>& /*scene*/, const std::optional<coop::host::PlayerReading>& /*reading*/,
           coop::host::Session* session, std::uint64_t /*server_now_us*/) {
    if (!On()) {
        return;
    }
    Glue& g = TheGlue();
    const bool pack_pvp = g.pack_pvp.load(std::memory_order_relaxed);
    if (session == nullptr) {
        CoopPuppet::SetPvpCopies(0);
        return;
    }
    std::uint32_t peers = 0;
    bool say_now = false;
    {
        std::lock_guard lock(g.mutex);
        peers = g.native_peers;
        say_now = std::exchange(g.say_now, false);
    }
    // Which copies are the _Pvp actor: PvP on (the session's value), our glue and pack, their PVP_NATIVE.
    const bool pvp_on = CoopPvp::On();
    std::uint32_t wants = 0;
    for (std::uint8_t n = 1; n <= proto::kMaxPlayers; ++n) {
        const CoopPvpNative::CopyMode mode{pvp_on, true, pack_pvp, (peers & Bit(n)) != 0};
        if (CoopPvpNative::WantedCopy(mode) == CoopPvpNative::CopyActor::pvp) wants |= Bit(n);
    }
    CoopPuppet::SetPvpCopies(wants);  // the director makes those copies the _Pvp actor
    if (wants != g.wants_logged) {
        LOG_INFO(Core, "coop: pvp native: _Pvp copies for players {:#x} (PvP {})", wants >> 1, pvp_on ? "on" : "off");
        g.wants_logged = wants;
    }

    const std::uint64_t now_us = SteadyUs();
    if (!say_now && now_us - g.say_at_us < kNativeEveryUs) {
        return;
    }
    g.say_at_us = now_us;
    const proto::PvpNative native{proto::kPlayerSelf, CoopPvpNative::SayNative(true, pack_pvp)};
    session->with_client([&](coop::net::Client& client) { client.send(native); });
}

} // namespace CoopPvpNativeGame
