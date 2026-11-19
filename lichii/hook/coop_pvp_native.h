// SPDX-License-Identifier: GPL-3.0-or-later
//
// PvP on real bodies (native puppets phase 4, docs/native-puppets.md section
// 5): the rules, with no game code in them. The attacker's game finds a hit on
// another player's copy with the game's own collision and sends PVP_STRIKE;
// the victim's game decides here whether its real Link takes it (5.6), then
// hands it to the game's own hit path (A, the sensor path, or B, the life
// replay behind a guard check) and answers PVP_RESULT.
//
// - Which copy actor: the plain CoopPuppet_* while PvP is off, the _Pvp one
//   only while PvP is on and both games are PVP_NATIVE with a pvp pack (5.0).
//   A copy that is lifted or holding a bomb is not re-made until that ends.
// - Which hit records are ours to send: our Link's, or a shot or blast our
//   game made (5.4). An enemy's hit on a copy never is.
// - The victim's checks: seq (newer per attacker, forgotten when the attacker
//   leaves, so one who comes back starts again), kept off, time, the victim's
//   own place ring, the attacker's place ring and reach by source.
// - Path B only: the amount from the victim's own tables and the mercy time.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "coop/protocol/messages.hpp"
#include "coop_pvp.h"

namespace CoopPvpNative {

namespace proto = coop::protocol;
using Vec3 = proto::Vec3;

// --- Which copy actor the mode asks for (5.0) ---

enum class CopyActor : std::uint8_t { plain, pvp };

struct CopyMode {
    bool pvp_on = false;       // PVP_MODE, the session's value
    bool glue_on = false;      // LICHII_COOP_PVP_NATIVE=1 in our game
    bool pack_pvp = false;     // the pack's manifest says "pvp": true
    bool peer_native = false;  // the copy's player said PVP_NATIVE 1
};

// The _Pvp copy only when everything says so; else the plain one (no sensor,
// no Life, no attention sender: nothing to lock on to or hit).
inline CopyActor WantedCopy(const CopyMode& m) {
    return m.pvp_on && m.glue_on && m.pack_pvp && m.peer_native ? CopyActor::pvp : CopyActor::plain;
}

// Whether a copy of `have` is re-made now. A copy that is lifted (phase 3) or
// holding a bomb (phase 2) keeps its actor until the lift or hold ends: the
// re-make would drop the carried player or bomb (review LOW note 2). The
// director asks again every frame, so the change lands the frame it ends.
inline bool RemakeNow(CopyActor have, CopyActor want, bool lifted_or_holding) {
    return have != want && !lifted_or_holding;
}

// The _Pvp copy's actor beside a puppet's (tools/puppet_pack/build.py pvp_name). Only the pack's own
// puppets have one: any other actor (a test actor, a real Player) is left as it is.
inline constexpr std::string_view kPvpSuffix = "_Pvp";

inline bool IsPvpActor(std::string_view actor) {
    return actor.size() > kPvpSuffix.size() && actor.substr(actor.size() - kPvpSuffix.size()) == kPvpSuffix;
}

inline std::string PvpActor(std::string_view actor) {
    if (actor.substr(0, 11) != "CoopPuppet_" || IsPvpActor(actor)) return std::string(actor);
    return std::string(actor) + std::string(kPvpSuffix);
}

// Whether `a` and `b` are one puppet, one of them its _Pvp copy (a mode change, not a new look).
inline bool OnlyPvpDiffers(std::string_view a, std::string_view b) {
    return a != b && (PvpActor(a) == b || PvpActor(b) == a);
}

// The pack's manifest says "pvp": true (build.py --puppet-pvp). Each actor's own "pvp" entry is an object,
// never true, so every "pvp" key is looked at.
inline bool PackHasPvp(std::string_view manifest) {
    constexpr std::string_view key = "\"pvp\"";
    for (auto at = manifest.find(key); at != std::string_view::npos; at = manifest.find(key, at + 1)) {
        const auto v = manifest.find_first_not_of(" \t\r\n:", at + key.size());
        if (v != std::string_view::npos && manifest.substr(v, 4) == "true") return true;
    }
    return false;
}

// Our game makes PVP_NATIVE 1 only with the glue on and a pvp pack.
inline bool SayNative(bool glue_on, bool pack_pvp) { return glue_on && pack_pvp; }

// --- Which hit records are ours to send (5.4) ---

struct RecordFrom {
    bool our_link = false;                 // the record's attacker is our real Link
    std::optional<proto::ObjectId> made;   // else: the shot's or blast's object, if our spawn sync has it
    bool blast = false;                    // that object is a bomb's, a bombchu's or Din's Fire's blast
};

// The strike's source, or nothing when the record is not ours (an enemy's,
// another player's shot, an object of no one).
inline std::optional<proto::PvpSource> StrikeSource(const RecordFrom& r, std::uint8_t self) {
    if (r.our_link) return proto::PvpSource::sword;
    if (r.made && r.made->kind == proto::ObjectKind::made && self != 0 && r.made->maker == self) {
        return r.blast ? proto::PvpSource::blast : proto::PvpSource::shot;
    }
    return std::nullopt;
}

// --- Rings of places (5.6) ---

inline constexpr std::uint64_t kRingUs = 1'000'000;  // a ring keeps this long

// Places over the last second, oldest first: the victim's own Link (each
// frame) and its copy of each player (each sample it received).
class PlaceRing {
public:
    void Push(std::uint64_t t_us, const Vec3& at) {
        if (!places_.empty() && t_us < places_.back().t_us) places_.clear();  // a clock step: start again
        places_.push_back({t_us, at});
        while (!places_.empty() && t_us - places_.front().t_us > kRingUs) places_.pop_front();
    }
    void Clear() { places_.clear(); }
    // The place nearest `t_us`, within `slack_us`; nothing when the ring has
    // none that near.
    std::optional<Vec3> At(std::uint64_t t_us, std::uint64_t slack_us) const {
        std::optional<Vec3> best;
        std::uint64_t best_gap = 0;
        for (const auto& p : places_) {
            const std::uint64_t gap = p.t_us > t_us ? p.t_us - t_us : t_us - p.t_us;
            if (gap <= slack_us && (!best || gap < best_gap)) {
                best = p.at;
                best_gap = gap;
            }
        }
        return best;
    }
    bool Empty() const { return places_.empty(); }

private:
    struct Place {
        std::uint64_t t_us;
        Vec3 at;
    };
    std::deque<Place> places_;
};

// --- Replays (5.6): a strike's seq must be newer than the last taken ---

class SeqGate {
public:
    // True when `seq` is newer than the last one seen from `attacker`; then it
    // is the last one. A repeated or older seq is false (dropped, no result).
    bool Take(std::uint8_t attacker, std::uint32_t seq) {
        auto it = last_.find(attacker);
        if (it != last_.end() && seq <= it->second) return false;
        last_[attacker] = seq;
        return true;
    }
    // The attacker left (PLAYER_LEFT) or joined again (PLAYER_JOINED): its
    // game counts from 1 again (review LOW note 1).
    void Forget(std::uint8_t attacker) { last_.erase(attacker); }
    void Reset() { last_.clear(); }

private:
    std::map<std::uint8_t, std::uint32_t> last_;
};

// --- The victim's checks (5.6) ---

inline constexpr std::uint64_t kStrikeLateUs = 350'000;  // the copy's ~100 ms delay + 250 ms
inline constexpr std::uint64_t kOwnRingSlackUs = 50'000;  // our own ring has a place each frame
inline constexpr std::uint64_t kCopyRingSlackUs = 100'000;
inline constexpr float kJitterM = 0.5f;      // sample interpolation and jitter
inline constexpr float kBodyLevelM = 1.0f;   // the body, level (today's numbers)
inline constexpr float kSwordReachFloorM = CoopPvp::kSwordReachM;  // a weapon without a Range: 1.3 m
inline constexpr float kShotReachM = 1.5f;
inline constexpr float kBlastReachM = 3.0f;  // today's bomb reach

// What the victim's game knows for one strike.
struct StrikeView {
    CoopPvp::Local local;                 // our game this frame
    CoopPvp::Hitter attacker;             // the attacker as presence knows it
    bool downed = false;                  // our Link is downed or dead (coop_dbno)
    const PlaceRing* own = nullptr;       // our Link's places
    const PlaceRing* attacker_ring = nullptr;  // our copy of the attacker's places
    // The weapon's reach from the game's data (WeaponParam.Range of the
    // weapon PLAYER_EQUIP says the attacker holds); nothing: it has none.
    std::optional<float> weapon_range;
    // A shot or blast: the last place our game has for the strike's object;
    // nothing when our game does not have that object.
    std::optional<Vec3> object_at;
};

inline float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline bool Finite(const Vec3& v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

// The hit's place on the body standing at `feet` (y up), with the jitter.
inline bool OnBody(const Vec3& hit, const Vec3& feet) {
    const float dx = hit[0] - feet[0], dz = hit[2] - feet[2];
    const float dy = hit[1] - feet[1];
    return std::sqrt(dx * dx + dz * dz) <= kBodyLevelM + kJitterM && dy >= CoopPvp::kBodyLowM - kJitterM &&
           dy <= CoopPvp::kBodyHighM + kJitterM;
}

// The sword's or body's reach: the weapon's Range, never less than 1.3 m,
// plus the jitter.
inline float SwordReach(std::optional<float> weapon_range) {
    const float range = weapon_range && std::isfinite(*weapon_range) ? *weapon_range : 0.0f;
    return std::max(range, kSwordReachFloorM) + kJitterM;
}

// The kept-off cases of today's PvP (coop_pvp.cpp Victim::Hit) plus a
// downed or dead Link, and a copy of the attacker we do not know.
inline bool KeptOff(const StrikeView& v) {
    const auto& l = v.local;
    const auto& a = v.attacker;
    return v.downed || !l.life || *l.life <= 0 || l.event || CoopPvp::Guarded(l.status) || !l.scene ||
           !a.known || CoopPvp::Guarded(a.status) || !a.scene || *a.scene != *l.scene;
}

// The checks after the seq: nothing when the strike passes (the game's hit
// path decides the rest: taken, guarded, guard broken, invincible); else the
// outcome to answer with.
inline std::optional<proto::PvpOutcome> Refusal(const proto::PvpStrike& s, const StrikeView& v,
                                                std::uint64_t now_us) {
    if (KeptOff(v) || v.own == nullptr || v.attacker_ring == nullptr) return proto::PvpOutcome::kept_off;
    // Never wrapped: a seen_us after now is as late as one too old.
    if (s.seen_us > now_us || now_us - s.seen_us > kStrikeLateUs) return proto::PvpOutcome::late;
    if (!Finite(s.at) || !Finite(s.from)) return proto::PvpOutcome::out_of_reach;
    // Where we were when the attacker saw us.
    const auto we = v.own->At(s.seen_us, kOwnRingSlackUs);
    if (!we || !OnBody(s.at, *we)) return proto::PvpOutcome::out_of_reach;
    // Where we saw the attacker then.
    const auto they = v.attacker_ring->At(s.seen_us, kCopyRingSlackUs);
    if (!they || Distance(s.from, *they) > kJitterM) return proto::PvpOutcome::out_of_reach;
    switch (s.source) {
        case proto::PvpSource::shot:
        case proto::PvpSource::blast: {
            if (!v.object_at) return proto::PvpOutcome::out_of_reach;
            const float reach = s.source == proto::PvpSource::shot ? kShotReachM : kBlastReachM;
            if (Distance(*v.object_at, s.at) > reach) return proto::PvpOutcome::out_of_reach;
            break;
        }
        case proto::PvpSource::sword:
        case proto::PvpSource::other:  // the body's own reach, as a sword without a Range
        default:
            if (Distance(*they, s.at) > SwordReach(s.source == proto::PvpSource::sword ? v.weapon_range
                                                                                     : std::nullopt)) {
                return proto::PvpOutcome::out_of_reach;
            }
            break;
    }
    return std::nullopt;
}

// --- Path B only: the amount from the victim's own tables (5.4) ---

// The kind's key: its first two words (the attack kind, as the record's
// header carries it: CoopHitReplay::kRecKind).
inline std::uint64_t KindKey(const proto::PvpKind& kind) {
    return (static_cast<std::uint64_t>(kind[0]) << 32) | kind[1];
}

// The victim's own copy of the game's tables, read from its romfs.
struct Tables {
    // WeaponParam BaseAttack by the weapon's id as PLAYER_EQUIP names it.
    std::map<std::uint32_t, std::int32_t> base_attack;
    // GameBalance/AttackParam/Player(_Child): DamageMultiplier by kind key.
    std::map<std::uint64_t, float> multiplier;
    // Link's DamageRates for the kind's damage type (DamageParameters/Player:
    // every one 1.0).
    float damage_rate = 1.0f;
    // Life units per point of attack (the game's own; U7 reads it).
    std::int32_t life_per_attack = 1;
};

// The amount our game counts for a strike's kind with the attacker's weapon,
// or nothing when the tables do not name the weapon or the kind (answered
// kept off). The attacker's own amount is never used.
inline std::optional<std::int32_t> PathBAmount(const Tables& t, std::uint32_t weapon,
                                               const proto::PvpKind& kind) {
    const auto w = t.base_attack.find(weapon);
    const auto k = t.multiplier.find(KindKey(kind));
    if (w == t.base_attack.end() || k == t.multiplier.end()) return std::nullopt;
    if (!std::isfinite(k->second) || !std::isfinite(t.damage_rate)) return std::nullopt;
    const double raw = static_cast<double>(w->second) * t.life_per_attack * k->second * t.damage_rate;
    const auto amount = static_cast<std::int32_t>(std::lround(std::clamp(raw, 0.0, double(proto::kMaxPvpLife))));
    return amount;
}

// Path B's invincible time: today's kMercyUs after a taken strike, until U7
// shows the life replay sets the game's own (5.5).
class Mercy {
public:
    bool Refuses(std::uint64_t now_us) const { return last_ && now_us < *last_ + CoopPvp::kMercyUs; }
    void Taken(std::uint64_t now_us) { last_ = now_us; }
    void Reset() { last_.reset(); }

private:
    std::optional<std::uint64_t> last_;
};

// --- One strike, path B, start to end ---

struct PathBInput {
    std::uint32_t weapon = 0;     // the attacker's weapon id (PLAYER_EQUIP)
    bool guarding = false;        // our Link's main AS is a guard one (U5), facing the attacker within 120 deg
    bool guard_fails = false;     // the shield fails for this kind (BadConditionAttackGuard) or GuardBreakPower
};

struct PathBDecision {
    proto::PvpResult result;      // to send (player field 0)
    std::int32_t take = 0;        // life to take off our Link through the hit replay
    bool replay = false;          // replay the kind on our Link (knockback, hurt reaction)
};

// Path B: the checks, the guard, the mercy, the amount. Nothing when the
// strike is dropped with no result (another victim, our own, a repeated seq,
// PvP off).
inline std::optional<PathBDecision> DecidePathB(const proto::PvpStrike& s, bool pvp_on, const StrikeView& v,
                                                const PathBInput& in, const Tables& t, SeqGate& seq,
                                                Mercy& mercy, std::uint64_t now_us) {
    const std::uint8_t self = v.local.self;
    if (!pvp_on || self == 0 || s.victim != self || s.player_number == self || s.player_number < 1 ||
        s.player_number > proto::kMaxPlayers || !seq.Take(s.player_number, s.seq)) {
        return std::nullopt;
    }
    PathBDecision d;
    d.result.attacker = s.player_number;
    d.result.seq = s.seq;
    d.result.kind = s.kind;
    d.result.at = s.at;
    const std::int32_t life = std::clamp<std::int32_t>(v.local.life.value_or(0), 0, proto::kMaxPvpLife);
    d.result.life = life;
    if (const auto no = Refusal(s, v, now_us)) {
        d.result.outcome = *no;
        return d;
    }
    if (in.guarding && !in.guard_fails) {
        d.result.outcome = proto::PvpOutcome::guarded;
        return d;
    }
    if (mercy.Refuses(now_us)) {
        d.result.outcome = proto::PvpOutcome::invincible;
        return d;
    }
    const auto amount = PathBAmount(t, in.weapon, s.kind);
    if (!amount) {
        d.result.outcome = proto::PvpOutcome::kept_off;
        return d;
    }
    d.take = std::min(*amount, life);
    d.replay = true;
    d.result.outcome = in.guarding ? proto::PvpOutcome::guard_broken : proto::PvpOutcome::taken;
    d.result.amount = d.take;
    d.result.life = life - d.take;
    mercy.Taken(now_us);
    return d;
}

}  // namespace CoopPvpNative
