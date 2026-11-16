"""BFEVFL (version 4) reader: flowchart actors, events, entry points, and a walk of one entry (G2, boss-0114-a: the bosses' beaten sets come from their death settings' entries)."""
import struct


class Flow:
    def __init__(self, raw):
        self.raw = raw
        assert raw[:8] == b'BFEVFL\x00\x00', raw[:8]
        fl = self.u64(0x28) if self.u16(0x20) else None  # flowchart offsets array
        fl = self.u64(fl) if fl else None
        self.name = None
        self.actors, self.events, self.entries = [], [], {}
        if fl is None:
            return
        assert raw[fl:fl + 4] == b'EVFL'
        na, nact, nq, ne, nep = [self.u16(fl + 0x10 + 2 * i) for i in range(5)]
        self.name = self.s(self.u64(fl + 0x20))
        actors, events, epdict, eps = (self.u64(fl + 0x28), self.u64(fl + 0x30), self.u64(fl + 0x38),
                                       self.u64(fl + 0x40))
        for i in range(na):
            o = actors + i * 0x38
            acts = [self.s(self.u64(self.u64(o + 0x18) + 8 * k)) for k in range(self.u16(o + 0x30))]
            qs = [self.s(self.u64(self.u64(o + 0x20) + 8 * k)) for k in range(self.u16(o + 0x32))]
            self.actors.append({'name': self.s(self.u64(o)), 'sub': self.s(self.u64(o + 8)), 'actions': acts,
                                'queries': qs})
        for i in range(ne):
            self.events.append(self._event(events + i * 0x30))
        n = self.u32(epdict + 4)
        for i in range(n):
            name = self.s(self.u64(epdict + 8 + 0x10 * (i + 1) + 8))
            self.entries[name] = self.u16(eps + i * 0x20 + 0x1c)

    def u16(self, o): return struct.unpack_from('<H', self.raw, o)[0]
    def u32(self, o): return struct.unpack_from('<I', self.raw, o)[0]
    def i32(self, o): return struct.unpack_from('<i', self.raw, o)[0]
    def f32(self, o): return struct.unpack_from('<f', self.raw, o)[0]
    def u64(self, o): return struct.unpack_from('<Q', self.raw, o)[0]

    def s(self, p):
        if not p or p >= len(self.raw):
            return None
        n = self.u16(p)
        return self.raw[p + 2:p + 2 + n].decode('utf-8', 'replace')

    def container(self, p):
        """A parameter container: {name: value}."""
        if not p:
            return {}
        t, n, d = self.raw[p], self.u16(p + 2), self.u64(p + 8)
        if t != 1:
            return self.item(p)
        names = [self.s(self.u64(d + 8 + 0x10 * (i + 1) + 8)) for i in range(n)]
        return {names[i]: self.item(self.u64(p + 0x10 + 8 * i)) for i in range(n)}

    def item(self, p):
        t, n = self.raw[p], self.u16(p + 2)
        v = p + 0x10
        if t == 1:
            return self.container(p)
        if t == 2: return self.i32(v)
        if t == 3: return bool(self.u32(v))
        if t == 4: return self.f32(v)
        if t in (5, 6): return self.s(self.u64(v))
        if t == 7: return [self.i32(v + 4 * k) for k in range(n)]
        if t == 8: return [bool(self.u32(v + 4 * k)) for k in range(n)]
        if t == 9: return [self.f32(v + 4 * k) for k in range(n)]
        if t in (10, 11): return [self.s(self.u64(v + 8 * k)) for k in range(n)]
        if t == 0xd: return ('actor', self.s(self.u64(v)), self.s(self.u64(v + 8)))
        return ('type', t)

    def _event(self, o):
        t = self.raw[o + 8]
        e = {'name': self.s(self.u64(o)), 'type': t}
        if t == 0:  # action
            e.update(next=self.u16(o + 0xa), actor=self.u16(o + 0xc), action=self.u16(o + 0xe),
                     params=self.container(self.u64(o + 0x10)))
        elif t == 1:  # switch
            nc, cases = self.u16(o + 0xa), self.u64(o + 0x18)
            e.update(actor=self.u16(o + 0xc), query=self.u16(o + 0xe), params=self.container(self.u64(o + 0x10)),
                     cases=[(self.u32(cases + 0x10 * k), self.u16(cases + 0x10 * k + 8)) for k in range(nc)])
        elif t == 2:  # fork
            nf, join, forks = self.u16(o + 0xa), self.u16(o + 0xc), self.u64(o + 0x10)
            e.update(join=join, forks=[self.u16(forks + 2 * k) for k in range(nf)])
        elif t == 3:  # join
            e.update(next=self.u16(o + 0xa))
        elif t == 4:  # sub flow
            e.update(next=self.u16(o + 0xa), params=self.container(self.u64(o + 0x10)),
                     flow=self.s(self.u64(o + 0x18)), entry=self.s(self.u64(o + 0x20)))
        return e

    def walk(self, entry):
        """Every event reachable from `entry`, in a depth-first order (each once)."""
        out, todo, seen = [], [self.entries[entry]], set()
        while todo:
            i = todo.pop()
            if i == 0xffff or i in seen or i >= len(self.events):
                continue
            seen.add(i)
            e = self.events[i]
            out.append((i, e))
            if e['type'] in (0, 3, 4):
                todo.append(e['next'])
            elif e['type'] == 1:
                todo.extend(c[1] for c in reversed(e['cases']))
            elif e['type'] == 2:
                todo.append(e['join'])
                todo.extend(reversed(e['forks']))
        return out

    def describe(self, e):
        if e['type'] == 0:
            a = self.actors[e['actor']]
            return f"{a['name']}.{a['actions'][e['action']]} {e['params']}"
        if e['type'] == 1:
            a = self.actors[e['actor']]
            return f"switch {a['name']}.{a['queries'][e['query']]} {e['params']} cases {e['cases']}"
        if e['type'] == 4:
            return f"subflow {e['flow']}:{e['entry']} {e['params']}"
        return ('fork', 'join')[e['type'] - 2] if e['type'] in (2, 3) else str(e)
