// SPDX-License-Identifier: GPL-3.0-or-later
// The pause menu's Multiplayer tab (coop_pause_tab.h): when its page shows, what
// the module does to the game, what the pad lets through, and the probe's lines.
#include <doctest/doctest.h>

#include <string>
#include <thread>
#include <vector>

#include "coop_pause_tab.h"

namespace {

using namespace CoopPauseTab;

Facts Strip(std::uint64_t screen, int tabs, int lit, int mode = kPlusMode, int state = 2) {
    Facts f;
    f.kind = Kind::kStrip;
    f.screen = screen;
    f.tabs = tabs, f.lit = lit, f.mode = mode, f.state = state;
    return f;
}

Facts Pages(std::uint64_t screen, int page, int asked, int state = 2) {
    Facts f;
    f.kind = Kind::kPages;
    f.screen = screen;
    f.page = page, f.asked = asked, f.state = state;
    return f;
}

Facts Blocker(int state) {
    Facts f;
    f.kind = Kind::kBlocker;
    f.screen = 0xb000;
    f.state = state;
    return f;
}

bool Has(const std::vector<std::string>& lines, const std::string& part) {
    for (const auto& l : lines) {
        if (l.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// A page whose panes and game calls were found (the module's SetUsable), as in a game with the pack and module.
Page UsablePage() {
    Page p;
    p.SetUsable(true);
    return p;
}

// The menu open on System, the strip with the pack's five tabs, at `now`.
void OnSystem(Page& p, std::uint64_t now, int tabs = 5) {
    p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now);
    p.OnScreen(Strip(0x9000, tabs, kSystemPage), now);
}

}  // namespace

TEST_CASE("pause tab: the screens are known by their class's vtable") {
    CHECK(KindOf(kPagesVtable) == Kind::kPages);
    CHECK(KindOf(kStripVtable) == Kind::kStrip);
    for (const std::uint64_t v : kBlockerVtables) {
        CHECK(KindOf(v) == Kind::kBlocker);
    }
    CHECK(KindOf(0x4bf06e8) == Kind::kOther);   // the title (UITitleScreen)
    CHECK(KindOf(0x4be8300) == Kind::kOther);   // the HUD (UIMapMenuScreen): updates every frame in play
    // Guest addresses, main at 0x80020000 (docs/game-ui.md and the game's names list).
    CHECK(0x80020000 + kPagesVtable == 0x84c0baa8);
    CHECK(0x80020000 + kStripVtable == 0x84c08aa0);
    CHECK(0x80020000 + kMenuManager == 0x84e9cfa8);
    CHECK(0x80020000 + kBlockerVtables[0] == 0x84c0fbf8);   // UISystemWindowScreen
    CHECK(0x80020000 + kBlockerVtables[7] == 0x84c07068);   // UIGameOverScreen
    CHECK(0x80020000 + kSetVisible == 0x8022aea0);
    CHECK(0x80020000 + kTabFill == 0x80a8ab50);
    CHECK(0x80020000 + kTabLight == 0x802ecf60);
    CHECK(0x80020000 + kPaneTable == 0x8045c040);
    CHECK(0x80020000 + kPaneAlpha == 0x802f8ac0);
    CHECK(0x80020000 + kTabOpen == 0x80228350);
    CHECK(0x80020000 + kPaneByName == 0x8045e300);
}

TEST_CASE("pause tab: R on System shows the page and lights the fifth tab, L gives System back") {
    Page p = UsablePage();
    std::uint64_t now = 1000;
    // First updates: our page hidden (System shown), the fifth tab shown, System's lit.
    auto d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now);
    CHECK(d.set_page);
    CHECK_FALSE(d.page_shown);
    CHECK(d.set_texts);
    d = p.OnScreen(Strip(0x9000, 5, kSystemPage), now);
    CHECK(d.set_tabs);
    CHECK(d.tab_shown);
    CHECK_FALSE(d.tab_lit);
    CHECK(d.fill_tab);
    CHECK_FALSE(p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now).any());   // (nothing again)
    CHECK_FALSE(p.OnScreen(Strip(0x9000, 5, kSystemPage), now).any());
    // R: taken, the page shows at the next updates.
    CHECK(p.OnPad(kPadR, now) == 0);
    CHECK(p.Shown());
    d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now + 16);
    CHECK(d.set_page);
    CHECK(d.page_shown);
    d = p.OnScreen(Strip(0x9000, 5, kSystemPage), now + 16);
    CHECK(d.set_tabs);
    CHECK(d.tab_lit);
    // R held: still the module's (no fresh R for the game).
    CHECK(p.OnPad(kPadR, now + 32) == 0);
    CHECK(p.OnPad(0, now + 48) == 0);
    // L: back to System; L stays the module's until let go, so the game does not step to Items.
    CHECK(p.OnPad(kPadL, now + 64) == 0);
    CHECK_FALSE(p.Shown());
    CHECK(p.TakeWhy() == "L");
    CHECK(p.OnPad(kPadL, now + 80) == 0);
    CHECK(p.OnPad(0, now + 96) == 0);
    CHECK(p.OnPad(kPadL, now + 112) == kPadL);   // (a new L is the game's: Items)
    d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now + 120);
    CHECK(d.set_page);
    CHECK_FALSE(d.page_shown);
    // ZR and ZL do the same.
    p.OnPad(0, now + 130);
    CHECK(p.OnPad(kPadZR, now + 140) == 0);
    CHECK(p.Shown());
    CHECK(p.OnPad(kPadZL, now + 150) == 0);
    CHECK_FALSE(p.Shown());
}

