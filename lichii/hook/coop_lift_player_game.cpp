// SPDX-License-Identifier: GPL-3.0-or-later
//
// Native puppets phase 3: lifting another player, the hook's side (coop_lift_player.h has the rules).
//
// Lifter: the director keeps where the game had each copy (CoopPuppet::WatchGamePlaces); Lifter sees the game's
// own lift on a copy and asks; from then on the director leaves that copy to the game (CoopPuppet::SetGameHolds)
// and its places go to the lifted game. Lifted: Victim decides; while lifted our Link is put on the lifter's
// places by the object walk with the game's own setter (CoopObjects::PlaceOurLink) and its input check answers
// "off" (OurInputOff, the input-off wrap).

#include "coop_lift_player_game.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

#include "common/logging.h"
#include "coop/host/event_owner.hpp"
#include "coop/host/session.hpp"
#include "coop/net/client.hpp"
#include "coop_lift_player.h"
#include "coop_objects.h"
#include "coop_puppet.h"

namespace CoopLiftPlayerGame {
namespace {

namespace proto = coop::protocol;
using namespace CoopLiftPlayer;

constexpr std::uint64_t kShareEveryUs = 5'000'000;

std::uint64_t SteadyUs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

const char* RefusalText(proto::LiftRefusal r) {
    switch (r) {
    case proto::LiftRefusal::none: return "yes";
    case proto::LiftRefusal::busy: return "busy (lifted already, or lifting)";
    case proto::LiftRefusal::event: return "in an event, a load or a menu";
    case proto::LiftRefusal::down: return "down";
    case proto::LiftRefusal::not_sharing: return "their game does not share lifts";
    case proto::LiftRefusal::freed: return "freed";
    case proto::LiftRefusal::too_far: return "too far";
    }
    return "?";
}

const char* PhaseText(proto::LiftPhase p) {
    switch (p) {
    case proto::LiftPhase::held: return "held";
    case proto::LiftPhase::flying: return "flying";
    case proto::LiftPhase::landed: return "landed";
    }
    return "?";
}

struct Glue {
    std::mutex mutex;
    std::deque<proto::LiftAsk> asks;        // network thread -> present thread
    std::deque<proto::LiftAnswer> answers;
    std::deque<proto::LiftPlace> places;
    bool share_now = true;                  // a new connection: say LIFT_SHARE at once
    std::atomic<bool> input_off{false};
    Lifter lifter;                          // present thread
    Victim victim;                          // present thread
    std::uint64_t share_at_us = 0;          // present thread
    proto::LiftPhase last_phase = proto::LiftPhase::landed;
};

Glue& TheGlue() {
    static Glue g;
    return g;
}

} // namespace

bool On() {
    static const bool on = [] {
        const char* v = std::getenv("LICHII_COOP_LIFT_PLAYER");
        return v != nullptr && std::strcmp(v, "1") == 0;
    }();
    return on;
}

bool OurInputOff() {
    return On() && TheGlue().input_off.load(std::memory_order_relaxed);
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
        g.asks.clear();
        g.answers.clear();
        g.places.clear();
        g.share_now = true;
        return;
    }
    if (event.kind != EventKind::message) {
        return;
    }
    if (const auto* a = std::get_if<proto::LiftAsk>(&event.message)) {
        if (g.asks.size() < 8) g.asks.push_back(*a);
    } else if (const auto* b = std::get_if<proto::LiftAnswer>(&event.message)) {
        if (g.answers.size() < 8) g.answers.push_back(*b);
    } else if (const auto* p = std::get_if<proto::LiftPlace>(&event.message)) {
        if (g.places.size() < 64) g.places.push_back(*p);
    }
}

