// SPDX-License-Identifier: GPL-3.0-or-later
// coop_pause_tab.h's logic: no game reads or calls, no log (the tests drive it).
#include "coop_pause_tab.h"

#include <cstdio>

namespace CoopPauseTab {

namespace {

// A screen whose updates stopped this long ago is taken as gone (the game updates an open menu once a frame).
constexpr std::uint64_t kStaleMs = 250;
// A window over the page keeps it away this long after its last opening, open or closing update.
constexpr std::uint64_t kBlockMs = 250;

std::string Hex(std::uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "%#llx", static_cast<unsigned long long>(v));
    return b;
}

// What a screen shows, as one line (the part that is compared).
std::string Shows(const Facts& f) {
    char b[160];
    if (f.kind == Kind::kStrip) {
        std::snprintf(b, sizeof b, "%d tabs, %s menu, lights category %d, state %d", f.tabs,
                      f.mode == kPlusMode ? "the Plus" : "the Minus", f.lit, f.state);
    } else {
        std::snprintf(b, sizeof b, "page %d, asked %d, state %d", f.page, f.asked, f.state);
    }
    return b;
}

}  // namespace

Kind KindOf(std::uint64_t vtable_offset) {
    if (vtable_offset == kPagesVtable) return Kind::kPages;
    if (vtable_offset == kStripVtable) return Kind::kStrip;
    for (const std::uint64_t v : kBlockerVtables) {
        if (vtable_offset == v) return Kind::kBlocker;
    }
    return Kind::kOther;
}

// --- Page ---

bool Page::CanShow(std::uint64_t now_ms) const {
    return usable_ && pages_.screen != 0 && strip_.screen != 0 && now_ms - pages_ms_ < kStaleMs && now_ms - strip_ms_ < kStaleMs &&
           pages_.state == 2 && pages_.page == kSystemPage && pages_.asked == kSystemPage && strip_.state == 2 &&
           strip_.mode == kPlusMode && strip_.tabs == 5 && now_ms >= blocked_until_ms_;
}

void Page::Hide(const char* why) {
    if (!shown_) {
        return;
    }
    shown_ = false;
    sub_ = false, back_ = false;
    why_ = why;
    // Every button held now stays the module's until let go, so none reaches System as a fresh press (an A held
    // as the page goes would otherwise be the System page's A).
    swallow_ |= pad_last_ & ~kPadPassed;
}

std::uint64_t Page::Track(const Facts& f) {
    if (f.kind != Kind::kPages && f.kind != Kind::kStrip) return 0;
    Seen& s = f.kind == Kind::kPages ? pages_seen_ : strip_seen_;
    const bool reopened = f.state == 1 && s.state != 1;
    if (s.gen == 0 || f.screen != s.screen || f.layout != s.layout || reopened) {
        s.screen = f.screen, s.layout = f.layout, s.gen = ++next_gen_;
        if (f.kind == Kind::kPages) {
            page_set_ = -1, texts_set_ = false;
        } else {
            tab_key_ = -1;
        }
    }
    s.state = f.state;
    return s.gen;
}

void Page::SetUsable(bool usable) {
    usable_ = usable;
    if (!usable_) Hide("its panes or the game's calls are not there");
}

