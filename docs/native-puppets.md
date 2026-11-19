# Native puppets: other players as real game bodies

Status: design and plan (native-puppets-a, 2026-11-19), plus the test switches `--puppet-body` (phase 1),
`LICHII_COOP_BOMB_SHARE` (phase 2) and `--puppet-liftable` + `LICHII_COOP_LIFT_PLAYER` (phase 3), each below, and the phase 4 design (section 5). Nothing here is in a release. Today's puppet ([puppet.md](puppet.md)) and the real-Player test
([native-player.md](native-player.md)) stay as they are until each phase below is proven in two games.

## The goal

The other players stop being picture-only puppets and become things the game itself handles:

1. **Bodies.** You bump into another player, can stand on them, push them, and they block a doorway.
2. **Lift and throw a player**, the way Link lifts a pot, a bomb or a Cucco.
3. **Take another player's bomb.** Pick up the bomb they put down (or catch it), and the bomb changes
   hands the way enemies and bosses do today: one fuse and one explosion in every game. An unlit bomb
   you hold can go back into your own bomb count, through the game's own count.
4. **A base for good PvP.** Hits land on the real body (its hit and guard shapes), with the game's own
   knockback and shield.

## Words

- **Owner game**: the game of the player the Link belongs to. It is the only one that runs that Link's
  input, life and death.
- **Copy**: what the other games show for that Link (today the `CoopPuppet_*` pack actor).
- **Hand-over**: the server's object ownership move (`HANDOVER_ASK` 20 / `HANDOVER_STATE` 21,
  [protocol.md](protocol.md)), which carries the last state to the new owner.

Every game fact below gives where it was read. "romfs" means the game's own files read with our pack
reader (`tools/puppet_pack`: zs, sarc, byml; `flatten_actor_param` merges the `$parent` chain) from the
extracted romfs of the pinned game build. "main+0x..." are addresses in the game's `main` (build
e0f3ccfa, as in native-player.md and the reports named). UNKNOWN facts carry the probe that settles them.

## 1. What the game's Link has that a puppet lacks

The player's components (romfs, `Pack/Actor/Player_Child.pack.zs`, ActorParam merged: 54 components;
Player_Adult 55, hs-puppets-alt-b). The puppet keeps 10 of them (`KEEP_COMPONENTS` in
`tools/puppet_pack/build.py`: AS, animation, model, bone/model bind, equipment user, LOD, shadow), the
two sound ones (`SLinkRef`, `SoundFootStepRef`), the player's `AffiliationRef` and its own
`PerceptionEmitterRef` with a body-less `PhysicsRef` (InterestSpotActor's, an empty controller set:
"the puppet still collides with nothing"), and the shared default `WorldInfoRef` / `XLinkRef`.

What each feature needs, by component (names from the romfs; component slots from the engine's registry
main+0x11e9b00 / main+0x120c800, reports/hs-puppets-alt-b.md: slot 10 Physics, 87 Life, 107 Player):

| Feature | Player's component (romfs) | What it holds | Puppet today |
|---|---|---|---|
| Body (bump, stand on, push) | `PhysicsRef` -> `Phive/ControllerSetParam/Player_Child` ($parent `Player`) | `InitializerName: Character`; character controller `Phive/CharacterControllerParam/Player` (gravity, slopes, `UserContactCollectionName`); shapes `Player_cct`, `Player_cct_crouch`, `Player_cct_climb`, `Player_Tgt_Body`, `Player_Atk_AtkPlayerBody`, `Player_Chemical_Body`, `Player_RideAstride`, `Player_ForArea`...; matter bodies `MainMatterRigidBody`, `ClimbMatterRigidBody`, `WeightBody`; contact collections `Contact_0_cct` (`HitToBlockable`), `RagdollBody`; ragdoll, foot/hand IK, cloth | body-less physics (perception only) |
| Being hit, guarding | `SensorHitRef` (`SensorHitParam/Player`: `AtHitInfoBufferSize 8`, `TgHitInfoBufferSize 8`, `IsThroughSameAffiliation false`) + the controller set's sensors `Tgt` (`Player_Tgt`, `PlayerReceipt`), `Atk` (`Player_Atk`, `PlayerAttackReceipt`), `Chemical`; `ReflectShield` rigid body | which attacks reach which shape; shield reflect | none |
| Life, knockback, death | `LifeRef` (`LifeParam/Player`), `ReactionRef` (`Reaction/Player`), `DeathRef`, `ConditionRef`, `ChemicalRef` | hearts, the hurt reaction, fire/ice/electric | none |
| Lifting things | `PlayerLifterRef` (`Game/PlayerLifterParam/Player_Child`, $parent `Player`): `LiftGrade` 3655598229 (child), `LiftThrowParams` Default (bind bone `Weapon_R`, throw 120 / 170 m/s impulse, `IsIgnoreBombOnPrepareLifted`), `HeavyRock`, `Fish` | what Link can lift and how he throws | none |
| Holding things | `ActorHolderRef` (`ActorHolderParam/Player_Child`) | what sits in his hands | none |
| Riding / being ridden | `RiderRef`, `RidableRef` (`RidableParam/Player`), the `RideAstride` rigid body (alias `Sub`) | Epona; something riding Link | none |
| Being the player | `PlayerRef` (slot 107, the player's own code), `ActorNameRef` `Player`, `PlayerCameraTargetRef`, `GameLifeConditionRef` `NoDelete` | input, camera, the game's "who is the player" | none (must stay out on a copy, alt-b) |
| Being seen | `AffiliationRef` `Player`, `PerceptionEmitterRef`, `AttentionReceiverRef`, `TargetRef` | enemies and Z-targeting find him | affiliation + emitter (perception-on-a) |

What is liftable in the game (romfs): an actor with a `LiftableRef`. `Item_Bomb`'s `LiftableParam`:
`LiftGrade` 3655598229 (the same hash as the child lifter's grade), `LiftableType` 3237216321,
`WeightForCalcThrowPower` 18, `ThrownEntityAt.IsDisable`. `Animal_Cucco` (a living, AI-run actor that
Link lifts: the model for lifting a player) has `LiftableRef` with `LiftGrade` 3069107721,
`LiftableType` 1577663790, `KeepSensorHitToParent`, `IsFinishThrownStateByDamage`,
`IsResetBehaviorModeOnThrown`, `WeightForCalcThrowPower` 28. **The player has no `LiftableRef`**, so
nothing in the game can lift Link as he is.

UNKNOWN: which lift grades the child and adult lifters accept (the hashes are names; the order between
them is in code). Probe: from the game's `LiftGrade` enum strings in main (search the rodata for the
murmur3 of the names the romfs uses: 3655598229, 3069107721, 375060617) and the lifter's check
(the `PlayerLifterParam` reader); or in game, one run lifting a Cucco as child and as adult.

## 2. The options

### A. A second real `Player_<Age>` driven by the network

What native-puppets-b2 started (`LICHII_COOP_NATIVE_PLAYER`, docs/native-player.md): the game's own Link,
made by the Director, then given the other player's pad through two wraps (buttons main+0x279ce80,
sticks main+0x279e2c0; Hookshot patch 0017 rows, wrap ids 16-17).

