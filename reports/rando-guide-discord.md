**Randomizer in co-op (0.11.2)**
It's OFF until the host turns it on. Every game (host too) builds the seed by itself in about a minute, then needs ONE restart.

**Before:** everyone on the same kit (Update.bat). Close the game fully and start it fresh. No old save loaded.

**Host**
1. Open the menu (− and + together, or F1), go to the **Mods** tab, press **Settings** on **OoT Remake Randomizer**
2. Turn **Randomizer** ON (inside Settings: the mod row saying "on" is NOT the switch)
3. Seed: empty = new. Settings: default or keysanity (or paste an OOTRR1- settings string)
4. Keep the game open until the chat says **"Randomizer seed X is ready: RESTART"**
5. Restart, host, start a **New Game**

**Guests**
1. Start fresh and join from the **title screen**. Don't load a save yet
2. The host's seed reaches you by itself: wait on the title (about a minute) for **"... is ready: RESTART"**
3. Restart, join again from the title
4. Wait for **"Success, load any save."**, then load

Each guest who joins later does the same: their own wait, one restart.

**Chat lines**
• "Randomizer on: keep the game open..." = building, don't restart yet
• "NO SEED - you play the BASE GAME" = your build failed: start from the kit again, else send last-build.txt
• "player N runs WITHOUT the randomizer seed" = they haven't restarted yet (or their build failed)
• "built unlike the host's" = that player is on another kit: update
• "a mod with its own size table" = a seed from OOTRR's website: make it in the Mods tab instead

**Common mistakes**
• Switch never turned on, so the chests are vanilla
• Guests loading an old save before "Success": its items end up in the host's file (fixed in 0.11.3)
• Restarting before "ready", or no New Game after it
• Mixed kits (0.11.1 + 0.11.2 = different worlds)
• Some vanilla chests are normal: OOTRR 0.6.0 doesn't shuffle everything yet

Full guide: <https://github.com/daurosk/oot-remake-multiplayer/blob/main/docs/randomizer.md>
