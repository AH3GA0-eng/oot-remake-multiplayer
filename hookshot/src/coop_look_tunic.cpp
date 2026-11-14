// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop_look_tunic.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string_view>
#include <utility>

namespace CoopLookTunic {
namespace {

// tools/puppet_pack/look_live.py
constexpr char kMagic[8] = {'C', 'O', 'O', 'P', 'L', 'O', 'O', 'K'};
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kNameBytes = 64;
// The puppets' file holds every copy's gear (8 copies of 17 textures today).
constexpr std::size_t kMaxTextures = 4096;


template <typename T>
T Little(const std::uint8_t* p) {
    T v = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        v |= static_cast<T>(p[i]) << (8 * i);
    }
    return v;
}

void Fail(std::string* why, std::string text) {
    if (why != nullptr) {
        *why = std::move(text);
    }
}

// Memory is compared in pieces of this size.
constexpr std::size_t kPiece = std::size_t{4} << 20;

bool Same(const Textures& memory, std::uint64_t address, const std::vector<std::uint8_t>& texels, std::size_t size) {
    std::vector<std::uint8_t> piece(std::min(kPiece, size));
    for (std::size_t at = 0; at < size; at += piece.size()) {
        const std::size_t n = std::min(piece.size(), size - at);
        if (!memory.Read(address + at, piece.data(), n) || std::memcmp(piece.data(), texels.data() + at, n) != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<Texels> Parse(const std::vector<std::uint8_t>& file, std::string* why) {
    if (file.size() < 16 || std::memcmp(file.data(), kMagic, sizeof kMagic) != 0) {
        Fail(why, "not a look file");
        return std::nullopt;
    }
    if (const auto version = Little<std::uint32_t>(file.data() + 8); version != kVersion) {
        Fail(why, "look file version " + std::to_string(version) + ", this build reads " + std::to_string(kVersion));
        return std::nullopt;
    }
    const auto count = Little<std::uint32_t>(file.data() + 12);
    if (count == 0 || count > kMaxTextures) {
        Fail(why, "a look file of " + std::to_string(count) + " textures");
        return std::nullopt;
    }
    Texels out;
    std::size_t at = 16;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (file.size() - at < kNameBytes + 16) {
            Fail(why, "the look file is cut short");
            return std::nullopt;
        }
        Texture t;
        const char* name = reinterpret_cast<const char*>(file.data() + at);
        t.name.assign(name, std::find(name, name + kNameBytes, '\0'));
        t.offset = Little<std::uint64_t>(file.data() + at + kNameBytes);
        const auto size = Little<std::uint64_t>(file.data() + at + kNameBytes + 8);
        at += kNameBytes + 16;
        if (size > file.size() - at) {
            Fail(why, "the look file is cut short");
            return std::nullopt;
        }
        if (t.name.empty() || t.offset < 16 || t.offset > kMaxOffset || size < kHead) {
            Fail(why, "texture " + std::to_string(i) + " of the look file cannot be placed");
            return std::nullopt;
        }
        t.texels.assign(file.begin() + static_cast<std::ptrdiff_t>(at),
                        file.begin() + static_cast<std::ptrdiff_t>(at + size));
        at += static_cast<std::size_t>(size);
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<std::uint8_t> Pack(const Texels& texels) {
    std::vector<std::uint8_t> out(kMagic, kMagic + sizeof kMagic);
    auto put = [&](std::uint64_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    };
    put(kVersion, 4);
    put(texels.size(), 4);
    for (const Texture& t : texels) {
        std::string name = t.name.substr(0, kNameBytes - 1);
        name.resize(kNameBytes, '\0');
        out.insert(out.end(), name.begin(), name.end());
        put(t.offset, 8);
        put(t.texels.size(), 8);
        out.insert(out.end(), t.texels.begin(), t.texels.end());
    }
    return out;
}

bool SameLayout(const Texels& a, const Texels& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const Texture& x, const Texture& y) {
        return x.name == y.name;
    });
}

Search Find(const Textures& textures, const std::vector<std::shared_ptr<const Texels>>& colours) {
    Search out;
    std::uint64_t smallest = ~std::uint64_t{0}, largest = 0;
    for (const auto& colour : colours) {
        for (std::size_t i = 0; colour && i < colour->size(); ++i) {
            smallest = std::min<std::uint64_t>(smallest, (*colour)[i].texels.size());
            largest = std::max<std::uint64_t>(largest, (*colour)[i].texels.size());
        }
    }
    if (largest == 0) {
        return out;
    }
    // A storage is the image's own size: up to kFilePad shorter than the pack's
    // texels (the file's padding), or its layout rounded up a little.
    const std::uint64_t low = smallest > kFilePad ? smallest - kFilePad : 1;
    const std::vector<Live> listed = textures.List(low, largest + kStorageSlack, nullptr);
    out.listed = listed.size();
    for (const Live& live : listed) {
        std::uint8_t head[kHead];
        if (!textures.Read(live.address, head, kHead)) {
            ++out.unread;
            continue;
        }
        // The game copies each texture's texels into its texture memory (no
        // file, no name in front: lane1, 2026-11-14), so a texture is known by
        // its texels alone. The puppets' copies of the same texels are marked
        // (tools/puppet_pack/look_live.py mark): only your own hold a colour's
        // texels exactly. The first colour that holds them is the one held.
        bool taken = false, head_only = false;
        for (const auto& colour : colours) {
            for (std::size_t i = 0; !taken && colour && i < colour->size(); ++i) {
                const Texture& texture = (*colour)[i];
                const std::size_t size =
                    static_cast<std::size_t>(std::min<std::uint64_t>(texture.texels.size(), live.size));
                if (texture.texels.size() > live.size + kFilePad || std::memcmp(head, texture.texels.data(), kHead) != 0) {
                    continue;
                }
                if (!Same(textures, live.address, texture.texels, size)) {
                    head_only = true;
                    continue;
                }
                taken = true;
                out.places.push_back({i, live.address, live.serial, size, colour, colour, false});
            }
            if (taken) {
                break;
            }
        }
        if (head_only && !taken) {
            ++out.heads;
        }
    }
    return out;
}

bool Draw(Textures& textures, Place& place, const std::shared_ptr<const Texels>& colour) {
    const Texture& held = (*place.held)[place.texture];
    const Texture& to = (*colour)[place.texture];
    const std::size_t n = static_cast<std::size_t>(place.size);
    if (to.texels.size() != held.texels.size() || n > to.texels.size()) {
        return false;
    }
    if (std::equal(to.texels.begin(), to.texels.begin() + static_cast<std::ptrdiff_t>(n), held.texels.begin())) {
        // the game's memory holds it: no override
        if (place.overridden && !textures.Override(place.address, place.serial, nullptr, n)) {
            return false;
        }
        place.overridden = false;
    } else {
        if (!textures.Override(place.address, place.serial, to.texels.data(), n)) {
            return false;
        }
        place.overridden = true;
    }
    place.drawn = colour;
    return true;
}

void Wardrobe::Wear(std::string colour) {
    std::lock_guard lock(mutex_);
    if (colour != host_.base) {
        busy_ = true;
    }
    wanted_ = std::move(colour);
}

void Wardrobe::Kick() {
    std::lock_guard lock(mutex_);
    kicked_ = true;
}

bool Wardrobe::Busy() const {
    std::lock_guard lock(mutex_);
    return busy_;
}

void Wardrobe::Clear() {
    for (Place& p : places_) {
        if (p.overridden) {
            host_.textures->Override(p.address, p.serial, nullptr, static_cast<std::size_t>(p.size));
            p.overridden = false;
            p.drawn = p.held;
        }
    }
    places_.clear();
    looked_ = false;
    target_name_.clear();
    asked_.clear();
}

std::shared_ptr<const Texels> Wardrobe::Load(const std::string& colour) {
    const auto file = host_.file(colour);
    if (!file) {
        host_.log("coop: look: the puppet pack has no coop_look/" + colour +
                  ".bin: your own Link keeps the colour it has (make the puppet pack again; the others see your "
                  "new colour all the same)");
        return nullptr;
    }
    std::string why;
    auto texels = Parse(*file, &why);
    if (!texels) {
        host_.log("coop: look: coop_look/" + colour + ".bin of the puppet pack is not read (" + why +
                  "): your own Link keeps the colour it has");
        return nullptr;
    }
    return std::make_shared<const Texels>(std::move(*texels));
}

void Wardrobe::Step(std::uint64_t now_ms) {
    (void)now_ms;
    std::string want;
    {
        std::lock_guard lock(mutex_);
        if (!busy_) {
            return;
        }
        want = wanted_;
        if (std::exchange(kicked_, false)) {
            look_now_ = true;   // the equipment may have changed
        }
    }
    if (want != asked_) {
        asked_ = want;
        look_now_ = true;
        said_nothing_found_ = false;
    }
    if (target_name_ != want && refused_ != want) {
        if (!green_) {
            green_ = Load(host_.base);
            if (green_ && !host_.start.empty() && host_.start != host_.base) {
                start_ = Load(host_.start);
                if (start_ && !SameLayout(*green_, *start_)) {
                    start_ = nullptr;
                }
            }
        }
        auto target = !green_ ? nullptr : want == host_.base ? green_ : start_ && want == host_.start ? start_ : Load(want);
        if (target && !SameLayout(*green_, *target)) {
            host_.log("coop: look: coop_look/" + want + ".bin does not fit the pack's " + host_.base + ".bin: your own Link keeps "
                      "the colour it has (make the puppet pack again)");
            target = nullptr;
        }
        if (target) {
            target_ = std::move(target);
            target_name_ = want;
        } else {
            refused_ = want;   // said once; the colour worn before stays
        }
    }
    if (!target_) {
        return;
    }

    // The game's textures, again when it made or finalized one (a scene
    // change, an age change, a new shield) or the pause screen closed.
    std::uint64_t generation = 0;
    host_.textures->List(1, 0, &generation);   // (no storage is that size: the generation only)
    if (look_now_ || !looked_ || generation != generation_) {
        // What the game's memory may hold: the start's colour first (the kit put
        // its files in), then the game's own, then the one asked for.
        std::vector<std::shared_ptr<const Texels>> colours;
        for (const auto& c : {start_, green_, target_}) {
            if (c && std::find(colours.begin(), colours.end(), c) == colours.end()) {
                colours.push_back(c);
            }
        }
        Search found = Find(*host_.textures, colours);
        for (Place& p : found.places) {   // a texture kept since: drawn as it was
            const auto old = std::find_if(places_.begin(), places_.end(), [&](const Place& o) {
                return o.address == p.address && o.serial == p.serial && o.texture == p.texture;
            });
            if (old != places_.end() && old->held == p.held) {
                p.drawn = old->drawn, p.overridden = old->overridden;
            }
        }
        places_ = std::move(found.places);   // (a texture finalized took its override with it)
        look_now_ = false, looked_ = true;
        generation_ = generation;
        if (places_.empty()) {
            std::string line = "coop: look: no " + host_.what + " texture of your Link among the game's yet (" +
                               std::to_string(found.listed) + " of the game's of those sizes, " +
                               std::to_string(found.unread) + " not readable, " + std::to_string(found.heads) +
                               " with only the first bytes the same)";
            // (said again when it changes, at most 20 times: the game makes its textures over a while)
            if ((!said_nothing_found_ || line != said_) && said_count_ < 20) {
                ++said_count_;
                host_.log(line);
            }
            said_nothing_found_ = true;
            said_ = std::move(line);
        }
    }
    std::size_t drawn = 0;
    for (Place& p : places_) {
        if (p.drawn != target_ && Draw(*host_.textures, p, target_)) {
            ++drawn;
        }
    }
    if (drawn != 0) {
        said_nothing_found_ = false;
        host_.log("coop: look: your Link's " + host_.what + " is " + target_name_ + " now (" + std::to_string(drawn) +
                  " texture(s) drawn so by Longshot's texture overrides; the game's memory is left as it is)");
    }
    if (want == (start_ ? host_.start : host_.base) && target_ == (start_ ? start_ : green_) &&
        std::none_of(places_.begin(), places_.end(), [](const Place& p) { return p.overridden; })) {
        // Everything is the game's own again: nothing to keep.
        std::lock_guard lock(mutex_);
        if (wanted_ == (start_ ? host_.start : host_.base)) {
            busy_ = false;
            places_.clear();
            looked_ = false;
        }
    }
}

}  // namespace CoopLookTunic