- For: everything at once and exactly as the game does it (body, lifter, sword, shield, items, cloth,
  hurt reaction), tens of bytes a frame instead of a pose.
- Against: the game is made for one player. Known (native-puppets-b, read from main): "the player" is the
  preload list record (main+0x9456b0 walker; main+0x37cca0, 15 callers), so a directly created second
  Player is not "the player" (good); but its `PlayerRef` code runs a full Link: it reads the pad
  (fixed by the wraps), it can open doors, start talks, pick up and spend items from **this** game's
  save (pouch and life live in the save, not on the actor: UNKNOWN which reads go to the save), trigger
  scene changes, voids, and cutscenes. Its result can drift from the owner game (two simulations of
  one Link) and must be corrected every frame, which the player code may fight (it moves by its own
  character controller).
- Status: step 1 code merged (test switch, idle by default); never run in a game.

### B. The puppet with bodies from the game's own data (component by component)

Keep `CoopPuppet_*` as the copy (placed by the hook through the game's physics-aware setter,
`ExecuteActorForceSetMtx` main+0x2622e50 / vtable slot 0x218 = main+0x4c6580, coop_puppet.h `kSetMatrix`),
and add, one at a time, the pieces each feature needs, each taken from the game's own files:

- a body: `PhysicsRef` with a character body and no gravity (phase 1);
- hit and guard shapes: the `Tgt`/`Atk` sensors + `SensorHitRef` (phase 4);
- liftable: a `LiftableRef` of its own (the Cucco's as the model) (phase 3).

- For: each piece small and testable alone; the owner game keeps sole authority (the copy never decides
  anything by itself); no player code runs on the copy, so nothing touches this game's save or camera.
- Against: knockback, guard and lift reactions are not played by the copy's own code; the owner game
  plays them and the copy shows them through pose sync (as today).

### C. Hybrid (the plan)

Option B for the copy's **body, sensors and liftable** (what the local game must collide with, hit and
lift), with the **decisions** taken where the game already has the code: the owner game's real Link.
When this game's sword touches the copy, the hit is sent to the owner, whose real Link takes it through
the game's own hit path (the hit replay, coop_hit_replay.h) and plays its own knockback; the copy shows
it. When this game lifts the copy, the owner's real Link is put in a "lifted" state by the owner game.
Option A stays a separate test track (driving with inputs); if it is ever proven it replaces the copy,
and every phase below still holds because the copy's components are the same ones the real Link has.

## 3. Lift a bomb, and stash it

What the game uses (romfs `Item_Bomb`): `LiftableRef` (above), `LifeRef` (max 1, `BallBomb`),
`ChemicalRef` (explode), `ShootableRef` (`ShootableParam/Item_Bomb`: `ShootUseAttackParam`,
`TimeKeepStationary` 30), `PouchContentRef` (`PouchContentParam/Item_Bomb`: `EnableStock true`,
`MaxNumGameData "Bomb"`, `SmallBagActor Item_BombBag_Small`), `DeathRef` `PlayerHoldedItem`, AI
`Item_Bomb.root.ainb`, AS `LiftableExplodeObject_BallBomb` (states `Lifted`, `Thrown`, `Bomb_Wait`,
`Bomb_Blink`, `Explode`: coop_projectiles.h `HeldByAnimation`). The fuse starts when Link lifts it
(reports/held-bomb.md: AI `ExecuteBarrelBombAI`, AS `Bomb_Blink`).

Today (docs/spawns.md "Held bombs and the fuse"): the thrower's game owns its bomb; the others see a
`CoopShot_Bomb` stand-in in the hands and a real `Item_Bomb` once thrown. A bomb lying on the floor in
the other game is a real `Item_Bomb` copy: its own AI would light and explode it, which is why only the
owner's explosion counts.

The plan (phase 2):

- **A bomb is a shared object with an owner** (coop_objects_sync, the same rules as enemies: one owner,
  epoch, hand-over with state). Its state: lit or not, fuse time left, held by (player or none).
