// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Browser-level tests of web/app.js (#81): the d3 graph, the filters, the panels, the
// table tab, the comparison views and drag & drop, in headless Chrome against the real
// index.html. What is drawn is compared with what model.js / scene.js say should be drawn
// (both unit-tested), so these tests assert the wiring and survive a regenerated sample.
// Run: node --test web/test  (skips when no Chrome is installed; see web/test/cdp.js).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const M = require('../model.js');
const S = require('../scene.js');
const cdp = require('./cdp.js');
const staticServer = require('./static_server.js');

const skip = cdp.browserSkip();
const opts = skip ? { skip } : {};

const load = name => {
  const doc = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'sample', name), 'utf8'));
  M.normalizeDocument(doc);
  return doc;
};
const sample = load('sample.json');

const filter = (over = {}) => ({ topic: '', node: '', transports: new Set(M.TRANSPORTS), hideInternal: true, ...over });
const scene = (doc, f) => S.visibleScene(M.buildModel(doc), f, false, { marks: new Map(), ghosts: [] });
/** What the graph should hold for `doc` under `f`, straight from the unit-tested model. */
const expected = (doc, f) => {
  const sc = scene(doc, f);
  return { edges: S.sceneEdges(sc).map(e => e.id).sort(), nodes: [...sc.model.nodes.keys()].sort() };
};

const EDGE_IDS = 'd3.selectAll("#graph g.edge").data().map(d => d.id).sort()';
const EDGE_MARKS = 'd3.selectAll("#graph g.edge").data().map(d => d.id + " " + d.mark).sort()';
const NODE_IDS = 'd3.selectAll("#graph g.node").data().map(d => d.n.id).sort()';
const TOPIC_NAMES = '[...document.querySelectorAll("#pairs-body tr.topic td.topic-name b")].map(b => b.textContent)';
const hidden = id => `document.getElementById(${JSON.stringify(id)}).hasAttribute("hidden")`;

/** Type into a filter box and flush the 100 ms debounce with Enter (app.js: flushFilter). */
const typeFilter = (page, id, value) => page.evaluate(`(() => {
  const input = document.getElementById(${JSON.stringify(id)});
  input.value = ${JSON.stringify(value)};
  input.dispatchEvent(new Event('input', {bubbles: true}));
  input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
  return true;
})()`);

const setChecked = (page, selector, checked) => page.evaluate(`(() => {
  const box = document.querySelector(${JSON.stringify(selector)});
  box.checked = ${checked ? 'true' : 'false'};
  box.dispatchEvent(new Event('change', {bubbles: true}));
  return true;
})()`);

/** The transport checkboxes carry no id: find the one labelled `name`. */
const setTransport = (page, name, checked) => page.evaluate(`(() => {
  const label = [...document.querySelectorAll('#filter-transports label')].find(l => l.textContent.trim() === ${JSON.stringify(name)});
  if (!label) throw new Error('no transport checkbox for ' + ${JSON.stringify(name)});
  const box = label.querySelector('input');
  box.checked = ${checked ? 'true' : 'false'};
  box.dispatchEvent(new Event('change', {bubbles: true}));
  return true;
})()`);

/** Click the arrow or the box bound to `id` (the datum d3 joined, not a DOM attribute). */
const clickDatum = (page, selector, expr, id) => page.evaluate(`(() => {
  const el = d3.selectAll(${JSON.stringify(selector)}).filter(d => ${expr} === ${JSON.stringify(id)}).node();
  if (!el) throw new Error('nothing bound to ' + ${JSON.stringify(id)});
  el.dispatchEvent(new MouseEvent('click', {bubbles: true}));
  return true;
})()`);
const clickEdge = (page, id) => clickDatum(page, '#graph g.edge', 'd.id', id);
const clickNode = (page, id) => clickDatum(page, '#graph g.node', 'd.n.id', id);

const clickHeader = (page, label) => page.evaluate(`(() => {
  const th = [...document.querySelectorAll('#pairs-head th')].find(t => t.textContent.startsWith(${JSON.stringify(label)}));
  if (!th) throw new Error('no column header ' + ${JSON.stringify(label)});
  th.dispatchEvent(new MouseEvent('click', {bubbles: true}));
  return true;
})()`);

let browser = null;
let server = null;
const url = query => `${server.origin}/index.html${query}`;

test.before(async () => {
  if (skip) return;
  server = await staticServer.start();
  browser = await cdp.launch();
});

test.after(async () => {
  if (browser) await browser.close();
  if (server) await server.close();
});

