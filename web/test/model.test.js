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

test('measuredText: every cell value', () => {
  assert.equal(M.measuredText(null), '');
  assert.equal(M.measuredText({ available: false }), '');
  assert.equal(M.measuredText({ available: true, transports: [], delivered: false }), 'none');
  assert.equal(M.measuredText({ available: true, transports: [], delivered: true }), 'none (delivered)');
  assert.equal(M.measuredText({ available: true, transports: ['SHM'], packets: 0 }), 'SHM (idle)');
  // a delivery proof without a measured packet is not idle (#149)
  assert.equal(
    M.measuredText({ available: true, transports: ['SHM'], packets: 0, delivered: true }),
    'SHM (unmeasured, delivered)');
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

test('visiblePairs: folded native-buffer companion topics count as internal', () => {
  const doc = load('sample.json');
  const chatter = doc.topics.find(t => t.topic === '/chatter');
  const companion = JSON.parse(JSON.stringify(chatter));
  companion.topic = '/chatter/_buf_cpu';
  companion.dds_topic = 'rt/chatter/_buf_cpu';
  for (const ep of [...companion.writers, ...companion.readers]) ep.buffer_parent_guid = ep.guid;
  doc.topics.push(companion);
  assert.ok(M.isFoldedBufferCompanion(companion));
  assert.ok(!M.isFoldedBufferCompanion(chatter));
  assert.ok(!M.isFoldedBufferCompanion({ topic: '/x/_buf_cpu', writers: [], readers: [] }));
  const f = allFilter();
  const count = () => M.visiblePairs(M.buildModel(doc), f).filter(vp => vp.topic.topic === '/chatter/_buf_cpu').length;
  assert.equal(count(), 0);
  f.hideInternal = false;
  assert.equal(count(), companion.pairs.length);
  delete companion.readers[0].buffer_parent_guid;   // an unmatched companion stays visible
  f.hideInternal = true;
  assert.equal(count(), companion.pairs.length);
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

test('participantShmText: the endpoint panel line from the `participants` entry (#125)', () => {
  assert.equal(M.participantShmText(null, 'P1'), '');
  assert.equal(M.participantShmText({ topics: [] }, 'P1'), '', 'a document written before #125');
  const doc = { participants: [
    { guid_prefix: 'P1', own: false, shm_visibility: 'visible',
      shm_ports: [{ port: 7417, lock: 'held', announced_by: 1, proof: true }, { port: 7001, lock: 'held', announced_by: 2, proof: false }] },
    { guid_prefix: 'P2', own: false, shm_visibility: 'not-visible', shm_ports: [{ port: 7419, lock: 'absent', announced_by: 1, proof: true }] },
    { guid_prefix: 'P9', own: true, shm_visibility: 'unprobed', shm_ports: [] },
  ] };
  assert.equal(M.participantShmText(doc, 'P1'), 'visible · ports 7417 held, 7001 held (2 participants) (no proof)');
  assert.equal(M.participantShmText(doc, 'P2'), 'not-visible · ports 7419 absent');
  assert.equal(M.participantShmText(doc, 'P9'), "unprobed (the tool's own) · no SHM port");
  assert.equal(M.participantShmText(doc, 'P3'), '', 'a participant the document does not list');
  // the shipped split sample: the tool in a third IPC namespace, the nodes' ports absent
  const split = load('shm_split.json');
  const chatter = split.topics.find(t => t.topic === '/chatter');
  const text = M.participantShmText(split, chatter.writers[0].participant_guid_prefix);
  assert.match(text, /^not-visible · ports 7000 own \(3 participants\) \(no proof\), \d+ absent$/);
  assert.ok(split.participants.some(p => p.own && M.participantShmText(split, p.guid_prefix).startsWith("unprobed (the tool's own)")));
});

test('datasharingText: the endpoint panel row from the segment visibility (#163)', () => {
  assert.equal(M.datasharingText(null, true), '');
  assert.equal(M.datasharingText({ datasharing_history_bytes: null }, true), '', 'a document before #163');
  assert.equal(M.datasharingText({ datasharing_history_bytes: 4016 }, true), 'history 4.02 kB in /dev/shm', 'before #163: the size only');
  assert.equal(M.datasharingText({ datasharing_history_bytes: null, datasharing_segment_visibility: 'unprobed' }, true), '', 'no data-sharing on this endpoint');
  assert.equal(M.datasharingText({ datasharing_history_bytes: 4016, datasharing_segment_visibility: 'visible' }, true), 'history 4.02 kB in /dev/shm · segment visible');
  assert.equal(M.datasharingText({ datasharing_history_bytes: null, datasharing_segment_visibility: 'not-visible' }, true), 'history segment not-visible');
  assert.equal(M.datasharingText({ datasharing_history_bytes: null, datasharing_segment_visibility: 'visible' }, false), 'notification segment visible');
  assert.equal(M.datasharingText({ datasharing_history_bytes: null, datasharing_segment_visibility: 'not-visible' }, false), 'notification segment not-visible');
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
  // the readers' notification segments only when there are some
  shm.datasharing_notifications = 3;
  assert.equal(M.shmText(shm), 'shared memory: /dev/shm 396 MB used of 16.7 GB · Fast DDS 63.4 MB in 114 segment(s), 14 port(s), 2 data-sharing histories, 3 data-sharing notification(s)');
});

test('statsText: sample count, what the tool lost, and the document-level warning', () => {
  assert.equal(M.statsText(null), 'no statistics');
  assert.equal(M.statsText({ enabled: false }), 'no statistics');
  assert.equal(M.statsText({ enabled: true, samples: 12 }), 'statistics: 12 samples');
  // the burst from before the readers matched is not the tool falling behind: not shown
  assert.equal(M.statsText({ enabled: true, samples: 12, samples_lost_at_start: 900 }),
    'statistics: 12 samples');
  // lost and rejected are one number: what is missing
  const lost = { enabled: true, samples: 12, samples_lost: 40, samples_rejected: 2,
    warnings: ['stats-samples-lost'] };
  const html = M.statsText(lost, { 'stats-samples-lost': 'could not keep "up"' },
    { 'stats-samples-lost': 'enable statistics on fewer nodes' });
  assert.ok(html.startsWith('statistics: 12 samples, 42 lost '), html);
  assert.ok(html.includes('<b>!stats-samples-lost</b>'), html);
  assert.ok(html.includes('title="could not keep &quot;up&quot; Fix: enable statistics on fewer nodes"'), html);
  // #141: HISTORY_LATENCY is received best-effort by design, so its gaps are named apart
  // and stay out of the number that means a pair may be unmeasured
  const lat = { enabled: true, samples: 12, samples_lost: 1040, samples_lost_latency: 1000,
    samples_rejected: 2 };
  assert.equal(M.statsText(lat, {}, {}), 'statistics: 12 samples, 42 lost, 1000 latency lost');
  // #147: a loss that cost no measurement is a number without a marker
  const harmless = { enabled: true, samples: 12, samples_lost: 40, samples_rejected: 2,
    pairs_delivered_unmeasured: 0, warnings: [] };
  assert.equal(M.statsText(harmless, { 'stats-samples-lost': 'x' }, {}),
    'statistics: 12 samples, 42 lost');
  // a document written before #134 carries none of the keys
  assert.equal(M.statsText({ enabled: true, samples: 12 }, {}, {}), 'statistics: 12 samples');
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

test('normalizeDocument: the rmw unknown node name falls back to the participant', () => {
  const unknown = M.UNKNOWN_NODE_NAME;
  const doc = {
    topics: [
      { dds_topic: 'rt/chatter', topic: '/chatter', unmatched_reasons: [],
        pairs: [{ writer_guid: 'W', reader_guid: 'R', writer_node: unknown, reader_node: unknown, transport: 'NONE', confidence: 'likely', reasons: [], warnings: [] }],
        writers: [{ guid: 'W', node: unknown, participant_guid_prefix: 'P1', host: 'local' }],
        readers: [{ guid: 'R', node: unknown, participant_guid_prefix: 'P2', host: 'local' }] },
    ],
  };
  assert.equal(M.normalizeDocument(doc), doc);
  assert.equal(doc.topics[0].writers[0].node, '');
  assert.equal(doc.topics[0].pairs[0].reader_node, '');
  const m = M.buildModel(doc);
  assert.deepEqual([...m.nodes.keys()].sort(), ['participant P1', 'participant P2'], 'not merged into one node');
  assert.equal(M.normalizeDocument(null), null);
  // the shipped split sample (re-taken after #112): both nodes carry their real names
  const split = M.buildModel(M.normalizeDocument(load('shm_split.json')));
  assert.ok(![...split.nodes.keys()].includes(unknown));
  assert.ok(split.nodes.has('/talker'));
  assert.ok(split.nodes.has('/listener'));
});

test('rateText / rateTitle (#143)', () => {
  assert.equal(M.rateText({ delivered_per_s: 9.96 }), '10.0');
  assert.equal(M.rateText({ delivered_per_s: 120.4 }), '120');
  assert.equal(M.rateText({ delivered_per_s: 120.4, delivered_per_s_lower_bound: true }), '\u2265120');
  assert.equal(M.rateText({ delivered_per_s: null }), '');
  assert.equal(M.rateText({}), '');
  assert.equal(M.rateText(null), '');
  assert.equal(M.rateTitle({ delivered_per_s: 10, delivered_per_s_window_s: 5 }), 'delivered samples/s over 5.0 s');
  assert.match(M.rateTitle({ delivered_per_s: 10, delivered_per_s_lower_bound: true }), /^delivered samples\/s; at least/);
  assert.equal(M.rateTitle({ delivered_per_s: null }), '');
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
  // the reader's participant does not publish RTPS_LOST
  assert.equal(M.lossText({ reliability: { lost_packets: null, resent_datas: 0 } }), '- lost');
  assert.equal(M.lossText({ reliability: { lost_packets: null, resent_datas: 2 } }), '- lost, 2 resent');
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

test('topicLatencyText / topicLossText: the CLI topic row cells from topics[] (#144)', () => {
  assert.equal(M.topicLatencyText({ latency_s: 0.00042 }), '420 µs');
  assert.equal(M.topicLatencyText({ latency_s: null }), '');
  assert.equal(M.topicLossText({ lost_packets: 0, resent_datas: 0 }), '0');
  assert.equal(M.topicLossText({ lost_packets: 3, resent_datas: 2 }), '3 lost, 2 resent');
  assert.equal(M.topicLossText({ lost_packets: null, resent_datas: 0 }), '- lost');
  assert.equal(M.topicLossText({ lost_packets: null, resent_datas: null }), '');
});

test('groupPairsByTopic: aggregates come from topics[], never from the visible rows (#144)', () => {
  const model = M.buildModel(sample);
  const chatter = sample.topics.find(t => t.topic === '/chatter');
  // only the SHM pair of /chatter is visible, the header still counts every endpoint and transport
  const rows = M.visiblePairs(model, { ...allFilter(), topic: '^/chatter$', transports: new Set(['SHM']) });
  assert.equal(rows.length, 1);
  const groups = M.groupPairsByTopic(rows, [], sample.topics);
  assert.equal(groups.length, 1);
  const g = groups[0];
  assert.equal(g.key, chatter.dds_topic);
  assert.equal(g.name, '/chatter');
  assert.equal(g.aggregates, true);
  assert.deepEqual([g.pubs, g.subs], [1, 2]);
  assert.deepEqual([...g.transports].sort(), ['SHM', 'UDPv4']);
  assert.equal(g.latency, M.topicLatencyText(chatter));
  assert.equal(g.latencyValue, chatter.latency_s);
  assert.deepEqual([g.loss, g.lostValue], ['0', 0]);
  assert.equal(g.reasons, '');
  assert.deepEqual(g.pairs, rows);
  // a topic with no visible pair has no group
  assert.equal(M.groupPairsByTopic([], [], sample.topics).length, 0);
});

test('groupPairsByTopic: request and reply topics of a service keep separate headers (#144)', () => {
  const all = load('sample_all.json');
  const model = M.buildModel(all);
  const rows = model.pairs.filter(vp => vp.topic.topic === '/talker/get_parameters');
  // no pairs on the sample's services: build rows for both dds topics by hand
  const topics = all.topics.filter(t => t.topic === '/talker/get_parameters');
  assert.equal(topics.length, 2);
  const fake = topics.map((t, i) => ({ id: `x${i}`, topic: t }));
  const groups = M.groupPairsByTopic([...rows, ...fake], [], all.topics);
  assert.equal(groups.length, 2);
  assert.deepEqual(groups.map(g => g.key).sort(), topics.map(t => t.dds_topic).sort());
  assert.deepEqual(groups.map(g => g.reasons).sort(), ['no-matching-reader', 'no-matching-writer']);
});

test('groupPairsByTopic: ghosts nest under their topic, orphans get a header without aggregates (#144)', () => {
  const model = M.buildModel(sample);
  const rows = M.visiblePairs(model, allFilter());
  const chatter = sample.topics.find(t => t.topic === '/chatter');
  const ghostKnown = { id: 'ghost|a', ghost: true, topic: chatter };
  const ghostByName = { id: 'ghost|b', ghost: true, topic: { topic: '/chatter', type: '' } };
  const orphan = { id: 'ghost|c', ghost: true, topic: { topic: '/gone', type: '' } };
  const groups = M.groupPairsByTopic(rows, [ghostKnown, ghostByName, orphan], sample.topics);
  const g = groups.find(x => x.name === '/chatter');
  assert.deepEqual(g.ghosts.map(x => x.id), ['ghost|a', 'ghost|b']);
  assert.equal(g.pairs.length, chatter.pairs.length);
  const o = groups.find(x => x.name === '/gone');
  assert.equal(o.aggregates, false);
  assert.deepEqual([o.pubs, o.subs, o.transports, o.latency, o.loss, o.reasons], [null, null, [], '', '', '']);
  assert.deepEqual(o.ghosts.map(x => x.id), ['ghost|c']);
  assert.equal(o.pairs.length, 0);
});

test('compareCells: numbers numerically, missing values last both ways (#144)', () => {
  assert.deepEqual([9, 10, 2].sort((a, b) => M.compareCells(a, b)), [2, 9, 10]);
  assert.deepEqual(['9 ms', '10 ms'].sort((a, b) => M.compareCells(a, b)), ['10 ms', '9 ms']);   // strings stay lexicographic
  assert.deepEqual([null, 3, '', 1, undefined].sort((a, b) => M.compareCells(a, b)), [1, 3, null, '', undefined]);
  assert.deepEqual([null, 3, 1].sort((a, b) => M.compareCells(a, b, false)), [3, 1, null]);
  assert.deepEqual(['b', 'a'].sort((a, b) => M.compareCells(a, b)), ['a', 'b']);
});
