# The official public game

An always-on public game that anyone with the kit finds at the top of **Public games** as
**Official public game** (room `squadala`). It is a plain `coop_server` with no hosting game:
`--public --dedicated --official-key-file ...`. Only the owner deploys it; nobody else logs in to
the relay's machine.

## The official mark (signed)

The server signs its listing with the official game's private key (Ed25519; Monocypher's): the
room name, the listing's version and the time, again with every listing (every 5 seconds). The
relay passes the proof on beside the room list (a kit asks for it after the list), and every kit
has the public key built in (coop/protocol/official_mark.hpp). A kit shows **Official public
game**, first in the list, only when the proof checks for that room and version and is at most 5
minutes old by the kit's clock. So:

- Someone who takes the room name `squadala` (while the server restarts, or on another relay) is
  an ordinary public game: they have no key, and a proof copied from the list stops checking
  within 5 minutes and never checks for another room or version.
- Old kits and old relays (before 0.11.8) ignore the proofs: the official game shows there as an
  ordinary public game, never an error and never a missing room. The relay needs this release's
  `coop_server --relay` for the mark to show.
- A PC whose clock is more than 5 minutes off sees no mark (the game still lists and joins).

## How it plays

- No host game: the first player in hosts the session's progress (their save decides for that
  run). When they leave, the player who has been in the longest takes over (join order).
- The player who hosts the progress gets no host powers there: no kick, ban or mute, no letting
  players in, no PvP, down-but-not-out or difficulty change. Moderation is the console's alone, and the
  session keeps the server's settings. (The save's mod settings stay that player's.)
- When everyone leaves, the session empties and resets (flags, objects, world); the next first
  player's save decides. Kept bans stay.
- Public game rules (docs/play-online.md, "Public games"): no password, still encrypted, relay
  only (the server listens on 127.0.0.1 and never opens a port; no player sees another's address).
- At most 16 players. A kit of another version sees the row as "Needs an update" or "Another
  version" and cannot join.

## Deploy (owner)

1. The server: from the release's Linux kit zip, `data/server/coop_server` (x86_64). On an ARM
   machine (aarch64), build it there instead: `cmake -B build -DCMAKE_BUILD_TYPE=Release &&
   cmake --build build --target coop_server` and take `build/server/coop_server`.
2. `sudo useradd --system --no-create-home coop`, then
   `sudo install -D -m 755 coop_server /opt/squadala-public/coop_server`.
   The relay: run this release's `coop_server --relay` (restart the relay's service with the new
   binary) so it passes the proofs on.
3. The key: copy the owner's key file (`official-game.key`, 64 hex digits; kept outside the repo)
   to the box and lock it to the service's user:
   `sudo install -D -m 600 -o coop -g coop official-game.key /etc/squadala-public/official-game.key`.
   coop_server refuses a key file other users can read, never prints the key, and says at start
   whether it is the kits' key (`official key: the listing is signed`). A new pair (if the key
   is ever lost or leaked): `python tools/official-game/make_key.py PATH` writes a new key file
   and prints its public key, which then goes into `kOfficialPublicKey` in a new release.
4. `sudo cp tools/official-game/squadala-public.service /etc/systemd/system/`, then
   `sudo systemctl daemon-reload && sudo systemctl enable --now squadala-public`.
   If the relay was started with a host key, add it as the unit's comment says.
5. Check: `journalctl -u squadala-public -n 20` says `room "squadala" is yours` and
   `official key: the listing is signed`, and a kit's Public games shows **Official public game**
   first.

Updating for a new release: replace `/opt/squadala-public/coop_server`, then
`sudo systemctl restart squadala-public`. Players in the game are disconnected and can rejoin.

## Moderation (no host to do it)

Commands go to the server's console, a pipe at `/run/squadala-public/console`; the answers are in
the log:

    echo /players | sudo -u coop tee /run/squadala-public/console
    journalctl -u squadala-public -n 20

| Command | What it does |
|---|---|
| `/players` | Number, name and short key id of each player (never an address). |
| `/kick N` | Kicks player N (they can join again). |
| `/ban N` | Bans player N's key; kept in `/var/lib/squadala-public/bans.txt` across restarts. |
| `/unban KEYID` | Lifts a ban: the key id from `/players` or the bans file (8 or more of its hex digits). |
| `/mute N voice` / `/mute N chat` | Mutes player N's voice or chat for everyone; `/unmute` undoes it. |

The commands act on every player, also the one who hosts the session's progress. `--dedicated` refuses
`--approve-joins` and `--invite` (there is no host to answer them).

## Not there yet (0.11.9)

- Moderation from the owner's own game (an admin key that lets the Lobby tab act as host there).
- A reserved room name: while the server restarts (about 2 seconds), someone else could take the
  room `squadala` on the relay. They show as an ordinary public game (no mark); the server takes
  the name back when they leave.
- Rate limits: the per-player message limits and the relay's per-address limits only; no word
  filter, no report button.
