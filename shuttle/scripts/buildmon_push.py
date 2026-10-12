#!/usr/bin/env python3
"""Push a build's siso progress to the build host for the buildmon TUI (scripts/buildmon.py).

Usage (on the Mac, next to a running scripts/build.sh / build-sccache.sh):
    python3 scripts/buildmon_push.py [Dir=Sccache] [--host <ssh-host>] [--every 10] [--once]

Every --every seconds it reads the tail of $SHUTTLE_ROOT/logs/build-<Dir>.log, takes siso's last
"[done/total]" status line and writes a small JSON snapshot (plus the last ~15 min of samples, so
the TUI can compute rates and an ETA without its own history) to
    <host>:~/.local/state/buildmon/<machine>-<Dir>.json
over plain ssh (ControlMaster keeps one connection open). No listener, no extra service.
It exits after pushing the final state once the log ends with "BUILD EXIT <rc>".
Stdlib only: also imported by buildmon.py to read local (Linux) build logs directly.
"""
import argparse
import json
import os
import re
import socket
import subprocess
import sys
import time
from shuttle_paths import SRC as _SHUTTLE_SRC, LOGS as _SHUTTLE_LOGS  # noqa: E402

ANSI = re.compile(r'\x1b\[[0-9;?]*[A-Za-z]')
PROGRESS = re.compile(r'^\[\s*(\d+)/(\d+)\]\s*(\S+)?(.*)$')
STARTED = re.compile(r'^Script started on (\S+ \S+)')
FINISHED = re.compile(r'Build (Succeeded|Failed): (\d+) steps')
EXIT = re.compile(r'BUILD EXIT (\d+)')
STAT = re.compile(r'(\w+):(\d+)')
TAIL = 256 * 1024
HISTORY_S = 15 * 60


def log_path(d):
    return os.path.join(_SHUTTLE_LOGS, f'build-{d}.log')


def parse_log(path):
    """Snapshot of a siso build log: state, done/total, in-flight counters, start time."""
    st = os.stat(path)
    with open(path, 'rb') as f:
        head = f.read(512).decode('utf-8', 'replace')
        f.seek(max(0, st.st_size - TAIL))
        tail = f.read().decode('utf-8', 'replace')
    snap = {'log': path, 'mtime': st.st_mtime, 'state': 'running', 'done': None, 'total': None}
    m = STARTED.match(head)
    if m:
        snap['started'] = m.group(1)
        cmd = re.search(r'COMMAND="([^"]*)"', head)
        if cmd:
            snap['command'] = cmd.group(1)
    lines = [ANSI.sub('', l).strip() for l in re.split(r'\r\n|\r|\n', tail)]
    for l in reversed(lines):
        m = PROGRESS.match(l)
        if m:
            snap['done'], snap['total'] = int(m.group(1)), int(m.group(2))
            snap['elapsed'] = m.group(3)
            snap['counters'] = {k: int(v) for k, v in STAT.findall(m.group(4))}
            break
    for l in reversed(lines[-80:]):
        m = EXIT.search(l)
        if m:
            snap['state'] = 'succeeded' if m.group(1) == '0' else 'failed'
            snap['rc'] = int(m.group(1))
            break
        m = FINISHED.search(l)
        if m:
            snap['state'] = m.group(1).lower()
            if m.group(1) == 'Succeeded':
                snap['done'] = snap['total'] = int(m.group(2))
            break
    if snap['state'] == 'succeeded' and snap['total']:
        snap['done'] = snap['total']   # siso's last status line stops short of the total
    return snap


def push(host, name, payload):
    cmd = ['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5',
           '-o', 'ControlMaster=auto', '-o', 'ControlPath=~/.ssh/cm-buildmon-%r@%h:%p',
           '-o', 'ControlPersist=120', host,
           f'mkdir -p ~/.local/state/buildmon && cat > ~/.local/state/buildmon/.{name}.tmp'
           f' && mv ~/.local/state/buildmon/.{name}.tmp ~/.local/state/buildmon/{name}']
    r = subprocess.run(cmd, input=json.dumps(payload).encode(), capture_output=True, timeout=30)
    return r.returncode == 0, r.stderr.decode(errors='replace').strip()


def main(argv):
    ap = argparse.ArgumentParser(description='Push siso build progress to the build host for buildmon.')
    ap.add_argument('dir', nargs='?', default='Sccache', help='build dir name (log build-<dir>.log)')
    ap.add_argument('--host', default=os.environ.get('SHUTTLE_BUILD_HOST'),
                    help='ssh host running buildmon (default $SHUTTLE_BUILD_HOST)')
    ap.add_argument('--every', type=float, default=10, help='seconds between pushes')
    ap.add_argument('--once', action='store_true', help='push one snapshot and exit')
    a = ap.parse_args(argv)
    d, host, every = a.dir, a.host, a.every
    machine = socket.gethostname().split('.')[0].lower()
    name = f'{machine}-{d}.json'
    path = log_path(d)
    history, warned = [], False
    print(f'buildmon_push: {path} -> {host}:~/.local/state/buildmon/{name} every {every:g}s', flush=True)
    while True:
        now = time.time()
        try:
            snap = parse_log(path)
        except FileNotFoundError:
            snap = {'log': path, 'state': 'no-log', 'done': None, 'total': None}
        if snap.get('done') is not None and (not history or history[-1][1] != snap['done']
                                             or now - history[-1][0] >= 60):
            history.append([round(now, 1), snap['done']])
        history = [h for h in history if now - h[0] <= HISTORY_S]
        snap.update(machine=machine, dir=d, ts=now, history=history)
        try:
            ok, err = push(host, name, snap)
        except subprocess.TimeoutExpired:
            ok, err = False, 'ssh timeout'
        if not ok and not warned:
            print(f'buildmon_push: push failed: {err}', file=sys.stderr, flush=True)
        warned = not ok
        if a.once or snap['state'] in ('succeeded', 'failed'):
            print(f"buildmon_push: {snap['state']} {snap.get('done')}/{snap.get('total')}", flush=True)
            return 0
        time.sleep(every)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
