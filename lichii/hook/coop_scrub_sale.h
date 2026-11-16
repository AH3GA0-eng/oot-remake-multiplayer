// SPDX-License-Identifier: GPL-3.0-or-later
//
// Business Scrubs' one-time sales (tracker row 100 follow-up, docs/items.md
// "One-time sales"): a sale bought in one game is bought in every game.
//
// Read from the romfs (build e0f3ccfaf50065ae, 1.1.1; GameDataList, Banc,
// Logic, Event/EventFlow/Enemy_Scrub_Business_Common):
// - A one-time sale is the shop flow's Onetime_Shop: after the buy it sets a
//   saved Bool (ResetTypeValue 8) named after the scrub, and the scrub's
//   prefab graph deletes the scrub from that Bool (HoldGameDataGetBoolSimple
//   -> PrefabAllDelete): Logic/Enemy_Scrub_Business_DekuNutsPouch_a676,
//   _DekuStickPouch_300c, _Heart_Piece_86ac and
//   DeathMountainCrater_Child_Prefab_0df2. The four Bools below are every
//   such flag in GameDataList (every saved Bool whose name holds
//   "_Enemy_Scrub_Business_"); nothing in the romfs sets one back to 0.
// - What the sale gives is a shared item: the nut bag and stick bag (max),
//   a heart piece (a count: each buy adds one for the whole group).
// - These Bools were not shared, so after one player's buy every other game
//   still had its scrub: a second bag step (the menu picks Item_NutBag_Large
//   once IsGet Medium is shared) or a second heart piece for the group.
// - The randomizer (OOTRR 0.6, setting shuffle_scrubs_b) turns the Lost Woods
//   grotto's rear scrub and Jabu-Jabu's scrub into one-time sales of a placed
//   item: its new flow entry asks a spare saved Bool and leaves at once when
//   it is set (crates/core/src/scrubs_b/flow.rs, data/scrubs_b.json). The same
//   rule, so those two Bools are here too. Nothing else sets them: their
//   names are flow entries no placement uses.
// Left out on purpose: the Boss Rush room's scrubs (DreamWorld_Scrub_Dream_*,
// ResetTypeValue 1: one run's own state, not the save), the menu scrubs that
// sell again and again (seeds, nuts, bombs, potions, arrows), and the Deku and
// Mad Scrubs (enemies; their drops are not saved).
//
// Shared as a pickup (FlagWatcher::share_pickup_named, as a chest's opened
// Bool): 0 to 1 goes out, a 1 never goes back to 0. While our Link is in an
// event (coop_scrub_talk.h's Hold) or within kNearM of a scrub the flag
// deletes, in that scrub's scene, another player's 1 waits (coop_hook's
// HoldFlagStore): our own talk with that scrub must not lose the scrub
// (issue 144's softlock, the same graph rule). Once we walk away it lands, and
// the scrub is gone as in the buyer's game. Off with LICHII_COOP_SCRUB_SALE=0.
// Pure: tested in tests/lichii/scrub_sale_test.cpp.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "coop/host/flags.hpp"