// ------------------------------------------------------------------ graph

test('graph: the sample renders every visible arrow, node and host box', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const want = expected(sample, filter());
  assert.ok(want.edges.length > 0 && want.nodes.length >= 5, 'the fixture must draw something');
  assert.deepEqual(await page.evaluate(EDGE_IDS), want.edges);
  assert.deepEqual(await page.evaluate(NODE_IDS), want.nodes);
  assert.ok(await page.count('#graph g.host rect') > 0, 'no host box drawn');
  assert.equal(await page.evaluate('d3.selectAll("#graph g.edge text").nodes().filter(t => !t.textContent).length'), 0,
    'every arrow carries a label');
  assert.match(await page.text('#meta'), /domain 0 · .* · 4 topics, 28 pairs/);
}));

test('graph: the embedded sample.js draws the same graph as sample.json', opts, () => browser.withPage(url(''), async (page) => {
  // no ?src: app.js shows window.TRANSPORT_VIZ_SAMPLE, the copy of sample.json that makes
  // file:// work. A stale copy would show up here and nowhere else.
  assert.deepEqual(await page.evaluate(EDGE_IDS), expected(sample, filter()).edges);
  assert.equal(await page.evaluate('document.title'), 'transport_viz viewer – sample/sample.json');
}));

// ------------------------------------------------------------------ filters

test('filters: the topic box keeps the matching pairs, an unparseable regex is marked', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  let before = await page.render();
  await typeFilter(page, 'filter-topic', '^/chatter$');
  await page.waitForRender(before, 'the topic filter to render');
  assert.deepEqual(await page.evaluate(EDGE_IDS), expected(sample, filter({ topic: '^/chatter$' })).edges);

  before = await page.render();
  await typeFilter(page, 'filter-topic', '(');
  await page.waitForRender(before, 'the invalid regex to render');
  assert.equal(await page.evaluate('document.getElementById("filter-topic").classList.contains("invalid")'), true);
  // an unparseable regex filters nothing (filterRegex returns null), it only marks the box
  assert.deepEqual(await page.evaluate(EDGE_IDS), expected(sample, filter()).edges);

  before = await page.render();
  await typeFilter(page, 'filter-topic', '');
  await page.waitForRender(before, 'the cleared filter to render');
  assert.equal(await page.evaluate('document.getElementById("filter-topic").classList.contains("invalid")'), false);
  assert.deepEqual(await page.evaluate(EDGE_IDS), expected(sample, filter()).edges);
}));

test('filters: the node box keeps the matching nodes and their partners', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const before = await page.render();
  await typeFilter(page, 'filter-node', '^/listener$');
  await page.waitForRender(before, 'the node filter to render');
  const want = expected(sample, filter({ node: '^/listener$' }));
  assert.deepEqual(await page.evaluate(NODE_IDS), want.nodes);
  assert.deepEqual(await page.evaluate(EDGE_IDS), want.edges);
  assert.ok(want.nodes.includes('/talker') && !want.nodes.includes('/bounded_pub'));
}));

test('filters: unchecking a transport drops its arrows', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const before = await page.render();
  await setTransport(page, 'SHM', false);
  await page.waitForRender(before, 'the transport filter to render');
  const want = expected(sample, filter({ transports: new Set(M.TRANSPORTS.filter(t => t !== 'SHM')) }));
  assert.deepEqual(await page.evaluate(EDGE_IDS), want.edges);
  assert.ok(want.edges.length < expected(sample, filter()).edges.length, 'the fixture must have an SHM pair');
}));

test('filters: showing the ROS internal topics grows the graph', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const before = await page.render();
  await setChecked(page, '#filter-internal', false);
  await page.waitForRender(before, 'the internal topics to render');
  const want = expected(sample, filter({ hideInternal: false }));
  assert.deepEqual(await page.evaluate(EDGE_IDS), want.edges);
  assert.ok(want.edges.length > expected(sample, filter()).edges.length);
}));

// ------------------------------------------------------------------ panels

test('panel: an arrow shows its pairs, the background clears the selection', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const sc = scene(sample, filter());
  const edge = S.sceneEdges(sc).find(e => e.id.startsWith('/talker→/listener|'));
  const before = await page.render();
  await clickEdge(page, edge.id);
  await page.waitForRender(before, 'the edge panel');
  assert.equal(await page.text('#panel h2'), `${edge.source} → ${edge.target}`);
  assert.equal(await page.count('#panel .pair'), edge.pairs.length);
  assert.equal(await page.text('#panel .badge'), edge.transport);
  assert.equal(await page.count('#graph g.edge.selected'), 1);

  const before2 = await page.render();
  await page.click('#graph');
  await page.waitForRender(before2, 'the cleared selection');
  assert.equal(await page.count('#panel .panel-empty'), 1);
  assert.equal(await page.count('#graph g.edge.selected'), 0);
}));

