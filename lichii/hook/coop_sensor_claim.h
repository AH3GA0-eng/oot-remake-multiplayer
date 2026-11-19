// SPDX-License-Identifier: GPL-3.0-or-later
//
// Row 138(8) (room-result-0116-2): shared actors whose visible state follows a sensor of OUR Link only.
//
// Jabu-Jabu (Npc_JabuJabu, ZoraFountain) opens his mouth from his own logic inputs IsInOpenMouthArea and
// IsInKeepOpenMouthArea. The romfs wires both from HoldActorObserverBySensorHitInfo (ZoraFountain_Child_Prefab_41c4):
// a sensor that only ever sees the local Link. Jabu is shared as a townsperson copy (IsShared: every Npc_), so
// the owner's animation was put over every other game's Jabu. With the owner's Link away from the mouth, a player
// standing at it got the owner's closed mouth over their own open one, and the others never saw it open. That is
// "Jabu opens only for players in the zone".
//
// Both inputs are wired, so setting them from outside cannot hold (CoopLogicSet::SetInput: unwired inputs only).
// The native path is ownership: while the input reads 1 here, our Link is in the actor's area, and our game's
// own AI is the one that runs it right for every game. The objects sync then claims the actor the way an event
// claims its partners (Sync::AreaClaim; Seen::sensor_in_area): it is flagged targeting so the server hands it to
// us, the owner's place, animation and remove are not put on our copy meanwhile (Sync::EventPartner), and a
// claim we already own is held as a fight (two players in the area: the current owner keeps it).
//
// The same pattern over the whole romfs (reports/room-result-0116-sensors.txt: 452 inputs read a local-Link
// sensor) was sorted by hand (docs/objects.md "Area claims"). Only a SHARED actor can be overridden by its owner,
// and only a sensor that makes the actor act on the Link in it needs a claim. These are listed below by input
// name, for every shared townsperson or animal that has a live input plug of that name. Enemies are not listed:
// their area inputs (AreaHit, IsActive, IsActivate, RespawnTrigger ...) wake their own AI on our Link, and the
// chase rule already claims a copy whose own AI asks for another animation than its owner's (issue 101/132/157).
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "coop/protocol/codec.hpp"
#include "coop_logic_probe.h"

namespace CoopSensorClaim {

// The input plugs, by name, whose 1 means "our Link is in this actor's area and it acts on him".
inline constexpr std::array<std::string_view, 6> kInputs{
    "IsInOpenMouthArea",      // Npc_JabuJabu (ZoraFountain): the mouth opens for the Link at it
    "IsInKeepOpenMouthArea",  // Npc_JabuJabu: ... and stays open while that Link is in it
    "IsInChaseArea",          // Npc_Hylia_030_Child (Graveyard), _052/_065_Child (Market): runs after that Link
    "IsStumpAreaContact",     // Npc_SkullKid_Child (LostWood): that Link on the stump
    "IsInLightArea",          // Npc_Gerudo_014.._018 (GerudoFortress): that Link in the guard's light
    "IsExistTarget",          // Animal_Shark (HyruleField): it goes for that Link
};

// Whether an actor of this name can have such an input: a shared townsperson or animal (enemies: see above).
inline bool Kind(std::string_view name) { return name.starts_with("Npc_") || name.starts_with("Animal_"); }

// The plug keys of kInputs as the live logic lists them (Murmur3 of the name, xor 7).
inline const std::array<std::uint32_t, kInputs.size()>& Keys() {
    static const auto keys = [] {
        std::array<std::uint32_t, kInputs.size()> k{};
        std::transform(kInputs.begin(), kInputs.end(), k.begin(),
                       [](std::string_view n) { return coop::protocol::flag_hash(n) ^ 7u; });
        return k;
    }();
    return keys;
}

// From the actor's live plugs (CoopLogicProbe::Read): true when one of kInputs reads 1, false when it has one
// and none reads 1, nothing when it has none of them (or none could be read).
inline std::optional<bool> InArea(const std::vector<CoopLogicProbe::Plug>& plugs) {
    std::optional<bool> in;
    for (const CoopLogicProbe::Plug& p : plugs) {
        if (p.type != 0 || !p.value) continue;  // an input plug (CoopLogicSet::kInputType) with a value read
        if (std::find(Keys().begin(), Keys().end(), p.key) == Keys().end()) continue;
        if (*p.value != 0) return true;
        in = false;
    }
    return in;
}

}  // namespace CoopSensorClaim
