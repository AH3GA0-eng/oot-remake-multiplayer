// SPDX-License-Identifier: GPL-3.0-or-later
//
// Native puppets phase 3: one player lifts, carries, throws or sets down another (docs/native-puppets.md,
// "Lifting another player"; docs/protocol.md, "Player lift"). The rules only, without any game or network
// code (tests/lichii/lift_player_test.cpp); coop_lift_player_game.cpp is the glue.
//
// - The lift itself is the game's own: the copy (CoopPuppet_*) built with the pack switch --puppet-liftable has a
//   LiftableRef (the Cucco's, the game's living liftable), so our Link's own lifter picks it up, carries it on
//   its hand bone and throws or sets it down with the game's own throw (PlayerLifterParam LiftThrowParams).
// - The lifter's game sees the lift on the copy: the game raised it kRiseM over the place the director drew
//   it, within kReachM of our Link, kRiseFrames frames running. It sends LIFT_ASK and stops placing the copy
//   (the director leaves it to the game: CoopPuppet::SetGameHolds) while it waits for the answer.
// - The lifted player's game decides (Victim::Ask): no while its Link is in an event, a load or a menu, dead,
//   already lifted, lifting someone itself, or not near the lifter.
// - Taken: the lifter's game sends the copy's place as the game has it, each frame (LIFT_PLACE held); when the
//   copy leaves our Link's hands (thrown or set down: it is kReleaseM further out or lower than it was carried)
//   the places go on as flying until the copy is at rest (kRestFrames still frames, or kFlightMaxUs), then one
//   landed place ends the lift. The lifted game puts its real Link on each place with the game's own
//   physics-aware setter and answers "input off" to its Link's own input check meanwhile; after the landed
//   place its Link is its own again (it falls and lands by its own character controller).
// - The lifted game ends a lift by itself (LIFT_ANSWER freed) when its Link goes into an event or dies, when
//   the places stop for kPlaceWaitUs (the lifter left or lost the copy), or after kMaxHeldUs (it wriggles free).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "coop/protocol/messages.hpp"

namespace CoopLiftPlayer {

namespace proto = coop::protocol;
using Vec3 = std::array<float, 3>;

inline constexpr float kRiseM = 0.30f;     // the game raised the copy this far over where we drew it ...
inline constexpr float kReachM = 1.6f;     // ... within this of our Link ...
inline constexpr int kRiseFrames = 2;      // ... this many frames running: our Link lifted it
inline constexpr std::uint64_t kAnswerWaitUs = 1'500'000;  // no answer: let go
inline constexpr float kReleaseM = 0.6f;   // further out or lower than carried by this: thrown or set down
inline constexpr float kRestM = 0.02f;     // a flying copy moving less than this a frame ...
inline constexpr int kRestFrames = 8;      // ... this many frames running is at rest
inline constexpr std::uint64_t kFlightMaxUs = 2'000'000;   // a flight ends by this anyway
inline constexpr std::uint64_t kPlaceWaitUs = 1'000'000;   // lifted, no place this long: free
inline constexpr std::uint64_t kMaxHeldUs = 15'000'000;    // lifted this long: wriggles free
inline constexpr float kFarM = 3.0f;       // the lifter's Link must be this near ours

inline float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
inline float Flat(const Vec3& a, const Vec3& b) {
    return std::hypot(a[0] - b[0], a[2] - b[2]);
}

// One copy of another player's Link as the director left it this frame: where it drew the copy last, and
// where the game had it when the director came back (before its write), if read.
struct CopyView {
    std::uint8_t number = 0;
    Vec3 drawn{};
    std::optional<Vec3> game{};
};

// The lifter's side: at most one lift at a time.
class Lifter {
public:
    enum class State { idle, asking, holding, flying };
    struct Out {
        std::optional<proto::LiftAsk> ask;
        std::optional<proto::LiftPlace> place;
    };