TEST_CASE("pause tab: under the page the game gets only B, + and -, and up and down for its cursor, A is ours") {
    Page p = UsablePage();
    std::uint64_t now = 1000;
    OnSystem(p, now);
    REQUIRE(p.OnPad(kPadR, now) == 0);
    p.OnPad(0, now + 1);
    const std::uint64_t x = 1ull << 2, y = 1ull << 3, left = 1ull << 12, right = 1ull << 14, stick_right = 1ull << 18;
    CHECK(p.OnPad(kPadA, now + 2) == 0);   // A: ours, not the System page's
    CHECK(p.TakeDecide());
    CHECK_FALSE(p.TakeDecide());
    CHECK(p.OnPad(x | y | left | right | stick_right | kPadZL, now + 3) == 0);   // (ZL: back to System, taken)
    CHECK_FALSE(p.Shown());
    p.OnPad(0, now + 4);
    REQUIRE(p.OnPad(kPadR, now + 5) == 0);
    p.OnPad(0, now + 6);
    // Up and down reach the game: the System page's focus moves, and the game's cursor frame with it over our rows.
    CHECK(p.OnPad(kPadDown, now + 7) == kPadDown);
    CHECK(p.OnPad(kPadUp | kPadStickUp, now + 8) == (kPadUp | kPadStickUp));
    CHECK(p.OnPad(kPadDown | kPadA, now + 9) == kPadDown);
    CHECK(p.TakeDecide());
    p.OnPad(0, now + 10);
    CHECK(p.OnPad(kPadB, now + 11) == kPadB);
    CHECK(p.OnPad(kPadPlus, now + 12) == kPadPlus);
    CHECK(p.OnPad(kPadMinus, now + 13) == kPadMinus);
    CHECK(p.Shown());   // (the game acts on B, + and -; the screens' updates then give System back)
}

