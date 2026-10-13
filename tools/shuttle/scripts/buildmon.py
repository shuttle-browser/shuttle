#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["textual>=1.0", "psutil>=6.0"]
# ///
"""buildmon: live TUI for the build host's sccache-dist service and system resources.

    scripts/buildmon                      # on the build host (wrapper: uv run --script, deps cached by uv)
    ssh -t <build-host> <checkout>/src/tools/shuttle/scripts/buildmon   # from the Mac

Panels: build service (units, scheduler, compiles per minute, per-server split, 60-min history),
builds (siso [done/total] pushed by scripts/buildmon_push.py from the Mac, plus local
$SHUTTLE_ROOT/logs/build-*.log; rate + ETA), CPU/RAM/swap/pressure/temps, GPU, disks, events.
Keys: q quit, r reset session counters, w cycle the cpm window (1/5/15 min), p pause, ? help.
Read-only: journalctl, the scheduler status endpoint, /proc, nvidia-smi, scripts/buildhost-temp.sh.
"""
import collections
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import threading
import time
import urllib.request

import psutil
from rich.console import Group
from rich.table import Table
from rich.text import Text
from textual.app import App, ComposeResult
from textual.containers import Grid, Vertical
from textual.widgets import Footer, Header, RichLog, Sparkline, Static

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from buildmon_push import parse_log  # noqa: E402
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCHED = f"http://{os.environ.get('SHUTTLE_BUILD_HOST_IP', '127.0.0.1')}:10600/api/v1/scheduler/status"
UNITS = {'scheduler': 'sccache-dist-scheduler.service', 'server 1': 'sccache-dist-server@1.service',
         'server 2': 'sccache-dist-server@2.service'}
PORT_NAME = {'10501': 's1', '10502': 's2'}
STATE_DIR = os.path.expanduser('~/.local/state/buildmon')
LOG_GLOB = os.path.join(_SHUTTLE_LOGS, 'build-*.log')
DISKS = [('/', 'WSL root (/)'), ('/mnt/c', 'Windows C:')]
BACKFILL_MIN = 60
WINDOWS = [1, 5, 15]
NVSMI = shutil.which('nvidia-smi') or '/usr/lib/wsl/lib/nvidia-smi'

JOB_RE = re.compile(r'Job (\d+) (created and will be assigned to server ServerId\([^:]+:(\d+)\)|updated state to (\w+))')
LEVEL_RE = re.compile(r'\b(WARN|ERROR)\b')
UNIT_RE = re.compile(r'Main process exited|Failed with result|Scheduled restart job|^Stopped |^Started |^Stopping ')


# ----------------------------------------------------------------------------- helpers
def bar(frac, width=24, warn=0.75, crit=0.9, color=None):
    frac = max(0.0, min(1.0, frac or 0.0))
    eighths = int(round(frac * width * 8))
    full, part = divmod(eighths, 8)
    s = '█' * full + (' ▏▎▍▌▋▊▉'[part] if part else '')
    c = color or ('red' if frac >= crit else 'yellow' if frac >= warn else 'green')
    t = Text(s, style=c)
    t.append('·' * (width - len(s)), style='grey30')
    return t


def human(n, unit='B'):
    for u in ['', 'K', 'M', 'G', 'T']:
        if abs(n) < 1024 or u == 'T':
            return f'{n:.1f} {u}{unit}' if u else f'{n:.0f} {unit}'
        n /= 1024


def dur(s):
    if s is None:
        return '–'
    s = int(s)
    h, r = divmod(s, 3600)
    m, s = divmod(r, 60)
    return f'{h}h{m:02d}m' if h else f'{m}m{s:02d}s'


def psi(kind):
    try:
        with open(f'/proc/pressure/{kind}') as f:
            line = f.readline()
        return float(line.split()[1].split('=')[1])
    except OSError:
        return 0.0


def kv_table():
    t = Table.grid(padding=(0, 1))
    t.add_column(style='bold cyan', no_wrap=True)
    t.add_column()
    return t


