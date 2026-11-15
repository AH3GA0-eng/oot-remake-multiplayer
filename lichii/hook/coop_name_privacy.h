// SPDX-License-Identifier: GPL-3.0-or-later
//
// The co-op name never defaults to anything that names the player (players'
// report, 0.11.1: the Windows user name showed to everyone and in shared
// screenshots). A new player gets a random name ("Goron-4821"); a saved name
// that is this PC's user name or PC name (the old default) is replaced once,
// and the chat says so once (docs/play-online.md, "Your name").
//
// The OS lookups and coop.ini stay with the caller (hookshot/src/
// coop_pause_files.cpp); this file only decides, so the tests check it.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CoopNamePrivacy {

// A friendly name: a word of the game's world, a dash and four digits (at most
// 14 characters, always a valid co-op name). The same `r` gives the same name.
std::string RandomName(std::uint32_t r);
// A random name from the system's random source.
std::string NewName();

// Which of this PC's names `name` is: the user name (also as the old default cut
// it to 16 characters) or the PC name, ignoring case and spaces around. An empty
// user or PC name matches nothing.
enum class Source { none, user, pc };
Source Matches(std::string_view name, std::string_view user, std::string_view pc);

// What to do with the saved name: fill (none saved: a random one), replace (it
// is this PC's name and was never checked: a random one, said once in the chat)
// or keep. `checked`: the marker beside coop.ini (CoopFiles::CheckName) says a
// check ran before, so a name typed in later, even the user name, is the
// player's choice and stays.
enum class Action { keep, fill, replace };
Action Decide(std::string_view saved, bool checked, Source match);

// The chat line that tells the player their name changed (windows: says
// "Windows user name" rather than "user name").
std::string Notice(Source was, std::string_view now, bool windows);

// The line for the co-op chat, kept until the session's next frame takes it
// (lichii/hook/coop_hook.cpp). Any thread.
void Say(std::string line);
std::vector<std::string> TakeLines();

}  // namespace CoopNamePrivacy