TEST_CASE("pause tab: the page does not show off System, mid-step, in the Minus menu, without the pack or under a window") {
    std::uint64_t now = 1000;
    SUBCASE("on Items") {
        Page p = UsablePage();
        p.OnScreen(Pages(0xa000, 4, 4), now);
        p.OnScreen(Strip(0x9000, 5, 4), now);
        CHECK(p.OnPad(kPadR, now) == kPadR);   // (the game's: Items to System)
        CHECK_FALSE(p.Shown());
    }
    SUBCASE("mid-step to System") {
        Page p = UsablePage();
        p.OnScreen(Pages(0xa000, 4, kSystemPage), now);
        p.OnScreen(Strip(0x9000, 5, kSystemPage), now);
        CHECK(p.OnPad(kPadR, now) == kPadR);
        CHECK_FALSE(p.Shown());
    }
    SUBCASE("the Minus menu") {
        Page p = UsablePage();
        p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now);
        p.OnScreen(Strip(0x9000, 5, 2, 0), now);
        CHECK(p.OnPad(kPadR, now) == kPadR);
        CHECK_FALSE(p.Shown());
    }
    SUBCASE("four tabs: the pack's layouts are not there") {
        Page p = UsablePage();
        OnSystem(p, now, 4);
        CHECK(p.OnPad(kPadR, now) == kPadR);
        CHECK_FALSE(p.Shown());
        CHECK_FALSE(p.OnScreen(Strip(0x9000, 4, kSystemPage), now).set_tabs);   // (no fifth tab to touch)
    }
    SUBCASE("the menu still opening") {
        Page p = UsablePage();
        p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage, 1), now);
        p.OnScreen(Strip(0x9000, 5, kSystemPage, kPlusMode, 1), now);
        CHECK(p.OnPad(kPadR, now) == kPadR);
        CHECK_FALSE(p.Shown());
    }
    SUBCASE("a window over the System page (Save, Options, a question)") {
        Page p = UsablePage();
        OnSystem(p, now);
        p.OnScreen(Blocker(2), now);
        CHECK(p.OnPad(kPadR, now + 10) == kPadR);
        CHECK_FALSE(p.Shown());
        // Gone (state 0) for a while: R is ours again.
        p.OnScreen(Blocker(0), now + 20);
        p.OnPad(0, now + 299);
        OnSystem(p, now + 300);
        CHECK(p.OnPad(kPadR, now + 300) == 0);
        CHECK(p.Shown());
    }
    SUBCASE("the screens' updates stopped") {
        Page p = UsablePage();
        OnSystem(p, now);
        CHECK(p.OnPad(kPadR, now + 1000) == kPadR);
        CHECK_FALSE(p.Shown());
    }
}

TEST_CASE("pause tab: closing, the Minus menu, a window or Game Over give System back at once") {
    std::uint64_t now = 1000;
    const auto shown = [&](Page& p) {
        OnSystem(p, now);
        REQUIRE(p.OnPad(kPadR, now) == 0);
        p.OnPad(0, now + 1);
        p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now + 2);
        REQUIRE(p.Shown());
    };
    SUBCASE("B closes the menu") {
        Page p = UsablePage();
        shown(p);
        CHECK(p.OnPad(kPadB, now + 3) == kPadB);
        const auto d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage, 3), now + 20);
        CHECK_FALSE(p.Shown());
        CHECK(d.set_page);
        CHECK_FALSE(d.page_shown);
        CHECK(p.TakeWhy() == "the menu is closing");
    }
    SUBCASE("the Minus menu") {
        Page p = UsablePage();
        shown(p);
        p.OnScreen(Strip(0x9000, 5, 2, 0), now + 20);
        CHECK_FALSE(p.Shown());
        CHECK(p.TakeWhy() == "the Minus menu");
    }
    SUBCASE("Game Over or a window") {
        Page p = UsablePage();
        shown(p);
        p.OnPad(kPadA, now + 3);   // (A held as the page goes)
        p.OnScreen(Blocker(1), now + 20);
        CHECK_FALSE(p.Shown());
        CHECK(p.TakeWhy() == "a window opened over it");
        CHECK(p.OnPad(kPadA, now + 21) == 0);         // still held: not a fresh A for System
        CHECK(p.OnPad(0, now + 22) == 0);
        CHECK(p.OnPad(kPadA, now + 23) == kPadA);     // a new press is the game's
    }
    SUBCASE("a new pages screen object (a load) starts over: our page hidden") {
        Page p = UsablePage();
        shown(p);
        const auto d = p.OnScreen(Pages(0xa100, kSystemPage, kSystemPage, 0), now + 20);
        CHECK_FALSE(p.Shown());
        CHECK(d.set_page);
        CHECK_FALSE(d.page_shown);
    }
}

