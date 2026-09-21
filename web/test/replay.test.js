// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Unit tests of web/replay.js, the recording model of #82 (run: node --test web/test).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const R = require('../replay.js');
const M = require('../model.js');

const enc = s => new TextEncoder().encode(s);
const dec = b => new TextDecoder().decode(b);

/** A one-topic document: a talker -> listener pair on /chatter with the given measurements. */
function frame(i, { transport = 'SHM', writer = 'w1', hz = 10, mean = 0.001, samples = 10 * (i + 1), lost = 0, changes = null } = {}) {
  const ep = (guid, node) => ({ guid, node, host: 'h', participant_guid_prefix: guid.split('|')[0], unicast_locators: [], multicast_locators: [], qos: {} });
  const doc = {
    schema_version: 1, domain: 0, observed_at: `2026-09-22T00:00:${String(i * 2).padStart(2, '0')}Z`,
    topics: [{
      topic: '/chatter', dds_topic: 'rt/chatter', type: 'std_msgs/msg/String', unmatched_reasons: [],
      writers: [ep(`${writer}|1`, '/talker')], readers: [ep('r1|1', '/listener')],
      pairs: [{
        writer_guid: `${writer}|1`, reader_guid: 'r1|1', writer_node: '/talker', reader_node: '/listener', transport,
        confidence: 'certain', reasons: [], warnings: [],
        measured: { delivered_per_s: hz, latency_s: { mean, samples }, reliability: { lost_packets: lost } },
      }],
    }],
  };
  if (changes) doc.changes = changes;
  return doc;
}

function record(docs, key) {
  const rec = R.createRecording(key);
  let at = 0;
  for (const d of docs) { const line = JSON.stringify(d); R.addFrame(rec, d, at, at + line.length); at += line.length + 1; }
  return rec;
}

test('lineSplitter: lines across chunk borders keep their file offsets', () => {
  const text = '{"a":1}\n{"bb":22}\nnot json\n{"c":3}';
  const bytes = enc(text);
  for (const size of [1, 3, 7, 8, 100]) {
    const split = R.lineSplitter();
    const lines = [];
    for (let at = 0; at < bytes.length; at += size) lines.push(...split.push(bytes.subarray(at, at + size), at));
    lines.push(...split.finish());
    assert.deepEqual(lines.map(l => dec(l.bytes)), ['{"a":1}', '{"bb":22}', 'not json', '{"c":3}'], `chunk ${size}`);
    for (const l of lines) assert.equal(text.slice(l.start, l.end), dec(l.bytes), `chunk ${size}`);
  }
});

test('lineSplitter: a trailing newline leaves nothing for finish', () => {
  const split = R.lineSplitter();
  assert.equal(split.push(enc('{"a":1}\n'), 0).length, 1);
  assert.deepEqual(split.finish(), []);
});

test('parseFrame: only schema_version 1 documents are frames', () => {
  assert.equal(R.parseFrame('[ros2run]: Process exited with failure 1'), null);
  assert.equal(R.parseFrame('{"schema_version":1,"topics":[]'), null);   // cut off mid-line
  assert.equal(R.parseFrame('{"schema_version":2,"topics":[]}'), null);
  assert.equal(R.parseFrame('   '), null);
  assert.deepEqual(R.parseFrame(' {"schema_version":1,"topics":[]}\r'), { schema_version: 1, topics: [] });
});

test('addFrame: byte ranges, times, change flags and a series per pair', () => {
  const changes = { added_pairs: [], removed_pairs: [], changed_pairs: [{}] };
  const rec = record([frame(0), frame(1, { transport: 'UDPv4', changes }), frame(2)]);
  assert.equal(rec.frames, 3);
  assert.deepEqual(rec.changed, [false, true, false]);
  assert.equal(rec.observedAt[1], '2026-09-22T00:00:02Z');
  assert.equal(rec.series.size, 1);
  const s = [...rec.series.values()][0];
  assert.deepEqual(R.transportRuns(s, 3).map(r => [r.from, r.to, r.transport]), [[0, 1, 'SHM'], [1, 2, 'UDPv4'], [2, 3, 'SHM']]);
  assert.deepEqual([...s.hz.subarray(0, 3)], [10, 10, 10]);
});

test('addFrame: with the node key a restarted writer continues the series, with guid it does not', () => {
  const docs = [frame(0), frame(1, { writer: 'w2' })];
  assert.equal(record(docs, 'node').series.size, 1);
  assert.equal(record(docs, 'guid').series.size, 2);
});

test('addFrame: a pair gone for a while comes back in the same series', () => {
  const rec = R.createRecording();
  const empty = i => ({ ...frame(i), topics: [] });
  R.addFrame(rec, frame(0), 0, 1);
  for (let i = 1; i < 40; ++i) R.addFrame(rec, empty(i), 0, 1);
  R.addFrame(rec, frame(40, { transport: 'UDPv4' }), 0, 1);
  const s = [...rec.series.values()][0];
  const runs = R.transportRuns(s, rec.frames);
  assert.deepEqual(runs.map(r => [r.from, r.to, r.transport]), [[0, 1, 'SHM'], [1, 40, null], [40, 41, 'UDPv4']]);
  assert.ok(Number.isNaN(s.hz[20]));
  assert.equal(s.hz[40], 10);
});