namespace CoopScrubSale {

struct SaleFlag {
    std::uint32_t hash;  // Murmur3 x86 32, seed 0, of the name
    std::string_view name;
    std::string_view what;  // for the log
};

inline constexpr std::array<SaleFlag, 6> kSaleFlags{{
    {0x5ef24799, "Lostwood_Enemy_Scrub_Business_DekuNutsPouch_01", "the Lost Woods grotto's nut bag scrub"},
    {0x8c98cf5a, "LostWood_Enemy_Scrub_Business_DekuStickPouch_01", "the stick bag scrub (Lost Woods, Kokiri Forest)"},
    {0xe41e1597, "HyruleField_Enemy_Scrub_Business_HeartPiece_01", "Hyrule Field's heart piece scrub"},
    {0x6b8fc751, "DeathMountain_Enemy_Scrub_Business_HeartPiece_01", "Death Mountain Crater's heart piece scrub"},
    // OOTRR's one-time sales (shuffle_scrubs_b), spare flags it reuses:
    {0x1ef2f533, "HavePlayedEvent.Enemy_Scrub_Business_DekuStick_01", "the randomizer's Lost Woods grotto rear scrub"},
    {0x25359a9e, "HavePlayedEvent.Enemy_Scrub_Business_RedPotion_01", "the randomizer's Jabu-Jabu scrub"},
}};

inline const SaleFlag* Find(std::uint32_t hash) {
    for (const SaleFlag& f : kSaleFlags) {
        if (f.hash == hash) return &f;
    }
    return nullptr;
}

inline bool IsSaleFlag(std::uint32_t hash) {
    return Find(hash) != nullptr;
}

// Where each flag's scrub stands: the Banc placement (Translate, metres, as
// Link's position), by the scene name the game reports (Banc file name).
struct Seller {
    std::uint32_t flag;
    std::string_view scene;
    float x, y, z;
};

inline constexpr std::array<Seller, 12> kSellers{{
    {0x5ef24799, "LostWood_Child", 7.3f, 82.9f, -85.6f},
    {0x5ef24799, "LostWood_Adult", 7.3f, 82.9f, -85.6f},
    {0x8c98cf5a, "LostWood_Child", -45.2f, 95.2f, 72.6f},
    {0x8c98cf5a, "LostWood_Adult", -45.2f, 95.2f, 72.6f},
    {0x8c98cf5a, "KokiriForest_Child", -128.0f, 114.6f, -15.3f},
    {0x8c98cf5a, "KokiriForest_Adult", -128.0f, 114.6f, -15.3f},
    {0xe41e1597, "HyruleField_Child", 25.0f, 129.9f, 22.1f},
    {0xe41e1597, "HyruleField_Adult", 25.0f, 129.9f, 22.1f},
    {0x6b8fc751, "DeathMountainCrater_Child", -23.0f, 119.1f, 33.0f},
    // OOTRR's: the grotto's rear scrub (the child seed and adult arrow
    // scrubs' spot) and Jabu-Jabu's scrub.
    {0x1ef2f533, "LostWood_Child", 8.8f, 82.9f, -89.4f},
    {0x1ef2f533, "LostWood_Adult", 8.8f, 82.9f, -89.4f},
    {0x25359a9e, "InsideJabuJabuBelly", -27.7f, 74.1f, -42.5f},
}};

// A talk can start 5.3 m from a scrub's actor place (coop_scrub_talk.h,
// Near::kNearM, seen in a run): the same 8 m.
inline constexpr float kNearM = 8.0f;

// Is our Link within kNearM of a scrub that `flag` deletes, in its scene?
inline bool Near(std::uint32_t flag, std::string_view scene, float x, float y, float z) {
    for (const Seller& s : kSellers) {
        if (s.flag != flag || s.scene != scene) continue;
        const float dx = x - s.x, dy = y - s.y, dz = z - s.z;
        if (dx * dx + dy * dy + dz * dz <= kNearM * kNearM) return true;
    }
    return false;
}

// What our game is doing this frame, as the flag hold sees it.
struct Here {
    bool in_event = false;  // coop_scrub_talk.h's Hold: our event, with its tail
    bool known = false;     // a reading this frame (scene and position)
    std::string_view scene;
    float x = 0, y = 0, z = 0;
};

// Does another player's write of `hash` wait in our game now?
inline bool Waits(std::uint32_t hash, const Here& here) {
    if (!IsSaleFlag(hash)) return false;
    if (here.in_event) return true;
    return here.known && Near(hash, here.scene, here.x, here.y, here.z);
}

// Adds the sale flags to the shared list; returns how many were new.
inline std::size_t Merge(std::optional<coop::host::SharedFlags>& shared) {
    std::vector<coop::host::FlagKey> keys;
    if (shared) keys = shared->keys();
    const std::size_t before = shared ? shared->size() : 0;
    for (const SaleFlag& f : kSaleFlags) keys.push_back(coop::host::FlagKey(f.hash, std::uint16_t{0}));
    shared = coop::host::SharedFlags(std::move(keys));  // sorts, drops repeats
    return shared->size() - before;
}

} // namespace CoopScrubSale
