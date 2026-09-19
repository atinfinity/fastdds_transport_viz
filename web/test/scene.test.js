// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Unit tests of web/scene.js (run: node --test web/test).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const M = require('../model.js');
const S = require('../scene.js');
const sample = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'sample', 'sample.json'), 'utf8'));
M.normalizeDocument(sample);
const model = M.buildModel(sample);

const allFilter = () => ({ topic: '', node: '', transports: new Set(M.TRANSPORTS), hideInternal: true });
const noDeco = () => ({ marks: new Map(), ghosts: [] });
const box = (x, y) => ({ x, y, w: 190, h: 46 });

test('visibleScene: no filter draws every visible pair and every node', () => {
  const scene = S.visibleScene(model, allFilter(), false, noDeco());
  assert.equal(scene.pairs.length, M.visiblePairs(model, allFilter()).length);
  assert.equal(scene.model.nodes.size, model.nodes.size);
  assert.deepEqual([...scene.marks], []);
  assert.deepEqual(scene.ghosts, []);
});

test('visibleScene: a node filter keeps the matching nodes and their partners only', () => {
  const f = allFilter();
  f.node = '^/listener$';
  const scene = S.visibleScene(model, f, false, noDeco());
  assert.ok(scene.matched('/listener'));
  assert.ok(scene.model.nodes.has('/listener'));
  assert.ok(scene.model.nodes.has('/talker'));
  assert.ok(!scene.model.nodes.has('/bounded_pub'));
  assert.ok(scene.pairs.every(vp => vp.writerNode === '/listener' || vp.readerNode === '/listener'));
});

test('visibleScene: changes only keeps the marked pairs and prunes the nodes to them', () => {
  const full = S.visibleScene(model, allFilter(), false, noDeco());
  const vp = full.pairs.find(p => p.topic.topic === '/chatter');
  const marks = new Map([[M.keyId(M.pairKey(vp.topic, vp.pair)), { mark: '+' }]]);
  const scene = S.visibleScene(model, allFilter(), true, { marks, ghosts: [] });
  assert.equal(scene.pairs.length, 1);
  assert.equal(scene.pairs[0].topic.topic, '/chatter');
  assert.deepEqual([...scene.model.nodes.keys()].sort(), [vp.readerNode, vp.writerNode].sort());
});

test('visibleGhosts: topic, node and transport filters apply to removed pairs', () => {
  const ghosts = [
    { id: 'g1', key: { topic: '/chatter', writer_node: '/talker', reader_node: '/listener' }, pair: { transport: 'SHM' } },
    { id: 'g2', key: { topic: '/rosout', writer_node: '/talker', reader_node: '/listener' }, pair: null },
    { id: 'g3', key: { topic: '/other', writer_node: '/a', reader_node: '/b' }, pair: { transport: 'UDPv4' } },
  ];
  assert.deepEqual(S.visibleGhosts(ghosts, allFilter()).map(g => g.id), ['g1', 'g3']);   // /rosout is internal
  const f = allFilter();
  f.node = 'listener';
  assert.deepEqual(S.visibleGhosts(ghosts, f).map(g => g.id), ['g1']);
  f.node = '';
  f.transports = new Set(['UDPv4']);
  assert.deepEqual(S.visibleGhosts(ghosts, f).map(g => g.id), ['g3']);
});

test('sceneEdges: bundles carry the strongest mark, ghosts become dashed edges, parallel edges get k', () => {
  const scene = S.visibleScene(model, allFilter(), false, noDeco());
  const vp = scene.pairs.find(p => p.topic.topic === '/chatter');
  scene.marks = new Map([[M.keyId(M.pairKey(vp.topic, vp.pair)), { mark: '~' }]]);
  scene.ghosts = [{ id: 'ghost', key: { topic: '/gone', writer_node: vp.writerNode, reader_node: vp.readerNode }, pair: null },
    { id: 'orphan', key: { topic: '/gone', writer_node: '/nowhere', reader_node: vp.readerNode }, pair: null }];
  const edges = S.sceneEdges(scene);
  const marked = edges.find(e => e.pairs.includes(vp));
  assert.equal(marked.mark, '~');
  const ghost = edges.find(e => e.id === 'ghost');
  assert.equal(ghost.mark, '-');
  assert.equal(ghost.transport, 'NONE');
  assert.ok(!edges.some(e => e.id === 'orphan'));
  // ghost and the bundle share source/target: they are spread apart
  assert.notEqual(marked.k, ghost.k);
  assert.equal(marked.k + ghost.k + edges.filter(e => e.source === marked.source && e.target === marked.target && e !== marked && e !== ghost).reduce((a, e) => a + e.k, 0), 0);
  assert.ok(edges.every(e => typeof e.k === 'number'));
});