test('frameLatency: the mean of each interval from the cumulative mean', () => {
  // 10 samples at 1 ms, then 10 more at 3 ms (cumulative mean 2 ms), then none
  const rec = record([frame(0, { mean: 0.001, samples: 10 }), frame(1, { mean: 0.002, samples: 20 }), frame(2, { mean: 0.002, samples: 20 })]);
  const lat = R.frameLatency([...rec.series.values()][0], 3);
  assert.ok(Math.abs(lat[0] - 0.001) < 1e-12);
  assert.ok(Math.abs(lat[1] - 0.003) < 1e-12);
  assert.ok(Number.isNaN(lat[2]));
});

test('frameLatency / frameLoss: a restart behind the node key starts over', () => {
  const rec = record([
    frame(0, { mean: 0.001, samples: 100, lost: 5 }),
    frame(1, { writer: 'w2', mean: 0.004, samples: 150, lost: 7 }),   // new GUIDs, counters from 0
    frame(2, { writer: 'w2', mean: 0.004, samples: 160, lost: 9 }),
  ], 'node');
  const s = [...rec.series.values()][0];
  const lat = R.frameLatency(s, 3);
  assert.equal(lat[1], 0.004);
  assert.ok(Math.abs(lat[2] - 0.004) < 1e-12);
  assert.deepEqual([...R.frameLoss(s, 3)], [5, 7, 2]);
});

test('frameLoss: per-interval differences, the first frame its own count', () => {
  const rec = record([frame(0, { lost: 3 }), frame(1, { lost: 3 }), frame(2, { lost: 8 })]);
  assert.deepEqual([...R.frameLoss([...rec.series.values()][0], 3)], [3, 0, 5]);
});

test('hasValues: a recording without --stats has only the transport strip', () => {
  const d = frame(0);
  delete d.topics[0].pairs[0].measured;
  const s = [...record([d]).series.values()][0];
  assert.equal(R.hasValues(s.hz, 1), false);
  assert.equal(R.hasValues(R.frameLatency(s, 1), 1), false);
  assert.equal(R.transportRuns(s, 1)[0].transport, 'SHM');
});

test('identsOf: maps a frame pair to its series', () => {
  const d = frame(0);
  const rec = record([d]);
  const ids = R.identsOf(d, 'node');
  const real = M.keyId(M.pairKey(d.topics[0], d.topics[0].pairs[0]));
  assert.ok(rec.series.has(ids.get(real)));
});

test('nextChange: forward and back, -1 at either end', () => {
  const rec = record([0, 1, 2, 3, 4].map(i => frame(i, { changes: i === 1 || i === 3 ? { added_pairs: [{}], removed_pairs: [], changed_pairs: [] } : null })));
  assert.equal(R.nextChange(rec, 0, 1), 1);
  assert.equal(R.nextChange(rec, 1, 1), 3);
  assert.equal(R.nextChange(rec, 3, 1), -1);
  assert.equal(R.nextChange(rec, 3, -1), 1);
  assert.equal(R.nextChange(rec, 1, -1), -1);
});

test('frameX: by time when it parses and goes forward, else by index', () => {
  const rec = record([frame(0), frame(1), frame(4)]);   // 0 s, 2 s, 8 s
  assert.deepEqual([...R.frameX(rec)], [0, 0.25, 1]);
  rec.observedAt[1] = 'frame-1';
  assert.deepEqual([...R.frameX(rec)], [0, 0.5, 1]);
  assert.equal(R.nearestFrame(R.frameX(record([frame(0), frame(1), frame(4)])), 0.3), 1);
});

test('the shipped recording replays: several frames, the flip is in the series', () => {
  const file = path.join(__dirname, '..', 'sample', 'recording.jsonl');
  if (!fs.existsSync(file)) return;   // captured by scripts/integration_test.sh record_flip
  const bytes = fs.readFileSync(file);
  const split = R.lineSplitter();
  const rec = R.createRecording('node');
  for (const l of [...split.push(new Uint8Array(bytes), 0), ...split.finish()]) {
    const doc = R.parseFrame(dec(l.bytes));
    if (doc) R.addFrame(rec, doc, l.start, l.end);
  }
  assert.ok(rec.frames >= 10, `${rec.frames} frames`);
  assert.ok(rec.changed.some(Boolean), 'no frame with changes');
  const flips = [...rec.series.entries()].filter(([id]) => id.startsWith('/chatter|'))
    .map(([, s]) => R.transportRuns(s, rec.frames).map(r => r.transport).filter(Boolean));
  assert.ok(flips.some(t => t.includes('SHM') && t.includes('UDPv4')), JSON.stringify(flips));
});