TEST_CASE("pause tab: the probe logs a screen's first update, its first second's count and each change") {
    Probe p;
    auto lines = p.Seen(Strip(0x9000, 5, 5), 1000);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "the tab strip (MenuOverlay_00), screen 0x9000: its updates reach the module (patch 0030)"));
    CHECK(Has(lines, "5 tabs, the Plus menu, lights category 5, state 2"));
    for (int i = 1; i < 60; ++i) {
        CHECK(p.Seen(Strip(0x9000, 5, 5), 1000 + 16 * static_cast<std::uint64_t>(i)).empty());
    }
    lines = p.Seen(Strip(0x9000, 5, 5), 2000);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "the tab strip (MenuOverlay_00): 61 updates in its first 1000 ms"));
    CHECK(p.Seen(Strip(0x9000, 5, 5), 3000).empty());
    lines = p.Seen(Strip(0x9000, 5, 4), 3016);
    REQUIRE(lines.size() == 1);
    CHECK(Has(lines, "lights category 4, state 2 (was 5 tabs, the Plus menu, lights category 5, state 2)"));
    CHECK(Has(p.Seen(Strip(0x9000, 5, 2, 0), 3032), "the Minus menu, lights category 2"));   // (mode 0: Minus, seen in a run)
    lines = p.Seen(Pages(0xa000, 5, 5), 3048);
    CHECK(Has(lines, "the pause menu's pages (PlusMenu_00), screen 0xa000: its updates reach the module"));
    CHECK(Has(p.Seen(Pages(0xa000, 5, 4), 3064), "page 5, asked 4, state 2 (was page 5, asked 5, state 2)"));
    CHECK(Has(p.Seen(Strip(0x9100, 4, 5), 4000), "screen 0x9100: its updates reach the module"));
    Facts other;
    other.screen = 0x9000;
    CHECK(p.Seen(other, 0).empty());
    CHECK(p.Seen(Blocker(2), 0).empty());
}

TEST_CASE("pause tab: each row leads to co-op's own part, by what co-op is doing") {
    Session idle;
    Session joined;
    joined.in_one = true;
    Session hosting_no_code;
    hosting_no_code.hosting = hosting_no_code.in_one = true;
    Session hosting = hosting_no_code;
    hosting.code = true;

    for (const Session& s : {idle, joined, hosting_no_code, hosting}) {
        CHECK(ActionFor(kRowSession, s) == (s.in_one ? RowAction::kOpenMain : RowAction::kHost));
        CHECK(ActionFor(kRowPlayers, s) == RowAction::kViewPlayers);
        CHECK(ActionFor(kRowLeave, s) == (s.in_one ? RowAction::kConfirmLeave : RowAction::kNone));
        CHECK(ActionFor(-1, s) == RowAction::kNone);              // (no row under the cursor)
        CHECK(ActionFor(kRows, s) == RowAction::kNone);
    }
    // Join code: the host copies its room name or code (shown on the main page); a host whose code is still coming
    // sees the main page; in another's session the main page; alone the Join page.
    CHECK(ActionFor(kRowJoinCode, hosting) == RowAction::kCopyCode);
    CHECK(ActionFor(kRowJoinCode, hosting_no_code) == RowAction::kOpenMain);
    CHECK(ActionFor(kRowJoinCode, joined) == RowAction::kOpenMain);
    CHECK(ActionFor(kRowJoinCode, idle) == RowAction::kViewJoin);
    // Let in: the host's Let in view; anyone else the Lobby tab (who is in, who asks).
    CHECK(ActionFor(kRowLetIn, hosting) == RowAction::kViewLetIn);
    CHECK(ActionFor(kRowLetIn, hosting_no_code) == RowAction::kViewLetIn);
    CHECK(ActionFor(kRowLetIn, joined) == RowAction::kOpenLobby);
    CHECK(ActionFor(kRowLetIn, idle) == RowAction::kOpenLobby);
    // Each has words for the log.
    for (RowAction a : {RowAction::kNone, RowAction::kOpenMain, RowAction::kOpenJoin, RowAction::kCopyCode,
                        RowAction::kOpenLobby, RowAction::kAllow, RowAction::kHost, RowAction::kConfirmLeave}) {
        CHECK(std::string(ActionWords(a)).size() > 3);
    }
}