# ----------------------------------------------------------------------------- collectors
class Journal(threading.Thread):
    """Follows the sccache-dist journal: job completions (ts, server), failures, warnings, unit events."""

    def __init__(self, events):
        super().__init__(daemon=True)
        self.lock = threading.Lock()
        self.events = events          # deque of (ts, style, text) for the event log
        self.done = collections.deque()   # (ts, server)
        self.failed = collections.deque()
        self.job_server = {}
        self.seen = {}
        self.last_evt = 0.0
        self.seq = 0

    def run(self):
        cmd = ['journalctl', '-f', '--no-pager', '-o', 'short-unix', '--since', f'-{BACKFILL_MIN}min',
               '-u', 'sccache-dist-*']
        while True:
            try:
                p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
                for line in p.stdout:
                    self.handle(line.rstrip('\n'))
            except Exception as e:  # noqa: BLE001
                self.note('red', f'journal follower: {e}')
            time.sleep(5)

    def note(self, style, text, ts=None, key=None, every=600):
        ts = ts or time.time()
        if key is not None:
            if ts - self.seen.get(key, 0) < every:
                return
            self.seen[key] = ts
        with self.lock:
            self.seq += 1
            self.events.append((self.seq, ts, style, text))

    def handle(self, line):
        try:
            ts_s, rest = line.split(' ', 1)
            ts = float(ts_s)
        except ValueError:
            return
        msg = rest.split(': ', 1)[1] if ': ' in rest else rest
        m = JOB_RE.search(msg)
        if m:
            job = m.group(1)
            if m.group(3):
                self.job_server[job] = PORT_NAME.get(m.group(3), m.group(3))
            elif m.group(4) == 'Complete':
                with self.lock:
                    self.done.append((ts, self.job_server.pop(job, '?')))
            self.last_evt = ts
            return
        if LEVEL_RE.search(msg):
            if re.search(r'Res \d+ error', msg):
                with self.lock:
                    self.failed.append(ts)
            key = re.sub(r'[0-9a-f]{16,}|\d+', 'N', re.sub(r'^.*?(WARN|ERROR)\s+', '', msg))[:160]
            text = re.sub(r'^\[\S+Z\s+', '[', msg)   # drop sccache's own UTC timestamp
            self.note('red' if 'ERROR' in msg else 'yellow', text[:400], ts, key=key, every=10**9)
            return
        if UNIT_RE.search(msg):
            self.note('magenta', 'unit: ' + msg[:200], ts, key='unit:' + re.sub(r'\d+', 'N', msg), every=60)

    def counts(self, since):
        with self.lock:
            return sum(1 for t, _ in self.done if t >= since)

    def snapshot(self, now, horizon=BACKFILL_MIN * 60):
        with self.lock:
            while self.done and self.done[0][0] < now - horizon:
                self.done.popleft()
            while self.failed and self.failed[0] < now - horizon:
                self.failed.popleft()
            return list(self.done), list(self.failed)


class Slow(threading.Thread):
    """Slow probes off the UI thread: scheduler status, unit states, GPU, temperatures."""

    def __init__(self):
        super().__init__(daemon=True)
        self.sched = None
        self.units = {}
        self.gpu = None
        self.temps = {}
        self.next_temp = 0

    def run(self):
        while True:
            try:
                raw = urllib.request.urlopen(SCHED, timeout=3).read()
                self.sched = struct.unpack('<QQQ', raw[:24]) if len(raw) >= 24 else None
            except Exception:  # noqa: BLE001
                self.sched = None
            for name, unit in UNITS.items():
                r = subprocess.run(['systemctl', 'is-active', unit], capture_output=True, text=True)
                self.units[name] = r.stdout.strip() or 'unknown'
            try:
                r = subprocess.run([NVSMI, '--query-gpu=name,utilization.gpu,memory.used,memory.total,'
                                    'temperature.gpu,power.draw', '--format=csv,noheader,nounits'],
                                   capture_output=True, text=True, timeout=5)
                f = [x.strip() for x in r.stdout.strip().split(',')]
                self.gpu = {'name': f[0], 'util': float(f[1]), 'mem': float(f[2]), 'mem_total': float(f[3]),
                            'temp': float(f[4]), 'power': float(f[5])} if len(f) == 6 else None
            except Exception:  # noqa: BLE001
                self.gpu = None
            if time.time() >= self.next_temp:   # PowerShell + LibreHardwareMonitor, ~1-2 s per call
                self.next_temp = time.time() + 30
                try:
                    r = subprocess.run([os.path.join(REPO, 'scripts/buildhost-temp.sh')], capture_output=True,
                                       text=True, timeout=25)
                    self.temps = dict(kv.split('=', 1) for kv in r.stdout.split() if '=' in kv and kv != 'temp=n/a')
                except Exception:  # noqa: BLE001
                    self.temps = {}
            time.sleep(2)


