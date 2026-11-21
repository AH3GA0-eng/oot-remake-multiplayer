# The official public game

An always-on public game that anyone with the kit finds at the top of **Public games** as
**Official public game** (room `squadala` on the kit's own relay). It is a plain `coop_server`
with no hosting game: `--public --dedicated`. Only the owner deploys it; nobody else logs in to
the relay's machine.

## How it plays

- No host game: the first player in hosts the session's progress (their save decides for that
  run). When they leave, the player who has been in the longest takes over (join order).
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
3. `sudo cp tools/official-game/squadala-public.service /etc/systemd/system/`, then
   `sudo systemctl daemon-reload && sudo systemctl enable --now squadala-public`.
   If the relay was started with a host key, add it as the unit's comment says.
4. Check: `journalctl -u squadala-public -n 20` says `room "squadala" is yours`, and a kit's
   Public games shows **Official public game** first.

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

The commands act on every player, also the one who hosts the session's progress.

## Not there yet (0.11.9)

- Moderation from the owner's own game (an admin key that lets the Lobby tab act as host there).
- A reserved room name: while the server restarts (about 2 seconds), someone else could take the
  room `squadala` on the relay and show as official. The relay change that reserves it for the
  relay's host key comes later.
- Rate limits: the per-player message limits and the relay's per-address limits only; no word
  filter, no report button.
