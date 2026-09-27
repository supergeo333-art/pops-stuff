"""
Cycle-counted 8051 simulator with a small STC15W1xx peripheral model.

Just enough of the chip to verify the bit-bang UART driver:
  * Full MCS-51 instruction set (MOVX / undefined opcodes are treated as faults:
    the STC15W104 has no XRAM).
  * STC15 (STC-Y5 core, 1T) instruction cycle counts, taken from the STC15
    datasheet instruction table.
  * Timer0 mode 0 = 16-bit auto-reload (STC15 semantics: writing TL0/TH0 while
    TR0=0 loads counter+reload, while TR0=1 loads only the reload register).
  * Timer2 (always 16-bit auto-reload, same write semantics, run bit AUXR.T2R).
  * INT4 on P3.0: falling edge, hidden request flag.
  * Two-level interrupt priority (IP.PT0 for Timer0; Timer2 / INT4 are fixed low).
  * P3.0 is driven by an external waveform, P3.1 transitions are recorded.

Everything that is a modelling assumption rather than a datasheet fact is a
constructor parameter so the test bench can sweep it.
"""
import bisect
import re

# ---------------------------------------------------------------- SFR addresses
ACC, B, PSW, SP, DPL, DPH = 0xE0, 0xF0, 0xD0, 0x81, 0x82, 0x83
TCON, TMOD, TL0, TH0 = 0x88, 0x89, 0x8A, 0x8C
AUXR, INT_CLKO, IE, IE2, IP = 0x8E, 0x8F, 0xA8, 0xAF, 0xB8
T2H, T2L, P3 = 0xD6, 0xD7, 0xB0

# ---------------------------------------------------------------- cycle table
def _build_cycles():
    c = [0] * 256
    def s(ops, n):
        for o in ops:
            c[o] = n
    rn = lambda base: range(base + 8, base + 16)
    ri = lambda base: (base + 6, base + 7)
    s([0x00], 1)                                        # NOP
    for base in (0x20, 0x30, 0x90):                     # ADD / ADDC / SUBB
        s([base + 4], 2); s([base + 5], 2); s(ri(base), 2); s(rn(base), 1)
    s([0x04], 1); s([0x05], 3); s(ri(0x00), 3); s(rn(0x00), 2)   # INC
    s([0x14], 1); s([0x15], 3); s(ri(0x10), 3); s(rn(0x10), 2)   # DEC
    s([0xA3], 1); s([0xA4], 2); s([0x84], 6); s([0xD4], 3)       # INC DPTR, MUL, DIV, DA
    for base in (0x40, 0x50, 0x60):                     # ORL / ANL / XRL
        s([base + 2], 3); s([base + 3], 3); s([base + 4], 2); s([base + 5], 2)
        s(ri(base), 2); s(rn(base), 1)
    s([0xE4, 0xF4, 0x03, 0x13, 0x23, 0x33, 0xC4], 1)    # CLR/CPL A, rotates, SWAP
    s(rn(0xE0), 1); s([0xE5], 2); s(ri(0xE0), 2); s([0x74], 2)   # MOV A,src
    s(rn(0xF0), 1); s(rn(0xA0), 3); s(rn(0x70), 2)              # MOV Rn,..
    s([0xF5], 2); s(rn(0x80), 2); s([0x85], 3); s(ri(0x80), 3); s([0x75], 3)
    s(ri(0xF0), 2); s(ri(0xA0), 3); s(ri(0x70), 2)               # MOV @Ri,..
    s([0x90], 3); s([0x93], 5); s([0x83], 4)                     # DPTR, MOVC
    s([0xC0], 3); s([0xD0], 2)                                   # PUSH / POP
    s(rn(0xC0), 2); s([0xC5], 3); s(ri(0xC0), 3); s(ri(0xD0), 3) # XCH / XCHD
    s([0xC3, 0xD3, 0xB3], 1); s([0xC2, 0xD2, 0xB2], 3)           # CLR/SETB/CPL
    s([0x82, 0xB0, 0x72, 0xA0, 0xA2], 2); s([0x92], 3)           # ANL/ORL C, MOV C/bit
    s([0x40, 0x50], 3); s([0x20, 0x30], 5); s([0x10], 5)         # JC JNC JB JNB JBC
    for a in range(0x01, 0x100, 0x20):
        c[a] = 3                                                 # AJMP
        c[a + 0x10] = 4                                          # ACALL
    s([0x12, 0x22, 0x32, 0x02], 4); s([0x80], 3); s([0x73], 5)   # LCALL RET RETI LJMP SJMP JMP@
    s([0x60, 0x70], 4)                                           # JZ JNZ
    s([0xB5], 5); s([0xB4], 4); s(rn(0xB0), 4); s(ri(0xB0), 5)   # CJNE
    s(rn(0xD0), 4); s([0xD5], 5)                                 # DJNZ
    return c

