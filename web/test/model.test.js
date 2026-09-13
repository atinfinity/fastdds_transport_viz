// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Unit tests of web/model.js (run: node --test web/test).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const M = require('../model.js');
const load = name => JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'sample', name), 'utf8'));
const sample = load('sample.json');

const allFilter = () => ({ topic: '', node: '', transports: new Set(M.TRANSPORTS), hideInternal: true });

test('humanBytes: SI prefixes and 3 significant digits', () => {
  assert.equal(M.humanBytes(23, 'B/s'), '23 B/s');
  assert.equal(M.humanBytes(1310, 'B'), '1.31 kB');
  assert.equal(M.humanBytes(63400000, 'B'), '63.4 MB');
  assert.equal(M.humanBytes(16668618752, 'B'), '16.7 GB');
  assert.equal(M.humanBytes(0, 'B'), '0 B');
  assert.equal(M.humanBytes(7.63e6, 'B'), '7.63 MB');
});

test('measuredText / rateText: every cell value', () => {
  assert.equal(M.measuredText(null), '');
  assert.equal(M.measuredText({ available: false }), '');
  assert.equal(M.measuredText({ available: true, transports: [], delivered: false }), 'none');
  assert.equal(M.measuredText({ available: true, transports: [], delivered: true }), 'none (delivered)');
  assert.equal(M.measuredText({ available: true, transports: ['SHM'], packets: 0 }), 'SHM (idle)');
  assert.equal(M.measuredText({ available: true, transports: ['SHM', 'UDPv4'], packets: 47, bytes: 1310 }), 'SHM+UDPv4 47 pkt 1.31 kB');
  assert.equal(M.measuredText({ available: true, transports: ['SHM'], packets: 3 }), 'SHM 3 pkt');
  // with measured.locators the addresses replace the bare kinds
  const locators = [{ kind: 'UDPv4', address: '127.0.0.1', port: 7413 }, { kind: 'SHM', address: '', port: 8169 }];
  assert.equal(
    M.measuredText({ available: true, transports: ['UDPv4', 'SHM'], locators, packets: 47, bytes: 1310 }),
    'UDPv4 127.0.0.1:7413 + SHM:8169 47 pkt 1.31 kB');
  assert.equal(
    M.measuredText({ available: true, transports: ['SHM'], locators: [locators[1]], packets: 0 }),
    'SHM:8169 (idle)');
  assert.equal(M.rateText({ throughput_bytes_per_s: 1.31e6 }), '1.31 MB/s');
  assert.equal(M.rateText({ throughput_bytes_per_s: null }), '');
  assert.equal(M.rateText(undefined), '');
});

test('escapeHtml', () => {
  assert.equal(M.escapeHtml('<a href="x">&\'</a>'), '&lt;a href=&quot;x&quot;&gt;&amp;&#39;&lt;/a&gt;');
  assert.equal(M.escapeHtml(42), '42');
});

test('filterRegex: empty and invalid patterns filter nothing', () => {
  assert.equal(M.filterRegex(''), null);
  assert.equal(M.filterRegex('('), null);
  assert.ok(M.filterRegex('^/chatter$').test('/chatter'));
});

test('buildModel: nodes, hosts and pairs from the sample document', () => {
  const m = M.buildModel(sample);
  assert.ok(m.nodes.size >= 2, 'nodes');
  assert.ok(m.hosts.length >= 1, 'hosts');
  const total = sample.topics.reduce((a, t) => a + t.pairs.length, 0);
  assert.equal(m.pairs.length, total);
  for (const vp of m.pairs) {
    assert.ok(vp.writer && vp.reader, 'endpoints resolved by guid');
    assert.equal(vp.writer.guid, vp.pair.writer_guid);
    assert.ok(m.nodes.has(vp.writerNode) && m.nodes.has(vp.readerNode));
  }
  // every node belongs to exactly one host column, hosts sorted with "local" first
  const inHosts = new Set(m.hosts.flatMap(h => h.nodes.map(n => n.id)));
  assert.deepEqual([...inHosts].sort(), [...m.nodes.keys()].sort());
  if (m.hosts.some(h => h.label === 'local')) assert.equal(m.hosts[0].label, 'local');
  // publishers/subscriptions sorted by topic name
  for (const n of m.nodes.values()) {
    const names = n.pubs.map(p => p.topic.topic);
    assert.deepEqual(names, [...names].sort((a, b) => a.localeCompare(b)));
  }
});

