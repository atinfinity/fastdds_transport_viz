// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Unit tests of web/model.js (run: node --test web/test).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const M = require('../model.js');
const sample = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'sample', 'sample.json'), 'utf8'));

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
  const html = M.shmText(shm, { 'shm-stale-files': 'run "fastdds shm clean"' });
  assert.ok(html.startsWith('shared memory: /dev/shm 396 MB used of 16.7 GB · Fast DDS 63.4 MB in 114 segment(s), 14 port(s), 1 data-sharing history (117 stale) · nodes in another IPC namespace '));
  assert.ok(html.includes('<b>!shm-stale-files</b>'));
  assert.ok(html.includes('title="run &quot;fastdds shm clean&quot;"'), 'description escaped into the title');
  shm.datasharing_histories = 2; shm.stale_segments = 0; shm.stale_ports = 0; shm.nodes_visible = true; shm.warnings = [];
  assert.equal(M.shmText(shm), 'shared memory: /dev/shm 396 MB used of 16.7 GB · Fast DDS 63.4 MB in 114 segment(s), 14 port(s), 2 data-sharing histories');
});
