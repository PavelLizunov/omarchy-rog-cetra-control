"""Bounded read-only /proc observation. No capture, restart or process attachment.

Usage: python3 -B tests/resource-observe.py --seconds 1800
Reports aggregate resources, not audio samples or per-plugin shell attribution.
"""
import argparse
import json
import os
import pathlib
import subprocess
import time

args = argparse.ArgumentParser()
args.add_argument('--seconds', type=int, default=60)
duration = args.parse_args().seconds
if not 1 <= duration <= 3600:
    raise SystemExit('Observation must be between 1 and 3600 seconds')
hz = os.sysconf('SC_CLK_TCK')
histories = {}
errors = []
start = time.monotonic()
deadline = start + duration
while True:
    for name in ('cetra-watch', 'cetra-peak', 'quickshell'):
        pids = subprocess.run(['pgrep', '-x', name], capture_output=True, text=True).stdout.split()
        for pid in pids:
            try:
                root = pathlib.Path('/proc') / pid
                stat = root.joinpath('stat').read_text().split(') ', 1)[1].split()
                status = dict(line.split(':', 1) for line in root.joinpath('status').read_text().splitlines())
                key = (name, pid, stat[19])
                row = {'t': time.monotonic(), 'ticks': int(stat[11]) + int(stat[12]),
                       'rss_kib': int(status['VmRSS'].split()[0]),
                       'fds': len(list(root.joinpath('fd').iterdir())), 'threads': int(status['Threads'])}
                histories.setdefault(key, []).append(row)
            except (OSError, KeyError, ValueError) as error:
                if len(errors) < 20: errors.append(f'{name}/{pid}: {error}')
    remaining = deadline - time.monotonic()
    if remaining <= 0: break
    time.sleep(min(10, remaining))
report = {'duration_s': round(time.monotonic() - start, 2), 'errors': errors, 'processes': []}
for (name, pid, identity), rows in histories.items():
    elapsed = rows[-1]['t'] - rows[0]['t']
    result = {'name': name, 'pid': pid, 'start_ticks': identity, 'samples': len(rows)}
    for field in ('rss_kib', 'fds', 'threads'):
        values = [r[field] for r in rows]
        result[field] = {'first': values[0], 'last': values[-1], 'min': min(values), 'max': max(values)}
    result['cpu_percent_one_core'] = round((rows[-1]['ticks'] - rows[0]['ticks']) / hz / elapsed * 100, 3) if elapsed > 0 else None
    report['processes'].append(result)
print(json.dumps(report, indent=2))
