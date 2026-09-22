#!/usr/bin/env python3
"""Per label: probe TI hashes for rt/p213, pub/sub match + received, tool pair verdict."""
import json, re, sys, glob, os
# logs of run_case.sh: <repo>/build/probes/p213_ros/out
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'build', 'probes', 'p213_ros', 'out'))
labels = sys.argv[1:] or sorted({p.split('.')[0] for p in glob.glob('*.probe.log')})
for L in labels:
    print(f"===== {L}")
    pr = open(f'{L}.probe.log').read()
    for m in re.finditer(r'(WRITER|READER) topic=rt/p213 type=(\S+) guid=\S+\n\s+type_information.assigned=(\w+) minimal\{_d=\S+ hash=(\w+) size=(\d+)\} complete\{_d=\S+ hash=(\w+) size=(\d+)\}\n\s+representation=\[[^\]]*\] user_data="typehash=(\w+);', pr):
        k, tn, a, mn, ms, cp, cs, th = m.groups()
        print(f"  {k:6} TI min={mn} ({ms}B) cmp={cp} ({cs}B) rep2011={th[7:23]}")
    pub = open(f'{L}.pub.log').read(); sub = open(f'{L}.sub.log').read()
    pm = re.findall(r'matched=(\d+)', pub); sm = re.findall(r'sub matched=(\d+) recv=(\d+)', sub)
    recv = len(re.findall(r'RECV|^s: ', sub, re.M))
    errs = [l for l in sub.splitlines() if re.search(r'rror|xception|fail', l)][:2]
    print(f"  pub matched(last)={pm[-1] if pm else '?'}  sub matched/recv(last)={sm[-1] if sm else '-'}  recv lines={recv} {errs}")
    j = json.load(open(f'{L}.json'))
    for t in j['topics']:
        if t.get('dds_topic') != 'rt/p213': continue
        for e in t['writers'] + t['readers']:
            print(f"  ep node={e['node']:8} type_hash={e['type_hash'][7:23]} tih={e.get('type_information_hash','')[:16]}")
        for p in t['pairs']:
            print(f"  tool pair {p['transport']} conf={p['confidence']} reasons={p['reasons']} warnings={p['warnings']}")
        print(f"  unmatched_reasons={t.get('unmatched_reasons')}")
    err = open(f'{L}.tool.err').read().strip()
    if err: print('  tool stderr:', err[:300])
