// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Pure data-model and formatting functions of the web viewer: no DOM, no d3, so they
// run under Node for the unit tests (web/test/model.test.js) and in the browser, where
// app.js reads them from globalThis.TransportVizModel.

(function (root, factory) {
  const api = factory();
  root.TransportVizModel = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, () => {
  'use strict';

  const TRANSPORTS = ['UDPv4', 'UDPv6', 'TCPv4', 'TCPv6', 'SHM', 'DATA_SHARING', 'NONE'];
  const INTERNAL_TOPICS = new Set(['/parameter_events', '/rosout']);

  /** Flatten the document into nodes, hosts and pairs with resolved endpoints. */
  function buildModel(doc) {
    const nodes = new Map();      // key -> {id, name, host, process, pubs:[], subs:[], unmatched:[]}
    const hosts = new Map();      // host label -> {label, nodes:[]}
    const pairs = [];
    const endpointsByGuid = new Map();

    // Service endpoints carry no node name (the ROS graph API only covers
    // topics); attribute them to the node that owns the same participant.
    const nodeByParticipant = new Map();
    for (const t of doc.topics) {
      for (const ep of [...t.writers, ...t.readers]) {
        if (ep.node) nodeByParticipant.set(ep.participant_guid_prefix, ep.node);
      }
    }
    const nodeKey = (ep) => ep.node || nodeByParticipant.get(ep.participant_guid_prefix) || `participant ${ep.participant_guid_prefix}`;

    const touchNode = (ep) => {
      const id = nodeKey(ep);
      if (!nodes.has(id)) {
        nodes.set(id, { id, name: id, host: ep.host, process: ep.process || '', pubs: [], subs: [], unmatched: [] });
        if (!hosts.has(ep.host)) hosts.set(ep.host, { label: ep.host, nodes: [] });
        hosts.get(ep.host).nodes.push(nodes.get(id));
      }
      return nodes.get(id);
    };

    for (const t of doc.topics) {
      for (const w of t.writers) { endpointsByGuid.set(w.guid, w); touchNode(w).pubs.push({ topic: t, ep: w }); }
      for (const r of t.readers) { endpointsByGuid.set(r.guid, r); touchNode(r).subs.push({ topic: t, ep: r }); }
      if (t.unmatched_reasons.length) {
        for (const ep of [...t.writers, ...t.readers]) touchNode(ep).unmatched.push({ topic: t, reasons: t.unmatched_reasons });
      }
      t.pairs.forEach((p, i) => {
        const w = endpointsByGuid.get(p.writer_guid);
        const r = endpointsByGuid.get(p.reader_guid);
        pairs.push({
          id: `${t.dds_topic}#${i}`, topic: t, pair: p, writer: w, reader: r,
          writerNode: nodeKey(w), readerNode: nodeKey(r),
        });
      });
    }
    for (const n of nodes.values()) {
      n.pubs.sort((a, b) => a.topic.topic.localeCompare(b.topic.topic));
      n.subs.sort((a, b) => a.topic.topic.localeCompare(b.topic.topic));
    }
    const hostList = [...hosts.values()].sort((a, b) => (a.label === 'local' ? -1 : b.label === 'local' ? 1 : a.label.localeCompare(b.label)));
    for (const h of hostList) h.nodes.sort((a, b) => a.name.localeCompare(b.name));
    return { nodes, hosts: hostList, pairs };
  }

  /** RegExp for a filter field, or null when empty or invalid (an invalid pattern filters nothing). */
  function filterRegex(pattern) {
    if (!pattern) return null;
    try { return new RegExp(pattern); } catch (e) { return null; }
  }

  /**
   * Same semantics as `transport_viz --node`: pairs where the writer's or the reader's node
   * matches. `filter` = {topic, node, transports: Set, hideInternal}.
   */
  function visiblePairs(model, filter) {
    const f = filter;
    const re = filterRegex(f.topic);
    const nre = filterRegex(f.node);
    return model.pairs.filter(({ topic, pair, writerNode, readerNode }) => {
      if (f.hideInternal && INTERNAL_TOPICS.has(topic.topic)) return false;
      if (!f.transports.has(pair.transport)) return false;
      if (re && !re.test(topic.topic)) return false;
      if (nre && !nre.test(writerNode) && !nre.test(readerNode)) return false;
      return true;
    });
  }

  /** With a node filter: matching nodes (even without visible pairs) plus the partners of visible pairs. */
  function visibleNodesModel(model, pairs, nodePattern) {
    const nre = filterRegex(nodePattern);
    if (!nre) return { model, matched: () => false };
    const keep = new Set();
    for (const n of model.nodes.values()) if (nre.test(n.id)) keep.add(n.id);
    for (const vp of pairs) { keep.add(vp.writerNode); keep.add(vp.readerNode); }
    const nodes = new Map([...model.nodes].filter(([id]) => keep.has(id)));
    const hosts = model.hosts.map(h => ({ ...h, nodes: h.nodes.filter(n => keep.has(n.id)) })).filter(h => h.nodes.length);
    return { model: { ...model, nodes, hosts }, matched: id => nre.test(id) };
  }

  /** Bundle pairs into edges: same writer node, reader node, transport and confidence. */
  function bundle(pairs) {
    const edges = new Map();
    for (const vp of pairs) {
      const key = `${vp.writerNode}→${vp.readerNode}|${vp.pair.transport}|${vp.pair.confidence}`;
      if (!edges.has(key)) {
        edges.set(key, { id: key, source: vp.writerNode, target: vp.readerNode, transport: vp.pair.transport, confidence: vp.pair.confidence, pairs: [], warn: false });
      }
      const e = edges.get(key);
      e.pairs.push(vp);
      if (vp.pair.warnings.length) e.warn = true;
    }
    return [...edges.values()];
  }

  /** SI formatting like the table: 3 significant digits below 1000 of a unit. */
  function humanBytes(v, unit) {
    const prefixes = ['', 'k', 'M', 'G', 'T'];
    let i = 0;
    while (v >= 1000 && i < 4) { v /= 1000; i++; }
    const digits = i === 0 ? 0 : v < 10 ? 2 : v < 100 ? 1 : 0;
    return `${v.toFixed(digits)} ${prefixes[i]}${unit}`;
  }

  function measuredText(m) {
    if (!m || !m.available) return '';
    if (!m.transports.length) return m.delivered ? 'none (delivered)' : 'none';
    if (!m.packets) return `${m.transports.join('+')} (idle)`;
    const bytes = typeof m.bytes === 'number' ? ` ${humanBytes(m.bytes, 'B')}` : '';
    return `${m.transports.join('+')} ${m.packets} pkt${bytes}`;
  }

  function rateText(m) {
    if (!m || typeof m.throughput_bytes_per_s !== 'number') return '';
    return humanBytes(m.throughput_bytes_per_s, 'B/s');
  }

  function escapeHtml(s) { return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])); }

  /** Shared memory of the environment transport_viz ran in (the `shm` object), as HTML. */
  function shmText(shm, descriptions) {
    if (!shm || !shm.available) return '';
    const stale = shm.stale_segments + shm.stale_ports;
    const desc = descriptions || {};
    const warnings = (shm.warnings || []).map(w => `<span class="code warn" title="${escapeHtml(desc[w] || '')}"><b>!${escapeHtml(w)}</b></span>`).join(' ');
    return `shared memory: ${escapeHtml(shm.path)} ${humanBytes(shm.used_bytes, 'B')} used of ${humanBytes(shm.total_bytes, 'B')}` +
      ` · Fast DDS ${humanBytes(shm.fastdds_bytes, 'B')} in ${shm.segments} segment(s), ${shm.ports} port(s), ${shm.datasharing_histories} data-sharing histor${shm.datasharing_histories === 1 ? 'y' : 'ies'}` +
      (stale ? ` (${stale} stale)` : '') + (shm.nodes_visible === false ? ' · nodes in another IPC namespace' : '') +
      (warnings ? ` ${warnings}` : '');
  }

  return { TRANSPORTS, INTERNAL_TOPICS, buildModel, filterRegex, visiblePairs, visibleNodesModel, bundle, humanBytes, measuredText, rateText, escapeHtml, shmText };
});