test('panel: a node shows its publishers, subscriptions and unmatched topics', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const node = M.buildModel(sample).nodes.get('/talker');
  const before = await page.render();
  await clickNode(page, '/talker');
  await page.waitForRender(before, 'the node panel');
  assert.equal(await page.text('#panel h2'), '/talker');
  const text = await page.text('#panel');
  assert.match(text, new RegExp(`Publishers \\(${node.pubs.length}\\)`));
  assert.match(text, new RegExp(`Subscriptions \\(${node.subs.length}\\)`));
  assert.match(text, new RegExp(`Unmatched topics \\(${node.unmatched.length}\\)`));
  assert.equal(await page.count('#graph g.node.selected'), 1);
}));

// ------------------------------------------------------------------ table

test('table: topic header rows, collapsing and sorting', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  const sc = scene(sample, filter());
  const groups = M.groupPairsByTopic(sc.pairs, [], sample.topics);
  const names = groups.map(g => g.name).sort();

  let before = await page.render();
  await page.click('.tab[data-view="table"]');
  await page.waitForRender(before, 'the table tab');
  assert.equal(await page.evaluate(hidden('table-view')), false);
  assert.equal(await page.evaluate(hidden('graph-view')), true);
  assert.equal(await page.evaluate(hidden('table-tools')), false);
  assert.deepEqual((await page.evaluate(TOPIC_NAMES)).slice().sort(), names);
  assert.equal(await page.count('#pairs-body tr:not(.topic)'), sc.pairs.length);

  // a click on a topic header folds its pairs away
  const first = groups.slice().sort((a, b) => a.name.localeCompare(b.name))[0];
  before = await page.render();
  await page.click('#pairs-body tr.topic');
  await page.waitForRender(before, 'the collapsed topic');
  assert.equal(await page.count('#pairs-body tr.topic.collapsed'), 1);
  assert.match(await page.text('#pairs-body tr.topic.collapsed'), new RegExp(`\\(${first.pairs.length} pairs?\\)`));
  assert.equal(await page.count('#pairs-body tr:not(.topic)'), sc.pairs.length - first.pairs.length);

  before = await page.render();
  await page.click('#collapse-all');
  await page.waitForRender(before, 'every topic collapsed');
  assert.equal(await page.count('#pairs-body tr:not(.topic)'), 0);
  assert.equal(await page.count('#pairs-body tr.topic'), groups.length);
  assert.equal(await page.text('#collapse-all'), 'Expand all');

  before = await page.render();
  await page.click('#collapse-all');
  await page.waitForRender(before, 'every topic expanded');
  assert.equal(await page.count('#pairs-body tr:not(.topic)'), sc.pairs.length);
  assert.equal(await page.text('#collapse-all'), 'Collapse all');

  // the Topic column starts ascending, so one click turns it around
  const ascending = await page.evaluate(TOPIC_NAMES);
  before = await page.render();
  await clickHeader(page, 'Topic');
  await page.waitForRender(before, 'the reversed sort');
  assert.deepEqual(await page.evaluate(TOPIC_NAMES), ascending.slice().reverse());
  assert.match(await page.evaluate('[...document.querySelectorAll("#pairs-head th")].map(t => t.textContent).join("|")'), /Topic ▼/);
}));

test('table: a pair row opens its card', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  let before = await page.render();
  await page.click('.tab[data-view="table"]');
  await page.waitForRender(before, 'the table tab');
  before = await page.render();
  await page.click('#pairs-body tr:not(.topic)');
  await page.waitForRender(before, 'the pair panel');
  assert.equal(await page.text('#panel h2'), 'Pair');
  assert.equal(await page.count('#panel .pair.selected'), 1);
  assert.equal(await page.count('#pairs-body tr.selected'), 1);
}));

// ------------------------------------------------------------------ comparison