test('buildModel: service endpoints without a node name fall back to the participant', () => {
  const doc = {
    topics: [
      { dds_topic: 'rt/chatter', topic: '/chatter', unmatched_reasons: [], pairs: [{ writer_guid: 'W', reader_guid: 'R', transport: 'SHM', confidence: 'certain', reasons: [], warnings: [] }],
        writers: [{ guid: 'W', node: '/talker', participant_guid_prefix: 'P1', host: 'local' }],
        readers: [{ guid: 'R', node: '/listener', participant_guid_prefix: 'P2', host: 'local' }] },
      { dds_topic: 'rq/add', topic: '/add', unmatched_reasons: ['no-matching-reader'], pairs: [],
        writers: [{ guid: 'S', node: '', participant_guid_prefix: 'P1', host: 'local' }, { guid: 'X', node: '', participant_guid_prefix: 'P9', host: 'host:1' }],
        readers: [] },
    ],
  };
  const m = M.buildModel(doc);
  assert.ok(m.nodes.has('/talker') && m.nodes.has('participant P9'));
  assert.equal(m.nodes.get('/talker').unmatched.length, 1, 'service writer attributed to the talker node');
  assert.equal(m.nodes.get('participant P9').host, 'host:1');
  assert.equal(m.hosts.map(h => h.label).join(','), 'local,host:1');
});

test('visiblePairs: internal topics, transports, topic and node regexes', () => {
  const m = M.buildModel(sample);
  const f = allFilter();
  const all = M.visiblePairs(m, f);
  assert.ok(all.every(vp => !M.INTERNAL_TOPICS.has(vp.topic.topic)));
  f.hideInternal = false;
  assert.ok(M.visiblePairs(m, f).length >= all.length);
  f.hideInternal = true;
  f.transports = new Set(['NONE']);
  assert.equal(M.visiblePairs(m, f).length, all.filter(vp => vp.pair.transport === 'NONE').length);
  f.transports = new Set(M.TRANSPORTS);
  f.topic = '^/chatter$';
  const chatter = M.visiblePairs(m, f);
  assert.ok(chatter.length >= 1 && chatter.every(vp => vp.topic.topic === '/chatter'));
  f.topic = '(';   // invalid: filters nothing
  assert.equal(M.visiblePairs(m, f).length, all.length);
  f.topic = '';
  f.node = 'listener';
  const byNode = M.visiblePairs(m, f);
  assert.ok(byNode.length >= 1);
  assert.ok(byNode.every(vp => /listener/.test(vp.writerNode) || /listener/.test(vp.readerNode)));
  f.node = 'no-such-node';
  assert.equal(M.visiblePairs(m, f).length, 0);
});

test('visibleNodesModel: matching nodes stay even without pairs, partners are kept', () => {
  const m = M.buildModel(sample);
  const none = M.visibleNodesModel(m, [], '');
  assert.equal(none.model, m);
  assert.equal(none.matched('/talker'), false);
  const f = allFilter();
  f.node = 'no-such-node';
  const empty = M.visibleNodesModel(m, M.visiblePairs(m, f), f.node);
  assert.equal(empty.model.nodes.size, 0);
  assert.equal(empty.model.hosts.length, 0);
  f.node = 'talker';
  const pairs = M.visiblePairs(m, f);
  const r = M.visibleNodesModel(m, pairs, f.node);
  for (const vp of pairs) assert.ok(r.model.nodes.has(vp.writerNode) && r.model.nodes.has(vp.readerNode));
  for (const id of r.model.nodes.keys()) {
    assert.ok(r.matched(id) || pairs.some(vp => vp.writerNode === id || vp.readerNode === id));
  }
  assert.ok([...r.model.nodes.keys()].some(id => r.matched(id)));
  for (const h of r.model.hosts) assert.ok(h.nodes.length > 0);
});

test('bundle: by writer node, reader node, transport and confidence; warn flag', () => {
  const mk = (w, r, transport, confidence, warnings = []) => ({ writerNode: w, readerNode: r, pair: { transport, confidence, warnings } });
  const edges = M.bundle([
    mk('/a', '/b', 'SHM', 'certain'),
    mk('/a', '/b', 'SHM', 'certain', ['measured-transport-mismatch']),
    mk('/a', '/b', 'SHM', 'likely'),
    mk('/a', '/c', 'UDPv4', 'certain'),
  ]);
  assert.equal(edges.length, 3);
  const ab = edges.find(e => e.target === '/b' && e.confidence === 'certain');
  assert.equal(ab.pairs.length, 2);
  assert.equal(ab.warn, true);
  assert.equal(edges.find(e => e.confidence === 'likely').warn, false);
  assert.equal(edges.find(e => e.target === '/c').transport, 'UDPv4');
});