- **Taking it:** when this game's Link lifts a bomb copy (the game's own lift, on the real `Item_Bomb`
  copy, which keeps its `LiftableRef`), this game asks for it (`HANDOVER_ASK`-like request with the lift
  time). The server gives it to the first asker by the hand-over rules; the new owner gets
  `HANDOVER_STATE` with the fuse left and continues that fuse on its own bomb; the old owner's bomb
  becomes the copy. A refused game drops its lift (the bomb returns to the owner's place). Fuse: one
  number (time left) carried in the state, so every game explodes it once, at the owner's word
  (`WORLD_BREAK`-like event, as bosses' deaths).
- **Stash:** an unlit bomb in Link's hands can go back into the pouch through the game's own count:
  `PouchContentParam.MaxNumGameData "Bomb"` is the game data the pouch counts in (coop_cheat_items.h
  writes the same "Bomb" count through the game's flag queue). The native route is the game's own
  "pick up a pouch item" path (`PouchContentRef`, `EnableStock`), not a count write. UNKNOWN: whether
  the game lets Link put a held unlit bomb back (a button) or only picks a lying bomb up. Probe (one
  game, solo, module log only): hold an unlit bomb, press the item button / put it down and walk over it;
  log the "Bomb" game data and the bomb actor's AS state each frame. If the game has no stash path,
  the stash is the pick-up path run on a bomb put down (no new rule).
- A bomb that is stashed is gone for everyone (the owner deletes; `WORLD_BREAK`-like gone with
  no explosion), and only the stashing game's count changes.

### Phase 2 status (native-puppets-b, branch `native-puppets-bomb`)

- **Built and host-tested:** the protocol ([protocol.md](protocol.md), "Bomb hand-over": `BOMB_TAKE` 530,
  `BOMB_ANSWER` 531, `BOMB_SHARE` 532, no protocol bump) and the server's rules (first lift wins, an
  unanswered ask keeps the bomb with its owner, the old owner's late explosion refused, a stash is a
  picked-up remove, a bomb whose owner leaves goes on with a sharing game and its fuse). The game-side
  rules as pure code, `lichii/hook/coop_bomb_share.h` (lift detection from the copy's own `Lifted`,
  one ask per lift, the fuse moment from the handed state, the owner's answer only when its Link does
  not hold the bomb, the stash from the pouch's "Bomb" count rising by one, the own-bomb registry).
- **Wired (native-puppets-b-2), host-tested, not yet run in the game:** `LICHII_COOP_BOMB_SHARE=1` turns it
  on in the spawn sync (`Sync::SetBombShare`; off by default, and off nothing is sent or taken in).
  Lifting a copy sends `BOMB_TAKE` and stops the director placing and animating it; the hand-over moves the
  copy's actor into our own bombs (the director lets go of it, `Act::release`), samples it at the new epoch
  with the fuse handed over, and at its end sets its life to 0 once (the game's own explosion, the way a
  copy is blown). A refused or unanswered lift gives the copy back and is not asked again until our Link
  lets go. Asked for one of ours: answered with its state unless our Link holds it. Handed on: our actor
  is deleted without an explosion (checked by vtable) and the new owner's copy shown. A taken unlit bomb
  gone as `PouchData.Item_Bomb.StockNum` rose by one is removed as picked up. The objects sync leaves a
  `HANDOVER_ASK` for one of these bombs to the spawn sync. A bomb handed to us because its owner left is
  adopted too (made as the game's own bomb first if it was shown held). Only `Item_Bomb` changes hands:
  bombchus, pots, rocks and other liftables are not shared this way (`CoopBombShare::IsBomb`).
- **The game facts this leans on (phase 0 probe):** that a lifted copy plays `Lifted` on its own AS slot 0
  (the copy is the game's real `Item_Bomb`, with its `LiftableRef`); that the director must stop placing it
  while our Link holds it; whether the game can stash a held unlit bomb at all (if not, the stash path is
  the pick-up of a bomb put down: the same remove, the same count rise).

## 4. Lifting another player

Needs, on the copy, what a Cucco has: a `LiftableRef` (phase 3; the Cucco's param, own file in the
puppet pack, Link-sized offsets), and on the owner game a "lifted" state for its real Link.

- **Who decides.** The lifted player's game (owner of that Link). The lifter's game sees the game's own
  lift start on the copy and sends `LIFT_ASK(lifted, lifter)`; the owner game accepts unless its Link is
  in an event, a cutscene, dead, already lifted, or (PvP off) the player turned it off. Until the answer
  (one round trip) the lifter's game plays the lift as the game does (the copy follows Link's hands);
  on refuse it drops the copy back to the owner's samples.
- **While lifted.** The lifter's game owns the **place** (the copy sits on the lifter's hand bone,
  `Weapon_R` / `Weapon_Root` per `LiftThrowParams`), and sends it; the lifted game puts its real Link
  there through the same physics-aware setter (main+0x4c6580), input to movement off (the game's own
  "input off" check main+0x3025850, native-player.md), the camera stays on its Link. The lifted
  player's buttons only ask to wriggle free (a timer, as the game's held Cucco escapes: UNKNOWN).
- **Throw.** The lifter's game throws the copy with the game's own throw (`ImpulseBasedThrowSpdForwardMps`
  120, `ImpulseBasedThrowSpeedYMps` 170 for Default), sends `LIFT_THROW(velocity)`, and the lifted
  game gives its real Link that velocity (knockback path, UNKNOWN: which setter takes a velocity on the
  player's character controller; probe: the hurt reaction's push in main, or the `ReactionParam/Player`
  knockback field), and owns its place again.
- Ownership of the lifted Link never moves; only "who drives the place" does, for the lift time. This
  is the same split as a boss carried by its root.

### Phase 3 status (native-puppets-c, branch `native-puppets-lift`)

Built and host-tested, **not run in a game**. Two switches, both off by default; with either off nothing below
runs, nothing is sent, and the solo and idle game are as before (no session: no copy exists, and the glue
returns before anything when not connected).

- **Pack:** `tools/puppet_pack/build.py --puppet-liftable` gives every puppet a `LiftableRef` of its own, the
  Cucco's (`Animal_Cucco`'s `LiftableParam`, its in-pack `$parent` chain merged; own file
  `Component/Liftable/<puppet>.game__component__LiftableParam.bgyml`, sized in the size table as the Cucco's
  file). Off: the pack is byte for byte as before (test `test_pack_liftable`), the manifest has no `liftable`.
- **Protocol:** `LIFT_ASK` 535, `LIFT_ANSWER` 536, `LIFT_PLACE` 537, `LIFT_SHARE` 538 ([protocol.md](protocol.md),
  "Player lift"). The plan's `LIFT_THROW(velocity)` is not needed: the throw is the lifter's game's own (its
  lifter throws the copy), and the places go on as `flying` until the copy is at rest, then one `landed` place.
  So the lifted game never needs a velocity setter (the UNKNOWN above): its Link follows the thrown copy's path
  and is let go at the copy's resting place, where its own character controller takes over.
