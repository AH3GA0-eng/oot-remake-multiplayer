// Tests for which actors our event is with (lichii/hook/coop_event_member.h, row 97).
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>

#include "coop_event_member.h"

namespace {

// Guest memory as 8-byte words; anything not put cannot be read.
struct Memory {
    std::map<std::uint64_t, std::uint64_t> words;
    void Put(std::uint64_t at, std::uint64_t v) { words[at] = v; }
    void PutText(std::uint64_t at, const std::string& text) {
        std::string padded = text;
        padded.resize((text.size() / 8 + 1) * 8, '\0');
        for (std::size_t i = 0; i < padded.size(); i += 8) {
            std::uint64_t w = 0;
            std::memcpy(&w, padded.data() + i, 8);
            Put(at + i, w);
        }
    }
    CoopEventMember::ReadWord Reader() const {
        return [this](std::uint64_t at) -> std::optional<std::uint64_t> {
            const auto it = words.find(at);
            if (it == words.end()) {
                return std::nullopt;
            }
            return it->second;
        };
    }
};

constexpr std::uint64_t kActor = 0x10'0000;
constexpr std::uint64_t kParts = 0x20'0000;
constexpr std::uint64_t kPerformer = 0x30'0000;
constexpr std::uint64_t kEventLink = 0x106d7f9dd0;  // as in the probe run

} // namespace

TEST_CASE("event member: an actor whose own performer holds an event link is in the event (row 97)") {
    using namespace CoopEventMember;
    Memory m;
    m.Put(kActor + kActorParts, kParts);
    m.Put(kParts + kPerformerSlot, kPerformer);
    m.Put(kPerformer + kPerformerActor, kActor);
    m.Put(kPerformer + kPerformerLink, 0);
    CHECK(PerformerOf(m.Reader(), kActor) == kPerformer);
    REQUIRE(InEvent(m.Reader(), kActor));
    CHECK_FALSE(*InEvent(m.Reader(), kActor));  // not bound: no event link
    m.Put(kPerformer + kPerformerLink, kEventLink);  // the event's start binds it
    CHECK(*InEvent(m.Reader(), kActor));
    // The second slot (the player's) is looked at when the first is not its own.
    m.Put(kParts + kPerformerSlot, 0x40'0000);
    m.Put(0x40'0000 + kPerformerActor, 0x99);  // another actor's performer
    CHECK_FALSE(InEvent(m.Reader(), kActor));   // neither is its own: not known
    m.Put(kParts + kPerformerSlotAlt, kPerformer);
    CHECK(PerformerOf(m.Reader(), kActor) == kPerformer);
    CHECK(*InEvent(m.Reader(), kActor));
    // Unreadable or empty: not known, never "in the event".
    CHECK_FALSE(InEvent(m.Reader(), 0));
    CHECK_FALSE(InEvent(m.Reader(), 0x77'0000));
}

TEST_CASE("event member: our running event's kind and the names it points at, for the log") {
    using namespace CoopEventMember;
    constexpr std::uint64_t kMain = 0x7100000000;
    constexpr std::uint64_t kManager = 0x50'0000, kSession = 0x60'0000, kEvents = 0x70'0000;
    constexpr std::uint64_t kEvent = 0x80'0000, kName = 0x90'0000, kJunk = 0xa0'0000;
    Memory m;
    CHECK(ReadRunning(m.Reader(), kMain).event == 0);  // nothing readable
    m.Put(kMain + kEventManagerPtr, kManager);
    m.Put(kManager + kEventSession, 0);
    CHECK(ReadRunning(m.Reader(), kMain).event == 0);  // no event runs
    m.Put(kManager + kEventSession, kSession);
    m.Put(kSession + kSessionEvents, kEvents);
    m.Put(kSession + kSessionCount, 2);
    m.Put(kSession + kSessionCurrent, 1);
    m.Put(kEvents + 8, kEvent);
    m.Put(kEvent + kEventKind, 0);
    m.Put(kEvent + 0x10, kName);
    m.PutText(kName, "InsideTheDekuTree_Scrub_Talk");
    m.Put(kEvent + 0x18, kJunk);
    m.PutText(kJunk, "\x01\x02zz");          // not a name
    m.Put(kEvent + 0x20, kName);             // the same name again: once
    const Running r = ReadRunning(m.Reader(), kMain);
    CHECK(r.event == kEvent);
    CHECK(r.count == 2);
    REQUIRE(r.kind);
    CHECK(*r.kind == 0);
    REQUIRE(r.names.size() == 1);
    CHECK(r.names[0] == "InsideTheDekuTree_Scrub_Talk");
    m.Put(kSession + kSessionCurrent, 2);  // past the count: none
    CHECK(ReadRunning(m.Reader(), kMain).event == 0);
}
