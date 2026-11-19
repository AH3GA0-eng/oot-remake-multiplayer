# Native puppets: other players as real game bodies

Status: design and plan (native-puppets-a, 2026-11-19), plus the test switches `--puppet-body` (phase 1),
`LICHII_COOP_BOMB_SHARE` (phase 2) and `--puppet-liftable` + `LICHII_COOP_LIFT_PLAYER` (phase 3), each below. Nothing here is in a release. Today's puppet ([puppet.md](puppet.md)) and the real-Player test
([native-player.md](native-player.md)) stay as they are until each phase below is proven in two games.

## The goal

The other players stop being picture-only puppets and become things the game itself handles:

1. **Bodies.** You bump into another player, can stand on them, push them, and they block a doorway.
2. **Lift and throw a young player**, the way Link lifts and carries Ruto in Jabu-Jabu (owner rule: only a
   young Link can be lifted; any Link may lift him).
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
- liftable: a `LiftableRef` of its own, Ruto's, on the child copies only (phase 3).

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

**Owner rule (2026-11-19, absolute): only a YOUNG Link can be lifted, carried the way Link carries Ruto.**
"You should only be able to grab a young link, like how you grab ruto. You can basically give link the same
animation." An adult Link is never liftable (no lift reaction in the game, no `LIFT_ASK`, no message); a young
Link may be lifted by a young or an adult Link (the lifter's age is never a reason to refuse; the game's own
lift rules decide). The carry is Ruto's: the copy's `LiftableRef` is Ruto's own `LiftableParam`, so the
lifter's game plays Link's Ruto set of lift, carry, put-down and throw.

Needs, on the child copy, what Jabu-Jabu's Ruto (`Npc_Zora_Ruto_Child_001`) has: a `LiftableRef` (phase 3;
her param, own file in the puppet pack), and on the owner game a "lifted" state for its real Link.

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