- **Rules** (`lichii/hook/coop_lift_player.h`, tests `tests/lichii/lift_player_test.cpp`):
  - Lifter: the copy is lifted when the game put it at least 0.3 m higher than the director drew it, within
    1.6 m of our Link, two frames running (the director reads the copy's place before each of its writes:
    `CoopPuppet::WatchGamePlaces`), and only while our own Link's main animation slot plays a lift or a carry
    (`OurLinkLifts`: a name with "lift" or "carry"; not read is no lift). So a copy raised by anything else (a
    rising platform under a `--puppet-body` copy, a step, our Link pushing under it) never asks; the log says
    "copy rose but our Link's animation '...' is no lift" once per such rise (the probe for UNKNOWN 6). It sends `LIFT_ASK` and the director stops placing, turning and setting that
    copy (`CoopPuppet::SetGameHolds`), so the game's own carry has it. No answer in 1.5 s, or a no: the director
    places it again and it is not asked again until the game lets it down. Taken: `LIFT_PLACE held` each frame
    with the copy's place as the game has it; when it is 0.6 m further out or lower than it was carried
    (thrown or set down) the places are `flying` until 8 frames with under 2 cm of movement (or 2 s), then
    `landed`, and the director places it from the samples again.
  - Lifted (decides): no while our Link is in an event (G1, our own), a load or a menu (`event`), dead or at
    life 0 (`down`), already lifted by another or lifting someone (`busy`), or the lifter's Link more than 3 m
    away, not known, or in another scene (`too far`; the server also answers `too far` itself when the lifter
    has no scene or no fresh sample position, or is not in the lifted player's scene, so a stray ask never takes
    a player's input away). Taken: each place is given to our real Link by the object walk the way copies are moved
    (place written, then the game's physics-aware setter main+0x4c6580 on it: `CoopObjects::PlaceOurLink`), and
    our Link's own input check (main+0x3025850, Hookshot wrap 18) answers "input off" for our Link only (the
    reading actor checked), as for an extra local player without a controller. The "off" is a time stamp each
    lift frame renews: older than 0.5 s it reads "on" again (`InputOffFresh`), so input can never stay off if
    the lift's frame stops being called. The lifted game frees itself
    (`LIFT_ANSWER freed`) on an event or death, when the places stop for 1 s (the lifter left), or after 15 s
    (it wriggles free; the game's own escape timer is UNKNOWN). The camera stays on our Link (nothing changes it).
- **The game's functions used:** the lifter's own lift, carry and throw (`PlayerLifterParam` `LiftThrowParams` on
  the copy's `LiftableRef`); the physics-aware setter `ExecuteActorForceSetMtx` (vtable slot 0x218,
  main+0x4c6580) on our Link; the player's input-off check main+0x3025850 (wrap 18). No memory poke of the
  player's state; the only writes are the place before the setter, as for every copy.

Phase 3 UNKNOWNs (settled by the proof job in reports/native-puppets-c.md, screenshots on both games):

1. Whether the child and adult lifters lift a copy with the Cucco's `LiftGrade` 3069107721 at all (the copy
   has no Cucco AI; `IsResetBehaviorModeOnThrown` and `KeepSensorHitToParent` may need it). Without
   `--puppet-body` the copy has no body to grab: the proof runs with both switches.
2. That the game's carry moves the copy's own place (+0x31c) so the read-back shows the rise (else the lift
   is not seen: then read the lifter's held actor instead, `ActorHolderRef`, UNKNOWN offsets).
3. That the game's throw moves a copy whose character controller is in `Free` (phase 1's body); if it does
   not fly, the copy rests at once and the lifted Link is let go at the lifter's hands (it then falls by its
   own controller): a set-down, not a throw.
4. That our Link placed through the setter each frame is not pulled back by its own controller between the
   walk's places (the held-bomb fall, reports/held-bomb.md); and that "input off" stops its own moves.
5. How the game lets a refused lift go (the lifter keeps carrying a copy the director now places): the next
   fix would be the lifter's own put-down action verb (the request-action-verb path main+0xc08f0, verb
   UNKNOWN).
6. Our Link's own animation names while it lifts and carries (AS slot 0). The gate takes any name with "lift"
   or "carry"; if the proof log shows "copy rose but our Link's animation 'X' is no lift" during a real lift,
   add X's family to `OurLinkLifts` (or read the lifter's `ActorHolderRef` instead, offsets UNKNOWN).

The same "ask without our own state" and "unknown place" rules hold for the phase 2 bomb take: our game sends
`BOMB_TAKE` only when the copy's own animation says Lifted AND our Link's place is read and within 2.5 m of the
copy (`CoopBombShare::NearOurLink`); the server refuses a `BOMB_TAKE` (`not_a_bomb`) when the taker has no scene,
another scene than the bomb's, or no fresh sample position.

### Other liftable and shared things (named here, later phases)

- **Bomb flowers** (`Obj_BombFlower_*`, [sync-coverage.md](sync-coverage.md) P2): an unshared world object today
  (each player picks their own fruit; one picked and thrown in one game is not seen in the other). Not a made `Item_Bomb`,
  so phase 2's hand-over does not cover it. Later phase: share the plant's pick (a world object with an owner,
  as grass cuts and world breaks) and the picked bomb as phase 2's bomb.
- **Pots and rocks** (scene objects Link lifts and throws): their break is shared today ([world.md](world.md)); who
  carries one is not. Later phase, after phase 3 is proven: the lifted pot or rock as a shared object with an
  owner (phase 2's rules: one owner, the place sampled while carried, the break once at the owner's word).