TEST_CASE("pause tab: the invite code's two rows: the host's Paste answer, a waiting guest's Copy answer") {
    // A host with an invite code (no relay room): a friend whose direct join failed sends back an answer code, which
    // Let in pastes (co-op's menu's Paste answer) while nobody asks; somebody asking still comes first (/allow).
    Session host;
    host.hosting = host.in_one = host.code = host.invite = true;
    CHECK(ActionFor(kRowLetIn, host) == RowAction::kPasteAnswer);
    CHECK(ActionFor(kRowJoinCode, host) == RowAction::kCopyCode);
    host.asking = true;
    CHECK(ActionFor(kRowLetIn, host) == RowAction::kViewLetIn);
    // A relay room has no answer codes: Let in is its view (Let in or Refuse who asks).
    Session relay;
    relay.hosting = relay.in_one = relay.code = true;
    CHECK(ActionFor(kRowLetIn, relay) == RowAction::kViewLetIn);
    // A guest the host could not reach: Join code copies this game's answer code (co-op's menu's Copy answer).
    Session waiting;
    waiting.in_one = waiting.wait_host = true;
    CHECK(ActionFor(kRowJoinCode, waiting) == RowAction::kCopyAnswer);
    CHECK(ActionFor(kRowLeave, waiting) == RowAction::kConfirmLeave);
    for (RowAction a : {RowAction::kPasteAnswer, RowAction::kCopyAnswer}) {
        CHECK(std::string(ActionWords(a)).size() > 3);
    }

    SessionView v;
    v.s = Session{};
    v.s.hosting = v.s.in_one = v.s.code = v.s.invite = true;
    v.players = 1;
    CHECK(RowTexts(v) ==
          std::vector<std::string>{"Hosting", "Copy invite code", "Players: just you", "Paste answer code", "Stop hosting"});
    v.asking = {"Saria"};
    v.s.asking = true;
    CHECK(RowTexts(v)[kRowLetIn] == "Let in Saria");
    SessionView w;
    w.s.in_one = w.s.wait_host = true;
    w.connecting = true;
    CHECK(RowTexts(w)[kRowSession] == "Connecting...");
    CHECK(RowTexts(w)[kRowJoinCode] == "Copy answer code");
}

TEST_CASE("pause tab: the rows say what co-op is doing, in at most a row's width") {
    SessionView idle;
    CHECK(RowTexts(idle) ==
          std::vector<std::string>{"Host a game", "Join a game", "Players", "Let in", "Leave (not in a session)"});

    SessionView host;
    host.s.hosting = host.s.in_one = true;
    host.players = 1;
    CHECK(RowTexts(host) ==
          std::vector<std::string>{"Hosting", "Room: coming...", "Players: just you", "Let in: nobody asks", "Stop hosting"});
    host.room = "forest-gang";
    host.players = 3;
    host.asking = {"Saria"};
    CHECK(RowTexts(host)[kRowJoinCode] == "Room: forest-gang");
    CHECK(RowTexts(host)[kRowPlayers] == "Players: 3");
    CHECK(RowTexts(host)[kRowLetIn] == "Let in Saria");
    host.asking = {"Saria", "Mido", "Darunia"};
    CHECK(RowTexts(host)[kRowLetIn] == "Let in Saria (+2)");

    SessionView guest;
    guest.s.in_one = true;
    guest.players = 2;
    CHECK(RowTexts(guest) == std::vector<std::string>{"In a session", "Room", "Players: 2", "Let in", "Leave"});
    guest.confirm_leave = true;
    CHECK(RowTexts(guest)[kRowLeave] == "A again: leave");
    host.confirm_leave = true;
    CHECK(RowTexts(host)[kRowLeave] == "A again: stop hosting");
    host.confirm_leave = false;
    SessionView joining;
    joining.connecting = true;
    joining.s.in_one = true;
    CHECK(RowTexts(joining)[kRowSession] == "Connecting...");

    // A long room name is cut at a UTF-8 character's start, with "...".
    host.room = std::string(30, 'a') + "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9";
    const std::string cut = RowTexts(host)[kRowJoinCode];
    CHECK(cut.size() <= kRowChars);
    CHECK(cut.substr(cut.size() - 3) == "...");
    CHECK((static_cast<unsigned char>(cut[cut.size() - 4]) & 0xc0) != 0xc0);   // (no lead byte left alone)
    for (const SessionView& v : {idle, host, guest, joining}) {
        for (const std::string& r : RowTexts(v)) CHECK(r.size() <= kRowChars);
    }
}