- **Pack:** `tools/puppet_pack/build.py --puppet-liftable` gives every **child** puppet (`CoopPuppet_Child*`,
  `LIFTABLE_VARIANTS`) a `LiftableRef` of its own, Ruto's (native-puppets-d; before it the Cucco's):
  `Npc_Zora_Ruto_Child_001`'s `Component/LiftableParam/NpcZoraRutoChild.game__component__LiftableParam.bgyml`
  (`LiftGrade` 3069107721 as the Cucco's, `LiftableType` 3056776872, `LiftPosOffset`/`PutPosOffset`
  (0, -0.0022, 0.0501), `WeightForCalcThrowPower` 19, `SpecialDeal` [213340665], `IgnoreLifterOnThrownFrame` 8),
  its in-pack `$parent` chain merged (the game's file has none); own file
  `Component/LiftableParam/<puppet>.game__component__LiftableParam.bgyml`, sized in the size table as hers.
  Adult puppets get none: the game itself can never lift an adult copy. Off: the pack is byte for byte as
  before (test `test_pack_liftable`, which also checks the adult pack is the same on and off).
- **Ruto's carry (owner rule).** The lifter's game picks Link's animations by the liftable's type, and Link's AS
  (Player_Child and Player_Adult, both) has a Ruto set beside the Cucco's: `Lift_Ready_Ruto`, `Lift_Up_Ruto`
  (and `_High`), `Pose_Ruto` / `Pose_Ruto_Run` / `Pose_Ruto_Caution` (the carry), `Lift_Put_Ruto` /
  `Lift_Put_Stand_Ruto` (and `_High`), `Lift_Throw_Ruto`, `Damage_Though_Ruto*`. With Ruto's `LiftableType` on
  the copy the game plays those, with its own carry limits (walk, no roll or attack while carrying, put down or
  throw), as with Ruto. `OurLinkLifts` takes `Pose_Ruto` as a carry too.
- **The carried Link's pose: Ruto's own animation (native-puppets-d follow-up).** Ruto's carried pose is her
  AS command `Act_LiftRest` (`_St`, `_Wait`, `_Ed`, `_LookAround`, `_Detect`) in `AS/Npc_Zora_ZU_Ruto_Child.root.asb`,
  its loop the clip `Act_LiftRest_Wait` in `Npc_Zora_ZU_Ruto_Child_Animation.anim.bfres`. Link's AS has no such
  command, so the young copies get their own (`tools/puppet_pack/asb_carry.py`, with `--puppet-liftable`):
  - `AS/CoopPuppet_Carry_Child.root.asb`: a copy of `AS/Player_Child.root.asb` (from the user's romfs) in which
    the two clips of Link's `Eaten` command (`Eaten_F`, `Eaten_B`) name `Act_LiftRest_Wait` instead. The AS
    binary as far as this needs it (version 0x41b, read from the game's files): the string pool is the file's
    last section (offset header word 14, size word 23); a node's clip name is a u32 pool offset, and each of the
    two clip names is named by exactly one node field. The new name goes at the pool's end (no offset moves; 7
    bytes change and 18 are added on the game's file) and the two fields point at it. The child copies'
    `ASRef` names it (a skin keeps its own AS); the game's own Link, the adult copies and every other actor
    keep the game's file.
  - The copy's own AnimationParam (`Component/AnimationParam/<puppet>.engine__component__AnimationParam.bgyml`):
    the player's resource list plus `{"IsRetarget": true, "ModelProjectName": "Npc_Zora_ZU_Ruto_Child_Animation"}`,
    the way the game's own child Epona (`Animal_Horse_Child`) plays the adult Epona's clips. Link's and Ruto's
    skeletons name their main bones alike (`Arm_1/2_L/R`, `Leg_1/2_L/R`, `Spine_1/2`).
  - While lifted, the lifted game sends `Eaten` as its Link's animation (`CoopLiftPlayerGame::CarriedPose` in the
    hook's read, `CoopLiftPlayer::CarriedAnimation`), so every copy of it, on the lifter's screen and every
    other, plays `Eaten` from frame 0 at rate 1 (looped, not held on one frame), which on a young copy is
    Ruto's carry loop. Fallback: if the AS cannot be made (another
    AS version, a clip name missing or named twice) the build says so and the copies keep the game's AS, so
    `Eaten` plays Link's own Like Like hold (`Eaten_F`/`_B`), the closest pose Link himself has. One side effect:
    a young player really held by a Like Like also shows Ruto's pose on the copies (their copy's `Eaten`).
  - **The carried player's own screen: the exact limit.** Our own Link is the game's real `Player_Child`, on
    the game's own AS (changing that file would change solo play, which must stay untouched), and its pose is
    picked each frame by its own action AI (`Player.action.*`): no action of the player is "carried" (the
    nearest, `Player.action.reactionrestraintrideonenemy`, plays `Eaten` = the Like Like's hold). So on our own
    screen, while placed with input off, our Link plays what its own AI picks (likely its fall or float), not
    Ruto's pose. The probe (UNKNOWN 7): the lifted game logs `coop: lift: carried: our own Link plays '<X>'` once
    per change. If X is a fall or float, the native next step is asking our Link's action verb for
    `RestraintRideOnEnemy` through the game's own request-action-verb path (main+0xc08f0, as the downed crawl
    asks `Crawl`), which plays `Eaten`, and giving our own Link the carry AS only while that verb runs (a new
    probe: whether an actor's AS can be switched at run time); until that is proven, our own screen keeps the
    game's own pose.
- **Young only, on every side** (native-puppets-d, tests in lift_player_test.cpp and session_test.cpp):
  - Lifter: a copy counts only while its player's presence says child (`CopyView::young` from
    `CoopPuppet::Remote::young`, read every frame: the age can change, as at the Master Sword); an adult or an
    unknown age never asks and logs nothing. Asking or carrying when the copy stops being young: the lift ends
    (a `landed` place where it last was) and the director places the copy again.
  - Server: `on_lift` refuses a `LIFT_ASK` (`LIFT_ANSWER` 7 `not_young`, stat `lifts_not_young`) when the lifted
    player's last PLAYER_SAMPLE age is adult or not known (kept over scene changes, unset on a rejoin), and drops
    every `LIFT_PLACE` to such a player. Answers always go.
  - Lifted game: refuses `not_young` when its own Link is not child at answer time (the age read then, so an age
    change while the ask was on its way is caught); lifted, it frees itself (`LIFT_ANSWER not_young`) the frame
    its Link is not child: input back, no more places, its Link falls by its own character controller (the
    game's own drop).
  - Re-lift after landing, a throw (flying places), a scene change (an event or load frees it) and a rejoin (no
    age until a sample) all go through the same gates.
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

1. Whether the child and adult lifters lift a child copy with Ruto's `LiftableParam` at all (the copy has no
   Ruto AI; `SpecialDeal` 213340665, `IsResetBehaviorModeOnThrown` and `KeepSensorHitToParent` may need it), and
   that they play the Ruto set (`Lift_Up_Ruto`, `Pose_Ruto`) on it. Without
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
7. The carried pose: (a) that a young copy built with the carry AS plays Ruto's `Act_LiftRest_Wait` on Link's
   skeleton for `Eaten` (retargeted by bone name; the copy's heap holds the extra animation project), screenshot
   on the lifter's game; (b) a build log line `AS/CoopPuppet_Carry_Child.root.asb: ... Ruto's Act_LiftRest_Wait`
   (else the fallback note). The fallback is chosen at pack build time only: if the game fails to retarget
   Ruto's clip at run time (a bad pose, a T-pose, nothing played), no switch turns the carry AS off by itself;
   the cure is a pack built without it (the whole lift is off by default, `--puppet-liftable`). (c) what our own Link plays on its own screen while carried: the log line
   `carried: our own Link plays '<X>'`, and a screenshot of the lifted player's own screen.
8. That an adult copy really has no lift reaction (no `LiftableRef`: the lifter's button does nothing at it).

Proof job checks for the owner rule (add to the job in reports/native-puppets-c.md; screenshots of both games):
- A (adult, then young) lifts young B: both screens show B in A's hands with A in Ruto's carry pose
  (`Pose_Ruto`); A walks with B, puts B down, and throws B as Ruto is thrown. A's log has the Ruto lift names.
- Carried pose: see= A.png: B's copy in A's hands in Ruto's carried pose (sitting up, as Ruto sits on Link's
  hands), not Link's Like Like hold; see= B.png: B's own screen (what B's Link plays: B's log line
  `carried: our own Link plays '...'` names it). Both packs built with `--puppet-liftable`; the build log has the
  carry AS line.
- A tries to lift adult B: nothing happens on either screen (no lift, no message); A's log has no
  `coop: lift:` line for B; the server's `lifts_not_young` stays 0 (the ask is never sent).
- B ages while carried: in the game the age changes only in the Temple of Time's Master Sword event, which
  frees B first (the event gate). So A carries B into the Temple of Time and B takes or puts back the sword:
  B's log `freed itself from P1`, input back, B falls and lands by its own (screenshot B); A's log `landed`,
  A's carry ends; afterwards (B adult) A's lift at B does nothing. The `not_young` free itself is proven by
  the host tests (no game path changes the age outside that event).

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
blade points, 1.3 m) and sends `PVP_HIT` (242); the victim decides (guards, 0.8 s mercy) and replays the
hit on its own Link (`HurtOurLink`, the hit replay), then `PVP_HURT` (243) to all.

With sensors on the copy (phase 4) the **attacker's game detects the hit with the game's own
collision**: its sword's `Atk` shape touches the copy's `Tgt` shape and the copy's `SensorHitRef`
receives it (the game's hit info buffer, `TgHitInfoBufferSize 8`). The copy has no Life, so nothing
happens locally except the report: the module reads the received hit (attack kind, place, attacker) and
sends `PVP_HIT` with it. Shields: the copy carries the owner's guard state (pose sync already sends the
shield-up pose); the guard check stays the victim's.

- **Who decides a hit: the victim, with a rewind check.** The attacker sends the hit with its game time
  and the copy's place it saw. The victim game checks it against where its Link was at that time
  (rewind by the copy's delay: today's puppet is shown ~100 ms behind, coop_follow.h) within a margin,
  then applies it through the game's hit path, which plays knockback, guard and sound natively. Why the
  victim: it alone knows its Link's guard, invincibility frames, roll, and life; and the attacker's
  advantage (what you saw is what you hit) is kept by the rewind window.
- **Lag compensation.** A hit is accepted if the copy's place at the attacker's time is within
  `max(0.5 m, speed x one-way delay)` of the victim's own recorded place at that time (the victim keeps
  a 1 s ring of its own places). Older than 250 ms: dropped (as `kHitLateMs` 200 for enemy hits).
- **Knockback.** Played by the victim's real Link (`ReactionRef`), then shown to others by pose sync.
  No game moves another game's Link.

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
| 3 | Lift a young player (owner rule: young only, Ruto's carry) | M-L | Ruto's `LiftableRef` on the child copies only; `LIFT_ASK` / `LIFT_ANSWER` / `LIFT_PLACE` messages (the throw is `LIFT_PLACE flying`); young-only gates on the lifter, the server (`not_young`) and the lifted game; the lifted game places its real Link, its copies play the carried pose (built, not run: "Phase 3 status") | A lifts young B: both screens show B in A's hands with A in Ruto's carry (screenshot both), A walks, B follows; A throws: B lands where A's screen shows (within 1 m), both screenshots; A tries adult B: nothing happens, no message; B ages mid-carry: safe drop; B in a talk: the lift is refused |
| 4 | PvP sensors | M | Copy gets `Tgt`/`Atk` sensors + `SensorHitRef` (no Life); received hits read and sent as `PVP_HIT`; victim rewind check; enemy hits on copies dropped | PvP on: A's sword on B's copy indoors (no grass) -> B flinches and loses hearts (screenshot B), A's screen shows B's knockback; B shields: no damage; PvP off: nothing; an enemy hitting B's copy in A's game: no damage to B |
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
