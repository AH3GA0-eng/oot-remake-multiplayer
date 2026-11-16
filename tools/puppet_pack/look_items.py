"""Gear colours on the puppets (docs/puppet.md, "Gear colours on the puppets").

A puppet's shield, sword, boots and gauntlets are the game's own item actors, and every Link on
screen draws the same item with the same textures. So colouring one player's shield would colour
everyone's. The pack therefore makes SLOTS copies of each coloured item, each with textures of its
own:

- the actor `<item>_Look<k>` (k = 1..SLOTS): the game's pack of the item with its ActorParam under
  the new name and a ModelInfo naming the model project `<item>_Look<k>`; its ActorInfo and
  GameActorInfo rows are the item's, renamed;
- that project's models: the item's model files with every gear texture name (look_live.GEAR)
  changed to the slot's (`Mt_...` -> `C<k>_...`, as long, so nothing in the file moves);
- the slot's textures `Tex/C<k>_....bntx`: the game's texture under the slot's name, the game's own
  texels.

Each slot has two copies (COPIES_PER_SLOT; copy k = 2 * slot - 1 or 2 * slot). A player who colours
a part gets a slot (lichii/hook/coop_look_share.h); the co-op module paints the slot's copy that is
not worn in their look and Longshot's texture overrides (patch 0020) draw that copy's textures in
it (hookshot/src/coop_look_live.cpp, nothing of the game is written); then their puppet wears
that copy (lichii/hook/coop_puppet.cpp).
Nothing from the game is kept in the tool: everything is made from the user's own romfs.
"""
from . import byml, sarc
from .look_live import GEAR, mark_bntx
from .tunic import COLORS, TunicError, rename

SLOTS = 4
COPIES_PER_SLOT = 2
MAX_SLOTS = 4  # copies 1..8: the texture prefix C<k>_ has one digit

# The coloured items (hookshot/src/coop_look_parts.cpp names their textures). The Kokiri boots are
# in the player's own model, not an item: see BODY_BOOTS.
ITEMS = (
    "Weapon_Shield_Hylian",
    "Weapon_Shield_Mirror",
    "Weapon_Shield_Deku",
    "Weapon_Sword_Kokiri",
    "Weapon_Sword_Master",
    "Weapon_Sword_Biggoron",
    # The Giant's Knife and the broken one are their own actors on the Biggoron model project
    # (their ModelInfo names it, the Giant's through `$parent`): copies of their own, or a puppet
    # holding one wears the game's actor, whose blade shows only while your own sword is out.
    "Weapon_Sword_Giant",
    "Weapon_Sword_BrokenGiant",
    "Armor_Adult_HoverBoot",
    "Armor_Adult_HeavyBoot",
    "Armor_Adult_Gauntlet_Silver",
    "Armor_Adult_Gauntlet_Gold",
    "Armor_Adult_Goron",
    "Armor_Adult_Zora",
)

PREFIX = "Mt_"

# Blade copies (held-piece). A sword's blade is a material switch kept with its model resource,
# which every actor of one name shares (docs/puppet.md, "Sword not seen in the puppet's hand"):
# each wearer's weapon update sets it from its own state, so a puppet in the game's own sword
# shows its blade only while your own sword is out, and the blade flickers when two wearers
# disagree. The Look copies above give a puppet a resource of its own only while it holds a
# colour slot (4 slots, Hookshot with patch 0020). So every sword also gets one copy per player
# number, `<sword>_Blade<n>` (n = 1..BLADE_COPIES, the protocol's kMaxPlayers): the game's pack
# under the copy's name and its model files under the copy's project, unchanged (the game's own
# textures, so the copy draws exactly as the game's item, with no painting). A puppet without a
# ready Look copy wears its player number's blade copy (lichii/hook/coop_puppet.cpp
# DressInBlades), on every runtime.
BLADE_ITEMS = (
    "Weapon_Sword_Kokiri",
    "Weapon_Sword_Master",
    "Weapon_Sword_Biggoron",
    "Weapon_Sword_Giant",
    "Weapon_Sword_BrokenGiant",
)
BLADE_COPIES = 16


def blade_name(item, number):
    return f"{item}_Blade{number}"


def actor_name(item, slot):
    return f"{item}_Look{slot}"


def tex_name(name, slot):
    """The slot's name of a gear texture: `Mt_` -> `C<k>_` (as long)."""
    if not name.startswith(PREFIX) or not 1 <= slot <= 9:
        raise TunicError(f"{name}: no slot name for slot {slot}")
    return f"C{slot}_" + name[len(PREFIX):]


def model_for(model, slot):
    """A model copy naming the slot's gear textures; (bytes, the gear names it names)."""
    named = [g for g in GEAR if (g.encode() + b"\0") in model]
    for g in named:
        model = rename(model, g, tex_name(g, slot), at_least=1)
    return model, named


def texture_for(bntx, name, slot):
    """The game's texture under the slot's name, its texels marked with the slot's number
    (look_live.mark: the module tells your own Link's textures from the copies' by it)."""
    return mark_bntx(rename(bntx, name, tex_name(name, slot), at_least=1), slot)


def actor_param_path(actor):
    return f"Actor/{actor}.engine__actor__ActorParam.bgyml"


def model_info_path(actor):
    return f"Component/ModelInfo/{actor}.engine__component__ModelInfo.bgyml"


def _member(ref):
    if not isinstance(ref, str) or not ref.startswith("?"):
        return None
    return ref[1:]


