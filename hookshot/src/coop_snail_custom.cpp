// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_snail_custom.h"

namespace CoopSnailCustom {

int Neighbour(int f, Dir dir) {
    if (f < 0 || f >= kOptions) return -1;
    const int side = f / kRows, i = f % kRows;
    int to = -1;
    switch (dir) {
    case Dir::kUp: to = i > 0 ? f - 1 : -1; break;
    case Dir::kDown: to = i < kRows - 1 ? f + 1 : -1; break;
    case Dir::kLeft: to = side > 0 ? f - kRows : -1; break;
    case Dir::kRight: to = f + kRows; break;
    }
    return to >= 0 && to < kOptions ? to : -1;
}

void Driver::Open(const CoopLook::Look& look, std::uint16_t character) {
    page_.SetPaged(true);
    page_.SetCharacter(character, false);
    page_.Open(look);
    focus_ = 0;   // (his screen opens on its first option)
    active_ = true;
}

std::uint64_t Driver::Pad(std::uint64_t down) {
    if (!active_) return down;
    std::uint64_t his = 0;
    // (the d-pad and the stick: his graph moves his marker; we follow his rule)
    const struct {
        std::uint64_t bits;
        Dir dir;
    } moves[] = {{kUp | kStickUp, Dir::kUp}, {kDown | kStickDown, Dir::kDown},
                 {kLeft | kStickLeft, Dir::kLeft}, {kRight | kStickRight, Dir::kRight}};
    for (const auto& m : moves) {
        if ((down & m.bits) == 0) continue;
        his |= down & m.bits;
        if (const int to = Neighbour(focus_, m.dir); to >= 0) focus_ = to;
    }
    const std::size_t row = static_cast<std::size_t>(focus_ + 1);   // (the page's rows: its heading, then the six)
    const bool has = row < page_.view().rows.size();   // (a page of fewer: his option past them does nothing)
    if (has && (down & (kZL | kZR)) != 0) {
        page_.Select(row);
        page_.Press((down & kZR) != 0 ? CoopLookMenu::Input::kRight : CoopLookMenu::Input::kLeft);
    }
    if (has && (down & kA) != 0) {
        page_.Select(row);
        page_.Press(CoopLookMenu::Input::kDecide);
    }
    if ((down & kY) != 0) page_.Press(CoopLookMenu::Input::kPageNext);
    if ((down & kB) != 0) {
        if (page_.editing()) {
            page_.Press(CoopLookMenu::Input::kBack);   // (the mixer: back to the parts, his screen stays)
        } else {
            his |= kB;   // (his: back to his picker)
        }
    }
    if ((down & kMinus) != 0 && !page_.editing()) his |= kMinus;
    return his;
}

Texts Driver::Shown() const {
    const CoopLookMenu::View v = page_.view();
    Texts t;
    if (v.rows.empty()) return t;
    t.title = v.rows[0].name;
    if (!page_.editing() && page_.GroupsOffered() > 1) t.title += "   Y: next page";
    for (int f = 0; f < kOptions; ++f) {
        const std::size_t r = static_cast<std::size_t>(f + 1);
        if (r >= v.rows.size()) break;
        t.labels[static_cast<std::size_t>(f)] = v.rows[r].name;
        // (an action row, Game's own colour or Done, has no value of its own: its key)
        t.values[static_cast<std::size_t>(f)] = v.rows[r].kind == 0 ? std::string("A") : v.rows[r].value;
    }
    return t;
}

}  // namespace CoopSnailCustom
