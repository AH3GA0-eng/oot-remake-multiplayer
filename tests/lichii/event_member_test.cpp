// Tests for which actors our event is with (lichii/hook/coop_event_member.h, row 97).
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

TEST_CASE("event member: bound only into OUR running event, the performer in any slot (row 97)") {
    using namespace CoopEventMember;
    constexpr std::uint64_t kMain = 0x7100000000;
    constexpr std::uint64_t kManager = 0x50'0000, kSession = 0x60'0000, kEvents = 0x70'0000;
    constexpr std::uint64_t kEvent = 0x80'0000, kOther = 0xb0'0000, kRecord = 0xc0'0000;
    Memory m;
    // Our running event: one session with one event.
    m.Put(kMain + kEventManagerPtr, kManager);
    m.Put(kManager + kEventSession, kSession);
    m.Put(kSession + kSessionEvents, kEvents);
    m.Put(kSession + kSessionCount, 1);
    m.Put(kEvents, kEvent);
    const Ours ours = ReadOurs(m.Reader(), kMain);
    CHECK(ours.session == kSession);
    REQUIRE(ours.events.size() == 1);
    CHECK(ours.events[0] == kEvent);
    // The actor's performer sits at slot 0x88 (the asker's, as the probe read it).
    m.Put(kActor + kActorParts, kParts);
    m.Put(kParts + kPerformerSlot, kPerformer);
    m.Put(kPerformer + kPerformerActor, kActor);
    m.Put(kPerformer + kPerformerLink, 0);
    Member r = MemberOf(m.Reader(), kActor, ours);
    REQUIRE(r.bound);
    CHECK_FALSE(*r.bound);  // its performer, no link
    // Linked to our event itself, inside it, inside the session, or to a record that refers to it.
    for (const std::pair<std::uint64_t, LinkHow>& c : std::vector<std::pair<std::uint64_t, LinkHow>>{
             {kEvent, LinkHow::event}, {kEvent + 0x40, LinkHow::inside_event},
             {kSession + 0x100, LinkHow::session}, {kRecord, LinkHow::refers}}) {
        const std::uint64_t link = c.first;
        const LinkHow how = c.second;
        CAPTURE(link);
        m.Put(kRecord + 0x10, kEvent);  // the record's third word is our event
        m.Put(kPerformer + kPerformerLink, link);
        r = MemberOf(m.Reader(), kActor, ours);
        REQUIRE(r.bound);
        CHECK(*r.bound);
        CHECK(r.how == how);
        CHECK(r.slot == kPerformerSlot);
    }
    // A butterfly's own event (seen 26-32 m away in the scrub runs): a link to something else.
    m.Put(kOther, 0x1234);
    m.Put(kPerformer + kPerformerLink, kOther);
    r = MemberOf(m.Reader(), kActor, ours);
    REQUIRE(r.bound);
    CHECK_FALSE(*r.bound);
    CHECK(r.link == kOther);
    CHECK(r.how == LinkHow::none);
    // No running session (the manager's word 0): nothing is ours, whatever the link.
    m.Put(kPerformer + kPerformerLink, kEvent);
    CHECK_FALSE(*MemberOf(m.Reader(), kActor, Ours{}).bound);
    // The performer in another slot (a Deku Scrub's is not at 0x88 or 0x90): found there.
    m.Put(kParts + kPerformerSlot, 0);
    m.Put(kParts + 0xa8, kPerformer);
    r = MemberOf(m.Reader(), kActor, ours);
    REQUIRE(r.bound);
    CHECK(*r.bound);
    CHECK(r.slot == 0xa8);
    // A component that is not the actor's own (+0x10 another actor) never counts.
    m.Put(kPerformer + kPerformerActor, 0x99);
    CHECK_FALSE(MemberOf(m.Reader(), kActor, ours).bound);
    // Unreadable or no actor: not known.
    CHECK_FALSE(MemberOf(m.Reader(), 0, ours).bound);
    CHECK_FALSE(MemberOf(m.Reader(), 0x77'0000, ours).bound);
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