CYCLES = _build_cycles()
LENGTHS = [1] * 256
for _o in (0x05, 0x15, 0x24, 0x25, 0x34, 0x35, 0x42, 0x44, 0x45, 0x52, 0x54, 0x55,
           0x62, 0x64, 0x65, 0x72, 0x74, 0x76, 0x77, 0x82, 0x86, 0x87, 0x92, 0x94,
           0x95, 0xA0, 0xA2, 0xA6, 0xA7, 0xB0, 0xB2, 0xC0, 0xC2, 0xC5, 0xD0, 0xD2,
           0xE5, 0xF5, 0x40, 0x50, 0x60, 0x70, 0x80) + tuple(range(0x78, 0x80)) \
        + tuple(range(0x88, 0x90)) + tuple(range(0xA8, 0xB0)) + tuple(range(0xD8, 0xE0)):
    LENGTHS[_o] = 2
for _a in range(0x01, 0x100, 0x20):
    LENGTHS[_a] = LENGTHS[_a + 0x10] = 2
for _o in (0x02, 0x10, 0x12, 0x20, 0x30, 0x43, 0x53, 0x63, 0x75, 0x85, 0x90,
           0xB4, 0xB5, 0xD5) + tuple(range(0xB6, 0xC0)):
    LENGTHS[_o] = 3

# Interrupt sources: name -> (vector, natural polling order)
VEC = {'T0': (0x000B, 1), 'T2': (0x0063, 12), 'INT4': (0x0083, 16)}


class Fault(Exception):
    pass


def load_ihx(path, size=4096):
    code = bytearray([0xFF] * size)
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        n, addr, typ = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        if typ == 0:
            code[addr:addr + n] = bytes.fromhex(line[9:9 + 2 * n])
    return code


def parse_listing(path):
    """Return (labels{name:addr}, set of instruction start addresses)."""
    labels, insns = {}, set()
    lab = re.compile(r'^\s+([0-9A-F]{6})\s+\d+\s+([A-Za-z_]\w*):')
    ins = re.compile(r'^\s+([0-9A-F]{6})[ r][0-9A-F]{2}.*\[\d+\]')
    for line in open(path):
        m = lab.match(line)
        if m:
            labels[m.group(2)] = int(m.group(1), 16)
        m = ins.match(line)
        if m:
            insns.add(int(m.group(1), 16))
    return labels, insns


class Line:
    """Piecewise-constant logic waveform: list of (time, level), sorted."""
    def __init__(self, level=1):
        self.t, self.v = [0], [level]

    def add(self, t, level):
        if level != self.v[-1]:
            assert t >= self.t[-1]
            self.t.append(t); self.v.append(level)

    def at(self, t):
        return self.v[bisect.bisect_right(self.t, t) - 1]

    def falling_edges(self, t0, t1):
        """Falling edges with t0 < t <= t1."""
        i = bisect.bisect_right(self.t, t0)
        out = []
        while i < len(self.t) and self.t[i] <= t1:
            if self.v[i] == 0 and self.v[i - 1] == 1:
                out.append(self.t[i])
            i += 1
        return out


