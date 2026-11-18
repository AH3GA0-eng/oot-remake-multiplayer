// SPDX-License-Identifier: GPL-3.0-or-later
// coop_pause_tab.h's logic: no game reads, no log (the tests drive it).
#include "coop_pause_tab.h"

#include <cstdio>

namespace CoopPauseTab {

namespace {

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
                      f.mode == 0 ? "the Plus" : "the Minus", f.lit, f.state);
    } else {
        std::snprintf(b, sizeof b, "page %d, asked %d, state %d", f.page, f.asked, f.state);
    }
    return b;
}

}  // namespace

Kind KindOf(std::uint64_t vtable_offset) {
    return vtable_offset == kPagesVtable ? Kind::kPages : vtable_offset == kStripVtable ? Kind::kStrip : Kind::kOther;
}

std::vector<std::string> Probe::Seen(const Facts& f, std::uint64_t now_ms) {
    std::vector<std::string> out;
    if (f.kind == Kind::kOther || f.screen == 0) {
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

int MousePage() {
    return -1;
}

}  // namespace CoopPauseTab
