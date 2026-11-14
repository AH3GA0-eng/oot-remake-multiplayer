// SPDX-License-Identifier: GPL-3.0-or-later
#include "coop/overlay/map_icons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace coop::overlay::map {
namespace {

bool finite(float v) { return std::isfinite(v); }
bool finite(const Vec3& v) { return finite(v[0]) && finite(v[1]) && finite(v[2]); }
bool usable(const Box& b) { return finite(b.x) && finite(b.y) && finite(b.w) && finite(b.h) && b.w > 0 && b.h > 0; }

std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
    std::size_t j = i;
    while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') ++j;
    if (j > i) out.push_back(line.substr(i, j - i));
    i = j;
  }
  return out;
}

std::optional<float> number(std::string_view s) {
  // strtof, not from_chars: some standard libraries CI builds with have no float from_chars.
  if (s.empty() || s.size() > 32) return std::nullopt;
  const std::string text(s);
  char* end = nullptr;
  const float v = std::strtof(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !finite(v)) return std::nullopt;
  return v;
}

// "-" is a side that is not cut.
bool plane(std::string_view s, std::optional<float>& out) {
  if (s == "-") {
    out.reset();
    return true;
  }
  out = number(s);
  return out.has_value();
}

std::optional<Capture> parse_line(std::string_view line) {
  const auto w = words(line);
  if (w.size() < 8 || w[0] != "map") return std::nullopt;
  Capture c;
  c.location = std::string(w[1]);
  const auto cx = number(w[2]), cz = number(w[3]), cy = number(w[4]), h = number(w[5]);
  if (!cx || !cz || !cy || !h || *h <= 0 || (w[6] != "0" && w[6] != "1")) return std::nullopt;
  c.centre_x = *cx;
  c.centre_z = *cz;
  c.camera_y = *cy;
  c.height = *h;
  c.dungeon = w[6] == "1";
  std::string_view scenes = w[7];
  while (!scenes.empty()) {
    const auto comma = scenes.find(',');
    const auto one = scenes.substr(0, comma);
    if (!one.empty()) c.scenes.emplace_back(one);
    if (comma == std::string_view::npos) break;
    scenes.remove_prefix(comma + 1);
  }
  for (std::size_t k = 8; k < w.size(); ++k) {
    const auto colon = w[k].find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    Floor f;
    if (!plane(w[k].substr(0, colon), f.near_m) || !plane(w[k].substr(colon + 1), f.far_m)) return std::nullopt;
    c.floors.push_back(f);
  }
  if (c.floors.empty()) c.floors.push_back({});
  return c;
}

std::string_view without_era(std::string_view s) {
  for (const std::string_view era : {std::string_view("_Child"), std::string_view("_Adult")}) {
    if (s.size() > era.size() && s.substr(s.size() - era.size()) == era) return s.substr(0, s.size() - era.size());
  }
  return s;
}

// Whether `scene` is one of the places `c` is the map of.
bool in_capture(const Capture& c, std::string_view scene) {
  if (scene == c.location) return true;
  for (const auto& s : c.scenes) {
    if (s == scene) return true;
  }
  return false;
}

Icon start(const Player& p) {
  Icon icon;
  icon.number = p.number;
  icon.rgb = p.rgb & 0xFFFFFF;
  icon.alpha = std::clamp(finite(p.fade) ? p.fade : 0.0f, 0.0f, 1.0f);
  if (p.rotation) icon.angle = map_angle(*p.rotation);
  return icon;
}

}  // namespace

std::vector<Capture> parse_captures(std::string_view text, int* bad) {
  std::vector<Capture> out;
  int skipped = 0;
  while (!text.empty()) {
    const auto nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
    if (const auto hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    if (words(line).empty()) continue;
    if (auto c = parse_line(line)) {
      out.push_back(std::move(*c));
    } else {
      ++skipped;
    }
  }
  if (bad) *bad = skipped;
  return out;
}

const Capture* Table::find(std::string_view scene) const {
  if (scene.empty()) return nullptr;
  const std::string_view bare = without_era(scene);
  // A location's own name first: "GanonCastle" is also a scene of HyruleCastle's map.
  for (const auto& c : captures_) {
    if (c.location == scene) return &c;
  }
  for (const auto& c : captures_) {
    for (const auto& s : c.scenes) {
      if (s == scene) return &c;
    }
  }
  for (const auto& c : captures_) {
    if (c.location == bare) return &c;
    for (const auto& s : c.scenes) {
      if (without_era(s) == bare) return &c;
    }
  }
  return nullptr;
}

int floor_of(const Capture& capture, float y) {
  if (capture.floors.empty() || !finite(y)) return 0;
  constexpr float inf = std::numeric_limits<float>::infinity();
  const float eye = y + kFloorEye;
  int best = -1;
  float best_top = inf;
  for (std::size_t i = 0; i < capture.floors.size(); ++i) {
    const Floor& f = capture.floors[i];
    const float top = f.near_m ? capture.camera_y - *f.near_m : inf;
    const float bottom = f.far_m ? capture.camera_y - *f.far_m : -inf;
    if (eye > top || eye < bottom) continue;
    if (best < 0 || top < best_top) {
      best = static_cast<int>(i);
      best_top = top;
    }
  }
  return best < 0 ? 0 : best;
}

std::optional<float> map_angle(const Quat& q) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  // +Z turned by q: q (0,0,1) q*.
  const float fx = 2.0f * (x * z + w * y);
  const float fz = 1.0f - 2.0f * (x * x + y * y);
  if (!finite(fx) || !finite(fz) || fx * fx + fz * fz < 1e-6f) return std::nullopt;
  return std::atan2(fz, fx);  // the picture's y is world +Z, down
}