    Out Frame(const Vec3& our_link, const std::vector<CopyView>& copies, std::uint64_t now_us) {
        Out out;
        const CopyView* mine = nullptr;
        for (const auto& c : copies) {
            if (c.number == who_ && who_ != 0) mine = &c;
        }
        switch (state_) {
        case State::idle:
            Watch(our_link, copies, now_us, out);
            break;
        case State::asking:
            if (now_us - since_us_ >= kAnswerWaitUs || mine == nullptr) {
                Drop();  // no answer, or the copy is gone: the director places it again
            }
            break;
        case State::holding:
        case State::flying:
            if (mine == nullptr || !mine->game) {
                // The copy is gone (they left, a load): the last place we had ends the lift.
                if (last_) out.place = Place(proto::LiftPhase::landed, *last_);
                Drop();
                break;
            }
            Carry(our_link, *mine->game, now_us, out);
            break;
        }
        // A refused or ended copy is asked again only once the game lets it down.
        for (auto it = blocked_.begin(); it != blocked_.end();) {
            const CopyView* c = nullptr;
            for (const auto& v : copies) {
                if (v.number == *it) c = &v;
            }
            if (c == nullptr || !c->game || (*c->game)[1] - c->drawn[1] < kRiseM / 2) {
                it = blocked_.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    }

    // The lifted game's answer (or the server's not_sharing).
    void Answer(const proto::LiftAnswer& a, std::uint64_t now_us) {
        if (a.player_number != who_ || who_ == 0) return;
        if (state_ == State::asking && a.refusal == proto::LiftRefusal::none) {
            state_ = State::holding;
            since_us_ = now_us;
            carried_.reset();
            return;
        }
        if (a.refusal != proto::LiftRefusal::none) {
            last_refusal_ = a.refusal;
            Drop();
        }
    }

    // The copy the director leaves to the game (asking, holding or flying); 0 none.
    std::uint8_t Held() const { return state_ == State::idle ? 0 : who_; }
    State Now() const { return state_; }
    std::optional<proto::LiftRefusal> LastRefusal() const { return last_refusal_; }
    bool Blocked(std::uint8_t number) const {
        for (auto b : blocked_) {
            if (b == number) return true;
        }
        return false;
    }
    void Reset() { *this = Lifter{}; }

private:
    struct Carried {
        float flat = 0.0f;    // from our Link, across
        float height = 0.0f;  // over our Link
    };

    void Watch(const Vec3& our_link, const std::vector<CopyView>& copies, std::uint64_t now_us, Out& out) {
        for (const auto& c : copies) {
            int& n = rising_[c.number];
            const bool rose = c.game && (*c.game)[1] - c.drawn[1] >= kRiseM && Distance(*c.game, our_link) <= kReachM;
            n = rose ? n + 1 : 0;
            if (n >= kRiseFrames && !Blocked(c.number) && c.number != 0) {
                state_ = State::asking;
                who_ = c.number;
                since_us_ = now_us;
                last_ = c.game;
                out.ask = proto::LiftAsk{proto::kPlayerSelf, c.number};
                rising_.clear();
                return;
            }
        }
    }

    void Carry(const Vec3& our_link, const Vec3& at, std::uint64_t now_us, Out& out) {
        const Carried now{Flat(at, our_link), at[1] - our_link[1]};
        if (state_ == State::holding) {
            if (!carried_) carried_ = now;
            if (now.flat > carried_->flat + kReleaseM || now.height < carried_->height - kReleaseM) {
                state_ = State::flying;  // out of our Link's hands: thrown, or set down
                since_us_ = now_us;
                still_ = 0;
            } else {
                out.place = Place(proto::LiftPhase::held, at);
                last_ = at;
                return;
            }
        }
        // Flying: on until it rests.
        still_ = last_ && Distance(at, *last_) < kRestM ? still_ + 1 : 0;
        last_ = at;
        if (still_ >= kRestFrames || now_us - since_us_ >= kFlightMaxUs) {
            out.place = Place(proto::LiftPhase::landed, at);
            Drop();
            return;
        }
        out.place = Place(proto::LiftPhase::flying, at);
    }

    proto::LiftPlace Place(proto::LiftPhase phase, const Vec3& at) const {
        return proto::LiftPlace{proto::kPlayerSelf, who_, phase, at};
    }

    void Drop() {
        if (who_ != 0 && !Blocked(who_)) blocked_.push_back(who_);
        state_ = State::idle;
        who_ = 0;
        carried_.reset();
        last_.reset();
        still_ = 0;
        rising_.clear();
    }

    State state_ = State::idle;
    std::uint8_t who_ = 0;
    std::uint64_t since_us_ = 0;
    std::optional<Carried> carried_;
    std::optional<Vec3> last_;
    int still_ = 0;
    std::map<std::uint8_t, int> rising_;
    std::vector<std::uint8_t> blocked_;
    std::optional<proto::LiftRefusal> last_refusal_;
};

// Our own game, as the lifted side sees it.
struct Local {
    std::uint8_t self = 0;
    bool event = false;     // our Link in an event, a load or a menu (PLAYER_STATUS away bits but dead)
    bool down = false;      // dead, life 0, or down but not out
    bool lifting = false;   // our Link holds another player's copy (Lifter::Held)
    std::optional<Vec3> our_link;
};

// The lifted player's side: our game decides.
class Victim {
public:
    struct Step {
        std::optional<Vec3> put;                   // our Link's place this frame
        bool input_off = false;                    // our Link's own input check answers "off"
        std::optional<proto::LiftAnswer> freed;    // the lift ended here: tell the lifter
    };

    // A LIFT_ASK; `lifter_at` is the lifter's Link as presence has it (unset: not known).
    proto::LiftAnswer Ask(const proto::LiftAsk& m, const Local& local, const std::optional<Vec3>& lifter_at,
                          std::uint64_t now_us) {
        proto::LiftAnswer a{proto::kPlayerSelf, m.player_number, proto::LiftRefusal::none};
        if (m.lifted != local.self || m.player_number == local.self || m.player_number == 0) {
            a.refusal = proto::LiftRefusal::busy;  // not for us (the server names us; kept for safety)
        } else if (by_ != 0 && by_ != m.player_number) {
            a.refusal = proto::LiftRefusal::busy;
        } else if (local.lifting) {
            a.refusal = proto::LiftRefusal::busy;
        } else if (local.down) {
            a.refusal = proto::LiftRefusal::down;
        } else if (local.event || !local.our_link) {
            a.refusal = proto::LiftRefusal::event;
        } else if (lifter_at && Distance(*lifter_at, *local.our_link) > kFarM) {
            a.refusal = proto::LiftRefusal::too_far;
        }
        if (a.refusal == proto::LiftRefusal::none) {
            by_ = m.player_number;
            since_us_ = now_us;
            place_us_ = now_us;
            at_.reset();
            landed_ = false;
        }
        return a;
    }

    // A LIFT_PLACE: only the lifter's, only while lifted.
    void Place(const proto::LiftPlace& m, std::uint64_t now_us) {
        if (by_ == 0 || m.player_number != by_ || m.lifted == 0) return;
        at_ = m.position;
        place_us_ = now_us;
        landed_ = m.phase == proto::LiftPhase::landed;
    }

    Step Frame(const Local& local, std::uint64_t now_us) {
        Step s;
        if (by_ == 0) return s;
        if (landed_) {
            s.put = at_;  // the last place, then our Link is its own
            End();
            return s;
        }
        proto::LiftRefusal why = proto::LiftRefusal::none;
        if (local.down || local.event || !local.our_link) {
            why = proto::LiftRefusal::freed;
        } else if (now_us - place_us_ >= kPlaceWaitUs || now_us - since_us_ >= kMaxHeldUs) {
            why = proto::LiftRefusal::freed;
        }
        if (why != proto::LiftRefusal::none) {
            s.freed = proto::LiftAnswer{proto::kPlayerSelf, by_, why};
            End();
            return s;
        }
        s.put = at_;
        s.input_off = true;
        return s;
    }

    std::uint8_t By() const { return by_; }
    void Reset() { *this = Victim{}; }

private:
    void End() {
        by_ = 0;
        at_.reset();
        landed_ = false;
    }

    std::uint8_t by_ = 0;
    std::uint64_t since_us_ = 0;
    std::uint64_t place_us_ = 0;
    std::optional<Vec3> at_;
    bool landed_ = false;
};

} // namespace CoopLiftPlayer
