// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Static viewer for `transport_viz --json` documents (schema_version 1).
// Hosts are columns, ROS nodes are boxes, writer -> reader pairs are arrows
// colored by transport. No build step; d3 is used for SVG data joins and zoom.

/* global d3 */
(() => {
  'use strict';

  // pure model / formatting functions live in model.js, the scene (filters, layout, edge
  // geometry) in scene.js; both unit-tested under Node
  const { TRANSPORTS, isInternalTopic, normalizeDocument, buildModel, filterRegex, bundle,
    humanBytes, measuredText, latencyText, rateText, rateTitle, lossText, groupPairsByTopic, compareCells, escapeHtml, codeListHtml, shmText, participantShmText, datasharingText, statsText,
    pairKey, keyId, diffDocuments, changeText, changesSummary, decorations, holdChanges, heldDecorations } = globalThis.TransportVizModel;
  const { L, markOf, visibleScene, sceneEdges, edgeLabel, layout, edgeCurve, edgePathD, edgeMidpoint } = globalThis.TransportVizScene;
  const COLORS = {
    UDPv4: 'var(--c-udpv4)', UDPv6: 'var(--c-udpv6)', TCPv4: 'var(--c-tcp)', TCPv6: 'var(--c-tcp)',
    SHM: 'var(--c-shm)', DATA_SHARING: 'var(--c-ds)', NONE: 'var(--c-none)',
  };

  const state = {
    doc: null,
    model: null,   // buildModel(doc), once per document (#136)
    scene: null,   // what the last render drew; selection changes reuse it (#136)
    view: 'graph',
    filter: { topic: '', node: '', transports: new Set(TRANSPORTS), hideInternal: true },
    selection: null,   // {kind: 'node', id} | {kind: 'edge', id} | {kind: 'pair', id} | {kind: 'ghost', id}
    sort: { key: 'topic', asc: true },
    collapsed: new Set(),   // DDS topic names whose pair rows are folded under their header (#144)
    // comparison (transport_viz diff): `changes` from the document itself (a diff file or a
    // live frame) or computed here against `before`; `hold` ages live marks over 3 frames
    before: null, changes: null, hold: null, previousLive: null, key: 'node', changesOnly: false,
  };
  const HOLD_FRAMES = 3;

  /** Marks (Map keyId -> {mark, from}) and ghosts (removed pairs) to draw for the current document. */
  function currentDecorations() {
    if (state.hold) return heldDecorations(state.hold);
    return decorations(state.changes, state.before);
  }

  const MARK_CLASS = { '+': 'added', '~': 'changed', '-': 'removed', ' ': '' };
  const markHtml = m => (m === ' ' ? '' : `<span class="mark ${MARK_CLASS[m]}">${m}</span>`);

  /** The pairs, ghosts and node model to draw for the current filters. */
  function currentScene() {
    return visibleScene(state.model, state.filter, state.changesOnly, currentDecorations());
  }

  // ---------------------------------------------------------------- rendering: graph

  const svg = d3.select('#graph');
  const root = svg.append('g').attr('class', 'root');
  svg.append('defs').html(TRANSPORTS.map(t =>
    `<marker id="arrow-${t}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
       <path d="M0,0 L10,5 L0,10 z" fill="${COLORS[t]}"/></marker>`).join(''));
  svg.call(d3.zoom().scaleExtent([0.2, 3]).on('zoom', (ev) => root.attr('transform', ev.transform)));

  const edgeClass = d => `edge ${d.confidence === 'likely' ? 'likely' : ''} ${d.warn ? 'warn' : ''} ${MARK_CLASS[d.mark]} ${isSelected(d.ghost ? 'ghost' : 'edge', d.id) ? 'selected' : ''}`;
  const nodeClass = d => `node ${d.matched ? 'matched' : ''} ${isSelected('node', d.n.id) ? 'selected' : ''}`;
  const rowClass = d => (d.group ? `topic ${d.collapsed ? 'collapsed' : ''}` : `${MARK_CLASS[d.mark || ' ']} ${isSelected(d.ghost ? 'ghost' : 'pair', d.id) ? 'selected' : ''}`);

  function renderGraph(scene) {
    const edges = sceneEdges(scene);
    scene.edges = edges;   // the panel finds a selected edge here instead of bundling again
    const { model, matched } = scene;
    const { pos, hostBoxes } = layout(model);

    const hosts = root.selectAll('g.host').data(hostBoxes, d => d.host.label);
    const hostsEnter = hosts.enter().append('g').attr('class', 'host');
    hostsEnter.append('rect');
    hostsEnter.append('text').attr('class', 'label');
    hosts.exit().remove();
    const hostsAll = hostsEnter.merge(hosts);
    hostsAll.select('rect').attr('x', d => d.x).attr('y', d => d.y).attr('width', d => d.w).attr('height', d => d.h).attr('rx', 8);
    hostsAll.select('text.label').attr('x', d => d.x + L.hostPad).attr('y', d => d.y + L.hostPad + 6).text(d => `host: ${d.host.label}`);

    const edgeSel = root.selectAll('g.edge').data(edges, d => d.id);
    const edgeEnter = edgeSel.enter().append('g').attr('class', 'edge');
    edgeEnter.append('path').attr('class', 'halo');
    edgeEnter.append('path').attr('class', 'main');
    edgeEnter.append('path').attr('class', 'hit');
    edgeEnter.append('text');
    edgeSel.exit().remove();
    const edgesAll = edgeEnter.merge(edgeSel)
      .attr('class', edgeClass)
      .on('click', (ev, d) => { ev.stopPropagation(); select({ kind: d.ghost ? 'ghost' : 'edge', id: d.id }); });
    // the label sits at the curve's half-length point computed from the control points:
    // no getTotalLength / getPointAtLength, which forced one layout per arrow (#136)
    for (const e of edges) { e.curve = edgeCurve(pos.get(e.source), pos.get(e.target), e.k); e.mid = edgeMidpoint(e.curve); }
    edgesAll.selectAll('path').attr('d', d => edgePathD(d.curve));
    edgesAll.select('path.main').attr('stroke', d => COLORS[d.transport]).attr('marker-end', d => `url(#arrow-${d.ghost ? 'NONE' : d.transport})`);
    edgesAll.select('text').attr('x', d => d.mid.x).attr('y', d => d.mid.y - 6).attr('text-anchor', 'middle').text(edgeLabel);

    const hideInternal = state.filter.hideInternal;
    const unmatchedCount = n => n.unmatched.filter(u => !(hideInternal && isInternalTopic(u.topic))).length;
    const nodeData = [...model.nodes.values()].map(n => ({ n, p: pos.get(n.id), unmatched: unmatchedCount(n), matched: matched(n.id) }));
    const nodeSel = root.selectAll('g.node').data(nodeData, d => d.n.id);
    const nodeEnter = nodeSel.enter().append('g').attr('class', 'node');
    nodeEnter.append('rect');
    nodeEnter.append('text').attr('class', 'name');
    nodeEnter.append('text').attr('class', 'proc');
    nodeEnter.append('text').attr('class', 'unmatched');
    nodeSel.exit().remove();
    const nodesAll = nodeEnter.merge(nodeSel)
      .attr('class', nodeClass)
      .attr('transform', d => `translate(${d.p.x},${d.p.y})`)
      .on('click', (ev, d) => { ev.stopPropagation(); select({ kind: 'node', id: d.n.id }); });
    nodesAll.select('rect').attr('width', L.nodeW).attr('height', L.nodeH);
    nodesAll.select('text.name').attr('x', 10).attr('y', 19).text(d => d.n.name);
    nodesAll.select('text.proc').attr('x', 10).attr('y', 36).text(d => d.n.process ? `pid ${d.n.process}` : '');
    nodesAll.select('text.unmatched').attr('x', L.nodeW - 8).attr('y', 36).attr('text-anchor', 'end')
      .text(d => d.unmatched ? `+${d.unmatched} unmatched` : '');
  }

  svg.on('click', () => select(null));

  // ---------------------------------------------------------------- rendering: table

  // `get` is a pair row's cell text, `sort` its sort value when that is not the text
  // (numbers sort numerically, missing values last); `group` / `groupSort` the same for a
  // topic header row (#144), whose numbers come from the document's `topics[]` entry only.
  const meas = v => v.pair.measured;
  const COLUMNS = [
    { key: 'mark', label: '', get: v => (v.mark || ' '), diff: true, group: () => '' },
    { key: 'topic', label: 'Topic', get: v => v.topic.topic, group: g => g.name },
    { key: 'type', label: 'Type', get: v => v.topic.type, group: g => g.topic.type },
    { key: 'writer', label: 'Writer', get: v => `${v.pair.writer_node || v.writerNode}@${v.pair.writer_host}`,
      group: g => (g.pubs === null ? '' : `${g.pubs} pub${g.pubs === 1 ? '' : 's'}`), groupSort: g => g.pubs },
    { key: 'reader', label: 'Reader', get: v => `${v.pair.reader_node || v.readerNode}@${v.pair.reader_host}`,
      group: g => (g.subs === null ? '' : `${g.subs} sub${g.subs === 1 ? '' : 's'}`), groupSort: g => g.subs },
    { key: 'transport', label: 'Transport', get: v => v.pair.transport, group: g => g.transports.join(' ') },
    { key: 'confidence', label: 'Confidence', get: v => v.pair.confidence, group: () => '' },
    { key: 'latency', label: 'Latency', get: v => latencyText(meas(v)), sort: v => (meas(v) && meas(v).latency_s ? meas(v).latency_s.mean : null),
      group: g => g.latency, groupSort: g => g.latencyValue, groupTitle: g => (g.latency ? 'slowest pair, mean' : '') },
    { key: 'rate', label: 'Hz', get: v => rateText(meas(v)), sort: v => (meas(v) ? meas(v).delivered_per_s : null), title: v => rateTitle(meas(v)), group: () => '' },
    { key: 'loss', label: 'Loss', get: v => lossText(meas(v)), sort: v => (meas(v) && meas(v).reliability ? meas(v).reliability.lost_packets : null),
      group: g => g.loss, groupSort: g => g.lostValue },
    { key: 'measured', label: 'Measured', get: v => measuredText(meas(v)), group: () => '' },
    { key: 'reasons', label: 'Reasons', get: v => [...v.pair.reasons, ...v.pair.warnings.map(w => '!' + w)].join(', '), group: g => g.reasons },
  ];
  const sortValue = (c, v) => (c.sort ? c.sort(v) : c.get(v));
  const groupSortValue = (c, g) => (c.groupSort ? c.groupSort(g) : c.group(g));

  /** A removed pair as a table row: what the before document knew about it, else the key alone. */
  function ghostRow(g) {
    const p = g.pair;
    return { id: g.id, ghost: g, mark: '-', topic: g.topic || { topic: g.key.topic, type: '' },
      pair: p || { writer_node: g.key.writer_node, reader_node: g.key.reader_node, writer_host: '?', reader_host: '?',
        transport: '', confidence: '', reasons: [], warnings: [], measured: null },
      writerNode: g.key.writer_node, readerNode: g.key.reader_node };
  }

  /** Sorted pair rows and ghost rows of a topic group: pairs by the sort column, ghosts after them. */
  function groupRows(g, col) {
    const pairs = [...g.pairs].sort((a, b) => compareCells(sortValue(col, a), sortValue(col, b), state.sort.asc));
    return [...pairs, ...g.ghosts];
  }

  /** The Collapse all / Expand all button: collapse when any visible topic is expanded. */
  function renderCollapseButton(groups) {
    const anyExpanded = groups.some(g => !state.collapsed.has(g.key));
    d3.select('#collapse-all').text(anyExpanded ? 'Collapse all' : 'Expand all').on('click', () => {
      if (anyExpanded) for (const g of groups) state.collapsed.add(g.key);
      else for (const g of groups) state.collapsed.delete(g.key);
      render();
    });
  }

  /**
   * One header row per topic (#144) above its pair rows, the CLI's `--verbose` shape.
   * Topics sort by the column's aggregate, pairs within a topic by their own value; a
   * collapsed topic keeps its header only. Header numbers are the document's `topics[]`
   * aggregates, so they never change with the filters.
   */
  function renderTable(scene) {
    const hasChanges = !!(state.changes || state.hold);
    const columns = COLUMNS.filter(c => !c.diff || hasChanges);
    const pairRows = scene.pairs.map(vp => ({ ...vp, mark: markOf(vp, scene.marks), from: (scene.marks.get(keyId(pairKey(vp.topic, vp.pair))) || {}).from }));
    const col = COLUMNS.find(c => c.key === state.sort.key) || COLUMNS[1];
    const groups = groupPairsByTopic(pairRows, scene.ghosts.map(ghostRow), state.doc.topics)
      .sort((a, b) => compareCells(groupSortValue(col, a), groupSortValue(col, b), state.sort.asc) || a.name.localeCompare(b.name));
    renderCollapseButton(groups);
    const rows = [];
    for (const g of groups) {
      const collapsed = state.collapsed.has(g.key);
      rows.push({ id: g.id, group: g, collapsed, count: g.pairs.length + g.ghosts.length });
      if (!collapsed) rows.push(...groupRows(g, col));
    }
    const head = d3.select('#pairs-head').selectAll('th').data(columns, d => d.key);
    head.exit().remove();
    head.enter().append('th').merge(head)
      .text(d => `${d.label}${state.sort.key === d.key ? (state.sort.asc ? ' ▲' : ' ▼') : ''}`)
      .on('click', (ev, d) => { state.sort = { key: d.key, asc: state.sort.key === d.key ? !state.sort.asc : true }; render(); });
    const tr = d3.select('#pairs-body').selectAll('tr').data(rows, d => d.id);
    const trEnter = tr.enter().append('tr');
    tr.exit().remove();
    const trAll = trEnter.merge(tr)
      .attr('class', rowClass)
      .on('click', (ev, d) => {
        if (!d.group) { select({ kind: d.ghost ? 'ghost' : 'pair', id: d.id }); return; }
        if (d.collapsed) state.collapsed.delete(d.group.key); else state.collapsed.add(d.group.key);
        render();
      });
    trAll.order();
    const td = trAll.selectAll('td').data(d => columns.map(c => ({ c, v: d })));
    td.exit().remove();
    td.enter().append('td').merge(td)
      .attr('class', ({ c, v }) => (c.key === 'topic' ? (v.group ? 'topic-name' : 'indent') : null))
      .attr('title', ({ c, v }) => ((v.group ? c.groupTitle && c.groupTitle(v.group) : c.title && c.title(v)) || null))
      .html(({ c, v }) => {
        if (v.group) return topicCell(c, v);
        if (c.key === 'mark') return markHtml(v.mark || ' ');
        if (c.key === 'transport') {
          if (v.ghost) return v.pair.transport ? badge(v.pair) + ' <span class="muted">(removed)</span>' : '<span class="muted">(removed)</span>';
          return v.from && v.from.transport !== v.pair.transport ? `${badge(v.from)}<span class="arrow">→</span>${badge(v.pair)}` : badge(v.pair);
        }
        if (v.ghost && !v.pair.transport && ['confidence', 'latency', 'rate', 'loss', 'measured', 'reasons'].includes(c.key)) return '';
        return escapeHtml(c.get(v));
      });
  }

  /** A topic header row's cell: the chevron and name, a badge per transport, or the aggregate text. */
  function topicCell(c, row) {
    const g = row.group;
    if (c.key === 'topic') {
      return `<span class="chevron">${row.collapsed ? '▸' : '▾'}</span><b>${escapeHtml(g.name)}</b>` +
        (row.collapsed ? ` <span class="muted">(${row.count} pair${row.count === 1 ? '' : 's'})</span>` : '');
    }
    if (c.key === 'transport') return g.transports.map(t => `<span class="badge" style="background:${COLORS[t]}">${t}</span>`).join(' ');
    return escapeHtml(c.group(g));
  }

  // ---------------------------------------------------------------- rendering: panel

  const panel = d3.select('#panel');

  function badge(pair) {
    const t = pair.transport;
    const cls = `badge ${pair.confidence === 'likely' ? 'likely' : ''}`;
    return `<span class="${cls}" style="background:${COLORS[t]}">${t}${pair.confidence === 'likely' ? '?' : ''}</span>` +
      ((pair.warnings || []).length ? ' <span class="badge warn">!</span>' : '');
  }

  /** The change line of a pair card: "+ added", "~ changed: transport SHM → UDPv4", or ''. */
  function changeHtml(vp, marks) {
    const m = marks.get(keyId(pairKey(vp.topic, vp.pair)));
    if (!m) return '';
    if (m.mark === '+') return '<div class="change added"><span class="mark added">+</span> added</div>';
    const text = m.from ? changeText(m.from, {
      transport: vp.pair.transport, confidence: vp.pair.confidence, measured: (vp.pair.measured && vp.pair.measured.transports) || [],
      locator: vp.pair.locator, measured_locators: (vp.pair.measured && vp.pair.measured.locators) || [], warnings: vp.pair.warnings }) : '';
    return `<div class="change changed"><span class="mark changed">~</span> changed${text ? ': ' + escapeHtml(text) : ''}</div>`;
  }

  function ghostCard(g) {
    const k = g.key;
    const p = g.pair;
    return `<div class="pair"><div class="change removed"><span class="mark removed">-</span> removed</div>
      <div><b>${escapeHtml(k.topic)}</b>${g.topic ? ` <span class="muted">${escapeHtml(g.topic.type)}</span>` : ''}</div>
      ${p ? `<div style="margin:4px 0">${badge(p)} confidence ${p.confidence}</div>` : ''}
      <div>${escapeHtml(k.writer_node || 'guid:' + k.writer_guid)}${p ? '@' + escapeHtml(p.writer_host) : ''} → ${escapeHtml(k.reader_node || 'guid:' + k.reader_guid)}${p ? '@' + escapeHtml(p.reader_host) : ''}</div>
      ${p ? codeList(p.reasons, false) + codeList(p.warnings, true) : '<div class="muted">as it was before: open the before document with "Compare with…" to see it</div>'}
    </div>`;
  }

  function codeList(codes, warn) {
    return codeListHtml(codes, state.doc.reason_code_descriptions, state.doc.reason_code_remedies, warn);
  }

  /** `selected` is the pair's chosen locator (reader side only); it is marked in the list. */
  function locators(ep, selected) {
    const fmt = l => `${l.kind}${l.address ? ' ' + l.address : ''}:${l.port}`;
    const isSel = l => selected && l.kind === selected.kind && l.address === selected.address && l.port === selected.port;
    const mark = l => fmt(l) + (isSel(l) ? ' (selected)' : '');
    return escapeHtml([...ep.unicast_locators.map(mark), ...ep.multicast_locators.map(l => mark(l) + ' (multicast)')].join(', ')) || '—';
  }

  /** Non-default request/offer policies (deadline, liveliness, ownership, partitions). */
  function qosExtras(q) {
    const parts = [];
    if (typeof q.deadline_s === 'number') parts.push(`deadline ${q.deadline_s} s`);
    if (q.liveliness && q.liveliness !== 'AUTOMATIC') parts.push(`liveliness ${q.liveliness}`);
    if (typeof q.liveliness_lease_s === 'number') parts.push(`lease ${q.liveliness_lease_s} s`);
    if (q.ownership && q.ownership !== 'SHARED') parts.push(`ownership ${q.ownership}`);
    if (q.partitions && q.partitions.length) parts.push(`partitions [${q.partitions.join(', ')}]`);
    return parts.length ? ', ' + escapeHtml(parts.join(', ')) : '';
  }

  function endpointDetails(label, ep, selected) {
    return `<h3>${label}</h3><dl>
      <dt>node</dt><dd>${escapeHtml(ep.node || '(non-ROS participant)')}</dd>
      <dt>host</dt><dd>${escapeHtml(ep.host)}${ep.process ? ` (pid ${escapeHtml(ep.process)})` : ''}</dd>
      <dt>guid</dt><dd><code>${escapeHtml(ep.guid)}</code></dd>
      <dt>locators</dt><dd>${locators(ep, selected)}</dd>
      ${datasharingText(ep, label === 'Writer') ? `<dt>data-sharing</dt><dd>${escapeHtml(datasharingText(ep, label === 'Writer'))}</dd>` : ''}
      ${participantShmText(state.doc, ep.participant_guid_prefix) ? `<dt>shm</dt><dd>${escapeHtml(participantShmText(state.doc, ep.participant_guid_prefix))}</dd>` : ''}
      <dt>qos</dt><dd>${escapeHtml(ep.qos.reliability)}, ${escapeHtml(ep.qos.durability)}, data-sharing ${escapeHtml(ep.qos.data_sharing)}${ep.qos.data_sharing_domain_ids && ep.qos.data_sharing_domain_ids.length ? ` [${ep.qos.data_sharing_domain_ids.join(', ')}]` : ''}${qosExtras(ep.qos)}</dd>
    </dl>`;
  }

  function pairCard(vp, selected, marks) {
    const p = vp.pair;
    return `<div class="pair ${selected ? 'selected' : ''}">
      ${marks ? changeHtml(vp, marks) : ''}
      <div><b>${escapeHtml(vp.topic.topic)}</b> <span class="muted">${escapeHtml(vp.topic.type)}</span></div>
      <div style="margin:4px 0">${badge(p)} confidence ${p.confidence}${p.measured && p.measured.available ? ` · measured ${escapeHtml(measuredText(p.measured))}` : ''}${latencyText(p.measured) ? ` · latency ${escapeHtml(latencyText(p.measured))}` : ''}${rateText(p.measured) ? ` · <span title="${escapeHtml(rateTitle(p.measured))}">${escapeHtml(rateText(p.measured))} Hz</span>` : ''}${lossText(p.measured) ? ` · loss ${escapeHtml(lossText(p.measured))}` : ''}</div>
      ${p.measured && p.measured.reliability ? `<div class="muted">heartbeats ${p.measured.reliability.heartbeats}, gaps ${p.measured.reliability.gaps}, acknacks ${p.measured.reliability.acknacks}, nackfrags ${p.measured.reliability.nackfrags}</div>` : ''}
      <div>${escapeHtml(p.writer_node || vp.writerNode)}@${escapeHtml(p.writer_host)} → ${escapeHtml(p.reader_node || vp.readerNode)}@${escapeHtml(p.reader_host)}</div>
      ${codeList(p.reasons, false)}${codeList(p.warnings, true)}
      ${endpointDetails('Writer', vp.writer)}${endpointDetails('Reader', vp.reader, p.locator)}
    </div>`;
  }

  function renderPanel(scene) {
    const sel = state.selection;
    const model = state.model;
    if (!sel) { panel.html('<div class="panel-empty">Click a node or an edge for details.</div>'); return; }
    if (sel.kind === 'edge') {
      const e = (scene.edges || bundle(scene.pairs)).find(x => x.id === sel.id);
      if (!e) { select(null); return; }
      panel.html(`<h2>${escapeHtml(e.source)} → ${escapeHtml(e.target)}</h2><div>${e.pairs.length} pair(s), ${e.transport}${e.confidence === 'likely' ? ' (likely)' : ''}</div>` +
        e.pairs.map(vp => pairCard(vp, false, scene.marks)).join(''));
    } else if (sel.kind === 'ghost') {
      const g = scene.ghosts.find(x => x.id === sel.id);
      if (!g) { select(null); return; }
      panel.html(`<h2>${escapeHtml(g.key.writer_node || 'guid:' + g.key.writer_guid)} → ${escapeHtml(g.key.reader_node || 'guid:' + g.key.reader_guid)}</h2>${ghostCard(g)}`);
    } else if (sel.kind === 'pair') {
      const vp = model.pairs.find(x => x.id === sel.id);
      if (!vp) { select(null); return; }
      panel.html(`<h2>Pair</h2>${pairCard(vp, true, scene.marks)}`);
    } else if (sel.kind === 'node') {
      const n = model.nodes.get(sel.id);
      if (!n) { select(null); return; }
      const list = (items) => items.length ? `<dl>${items.map(({ topic, ep }) => `<dt>${escapeHtml(topic.topic)}</dt><dd>${escapeHtml(topic.type)}${typeof ep.datasharing_history_bytes === 'number' ? ` · data-sharing history ${humanBytes(ep.datasharing_history_bytes, 'B')}` : ''}</dd>`).join('')}</dl>` : '<div class="muted">none</div>';
      panel.html(`<h2>${escapeHtml(n.name)}</h2><dl><dt>host</dt><dd>${escapeHtml(n.host)}</dd>${n.process ? `<dt>pid</dt><dd>${escapeHtml(n.process)}</dd>` : ''}</dl>
        <h3>Publishers (${n.pubs.length})</h3>${list(n.pubs)}
        <h3>Subscriptions (${n.subs.length})</h3>${list(n.subs)}
        ${n.unmatched.length ? `<h3>Unmatched topics (${n.unmatched.length})</h3>` + n.unmatched.map(u => `<div><b>${escapeHtml(u.topic.topic)}</b>${codeList(u.reasons, false)}</div>`).join('') : ''}`);
    }
  }

  // ---------------------------------------------------------------- toolbar, legend, loading

  function renderToolbar() {
    const box = d3.select('#filter-transports');
    if (box.selectAll('label').empty()) {
      box.selectAll('label').data(TRANSPORTS).enter().append('label')
        .html(t => `<input type="checkbox" checked> <span style="color:${COLORS[t]};font-weight:600">${t}</span>`)
        .select('input').on('change', function (ev, t) { this.checked ? state.filter.transports.add(t) : state.filter.transports.delete(t); render(); });
    }
    d3.select('#legend').html(
      TRANSPORTS.map(t => `<span class="item"><span class="sw" style="border-top-color:${COLORS[t]}"></span>${t}</span>`).join('') +
      '<span class="item"><span class="sw dashed" style="border-top-color:#8b949e"></span>likely</span>' +
      '<span class="item"><span class="sw warn"></span>warning</span>' +
      (state.changes || state.hold ? '<span class="item"><span class="sw added"></span>+ added</span><span class="item"><span class="sw changed"></span>~ changed</span><span class="item"><span class="sw removed"></span>- removed</span>' : ''));
    const hasChanges = !!(state.changes || state.hold);
    d3.select('#changes-group').attr('hidden', hasChanges ? null : true);
    d3.select('#changes-summary').text(state.hold ? changesSummary(state.changes) : changesSummary(state.changes));
    d3.select('#diff-key-label').attr('hidden', state.before ? null : true);
    d3.select('#diff-key').property('value', state.key);
    d3.select('#filter-changes').property('checked', state.changesOnly);
  }

  function renderMeta() {
    const d = state.doc;
    if (!d) { d3.select('#meta').text('no document loaded'); return; }
    const n = d.topics.reduce((a, t) => a + t.pairs.length, 0);
    const c = state.changes;
    const vs = c && c.before ? ` · vs ${c.before.observed_at}${c.before.domain !== d.domain ? ` (domain ${c.before.domain})` : ''} by ${c.key || 'guid'} key` : '';
    d3.select('#meta').html(
      escapeHtml(`domain ${d.domain} · ${d.observed_at} · ${d.topics.length} topics, ${n} pairs · `) +
      statsText(d.stats, d.reason_code_descriptions, d.reason_code_remedies) + escapeHtml(vs));
    d3.select('#shm').html(shmText(d.shm, d.reason_code_descriptions, d.reason_code_remedies));
  }

  let renders = 0;
  /** Counts renders and selection updates for the scale harness (scripts/scale_viewer.js waits on it). */
  function rendered() { document.body.dataset.render = String(++renders); }

  function render() {
    renderMeta();
    renderToolbar();
    d3.selectAll('.tab').classed('active', function () { return this.dataset.view === state.view; });
    d3.select('#graph-view').attr('hidden', state.view === 'graph' ? null : true);
    d3.select('#table-view').attr('hidden', state.view === 'table' ? null : true);
    d3.select('#table-tools').attr('hidden', state.view === 'table' && state.doc ? null : true);
    if (!state.doc) { rendered(); return; }
    const scene = currentScene();
    state.scene = scene;
    if (state.view === 'graph') renderGraph(scene); else renderTable(scene);
    renderPanel(scene);
    rendered();
  }

  function isSelected(kind, id) { return state.selection && state.selection.kind === kind && state.selection.id === id; }

  /** A selection change touches only the `selected` classes and the panel, not the graph (#136). */
  function select(sel) {
    state.selection = sel;
    if (!state.scene) { render(); return; }
    root.selectAll('g.edge').attr('class', edgeClass);
    root.selectAll('g.node').attr('class', nodeClass);
    d3.select('#pairs-body').selectAll('tr').attr('class', rowClass);
    renderPanel(state.scene);
    rendered();
  }

  function isDocument(doc) { return doc && doc.schema_version === 1 && Array.isArray(doc.topics); }

  /**
   * Show `doc`. opts.before: compare against that document here (Compare with…, ?diff=);
   * opts.live: a frame of the live stream, whose own `changes` are held for a few frames.
   * Otherwise a `changes` object inside the document (a `transport_viz diff --json` file)
   * is shown as it is.
   */
  function setDocument(doc, sourceName, keepSelection, opts = {}) {
    if (!isDocument(doc)) {
      alert(`Not a transport_viz --json document (schema_version 1): ${sourceName}`);
      return;
    }
    normalizeDocument(doc);
    if (opts.before) normalizeDocument(opts.before);
    if (opts.live) {
      state.hold = holdChanges(state.hold, doc.changes || null, state.previousLive, HOLD_FRAMES);
      state.previousLive = doc;
      state.changes = doc.changes || null;
      state.before = null;
    } else if (opts.before) {
      state.hold = null;
      state.before = opts.before;
      state.changes = diffDocuments(opts.before, doc, state.key);
    } else {
      state.hold = null;
      state.before = null;
      state.changes = doc.changes && Array.isArray(doc.changes.added_pairs) ? doc.changes : null;
    }
    if (!state.changes && !state.hold) state.changesOnly = false;
    state.doc = doc;
    state.model = buildModel(doc);
    if (!keepSelection) state.selection = null;   // live updates keep the selection; render() drops it if gone
    render();
    document.title = `transport_viz viewer – ${sourceName}`;
  }

  /** Compare the loaded document with `doc` (which becomes the one shown). */
  function compareWith(doc, sourceName) {
    if (!state.doc) { setDocument(doc, sourceName); return; }
    setDocument(doc, `${document.title.replace(/^transport_viz viewer – /, '')} vs ${sourceName}`, false, { before: state.doc });
  }

  // ---------------------------------------------------------------- live mode (serve.py / transport_viz_web)

  const live = { es: null, paused: false, pending: null, updates: 0 };

  function liveStatus(cls, text) {
    const el = d3.select('#live').attr('hidden', null).attr('class', `live ${cls}`);
    el.select('#live-text').text(text);
  }

  function connectLive() {
    live.es = new EventSource('events');
    liveStatus('connecting', 'live: connecting…');
    live.es.addEventListener('document', (e) => {
      let doc;
      try { doc = JSON.parse(e.data); } catch (err) { console.error('live: bad document', err); return; }
      live.updates++;
      if (live.paused) { live.pending = doc; liveStatus('paused', `live: paused (${live.updates} updates, newest ${doc.observed_at})`); return; }
      setDocument(doc, 'live', true, { live: true });
      liveStatus('', `live: updated ${doc.observed_at} (#${live.updates})`);
    });
    live.es.addEventListener('status', (e) => {
      const st = JSON.parse(e.data);
      liveStatus('ended', `live: ${st.message || st.state}`);
      live.es.close();
    });
    live.es.onerror = () => { if (live.es.readyState !== EventSource.CLOSED) liveStatus('reconnecting', 'live: connection lost, reconnecting…'); };
    d3.select('#live-pause').on('click', function () {
      live.paused = !live.paused;
      this.textContent = live.paused ? 'Resume' : 'Pause';
      if (!live.paused && live.pending) { setDocument(live.pending, 'live', true, { live: true }); live.pending = null; }
      liveStatus(live.paused ? 'paused' : '', live.paused ? 'live: paused' : 'live: resumed');
    });
  }

  function loadFile(file, compare) {
    const reader = new FileReader();
    reader.onload = () => {
      let doc;
      try { doc = JSON.parse(reader.result); } catch (e) { alert(`Invalid JSON: ${e.message}`); return; }
      if (compare) compareWith(doc, file.name); else setDocument(doc, file.name);
    };
    reader.readAsText(file);
  }

  function fetchDocument(url) {
    return fetch(url).then(r => { if (!r.ok) throw new Error(`${r.status} ${r.statusText}`); return r.json(); });
  }

  function loadFailed(url, e) {
    d3.select('#meta').text(`failed to load ${url}: ${e.message} (fetch does not work from file://; use "Open JSON…")`);
  }

  function loadUrl(url) {
    fetchDocument(url).then(doc => setDocument(doc, url)).catch(e => loadFailed(url, e));
  }

  /** ?src=before&diff=after: both fetched, then compared like `transport_viz diff before after`. */
  function loadDiffUrls(beforeUrl, afterUrl) {
    Promise.all([fetchDocument(beforeUrl), fetchDocument(afterUrl)])
      .then(([b, a]) => {
        if (!isDocument(b)) throw new Error(`${beforeUrl} is not a transport_viz --json document`);
        setDocument(a, `${beforeUrl} vs ${afterUrl}`, false, { before: b });
      })
      .catch(e => loadFailed(`${beforeUrl} / ${afterUrl}`, e));
  }

  // wiring
  d3.select('#file').on('change', function () { if (this.files[0]) loadFile(this.files[0]); this.value = ''; });
  d3.select('#file-diff').on('change', function () { if (this.files[0]) loadFile(this.files[0], true); this.value = ''; });
  d3.select('#filter-changes').on('change', function () { state.changesOnly = this.checked; render(); });
  d3.select('#diff-key').on('change', function () {
    state.key = this.value;
    if (state.before) { state.changes = diffDocuments(state.before, state.doc, state.key); render(); }
  });
  d3.select('#load-sample').on('click', () => loadSample());
  d3.selectAll('.tab').on('click', function () { state.view = this.dataset.view; render(); });
  // typing re-renders once per pause of FILTER_DEBOUNCE_MS, or at once on Enter / blur (#136)
  const FILTER_DEBOUNCE_MS = 100;
  let filterTimer = null;
  const flushFilter = () => { if (filterTimer) { clearTimeout(filterTimer); filterTimer = null; render(); } };
  const onFilterInput = (key) => function () {
    state.filter[key] = this.value;
    this.classList.toggle('invalid', !!this.value && filterRegex(this.value) === null);
    clearTimeout(filterTimer);
    filterTimer = setTimeout(() => { filterTimer = null; render(); }, FILTER_DEBOUNCE_MS);
  };
  for (const key of ['topic', 'node']) {
    d3.select(`#filter-${key}`).on('input', onFilterInput(key))
      .on('blur', flushFilter)
      .on('keydown', (ev) => { if (ev.key === 'Enter') flushFilter(); });
  }
  d3.select('#filter-internal').on('change', function () { state.filter.hideInternal = this.checked; render(); });
  const overlay = document.getElementById('drop-overlay');
  let dragDepth = 0;
  document.addEventListener('dragenter', (e) => { e.preventDefault(); dragDepth++; overlay.hidden = false; });
  document.addEventListener('dragleave', () => { if (--dragDepth <= 0) { dragDepth = 0; overlay.hidden = true; } });
  document.addEventListener('dragover', (e) => e.preventDefault());
  document.addEventListener('drop', (e) => { e.preventDefault(); dragDepth = 0; overlay.hidden = true; if (e.dataTransfer.files[0]) loadFile(e.dataTransfer.files[0]); });

  function loadSample() {
    // sample/sample.js embeds sample.json so this also works from file://
    if (window.TRANSPORT_VIZ_SAMPLE) setDocument(window.TRANSPORT_VIZ_SAMPLE, 'sample/sample.json');
    else loadUrl('sample/sample.json');
  }

  render();
  const params = new URLSearchParams(location.search);
  if (params.get('key') === 'guid') state.key = 'guid';
  if (params.get('live')) connectLive();
  else if (params.get('src') && params.get('diff')) loadDiffUrls(params.get('src'), params.get('diff'));
  else if (params.get('src')) loadUrl(params.get('src'));
  else loadSample();
})();