Page::Do Page::OnScreen(const Facts& f, std::uint64_t now_ms) {
    Do d;
    Track(f);
    switch (f.kind) {
    case Kind::kBlocker:
        if (f.state >= 1 && f.state <= 3) {
            blocked_until_ms_ = now_ms + kBlockMs;
            Hide("a window opened over it");
        }
        return d;
    case Kind::kPages: {
        pages_ = f, pages_ms_ = now_ms;
        if (shown_ && !CanShow(now_ms)) {
            Hide(f.state != 2 ? "the menu is closing" : "the menu left the System page");
        }
        const int want = shown_ ? 1 : 0;
        if (page_set_ != want) {
            d.set_page = true, d.page_shown = shown_, page_set_ = want;
        }
        if (!texts_set_ && f.state == 2) {
            d.set_texts = true, texts_set_ = true;
        }
        return d;
    }
    case Kind::kStrip: {
        strip_ = f, strip_ms_ = now_ms;
        if (shown_ && !CanShow(now_ms)) {
            Hide(f.mode != kPlusMode ? "the Minus menu" : "the tab strip is closing");
        }
        const bool visible = f.tabs == 5 && f.mode == kPlusMode && f.state == 2;
        const int key = (visible ? 1 : 0) | (shown_ ? 2 : 0) | ((f.lit & 0xff) << 2) | ((f.state & 0xf) << 10) |
                        ((f.mode & 0xf) << 14);
        if (f.tabs == 5 && key != tab_key_) {
            tab_key_ = key;
            d.set_tabs = true, d.tab_shown = visible, d.tab_lit = visible && shown_, d.fill_tab = visible;
        }
        return d;
    }
    case Kind::kOther:
        break;
    }
    return d;
}

std::uint64_t Page::OnPad(std::uint64_t buttons, std::uint64_t now_ms) {
    const std::uint64_t down = buttons & ~pad_last_;
    pad_last_ = buttons;
    swallow_ &= buttons;   // (let go: the game gets it again)
    if (shown_ && !CanShow(now_ms)) {
        Hide("the menu's state changed");
    }
    if (!shown_) {
        if ((down & (kPadR | kPadZR)) != 0 && CanShow(now_ms)) {
            shown_ = true, decide_ = false, why_.clear();
            swallow_ |= buttons & ~kPadPassed;   // (R and anything held with it: none of it reaches System)
            return buttons & kPadPassed & ~swallow_;
        }
        return buttons & ~swallow_;
    }
    if ((down & (kPadL | kPadZL)) != 0) {
        Hide("L");
        return buttons & ~swallow_;
    }
    if ((down & kPadA) != 0) {
        decide_ = true;
    }
    if (sub_ && (down & kPadB) != 0) {   // (a view: B goes back to the five rows, kept until let go)
        back_ = true;
        swallow_ |= kPadB;
    }
    return buttons & kPadPassed & ~swallow_;
}

bool Page::TakeBack() {
    const bool b = back_;
    back_ = false;
    return b;
}

bool Page::TakeDecide() {
    const bool d = decide_;
    decide_ = false;
    return d;
}

std::string Page::TakeWhy() {
    std::string w;
    w.swap(why_);
    return w;
}

// --- the rows ---

RowAction ActionFor(int row, const Session& s) {
    switch (row) {
    case kRowSession:
        return s.in_one || s.by_launcher || s.failed ? RowAction::kOpenMain : RowAction::kHost;
    case kRowJoinCode:
        if (s.wait_host) return RowAction::kCopyAnswer;
        if (s.hosting) return s.code ? RowAction::kCopyCode : RowAction::kOpenMain;   // (the code still coming: shown there)
        return s.in_one || s.by_launcher ? RowAction::kOpenMain : RowAction::kViewJoin;
    case kRowPlayers:
        return RowAction::kViewPlayers;
    case kRowLetIn:
        if (!s.hosting) return RowAction::kOpenLobby;
        return s.invite && !s.asking ? RowAction::kPasteAnswer : RowAction::kViewLetIn;   // (a knock first)
    case kRowLeave:
        if (s.by_launcher) return RowAction::kOpenMain;   // (not stopped here: the main page says to start the game again)
        return s.in_one ? RowAction::kConfirmLeave : RowAction::kNone;   // (a second A leaves: never one stray press)
    default:
        return RowAction::kNone;
    }
}