- **Cuccos:** living AI actors; today each game runs its own. Later phase, with the pots: the Cucco as an
  enemy-like shared object (owner runs its AI, others copy), then lifting it is phase 2's take.
- Bombchus are not liftable (nothing to do).

## 5. PvP on real bodies

Today (reports/pvp.md, coop_pvp.*): the attacker's game finds a swing near the other Link (grass
blade points, 1.3 m) and sends `PVP_HIT` (242); the victim decides (kept off in events, 0.8 s mercy) and
replays a stand-in sword hit on its own Link (`HurtOurLink`, the hit replay), with a written life after
0.4 s if the game did not take it, then `PVP_HURT` (243) to all. Indoors (no grass) a sword hits nothing,
the damage is a fixed half heart, nobody can lock on, and a shield blocks nothing.

Phase 4 (design native-puppets-e, 2026-11-19; owner: "you should be able to target them, get your attacks
blocked, have real hit effects when hitting them, etc etc."): with PvP on, another player's copy is an
opponent the **game's own systems** handle. The attacker's game finds the hit with its own collision; the
**victim's game decides** with its real Link (guard, facing, invincible frames, life, death); every other
screen shows it. No number here is ours where the game has one.

### 5.0 Switches, and what PvP off means

- Session value: `PVP_MODE` (241, host only, off at every start), as today.
- Pack `--puppet-pvp` (implies `--puppet-body`): the copy gets the pieces in 5.1. Without it the pack is byte for
  byte as before (the pack test checks it).
- Glue `LICHII_COOP_PVP_NATIVE=1` (off by default) in the module; the game says so with `PVP_NATIVE` (541).
  Off: today's PvP (242/243) is untouched.
- PvP off (or the switch off): every copy's sensors are in the game's no-hit mode and its attention sender is
  invalid, so nothing can lock on to it or hit it. Solo and co-op idle have no copy at all, so nothing changes.

### 5.1 The copy's pieces (`--puppet-pvp`, all cut from the player's own files)

| Piece | From (romfs) | Why |
|---|---|---|
| `SensorHitRef` | `SensorHitParam/Player` (`TgHitInfoBufferSize 8`, `IsThroughSameAffiliation false`: a Player-affiliation sword hits a Player-affiliation body) | the game records hits on the copy |
| Sensors `Tgt` (`PlayerReceipt`) and `Atk` receipt (`PlayerAttackReceipt`) with their shapes | `Phive/ControllerSetParam/Player` `CColSensorNamePathAry` (`ForArea` left out) | the attacker's sword, shot or blast touches the copy; sword meets sword |
| `LifeRef` + `ReactionRef` | `LifeParam/Player` (DamageParameters/Player: `GuardableAngleMaxDeg 120`, every `DamageRates` 1.0), `Reaction/Player` | the attacker's game runs its own hit step on the copy (hit-stop, flash, sparks, the Reaction's `Hit` sound) and writes the incoming hit record (Life +0x240) that is read and sent |
| `AttentionSenderRef` | a new param whose `ClientList` is the copy's own `LockOn` and `AutoAim` clients, cut from `Attention/ClientParam/EnemyCommon_LockOn` / `_AutoAim` (`ClientName` LockOn / AutoAim; `AttentionType` 3016662870 = murmur3 "LockOn"; `CheckAffiliation Player`: who may lock on) | Z-targeting and item aim assist find the copy as they find an enemy |
| `TargetRef` | `TargetParam/Player` | the target slot the attention and the camera look at |

Never on the copy (alt-b's rule): `PlayerRef`, `ActorNameRef Player`, `PlayerCameraTargetRef`, `DeathRef`,
`ConditionRef`, `ChemicalRef`, `GameBalance/AttackParam` (the copy runs no attack of its own: its sword never
hits anything in our game; its owner's hits come from its owner's game as messages).

The copy's Life is **invincible from its creation**, through the game's own call: `OneShotLifeSetInvincible`'s
on-function main+0x8d75e0 (Life, IsNoReaction **0**, &our key), the call down-crawl uses (reports/down-crawl-a.md
round 3). Nothing ever falls on a copy, and the game still plays the hit reaction (IsNoReaction 0 keeps it off the
no-reaction list Life +0x1c60). UNKNOWN U1.

Runtime on/off per copy (at its creation and on every `PVP_MODE` change): attention sender valid = PvP on
(`OneShotAttentionSenderChangeValid`'s run), sensors hit = PvP on (`OneShotChangeForceNoHitModeRigidBody`'s run).
Both are AI nodes of the game's own; their runs are found as down-crawl found OneShotLifeSetInvincible's (the
node's vtable, its setup registering the argument names). UNKNOWN U3. A downed or dead player's copy is made
invalid and no-hit too (nobody locks on to a downed player).

### 5.2 Point 1: targeting

The game's lock-on is our Link's `AttentionReceiverRef` (`AttentionReceiverParam/Player`: `HasLockOnState`,
`IsCheckScreen`) choosing among attention senders. With the copy's `LockOn` client valid, Z picks the copy as it
picks an enemy, and the game's own reticle (`Cursor_LockOn`), lock-on camera and strafing
(`ExecutePlayerLockOnMove` / `ExecutePlayerLockOnWait`) follow; the `AutoAim` client gives the bow, slingshot,
hookshot and boomerang their aim assist. Nothing of ours draws or moves. PvP off: the client is invalid, so a copy
is never a target (today). Left out: Navi's enemy hint (`NaviAttentionParam`, which names an enemy's hint event).
UNKNOWN U4.

### 5.3 Point 2: blocking (decided on the victim's game)

**Victim.** The hit goes into the victim's real Link the way the game delivers any hit, so the game's own guard
check runs with Link's real guard state, facing and shield: the angle (`GuardableAngleMaxDeg 120`), the shield
(`ShieldParam`: `BadConditionAttackGuard` per damage type, e.g. the Deku Shield and fire) and the weapon's
`GuardBreakPower` (WeaponParam: Kokiri Sword 2, Master Sword 40, Biggoron/Giant's 10, Deku Stick 10). Guarded:
Link plays the game's own guard push (`reactionguard`, `reactioncrouchguard`; `reactionguardbreak` /
`reactionguardfaile` when it fails), no life falls. Two ways in, A preferred:
- **A. The sensor path.** Put the hit into Link's `SensorHit` Tg hit info (the buffer the game fills from its
  collision, 8 entries) before the game's step that turns hit infos into guard, damage, reaction and invincible
  frames. Everything after that is the game's. UNKNOWN U2.