test('edgeLabel: topic and transport, count for bundles, mark in front', () => {
  const topic = { topic: '/chatter' };
  assert.equal(S.edgeLabel({ pairs: [{ topic }], transport: 'SHM', confidence: 'certain', mark: ' ' }), '/chatter · SHM');
  assert.equal(S.edgeLabel({ pairs: [{ topic }, { topic }], transport: 'UDPv4', confidence: 'likely', mark: '+' }), '+ 2 topics · UDPv4?');
  assert.equal(S.edgeLabel({ ghost: { key: { topic: '/gone' } }, pairs: [], transport: 'NONE', mark: '-' }), '- /gone · removed');
});

test('layout: one column per host, nodes in rows of maxRows, box per host', () => {
  const { pos, hostBoxes, width, height } = S.layout(model);
  assert.equal(hostBoxes.length, model.hosts.length);
  assert.equal(pos.size, model.nodes.size);
  for (const n of model.hosts[0].nodes.slice(0, S.L.maxRows)) assert.equal(pos.get(n.id).x, hostBoxes[0].x + S.L.hostPad);
  assert.ok(width > hostBoxes[hostBoxes.length - 1].x + hostBoxes[hostBoxes.length - 1].w);
  assert.ok(height > S.L.top);
});

test('edgeCurve / edgePathD: the three shapes and the spread of parallel edges', () => {
  const a = box(30, 100), b = box(400, 300);
  const c = S.edgeCurve(a, b, 0);
  assert.deepEqual(c[0], { x: 220, y: 123 });   // leaves a's right edge at its middle
  assert.deepEqual(c[3], { x: 400, y: 323 });   // enters b's left edge
  assert.equal(S.edgePathD(c), 'M220,123 C310,123 310,323 400,323');
  assert.equal(S.edgePath(a, b, 0), S.edgePathD(c));
  assert.equal(S.edgeCurve(a, b, 1)[0].y, 123 + 14);
  const loop = S.edgeCurve(a, a, 0);
  assert.equal(loop[0].x, 220);
  assert.equal(loop[3].x, 220);
  assert.ok(loop[1].x > 220 && loop[2].x > 220);
  const same = S.edgeCurve(a, box(30, 300), 0);
  assert.equal(same[0].x, 220);
  assert.equal(same[3].x, 220);
  assert.ok(same[1].x > 220);
});

test('edgeMidpoint: the half-length point, within a pixel of a fine arc-length walk', () => {
  const fine = (c) => {
    const n = 2000;
    let prev = S.bezierAt(c, 0), total = 0;
    const seg = [];
    for (let i = 1; i <= n; i++) { const p = S.bezierAt(c, i / n); const d = Math.hypot(p.x - prev.x, p.y - prev.y); seg.push([d, p]); total += d; prev = p; }
    let r = total / 2;
    for (const [d, p] of seg) { if (r <= d) return p; r -= d; }
    return prev;
  };
  const cases = [S.edgeCurve(box(30, 100), box(400, 300), 0), S.edgeCurve(box(30, 100), box(400, 300), 2.5),
    S.edgeCurve(box(30, 100), box(30, 100), 0), S.edgeCurve(box(30, 100), box(30, 400), -1), S.edgeCurve(box(600, 100), box(30, 120), 0)];
  for (const c of cases) {
    const m = S.edgeMidpoint(c), ref = fine(c);
    assert.ok(Math.hypot(m.x - ref.x, m.y - ref.y) < 1, `${JSON.stringify(m)} vs ${JSON.stringify(ref)}`);
  }
  // a symmetric S-curve is centred exactly
  const m = S.edgeMidpoint(S.edgeCurve(box(30, 100), box(400, 300), 0));
  assert.ok(Math.abs(m.x - 310) < 1e-6 && Math.abs(m.y - 223) < 1e-6);
});