void Frame(const std::optional<std::string>& scene, const std::optional<coop::host::PlayerReading>& reading,
           coop::host::Session* session, std::uint64_t server_now_us) {
    if (!On()) {
        return;
    }
    Glue& g = TheGlue();
    CoopPuppet::WatchGamePlaces(true);
    std::deque<proto::LiftAsk> asks;
    std::deque<proto::LiftAnswer> answers;
    std::deque<proto::LiftPlace> places;
    bool share_now = false;
    {
        std::lock_guard lock(g.mutex);
        asks.swap(g.asks);
        answers.swap(g.answers);
        places.swap(g.places);
        share_now = std::exchange(g.share_now, false);
    }
    if (session == nullptr) {
        if (g.lifter.Held() != 0 || g.victim.By() != 0) {
            LOG_INFO(Core, "coop: lift: not connected: the lift ends");
        }
        g.lifter.Reset();
        g.victim.Reset();
        CoopPuppet::SetGameHolds(0);
        g.input_off.store(false);
        return;
    }
    const std::uint64_t now_us = SteadyUs();

    std::uint8_t self = 0;
    std::uint8_t status = 0;
    session->with_client([&](coop::net::Client& client) {
        self = client.player_number();
        if (const auto* me = client.find_player(self)) {
            status = me->status.value_or(0);
        }
    });

    std::vector<proto::Message> send;
    if (share_now || now_us - g.share_at_us >= kShareEveryUs) {
        g.share_at_us = now_us;
        send.emplace_back(proto::LiftShare{true});
    }

    // Lifter: our Link and the copies the director drew.
    for (const auto& a : answers) {
        const std::uint8_t was = g.lifter.Held();
        g.lifter.Answer(a, now_us);
        if (was == a.player_number) {
            LOG_INFO(Core, "coop: lift: P{} answered our lift: {}", a.player_number, RefusalText(a.refusal));
        }
    }
    {
        std::vector<CopyView> copies;
        if (reading && scene) {
            for (const auto& p : CoopPuppet::GamePlaces()) {
                copies.push_back(CopyView{p.number, p.drawn, p.game});
            }
        }
        const Vec3 our_link = reading ? Vec3{reading->position[0], reading->position[1], reading->position[2]}
                                      : Vec3{};
        const auto out = g.lifter.Frame(our_link, copies, now_us);
        if (out.ask) {
            LOG_INFO(Core, "coop: lift: our Link lifted P{}'s copy: asking", out.ask->lifted);
            send.emplace_back(*out.ask);
        }
        if (out.place) {
            if (out.place->phase != g.last_phase) {
                LOG_INFO(Core, "coop: lift: P{} {} at {:.2f} {:.2f} {:.2f}", out.place->lifted,
                         PhaseText(out.place->phase), out.place->position[0], out.place->position[1],
                         out.place->position[2]);
                g.last_phase = out.place->phase;
            }
            send.emplace_back(*out.place);
        }
        if (CoopPuppet::GameHolds() != g.lifter.Held()) {
            LOG_INFO(Core, "coop: lift: the director {} P{}'s copy",
                     g.lifter.Held() != 0 ? "leaves to the game" : "places again",
                     g.lifter.Held() != 0 ? g.lifter.Held() : CoopPuppet::GameHolds());
            CoopPuppet::SetGameHolds(g.lifter.Held());
        }
    }

    // Lifted: our game decides.
    Local local;
    local.self = self;
    local.event = (reading && coop::host::our_link_in_event(reading->event_running.value_or(false),
                                                             reading->event_whose)) ||
                  (status & (proto::status::away & ~proto::status::dead)) != 0;
    const auto life = CoopObjects::OurLinkLife();
    local.down = (status & proto::status::dead) != 0 || (life && *life <= 0);
    local.lifting = g.lifter.Held() != 0;
    if (reading) {
        local.our_link = Vec3{reading->position[0], reading->position[1], reading->position[2]};
    }
    if (!asks.empty()) {
        const auto remotes = CoopPuppet::Collect(session, server_now_us);
        for (const auto& a : asks) {
            std::optional<Vec3> lifter_at;
            for (const auto& r : remotes) {
                if (r.number == a.player_number && r.scene == scene) {
                    lifter_at = Vec3{r.position[0], r.position[1], r.position[2]};
                }
            }
            const auto answer = g.victim.Ask(a, local, lifter_at, now_us);
            LOG_INFO(Core, "coop: lift: P{} lifts our Link: {}", a.player_number, RefusalText(answer.refusal));
            send.emplace_back(answer);
        }
    }
    for (const auto& p : places) {
        g.victim.Place(p, now_us);
    }
    const std::uint8_t by = g.victim.By();
    const auto step = g.victim.Frame(local, now_us);
    if (step.put) {
        CoopObjects::PlaceOurLink(*step.put);
    }
    if (g.input_off.exchange(step.input_off) != step.input_off) {
        LOG_INFO(Core, "coop: lift: our Link's input {} (held by P{})", step.input_off ? "off" : "back", by);
    }
    if (step.freed) {
        LOG_INFO(Core, "coop: lift: our Link freed itself from P{}", step.freed->lifter);
        send.emplace_back(*step.freed);
    }

    if (send.empty()) {
        return;
    }
    session->with_client([&](coop::net::Client& client) {
        for (const auto& m : send) {
            client.send(m);
        }
    });
}

} // namespace CoopLiftPlayerGame