test('shmText: summary line, stale count, visibility and warnings with descriptions', () => {
  assert.equal(M.shmText(null), '');
  assert.equal(M.shmText({ available: false }), '');
  const shm = { available: true, path: '/dev/shm', total_bytes: 16668618752, used_bytes: 396000000, fastdds_bytes: 63400000,
    segments: 114, stale_segments: 110, ports: 14, stale_ports: 7, datasharing_histories: 1, nodes_visible: false,
    warnings: ['shm-stale-files'] };
  const html = M.shmText(shm, { 'shm-stale-files': 'stale "files"' });
  assert.ok(html.startsWith('shared memory: /dev/shm 396 MB used of 16.7 GB · Fast DDS 63.4 MB in 114 segment(s), 14 port(s), 1 data-sharing history (117 stale) · nodes in another IPC namespace '));
  assert.ok(html.includes('<b>!shm-stale-files</b>'));
  assert.ok(html.includes('title="stale &quot;files&quot;"'), 'description escaped into the title');
  // the remedy joins the tooltip when the document carries one
  const withFix = M.shmText(shm, { 'shm-stale-files': 'stale files.' }, { 'shm-stale-files': "run 'fastdds shm clean'" });
  assert.ok(withFix.includes('title="stale files. Fix: run &#39;fastdds shm clean&#39;"'), withFix);
  shm.datasharing_histories = 2; shm.stale_segments = 0; shm.stale_ports = 0; shm.nodes_visible = true; shm.warnings = [];
  assert.equal(M.shmText(shm), 'shared memory: /dev/shm 396 MB used of 16.7 GB · Fast DDS 63.4 MB in 114 segment(s), 14 port(s), 2 data-sharing histories');
});

test('codeListHtml: description, remedy line only when known, warning prefix, escaping', () => {
  const desc = { 'reader-no-shm-locator': 'no SHM <locator>', 'same-host-guid': 'same host' };
  const rem = { 'reader-no-shm-locator': 'unset FASTDDS_BUILTIN_TRANSPORTS & co', 'same-host-guid': null };
  const html = M.codeListHtml(['same-host-guid', 'reader-no-shm-locator'], desc, rem, false);
  assert.equal(html,
    '<span class="code "><b>same-host-guid</b><span class="desc">same host</span></span>' +
    '<span class="code "><b>reader-no-shm-locator</b><span class="desc">no SHM &lt;locator&gt;</span>' +
    '<span class="fix">fix: unset FASTDDS_BUILTIN_TRANSPORTS &amp; co</span></span>');
  // warnings get the ! prefix; a document without the dictionaries renders bare codes
  assert.equal(M.codeListHtml(['shm-stale-files'], undefined, undefined, true),
    '<span class="code warn"><b>!shm-stale-files</b><span class="desc"></span></span>');
  assert.equal(M.codeListHtml([], desc, rem, false), '');
  // the shipped sample carries both dictionaries with the same keys
  assert.ok(sample.reason_code_remedies, 'sample.json predates reason_code_remedies: re-capture it');
  assert.deepEqual(Object.keys(sample.reason_code_remedies).sort(), Object.keys(sample.reason_code_descriptions).sort());
});

test('split IPC sample: the lost SHM pair is NONE with its warning, description and fix', () => {
  const doc = load('shm_split.json');
  const vp = M.buildModel(doc).pairs.find(p => p.pair.warnings.includes('shm-ipc-namespace-split') && p.topic.topic === '/chatter');
  assert.ok(vp, 'split pair on /chatter');
  assert.equal(vp.pair.transport, 'NONE');
  const html = M.codeListHtml(vp.pair.warnings, doc.reason_code_descriptions, doc.reason_code_remedies, true);
  assert.match(html, /<b>!shm-ipc-namespace-split<\/b><span class="desc">The writer and the reader have the same host id/);
  assert.match(html, /<span class="fix">fix: Put both nodes in one IPC namespace/);
});

test('humanSeconds / latencyText', () => {
  assert.equal(M.humanSeconds(0.00042), '420 µs');
  assert.equal(M.humanSeconds(0.0013), '1.30 ms');
  assert.equal(M.humanSeconds(2.5), '2.50 s');
  assert.equal(M.humanSeconds(15e-9), '15.0 ns');
  assert.equal(M.humanSeconds(-0.002), '-2.00 ms');
  assert.equal(M.latencyText({ latency_s: { mean: 0.00042, max: 0.0013 } }), '420 µs (max 1.30 ms)');
  assert.equal(M.latencyText({ latency_s: null }), '');
  assert.equal(M.latencyText(null), '');
});