TEST_CASE("pause tab: a page whose panes or game calls are missing never shows, and the mouse keeps tab 5 out") {
    // (an old or mismatched pack, a module without the scratch API: the page would be undrawn, so System's A and
    // its focused button must stay System's; Act would otherwise run row 0 Host or row 4 Leave on System's buttons)
    std::uint64_t now = 1000;
    Page p;   // (not usable until the module says so)
    CHECK(p.MousePage() == -2);
    OnSystem(p, now);
    CHECK(p.OnPad(kPadR, now) == kPadR);   // (the game's R: nothing of ours)
    CHECK_FALSE(p.Shown());
    p.OnPad(0, now + 1);
    CHECK(p.OnPad(kPadA, now + 2) == kPadA);   // (System's A)
    CHECK_FALSE(p.TakeDecide());
    p.OnPad(0, now + 3);
    // Usable: tab 5 counts for the mouse, R shows the page.
    p.SetUsable(true);
    CHECK(p.MousePage() == -1);
    OnSystem(p, now + 4);
    CHECK(p.OnPad(kPadR, now + 4) == 0);
    CHECK(p.Shown());
    CHECK(p.MousePage() == 4);
    // Its panes lost while it shows (a new screen whose look-up failed): System back at once, A not ours.
    p.OnPad(0, now + 5);
    p.SetUsable(false);
    CHECK_FALSE(p.Shown());
    CHECK(p.TakeWhy() == "its panes or the game's calls are not there");
    CHECK(p.MousePage() == -2);
    CHECK(p.OnPad(kPadA, now + 6) == kPadA);
    CHECK_FALSE(p.TakeDecide());
}

TEST_CASE("pause tab: a screen reopened at the same address, or with a new layout, is a new screen") {
    std::uint64_t now = 1000;
    Page p = UsablePage();
    const std::uint64_t g1 = p.Track(Pages(0xa000, kSystemPage, kSystemPage, 1));
    CHECK(g1 != 0);
    CHECK(p.Track(Pages(0xa000, kSystemPage, kSystemPage, 1)) == g1);   // (still opening)
    CHECK(p.Track(Pages(0xa000, kSystemPage, kSystemPage, 2)) == g1);   // (open)
    auto d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now);
    CHECK(d.set_texts);   // (once per opening: its first open update)
    CHECK_FALSE(p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage), now + 1).set_texts);
    // Closed and opened again at the same address: its texts and the page's state are set again.
    p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage, 3), now + 10);
    p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage, 0), now + 20);
    const std::uint64_t g2 = p.Track(Pages(0xa000, kSystemPage, kSystemPage, 1));
    CHECK(g2 != g1);
    d = p.OnScreen(Pages(0xa000, kSystemPage, kSystemPage, 2), now + 30);
    CHECK(d.set_texts);
    CHECK(d.set_page);
    // A new layout at the same address: a new screen too.
    Facts f = Pages(0xa000, kSystemPage, kSystemPage, 2);
    f.layout = 0x7000;
    CHECK(p.Track(f) != g2);
    CHECK(p.OnScreen(f, now + 40).set_texts);
    // The strip the same way: its fifth tab set again.
    const std::uint64_t s1 = p.Track(Strip(0x9000, 5, kSystemPage, kPlusMode, 1));
    CHECK(s1 != 0);
    CHECK(p.OnScreen(Strip(0x9000, 5, kSystemPage), now + 50).set_tabs);
    CHECK_FALSE(p.OnScreen(Strip(0x9000, 5, kSystemPage), now + 51).set_tabs);
    p.OnScreen(Strip(0x9000, 5, kSystemPage, kPlusMode, 0), now + 60);
    CHECK(p.Track(Strip(0x9000, 5, kSystemPage, kPlusMode, 1)) != s1);
    CHECK(p.OnScreen(Strip(0x9000, 5, kSystemPage), now + 70).set_tabs);
}