bool pane_usable(const Pane& p) {
  for (const float v : p.m) {
    if (!finite(v)) return false;
  }
  const float area = p.m[0] * p.m[4] - p.m[1] * p.m[3];
  return std::fabs(area) > 1e-6f && finite(p.w) && finite(p.h) && p.w > 0 && p.h > 0;
}

std::array<float, 2> pane_point(const Pane& p, float x, float y) {
  const float gx = p.m[0] * x + p.m[3] * y + p.m[9];
  const float gy = p.m[1] * x + p.m[4] * y + p.m[10];
  return {kLayoutW * 0.5f + gx, kLayoutH * 0.5f - gy};
}

void minimap_icons(const MinimapPanes& mm, const Capture& c, int shown_floor, const std::vector<Player>& players,
                   std::vector<Icon>& out) {
  out.clear();
  const Pane& pic = mm.picture;
  const Pane& win = mm.window;
  if (!pane_usable(pic) || !pane_usable(win) || !finite(c.height) || c.height <= 0) return;
  const float fade = finite(mm.alpha) ? std::clamp(mm.alpha, 0.0f, 1.0f) : 0.0f;
  if (fade <= 0.0f) return;
  // The window's own units from the layout's (y up from the middle): its matrix inverted.
  const float det = win.m[0] * win.m[4] - win.m[1] * win.m[3];
  const float inset_layout = finite(mm.edge_inset) ? std::max(0.0f, mm.edge_inset) : 0.0f;
  const float win_scale = std::sqrt(std::fabs(det));  // layout units per window unit
  const float inset = inset_layout / win_scale;
  const float hx = std::max(0.0f, win.w * 0.5f - inset), hy = std::max(0.0f, win.h * 0.5f - inset);
  const float width_m = c.height * pic.w / pic.h;
  for (const Player& p : players) {
    if (!p.scene || !finite(p.position) || !in_capture(c, *p.scene)) continue;
    Icon icon = start(p);
    icon.alpha *= fade;
    // The capture fills the picture: +X right, +Z down on it; the pane's y is up.
    const float u = (p.position[0] - c.centre_x) / width_m;
    const float v = (p.position[2] - c.centre_z) / c.height;
    const float px = u * pic.w, py = -v * pic.h;
    const float gx = pic.m[0] * px + pic.m[3] * py + pic.m[9] - win.m[9];
    const float gy = pic.m[1] * px + pic.m[4] * py + pic.m[10] - win.m[10];
    float wx = (win.m[4] * gx - win.m[3] * gy) / det;
    float wy = (-win.m[1] * gx + win.m[0] * gy) / det;
    if (std::fabs(wx) > hx || std::fabs(wy) > hy) {
      const float sx = std::fabs(wx) > 0 ? hx / std::fabs(wx) : 1.0f;
      const float sy = std::fabs(wy) > 0 ? hy / std::fabs(wy) : 1.0f;
      const float k = std::min({sx, sy, 1.0f});
      wx *= k;
      wy *= k;
      icon.clamped = true;
    }
    const auto [x, y] = pane_point(win, wx, wy);
    icon.x = x;
    icon.y = y;
    if (icon.angle) {
      // The facing on the picture (y down) through the picture's turn, back to y down.
      const float dx = std::cos(*icon.angle), dy = -std::sin(*icon.angle);
      const float tx = pic.m[0] * dx + pic.m[3] * dy, ty = pic.m[1] * dx + pic.m[4] * dy;
      icon.angle = tx * tx + ty * ty > 1e-12f ? std::optional<float>(std::atan2(-ty, tx)) : std::nullopt;
    }
    if (c.dungeon) {
      icon.floor = floor_of(c, p.position[1]);
      icon.floor_delta = icon.floor - shown_floor;
    }
    out.push_back(icon);
  }
}

std::array<float, 2> to_picture(const Box& picture, const Capture& c, const Vec3& p) {
  const float width_m = c.height * picture.w / picture.h;
  const float u = (p[0] - c.centre_x) / width_m + 0.5f;
  const float v = (p[2] - c.centre_z) / c.height + 0.5f;
  return {picture.x + u * picture.w, picture.y + v * picture.h};
}

void picture_icons(const Box& picture, const Capture& c, int shown_floor, const std::vector<Player>& players,
                   std::vector<Icon>& out) {
  out.clear();
  if (!usable(picture) || !finite(c.height) || c.height <= 0) return;
  for (const Player& p : players) {
    if (!p.scene || !finite(p.position)) continue;
    if (!in_capture(c, *p.scene)) continue;
    Icon icon = start(p);
    auto [x, y] = to_picture(picture, c, p.position);
    const float cx = std::clamp(x, picture.x, picture.x + picture.w);
    const float cy = std::clamp(y, picture.y, picture.y + picture.h);
    icon.clamped = cx != x || cy != y;
    icon.x = cx;
    icon.y = cy;
    if (c.dungeon) {
      icon.floor = floor_of(c, p.position[1]);
      icon.floor_delta = icon.floor - shown_floor;
    }
    out.push_back(icon);
  }
}

}  // namespace coop::overlay::map