- **B. The life path (fallback, only if U2 finds no safe way in).** Today's hit replay into Life +0x240 (it
  already plays knockback and takes the life), behind a guard check made only of the game's own data: Link
  guarding = his main AS / verb is a guard one (U5), the attacker within 120 degrees of his facing, the shield not
  failing for that kind; guarded: the game's guard reaction verb (ExecuteBSASetActionVerb) and the shield's guard
  effect through the game's own XLink emit (docs/spawns.md, main+0x295b740's way). The reviewer checks that B is
  used only with U2's reason written down.

**Attacker.** Its sword must clang on the shield on its own screen, at once. The copy wears the real shield actor
(`EquipmentUserRef`), and `Weapon_Shield_*` has its own `Tgt` sensor (`WeaponShieldMetal`:
`WeaponShieldTgtMetal`). If the attacker's game judges a hit on that shield of a copy in the guard pose as
guarded, the game itself plays the clang, sparks and the attacker's recoil (`reactionattackcollide`,
`guardjustrepelled` on the attacker's real Link). UNKNOWN U6. If not, the attacker's Link plays them when the
victim's `PVP_RESULT guarded` comes (one round trip late), as in B.
Either way **the victim's result decides the damage**; a clang on one screen with a hit on the other is logged
(`pvp: strike N: attacker saw a guard, victim took it`) and counted in the proof. The same holds both ways: our
hit on a guarding copy, and the copy's owner's hit on our guarding Link.

### 5.4 Point 3: real hit effects, damage from the game

- **Attacker's screen.** The copy's own Life step (5.1): hit-stop, flash, sparks, sound, at the contact.
  Knockback and the hurt pose come from the victim by pose sync.
- **Victim's screen.** Link takes the hit by path A (or B): knockback, hurt reaction, hearts, the game's sound and
  effects, all native.
- **Every other screen.** `PVP_RESULT` (540) goes to every game in the scene with the hit's kind words. Each one
  replays that kind into its own copy of the victim (invincible Life: flash, sparks and sound, nothing taken), as
  protocol 14 does for enemy copies (objects.md, "The real hit"); pose sync shows the knockback.
- **Damage.** Never ours. The attacker's game computed the hit on the copy (the record's amount +0x224 and kind
  words +0x250..+0x27c, `ReadHitRecord`) from the weapon's `BaseAttack` (WeaponParam: Kokiri Sword 1, Master
  Sword 2, Biggoron/Giant's Knife 4, Megaton Hammer 3, Deku Stick 2), the attack's row in
  `GameBalance/AttackParam/Player(_Child)` (`DamageIntensity` Weak / Strong: spin and jump attacks are Strong,
  `DamageType`, `DamageMultiplier`, `HitStopType`, `KnockbackIntensity`) and the target's `DamageRates`. Path A:
  the victim's game counts it again from the hit info, and its own number is what Link loses; path B: the
  attacker's amount, bounded 0 to 80.
- **Items.** Arrows (their elements in `DamageElements`: fire, ice, light), seeds, bombs and bombchus (their
  blast's own Atk), nuts, the hookshot, the boomerang, Din's Fire: their own shapes touch the copy's `Tgt` in the
  attacker's game and are read the same way. A no-life hit (a nut's, boomerang's or hookshot's stun) has amount 0
  and is sent too: the victim's game does what the game does with that kind on Link. UNKNOWN U7. The hookshot never
  pulls a player (the game pulls only actors made for it; a copy is not one).
- **Which hits are sent.** Only a record whose attacker is our Link, or a shot or blast our game made (CoopSpawn's
  own list). An enemy's hit on a copy is never sent (the invincible Life does nothing with it locally either): the
  owner game's enemies decide (section 6, "Enemies").

### 5.5 Point 4: the rest

- **Invincible frames: the game's own.** After a hit, Link's Life is invincible for the game's time
  (`OneShotLifeReserveInvincibility`); a hit landing then is refused by the game itself and answered
  `invincible`. Today's 0.8 s `kMercyUs` and the written-life fallback are not used on this path.
- **Death: no PvP path of its own.** The hit goes through Link's own Life, so a PvP kill is whatever an enemy's kill
  is in that session: the co-op downed path when it is on (coop_dbno: the game's Crawl verb 234, the downed
  invincibility main+0x8d75e0, revive by a teammate; it already counts an up rival as a threat with PvP on), else
  the game's own death and Game Over. Why: one rule for every death, both built on the game's own verbs, and the
  downed path is what the session already chose; a separate "PvP death" would be ours. A downed or dead player
  refuses every strike (`kept off`), and its copy is no-hit and invalid.
- **Shield break and burn: the game's** (path A: `GuardBreakPower` against the shield, `BadConditionAttackGuard`;
  path B keeps only what U2 and U5 read).
- **No friendly fire when PvP is off**: sensors no-hit and attention invalid in every game (5.1), and the server
  drops every `PVP_STRIKE` while off (as it drops 242 today).

### 5.6 Who decides, and lag

- The victim decides. A strike names `seen_us`: the server-clock time of the victim's own sample the copy showed
  when it was hit (the director knows which sample it drew; samples carry server-clock times, protocol.md). No
  clock of the attacker's is trusted.
