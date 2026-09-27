#!/usr/bin/env python3
"""
Test bench for the bit-bang UART echo firmware.

    python3 tb.py ../uart_drv_orig.asm      # original driver
    python3 tb.py ../uart_drv.asm           # optimized driver

Assembles + links the source with sdas8051/sdld, runs it on the STC15 model in
sim8051.py and drives P3.0 (RX) with generated frames. The firmware echoes every
byte it receives; the P3.1 (TX) waveform is decoded with an ideal UART receiver
and compared with what was sent.
"""
import os
import random
import subprocess
import sys

from sim8051 import STC15, Fault, Line, load_ihx, parse_listing

FOSC = 22118400          # the simulated oscillator
BAUD = 19200
T_NOM = FOSC / BAUD      # 1152 cycles per bit


def build(asm):
    d, base = os.path.dirname(os.path.abspath(asm)), os.path.splitext(os.path.basename(asm))[0]
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build')
    os.makedirs(out, exist_ok=True)
    rel = os.path.join(out, base + '.rel')
    subprocess.run(['sdas8051', '-plosgff', '-I' + d, rel, os.path.abspath(asm)], check=True, cwd=d)
    subprocess.run(['sdld', '-n', '-i', os.path.join(out, base + '.ihx'), rel], check=True)
    return (load_ihx(os.path.join(out, base + '.ihx')),) + parse_listing(os.path.join(out, base + '.lst'))


class Host:
    """Builds the waveform the PC/host drives onto the MCU RX pin."""
    def __init__(self, err=0.0, t0=4000):
        self.T = T_NOM * (1 + err)
        self.line = Line(1)
        self.t = t0
        self.frames = []           # (start_time, byte, stop_level)

    def byte(self, b, stop=1, gap_bits=0.0):
        s = self.t
        self.frames.append((s, b, stop))
        bits = [0] + [(b >> i) & 1 for i in range(8)] + [stop]
        for i, v in enumerate(bits):
            self.line.add(s + i * self.T, v)
        self.line.add(s + 10 * self.T, 1)
        self.t = s + (10 + gap_bits) * self.T

    def idle(self, bits):
        self.t += bits * self.T

    def low(self, bits):           # BREAK or glitch
        self.line.add(self.t, 0)
        self.t += bits * self.T
        self.line.add(self.t, 1)


def decode(line, T, t_from=0):
    """Ideal receiver: returns list of (start_time, byte, stop_ok)."""
    out, i = [], 1
    t = t_from
    while True:
        # next falling edge after t
        edges = line.falling_edges(t, float('inf'))
        if not edges:
            return out
        s = edges[0]
        if line.at(s + 0.5 * T) != 0:          # glitch, not a start bit
            t = s + 1
            continue
        b = sum(line.at(s + (k + 1.5) * T) << k for k in range(8))
        stop = line.at(s + 9.5 * T)
        out.append((s, b, stop))
        t = s + 9.5 * T
        while line.at(t) == 0 and t < line.t[-1]:
            t = line.t[bisect_right(line.t, t)]


from bisect import bisect_right


def edge_errors(line, frames, T):
    """Worst deviation (cycles) of TX transitions from ideal bit boundaries."""
    worst, start_w = 0.0, []
    for s, _, _ in frames:
        i = bisect_right(line.t, s)
        j = i
        while j < len(line.t) and line.t[j] <= s + 9.9 * T:
            e = line.t[j] - s
            k = round(e / T)
            worst = max(worst, abs(e - k * T))
            j += 1
    return worst


class Run:
    def __init__(self, prog, host, t_end=None, **kw):
        self.code, self.labels, self.insns = prog
        self.host = host
        self.cpu = STC15(self.code, self.insns, host.line, **kw)
        self.fault = None
        t_end = t_end or host.t + 40 * T_NOM
        try:
            self.cpu.run_until(t_end)
        except Fault as e:
            self.fault = '%s (t=%d, %.1f bit-times)' % (e, self.cpu.t, self.cpu.t / T_NOM)
        self.rx_out = decode(self.cpu.tx, host.T)

    def echoed(self):
        return [b for _, b, stop in self.rx_out]

    def sample_positions(self):
        """Where inside each host bit the firmware read P3.0 while in the RX timer ISR."""
        pos = {}
        frames = self.host.frames
        starts = [f[0] for f in frames]
        for t, pc in self.cpu.rx_reads:
            i = bisect_right(starts, t) - 1
            if i < 0:
                continue
            s = starts[i]
            x = (t - s) / self.host.T
            k = int(x)
            if 0 <= k <= 9:
                pos.setdefault(k, []).append(x - k)
        return pos


