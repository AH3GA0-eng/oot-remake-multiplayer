// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_map_refresh.h"

namespace CoopMapRefresh {

void Refresher::Want(std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    due_ms_ = now_ms + kDelayMs;  // a later arrival moves it: one refresh after the last
}

void Refresher::SetInPlay(bool in_play) {
    std::lock_guard lock(mutex_);
    in_play_ = in_play;
}

bool Refresher::Due(std::uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    if (due_ms_ == 0 || now_ms < due_ms_ || !in_play_) return false;
    due_ms_ = 0;
    return true;
}

Refresher& TheRefresher() {
    static Refresher refresher;
    return refresher;
}

}  // namespace CoopMapRefresh