TEST_CASE("pause tab: co-op that came with the game's start, and a failed Host, open co-op's main page") {
    // (the launcher rule, as co-op's menu: Host.bat / Join.bat / coop.ini co-op is not started or stopped here)
    Session launcher;
    launcher.by_launcher = launcher.in_one = true;
    CHECK(ActionFor(kRowSession, launcher) == RowAction::kOpenMain);
    CHECK(ActionFor(kRowLeave, launcher) == RowAction::kOpenMain);   // (never two A's to a teardown)
    launcher.in_one = false;   // (not connected yet: still not Host)
    CHECK(ActionFor(kRowSession, launcher) == RowAction::kOpenMain);
    CHECK(ActionFor(kRowLeave, launcher) == RowAction::kOpenMain);
    SessionView lv;
    lv.s = launcher;
    CHECK(RowTexts(lv)[kRowLeave] == "Leave: start the game again");
    // A Host (or Join) that failed: the row says so, and A opens the main page with the session's lines.
    Session failed;
    failed.failed = true;
    CHECK(ActionFor(kRowSession, failed) == RowAction::kOpenMain);
    SessionView fv;
    fv.s = failed;
    CHECK(RowTexts(fv)[kRowSession] == "Co-op failed: A says why");
    CHECK(RowTexts(fv)[kRowSession].size() <= kRowChars);
}

TEST_CASE("pause tab: the Leave row's second A is waited for only while the cursor stays on it") {
    CHECK(KeepConfirm(true, kRowLeave));
    CHECK_FALSE(KeepConfirm(true, kRowLetIn));
    CHECK_FALSE(KeepConfirm(true, -1));
    CHECK_FALSE(KeepConfirm(false, kRowLeave));   // (System back: no second A waited for)
}

TEST_CASE("pause tab: the rows' own views: Players, Let in and Join on the same five rows, Back last") {
    Session idle;
    Session host;
    host.hosting = host.in_one = host.code = true;
    // Players: who is in, one a row; only Back acts.
    SessionView pv;
    pv.s.in_one = true;
    pv.names = {"dauro (you)", "Saria (host)"};
    CHECK(RowTexts(View::kPlayers, pv) == std::vector<std::string>{"dauro (you)", "Saria (host)", " ", " ", "Back"});
    pv.names = {"a", "b", "c", "d", "e", "f"};
    CHECK(RowTexts(View::kPlayers, pv)[3] == "+3 more (Lobby tab)");
    SessionView none;
    CHECK(RowTexts(View::kPlayers, none)[0] == "Not in a session");
    for (int r = 0; r < kRows - 1; ++r) CHECK(ActionFor(View::kPlayers, r, host) == RowAction::kNone);
    CHECK(ActionFor(View::kPlayers, kRows - 1, host) == RowAction::kBack);
    // Let in: the oldest who asks, Let in (/allow) or Refuse (/deny); nobody: nothing to press.
    CHECK(ActionFor(View::kLetIn, 0, host) == RowAction::kNone);
    host.asking = true;
    CHECK(ActionFor(View::kLetIn, 0, host) == RowAction::kAllow);
    CHECK(ActionFor(View::kLetIn, 1, host) == RowAction::kDeny);
    CHECK(ActionFor(View::kLetIn, 2, host) == RowAction::kNone);
    CHECK(ActionFor(View::kLetIn, kRows - 1, host) == RowAction::kBack);
    SessionView lv;
    lv.s = host;
    CHECK(RowTexts(View::kLetIn, lv)[0] == "Nobody asks to join");
    lv.asking = {"Mido", "Darunia"};
    CHECK(RowTexts(View::kLetIn, lv) == std::vector<std::string>{"Let in Mido", "Refuse Mido", "+1 more ask", " ", "Back"});
    // Join: the clipboard's code, the last game again, typing in co-op's menu; never from inside a session.
    CHECK(ActionFor(View::kJoin, 0, idle) == RowAction::kJoinPaste);
    CHECK(ActionFor(View::kJoin, 1, idle) == RowAction::kNone);   // (no last game kept)
    Session again = idle;
    again.rejoin = true;
    CHECK(ActionFor(View::kJoin, 1, again) == RowAction::kRejoin);
    CHECK(ActionFor(View::kJoin, 2, idle) == RowAction::kOpenJoin);
    CHECK(ActionFor(View::kJoin, kRows - 1, idle) == RowAction::kBack);
    Session in_one;
    in_one.in_one = in_one.rejoin = true;
    CHECK(ActionFor(View::kJoin, 0, in_one) == RowAction::kNone);
    CHECK(ActionFor(View::kJoin, 1, in_one) == RowAction::kNone);
    Session launcher;
    launcher.by_launcher = launcher.rejoin = true;
    CHECK(ActionFor(View::kJoin, 0, launcher) == RowAction::kNone);
    SessionView jv;
    jv.s = again;
    jv.rejoin_host = "Saria";
    CHECK(RowTexts(View::kJoin, jv) ==
          std::vector<std::string>{"Paste code and join", "Join Saria again", "Type a code (co-op's menu)", " ", "Back"});
    // The main view is the five rows; out of range is nothing.
    CHECK(ActionFor(View::kMain, kRowPlayers, idle) == ActionFor(kRowPlayers, idle));
    CHECK(RowTexts(View::kMain, none) == RowTexts(none));
    CHECK(ActionFor(View::kJoin, -1, idle) == RowAction::kNone);
    CHECK(ActionFor(View::kLetIn, kRows, host) == RowAction::kNone);
    for (View w : {View::kPlayers, View::kLetIn, View::kJoin}) {
        for (const std::string& r : RowTexts(w, lv)) CHECK((!r.empty() && r.size() <= kRowChars));
    }
    for (RowAction a : {RowAction::kViewPlayers, RowAction::kViewLetIn, RowAction::kViewJoin, RowAction::kBack,
                        RowAction::kDeny, RowAction::kJoinPaste, RowAction::kRejoin}) {
        CHECK(std::string(ActionWords(a)).size() > 3);
    }
}