RowAction ActionFor(View view, int row, const Session& s) {
    if (row < 0 || row >= kRows) return RowAction::kNone;
    switch (view) {
    case View::kMain: return ActionFor(row, s);
    case View::kPlayers: return row == kRows - 1 ? RowAction::kBack : RowAction::kNone;
    case View::kLetIn:
        if (row == 0 && s.hosting && s.asking) return RowAction::kAllow;
        if (row == 1 && s.hosting && s.asking) return RowAction::kDeny;
        return row == kRows - 1 ? RowAction::kBack : RowAction::kNone;
    case View::kJoin: {
        const bool free = !s.in_one && !s.by_launcher;   // (as co-op's menu: nobody joins from inside a session)
        if (row == 0) return free ? RowAction::kJoinPaste : RowAction::kNone;
        if (row == 1) return free && s.rejoin ? RowAction::kRejoin : RowAction::kNone;
        if (row == 2) return RowAction::kOpenJoin;
        return row == kRows - 1 ? RowAction::kBack : RowAction::kNone;
    }
    }
    return RowAction::kNone;
}

namespace {

// At most kRowChars bytes, cut before a UTF-8 sequence's start, with "..." when cut.
std::string Fit(std::string t) {
    if (t.size() <= kRowChars) return t;
    std::size_t n = kRowChars - 3;
    while (n > 0 && (static_cast<unsigned char>(t[n]) & 0xc0) == 0x80) --n;
    return t.substr(0, n) + "...";
}

}  // namespace

std::vector<std::string> RowTexts(const SessionView& v) {
    std::vector<std::string> rows(kRows);
    if (v.s.failed && !v.s.in_one) {
        rows[kRowSession] = "Co-op failed: A says why";
    } else if (v.connecting) {
        rows[kRowSession] = "Connecting...";
    } else if (v.s.hosting) {
        rows[kRowSession] = "Hosting";
    } else if (v.s.in_one) {
        rows[kRowSession] = "In a session";
    } else {
        rows[kRowSession] = "Host a game";
    }
    if (v.s.wait_host) {
        rows[kRowJoinCode] = "Copy answer code";
    } else if (v.s.hosting) {
        rows[kRowJoinCode] = !v.room.empty() ? "Room: " + v.room : v.s.code ? "Copy invite code" : "Room: coming...";
    } else {
        rows[kRowJoinCode] = v.s.in_one ? "Room" : "Join a game";
    }
    rows[kRowPlayers] = v.players <= 0   ? "Players"
                        : v.players == 1 ? "Players: just you"
                                         : "Players: " + std::to_string(v.players);
    if (!v.s.hosting) {
        rows[kRowLetIn] = "Let in";
    } else if (v.asking.empty()) {
        rows[kRowLetIn] = v.s.invite ? "Paste answer code" : "Let in: nobody asks";
    } else if (v.asking.size() == 1) {
        rows[kRowLetIn] = "Let in " + v.asking.front();
    } else {
        rows[kRowLetIn] = "Let in " + v.asking.front() + " (+" + std::to_string(v.asking.size() - 1) + ")";
    }
    if (v.s.by_launcher) {
        rows[kRowLeave] = "Leave: start the game again";
    } else if (v.confirm_leave) {
        rows[kRowLeave] = v.s.hosting ? "A again: stop hosting" : "A again: leave";
    } else {
        rows[kRowLeave] = !v.s.in_one ? "Leave (not in a session)" : v.s.hosting ? "Stop hosting" : "Leave";
    }
    for (std::string& r : rows) r = Fit(std::move(r));
    return rows;
}

