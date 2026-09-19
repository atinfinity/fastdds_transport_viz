// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Pure scene functions of the web viewer: which pairs, ghosts and nodes to draw for the
// current filters, the column layout, the edge paths and the label positions. No DOM, no
// d3, so they run under Node for the unit tests (web/test/scene.test.js) and in the
// browser, where app.js reads them from globalThis.TransportVizScene (#136).

(function (root, factory) {
  const model = typeof module !== 'undefined' && module.exports ? require('./model.js') : root.TransportVizModel;
  const api = factory(model);
  root.TransportVizScene = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, (M) => {
  'use strict';
  const { isInternalTopic, filterRegex, visiblePairs, visibleNodesModel, bundle, pairKey, keyId, markedPairs, pruneNodes, serversOf } = M;

  /** Mark of a visible pair, ' ' when none. */
  function markOf(vp, marks) {
    const m = marks.get(keyId(pairKey(vp.topic, vp.pair)));
    return m ? m.mark : ' ';
  }

  /** Ghosts that pass the topic / node / transport filters (a ghost's transport is known only with the before document). */
  function visibleGhosts(ghosts, f) {
    const re = filterRegex(f.topic);
    const nre = filterRegex(f.node);
    return ghosts.filter(g => {
      if (f.hideInternal && isInternalTopic(g.topic || { topic: g.key.topic })) return false;
      if (re && !re.test(g.key.topic)) return false;
      if (nre && !nre.test(g.key.writer_node) && !nre.test(g.key.reader_node)) return false;
      if (g.pair && !f.transports.has(g.pair.transport)) return false;
      return true;
    });
  }

  /**
   * The pairs, ghosts and node model to draw: `filter` (topic, node, transports,
   * hideInternal), then `changes only` against `deco` ({marks, ghosts}).
   */
  function visibleScene(fullModel, filter, changesOnly, deco) {
    let pairs = visiblePairs(fullModel, filter);
    const ghosts = visibleGhosts(deco.ghosts, filter);
    if (changesOnly) pairs = markedPairs(pairs, deco.marks);
    let { model, matched } = visibleNodesModel(fullModel, pairs, filter.node);
    // a ghost edge needs both of its nodes: those the after document still has by name
    const ghostNodes = ghosts.flatMap(g => [g.key.writer_node, g.key.reader_node]).filter(id => fullModel.nodes.has(id));
    if (filter.node) {
      const nre = filterRegex(filter.node);
      const keep = new Set([...model.nodes.keys(), ...ghosts.filter(g => nre && (nre.test(g.key.writer_node) || nre.test(g.key.reader_node)))
        .flatMap(g => [g.key.writer_node, g.key.reader_node])]);
      model = pruneNodes(fullModel, [], [...keep].filter(id => fullModel.nodes.has(id)));
    }
    if (changesOnly) model = pruneNodes(model, pairs, ghostNodes);
    return { pairs, ghosts, model, matched, marks: deco.marks };
  }

  const MARK_RANK = { '+': 3, '~': 2, ' ': 0 };

  /**
   * Bundled edges plus one dashed ghost edge per removed pair whose nodes are both still
   * there, each with `k`, its index among the parallel edges of the same node pair.
   */
  function sceneEdges(scene) {
    const edges = bundle(scene.pairs);
    for (const e of edges) {
      e.mark = e.pairs.reduce((best, vp) => { const m = markOf(vp, scene.marks); return MARK_RANK[m] > MARK_RANK[best] ? m : best; }, ' ');
    }
    for (const g of scene.ghosts) {
      if (!scene.model.nodes.has(g.key.writer_node) || !scene.model.nodes.has(g.key.reader_node)) continue;
      edges.push({ id: g.id, source: g.key.writer_node, target: g.key.reader_node, transport: g.pair ? g.pair.transport : 'NONE',
        confidence: g.pair ? g.pair.confidence : 'certain', pairs: [], warn: false, mark: '-', ghost: g });
    }
    // one client -> server edge per attributed client node whose server is in view (#86);
    // no transport, no label, never bundled with the topic edges
    for (const n of scene.model.nodes.values()) {
      for (const s of serversOf(n)) {
        if (!scene.model.nodes.has(s)) continue;
        edges.push({ id: `client|${n.id}|${s}`, source: n.id, target: s, transport: 'NONE', confidence: 'certain', pairs: [], warn: false, mark: ' ', client: true });
      }
    }
    // parallel-edge index per (source,target) so bundles do not overlap
    const groups = new Map();
    for (const e of edges) {
      const g = [e.source, e.target].join('→');
      if (!groups.has(g)) groups.set(g, []);
      groups.get(g).push(e);
    }
    for (const list of groups.values()) list.forEach((e, i) => { e.k = i - (list.length - 1) / 2; });
    return edges;
  }

  /** Text of an edge label: topic (or count) and transport, with the change mark in front. */
  function edgeLabel(d) {
    if (d.client) return '';
    const label = d.ghost ? `${d.ghost.key.topic} · removed` :
      d.pairs.length === 1 ? `${d.pairs[0].topic.topic} · ${d.transport}${d.confidence === 'likely' ? '?' : ''}` : `${d.pairs.length} topics · ${d.transport}${d.confidence === 'likely' ? '?' : ''}`;
    return (d.mark === ' ' ? '' : d.mark + ' ') + label;
  }

  // ---------------------------------------------------------------- layout

  const L = { hostGap: 60, hostPad: 16, nodeW: 190, nodeH: 46, nodeGap: 34, maxRows: 8, top: 40, left: 30 };

  function layout(model) {
    let x = L.left;
    const pos = new Map();
    const hostBoxes = [];
    for (const h of model.hosts) {
      const cols = Math.ceil(h.nodes.length / L.maxRows);
      const rows = Math.min(h.nodes.length, L.maxRows);
      const w = L.hostPad * 2 + cols * L.nodeW + (cols - 1) * L.nodeGap;
      const hgt = L.hostPad * 2 + 24 + rows * L.nodeH + (rows - 1) * L.nodeGap;
      h.nodes.forEach((n, i) => {
        const c = Math.floor(i / L.maxRows);
        const r = i % L.maxRows;
        pos.set(n.id, { x: x + L.hostPad + c * (L.nodeW + L.nodeGap), y: L.top + L.hostPad + 24 + r * (L.nodeH + L.nodeGap), w: L.nodeW, h: L.nodeH });
      });
      hostBoxes.push({ host: h, x, y: L.top, w, h: hgt });
      x += w + L.hostGap;
    }
    return { pos, hostBoxes, width: x, height: L.top + Math.max(0, ...hostBoxes.map(b => b.h)) + 40 };
  }

  /**
   * Cubic Bézier between two node boxes as its four control points; `k` spreads parallel
   * edges apart. `edgePathD` renders it, `edgeMidpoint` places the label.
   */
  function edgeCurve(a, b, k) {
    const spread = k * 14;
    if (a === b) {
      const x = a.x + a.w, y = a.y + a.h / 2 + spread;
      return [{ x, y: y - 8 }, { x: x + 50, y: y - 30 }, { x: x + 50, y: y + 30 }, { x, y: y + 8 }];
    }
    const ac = { x: a.x + a.w / 2, y: a.y + a.h / 2 };
    const bc = { x: b.x + b.w / 2, y: b.y + b.h / 2 };
    const sameColumn = Math.abs(ac.x - bc.x) < 1;
    if (sameColumn) {
      const x = a.x + a.w;
      const bulge = 40 + Math.abs(ac.y - bc.y) * 0.15 + spread;
      return [{ x, y: ac.y + spread * 0.3 }, { x: x + bulge, y: ac.y }, { x: x + bulge, y: bc.y }, { x, y: bc.y - spread * 0.3 }];
    }
    const leftToRight = ac.x < bc.x;
    const sx = leftToRight ? a.x + a.w : a.x;
    const tx = leftToRight ? b.x : b.x + b.w;
    const sy = ac.y + spread, ty = bc.y + spread;
    const dx = (tx - sx) * 0.5;
    return [{ x: sx, y: sy }, { x: sx + dx, y: sy }, { x: tx - dx, y: ty }, { x: tx, y: ty }];
  }

  /** SVG path data of an edge curve. */
  function edgePathD(c) {
    return `M${c[0].x},${c[0].y} C${c[1].x},${c[1].y} ${c[2].x},${c[2].y} ${c[3].x},${c[3].y}`;
  }

  /** Path between two node boxes (edgeCurve + edgePathD). */
  function edgePath(a, b, k) { return edgePathD(edgeCurve(a, b, k)); }

  function bezierAt(c, t) {
    const u = 1 - t;
    const w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return { x: w0 * c[0].x + w1 * c[1].x + w2 * c[2].x + w3 * c[3].x, y: w0 * c[0].y + w1 * c[1].y + w2 * c[2].y + w3 * c[3].y };
  }

  const MIDPOINT_SAMPLES = 16;

  /**
   * The point half way along the curve's length, like getPointAtLength(getTotalLength()/2)
   * but from the control points alone: the polyline through 16 samples is within a pixel
   * of the arc on these gently bent curves, and no SVG layout is forced per arrow (#136).
   */
  function edgeMidpoint(c) {
    const pts = [];
    for (let i = 0; i <= MIDPOINT_SAMPLES; i++) pts.push(bezierAt(c, i / MIDPOINT_SAMPLES));
    const seg = [];
    let total = 0;
    for (let i = 1; i < pts.length; i++) {
      const d = Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
      seg.push(d);
      total += d;
    }
    let remaining = total / 2;
    for (let i = 0; i < seg.length; i++) {
      if (remaining <= seg[i]) {
        const f = seg[i] ? remaining / seg[i] : 0;
        return { x: pts[i].x + (pts[i + 1].x - pts[i].x) * f, y: pts[i].y + (pts[i + 1].y - pts[i].y) * f };
      }
      remaining -= seg[i];
    }
    return pts[pts.length - 1];
  }

  return { L, markOf, visibleGhosts, visibleScene, sceneEdges, edgeLabel, layout, edgeCurve, edgePathD, edgePath, edgeMidpoint, bezierAt };
});