test('lossText', () => {
  assert.equal(M.lossText(null), '');
  assert.equal(M.lossText({ reliability: null }), '');
  assert.equal(M.lossText({ reliability: { lost_packets: 0, resent_datas: 0 } }), '0');
  assert.equal(M.lossText({ reliability: { lost_packets: 3, resent_datas: 2 } }), '3 lost, 2 resent');
  assert.equal(M.lossText({ reliability: { lost_packets: 0, resent_datas: 5 } }), '5 resent');
});

// ---- comparing two documents (the port of transport_viz diff) ---------------------------

const before = () => load('diff_before.json');
const after = () => load('diff_after.json');
// what the C++ binary printed for `transport_viz diff --all --json diff_before.json diff_after.json`
const expected = () => load('diff.json');

test('pairKey / pairState: the C++ shapes', () => {
  const t = before().topics[0];
  const p = t.pairs[0];
  assert.deepEqual(M.pairKey(t, p), { topic: '/chatter', writer_guid: p.writer_guid, reader_guid: p.reader_guid, writer_node: '/talker', reader_node: '/listener' });
  assert.deepEqual(M.pairState(p), { transport: 'SHM', confidence: 'certain', measured: [], locator: { kind: 'SHM', address: '', port: 7415 }, measured_locators: [], warnings: [] });
  assert.equal(M.pairState({ transport: 'NONE', confidence: 'likely', locator: null, warnings: ['x'] }).locator, null);
});

test('sameState: counters ignored, ports ignored only when asked', () => {
  const a = M.pairState({ transport: 'SHM', confidence: 'certain', locator: { kind: 'SHM', address: '', port: 7415, multicast: false }, warnings: [],
    measured: { transports: ['SHM'], locators: [{ kind: 'SHM', address: '', port: 7415, packets: 5, bytes: 100 }] } });
  const b = M.pairState({ transport: 'SHM', confidence: 'certain', locator: { kind: 'SHM', address: '', port: 7417, multicast: false }, warnings: [],
    measured: { transports: ['SHM'], locators: [{ kind: 'SHM', address: '', port: 7417, packets: 900, bytes: 1e6 }] } });
  assert.equal(M.sameState(a, b, false), false);
  assert.equal(M.sameState(a, b, true), true);
  const c = { ...b, locator: { kind: 'UDPv4', address: '10.0.0.2', port: 7417 } };
  assert.equal(M.sameState(a, c, true), false);
  assert.equal(M.sameState(a, { ...a, warnings: ['no-traffic-observed'] }, true), false);
  assert.equal(M.sameState(a, { ...a, measured: [] }, true), false);
});

test('diffDocuments: node key reproduces the C++ output on the fixtures exactly', () => {
  assert.deepEqual(M.diffDocuments(before(), after(), 'node'), expected().changes);
});

test('diffDocuments: GUID key sees every restarted pair, the raw DDS pair survives', () => {
  const c = M.diffDocuments(before(), after(), 'guid');
  assert.equal(c.key, 'guid');
  assert.equal(c.added_pairs.length, 5);
  assert.equal(c.removed_pairs.length, 5);
  assert.equal(c.changed_pairs.length, 0);
  assert.ok(!c.added_pairs.some(k => k.topic === 'raw_topic') && !c.removed_pairs.some(k => k.topic === 'raw_topic'));
  // sorted like the C++ std::map: by topic, then writer GUID, then reader GUID
  const ids = c.added_pairs.map(k => `${k.topic}|${k.writer_guid}|${k.reader_guid}`);
  assert.deepEqual(ids, [...ids].sort());
  // a document compared with itself: nothing, under both keys
  for (const key of ['node', 'guid']) {
    const same = M.diffDocuments(before(), before(), key);
    assert.deepEqual([same.added_pairs, same.removed_pairs, same.changed_pairs], [[], [], []]);
    assert.deepEqual(same.before, { observed_at: '2026-09-13T09:00:00Z', domain: 0 });
  }
});