test('diff: ?src and ?diff compare two documents, by node key and by guid key', opts, () => browser.withPage(url('?src=sample/diff_before.json&diff=sample/diff_after.json'), async (page) => {
  const before = load('diff_before.json');
  const after = load('diff_after.json');
  const changes = M.diffDocuments(before, after, 'node');
  const sc = S.visibleScene(M.buildModel(after), filter(), false, M.decorations(changes, before));
  assert.equal(await page.evaluate(hidden('changes-group')), false);
  assert.equal(await page.text('#changes-summary'), M.changesSummary(changes));
  // every arrow carries the mark the comparison gave it
  assert.deepEqual(await page.evaluate(EDGE_MARKS), S.sceneEdges(sc).map(e => `${e.id} ${e.mark}`).sort());
  assert.equal(await page.count('#graph g.edge.added'), S.sceneEdges(sc).filter(e => e.mark === '+').length);
  assert.equal(await page.count('#graph g.edge.changed'), S.sceneEdges(sc).filter(e => e.mark === '~').length);
  assert.ok(await page.count('#graph g.edge.added') > 0 && await page.count('#graph g.edge.changed') > 0);

  // a removed pair whose nodes are gone with it has no arrow to draw (scene.js), it is a
  // ghost row in the table
  let mark = await page.render();
  await page.click('.tab[data-view="table"]');
  await page.waitForRender(mark, 'the table tab');
  assert.ok(sc.ghosts.length > 0, 'the fixture must remove a pair');
  assert.equal(await page.count('#pairs-body tr.removed'), sc.ghosts.length);
  mark = await page.render();
  await page.click('#pairs-body tr.removed');
  await page.waitForRender(mark, 'the ghost card');
  assert.match(await page.text('#panel .change.removed'), /removed/);

  mark = await page.render();
  await page.click('.tab[data-view="graph"]');
  await page.waitForRender(mark, 'the graph tab');
  mark = await page.render();
  await page.evaluate(`(() => {
    const select = document.getElementById('diff-key');
    select.value = 'guid';
    select.dispatchEvent(new Event('change', {bubbles: true}));
    return true;
  })()`);
  await page.waitForRender(mark, 'the guid key');
  assert.equal(await page.text('#changes-summary'), M.changesSummary(M.diffDocuments(before, after, 'guid')));

  const all = await page.count('#graph g.edge');
  mark = await page.render();
  await setChecked(page, '#filter-changes', true);
  await page.waitForRender(mark, '"changes only"');
  const changed = await page.count('#graph g.edge');
  assert.ok(changed > 0 && changed < all, `"changes only" narrows the graph (${changed} of ${all})`);
  assert.equal(await page.count('#graph g.edge:not(.added):not(.changed):not(.removed)'), 0);
}));

test('diff: a document that carries its own changes shows them without a before', opts, () => browser.withPage(url('?src=sample/diff.json'), async (page) => {
  const doc = load('diff.json');
  assert.equal(await page.evaluate(hidden('changes-group')), false);
  assert.equal(await page.text('#changes-summary'), M.changesSummary(doc.changes));
  assert.equal(await page.evaluate(hidden('diff-key-label')), true, 'the key selector needs a before document');
  assert.ok(await page.count('#graph g.edge.added') > 0);
}));

// ------------------------------------------------------------------ loading

test('drop: dropping a file shows the overlay and loads the document', opts, () => browser.withPage(url('?src=sample/sample.json'), async (page) => {
  assert.equal(await page.evaluate(`(() => {
    document.dispatchEvent(new DragEvent('dragenter', {bubbles: true}));
    return document.getElementById('drop-overlay').hidden;
  })()`), false, 'the overlay appears while dragging');

  await page.evaluate(`(async () => {
    const text = await fetch('sample/shm_split.json').then(r => r.text());
    const data = new DataTransfer();
    data.items.add(new File([text], 'shm_split.json', {type: 'application/json'}));
    document.dispatchEvent(new DragEvent('drop', {bubbles: true, dataTransfer: data}));
    return true;
  })()`);
  await page.waitFor('document.title.endsWith("shm_split.json")', 'the dropped document to load');
  assert.equal(await page.evaluate('document.getElementById("drop-overlay").hidden'), true);
  assert.deepEqual(await page.evaluate(NODE_IDS), expected(load('shm_split.json'), filter()).nodes);
  assert.match(await page.text('#meta'), /3 topics, 5 pairs/);
}));

test('loading: a ?src that does not exist reports the failure in the header', opts, () => browser.withPage(url('?src=sample/nope.json'), async (page) => {
  page.allow(/Failed to load resource/);   // the browser's own 404 line
  await page.waitFor('document.getElementById("meta").textContent.includes("failed to load")', 'the failure message');
  assert.match(await page.text('#meta'), /failed to load sample\/nope\.json: 404/);
}));