TEST_CASE("pause tab: in a view, B goes back to the five rows instead of closing the menu") {
    std::uint64_t now = 1000;
    Page p = UsablePage();
    OnSystem(p, now);
    REQUIRE(p.OnPad(kPadR, now) == 0);
    p.OnPad(0, now + 1);
    CHECK(p.OnPad(kPadB, now + 2) == kPadB);   // (the five rows: B is the game's, it closes the menu)
    CHECK_FALSE(p.TakeBack());
    p.OnPad(0, now + 3);
    p.SetSubView(true);
    CHECK(p.OnPad(kPadB, now + 4) == 0);   // (a view: ours)
    CHECK(p.TakeBack());
    CHECK_FALSE(p.TakeBack());
    CHECK(p.OnPad(kPadB, now + 5) == 0);   // (held: still ours)
    p.SetSubView(false);
    CHECK(p.OnPad(kPadB, now + 6) == 0);   // (still held from the view: not a fresh B for the game)
    p.OnPad(0, now + 7);
    CHECK(p.OnPad(kPadB, now + 8) == kPadB);
    // The page going ends the view.
    p.OnPad(0, now + 9);
    p.SetSubView(true);
    CHECK(p.OnPad(kPadL, now + 10) == 0);
    CHECK_FALSE(p.Shown());
    p.OnPad(0, now + 11);
    CHECK(p.OnPad(kPadB, now + 12) == kPadB);
    CHECK_FALSE(p.TakeBack());
    p.SetSubView(true);   // (not shown: no view)
    CHECK(p.OnPad(kPadB, now + 13) == kPadB);
}

TEST_CASE("pause tab: a screen update nested on the same thread is told so, other threads are not") {
    {
        const ScreenGuard outer;
        CHECK_FALSE(outer.Nested());
        {
            const ScreenGuard inner;   // (a game call of ours ran a screen update again)
            CHECK(inner.Nested());
        }
        const ScreenGuard again;
        CHECK(again.Nested());   // (the inner one ending leaves the outer one in place)
        bool other = true;
        std::thread t([&] {
            const ScreenGuard g;
            other = g.Nested();
        });
        t.join();
        CHECK_FALSE(other);
    }
    const ScreenGuard after;
    CHECK_FALSE(after.Nested());
}
