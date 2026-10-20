#!/usr/bin/env python3
"""Quiet build watcher for a Monitor: every stdout line is one notification.

Usage: scripts/build-watch.py <Dir> [--step 10] [--stall 10] [--health-every 5]
                              [--health-cmd CMD] [--from-start]

Watches $SHUTTLE_ROOT/logs/build-<Dir>.log (written by scripts/build.sh). Emits only:
  - progress every --step percent: done/total, rate over the last 15 min, ETA,
    failed-step count;
  - the first error of each FAILED step, deduplicated by message (repeats are
    only counted), max 15 distinct;
  - the output of --health-cmd (optional, e.g. a status check of something the build depends on), only when
    it changes or fails;
  - "stalled" when the step count hasn't moved for --stall minutes;
  - a final summary on BUILD EXIT (or when the build process disappears).
Works on macOS and Linux.
"""
import argparse, os, re, subprocess, sys, time
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

ANSI = re.compile(r'\x1b\[[0-9;?]*[A-Za-z]')
PROGRESS = re.compile(r'^\[\s*(\d+)/(\d+)\]')
LOC = re.compile(r'^[^\s:]+:\d+(:\d+)?:\s*')       # file:line:col prefix
OBJ = re.compile(r'\S*obj/\S+|\[[^\]]*\.o\]')

def out(msg):
    print(msg, flush=True)

class Tail:
    """Reads new bytes of a file; survives truncation (build.sh truncates the log)."""
    def __init__(self, path, from_start):
        self.path, self.pos, self.buf = path, 0, ''
        if not from_start and os.path.exists(path):
            self.pos = os.path.getsize(path)
    def lines(self):
        try:
            size = os.path.getsize(self.path)
        except OSError:
            return []
        if size < self.pos:
            self.pos, self.buf = 0, ''
        if size == self.pos:
            return []
        with open(self.path, 'rb') as f:
            f.seek(self.pos)
            data = f.read(size - self.pos)
        self.pos = size
        text = self.buf + ANSI.sub('', data.decode('utf-8', 'replace'))
        parts = re.split(r'\r\n|\r|\n', text)
        self.buf = parts.pop()
        return parts

def norm(msg):
    msg = LOC.sub('', msg.strip())
    msg = OBJ.sub('<obj>', msg)
    return re.sub(r'\s+', ' ', msg)[:200]

