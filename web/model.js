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

  /** What the rmw reports for a node whose ros_discovery_info it has not received. */
  const UNKNOWN_NODE_NAME = '_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_';

  /**
   * Documents written before transport_viz read ros_discovery_info itself (#112) carry the
   * rmw's unknown node name: make it '' in place, so that such endpoints fall back to their
   * participant instead of all merging into one node. Returns `doc`.
   */
  function normalizeDocument(doc) {
    const clear = (o, k) => { if (o && o[k] === UNKNOWN_NODE_NAME) o[k] = ''; };
    for (const t of (doc && doc.topics) || []) {
      for (const ep of [...(t.writers || []), ...(t.readers || [])]) clear(ep, 'node');
      for (const p of t.pairs || []) { clear(p, 'writer_node'); clear(p, 'reader_node'); }
    }
    return doc;
  }

  /**
   * A native-buffer companion topic (`<topic>/_buf_cpu` of rmw_fastrtps_cpp) whose every
   * endpoint transport_viz folded into a writer or reader of the parent topic
   * (`buffer_parent_guid`): its counters are already in the parent's pairs.
   */
  function isFoldedBufferCompanion(topic) {
    const eps = [...(topic.writers || []), ...(topic.readers || [])];
    return eps.length > 0 && eps.every(ep => !!ep.buffer_parent_guid);
  }

  /** What "hide ROS internal topics" hides: /parameter_events, /rosout and folded companion topics. */
  function isInternalTopic(topic) {
    return INTERNAL_TOPICS.has(topic.topic) || isFoldedBufferCompanion(topic);
  }

  /** A `participants[]` entry that is a Discovery Server (#86). */
  function isDiscoveryServer(p) { return p.discovery_protocol === 'SERVER' || p.discovery_protocol === 'BACKUP'; }
  /** A `participants[]` entry that announced itself a client of a Discovery Server. */
  function isDiscoveryClient(p) { return p.discovery_protocol === 'CLIENT' || p.discovery_protocol === 'SUPER_CLIENT'; }
  /** Node id of a server participant: never a ROS node name. */
  function serverNodeId(p) { return `server ${p.guid_prefix}`; }
  /** The client participants of a node, those whose server is known first. */
  function clientParticipants(n) { return (n.participants || []).filter(isDiscoveryClient); }
  /** Server node ids a node is attributed to (#86): the `discovery_server` of its client participants. */
  function serversOf(n) {
    return [...new Set(clientParticipants(n).filter(p => p.discovery_server).map(p => `server ${p.discovery_server}`))];
  }

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

    // what each participant announced about discovery (#86), by prefix
    const participants = new Map((doc.participants || []).map(p => [p.guid_prefix, p]));
    const addNode = (id, host, extra) => {
      nodes.set(id, { id, name: id, host, process: '', pubs: [], subs: [], unmatched: [], participants: [], server: null, ...extra });
      if (!hosts.has(host)) hosts.set(host, { label: host, nodes: [] });
      hosts.get(host).nodes.push(nodes.get(id));
      return nodes.get(id);
    };
    const touchNode = (ep) => {
      const id = nodeKey(ep);
      const n = nodes.get(id) || addNode(id, ep.host, { process: ep.process || '' });
      const p = participants.get(ep.participant_guid_prefix);
      if (p && !n.participants.includes(p)) n.participants.push(p);
      return n;
    };
    // Discovery Servers have no endpoint: one node per SERVER / BACKUP participant, named as
    // it announced itself (#86)
    for (const p of participants.values()) {
      if (!isDiscoveryServer(p) || p.own) continue;
      addNode(serverNodeId(p), p.host, { name: p.name || 'Discovery Server', server: p, participants: [p] });
    }

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
      if (f.hideInternal && isInternalTopic(topic)) return false;
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
    keepServers(model, keep);
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

  /** "UDPv4 127.0.0.1:7413 + SHM:8169 47 pkt 1.31 kB": the addresses stay next to their
   *  transport, falling back to the kinds alone for a document without measured.locators. */
  function measuredText(m) {
    if (!m || !m.available) return '';
    if (!m.transports.length) return m.delivered ? 'none (delivered)' : 'none';
    const locators = Array.isArray(m.locators) ? m.locators : [];
    const label = locators.length
      ? locators.map(l => `${l.kind}${l.address ? ' ' + l.address : ''}:${l.port}`).join(' + ')
      : m.transports.join('+');
    if (!m.packets) return `${label} (idle)`;
    const bytes = typeof m.bytes === 'number' ? ` ${humanBytes(m.bytes, 'B')}` : '';
    return `${label} ${m.packets} pkt${bytes}`;
  }

  /** Seconds with 3 significant digits in ns / µs / ms / s (sign kept). */
  function humanSeconds(seconds) {
    const units = ['ns', 'µs', 'ms', 's'];
    let v = Math.abs(seconds) * 1e9;
    let i = 0;
    while (v >= 1000 && i < 3) { v /= 1000; i++; }
    const digits = v < 10 ? 2 : v < 100 ? 1 : 0;
    return `${seconds < 0 ? '-' : ''}${v.toFixed(digits)} ${units[i]}`;
  }

  /**
   * "3 lost, 2 resent" / "0" from measured.reliability, '' without counters, "- lost" when
   * lost_packets is null (the reader's participant does not publish RTPS_LOST).
   */
  function lossText(m) {
    if (!m || !m.reliability) return '';
    const r = m.reliability;
    const lostKnown = r.lost_packets !== null && r.lost_packets !== undefined;
    if (lostKnown && !r.lost_packets && !r.resent_datas) return '0';
    const parts = [];
    if (!lostKnown) parts.push('- lost');
    else if (r.lost_packets) parts.push(`${r.lost_packets} lost`);
    if (r.resent_datas) parts.push(`${r.resent_datas} resent`);
    return parts.join(', ');
  }

  /** "0.42 ms (max 1.30 ms)" from measured.latency_s, '' without values. */
  function latencyText(m) {
    if (!m || !m.latency_s || typeof m.latency_s.mean !== 'number') return '';
    return `${humanSeconds(m.latency_s.mean)} (max ${humanSeconds(m.latency_s.max)})`;
  }

  /** "120", "9.9" or "≥120" from measured.delivered_per_s (#143), '' without a rate. */
  function rateText(m) {
    if (!m || typeof m.delivered_per_s !== 'number') return '';
    const v = m.delivered_per_s;
    return (m.delivered_per_s_lower_bound ? '\u2265' : '') + (v >= 100 ? v.toFixed(0) : v.toFixed(1));
  }

  /** "delivered samples/s over the last 5 s" for the rate cell's tooltip (#143), '' without a rate. */
  function rateTitle(m) {
    if (!rateText(m)) return '';
    const w = typeof m.delivered_per_s_window_s === 'number' ? ` over ${m.delivered_per_s_window_s.toFixed(1)} s` : '';
    return `delivered samples/s${w}${m.delivered_per_s_lower_bound ? '; at least: the reader participant\'s statistics writer skipped samples' : ''}`;
  }

  /** "0.42 ms" from a topic's `latency_s` (the slowest pair's mean), '' when null. */
  function topicLatencyText(t) {
    return t && typeof t.latency_s === 'number' ? humanSeconds(t.latency_s) : '';
  }

  /**
   * The CLI's topic LOSS cell from `lost_packets` / `resent_datas`: "3 lost, 2 resent", "0",
   * "- lost" when RTPS_LOST is unknown, '' when the topic has no counters (`resent_datas` null).
   */
  function topicLossText(t) {
    if (!t || typeof t.resent_datas !== 'number') return '';
    return lossText({ reliability: { lost_packets: t.lost_packets, resent_datas: t.resent_datas } });
  }

  /**
   * Table rows grouped under a header per topic (#144), the CLI's `--verbose` shape.
   * `rows` are visible pair rows, `ghosts` removed-pair rows; both carry `.topic.topic`.
   * `topics` is the document's `topics[]`: a header reads its aggregates from there and
   * never from the rows, so a filter that hides pairs leaves the topic's numbers alone.
   * A topic only ghosts still name (an orphan) gets a header without aggregates.
   * Groups are keyed by the DDS topic name (a service's request and reply topics share a
   * display name) and follow the first appearance of each topic in `rows` then `ghosts`.
   */
  function groupPairsByTopic(rows, ghosts, topics) {
    const byDds = new Map((topics || []).map(t => [t.dds_topic, t]));
    const byName = new Map();
    for (const t of topics || []) if (!byName.has(t.topic)) byName.set(t.topic, t);
    const groups = new Map();
    const groupOf = (row) => {
      // a ghost without its before document knows the display name only
      const named = row.topic.dds_topic ? null : byName.get(row.topic.topic);
      const key = row.topic.dds_topic || (named ? named.dds_topic : row.topic.topic);
      if (!groups.has(key)) {
        const t = byDds.get(key) || null;
        const name = row.topic.topic;
        groups.set(key, {
          id: `topic|${key}`, key, name, topic: t || { topic: name, type: row.topic.type || '' }, aggregates: !!t,
          pubs: t ? t.writers.length : null, subs: t ? t.readers.length : null,
          transports: t ? [...new Set(t.pairs.map(p => p.transport))] : [],
          latency: t ? topicLatencyText(t) : '', latencyValue: t && typeof t.latency_s === 'number' ? t.latency_s : null,
          loss: t ? topicLossText(t) : '', lostValue: t && typeof t.lost_packets === 'number' ? t.lost_packets : null,
          reasons: t ? t.unmatched_reasons.join(', ') : '',
          pairs: [], ghosts: [],
        });
      }
      return groups.get(key);
    };
    for (const r of rows) groupOf(r).pairs.push(r);
    for (const g of ghosts) groupOf(g).ghosts.push(g);
    return [...groups.values()];
  }

  /**
   * Table sort order for two cell values: numbers numerically, strings by locale, and a
   * missing value (null, undefined or '') last whichever the direction.
   */
  function compareCells(a, b, asc = true) {
    const missing = v => v === null || v === undefined || v === '';
    if (missing(a) && missing(b)) return 0;
    if (missing(a)) return 1;
    if (missing(b)) return -1;
    const r = typeof a === 'number' && typeof b === 'number' ? a - b : String(a).localeCompare(String(b));
    return r * (asc ? 1 : -1);
  }

  function escapeHtml(s) { return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])); }

  /**
   * Reason / warning codes as HTML: the code, its description and, when the document knows
   * one, its remedy (`reason_code_descriptions` / `reason_code_remedies`; both optional).
   */
  function codeListHtml(codes, descriptions, remedies, warn) {
    const desc = descriptions || {};
    const rem = remedies || {};
    return (codes || []).map(c => `<span class="code ${warn ? 'warn' : ''}"><b>${warn ? '!' : ''}${escapeHtml(c)}</b>` +
      `<span class="desc">${escapeHtml(desc[c] || '')}</span>` +
      (rem[c] ? `<span class="fix">fix: ${escapeHtml(rem[c])}</span>` : '') + '</span>').join('');
  }

  /** Shared memory of the environment transport_viz ran in (the `shm` object), as HTML. */
  function shmText(shm, descriptions, remedies) {
    if (!shm || !shm.available) return '';
    const stale = shm.stale_segments + shm.stale_ports;
    const desc = descriptions || {};
    const rem = remedies || {};
    const tip = w => (desc[w] || '') + (rem[w] ? ` Fix: ${rem[w]}` : '');
    const warnings = (shm.warnings || []).map(w => `<span class="code warn" title="${escapeHtml(tip(w))}"><b>!${escapeHtml(w)}</b></span>`).join(' ');
    return `shared memory: ${escapeHtml(shm.path)} ${humanBytes(shm.used_bytes, 'B')} used of ${humanBytes(shm.total_bytes, 'B')}` +
      ` · Fast DDS ${humanBytes(shm.fastdds_bytes, 'B')} in ${shm.segments} segment(s), ${shm.ports} port(s), ${shm.datasharing_histories} data-sharing histor${shm.datasharing_histories === 1 ? 'y' : 'ies'}` +
      (shm.datasharing_notifications ? `, ${shm.datasharing_notifications} data-sharing notification(s)` : '') +
      (stale ? ` (${stale} stale)` : '') + (shm.nodes_visible === false ? ' · nodes in another IPC namespace' : '') +
      (warnings ? ` ${warnings}` : '');
  }

  /**
   * What the split verdicts saw of an endpoint's participant in shared memory (#125): its
   * `participants` entry as one line, '' when the document has no such section or entry.
   * Ports read "7417 held", with "(2 participants)" where the number is announced by more
   * than one and "(no proof)" where only its ros_discovery_info reader has it.
   */
  function participantShmText(doc, prefix) {
    const p = ((doc && doc.participants) || []).find(x => x.guid_prefix === prefix);
    if (!p) return '';
    const ports = (p.shm_ports || []).map(sp => `${sp.port} ${sp.lock}` +
      (sp.announced_by > 1 ? ` (${sp.announced_by} participants)` : '') + (sp.proof ? '' : ' (no proof)'));
    return `${p.shm_visibility}${p.own ? ' (the tool\'s own)' : ''}` + (ports.length ? ` · ports ${ports.join(', ')}` : ' · no SHM port');
  }

  /**
   * The endpoint panel's data-sharing row (#163): the writer's history size in the tool's
   * /dev/shm, a reader's notification segment, and whether the segment the split verdicts
   * look for is there. '' when the document says nothing (no size, no visibility or
   * `unprobed`: a document before #163 or an endpoint without data-sharing).
   * `isWriter` names the segment when there is no size to show.
   */
  function datasharingText(ep, isWriter) {
    if (!ep) return '';
    const bytes = typeof ep.datasharing_history_bytes === 'number' ? `history ${humanBytes(ep.datasharing_history_bytes, 'B')} in /dev/shm` : '';
    const v = ep.datasharing_segment_visibility;
    if (!v || v === 'unprobed') return bytes;
    const segment = isWriter ? 'history segment' : 'notification segment';
    return bytes ? `${bytes} · segment ${v}` : `${segment} ${v}`;
  }

  /**
   * The `stats` object as the meta line shows it: what arrived, and what never did (#134).
   * The loss is cumulative over the run, and a pair can show no measurement because of it.
   */
  /**
   * The `stats` object as one line for the meta bar, warnings as tooltips like shmText.
   * `lost` is what never reached the tool (#134); `samples_lost_at_start` is the normal
   * burst from before the readers matched and stays out of it. Whether the loss cost a
   * measurement is the tool's call (#147): the marker follows `stats.warnings`, never the
   * numbers, so a lost count without a marker is a loss that did no harm.
   */
  function statsText(stats, descriptions, remedies) {
    if (!stats || !stats.enabled) return 'no statistics';
    const desc = descriptions || {};
    const rem = remedies || {};
    const tip = w => (desc[w] || '') + (rem[w] ? ` Fix: ${rem[w]}` : '');
    // HISTORY_LATENCY is received best-effort (#141): its gaps coarsen LATENCY, they do not
    // cost a pair its measurement, so they are named apart and raise no warning.
    const lostLatency = stats.samples_lost_latency || 0;
    const lostCounters = Math.max((stats.samples_lost || 0) - lostLatency, 0);
    const lost = lostCounters + (stats.samples_rejected || 0);
    const warnings = (stats.warnings || []).map(
      w => ` <span class="code warn" title="${escapeHtml(tip(w))}"><b>!${escapeHtml(w)}</b></span>`).join('');
    return escapeHtml(`statistics: ${stats.samples} samples` + (lost ? `, ${lost} lost` : '') +
      (lostLatency ? `, ${lostLatency} latency lost` : '')) + warnings;
  }

  // ---------------------------------------------------------------- comparing two documents
  //
  // A port of the C++ pair_state() / diff_snapshots() (decision.cpp): the same `changes`
  // object as `transport_viz diff --json` and `--watch --json`, so that a document carrying
  // one (from the CLI or a live frame) and two documents compared here look the same.

  /** Identity of a pair as the `changes` object names it. */
  function pairKey(topic, pair) {
    return { topic: topic.topic, writer_guid: pair.writer_guid, reader_guid: pair.reader_guid,
      writer_node: pair.writer_node || '', reader_node: pair.reader_node || '' };
  }

  /** String form of a key, for Maps. */
  function keyId(k) { return `${k.topic}|${k.writer_guid}|${k.reader_guid}`; }

  const locatorId = l => ({ kind: l.kind, address: l.address, port: l.port });

  /** The part of a pair whose change is worth highlighting (C++ PairState; counters are left out). */
  function pairState(pair) {
    const m = pair.measured || {};
    return {
      transport: pair.transport, confidence: pair.confidence,
      measured: [...(m.transports || [])],
      locator: pair.locator ? locatorId(pair.locator) : null,
      measured_locators: (m.locators || []).map(locatorId),
      warnings: [...(pair.warnings || [])],
    };
  }

  function sameLocator(a, b, ignorePorts) {
    if (!a || !b) return a === b;
    return a.kind === b.kind && a.address === b.address && (ignorePorts || a.port === b.port);
  }

  /** PairState equality; the node key ignores the port numbers a restart renumbers. */
  function sameState(a, b, ignorePorts) {
    return a.transport === b.transport && a.confidence === b.confidence &&
      a.measured.length === b.measured.length && a.measured.every((t, i) => t === b.measured[i]) &&
      sameLocator(a.locator, b.locator, ignorePorts) &&
      a.measured_locators.length === b.measured_locators.length &&
      a.measured_locators.every((l, i) => sameLocator(l, b.measured_locators[i], ignorePorts)) &&
      a.warnings.length === b.warnings.length && a.warnings.every((w, i) => w === b.warnings[i]);
  }

  const byteOrder = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
  const compareKeys = (a, b) => byteOrder(a.topic, b.topic) || byteOrder(a.writer_guid, b.writer_guid) || byteOrder(a.reader_guid, b.reader_guid);

  /**
   * The pairs of a document keyed for comparison: by GUIDs, or by node names (`node#<n>`,
   * the n-th endpoint of that node on the topic in GUID order; `guid:<guid>` without a
   * node name), sorted like the C++ std::map so the output order matches the binary's.
   */
  function keyedPairs(doc, key) {
    const out = [];
    for (const t of doc.topics) {
      const byNode = new Map();
      for (const ep of [...t.writers, ...t.readers]) {
        if (ep.node) { if (!byNode.has(ep.node)) byNode.set(ep.node, []); byNode.get(ep.node).push(ep.guid); }
      }
      for (const guids of byNode.values()) guids.sort(byteOrder);
      const identity = (guid, node) => node ? `${node}#${byNode.get(node).indexOf(guid)}` : `guid:${guid}`;
      for (const p of t.pairs) {
        const real = pairKey(t, p);
        const ident = key === 'node'
          ? { topic: t.topic, writer_guid: identity(p.writer_guid, real.writer_node), reader_guid: identity(p.reader_guid, real.reader_node) }
          : real;
        out.push({ ident, key: real, state: pairState(p) });
      }
    }
    out.sort((a, b) => compareKeys(a.ident, b.ident));
    return out;
  }

  /**
   * Compare two documents: the `changes` object of `transport_viz diff --json` (`key` =
   * 'node', the default, or 'guid'). Added and changed pairs carry the after document's
   * GUIDs, removed pairs the before document's; changed_pairs[].from names the before GUIDs.
   */
  function diffDocuments(before, after, key = 'node') {
    const prev = new Map(keyedPairs(before, key).map(e => [keyId(e.ident), e]));
    const cur = keyedPairs(after, key);
    const changes = { key, before: { observed_at: before.observed_at, domain: before.domain }, added_pairs: [], removed_pairs: [], changed_pairs: [] };
    const seen = new Set();
    for (const e of cur) {
      const id = keyId(e.ident);
      seen.add(id);
      const p = prev.get(id);
      if (!p) changes.added_pairs.push(e.key);
      else if (!sameState(p.state, e.state, key === 'node')) {
        changes.changed_pairs.push({ ...e.key, from: { ...p.state, writer_guid: p.key.writer_guid, reader_guid: p.key.reader_guid }, to: e.state });
      }
    }
    for (const [id, p] of prev) if (!seen.has(id)) changes.removed_pairs.push(p.key);
    return changes;
  }

  /** What changed between two pair states, e.g. "transport SHM → UDPv4, warnings [] → [x]". */
  function changeText(from, to) {
    const loc = l => (l ? `${l.kind}${l.address ? ' ' + l.address : ''}:${l.port}` : 'none');
    const list = a => `[${a.join(', ')}]`;
    const parts = [];
    if (from.transport !== to.transport) parts.push(`transport ${from.transport} → ${to.transport}`);
    if (from.confidence !== to.confidence) parts.push(`confidence ${from.confidence} → ${to.confidence}`);
    if (from.measured.join() !== to.measured.join()) parts.push(`measured ${list(from.measured)} → ${list(to.measured)}`);
    if (loc(from.locator) !== loc(to.locator)) parts.push(`locator ${loc(from.locator)} → ${loc(to.locator)}`);
    const mls = a => a.map(loc).join(', ');
    if (mls(from.measured_locators) !== mls(to.measured_locators)) parts.push(`measured locators [${mls(from.measured_locators)}] → [${mls(to.measured_locators)}]`);
    if (from.warnings.join() !== to.warnings.join()) parts.push(`warnings ${list(from.warnings)} → ${list(to.warnings)}`);
    return parts.join(', ');
  }

  /** "+2 pairs  -1 pair  ~1 changed" like the CLI's `changes:` line, or "none". */
  function changesSummary(c) {
    if (!c) return '';
    const n = (list, sign, word) => list.length ? `${sign}${list.length} ${word}` : '';
    const parts = [
      n(c.added_pairs, '+', c.added_pairs.length === 1 ? 'pair' : 'pairs'),
      n(c.removed_pairs, '-', c.removed_pairs.length === 1 ? 'pair' : 'pairs'),
      n(c.changed_pairs, '~', 'changed'),
    ].filter(Boolean);
    return parts.length ? parts.join('  ') : 'none';
  }

  /**
   * Marks and ghosts of a `changes` object, as the viewer draws them: marks = Map(keyId ->
   * {mark: '+'|'~', from}), ghosts = the removed pairs, each with the transport and type it
   * had when `before` (the before document) is at hand.
   */
  function decorations(changes, before) {
    const marks = new Map();
    const ghosts = [];
    if (!changes) return { marks, ghosts };
    for (const k of changes.added_pairs) marks.set(keyId(k), { mark: '+', from: null });
    for (const c of changes.changed_pairs) marks.set(keyId(c), { mark: '~', from: c.from });
    const beforePairs = new Map();
    if (before) for (const t of before.topics) for (const p of t.pairs) beforePairs.set(keyId(pairKey(t, p)), { topic: t, pair: p });
    for (const k of changes.removed_pairs) {
      const was = beforePairs.get(keyId(k));
      ghosts.push({ key: k, id: `ghost|${keyId(k)}`, topic: was ? was.topic : null, pair: was ? was.pair : null });
    }
    return { marks, ghosts };
  }

  /**
   * Live mode: every mark and ghost stays `hold` frames after its change, like the CLI's
   * three frames. `prev` is the previous hold state (or null), `changes` the new frame's.
   */
  function holdChanges(prev, changes, before, hold = 3) {
    const marks = new Map();
    const ghosts = new Map();
    if (prev) {
      for (const [id, m] of prev.marks) if (m.ttl > 1) marks.set(id, { ...m, ttl: m.ttl - 1 });
      for (const [id, g] of prev.ghosts) if (g.ttl > 1) ghosts.set(id, { ...g, ttl: g.ttl - 1 });
    }
    const now = decorations(changes, before);
    for (const [id, m] of now.marks) { marks.set(id, { ...m, ttl: hold }); ghosts.delete(`ghost|${id}`); }
    for (const g of now.ghosts) { ghosts.set(g.id, { ...g, ttl: hold }); marks.delete(keyId(g.key)); }
    return { marks, ghosts };
  }

  /** Marks / ghosts of a hold state in the shape decorations() returns. */
  function heldDecorations(hold) {
    return hold ? { marks: hold.marks, ghosts: [...hold.ghosts.values()] } : { marks: new Map(), ghosts: [] };
  }

  /** The visible pairs that carry a mark (`changes only`). */
  function markedPairs(pairs, marks) {
    return pairs.filter(vp => marks.has(keyId(pairKey(vp.topic, vp.pair))));
  }

  /** Keep only the nodes that a visible pair or ghost touches (`changes only`). */
  function pruneNodes(model, pairs, ghostNodes) {
    const keep = new Set(ghostNodes);
    for (const vp of pairs) { keep.add(vp.writerNode); keep.add(vp.readerNode); }
    keepServers(model, keep);
    const nodes = new Map([...model.nodes].filter(([id]) => keep.has(id)));
    const hosts = model.hosts.map(h => ({ ...h, nodes: h.nodes.filter(n => keep.has(n.id)) })).filter(h => h.nodes.length);
    return { ...model, nodes, hosts };
  }

  /** A kept client keeps its Discovery Server in view (#86). */
  function keepServers(model, keep) {
    for (const id of [...keep]) {
      const n = model.nodes.get(id);
      if (n) for (const s of serversOf(n)) if (model.nodes.has(s)) keep.add(s);
    }
  }

  /** The `discovery{}` of a document as one line for the footer (#86), '' for SIMPLE discovery. */
  function discoveryText(doc) {
    const d = doc && doc.discovery;
    if (!d || !d.observer_protocol || d.observer_protocol === 'SIMPLE') return '';
    const locator = l => `${l.kind} ${l.address}:${l.port}`;
    let s = `observed as ${d.observer_protocol}`;
    if (d.easy_mode) s += ` (Easy Mode, ROS2_EASY_MODE=${d.easy_mode})`;
    else if (d.discovery_servers && d.discovery_servers.length) s += ` of ${d.discovery_servers.map(locator).join(', ')}`;
    return s;
  }

  return { TRANSPORTS, INTERNAL_TOPICS, UNKNOWN_NODE_NAME, isFoldedBufferCompanion, isInternalTopic, normalizeDocument, buildModel, isDiscoveryServer, isDiscoveryClient, serverNodeId, clientParticipants, serversOf, discoveryText, filterRegex, visiblePairs, visibleNodesModel, bundle, humanBytes, humanSeconds, measuredText, latencyText, rateText, rateTitle, lossText, topicLatencyText, topicLossText, groupPairsByTopic, compareCells, escapeHtml, codeListHtml, shmText, participantShmText, datasharingText, statsText,
    pairKey, keyId, pairState, sameState, diffDocuments, changeText, changesSummary, decorations, holdChanges, heldDecorations, markedPairs, pruneNodes };
});