- The victim keeps a 1 s ring of its own places (each frame). A strike is taken if `now - seen_us` <= 350 ms (the
  copy's ~100 ms display delay + 250 ms, as `kHitLateMs` 200 for enemy hits plus the delay), and the hit's place is
  within `max(0.5 m, speed x (now - seen_us))` plus the body (1.0 m level, -0.4 to 2.2 m high: today's body
  numbers) of the ring's place at `seen_us`. Else `late` or `out of reach`.
- Kept off as today: an event, a menu, a load, pause, another scene, life not read, downed or dead, or the attacker
  in one of those.
- So what the attacker saw is what it hit (within the ring), and a laggy attacker cannot hit someone who got away.

### 5.7 Protocol (types 539 to 541, claimed in COORD-LOG; no bump: off by default, older games ignore them)

| Type | Name | Sender | Channel | Body |
|---|---|---|---|---|
| 539 | `PVP_STRIKE` | both | ownership | `u8` player field (the attacker), `u8` victim (1 to 16, not the attacker), `u32` seq, `u64` seen_us, 11 x `u32` the hit's kind words (record +0x250 up to +0x27c), `s32` amount (0 to 80, the attacker game's), `u8` source (1 sword or body, 2 shot, 3 blast, 4 other; others refused), 3 x `f32` hit place, 3 x `f32` the copy's place, 3 x `f32` the attacker's place (all finite). To the victim only |
| 540 | `PVP_RESULT` | both | ownership | `u8` player field (the victim), `u8` attacker (1 to 16, not the victim), `u32` seq, `u8` result (0 taken, 1 guarded, 2 guard broken, 3 invincible, 4 late, 5 out of reach, 6 kept off; others refused), `s32` amount taken (0 to 80), `s32` life left (0 to 80), 11 x `u32` kind words, 3 x `f32` hit place (finite). To every game in the victim's scene |
| 541 | `PVP_NATIVE` | both | ownership | `u8` player field, `u8` on (0 or 1; others refused). Its game runs this path; sent after joining and every 5 s, relayed to all |

Server (`on_pvp_strike`): drops a strike while PvP is off, to or from a game not `PVP_NATIVE`, between two scenes,
or from a player with no fresh place (5 s, `place_known_in`, as lifts); relays it to the victim only. A result is
relayed only from the victim a strike in flight named (by seq), to its scene. A native attacker sends the old
`PVP_HIT` (242) to a victim that is not native, so mixed sessions keep today's PvP. The victim still sends
`PVP_HURT` (243) for the chat line.

### 5.8 UNKNOWNs and the probe that settles each (one module-only research run, offline job)

| # | Unknown | Probe |
|---|---|---|
| U1 | An invincible Life (IsNoReaction 0) on the copy still writes the incoming record (+0x240, last hit id +0x1b18) and plays flash, sparks, sound and hit-stop | A's sword on B's copy indoors; log the copy's last hit id and record each frame; screenshot A at the contact |
| U2 | Path A's way in: the function that drains `SensorHit`'s Tg hit info into guard, damage and reaction, and whether a hit info can be added before it | Read main from the `TgHitInfoBufferSize` reader's xrefs to the consumer; then one run adding a copy of a Deku Baba's real hit info to Link's buffer, shield up and down |
| U3 | The runs of `OneShotAttentionSenderChangeValid` and `OneShotChangeForceNoHitModeRigidBody` | Read main (node vtables, setup names), then toggle them on a copy in the run |
| U4 | A copy with `AttentionSenderRef` + `TargetRef` and no AI is chosen by Z and by AutoAim | Run: Z at B's copy, screenshot the reticle and the strafe; aim the slingshot |
| U5 | Path B only: the guard verb values and the guard AS names | Every scheduler module's selector cases (as down-crawl's verb list) |
| U6 | A hit on a copy's real shield actor in the guard pose is judged guarded by the attacker's game (clang, recoil) | Run: B holds the shield up toward A; A slashes; screenshot A |
| U7 | What the game does with player-only kinds on Link (sword, nut, boomerang, hookshot, arrows, Din's Fire) | One strike of each kind in the research run; screenshot B and log life |

### 5.9 Proof scenarios (offline 2-game job, screenshots of both games, no PASS from logs)

Both kits: pack `--puppet-body --puppet-pvp`, `LICHII_COOP_PVP_NATIVE=1`, `/pvp on`, indoors (no grass).
1. A Z-targets B's copy: [shot A] reticle on B, A strafes around B. PvP off: Z finds nothing [shot A].
2. A's Kokiri Sword on B: [shot A] at contact (flash, sparks); [shot B] B knocked back, hearts lost as the game
   counts them; [shot A] B's knockback by pose sync.
3. B shields facing A, A slashes: [shot A] clang and recoil, [shot B] guard push, no hearts lost. B faces away: hit.
4. Two slashes inside B's invincible frames: the second answered `invincible`, one fall only [shot B].
5. Spin and jump attack, a Deku Stick, an arrow, a fire arrow, a bomb, a nut, the hookshot: each [shot A+B], life
   and result logged per kind.
6. B brought to 0: [shot B] B downed (or Game Over with downed off); A cannot lock on to downed B [shot A].
7. An enemy hits B's copy in A's game: B's life unchanged [shot B]. PvP off: A's sword passes through B's copy
   [shot A+B].
8. A third game C watching A hit B: the hit effect on B's copy [shot C].

### 5.10 Build order (step 2, behind the switches, host tests)

1. Protocol 539 to 541 (codec, tests), server `on_pvp_strike` + result relay + `PVP_NATIVE` (tests).
2. Pure rules `coop_pvp_native.h`: the victim's ring and rewind check, the strike-to-result rules (kept off,
   late, out of reach), which records are ours to send, per-copy on/off from the mode (tests).
3. Pack `--puppet-pvp` (pack test: components present, byte for byte without it).
4. Glue: read the copy's record (the protocol 14 reader), send strikes; victim path B now (today's replay behind the guard
   check, which reads the guard from our Link's main AS name, any case containing "guard", as phase 3 reads "lift",
   and logs each name once for U5), path A when U2 is read; third-game replay on results.
Every game address U2/U3/U5 need is read before its glue lands; until then the glue logs and does nothing.

