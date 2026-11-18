// SPDX-License-Identifier: GPL-3.0-or-later
// coop_pause_tab.h, the module's part: patch 0030's call for every menu
// screen's update, the game's fields read, the probe's lines logged. Only in
// hookshot_coop.dll (the tests drive Probe itself).
#include "coop_pause_tab.h"

#include <chrono>
#include <mutex>

#include "common/logging.h"
#include "coop_runtime.h"

namespace CoopPauseTab {

namespace {

std::mutex g_mutex;   // the probe: the screens' updates run on several of the game's threads at once
Probe g_probe;

template <class T>
T Read(std::uint64_t a) {
    T v{};
    return CoopRuntime::Read(a, &v, sizeof v) ? v : T{};
}

std::uint64_t NowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

}  // namespace

void OnScreen(void* ctx, std::uint64_t screen) noexcept {
    (void)ctx;   // (the page step's game calls will use it)
    try {
        if (screen == 0) {
            return;
        }
        const std::uint64_t base = CoopRuntime::MainBase();
        const std::uint64_t vtable = Read<std::uint64_t>(screen);
        if (vtable < base) {
            return;
        }
        Facts f;
        f.kind = KindOf(vtable - base);
        if (f.kind == Kind::kOther) {
            return;   // (most updates: the HUD's and the other screens', once a frame each)
        }
        f.screen = screen;
        f.state = Read<std::uint8_t>(screen + kScreenState);
        if (f.kind == Kind::kStrip) {
            f.tabs = static_cast<int>(Read<std::uint32_t>(screen + kStripTabs));
            f.mode = Read<std::uint8_t>(screen + kStripMode);
            f.lit = Read<std::int32_t>(screen + kStripLit);
        } else if (const std::uint64_t manager = Read<std::uint64_t>(base + kMenuManager); manager != 0) {
            f.page = Read<std::int32_t>(manager + kManagerPage);
            f.asked = Read<std::int32_t>(manager + kManagerAsked);
        }
        std::vector<std::string> lines;
        {
            std::lock_guard lock(g_mutex);
            lines = g_probe.Seen(f, NowMs());
        }
        for (const std::string& line : lines) {
            LOG_INFO(Core, "{}", line);
        }
    } catch (...) {
        // (a log line lost is all: the game's update goes on)
    }
}

}  // namespace CoopPauseTab
