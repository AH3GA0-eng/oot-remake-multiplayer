// SPDX-License-Identifier: GPL-3.0-or-later
//
// Row 97 (event-copy): which actors our running event is with. An event's
// flow binds each actor it uses (the one whose talk or entrance it is, the
// ones it moves or waits on) through that actor's event performer, the same
// performer an actor's own request goes through (coop_live_events.h,
// kPerformerRequest): [[actor + 0x270] + 0x88] (or + 0x90, the slot the
// player's is read from), whose +0x10 points back at the actor.
//
// Read in the deku-sync-fix probe runs (groups-20261114-074019 and -083814,
// reports/deku-sync-fix-b.md): at the event's start the asker's performer
// gets the event's link at +0x18 (0 -> a heap pointer), 1 at +0x20 and the
// state 3 (busy) at +0x28, in both games, and all three go back to 0 when the
// event ends. So an actor whose own performer holds an event link is a member
// of a running event: in our game that is our event, whatever kind it is (a
// scrub's talk or entrance, a shop, a townsperson's talk, a story event, a
// mini-game) and however far from our Link the actor stands.
//
// No emulator code: coop_objects.cpp reads it in its walk, tests in
// tests/lichii/event_member_test.cpp.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace CoopEventMember {

inline constexpr std::uint64_t kActorParts = 0x270;      // the actor's component table
inline constexpr std::uint64_t kPerformerSlot = 0x88;    // as CoopLiveEvents::addr::kPerformerSlot
inline constexpr std::uint64_t kPerformerSlotAlt = 0x90; // as CoopLiveEvents::addr::kPerformerSlotAlt
inline constexpr std::uint64_t kPerformerActor = 0x10;   // back at the actor: the performer is its own
inline constexpr std::uint64_t kPerformerLink = 0x18;    // the event it is bound into; 0 when none

// One 8-byte word of guest memory; nullopt when it cannot be read.
using ReadWord = std::function<std::optional<std::uint64_t>(std::uint64_t address)>;

// The actor's own performer (its +0x10 is the actor); 0 when it has none we know.
inline std::uint64_t PerformerOf(const ReadWord& read, std::uint64_t actor) {
    if (actor == 0) {
        return 0;
    }
    const auto parts = read(actor + kActorParts);
    if (!parts || *parts == 0) {
        return 0;
    }
    for (const std::uint64_t slot : {kPerformerSlot, kPerformerSlotAlt}) {
        const auto performer = read(*parts + slot);
        if (!performer || *performer == 0) {
            continue;
        }
        if (const auto owner = read(*performer + kPerformerActor); owner && *owner == actor) {
            return *performer;
        }
    }
    return 0;
}

// Whether the actor is bound into a running event (its performer holds an
// event link). nullopt: no performer of its own was found (an actor class
// whose performer sits elsewhere): the other rules decide for it.
inline std::optional<bool> InEvent(const ReadWord& read, std::uint64_t actor) {
    const std::uint64_t performer = PerformerOf(read, actor);
    if (performer == 0) {
        return std::nullopt;
    }
    const auto link = read(performer + kPerformerLink);
    if (!link) {
        return std::nullopt;
    }
    return *link != 0;
}

// For the log (row 97: the stuck event had no name in the log). Our running
// event as the event manager has it (docs player-state-map, "The event
// object"): [main + 0x4e94840] +0x68 the session S; S +0xeb8 its events,
// +0xeb0 their count, +0xf18 the current one; an event's kind at +0x254
// (0 flowchart, 1 timeline, 2 movie). Its name is not mapped: the words of
// the event's first kScanWords that point at a C string that looks like a
// name (a letter, then letters, digits, '_', '-', '.' or '/', kMinName to
// kMaxName long) are given as candidates, at most kMaxNames.
inline constexpr std::uint64_t kEventManagerPtr = 0x4e94840;
inline constexpr std::uint64_t kEventSession = 0x68;
inline constexpr std::uint64_t kSessionCount = 0xeb0;
inline constexpr std::uint64_t kSessionEvents = 0xeb8;
inline constexpr std::uint64_t kSessionCurrent = 0xf18;
inline constexpr std::uint64_t kEventKind = 0x254;
inline constexpr std::uint64_t kScanWords = 0x80;
inline constexpr std::size_t kMinName = 4;
inline constexpr std::size_t kMaxName = 63;
inline constexpr std::size_t kMaxNames = 4;

struct Running {
    std::uint64_t event = 0;           // 0: none read
    std::optional<std::uint32_t> kind;
    std::uint32_t count = 0;           // events in the session
    std::vector<std::string> names;    // name-like strings the event points at
};

// The C string at `at` when it looks like a name; empty otherwise.
inline std::string NameAt(const ReadWord& read, std::uint64_t at) {
    std::string out;
    for (std::size_t i = 0; i <= kMaxName; i += 8) {
        const auto word = read(at + i);
        if (!word) {
            return {};
        }
        for (int b = 0; b < 8; ++b) {
            const char c = static_cast<char>((*word >> (8 * b)) & 0xff);
            if (c == '\0') {
                return out.size() >= kMinName ? out : std::string{};
            }
            const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            const bool other = (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/';
            if (!(letter || (!out.empty() && other)) || out.size() >= kMaxName) {
                return {};
            }
            out.push_back(c);
        }
    }
    return {};
}

inline Running ReadRunning(const ReadWord& read, std::uint64_t main) {
    Running out;
    const auto manager = read(main + kEventManagerPtr);
    const auto session = manager && *manager ? read(*manager + kEventSession) : std::nullopt;
    if (!session || *session == 0) {
        return out;
    }
    const auto events = read(*session + kSessionEvents);
    const auto count = read(*session + kSessionCount);    // u32 in the low half
    const auto current = read(*session + kSessionCurrent);  // u32 in the low half
    if (!events || *events == 0 || !count || !current) {
        return out;
    }
    out.count = static_cast<std::uint32_t>(*count);
    const auto index = static_cast<std::uint32_t>(*current);
    if (index >= out.count) {
        return out;
    }
    const auto event = read(*events + 8ull * index);
    if (!event || *event == 0) {
        return out;
    }
    out.event = *event;
    if (const auto kind = read(*event + kEventKind)) {
        out.kind = static_cast<std::uint32_t>(*kind);
    }
    for (std::uint64_t w = 0; w < kScanWords && out.names.size() < kMaxNames; ++w) {
        const auto p = read(*event + 8 * w);
        if (!p || *p < 0x10000 || (*p & 7) != 0) {
            continue;
        }
        std::string name = NameAt(read, *p);
        if (!name.empty() && std::find(out.names.begin(), out.names.end(), name) == out.names.end()) {
            out.names.push_back(std::move(name));
        }
    }
    return out;
}

} // namespace CoopEventMember