def rnd_bytes(n, seed):
    r = random.Random(seed)
    return [r.randrange(256) for _ in range(n)]


def check(name, ok, detail=''):
    print('  [%s] %-58s %s' % ('PASS' if ok else 'FAIL', name, detail))
    return ok


def main(asm):
    prog = build(asm)
    print('=' * 78)
    print('Firmware: %s' % os.path.basename(asm))
    print('=' * 78)
    results = []

    # 1 ------------------------------------------------------------ gapped echo
    h = Host()
    data = rnd_bytes(40, 1) + [0x00, 0xFF, 0x55, 0xAA]
    for b in data:
        h.byte(b, gap_bits=2)
    r = Run(prog, h)
    results.append(check('echo, 44 bytes with 2-bit idle gaps', not r.fault and r.echoed() == data,
                         r.fault or '%d/%d echoed' % (sum(a == b for a, b in zip(r.echoed(), data)), len(data))))
    ew = edge_errors(r.cpu.tx, r.rx_out, T_NOM)   # MCU clock, not host
    results.append(check('TX bit edges within 2% of ideal (idle gaps)', ew < 0.02 * T_NOM,
                         'worst %.0f cycles (%.1f%%)' % (ew, 100 * ew / T_NOM)))
    pos = r.sample_positions()
    data_pos = [p for k in range(1, 10) for p in pos.get(k, [])]
    if data_pos:
        lo, hi = min(data_pos), max(data_pos)
        results.append(check('RX data/stop samples centered (0.40..0.60 bit)', 0.40 <= lo and hi <= 0.60,
                             'range %.3f..%.3f bit' % (lo, hi)))
    if 0 in pos:
        print('       start-bit validation read at %.3f..%.3f bit' % (min(pos[0]), max(pos[0])))
    load = r.cpu.isr_cycles / r.cpu.t
    print('       ISR CPU load while echoing: %.1f%%   max SP: 0x%02X' % (100 * load, r.cpu.max_sp))

    # 2 ------------------------------------------------- back-to-back, baud error
    for err in (0.0, -0.02, +0.02):
        h = Host(err=err)
        data = rnd_bytes(64, 2)
        for b in data:
            h.byte(b)
        r = Run(prog, h, t_end=h.t + 30 * T_NOM * 12)
        got = r.echoed()
        results.append(check('echo, 64 bytes back-to-back, host baud %+.0f%%' % (err * 100),
                             not r.fault and got == data,
                             r.fault or '%d/%d match, %d frames out' % (
                                 sum(a == b for a, b in zip(got, data)), len(data), len(got))))
        if not r.fault and got == data:
            # Full duplex: TX edges can be pushed back by an RX ISR (Timer2 is fixed low
            # priority on STC15) and INT4 can be held off by the TX ISR. Budget: 10% of a
            # bit for either, leaving >35% sampling margin after +-2% clock error.
            ew = edge_errors(r.cpu.tx, r.rx_out, T_NOM)   # MCU clock, not host
            pos = r.sample_positions()
            dp = [p for k in range(1, 10) for p in pos.get(k, [])]
            results.append(check('full duplex %+.0f%%: TX edge jitter / RX sample < 10%% bit' % (err * 100),
                                 ew < 0.10 * T_NOM and dp and max(abs(p - 0.5 - 0 * err) for p in dp) < 0.10 + 9.5 * abs(err),
                                 'TX worst %.0f cyc (%.1f%%), RX samples %.3f..%.3f' % (
                                     ew, 100 * ew / T_NOM, min(dp), max(dp))))

    # 3 --------------------------------------------------------- baud tolerance
    for sgn in (-1, 1):
        best = 0
        for e10 in range(5, 80, 5):   # 0.5% .. 7.5%
            h = Host(err=sgn * e10 / 1000)
            data = [0x00, 0xFF, 0x55, 0xAA, 0x0F, 0xF0, 0x01, 0x80]
            for b in data:
                h.byte(b, gap_bits=3)
            r = Run(prog, h)
            if r.fault or r.echoed() != data:
                break
            best = e10
        print('       RX tolerates host baud %s%.1f%%' % ('-' if sgn < 0 else '+', best / 10))

    # 4 --------------------------------------------------------- noise glitch
    h = Host()
    h.idle(2); h.low(0.15); h.idle(3)          # 15% bit-time glitch on idle line
    data = [0x41, 0x42, 0x43]
    for b in data:
        h.byte(b, gap_bits=2)
    r = Run(prog, h)
    results.append(check('0.15-bit glitch on idle line is rejected', not r.fault and r.echoed() == data,
                         r.fault or 'echoed %s' % [hex(x) for x in r.echoed()]))

    # 5 --------------------------------------------------------- framing error
    h = Host()
    h.byte(0x5A, stop=0); h.idle(3)
    data = [0x31, 0x32, 0x33]
    for b in data:
        h.byte(b, gap_bits=2)
    r = Run(prog, h)
    got = r.echoed()
    results.append(check('framing error (stop=0) survives, next bytes OK',
                         not r.fault and got[-3:] == data,
                         r.fault or 'echoed %s' % [hex(x) for x in got]))

    # 6 --------------------------------------------------------- BREAK
    h = Host()
    h.low(25); h.idle(3)
    for b in data:
        h.byte(b, gap_bits=2)
    r = Run(prog, h)
    got = r.echoed()
    results.append(check('25-bit BREAK survives, next bytes OK', not r.fault and got[-3:] == data,
                         r.fault or 'echoed %s' % [hex(x) for x in got]))

    # 7 --------------------------------------------- INT4 flag model robustness
    h = Host()
    data = rnd_bytes(32, 3)
    for b in data:
        h.byte(b)
    r = Run(prog, h, t_end=h.t + 30 * T_NOM * 4, int4_latch_when_disabled=True)
    results.append(check('back-to-back echo if INT4 flag latches while EX4=0',
                         not r.fault and r.echoed() == data,
                         r.fault or '%d/%d match' % (sum(a == b for a, b in zip(r.echoed(), data)), len(data))))

    # 8 --------------------------------------------- interrupt-latency sensitivity
    for lat in (4, 9):
        h = Host()
        data = rnd_bytes(32, 4)
        for b in data:
            h.byte(b)
        r = Run(prog, h, t_end=h.t + 30 * T_NOM * 4, irq_entry=lat)
        results.append(check('back-to-back echo with %d-cycle IRQ entry latency' % lat,
                             not r.fault and r.echoed() == data,
                             r.fault or '%d/%d match' % (sum(a == b for a, b in zip(r.echoed(), data)), len(data))))

    # 9 --------------------------------------------- sustained overrun
    h = Host(err=-0.04)                        # host 4% fast: echo falls behind
    data = rnd_bytes(400, 5)
    for b in data:
        h.byte(b)
    r = Run(prog, h, t_end=h.t + 30 * T_NOM * 10)
    got = r.echoed()
    it = iter(data)
    subseq = all(any(g == d for d in it) for g in got)
    bff = r.cpu.iram[0x20] >> 3 & 1
    results.append(check('400 bytes, host 4% fast: overflow flagged, rest in order',
                         not r.fault and subseq and len(got) > 380 and (bff or len(got) == len(data)),
                         r.fault or '%d/%d echoed in order, %d dropped, SWBFF=%s' % (
                             len(got), len(data), len(data) - len(got), bff)))

    print('-' * 78)
    print('  %d/%d checks passed' % (sum(results), len(results)))
    return all(results)


if __name__ == '__main__':
    ok = True
    for a in sys.argv[1:]:
        ok &= main(a)
    sys.exit(0 if ok else 1)
