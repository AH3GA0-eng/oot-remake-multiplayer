# The game's own UI

How the game's own UI system works, for building co-op's screens natively (issue #264, piece 1). The 1.0 goal: every
co-op screen is built from the game's own UI system and assets, and co-op is a tab in the game's own pause menu. Today
co-op's Multiplayer, Lobby and Mods tabs are in Longshot's own pause menu (Dear ImGui), not the game's.

Names, addresses, offsets and notes only: nothing from the game's files is here (the hard rule in
[CONTRIBUTING.md](../CONTRIBUTING.md)). Every function, vtable, global and string below is also in the names file,
[tools/game_scan/game_names.tsv](../tools/game_scan/game_names.tsv) ([below](#the-names-file)); call sites, vtable slot
words, hashes, segment bounds and the one address given as "near" are not.

## Addresses and conventions

- Game: version 1.1.1, main build id `e0f3ccfaf50065ae`, the only build co-op supports ([game-builds.md](game-builds.md)).
- Longshot loads rtld at `0x80000000` and main at **`0x80020000`**. `main+X` is `0x80020000 + X`: the title's vtable
  `0x84c106e8` is stored by `main+0x3406710`, which is `0x83426710`. `f_8xxxxxxx` is the function at that address
  (Longshot's names for the lifted code).
- On 1.1.1 main's text is `0x80020000..0x83c661d0`, ro `0x83c67000..0x8486c080`, data `0x8486d000..0x84d66b80`, and
  bss runs to `0x84ec0000`.
- Slot N of a vtable is the 8-byte word at vtable + N * 8. `+0x28` is a byte offset in an object; `*(x+0x28)` the
  pointer stored there; `**0x84c9e098` a global read twice.
- A screen is an object of a `UI...Screen` class. A layout is a Layout2 layout (`.buil` in a `.blarc` archive). A pane
  is one node of a layout; a part is a pane that holds another layout (names `Pa_...`).

Each fact ends with its status:

- **(checked)**: checked with the study tools: read on 1.1.1 with `tools/game_scan` ([game-study.md](game-study.md))
  and the readers in `tools/puppet_pack`; the address, slot, call or name is as written.
- **(code)**: from code, not run in a game by co-op: read in Longshot's code (GPL-3.0: `host/oot_menu.c`,
  `host/hos_wide_ui.c`, `host/oot_text_size.c`, `nx2/oot_wide.py`), which calls or reads it.
- **(notes)**: from the maintainer's notes: the maintainer's research notes and docs, mostly read from main's code and
  never run in a game; not checked again here.
- **(run)**: co-op's module used it in a game run; the doc named with it says which run.

## Screens

### The UIScreen table

- The game's screens are rows of a table in the romfs, `RSDB/UIScreen.Product.111.rstbl.byml.zs`: 62 rows on 1.1.1.
  Fields: `Name` (the screen's name, for example `Title_00`), `Class` (for example `UITitleScreen`), `BodyLayoutFile`
  (its layout), `ArchiveCategory` (a list), `LifetimeGroup`, `SortKey`, `DrawUnitId` (ints), `IsEnableControl`,
  `IsHeapReverse`, `IsPauseLower`, `IsAllocTextureDataOnInitialize` (bools) and `HeapInitialSize`. Not every row has
  every field. (checked)
- `UI/LayoutArchiveCategory.Product.100.byml.zs` lists the layout archives by category (`Boot`, `Common`, ...). 57 rows
  are in the `Common` category. 31 rows are `Resident`, 6 `NoUnload`, 25 have no group. (checked)
- Four rows have no `Class`: `AppLoadingMessage_00`, `MenuBG_00`, `SystemBG_00`, `TitleMenuBG_00`. They are
  backgrounds: a layout with no screen logic. A label or banner of ours could be such a row. (checked)
- In memory the rows are `0x38`-byte entries of a table object: `+8` the count, `+0x10` the array, `+0x18` hash buckets,
  `+0x24` the bucket count. An entry's `+0` is the row's `Name`, `+8` its `Class`. (checked)
- The table's vtables are `0x84c945d8` and `0x84c94708`: 30 slots each, the same functions in slots 4 to 29, not
  named by the game. Slot 4 `f_80aaf200` makes a screen (below), slot 5 `f_83beb570` gives a row's `Name`, slot 6
  `f_83beb590` the count, slot 8 `f_83beb5a0` looks a hash up, slot 17 `f_80aaf320` gives a row's `Class`. (checked)
- The game also names two table classes: `engine__rsdb__UIScreenTable` (vtable `0x84c9d190`) and
  `eui__UIScreenBaseTable` (`0x84c9d240`). How they relate to the two vtables above is not mapped. (checked)

### Factories

- Each screen class has one static factory object; the title's is `0x84e9ce18`, made at start-up by `f_80e4ee00`.
  (checked)
- A factory's vtable has 4 slots: 0 the destructor (`f_837a24c0` for every factory: it unlinks the factory), 1 the
  deleting destructor, 2 its name function (returns the class name), 3 `create(factory, heap)`. The title's:
  `0x84c10a50` with `f_837a24c0`, `f_834270a0`, `f_834270d0`, `f_834270e0`. (checked)
- The title's factory vtable comes right after its 107-slot screen vtable. Counted from the screen's vtable, the name
  function is "slot 111" (`0x84c106e8 + 111 * 8 = 0x84c10a60`); it is slot 2 of the factory's vtable. (checked)
- The factory base constructor `f_80e4ea00` links each factory into one global list: head `0x84ea4530` (next at `+8`),
  count `0x84ea4540`, node offset `0x84ea4544` (8: the node is at factory+8), guard `0x84ea4548`. Unlink:
  `f_837a2450`, `f_837a24c0`. (checked)
- `vtables.py` names the screen classes through their factories: it names 58 `UI...Screen` vtables, all with 107 slots
  (one base class). The full list is in the names file. (checked)

### Making a screen

- Slot 4 of the table, `f_80aaf200(table, heap, index)`, takes the row's `Class` (slot 17), walks the factory list
  comparing each factory's name (its slot 2) with `strcmp`, and tail-calls the match's slot 3 `create(factory, heap)`.
  It returns 0 when no factory has that name. (checked)
- A create, for example `f_834270e0`, calls the game's allocator `f_8005ebe0(size, heap, alignment)` with
  `(0x2f0, heap, 8)`, then the constructor `f_83426710`, which stores the vtable `0x84c106e8`. So a `UITitleScreen` is
  `0x2f0` bytes. (checked)
- So a screen is made once, when its row is loaded, and kept in the UI manager's array. (checked)
- For co-op: a new screen can be one more row in the table, generated from the player's own game at pack time, with an
  existing `Class` and a layout of ours as `BodyLayoutFile`. The game then makes it as it makes its own. Not tried yet.

### The UI manager

- `**0x84c9e098` (`main+0x4c7e098`) is the UI manager: `+8` the screen count, `+0x10` the screens' array, `+0x48` the
  table the name lookup uses. (checked)
- A screen's layout is `*(screen+0x28)`. (code, `oot_menu.c`; the maintainer's notes say the same)

### Opening a screen by name

The Sequence node `SeqOneShotOpenScreen` (vtable `0x84bd4a98`; its properties `ScreenName` and `Option` are bound in
slot 28 `f_80997670`) opens a screen in its slot 39 `f_80c86420` (checked):

1. `mgr = **0x84c9e098`;
2. `h = f_801939e0(ScreenName, 0)`: MurmurHash3 x86_32 with seed 0 (constants `0xcc9e2d51`, `0x1b873593`,
   `0xe6546b64`). It gives `0x6d47deab` for `SystemWindow_00` and `0xe1932d84` for `TitleSystemWindow_00`;
3. `index` = slot 10 of the table at `mgr+0x48`, called with `h`;
4. `screen = mgr->screens[index]` (0 when out of range), then slot 5 `open(screen, mode)`, with mode 2 for `Option` 0,
   3 for `Option` 2, else 1.

- Screen names are not strings in main: the Sequence graphs (`romfs/Sequence`) name them, and code finds a screen by the
  hash of its name. Co-op can open any loaded screen the same way: hash the name, look it up, call slot 5. (checked;
  not run)
- Two more lookups: `f_82919fb0(*(*0x84c9e098), hash)`, used with the two system window hashes above, and
  `f_832e4ed0(mgr, key)`, used with `0xc5a49707` for the banner. (notes) `0xc5a49707` is not the MurmurHash3 of
  `SimpleInfo_00` (checked), so the key `f_832e4ed0` takes is still to find.
- Another way, from the notes: walk the manager's array (count `+8`, array `+0x10`) and match each screen's vtable.
  (notes)
- The game opens screens from its Sequence graphs, not from C++: `OpenTitle.module.ainb` loads and opens `Title_00`,
  `SeqOneShotOpenSystemWindow` opens a system window with a preset, `Event.module.ainb` opens `MessageLog_01`, and
  `CloseHUD` / `OpenCloseUnuseUIFor*` hide the HUD's screens in events and menus. (notes)
- The other screen nodes, by vtable (checked): Sequence `SeqOneShotLoadScreen` `0x84bd3448`, `SeqOneShotCloseScreen`
  `0x84bce368`, `SeqExecuteLoadAndOpenScreen` `0x84bc67d8`, `SeqExecuteOpenScreen` `0x84bc7360`,
  `SeqExecuteCloseAndUnloadScreen` `0x84bc4cb8`, `SeqOneShotUnloadScreen` `0x84be25b0`, `SeqOneShotOpenSystemWindow`
  `0x84bd4be8`; AI `OneShotLoadScreen` `0x84a2b688`, `OneShotOpenScreen` `0x84a32c88`, `OneShotCloseScreen`
  `0x84a17c88`, `OneShotUnloadScreen` `0x84a61c70`, `ExecuteOpenScreen` `0x849d3058`, `OneShotOpenSystemWindow`
  `0x84a32f28`, `OneShotRequestOpenSimpleInfo` `0x84a48310`. Their class's own slots are 3, 28 and 39, as for the open
  node. Those slots lead to where the game loads, closes and unloads a screen by name: not followed yet.

### Closing a screen

- Slot 6, `f_802f6990(screen, -1)`, the same function in every screen looked at. (checked) The message window closes
  itself this way when its timer runs out. (notes)
- A system window is closed only by a Sequence that waits on it. (notes)
- A screen's open state may be the bytes `+0x1a5` / `+0x1a6`: to check in a game. (notes)

### Shared slots

Every screen vtable has 107 slots. Slots 5, 6, 17 and 40 hold the same function in every screen vtable looked at (ten
classes for 5, 6 and 17: the title, the pause menu, the tab strip, the HUD, the balloons, the banner, the message
window, the system window, the options and the save screen; seven of them for 40). The slots co-op uses:

| Slot | Function | What it does |
|---|---|---|
| 0, 1 | the class's | destructor, deleting destructor (checked) |
| 5 | `f_804d6100` | open `(screen, mode)`; the open node passes 1, 2 or 3 (checked) |
| 6 | `f_802f6990` | close `(screen, -1)` (checked) |
| 17 | `f_802f1380` | update, each frame (below) (checked) |
| 40 | `f_802f12b0` | a step delta from `*(screen+0x20)+0x68`; the message window counts its timer down by it (checked; the meaning: notes) |
| 46 | the class's | on focus: a button or pane got the focus (checked for the title and the pause menu) |
| 50 | the class's, base `f_8379bd50` | on decide; the base one does nothing (checked) |
| 93 | the class's | set-up, after the screen's layout is made (checked) |
| 96 | the class's | a per-frame step; the title's "active" step (checked) |

### Slot 17: the update

- `f_802f1380(screen)` runs each frame, before the screen's UI controller (`screen+0x38`) runs the buttons' requests. It
  runs for the HUD's, the menus' and the title's screens, on several game threads. Longshot's `oot_menu.c` hooks it to
  track the screens. (code)
- It drains the requests a Sequence node posts on the screen (`f_81006110`, see [Buttons and focus](#buttons-and-focus)).
  (notes)

### Slot 93: the set-up

- Every screen sets itself up in slot 93, after its layout is made: `UITitleScreen` `f_834267d0`,
  `UIMenuOverlayScreen` `f_83405cc0`, `UIPlusMenuScreen` `f_80f99400`, `UIMapMenuScreen` `f_833fed60`. (checked)
- The title's set-up, as an example: its animations by name (`f_8045c7a0`) into `+0x2b0`, `+0x2b8`, `+0x2c0`; it counts its
  `Pa_Button_%02d` parts (`f_8045e140` until 0) into `+0x2c8` (count) and `+0x2d0` (array); it makes each button with
  `f_80df5410(*(screen+0x38) the UI controller, pane)` and sets `0x20` in `button+0x40`; it puts the focus on button 0
  with `f_8088ffc0(screen, button, 0)` (`+0x2e0` the focused button); `+0x2d8` is the `N_BtnPos_00` pane. (notes)

### The screens co-op cares about

All vtables (checked); a full list of the 58 is in the names file.

| Class | Vtable | Screen and layout |
|---|---|---|
| `UITitleScreen` | `0x84c106e8` | the title, `Title_00` ([hookshot.md](hookshot.md#the-multiplayer-tab)) |
| `UIPlusMenuScreen` | `0x84c0baa8` | the pause menu, `PlusMenu_00` |
| `UIMenuOverlayScreen` | `0x84c08aa0` | the pause menu's tab strip and the panel's frame, `MenuOverlay_00` |
| `UIMapMenuScreen` | `0x84c08300` | the HUD and its maps, `MapMenu_00` (the Map page has no screen of its own) |
| `UIImportantMenuScreen` | `0x84c07798` | the Key Items panel |
| `UIChallengeMenuScreen` | `0x84c04a18` | the Threads of Time panel (list `N_SlideList_00`) |
| `UIShortCutScreen` | `0x84c0d430` | the R button's item choice (list `N_SlideList_00`) |
| `UISystemWindowScreen` | `0x84c0fbf8` | the system dialogs, `SystemWindow_00`, `TitleSystemWindow_00` |
| `UISystemSaveLoadScreen` | `0x84c0f840` | the save and load questions |
| `UISystemLoadListScreen` | `0x84c0eba0` | the save list |
| `UISystemOptionScreen` | `0x84c0f108` | the options, `SystemOption_00` |
| `UIMessageInGameScreen` | `0x84c09678` | the message window, `MessageInGame_00` |
| `UISimpleInfoScreen` | `0x84c0d7c8` | the banner, `SimpleInfo_00` |
| `UIBalloonMessageScreen` | `0x84c042e8` | the balloons over actors, `BalloonMessage_00` |
| `UIMessageLogScreen01` | `0x84c09a10` | the dialogue log, `MessageLog_01` |
| `UIDemoNicknameScreen` | `0x84c056b8` | the big name card, `DemoNickname_00` |

## Layouts and panes

- Layouts are in the romfs, `UI/Layout2Archive/*.blarc.zs` (SARC, zstd), their textures in `.bntx` files. The groups are
  `Boot`, `Common`, `Title` and `ZeldaNotes`; `Common` holds 150 layouts. (notes)
- Every layout's canvas is 3840 x 2160. (code, `hos_wide_ui.c`) Places are from its middle, y up: the HUD's
  `P_MiniMapFrame_00` is at 1536, -690, right of and below the middle ([overlay.md](overlay.md#players-on-the-map)).
  (run)
- A layout object: `+0xf8` its resource (the file's header: `+0x18` its name, `+0x20` its pane names), `+0x100` the pane
  array, `+0x13c` the pane count. A pane is `0xb0` bytes. (code, `hos_wide_ui.c`)
- A pane: `+0x18` its layout, `+0x28` its global 3x4 matrix (by column; xx at `+0x28`, yy at `+0x38`, the translation
  at `+0x4c`, `+0x50`), `+0x58` / `+0x5c` its width and height, `+0xa8` its flags. Kinds: 0 null, 4 scissor, 5 capture,
  6 part, 7 host (holds other layouts). (code, `hos_wide_ui.c`)
- A pane's `+0x78` is its alpha with its parents': 0 hidden, fading to 1 as the game shows it (seen in run
  hs-minimap-icons 20261114-105315). The matrix at `+0x28` is proven in run hs-minimap-icons 20261114-115734. Both in
  [overlay.md](overlay.md#players-on-the-map). (run)
- A matrix of all 0 is a pane the game has not shown since its screen came up. (notes)

### Finding a pane

- `f_8078e2b0(layout, name)` gives the pane, or 0. Longshot's `oot_menu.c` and co-op's map page use it. (run,
  [overlay.md](overlay.md))
- More lookups by name: `f_8045e140(layout, name)` (the title counts `Pa_Button_%02d` with it until 0) (notes),
  `f_8045e1e0(layout, name)` (the map set-up) (notes), `f_8045c4a0(name, layout)` (the tab strip counts its parts with
  it) (checked). Animations by name: `f_8045c7a0(layout, name)`. (notes)
- A pane's position: `f_8078fcb0(pane, 0, &x, &y)`. (code, `oot_menu.c`)

### Transforms, size, alpha

- `f_8154caa0` tests a pane's dirty bit 6 and falls into `f_8154cab0`, which makes the pane's global matrix from its own
  position, scale and rotation and its parent's matrix. `f_8154cab0` is an entry inside `f_8154caa0`, not a function
  start of its own. (code, `hos_wide_ui.c`, `oot_wide.py`)
- `f_8154ac30` gives a pane's size as drawn (s0, s1). (code, `hos_wide_ui.c`)
- `f_8154b220` scales a text pane (the title button's texts). (notes)
- Moving and showing a pane, as the balloons do: `f_80229650(part+0x40)` gives a balloon's root pane; it is moved as
  `f_8022a3e0` does (x, y at `+0x6c`; `f_815457d0` and `f_8154d540` tell the layout) and shown with `f_8022aea0`. A
  part's in and out animations: `f_802f0f00`, `f_802f0fe0`. (notes, never run)

### Picture panes

- A picture pane's material is `*(f_8045b700(pane) + 32)`. `f_80e6b660(material, &slot, index)` puts a texture on it:
  it stores a texture descriptor slot at `*(material+48) + index * 0x50 + 72` and marks the material dirty
  (`f_8150a1d0`). (notes)
- The slot comes from a TexRef `{u16 w, u16 h, u32, i64 slot}`: from a texture object by `f_80e6b700`, or at a
  resource's `+56` by `f_80e6b650`. A texture gets its slot from `f_800634b0` (the graphics manager `**0x84cacc98`).
  (notes)
- The map screen loads its `P_Replace*` pictures this way (`f_80b4a840`). A texture of ours made by the CPU has no known
  creator yet. (notes)

### The layout files

`tools/puppet_pack/title_menu.py` reads and writes a `.buil` layout: its docstring has the format (the pane and part
records, and how new parts go after the string pool so nothing moves).

## Buttons and focus

- A screen's button list: `+0x88` the head, `+0x90` the first, `+0x9c` an entry's offset from its node; `entry+0x18` the
  button. A button's `+0x38` is its state (non-zero: focused or shown), `+0x58` its boxes, `0xb0` apart: the first is
  its hit box (matrix `+0x28`, rectangle `+0x84` x, y, w, h about its origin). (code, `oot_menu.c`)
- The focus: `f_8088ffc0(screen, button, flag)`, as the game's screens call it (`f_80795090`). It finds the button's
  entry and hands it to the focus manager (`*(screen+0x20)+0x60`, through its vtable's `+0x30`) when that manager takes
  the player's input. (code, `oot_menu.c`)
- A button is made by `f_80df5410(*(screen+0x38) the UI controller, pane)`. Bit `0x4` of `button+0x40` is "enabled"; the
  title sets it from `f_837930f0(controller)`. (notes)
- A button's name: `*(button+0x18)` is its pane's name, `+0x20` its parent pane; `f_837a28f0` joins them into a path
  through `f_8045dc00` (names at `+0x40`, up at `+0x30`). This is the button object, not a Layout2 pane (whose `+0x18`
  is its layout); `oot_menu.c` reads `+0x18` as a name only on buttons. (notes; checked against `oot_menu.c`)

### What a press does

- What a press does is the Sequence graph's, not C++: the title's buttons lead where `NormalTitle.module.ainb` says.
  (notes)
- `SeqExecuteWaitScreenButtonInput` (vtable `0x84bcabc0`) enters in slot 29 `f_832e8ad0` and posts a request on the
  screen with `f_81006110`. The screen's update `f_802f1380` drains it; `f_83399160` reads the UI controller's decided
  button (`f_83793090`; the button's state `+0x38` is 4 or 5). (notes; the slots checked)
- A request: `+0x3c` its kind (2: stop an animation), `+0x28` an animation name or a pane path, `+0x30` a message ref,
  `+0x38` a frame (float). One note puts the request list at `screen+0xb8`, another at `screen+0xe8`: to check. (notes)
- `f_83791db0(button, 0)` decides a button without input. (notes) The game calls it from four places, among them the
  step (slot 96) of the save screen (`f_834251f0`, at `0x83425280`) and of the system window (`f_834258b0`, at
  `0x83425930`). (checked)

### The title's buttons

- `UITitleScreen` keeps `+0x2c8` its button count, `+0x2d0` its buttons, `+0x2e0` the focused one, `+0x1a8` 1 while it
  runs ([hookshot.md](hookshot.md#the-multiplayer-tab), "How it is found in the game"). (notes)
- `Title_00` has four button parts `Pa_Button_00..03`; the game shows three, by the frame of its `BtnNum` animation
  (4 tracks; frame N shows N rows). (notes)
- Its slots: on focus 46 `f_83426f40` (plays the focus sound, sets `+0x2e0`), active step 96 `f_83426d70` (each button's
  enabled bit; the active sound once), on decide 50 the base `f_8379bd50`, which does nothing. (checked)
- Co-op's patch 0004 calls the module from three places in it: its set-up (at `0x83426940`), its active step
  (`0x83426d70`) and its deleting destructor (`0x834267a0`) (`hookshot/src/hos_coop_api.h`). (notes)

### Who gets the pad

- Co-op's side: Longshot hands the pad to the module first (patch 0004), and keys and characters through its key hooks;
  while co-op's menu is open the game gets no presses ([hookshot.md](hookshot.md)). (notes)
- The game's side, how it picks the screen that reads the pad (its focus manager and the screens' order), is not mapped.

## Text

- The texts are in one archive per language, `romfs/Mals/<language>.Product.110.sarc.zs`, 16 languages. A layout's
  texts are in `LayoutMsg/<layout>.msbt`. Every message file of 1.1.1 is UTF-8 with only the `LBL1` and `TXT2` sections
  (`tools/puppet_pack/msbt.py`). (checked)
- A message ref is 24 bytes. From a label: `f_80a86f70(ref)` sets it to `0, 0, -1`, then
  `f_80a8afe0(ref, "LayoutMsg/<layout>", label, 1)` points it at the label. (notes; co-op's title code calls both,
  `hookshot/src/coop_title.cpp`)
- Setting a pane's text: `f_80a87d30(layout, "Part/Pane", &ref, 1, 0)` for a path, `f_8045b970(layout, "Pane", &ref, 1,
  0)` for a pane of the layout itself (layout `*(screen+0x28)`). One note gives `f_80a87d30` three arguments; co-op's
  code passes five. (notes)
- A string of ours: the ref `{const char *utf8, u32 bytes, i64 -1}`. The text box keeps the pointer:
  `f_80459680(textbox, ptr, len, -1)` stores it at `+0xf0` and the length at `+0x124`. So the text must stay alive while
  it is shown: take it from the game's malloc, `f_814e6c90` (`main+0x14c6c90`). (notes, never run)
- Pane name strings in main: `T_Name_00` `0x83d141c6`, `T_Message_00` `0x83d4fef3`, `T_Text_00` `0x83de2306`. (checked)
- Tags in the text: byte `0x0E`, then u16 group, u16 type, u16 parameter size, the parameters. A colour is group 0 type 3
  with a u16 index; the game uses only 0 to 5 and `0xFFFF` (back to the default). A player's text must be cleaned of
  `0x0E` and control bytes before it is shown. (notes)
- Labels the game uses: `Title_00` 0000 to 0002 (co-op adds 0100 to 0102); `SystemWindow_00` the dialog's text as
  `%04d` and its buttons as `10%02d`; `SystemOption_00` `<Key>_Title`, `<Key>_Caption`, `<Key>_00...`; the banner's
  texts in `SimpleInfo_00` by type. (notes)
- Dialogue text panes (Longshot `oot_text_size.c`): a `LetterAnimControl` (panes with letter animation), its step
  `f_80b1e140`; the control's `+0x28` is the game's pane wrapper: `+8` the Layout2 text box pane, `+0x28` / `+0x38` the
  text's 2D scale. (code)
- Still open: the text pane's own colour setter (16 player colours, the tag has 6), and one run of the string route.

## Sounds

- The UI sounds are played by name: `f_80ac65d0` (`main+0xaa65d0`), x0 = `*(*(screen+0x198)+8)`, x1 = the name, x8 = a
  result buffer (point x8 at free stack). (notes, never heard in a run)
- Names: `mc_button_on` (focus moves), `mc_NormalDecide` and `mc_GameStartDecide` (decide: the save screen's on decide
  `f_83425320`, slot 50, plays one of them), `mc_OpenDetail` (open), `mc_ReturnFromDetail` (back), `mc_Title_Active`
  (the title's active step). (notes; the slot checked)
- A game button plays its own decide sound with its `Decide` animation, so a button of ours made from the game's button
  part gets it too. (notes)
- Still open: a cancel, close or error sound name.

## The pause menu

### The pages

- `PlusMenu_00` (`UIPlusMenuScreen`, `Resident`, `IsEnableControl`; its layout in the `Common` archive) holds the four
  pages as parts `Pa_Collect_00`, `Pa_Equip_00`, `Pa_Item_00`, `Pa_System_00` under `N_MenuChange_01`, and the info and
  message boxes `T_Text_00`, `T_Text_01`. 40 panes. (checked)
- Its set-up, slot 93 `f_80f99400`, binds the four pages by name in four blocks: `Pa_Item_00` (string `0x83cd6111`) at
  `+0xf0`, `Pa_Equip_00` (`0x83d46f80`) at `+0x130`, `Pa_Collect_00` (`0x83d2da2e`) at `+0x170`, `Pa_System_00`
  (`0x83d20bda`) at `+0x1b0`. `f_80f9b1d0` also reads `Pa_Collect_00`. (checked)
- Its slot 46, `f_802ed660` (the word at `0x84c0bc18`), handles a pane getting the focus: it matches the pane's name
  against `PaItemMenu_00`, `PaEquipmentMenu_00`, `PaCollectMenu_00`, `PaSystemMenu_00` and hands the pane to that page
  (`f_802ebd20` on the Items page: the info panel). Longshot's `oot_menu.c` hooks it. (checked slot; code)
- The pages in order: 0 Collection, 1 Equipment, 2 Items, 3 System; L and R step between them. (code, `oot_menu.c`)
- So a fifth page needs code: a hook after slot 93 that binds a fifth part, and the L and R step that wraps at four.

### The tab strip

- The tab strip is another screen: `MenuOverlay_00` (`UIMenuOverlayScreen`, `Resident`). Its header `N_Header_00` /
  `P_MenuHeader_00` holds four tab parts `Pa_Category_00..03`, the L and R labels `T_BtnL_00`, `T_BtnR_00`, the control
  guide `Pa_Guide_00..04`, and the heart and magic gauges. `MenuBG_00` (no `Class`, `IsPauseLower`) is the background.
  (checked)
- The tabs count themselves: the set-up, slot 93 `f_83405cc0`, formats `Pa_Category_%02d` (string `0x83cefc97`) for 0,
  1, 2, ..., finds each in its layout with `f_8045c4a0(name, *(screen+0x28))` and stops at the first one missing. The
  count goes to `screen+0x328` and sizes the arrays it then allocates. (checked)
- So a `Pa_Category_04` part added to `MenuOverlay_00` at pack time gives a fifth tab with no code change. Not tried yet.

### The Map page

- The Map page has no screen of its own: its pictures are kept by the HUD's `UIMapMenuScreen`. Its set-up, slot 93
  `f_833fed60` (checked), keeps 18 map panes from `+0x1470`: `P_ReplaceMap_00` `+0x1470`, `P_ReplaceMapAll_00`
  `+0x1478`, `P_ReplaceField_00` `+0x1490`, `P_ReplaceFieldMini_00` `+0x1498`, `P_ReplaceDungeon_00` `+0x14a8`,
  `P_ReplaceDungeonMini_00` `+0x14b0`; it finds them through a table of names at `0x83f36250` with `f_8045e1e0`. Five
  picture holders of `0x2e8` bytes start at `+0x3c8`. (notes)
- The HUD's own map copy: `+0x1368` the floor shown, `+0x1500` the floors, `+0x1538...` the map, compass and boss key;
  `f_83404200` refreshes it. (notes; a call from co-op's module refreshed it in a run)
- The map pictures' panes `N_PatternField_00` and `N_PatternDungeon_00`, the HUD's minimap panes, and which game menu
  is in front (Longshot's patch 0018, `hos_coop_menu_state`) are in [overlay.md](overlay.md#players-on-the-map), which
  says what ran. (notes)

### Still open

Where the L and R step wraps at four pages, and how the tab strip's highlight follows the page.

## Dialogs, message window, banner, balloons

### The system window

- `UISystemWindowScreen` (`0x84c0fbf8`). On the title the screen is `TitleSystemWindow_00` (hash `0xe1932d84`), in play
  `SystemWindow_00` (`0x6d47deab`). The Sequence opens it with `SeqOneShotOpenSystemWindow` and a preset: `NewGame`,
  `ResetOption`, `BackToTitle`, `BackToTitleFromGameOver`, `NewGameMQ`, `ReleaseMQ` (a compiled enum near `0x83f0a9a0`,
  a table of label numbers just before it). (notes)
- `f_83425a00(screen, preset, is_title)` sets `T_Text_00` (label `%04d`) and the buttons (`f_83425bc0(screen, button,
  n)`: `Pa_Button_NN/T_Btn_00`, label `10%02d`; the `BtnNum` frame 1 or 2 through `f_802f2d90`; the focus through
  `f_8088ffc0`), then opens it. Nothing closes it unless a Sequence waits on it. (notes)

### The save screens

- `UISystemSaveLoadScreen` (`0x84c0f840`): its on decide, slot 50 `f_83425320`, plays a decide sound. (checked slot;
  notes) `UISystemLoadListScreen` (`0x84c0eba0`) is the save list.
- What the save list shows comes from the game's caption manager ([hookshot.md](hookshot.md#the-save-list)): the
  GOT entry `0x84ca1660`, read by `f_80f8da00` (the body of `SeqOneshotRequestLoadAndSerializeCaption`), which calls the
  re-read `f_80f8da40` (checked); the idle test `f_833cd380` and the list reset `f_80bcfd40`. (notes)

### The options

- `SystemOption_00` (`UISystemOptionScreen`, `0x84c0f108`): a slide list `N_SlideArea_00` made from the templates
  `Pa_ToggleBtn_00`, `Pa_SelectBtn_00`, `Pa_AmiiboBtn_00`, plus a caption bar and a scroll bar. Its keys are a compiled
  enum at `0x83f33790` (18 entries), not data; `f_832fedb0` names the `OcarinaKeyAssign` key. The code that builds the
  list from the enum is not found. (notes)

### The message window

- `MessageInGame_00` (`UIMessageInGameScreen`, `0x84c09678`): `T_Name_00` the speaker, `T_Message_00` the text.
- The game's call: `f_833d25c0(mgr = **0x84c9e908, &file, &label, &speaker, s0 = time)`. It locks `mgr+0x6218`, gets
  the screen with `f_833d2690(*0x84ea4518)`, has `f_834091d0` put the label's text into `screen+0x2b0`, the speaker into
  `T_Name_00` and the time into `+0x408`, sets `+0x2d0` to 1 (`f_83409410`) and opens it with slot 5 `(screen, 1)`.
  (notes)
- Each frame its slot 96 `f_83409420` (checked) puts `+0x2b0` into `T_Message_00` (`f_83409510`) while `+0x2d0` is 1.
  `+0x408` counts down by the slot 40 step (its unit is not known; the game's default is 180); at 0, slot 6
  `(screen, -1)` closes it. (notes)
- A line of ours, with no message label: our ref at `+0x2b0`, the speaker through `f_8045b970` on `T_Name_00`, the time
  at `+0x408`, `+0x2d0` = 1, then slot 5 `(screen, 1)`. (notes, never run)

### The banner

- `SimpleInfo_00` (`UISimpleInfoScreen`, `0x84c0d7c8`): one `T_Text_00`. `f_83419b10(screen, type, &str, flag)` sets
  it from `LayoutMsg/SimpleInfo_00` by type; slot 5 with mode 2 or 3 opens it. A text of ours: `T_Text_00` through
  `f_8045b970`, then slot 5. (notes, never run) Its AI node: `OneShotRequestOpenSimpleInfo` `0x84a48310`. (checked)

### The balloons

- `BalloonMessage_00` (`UIBalloonMessageScreen`, `0x84c042e8`): 16 parts `Pa_BalloonMessage_%02d`, made by
  `f_807906c0`, each with `T_Text_00` and `N_Position_00`. Its step `f_8022b900` places each over its actor with
  `f_80235ec0(out xy, &world xyz, &depth)` (true when on screen), the game's camera being `*(*0x84ea1cf0+0x28)`.
  (notes)
- Still to find: the part's own fields (entries `0x88` apart from `+0xc60` / `+0xe60`, an actor handle at `+0x10` /
  `+0x18`), or the request path of the AI node `OneShotRequestBalloonMessage` (`0x84a45f98`, checked). (notes)

### The message log and others

- `MessageLog_01` (`UIMessageLogScreen01`, `0x84c09a10`): `N_LogList_00`, `PaMessageLogText_00`,
  `PaMessageLogCharacter_00`, `PaMessageLogChoiceList_00`; `Event.module.ainb` opens it. Its entry list is not found.
  (notes)
- `DemoNickname_00` is the big name card; `PickUpWin_00` a small notice. `PaActorName_00` is the pause menu's item info,
  not a label for the world. (notes)

## The names file

[tools/game_scan/game_names.tsv](../tools/game_scan/game_names.tsv) names the game's functions, vtables, globals and
strings, so the raw `f_...` addresses in code and docs have a name and nobody maps the same function twice. One line
per address, tab-separated, sorted by address, each address once:

| Column | What |
|---|---|
| `address` | `0x8` and 7 lowercase hex digits, inside main on 1.1.1 |
| `name` | our name: `Class::Method` when a vtable or a name function gives the class, else a short description |
| `kind` | `function` (in text), `vtable` (ro or data), `global` (ro, data or bss), `string` (ro) |
| `status` | `checked`, `code`, `notes` or `run`, as in [Addresses and conventions](#addresses-and-conventions) |
| `note` | what it is, and its source |

The file is beside the study tools so a change to it runs its test in CI (the Build workflow skips changes under
`docs/`). `tools/game_scan/tests/test_game_names.py` checks the format: the header, the addresses, the order, each kind
in its segment, names and notes not empty. Add a line for each function you map, with its status as it is.

## What is open

1. Which existing screen class is the best host for an interactive page of ours.
2. The load, close and unload by name behind the screen nodes' own slots, and the key `f_832e4ed0` takes.
3. Where the pause menu's L and R step wraps at four pages, and how the tab strip's highlight follows the page.
4. The text pane's own colour setter, and one run of our own string in a pane.
5. A cancel, close or error sound name; hearing the sounds in a run.
6. How the game picks the screen that reads the pad.
7. A text field with the game's look (the game has no keyboard of its own): a frame or field pane to use, and a caret.
8. Smaller ones: a screen's open state bytes (`+0x1a5` / `+0x1a6`), the request list's offset (`+0xb8` or `+0xe8`), the
   message window timer's unit, the balloon part's fields, `MessageLog_01`'s entry list, the options list's builder,
   the packed track target of the title's `BtnNum`, and for picture panes a CPU-written texture and a host pane's
   draw order.
