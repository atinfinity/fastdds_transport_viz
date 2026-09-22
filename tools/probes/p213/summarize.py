#!/usr/bin/env python3
"""Summarise out/<label>.* of run_all.sh: probe hashes, app matches/decodes, ROS listener, tool warnings."""
import json, re, sys, glob, os
# logs of run_case.sh: <repo>/build/probes/p213/out
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'build', 'probes', 'p213'))
labels = sys.argv[1:] or sorted({os.path.basename(p).split('.')[0] for p in glob.glob('out/*.probe.log')})
for L in labels:
    print(f"===== {L}")
    pr = open(f'out/{L}.probe.log').read()
    for m in re.finditer(r'(WRITER|READER) topic=rt/chatter type=(\S+) guid=(\S+)\n\s+type_information.assigned=(\w+) minimal\{([^}]*)\} complete\{([^}]*)\}\n\s+representation=\[[^\]]*\] user_data="([^"]*)"', pr):
        kind, tn, guid, asg, mn, cp, ud = m.groups()
        print(f"  probe {kind} {guid[:24]} ros={'y' if 'typehash' in ud else 'n'} assigned={asg} min{{{mn}}} cmp{{{cp}}}")
    nl = open(f'out/{L}.nonros.log').read().splitlines()
    for l in nl:
        if l.startswith('# END') or l.startswith('#   from') or l.startswith('# fastdds') or 'DROP' in l:
            print('  app', l)
    lm = [l for l in nl if 'MATCH' in l]
    print('  app last matches:', lm[-4:] if len(lm) > 4 else lm)
    li = open(f'out/{L}.listener.log').read()
    fromapp = len(re.findall(r'hello from p206', li))
    print(f"  ROS listener lines from app: {fromapp}; total I heard/frame_id lines: {len(re.findall(r'I heard|frame_id', li))}")
    try:
        j = json.load(open(f'out/{L}.json'))
    except Exception as e:
        print('  json err', e); continue
    eps = {}
    for t in j.get('topics', []):
        if t.get('dds_topic') != 'rt/chatter':
            continue
        for e in t['writers'] + t['readers']:
            eps[e['guid']] = e
            print(f"  ep {e['guid'][:35]} node={e['node'][:28]:28} type_hash={e['type_hash'][:14]:14} tih={e.get('type_information_hash','')[:12]}")
        for p in t.get('pairs', []):
            print(f"  pair {p['writer_guid'][:35]} -> {p['reader_guid'][:35]} {p['transport']} warn={p['warnings']} reasons={[r for r in p['reasons'] if 'type' in r]}")
    err = open(f'out/{L}.tool.err').read().strip()
    if err: print('  tool stderr:', err[:300])
    for l in open(f'out/{L}.table.txt'):
        if '/chatter' in l: print('  table:', ' '.join(l.split()))