def _merged(base, over):
    """`over` on top of `base`, as a `$parent` is: maps merge key by key, anything else is replaced."""
    out = dict(base)
    for k, v in over.items():
        out[k] = _merged(out[k], v) if isinstance(v, dict) and isinstance(out.get(k), dict) else v
    return out


def _resolved(members, item, path):
    """The ModelInfo at `path` with its `$parent`s (from the same pack) merged in, `$parent` dropped."""
    chain = []
    while path is not None:
        if path in chain:
            raise TunicError(f"{item}: its ModelInfo's $parent loops at {path}")
        if path not in members:
            raise TunicError(f"{item}: its ModelInfo {path} is not in the pack")
        chain.append(path)
        parent = byml.loads(members[path]).get("$parent")
        if parent is None:
            break
        if not isinstance(parent, str):
            raise TunicError(f"{item}: its ModelInfo has a $parent that is no path")
        # `Work/<path>.gyml` names the pack's `<path>.bgyml`.
        path = parent[len("Work/"):] if parent.startswith("Work/") else parent
        path = path[:-len(".gyml")] + ".bgyml" if path.endswith(".gyml") else path
    doc = {}
    for p in reversed(chain):
        doc = _merged(doc, byml.loads(members[p]))
    doc.pop("$parent", None)
    return doc


def model_info(members, item):
    """(FmdbName, ModelProjectName, member path) of the item's own model, through its `$parent`s."""
    ap = members.get(actor_param_path(item))
    if ap is None:
        raise TunicError(f"{item}: no {actor_param_path(item)} in its pack")
    comps = byml.loads(ap).get("Components", {})
    path = _member(comps.get("ModelInfoRef"))
    if path is None or path not in members:
        raise TunicError(f"{item}: its ActorParam names no ModelInfo in the pack")
    info = _resolved(members, item, path)
    fmdb, project = info.get("FmdbName"), info.get("ModelProjectName")
    if not isinstance(fmdb, str) or not isinstance(project, str):
        raise TunicError(f"{item}: its ModelInfo has no FmdbName and ModelProjectName")
    return fmdb, project, path


def copy_model_info(members, item, actor):
    """The copy's ModelInfo: the item's whole ModelInfo (its `$parent`s merged in), every model
    project the item's own (its main model and any SubModels) renamed to the copy's. Only the
    names changed: a ModelInfo of the names alone lost the rest (the Biggoron's colour
    ModelVariationAnims, the Giant's Knife's broken SubModel)."""
    _, project, path = model_info(members, item)
    info = _resolved(members, item, path)
    info["ModelProjectName"] = actor
    subs = info.get("SubModels")
    if isinstance(subs, list):
        info["SubModels"] = [dict(m, ModelProjectName=actor)
                             if isinstance(m, dict) and m.get("ModelProjectName") == project else m
                             for m in subs]
    return info


def copy_pack(members, item, slot):
    """(pack bytes, actor param bytes, model info bytes) of the slot's copy of `item`: the game's
    pack, its ActorParam under the copy's name naming a ModelInfo of the copy's own project."""
    return copy_pack_as(members, item, actor_name(item, slot))


def copy_pack_as(members, item, actor):
    """copy_pack for any copy name (a Look slot's or a blade copy's)."""
    doc = byml.loads(members[actor_param_path(item)])
    doc["Components"]["ModelInfoRef"] = "?" + model_info_path(actor)
    info = byml.dumps(copy_model_info(members, item, actor))
    ap = byml.dumps(doc)
    files = {k: v for k, v in members.items() if k != actor_param_path(item)}
    files[actor_param_path(actor)] = ap
    files[model_info_path(actor)] = info
    return sarc.write(files), ap, info


def info_row(row, item, slot):
    """The copy's ActorInfo row: the item's, under the copy's name and model project."""
    return info_row_as(row, actor_name(item, slot))


def info_row_as(row, actor):
    """info_row for any copy name."""
    out = dict(row)
    out["__RowId"] = actor
    if "ActorName" in out:
        out["ActorName"] = actor
    out["ModelProjectName"] = actor
    return out


# The boots in the player's own model (the Kokiri boots; the child's leather texture holds his belts
# too). Every Link draws one texture of a name, so the puppets' body models (puppet_pack.tunic
# model_for, one per tunic colour) name a copy of their own per colour: `Mt_...` -> `P<L>_...`
# (L: the colour's letter, tunic.COLORS; as long). The module paints each colour's copy in the boots
# colour of the player who wears that colour (hookshot/src/coop_look_boots.cpp, same names and
# marks). Your own Link's model copy (coop_self) keeps the game's names: the module paints those.
BODY_BOOTS = (
    "Mt_Boots_Player_Adult_Alb",
    "Mt_Leather_Player_Child_Alb",
)


def boots_name(name, color):
    """A colour's name of a body boots texture: `Mt_` -> `P<L>_` (as long)."""
    if not name.startswith(PREFIX) or color not in COLORS:
        raise TunicError(f"{name}: no boots name for colour {color}")
    return f"P{COLORS[color][0]}_" + name[len(PREFIX):]


def boots_mark(color):
    """The mark of a colour's boots copy (look_live.mark): its letter's code, never a copy's 1..8."""
    return ord(COLORS[color][0])


def body_model_for(model, color):
    """A body model naming the colour's boots copies instead of the game's boots (those it names)."""
    for name in BODY_BOOTS:
        model = rename(model, name, boots_name(name, color), at_least=0)
    return model


def boots_texture_for(bntx, name, color):
    """The game's boots texture under the colour's name, its texels marked with the colour's mark."""
    return mark_bntx(rename(bntx, name, boots_name(name, color), at_least=1), boots_mark(color))