class STC15:
    def __init__(self, code, insns, rx_line, irq_entry=6, int4_latch_when_disabled=False):
        self.code, self.insns, self.rx = code, insns, rx_line
        self.irq_entry = irq_entry
        self.int4_latch = int4_latch_when_disabled
        self.iram = bytearray(256)
        self.sfr = bytearray(256)
        self.sfr[SP] = 0x07
        self.sfr[P3] = 0xFF
        self.pc, self.t = 0, 0
        self.t0_cnt = self.t0_rld = 0
        self.t2_cnt = self.t2_rld = 0
        self.flag = {'T0': 0, 'T2': 0, 'INT4': 0}
        self.active = []               # stack of priority levels being serviced
        self.block_irq = False         # one instruction after RETI
        self.tx = Line(1)              # P3.1 output waveform
        self.probe = Line(1)           # P3.2 output waveform (optional debug probe)
        self.rx_reads = []             # (t, pc) of every P3.0 pin read
        self.isr_cycles = 0
        self.irq_log = []              # (t, name)
        self.max_sp = 0

    # ------------------------------------------------------------ memory access
    def pin_p3(self, t):
        return (self.sfr[P3] & 0xFE) | (self.sfr[P3] & self.rx.at(t) & 1)

    def rd(self, a, rmw=False):
        if a < 0x80:
            return self.iram[a]
        if a == P3 and not rmw:
            self.rx_reads.append((self.t, self.pc_insn))
            return self.pin_p3(self.t_io)
        if a == TCON:
            return (self.sfr[TCON] & ~0x20) | (self.flag['T0'] << 5)
        if a == PSW:
            p = bin(self.sfr[ACC]).count('1') & 1
            return (self.sfr[PSW] & 0xFE) | p
        if a == TL0:
            return self.t0_cnt & 0xFF
        if a == TH0:
            return self.t0_cnt >> 8
        if a == T2L:
            return self.t2_cnt & 0xFF
        if a == T2H:
            return self.t2_cnt >> 8
        return self.sfr[a]

    def wr(self, a, v):
        v &= 0xFF
        if a < 0x80:
            self.iram[a] = v
            return
        if a in (TL0, TH0):
            sh = 0 if a == TL0 else 8
            self.t0_rld = (self.t0_rld & ~(0xFF << sh)) | (v << sh)
            if not self.tr0():
                self.t0_cnt = (self.t0_cnt & ~(0xFF << sh)) | (v << sh)
        elif a in (T2L, T2H):
            sh = 0 if a == T2L else 8
            self.t2_rld = (self.t2_rld & ~(0xFF << sh)) | (v << sh)
            if not (self.sfr[AUXR] & 0x10):
                self.t2_cnt = (self.t2_cnt & ~(0xFF << sh)) | (v << sh)
        elif a == INT_CLKO and not (v & 0x40) and not self.int4_latch:
            self.flag['INT4'] = 0               # clearing EX4 clears hidden flag
        elif a == TCON:
            self.flag['T0'] = (v >> 5) & 1
        if a == P3:
            self.tx.add(self.t_end, (v >> 1) & 1)
            self.probe.add(self.t_end, (v >> 2) & 1)
        self.sfr[a] = v

    def ind(self, a):
        return self.iram[a]

    def bit_addr(self, b):
        return (0x20 + (b >> 3), b & 7) if b < 0x80 else (b & 0xF8, b & 7)

    def rbit(self, b, rmw=False):
        a, n = self.bit_addr(b)
        return (self.rd(a, rmw) >> n) & 1

    def wbit(self, b, val):
        a, n = self.bit_addr(b)
        cur = self.rd(a, rmw=True)
        self.wr(a, (cur | (1 << n)) if val else (cur & ~(1 << n)))

    def tr0(self):
        return (self.sfr[TCON] >> 4) & 1

    @property
    def C(self):
        return self.sfr[PSW] >> 7

    @C.setter
    def C(self, v):
        self.sfr[PSW] = (self.sfr[PSW] & 0x7F) | ((v & 1) << 7)

    def R(self, n):
        return ((self.sfr[PSW] >> 3) & 3) * 8 + n

    def push(self, v):
        self.sfr[SP] = (self.sfr[SP] + 1) & 0xFF
        if self.sfr[SP] > 0x7F:
            raise Fault('stack overflow SP=%02X' % self.sfr[SP])
        self.max_sp = max(self.max_sp, self.sfr[SP])
        self.iram[self.sfr[SP]] = v & 0xFF

    def pop(self):
        v = self.iram[self.sfr[SP]]
        self.sfr[SP] = (self.sfr[SP] - 1) & 0xFF
        return v

    # ------------------------------------------------------------ peripherals
    def advance(self, n):
        t0 = self.t
        if self.tr0():
            assert self.sfr[AUXR] & 0x80, 'only 1T timer0 modelled'
            assert (self.sfr[TMOD] & 0x0F) == 0, 'only T0 mode 0 modelled'
            left = 0x10000 - self.t0_cnt
            if n >= left:
                self.flag['T0'] = 1
                self.t0_cnt = self.t0_rld + (n - left)
                assert self.t0_cnt < 0x10000
            else:
                self.t0_cnt += n
        if self.sfr[AUXR] & 0x10:
            assert self.sfr[AUXR] & 0x04, 'only 1T timer2 modelled'
            left = 0x10000 - self.t2_cnt
            if n >= left:
                self.flag['T2'] = 1
                self.t2_cnt = self.t2_rld + (n - left)
            else:
                self.t2_cnt += n
        if self.rx.falling_edges(t0, t0 + n):
            if self.int4_latch or (self.sfr[INT_CLKO] & 0x40):
                self.flag['INT4'] = 1
        self.t += n
        if self.active:
            self.isr_cycles += n

    def pending(self):
        if not (self.sfr[IE] & 0x80):
            return None
        en = {'T0': self.sfr[IE] & 0x02, 'T2': self.sfr[IE2] & 0x04,
              'INT4': self.sfr[INT_CLKO] & 0x40}
        prio = {'T0': (self.sfr[IP] >> 1) & 1, 'T2': 0, 'INT4': 0}
        cur = self.active[-1] if self.active else -1
        best = None
        for k in ('T0', 'T2', 'INT4'):
            if self.flag[k] and en[k] and prio[k] > cur:
                key = (-prio[k], VEC[k][1])
                if best is None or key < best[0]:
                    best = (key, k)
        return best and (best[1], prio[best[1]])

    # ------------------------------------------------------------ execution
    def step(self):
        if not self.block_irq:
            p = self.pending()
            if p:
                name, lvl = p
                self.flag[name] = 0
                self.push(self.pc & 0xFF); self.push(self.pc >> 8)
                self.active.append(lvl)
                self.irq_log.append((self.t, name))
                self.pc = VEC[name][0]
                self.advance(self.irq_entry)
        self.block_irq = False
        pc = self.pc
        if pc not in self.insns:
            raise Fault('executing non-instruction address %04X' % pc)
        op = self.code[pc]
        n = CYCLES[op]
        self.pc_insn = pc
        self.t_io = self.t + n - 1       # port pins sampled late in the instruction
        self.t_end = self.t + n
        self.exec(op)
        self.advance(n)

    def run_until(self, t_end):
        while self.t < t_end:
            self.step()

    def exec(self, op):
        c, pc = self.code, self.pc
        b1 = c[(pc + 1) & 0xFFFF]
        b2 = c[(pc + 2) & 0xFFFF]
        nxt = pc + LENGTHS[op]
        rel = lambda x: nxt + (x - 256 if x > 127 else x)
        A = self.sfr[ACC]
        lo = op & 0x0F
        hi = op & 0xF0
        self.pc = nxt

        def src(o):             # operand for arithmetic/logic group by low nibble
            if o == 4: return b1
            if o == 5: return self.rd(b1)
            if o in (6, 7): return self.ind(self.iram[self.R(o - 6)])
            return self.iram[self.R(o - 8)]

        if op == 0x00:
            return
        if lo == 1:                                     # AJMP / ACALL
            tgt = (nxt & 0xF800) | ((op >> 5) << 8) | b1
            if op & 0x10:
                self.push(nxt & 0xFF); self.push(nxt >> 8)
            self.pc = tgt
            return
        if op == 0x02: self.pc = (b1 << 8) | b2; return
        if op == 0x12:
            self.push(nxt & 0xFF); self.push(nxt >> 8); self.pc = (b1 << 8) | b2; return
        if op in (0x22, 0x32):
            h = self.pop(); l = self.pop(); self.pc = (h << 8) | l
            if op == 0x32:
                if not self.active:
                    raise Fault('RETI outside ISR')
                self.active.pop()
                self.block_irq = True
            return
        if op == 0x03: self.sfr[ACC] = ((A >> 1) | (A << 7)) & 0xFF; return
        if op == 0x13:
            cy = self.C; self.C = A & 1; self.sfr[ACC] = (A >> 1) | (cy << 7); return
        if op == 0x23: self.sfr[ACC] = ((A << 1) | (A >> 7)) & 0xFF; return
        if op == 0x33:
            cy = self.C; self.C = A >> 7; self.sfr[ACC] = ((A << 1) | cy) & 0xFF; return
        if hi in (0x00, 0x10) and lo >= 4:              # INC / DEC
            d = 1 if hi == 0 else -1
            if lo == 4: self.sfr[ACC] = (A + d) & 0xFF
            elif lo == 5: self.wr(b1, self.rd(b1, True) + d)
            elif lo in (6, 7):
                a = self.iram[self.R(lo - 6)]; self.iram[a] = (self.iram[a] + d) & 0xFF
            else:
                r = self.R(lo - 8); self.iram[r] = (self.iram[r] + d) & 0xFF
            return
        if op == 0x10:                                  # JBC
            if self.rbit(b1, True):
                self.wbit(b1, 0); self.pc = rel(b2)
            else:
                self.pc = pc + 3
            return
        if op in (0x20, 0x30):
            v = self.rbit(b1)
            if (v == 1) == (op == 0x20): self.pc = (pc + 3) + (b2 - 256 if b2 > 127 else b2)
            else: self.pc = pc + 3
            return
        if op == 0x40:
            if self.C: self.pc = rel(b1)
            return
        if op == 0x50:
            if not self.C: self.pc = rel(b1)
            return
        if op == 0x60:
            if A == 0: self.pc = rel(b1)
            return
        if op == 0x70:
            if A != 0: self.pc = rel(b1)
            return
        if op == 0x80: self.pc = rel(b1); return
        if hi in (0x20, 0x30, 0x90) and lo >= 4:        # ADD ADDC SUBB
            s = src(lo)
            cy = self.C if hi != 0x20 else 0
            if hi == 0x90:
                r = A - s - cy
                self.C = 1 if r < 0 else 0
                ac = 1 if (A & 0xF) - (s & 0xF) - cy < 0 else 0
                ov = 1 if ((A ^ s) & (A ^ r) & 0x80) else 0
            else:
                r = A + s + cy
                self.C = 1 if r > 0xFF else 0
                ac = 1 if (A & 0xF) + (s & 0xF) + cy > 0xF else 0
                ov = 1 if (~(A ^ s) & (A ^ r) & 0x80) else 0
            self.sfr[PSW] = (self.sfr[PSW] & ~0x44) | (ac << 6) | (ov << 2)
            self.sfr[ACC] = r & 0xFF
            return
        if hi in (0x40, 0x50, 0x60) and lo >= 2:        # ORL ANL XRL
            f = {0x40: lambda x, y: x | y, 0x50: lambda x, y: x & y,
                 0x60: lambda x, y: x ^ y}[hi]
            if lo == 2: self.wr(b1, f(self.rd(b1, True), A)); return
            if lo == 3: self.wr(b1, f(self.rd(b1, True), b2)); return
            self.sfr[ACC] = f(A, src(lo)); return
        if op in (0x72, 0x82, 0xA0, 0xB0):
            v = self.rbit(b1)
            if op in (0xA0, 0xB0): v ^= 1
            self.C = (self.C | v) if op in (0x72, 0xA0) else (self.C & v)
            return
        if op == 0x73: self.pc = (A + ((self.sfr[DPH] << 8) | self.sfr[DPL])) & 0xFFFF; return
        if op == 0x74: self.sfr[ACC] = b1; return
        if op == 0x75: self.wr(b1, b2); return
        if op in (0x76, 0x77): self.iram[self.iram[self.R(op - 0x76)]] = b1; return
        if 0x78 <= op <= 0x7F: self.iram[self.R(op - 0x78)] = b1; return
        if op == 0x83: self.sfr[ACC] = c[(A + nxt) & 0xFFFF]; return
        if op == 0x84:
            bb = self.sfr[B]
            if bb == 0:
                self.sfr[PSW] |= 0x04
            else:
                self.sfr[ACC], self.sfr[B] = A // bb, A % bb; self.sfr[PSW] &= ~0x04
            self.C = 0; return
        if op == 0x85: self.wr(b2, self.rd(b1)); return
        if op in (0x86, 0x87): self.wr(b1, self.iram[self.iram[self.R(op - 0x86)]]); return
        if 0x88 <= op <= 0x8F: self.wr(b1, self.iram[self.R(op - 0x88)]); return
        if op == 0x90: self.sfr[DPH], self.sfr[DPL] = b1, b2; return
        if op == 0x92: self.wbit(b1, self.C); return
        if op == 0x93:
            self.sfr[ACC] = c[(A + ((self.sfr[DPH] << 8) | self.sfr[DPL])) & 0xFFFF]; return
        if op == 0xA2: self.C = self.rbit(b1); return
        if op == 0xA3:
            d = (((self.sfr[DPH] << 8) | self.sfr[DPL]) + 1) & 0xFFFF
            self.sfr[DPH], self.sfr[DPL] = d >> 8, d & 0xFF; return
        if op == 0xA4:
            r = A * self.sfr[B]
            self.sfr[ACC], self.sfr[B] = r & 0xFF, r >> 8
            self.C = 0; self.sfr[PSW] = (self.sfr[PSW] & ~4) | (4 if r > 255 else 0); return
        if op in (0xA6, 0xA7): self.iram[self.iram[self.R(op - 0xA6)]] = self.rd(b1); return
        if 0xA8 <= op <= 0xAF: self.iram[self.R(op - 0xA8)] = self.rd(b1); return
        if op == 0xB2: self.wbit(b1, self.rbit(b1, True) ^ 1); return
        if op == 0xB3: self.C ^= 1; return
        if hi == 0xB0 and lo >= 4:                      # CJNE
            if lo == 4: x, y = A, b1
            elif lo == 5: x, y = A, self.rd(b1)
            elif lo in (6, 7): x, y = self.iram[self.iram[self.R(lo - 6)]], b1
            else: x, y = self.iram[self.R(lo - 8)], b1
            self.C = 1 if x < y else 0
            if x != y: self.pc = (pc + 3) + (b2 - 256 if b2 > 127 else b2)
            return
        if op == 0xC0: self.push(self.rd(b1, True)); return
        if op == 0xC2: self.wbit(b1, 0); return
        if op == 0xC3: self.C = 0; return
        if op == 0xC4: self.sfr[ACC] = ((A << 4) | (A >> 4)) & 0xFF; return
        if op == 0xC5: v = self.rd(b1); self.wr(b1, A); self.sfr[ACC] = v; return
        if op in (0xC6, 0xC7):
            a = self.iram[self.R(op - 0xC6)]; self.iram[a], self.sfr[ACC] = A, self.iram[a]; return
        if 0xC8 <= op <= 0xCF:
            r = self.R(op - 0xC8); self.iram[r], self.sfr[ACC] = A, self.iram[r]; return
        if op == 0xD0: self.wr(b1, self.pop()); return
        if op == 0xD2: self.wbit(b1, 1); return
        if op == 0xD3: self.C = 1; return
        if op == 0xD5:
            v = (self.rd(b1, True) - 1) & 0xFF; self.wr(b1, v)
            if v: self.pc = (pc + 3) + (b2 - 256 if b2 > 127 else b2)
            return
        if 0xD8 <= op <= 0xDF:
            r = self.R(op - 0xD8); self.iram[r] = (self.iram[r] - 1) & 0xFF
            if self.iram[r]: self.pc = rel(b1)
            return
        if op == 0xE4: self.sfr[ACC] = 0; return
        if op == 0xE5: self.sfr[ACC] = self.rd(b1); return
        if op in (0xE6, 0xE7): self.sfr[ACC] = self.iram[self.iram[self.R(op - 0xE6)]]; return
        if 0xE8 <= op <= 0xEF: self.sfr[ACC] = self.iram[self.R(op - 0xE8)]; return
        if op == 0xF4: self.sfr[ACC] = A ^ 0xFF; return
        if op == 0xF5: self.wr(b1, A); return
        if op in (0xF6, 0xF7): self.iram[self.iram[self.R(op - 0xF6)]] = A; return
        if 0xF8 <= op <= 0xFF: self.iram[self.R(op - 0xF8)] = A; return
        raise Fault('unsupported/illegal opcode %02X at %04X (MOVX/DA/XCHD?)' % (op, pc))