## 6. Risks

- **The game's physics fights the sync.** A character body under gravity falls between the hook's places
  (the held-bomb bug: the setter keeps the body's fall speed, reports/held-bomb.md). Answer: the copy's
  character controller uses the "Free" move state, the one the game gives NPCs placed on seats and
  walls (romfs `Phive/CharacterControllerParam/Npc_Kokiri_MC_ThroughGround`: `DefaultMoveState: Free`;
  `Npc_Kokiri_MC`: `OnGround.IsEnable: false`). UNKNOWN: that a Free body still blocks Link and carries
  him when he stands on it. Probe: phase 1's offline proof.
- **The copy's body pushes this game's Link**, or Link pushes the copy off its place: the copy is put back
  every frame, so a push shows as a jitter. The proof measures the copy's place read back against the
  sample (CoopSnap::Meter, issue 194 "snaps logged").
- **Two Links confuse the game** (option A only): camera, HUD, save, scene changes, events. Option C keeps
  the copy free of `PlayerRef`, `ActorNameRef Player` and the camera target, as today.
- **Null reads.** A copy with more components but not `PlayerRef` can meet code that reads its wearer's
  Player component (crash 2, main+0x2dceef8 reads +0x7d8, hs-puppets-alt-b). The module's lazy null page
  (hookshot/src/coop_null_page.h) stays armed; a new site is a log line (`unguarded null read`).
- **Enemies.** A copy with a body and sensors can be hit by this game's enemies. Enemy hits on a copy
  must be dropped (the owner game's enemies decide), or forwarded only as perception. Phase 4 filters
  by attacker affiliation.
- **Heap.** New files in the pack need size-table entries (issue 111: a short entry drops every create).
  The pack builder sizes them as the player files they are cut from (hs-puppets-alt-b's rule).

## 7. Phases

Sizes: S = under a day of one worker, M = 1 to 3 days, L = more. Every proof is an offline 2-game job
(lanes, docs/testing.md) with a screenshot on each game (no pass from log lines alone).

| # | Phase | Size | What | Offline proof (2 games) |
|---|---|---|---|---|
| 0 | Facts | S | One module-only research run: lift grades, the Free body test, stash path, throw velocity setter (the UNKNOWNs above) | Logs + screenshots of each probe; no product change |
| 1 | Body | S-M | Pack switch `--puppet-body` (built, below): the copy gets Link's own shapes and character body in Free move state, plus his cloth; no sensors, no ragdoll | A walks into B's copy: A stops (screenshot both); A jumps on the copy: A stands on it (A's y = copy's top, screenshot); B walks away: the copy's place read back = samples (no jitter lines). Same with an adult copy. Solo and idle unchanged (pack built without the switch is byte for byte today's) |
| 2 | Bomb hand-over | M | Bomb as a shared object (owner, epoch, fuse left, held by); lift of a copy asks for it; stash through the pouch path | A puts down a lit bomb, B lifts it: one explosion in both games at B's hands (screenshots both, the same frame window); A puts down an unlit bomb, B lifts and stashes: B's count +1, A's unchanged, the bomb gone in both |
| 3 | Lift a player | M-L | `LiftableRef` on the copy (Cucco's model); `LIFT_ASK` / `LIFT_ANSWER` / `LIFT_PLACE` messages (the throw is `LIFT_PLACE flying`); the lifted game places its real Link (built, not run: "Phase 3 status") | A lifts B's copy: B's own screen shows B in A's hands (screenshot), A walks, B follows; A throws: B lands where A's screen shows (within 1 m), both screenshots; B in a talk: the lift is refused |
| 4 | PvP on real bodies | M-L | Section 5 (design native-puppets-e): copy gets `Tgt`/`Atk` receipt sensors, `SensorHitRef`, an invincible `LifeRef` + `ReactionRef`, `AttentionSenderRef` (LockOn, AutoAim); the copy's hit record is sent as `PVP_STRIKE`; the victim's game decides (sensor path A or life path B, rewind check), `PVP_RESULT` to the scene; enemy hits on copies dropped | Section 5.9 (eight scenarios: lock-on, sword hit, guard, invincible frames, every weapon and item, downed, enemy and PvP off, a third game) |
| 5 | Option A track | L | native-player steps 2-5 (input wraps, correction) as a separate switch | As native-player.md |

Each phase is behind a switch, default off, and does not change the solo or idle game (no session: no copy
exists, so the pack switches change nothing without a co-op player).

## The phase 1 switch (built)

`tools/puppet_pack/build.py --puppet-body` (off by default). Every puppet (colours, skins and quiet copies
included) gets its own `PhysicsParam` and `ControllerSetParam` made from the player's own controller set
(its `$parent` chain merged), with only:

- `InitializerName`, `ShapeNamePathAry`, `MatterRigidBodyNamePathAry`, `CColEntityNamePathAry`,
  `PathCharacterController`: Link's own body shapes and character controller;
- `Cloth`, `ClothAdvandecOption`, `ClothList`, `ClothReaction`: his cloth (hair, hood, belt), as
  hs-puppets-alt-b's `cloth` mode, which ran without the slot-10 crash.

Left out: sensors (`Tgt`, `Atk`, `Chemical`: nothing hits or is hit by the copy yet), the ragdoll, the
rigid body entities (ride, elastic hands, reflect shield, liftable sweep), the controller entities, IK and
looking. Its character controller is the player's (merged) with `DefaultMoveState: Free` (the NPC
`ThroughGround` value) so it does not fall between the hook's places. The new files get size-table entries
as the player files they are cut from. The manifest (`coop_puppet.json`) records `"body": true`. No hook
change: the Director already places the puppet through the physics-aware setter.

Build for a proof (lane pair, offline job): `python -I -B tools\puppet_pack\build.py --romfs <the .nup>
--mod-dir <kit>\mods\contents\0400486024DEC000\coop-puppet --puppet-body` on both games, then phase 1's
checks. Back to normal: rebuild without the switch.
