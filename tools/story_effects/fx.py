# SPDX-License-Identifier: GPL-3.0-or-later
"""DRAFT (owl-time, row 129): what a shared story setting's flow sets in the game that played it.
Reads the romfs event flows (BFEVFL) and RSDB EventSetting. Usage: python -I fx.py [setting ...]
(no args: every shared HavePlayedEvent.* of lichii/hook/coop_story.cpp). To become the generator of
lichii/hook/coop_story_effects.inc (see reports/owl-time-a.md in the coordination folder)."""
import sys, re, os, json, struct
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from puppet_pack import zs, byml
R = os.environ["OOT_ROMFS"]  # the extracted romfs folder
DICTS = zs.Dicts.from_romfs(R)
def load(p): return zs.maybe_decompress(DICTS, open(os.path.join(R, p), 'rb').read())
class Flow:
    def __init__(self, name):
        B = self.B = load('Event/EventFlow/' + name + '.bfevfl.zs')
        fc = self.u64(self.u64(0x28)) if self.u16(0x20) else 0
        if B[fc:fc+4] != b'EVFL': raise ValueError('timeline' if b'SetGameData' not in B else 'timeline with SetGameData')
        na = self.u16(fc+0x10)
        ap, self.ep_ev, epd, self.eps = self.u64(fc+0x28), self.u64(fc+0x30), self.u64(fc+0x38), self.u64(fc+0x40)
        self.actors = []
        for i in range(na):
            a = ap + 0x38*i
            self.actors.append((self.s(self.u64(a)), [self.s(self.u64(self.u64(a+0x18)+8*k)) for k in range(self.u16(a+0x30))]))
        self.epn = self.dic(epd)
    def u16(self, o): return struct.unpack_from('<H', self.B, o)[0]
    def u32(self, o): return struct.unpack_from('<I', self.B, o)[0]
    def u64(self, o): return struct.unpack_from('<Q', self.B, o)[0]
    def s(self, p): return self.B[p+2:p+2+self.u16(p)].decode('utf-8', 'replace') if p else None
    def dic(self, p): return [self.s(self.u64(p+8+16*(i+1)+8)) for i in range(self.u32(p+4))]
    def cont(self, p):
        B = self.B; t, n = B[p], self.u16(p+2); v = p + 0x10
        if t == 1:
            names = self.dic(self.u64(p+8)) if self.u64(p+8) else []
            return {(names[i] if i < len(names) else i): self.cont(self.u64(v+8*i)) for i in range(n)}
        if t == 2: return struct.unpack_from('<i', B, v)[0]
        if t == 3: return bool(self.u32(v))
        if t == 4: return struct.unpack_from('<f', B, v)[0]
        if t in (5, 6): return self.s(self.u64(v))
        return 't%d' % t
    def entry(self, ep):
        k = self.epn.index(ep) if ep in self.epn else 0
        return self.u16(self.eps + 0x20*k + 0x1c)
    def event(self, i):
        e = self.ep_ev + 0x30*i; return e, self.B[e+8]
FLOWS = {}
def flow(n):
    if n not in FLOWS: FLOWS[n] = Flow(n)
    return FLOWS[n]
ROWS = {r['__RowId']: r for r in byml.loads(load('RSDB/EventSetting.Product.111.rstbl.byml.zs'))}
def run(setting, out, seen, cond=0):
    """Every GameData set, pouch and challenge action an event setting's flow does, following the
    settings it plays (EventTransition) and its subflows. cond>0: behind a switch."""
    if setting in seen or setting not in ROWS: return
    seen.add(setting)
    r = ROWS[setting]; fl = (r.get('FlowchartFileName') or setting).split('/')[-1].replace('.evfc', '')
    if not os.path.exists(os.path.join(R, 'Event/EventFlow', fl + '.bfevfl.zs')): return
    try: f = flow(fl)
    except ValueError as ex:
        if 'SetGameData' in str(ex): out.append(dict(act='TIMELINE-SETS', name=fl, value=None, setting=setting, cond=cond))
        return
    walk(f, f.entry(r.get('EntryPointName') or fl), set(), out, seen, setting, cond)
def walk(f, i, vis, out, seen, setting, cond):
    while i != 0xffff and i not in vis:
        vis.add(i); e, t = f.event(i)
        if t == 0:
            nx, ai, aa = f.u16(e+0xa), f.u16(e+0xc), f.u16(e+0xe); pp = f.u64(e+0x10)
            actor, acts = f.actors[ai]; act = acts[aa]; p = f.cont(pp) if pp else {}
            if 'SetGameData' in act: out.append(dict(act=act, name=p.get('GameDataName'), value=p.get('Value'), index=p.get('Index'), setting=setting, cond=cond))
            elif act == 'EventTransition' and p.get('EventName'): run(p['EventName'], out, seen, cond)
            elif actor in ('PouchSystemActor', 'ChallengeSystemActor'): out.append(dict(act=act, name=json.dumps(p), value=None, setting=setting, cond=cond))
            i = nx
        elif t == 1:
            nc, cp = f.u16(e+0xa), f.u64(e+0x18)
            for k in range(nc): walk(f, f.u16(cp+8*k+4), vis, out, seen, setting, cond+1)
            return
        elif t == 2:
            nf, jn, fp = f.u16(e+0xa), f.u16(e+0xc), f.u64(e+0x10)
            for k in range(nf): walk(f, f.u16(fp+2*k), vis, out, seen, setting, cond)
            i = jn
        elif t == 3: i = f.u16(e+0xa)
        elif t == 4:
            nx = f.u16(e+0xa); fl, en = f.s(f.u64(e+0x18)), f.s(f.u64(e+0x20))
            try:
                g = flow(fl) if fl else f
                walk(g, g.entry(en), set() if g is not f else vis, out, seen, setting, cond)
            except Exception as ex: out.append(dict(act='SUBERR', name=f'{fl}/{en} {ex}', value=None, setting=setting, cond=cond))
            i = nx
        else: return
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "lichii", "hook", "coop_story.cpp")
SHARED = {n: k for h, k, n in re.findall(r'\{0x([0-9a-f]{8})u, Kind::(\w+), "([^"]+)"\}', open(SRC).read())}
if __name__ == '__main__':
    names = sys.argv[1:] or [n.split('.', 1)[1] for n in SHARED if n.startswith('HavePlayedEvent.')]
    for n in names:
        out = []; run(n, out, set())
        if not out: continue
        print('==', n)
        for o in out:
            tag = 'SHARED' if o['name'] in SHARED else ''
            print('   ', o['act'].replace('EventTrigger', ''), o['name'], o['value'], 'idx=%s' % o.get('index') if o.get('index') not in (None, -1) else '', 'COND%d' % o['cond'] if o['cond'] else '', tag, '' if o['setting'] == n else '(in %s)' % o['setting'])