def proc_alive(d):
    pat = f'out/{d}'
    try:
        r = subprocess.run(['pgrep', '-f', f'(autoninja|siso).*{pat}'], capture_output=True)
        return r.returncode == 0
    except OSError:
        return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--step', type=int, default=10)
    ap.add_argument('--stall', type=int, default=10, help='minutes')
    ap.add_argument('--health-every', type=int, default=5, help='minutes')
    ap.add_argument('--health-cmd')
    ap.add_argument('--from-start', action='store_true',
                    help='read the logs from the beginning (default: only new output)')
    a = ap.parse_args()
    logs = _SHUTTLE_LOGS
    blog = Tail(f'{logs}/build-{a.dir}.log', a.from_start)
    health_cmd = a.health_cmd

    start = time.time()
    samples = []                 # (t, done)
    last_bucket, last_done, last_move = -1, -1, time.time()
    stalled = False
    total, done = 0, 0
    failed, errs, errcount = 0, {}, {}   # key -> first message, key -> count
    in_fail, fail_target, fail_found = False, '', False
    health_last, health_next = None, 0.0
    gone = 0
    exit_line = None
    finished = None

    def fail_err(target, line):
        nonlocal fail_found
        fail_found = True
        key = norm(line)
        errcount[key] = errcount.get(key, 0) + 1
        if key not in errs and len(errs) < 15:
            errs[key] = line
            out(f'{a.dir} FAIL #{failed} {target}: {line.strip()[:300]}')

    while True:
        for line in blog.lines():
            s = line.strip()
            m = PROGRESS.match(s)
            if m:
                done, total = int(m.group(1)), int(m.group(2))
                continue
            if s.startswith('FAILED:'):
                if in_fail and not fail_found:
                    fail_err(fail_target, '(no error line found)')
                failed += 1
                in_fail, fail_found = True, False
                t = re.search(r'"([^"]+)"', s)
                fail_target = (t.group(1) if t else s[8:80]).lstrip('./')
                continue
            if in_fail and not fail_found and not s.startswith('err: exit=') \
                    and re.search(r'\berror\b[: ]|fatal error|killed by signal', s):
                fail_err(fail_target, s)
                continue
            m2 = re.search(r'(\d+m)?[\d.]+s Build (Succeeded|Failed): (\d+) steps', s)
            if m2:
                finished = f'{m2.group(3)} steps, {s.split(" Build")[0]}'
            if s.startswith('BUILD EXIT'):
                exit_line = s

        now = time.time()
        # progress (Siso's first total is before its up-to-date scan: wait 45 s)
        if total and (now - start > 45 or exit_line):
            d = done
            if d != last_done:
                last_done, last_move, stalled = d, now, False
                samples.append((now, d))
                while len(samples) > 2 and now - samples[0][0] > 900:
                    samples.pop(0)
            pct = d * 100 // total if total else 0
            bucket = pct // a.step * a.step
            if bucket > last_bucket and not exit_line:
                last_bucket = bucket
                eta = ''
                t0, d0 = samples[0]
                if now - t0 >= 120 and d > d0:
                    rate = (d - d0) * 600 / (now - t0)
                    secs = (total - d) * 600 / rate
                    eta = (f', {rate:.0f} steps/10min, ETA {int(secs//3600)}h{int(secs%3600//60):02d}m'
                           f' (~{time.strftime("%H:%M", time.localtime(now + secs))})')
                extra = ''
                if failed:
                    extra += f', {failed} failed ({len(errcount)} distinct)'
                out(f'{a.dir} {pct}% ({d}/{total}) at {time.strftime("%H:%M")}{eta}{extra}')
            if not stalled and not exit_line and now - last_move > a.stall * 60:
                stalled = True
                out(f'{a.dir} STALLED: no step finished for {a.stall} min (at {d}/{total})')

        # --health-cmd, only on change
        if health_cmd and not exit_line and now >= health_next:
            health_next = now + a.health_every * 60
            try:
                r = subprocess.run(health_cmd, shell=True, capture_output=True, text=True, timeout=20)
                lines = r.stdout.strip().splitlines()
                if r.returncode == 0:
                    h = (lines or [''])[-1]
                else:  # name what's down: stdout lines that aren't "active", else stderr
                    bad = [l.strip() for l in lines if l.strip() and not re.search(r'\bactive\b', l)]
                    h = f'DOWN rc={r.returncode}: ' + ('; '.join(bad) or r.stderr.strip())[:200]
            except subprocess.TimeoutExpired:
                h = 'FAILED: timeout'
            hkey = re.sub(r'\d+ in progress', '', h)
            if hkey != health_last:
                health_last = hkey
                out(f'{a.dir} health: {h}')

        if exit_line:
            parts = [f'{a.dir} {exit_line}', finished or f'{done}/{total} steps']
            if failed:
                top = sorted(errcount.items(), key=lambda kv: -kv[1])[:5]
                parts.append(f'{failed} failed, {len(errcount)} distinct: ' +
                             '; '.join(f'{c}x {k[:90]}' for k, c in top))
            out(' | '.join(parts))
            return 0

        gone = 0 if proc_alive(a.dir) else gone + 1
        if gone >= 4:
            out(f'{a.dir} build process gone without BUILD EXIT (at {last_done}/{total}, {failed} failed)')
            return 1
        time.sleep(15)

if __name__ == '__main__':
    sys.exit(main())
