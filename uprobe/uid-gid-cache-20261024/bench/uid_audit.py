#!/usr/bin/env python3
"""UIDs recorded by sslsniff -x in audit captures (audit-uid.json.<leg>.sslsniff.txt), next to the
nginx worker's real uid. Event lines: FUNC TIME COMM PID LEN UID TID."""
import collections, glob, re, sys
from pathlib import Path
root = Path(__file__).resolve().parent
for f in sorted(glob.glob(str(root / 'audit-uid.json.*.sslsniff.txt'))):
    leg = f.split('audit-uid.json.')[1].split('.sslsniff')[0]
    uids = collections.Counter()
    for line in open(f, errors='replace'):
        if line.startswith(('READ/RECV', 'WRITE/SEND', 'HANDSHAKE')):
            parts = line.split()
            uids[parts[5]] += 1
    print(f'{leg}: events by UID {dict(uids)}')
