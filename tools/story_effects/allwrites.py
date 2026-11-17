# SPDX-License-Identifier: GPL-3.0-or-later
"""DRAFT: every SetGameDataBool write of every event flow in the romfs, as JSON {name: [[value, flow], ...]}."""
import sys, os, re, json, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fx
W = collections.defaultdict(set)
for f in os.listdir(os.path.join(fx.R, 'Event/EventFlow')):
    n = f.replace('.bfevfl.zs', '')
    try: fl = fx.flow(n)
    except Exception: continue
    ne = fx.struct.unpack_from('<H', fl.B, fx.Flow.__dict__ and 0)[0] if False else None
    fc = fl.u64(fl.u64(0x28)); ne = fl.u16(fc+0x16)
    for i in range(ne):
        e, t = fl.event(i)
        if t != 0: continue
        actor, acts = fl.actors[fl.u16(e+0xc)]; act = acts[fl.u16(e+0xe)]
        if 'SetGameDataBool' in act:
            pp = fl.u64(e+0x10); p = fl.cont(pp) if pp else {}
            if isinstance(p.get('GameDataName'), str): W[p['GameDataName']].add((p.get('Value'), n))
json.dump({k: sorted([str(v), f] for v, f in s) for k, s in W.items()}, open(sys.argv[1], 'w'))
print(len(W))