class Builds:
    """Pushed snapshots (~/.local/state/buildmon/*.json) and local siso logs, with rate + ETA."""

    def __init__(self):
        self.local_hist = collections.defaultdict(list)

    def collect(self, now):
        out = []
        for p in glob.glob(os.path.join(STATE_DIR, '*.json')):
            try:
                with open(p) as f:
                    s = json.load(f)
            except (OSError, ValueError):
                continue
            s['name'] = f"{s.get('machine', '?')} {s.get('dir', '?')}"
            s['age'] = now - s.get('ts', 0)
            s['stale'] = s['state'] == 'running' and s['age'] > 60
            out.append(s)
        for p in glob.glob(LOG_GLOB):
            if now - os.path.getmtime(p) > 6 * 3600:
                continue
            try:
                s = parse_log(p)
            except OSError:
                continue
            h = self.local_hist[p]
            if s.get('done') is not None and (not h or h[-1][1] != s['done'] or now - h[-1][0] >= 60):
                h.append([now, s['done']])
            self.local_hist[p] = [x for x in h if now - x[0] <= 900]
            s.update(name=f"host {os.path.basename(p)[6:-4]}", history=self.local_hist[p],
                     age=now - s['mtime'])
            s['stale'] = s['state'] == 'running' and s['age'] > 300
            out.append(s)
        for s in out:
            s['rate'] = self.rate(s.get('history') or [], now)
            rem = (s['total'] - s['done']) if s.get('total') and s.get('done') is not None else None
            s['eta'] = rem / s['rate'] * 60 if rem and s['rate'] else (0 if rem == 0 else None)
        out.sort(key=lambda s: (s['state'] != 'running', -s.get('ts', s.get('mtime', 0))))
        return out[:4]

    @staticmethod
    def rate(hist, now, window=300):
        """Steps per minute over the last `window` s (falls back to the whole history)."""
        if len(hist) < 2:
            return None
        recent = [h for h in hist if now - h[0] <= window]
        if len(recent) < 2:
            recent = hist[-2:]
        dt = recent[-1][0] - recent[0][0]
        return (recent[-1][1] - recent[0][1]) / dt * 60 if dt > 0 else None


# ----------------------------------------------------------------------------- UI
class Panel(Static):
    DEFAULT_CSS = 'Panel { border: round $primary; padding: 0 1; height: auto; }'

    def __init__(self, title, **kw):
        super().__init__(**kw)
        self.border_title = title