test('changeText: only the fields that differ', () => {
  const c = expected().changes.changed_pairs[0];
  assert.equal(M.changeText(c.from, c.to), 'transport SHM → UDPv4, locator SHM:7415 → UDPv4 127.0.0.1:7415');
  assert.equal(M.changeText(c.to, c.to), '');
  const a = { transport: 'SHM', confidence: 'certain', measured: [], locator: null, measured_locators: [], warnings: [] };
  const b = { ...a, confidence: 'likely', measured: ['SHM'], measured_locators: [{ kind: 'SHM', address: '', port: 8169 }], warnings: ['x'] };
  assert.equal(M.changeText(a, b), 'confidence certain → likely, measured [] → [SHM], measured locators [] → [SHM:8169], warnings [] → [x]');
});

test('changesSummary: the CLI wording', () => {
  assert.equal(M.changesSummary(null), '');
  assert.equal(M.changesSummary({ added_pairs: [], removed_pairs: [], changed_pairs: [] }), 'none');
  assert.equal(M.changesSummary(expected().changes), '+2 pairs  -2 pairs  ~1 changed');
  assert.equal(M.changesSummary({ added_pairs: [1], removed_pairs: [], changed_pairs: [] }), '+1 pair');
  assert.equal(M.changesSummary({ added_pairs: [], removed_pairs: [1], changed_pairs: [1, 2] }), '-1 pair  ~2 changed');
});

test('decorations: marks keyed by the after pair, ghosts with the before pair when known', () => {
  const doc = expected();
  const withBefore = M.decorations(doc.changes, before());
  const m = M.buildModel(doc);
  const marked = M.markedPairs(m.pairs, withBefore.marks);
  assert.deepEqual(marked.map(vp => [vp.topic.topic, vp.pair.reader_node, withBefore.marks.get(M.keyId(M.pairKey(vp.topic, vp.pair))).mark]).sort(),
    [['/chatter', '/listener', '~'], ['/chatter', '/listener2', '+'], ['/new_topic', '/sink2', '+']]);
  const changed = withBefore.marks.get(M.keyId(M.pairKey(doc.topics[0], doc.topics[0].pairs[0])));
  assert.equal(changed.from.transport, 'SHM');
  assert.equal(withBefore.ghosts.length, 2);
  const old = withBefore.ghosts.find(g => g.key.topic === '/old_topic');
  assert.equal(old.pair.transport, 'UDPv4');
  assert.equal(old.topic.type, 'std_msgs/msg/String');
  assert.ok(old.id.startsWith('ghost|/old_topic|'));
  // without the before document (a CLI diff file, a live frame) the ghosts keep their key only
  const bare = M.decorations(doc.changes, null);
  assert.equal(bare.ghosts.length, 2);
  assert.equal(bare.ghosts[0].pair, null);
  assert.equal(bare.ghosts[0].key.reader_node, '/listener_udp');
  assert.deepEqual(M.decorations(null).ghosts, []);
});

test('holdChanges: marks and ghosts stay three frames, a returning pair drops its ghost', () => {
  const doc = expected();
  const empty = { key: 'guid', added_pairs: [], removed_pairs: [], changed_pairs: [] };
  let h = M.holdChanges(null, doc.changes, null, 3);
  assert.equal(h.marks.size, 3);
  assert.equal(h.ghosts.size, 2);
  h = M.holdChanges(h, empty, null, 3);
  h = M.holdChanges(h, empty, null, 3);
  assert.equal(h.marks.size, 3, 'still shown on the third frame');
  assert.equal(M.heldDecorations(h).ghosts.length, 2);
  h = M.holdChanges(h, empty, null, 3);
  assert.equal(h.marks.size, 0, 'gone on the fourth');
  assert.equal(h.ghosts.size, 0);
  // the removed pair comes back: added mark, no ghost
  const removed = doc.changes.removed_pairs[0];
  h = M.holdChanges(null, { ...empty, removed_pairs: [removed] }, null, 3);
  assert.equal(h.ghosts.size, 1);
  h = M.holdChanges(h, { ...empty, added_pairs: [removed] }, null, 3);
  assert.equal(h.ghosts.size, 0);
  assert.equal(h.marks.get(M.keyId(removed)).mark, '+');
  assert.deepEqual(M.heldDecorations(null).ghosts, []);
});

test('pruneNodes: only nodes touched by the visible pairs or ghosts', () => {
  const m = M.buildModel(expected());
  const pairs = m.pairs.filter(vp => vp.topic.topic === '/new_topic');
  const pruned = M.pruneNodes(m, pairs, ['/talker']);
  assert.deepEqual([...pruned.nodes.keys()].sort(), ['/fresh', '/sink2', '/talker']);
  assert.ok(pruned.hosts.every(h => h.nodes.length));
  assert.equal(M.pruneNodes(m, [], []).nodes.size, 0);
});