std::vector<std::string> RowTexts(View view, const SessionView& v) {
    if (view == View::kMain) return RowTexts(v);
    std::vector<std::string> rows(kRows);
    rows[kRows - 1] = "Back";
    switch (view) {
    case View::kMain: break;
    case View::kPlayers:
        if (v.names.empty()) {
            rows[0] = v.s.in_one ? "Players: coming..." : "Not in a session";
        }
        for (std::size_t i = 0; i < v.names.size() && i < kRows - 1; ++i) rows[i] = v.names[i];
        if (v.names.size() > kRows - 1) {   // (the last row says how many more: the Lobby tab lists all)
            rows[kRows - 2] = "+" + std::to_string(v.names.size() - (kRows - 2)) + " more (Lobby tab)";
        }
        break;
    case View::kLetIn:
        if (v.asking.empty()) {
            rows[0] = "Nobody asks to join";
        } else {
            rows[0] = "Let in " + v.asking.front();
            rows[1] = "Refuse " + v.asking.front();
            if (v.asking.size() > 1) rows[2] = "+" + std::to_string(v.asking.size() - 1) + " more ask";
        }
        break;
    case View::kJoin:
        rows[0] = v.s.by_launcher ? "Co-op came with the game" : v.s.in_one ? "In a session already" : "Paste code and join";
        if (v.s.rejoin && !v.s.in_one && !v.s.by_launcher) {
            rows[1] = v.rejoin_host.empty() ? "Join the last game again" : "Join " + v.rejoin_host + " again";
        }
        rows[2] = "Type a code (co-op's menu)";
        break;
    }
    for (std::string& r : rows) r = r.empty() ? std::string(" ") : Fit(std::move(r));
    return rows;
}

const char* ActionWords(RowAction a) {
    switch (a) {
    case RowAction::kOpenMain: return "co-op's menu, its main page";
    case RowAction::kOpenJoin: return "co-op's menu, its Join page";
    case RowAction::kCopyCode: return "the room name or join code to the clipboard, then co-op's main page";
    case RowAction::kOpenLobby: return "co-op's menu, its Lobby tab";
    case RowAction::kAllow: return "the oldest player who asks to join let in (/allow)";
    case RowAction::kHost: return "host now, with the kit's settings";
    case RowAction::kConfirmLeave: return "leave (or stop hosting) after a second A";
    case RowAction::kPasteAnswer: return "a friend's answer code from the clipboard (Paste answer), then co-op's main page";
    case RowAction::kCopyAnswer: return "this game's answer code to the clipboard (Copy answer), then co-op's main page";
    case RowAction::kViewPlayers: return "the Players view";
    case RowAction::kViewLetIn: return "the Let in view";
    case RowAction::kViewJoin: return "the Join view";
    case RowAction::kBack: return "back to the five rows";
    case RowAction::kDeny: return "the oldest player who asks to join refused (/deny)";
    case RowAction::kJoinPaste: return "join with the code on the clipboard";
    case RowAction::kRejoin: return "join the last game again";
    case RowAction::kNone: break;
    }
    return "nothing";
}

// --- ScreenGuard ---

namespace {
thread_local bool t_in_screen = false;
}  // namespace

ScreenGuard::ScreenGuard() : nested_(t_in_screen) {
    t_in_screen = true;
}

ScreenGuard::~ScreenGuard() {
    if (!nested_) t_in_screen = false;
}

// --- Probe ---

std::vector<std::string> Probe::Seen(const Facts& f, std::uint64_t now_ms) {
    std::vector<std::string> out;
    if ((f.kind != Kind::kPages && f.kind != Kind::kStrip) || f.screen == 0) {
        return out;
    }
    Last& last = f.kind == Kind::kPages ? pages_ : strip_;
    const char* what = f.kind == Kind::kPages ? "the pause menu's pages (PlusMenu_00)" : "the tab strip (MenuOverlay_00)";
    if (last.screen != f.screen) {
        last = Last{};
        last.screen = f.screen;
        last.first_ms = now_ms;
        out.push_back("coop: pause tab: " + std::string(what) + ", screen " + Hex(f.screen) +
                      ": its updates reach the module (patch 0030); " + Shows(f));
        last.facts = f;
    } else if (Shows(f) != Shows(last.facts)) {
        out.push_back("coop: pause tab: " + std::string(what) + ": " + Shows(f) + " (was " + Shows(last.facts) + ")");
        last.facts = f;
    }
    ++last.updates;
    if (!last.counted && now_ms - last.first_ms >= 1000) {
        last.counted = true;
        out.push_back("coop: pause tab: " + std::string(what) + ": " + std::to_string(last.updates) +
                      " updates in its first " + std::to_string(now_ms - last.first_ms) + " ms");
    }
    return out;
}

}  // namespace CoopPauseTab