class BuildMon(App):
    TITLE = 'Shuttle buildmon'
    SUB_TITLE = 'build host · sccache-dist'
    CSS = """
    Screen { layout: vertical; }
    #grid { grid-size: 2; grid-columns: 1fr 1fr; grid-rows: auto auto; height: auto; grid-gutter: 0 1; }
    .col { height: auto; }
    Sparkline { height: 3; margin: 0 1; }
    #cpm-spark > .sparkline--max-color { color: $success; }
    #cpu-spark > .sparkline--max-color { color: $warning; }
    #events { border: round $primary; height: 1fr; min-height: 6; }
    .label { color: $text-muted; margin: 0 1; height: 1; }
    """
    BINDINGS = [('q', 'quit', 'Quit'), ('r', 'reset', 'Reset counters'), ('w', 'window', 'cpm window'),
                ('p', 'pause', 'Pause'), ('question_mark', 'help', 'Help')]

    def __init__(self):
        super().__init__()
        self.events = collections.deque(maxlen=500)
        self.journal = Journal(self.events)
        self.slow = Slow()
        self.builds = Builds()
        self.session_start = time.time()
        self.win = 1
        self.paused = False
        self.cpu_hist = collections.deque([0.0] * 120, maxlen=120)
        self.peak_cpm = 0.0
        self.swap0 = psutil.swap_memory().used
        self.sw_prev = (time.time(), psutil.swap_memory().sin, psutil.swap_memory().sout)
        self.alerted = {}
        self.logged = 0

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Grid(id='grid'):
            with Vertical(classes='col'):
                yield Panel('Build service', id='service')
                yield Static('compiles / min, last 60 min', classes='label')
                yield Sparkline([0] * 60, id='cpm-spark', summary_function=max)
            yield Panel('Builds', id='builds')
            with Vertical(classes='col'):
                yield Panel('CPU · memory', id='system')
                yield Static('CPU %, last 2 min', classes='label')
                yield Sparkline(list(self.cpu_hist), id='cpu-spark', summary_function=max)
            with Vertical(classes='col'):
                yield Panel('GPU', id='gpu')
                yield Panel('Disks', id='disks')
        yield RichLog(id='events', wrap=True, markup=False, highlight=False)
        yield Footer()

    def on_mount(self):
        self.query_one('#events', RichLog).border_title = 'Events'
        psutil.cpu_percent(percpu=True)
        self.journal.start()
        self.slow.start()
        self.set_interval(1.0, self.tick)
        self.tick()

    # -- actions
    def action_reset(self):
        self.session_start = time.time()
        self.peak_cpm = 0.0
        self.swap0 = psutil.swap_memory().used
        self.journal.note('cyan', 'session counters reset')

    def action_window(self):
        self.win = WINDOWS[(WINDOWS.index(self.win) + 1) % len(WINDOWS)]
        self.tick()

    def action_pause(self):
        self.paused = not self.paused
        self.sub_title = 'build host · sccache-dist' + (' · PAUSED' if self.paused else '')

    def action_help(self):
        self.notify('q quit · r reset session counters + swap baseline · w cycle cpm window (1/5/15 min) · '
                    'p pause updates. Mac progress: python3 scripts/buildmon_push.py <Dir> on the Mac.',
                    title='buildmon', timeout=10)

    def alert(self, key, cond, text):
        if cond and time.time() - self.alerted.get(key, 0) > 300:
            self.alerted[key] = time.time()
            self.journal.note('bold red', 'ALERT ' + text)
            self.notify(text, title='ALERT', severity='error', timeout=15)

    # -- refresh
    def tick(self):
        if self.paused:
            return
        now = time.time()
        self.render_service(now)
        self.render_builds(now)
        self.render_system(now)
        self.render_gpu()
        self.render_disks()
        log = self.query_one('#events', RichLog)
        with self.journal.lock:
            new = [e for e in self.events if e[0] > self.logged]
            self.logged = self.journal.seq
        for _, ts, style, text in new[-50:]:
            log.write(Text.assemble((time.strftime('%H:%M:%S ', time.localtime(ts)), 'grey50'), (text, style)))

    def render_service(self, now):
        done, failed = self.journal.snapshot(now)
        w = self.win * 60
        cpm = sum(1 for t, _ in done if t >= now - w) / self.win
        cpm1 = sum(1 for t, _ in done if t >= now - 60)
        cpm15 = sum(1 for t, _ in done if t >= now - 900) / 15
        self.peak_cpm = max(self.peak_cpm, cpm1)
        sess = [s for t, s in done if t >= self.session_start]
        per = collections.Counter(s for t, s in done if t >= now - w)
        hour = len(done)
        t = kv_table()
        units = Text()
        for name, st in self.slow.units.items():
            units.append('● ', style='green' if st == 'active' else 'red')
            units.append(f'{name}  ')
        t.add_row('units', units or Text('…', style='grey50'))
        if self.slow.sched:
            n, cpus, inprog = self.slow.sched
            t.add_row('scheduler', Text.assemble(f'{n} servers · {cpus} cpus · ', (f'{inprog}', 'bold'),
                                                 ' in progress  ', bar(inprog / 12, 12, 0.9, 1.01, 'cyan'),
                                                 (' of 12 real', 'grey50')))
        else:
            t.add_row('scheduler', Text('unreachable', style='bold red'))
        idle = now - self.journal.last_evt if self.journal.last_evt else None
        state = Text('active', style='bold green') if idle is not None and idle < 30 else \
            Text(f'idle {dur(idle)}' if idle else 'idle', style='grey50')
        t.add_row('activity', state)
        t.add_row(f'cpm ({self.win}m)', Text.assemble((f'{cpm:6.1f}', 'bold green'),
                                                   f'   1m {cpm1:.0f} · 15m {cpm15:.1f} · peak {self.peak_cpm:.0f}'))
        t.add_row('per server', Text(' · '.join(f'{k} {v / self.win:.1f}/min' for k, v in sorted(per.items()))
                                     or '–'))
        t.add_row('compiles', Text.assemble((f'{len(sess)}', 'bold'), f' this session ({dur(now - self.session_start)})'
                                            f' · {hour} last {BACKFILL_MIN} min'))
        nf = sum(1 for x in failed if x >= self.session_start)
        t.add_row('failed', Text(f'{nf} this session · {len(failed)} last {BACKFILL_MIN} min',
                                 style='bold red' if nf else 'green'))
        self.query_one('#service', Panel).update(t)
        buckets = [0] * 60
        for ts, _ in done:
            i = int((now - ts) // 60)
            if 0 <= i < 60:
                buckets[59 - i] += 1
        self.query_one('#cpm-spark', Sparkline).data = buckets
        self.alert('units', any(v != 'active' for v in self.slow.units.values()),
                   'sccache-dist unit down: ' + ', '.join(k for k, v in self.slow.units.items() if v != 'active'))

    def render_builds(self, now):
        rows = []
        for s in self.builds.collect(now):
            st = s['state']
            style = {'running': 'bold cyan', 'succeeded': 'bold green', 'failed': 'bold red'}.get(st, 'grey50')
            head = Text.assemble((s['name'], 'bold'), '  ', (st.upper(), style))
            if s.get('stale'):
                head.append(f'  stale {dur(s["age"])}', style='yellow')
            rows.append(head)
            done, total = s.get('done'), s.get('total')
            if total:
                frac = done / total
                rows.append(Text.assemble(bar(frac, 30, 2, 2, 'green' if st == 'succeeded' else 'cyan'),
                                          (f' {frac * 100:5.1f}%', 'bold'), f'  {done:,}/{total:,}'))
            t = kv_table()
            if s.get('rate'):
                t.add_row('rate', f"{s['rate']:.0f} steps/min (5m)")
            if st == 'running':
                eta = s.get('eta')
                t.add_row('ETA', Text.assemble((dur(eta), 'bold yellow'),
                                               f"  → {time.strftime('%H:%M', time.localtime(now + eta))}"
                                               if eta else ''))
            c = s.get('counters') or {}
            if c:
                t.add_row('in flight', ' · '.join(f'{k} {v}' for k, v in c.items()))
            info = []
            if s.get('elapsed'):
                info.append(f"elapsed {s['elapsed']}")
            if s.get('started'):
                info.append(f"started {s['started'][5:16]}")
            if info:
                t.add_row('time', ' · '.join(info))
            if s.get('command'):
                t.add_row('cmd', Text(s['command'][:70], style='grey50'))
            rows += [t, Text('')]
        if not rows:
            rows = [Text('No build progress yet.', style='grey50'),
                    Text('On the Mac: python3 scripts/buildmon_push.py Release', style='grey50'),
                    Text('Local Linux builds ($SHUTTLE_ROOT/logs/build-*.log) appear automatically.', style='grey50')]
        self.query_one('#builds', Panel).update(Group(*rows))

    def render_system(self, now):
        per = psutil.cpu_percent(percpu=True)
        tot = sum(per) / len(per)
        self.cpu_hist.append(tot)
        self.query_one('#cpu-spark', Sparkline).data = list(self.cpu_hist)
        vm, sw = psutil.virtual_memory(), psutil.swap_memory()
        t0, si0, so0 = self.sw_prev
        dt = max(now - t0, 1e-3)
        si, so = (sw.sin - si0) / dt / 4096, (sw.sout - so0) / dt / 4096
        self.sw_prev = (now, sw.sin, sw.sout)
        la = os.getloadavg()
        t = kv_table()
        t.add_row('CPU', Text.assemble(bar(tot / 100, 24), f' {tot:5.1f}%  load {la[0]:.1f} {la[1]:.1f} {la[2]:.1f}'))
        cores = Text()
        for i, p in enumerate(per):
            cores.append('▁▂▃▄▅▆▇█'[min(7, int(p / 12.5))], style='red' if p > 90 else 'yellow' if p > 60 else 'green')
        t.add_row('threads', Text.assemble(cores, (f'  {len(per)} threads · clang {self.clang_count()}', 'grey50')))
        t.add_row('RAM', Text.assemble(bar(vm.percent / 100, 24), f' {human(vm.used)} / {human(vm.total)}'
                                       f'  avail {human(vm.available)}'))
        grow = sw.used - self.swap0
        t.add_row('swap', Text.assemble(bar(sw.percent / 100, 24, 0.1, 0.25), f' {human(sw.used)}',
                                        (f' ({"+" if grow >= 0 else ""}{human(grow)})', 'yellow' if grow > 256 << 20
                                         else 'grey50'), f'  in {si:.0f} out {so:.0f} pg/s'))
        pc, pm, pio = psi('cpu'), psi('memory'), psi('io')
        t.add_row('pressure', Text.assemble('cpu ', (f'{pc:.1f}', 'yellow' if pc > 50 else ''), ' · mem ',
                                            (f'{pm:.1f}', 'red' if pm > 10 else ''), ' · io ',
                                            (f'{pio:.1f}', 'yellow' if pio > 20 else ''), (' (avg10 %)', 'grey50')))
        tp = self.slow.temps
        if tp:
            pkg = int(re.sub(r'\D', '', tp.get('pkg', '0')) or 0)
            t.add_row('sensors', Text.assemble(('pkg ' + tp.get('pkg', '?'), 'red' if pkg >= 90 else
                                                'yellow' if pkg >= 80 else ''),
                                               ' · ' + ' · '.join(f'{k} {v}' for k, v in tp.items() if k != 'pkg')))
        self.query_one('#system', Panel).update(t)
        self.alert('swap', grow > 1 << 30, f'swap grew {human(grow)} since start/reset')
        self.alert('swapio', si + so > 500, f'swapping: in {si:.0f} out {so:.0f} pages/s')
        self.alert('avail', vm.available < 3 << 30, f'MemAvailable {human(vm.available)}')
        self.alert('psi', pm > 10, f'memory pressure avg10 {pm:.1f}%')

    _clang = (0, 0)

    def clang_count(self):
        if time.time() - self._clang[0] > 3:
            n = sum(1 for p in psutil.process_iter(['name']) if (p.info['name'] or '').startswith('clang'))
            self._clang = (time.time(), n)
        return self._clang[1]

    def render_gpu(self):
        g = self.slow.gpu
        if not g:
            self.query_one('#gpu', Panel).update(Text('nvidia-smi unavailable', style='grey50'))
            return
        t = kv_table()
        t.add_row('device', g['name'])
        t.add_row('util', Text.assemble(bar(g['util'] / 100, 24), f" {g['util']:.0f}%"))
        t.add_row('VRAM', Text.assemble(bar(g['mem'] / g['mem_total'], 24),
                                        f" {g['mem'] / 1024:.1f} / {g['mem_total'] / 1024:.1f} GiB"))
        t.add_row('temp · power', f"{g['temp']:.0f} °C · {g['power']:.1f} W")
        self.query_one('#gpu', Panel).update(t)

    def render_disks(self):
        t = kv_table()
        for path, label in DISKS:
            try:
                u = shutil.disk_usage(path)
            except OSError:
                continue
            f = u.used / u.total
            t.add_row(label, Text.assemble(bar(f, 24, 0.8, 0.92), f' {human(u.total - u.used)} free'
                                           f' of {human(u.total)}'))
        self.query_one('#disks', Panel).update(t)


if __name__ == '__main__':
    BuildMon().run()
