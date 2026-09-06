// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Static viewer for `transport_viz --json` documents (schema_version 1).
// Hosts are columns, ROS nodes are boxes, writer -> reader pairs are arrows
// colored by transport. No build step; d3 is used for SVG data joins and zoom.

/* global d3 */
(() => {
  'use strict';

  // pure model / formatting functions live in model.js (unit-tested under Node)
  const { TRANSPORTS, INTERNAL_TOPICS, buildModel, filterRegex, visiblePairs, visibleNodesModel, bundle,
    humanBytes, measuredText, rateText, escapeHtml, shmText } = globalThis.TransportVizModel;
  const COLORS = {
    UDPv4: 'var(--c-udpv4)', UDPv6: 'var(--c-udpv6)', TCPv4: 'var(--c-tcp)', TCPv6: 'var(--c-tcp)',
    SHM: 'var(--c-shm)', DATA_SHARING: 'var(--c-ds)', NONE: 'var(--c-none)',
  };

  const state = {
    doc: null,
    view: 'graph',
    filter: { topic: '', node: '', transports: new Set(TRANSPORTS), hideInternal: true },
    selection: null,   // {kind: 'node', id} | {kind: 'edge', id} | {kind: 'pair', id}
    sort: { key: 'topic', asc: true },
  };

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

  /** Path between two node boxes; `k` spreads parallel edges apart. */
  function edgePath(a, b, k) {
    const spread = k * 14;
    if (a === b) {
      const x = a.x + a.w, y = a.y + a.h / 2 + spread;
      return `M${x},${y - 8} C${x + 50},${y - 30} ${x + 50},${y + 30} ${x},${y + 8}`;
    }
    const ac = { x: a.x + a.w / 2, y: a.y + a.h / 2 };
    const bc = { x: b.x + b.w / 2, y: b.y + b.h / 2 };
    const sameColumn = Math.abs(ac.x - bc.x) < 1;
    if (sameColumn) {
      const x = a.x + a.w, dir = 1;
      const mid = (ac.y + bc.y) / 2;
      const bulge = 40 + Math.abs(ac.y - bc.y) * 0.15 + spread;
      return `M${x},${ac.y + spread * 0.3} C${x + bulge * dir},${ac.y} ${x + bulge * dir},${bc.y} ${x},${bc.y - spread * 0.3}`;
    }
    const leftToRight = ac.x < bc.x;
    const sx = leftToRight ? a.x + a.w : a.x;
    const tx = leftToRight ? b.x : b.x + b.w;
    const sy = ac.y + spread, ty = bc.y + spread;
    const dx = (tx - sx) * 0.5;
    return `M${sx},${sy} C${sx + dx},${sy} ${tx - dx},${ty} ${tx},${ty}`;
  }

  // ---------------------------------------------------------------- rendering: graph

  const svg = d3.select('#graph');
  const root = svg.append('g').attr('class', 'root');
  svg.append('defs').html(TRANSPORTS.map(t =>
    `<marker id="arrow-${t}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
       <path d="M0,0 L10,5 L0,10 z" fill="${COLORS[t]}"/></marker>`).join(''));
  svg.call(d3.zoom().scaleExtent([0.2, 3]).on('zoom', (ev) => root.attr('transform', ev.transform)));

  function renderGraph(fullModel) {
    const pairs = visiblePairs(fullModel, state.filter);
    const edges = bundle(pairs);
    const { model, matched } = visibleNodesModel(fullModel, pairs, state.filter.node);
    const { pos, hostBoxes } = layout(model);

    // parallel-edge index per (source,target) so bundles do not overlap
    const groups = new Map();
    for (const e of edges) {
      const g = [e.source, e.target].join('→');
      if (!groups.has(g)) groups.set(g, []);
      groups.get(g).push(e);
    }
    for (const list of groups.values()) list.forEach((e, i) => { e.k = i - (list.length - 1) / 2; });

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
      .attr('class', d => `edge ${d.confidence === 'likely' ? 'likely' : ''} ${d.warn ? 'warn' : ''} ${isSelected('edge', d.id) ? 'selected' : ''}`)
      .on('click', (ev, d) => { ev.stopPropagation(); select({ kind: 'edge', id: d.id }); });
    edgesAll.selectAll('path').attr('d', d => edgePath(pos.get(d.source), pos.get(d.target), d.k));
    edgesAll.select('path.main').attr('stroke', d => COLORS[d.transport]).attr('marker-end', d => `url(#arrow-${d.transport})`);
    edgesAll.select('text').each(function (d) {
      const p = this.parentNode.querySelector('path.main');
      const len = p.getTotalLength();
      const pt = p.getPointAtLength(len * 0.5);
      d3.select(this).attr('x', pt.x).attr('y', pt.y - 6).attr('text-anchor', 'middle')
        .text(d.pairs.length === 1 ? `${d.pairs[0].topic.topic} · ${d.transport}${d.confidence === 'likely' ? '?' : ''}` : `${d.pairs.length} topics · ${d.transport}${d.confidence === 'likely' ? '?' : ''}`);
    });

    const hideInternal = state.filter.hideInternal;
    const unmatchedCount = n => n.unmatched.filter(u => !(hideInternal && INTERNAL_TOPICS.has(u.topic.topic))).length;
    const nodeData = [...model.nodes.values()].map(n => ({ n, p: pos.get(n.id), unmatched: unmatchedCount(n), matched: matched(n.id) }));
    const nodeSel = root.selectAll('g.node').data(nodeData, d => d.n.id);
    const nodeEnter = nodeSel.enter().append('g').attr('class', 'node');
    nodeEnter.append('rect');
    nodeEnter.append('text').attr('class', 'name');
    nodeEnter.append('text').attr('class', 'proc');
    nodeEnter.append('text').attr('class', 'unmatched');
    nodeSel.exit().remove();
    const nodesAll = nodeEnter.merge(nodeSel)
      .attr('class', d => `node ${d.matched ? 'matched' : ''} ${isSelected('node', d.n.id) ? 'selected' : ''}`)
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

  const COLUMNS = [
    { key: 'topic', label: 'Topic', get: v => v.topic.topic },
    { key: 'type', label: 'Type', get: v => v.topic.type },
    { key: 'writer', label: 'Writer', get: v => `${v.pair.writer_node || v.writerNode}@${v.pair.writer_host}` },
    { key: 'reader', label: 'Reader', get: v => `${v.pair.reader_node || v.readerNode}@${v.pair.reader_host}` },
    { key: 'transport', label: 'Transport', get: v => v.pair.transport },
    { key: 'confidence', label: 'Confidence', get: v => v.pair.confidence },
    { key: 'rate', label: 'Rate', get: v => rateText(v.pair.measured) },
    { key: 'measured', label: 'Measured', get: v => measuredText(v.pair.measured) },
    { key: 'reasons', label: 'Reasons', get: v => [...v.pair.reasons, ...v.pair.warnings.map(w => '!' + w)].join(', ') },
  ];

  function renderTable(model) {
    const rows = visiblePairs(model, state.filter);
    const col = COLUMNS.find(c => c.key === state.sort.key);
    rows.sort((a, b) => col.get(a).localeCompare(col.get(b)) * (state.sort.asc ? 1 : -1));
    const head = d3.select('#pairs-head').selectAll('th').data(COLUMNS, d => d.key);
    head.enter().append('th').merge(head)
      .text(d => `${d.label}${state.sort.key === d.key ? (state.sort.asc ? ' ▲' : ' ▼') : ''}`)
      .on('click', (ev, d) => { state.sort = { key: d.key, asc: state.sort.key === d.key ? !state.sort.asc : true }; render(); });
    const tr = d3.select('#pairs-body').selectAll('tr').data(rows, d => d.id);
    const trEnter = tr.enter().append('tr');
    tr.exit().remove();
    const trAll = trEnter.merge(tr)
      .attr('class', d => (isSelected('pair', d.id) ? 'selected' : ''))
      .on('click', (ev, d) => select({ kind: 'pair', id: d.id }));
    trAll.order();
    const td = trAll.selectAll('td').data(d => COLUMNS.map(c => ({ c, v: d })));
    td.enter().append('td').merge(td).html(({ c, v }) => {
      if (c.key === 'transport') return badge(v.pair);
      return escapeHtml(c.get(v));
    });
  }

  // ---------------------------------------------------------------- rendering: panel

  const panel = d3.select('#panel');

  function badge(pair) {
    const t = pair.transport;
    const cls = `badge ${pair.confidence === 'likely' ? 'likely' : ''}`;
    return `<span class="${cls}" style="background:${COLORS[t]}">${t}${pair.confidence === 'likely' ? '?' : ''}</span>` +
      (pair.warnings.length ? ' <span class="badge warn">!</span>' : '');
  }

  function codeList(codes, warn) {
    const desc = state.doc.reason_code_descriptions || {};
    return codes.map(c => `<span class="code ${warn ? 'warn' : ''}"><b>${warn ? '!' : ''}${escapeHtml(c)}</b><span class="desc">${escapeHtml(desc[c] || '')}</span></span>`).join('');
  }

  function locators(ep) {
    const fmt = l => `${l.kind}${l.address ? ' ' + l.address : ''}:${l.port}`;
    return escapeHtml([...ep.unicast_locators.map(fmt), ...ep.multicast_locators.map(l => fmt(l) + ' (multicast)')].join(', ')) || '—';
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

  function endpointDetails(label, ep, pairSide) {
    return `<h3>${label}</h3><dl>
      <dt>node</dt><dd>${escapeHtml(ep.node || '(non-ROS participant)')}</dd>
      <dt>host</dt><dd>${escapeHtml(ep.host)}${ep.process ? ` (pid ${escapeHtml(ep.process)})` : ''}</dd>
      <dt>guid</dt><dd><code>${escapeHtml(ep.guid)}</code></dd>
      <dt>locators</dt><dd>${locators(ep)}</dd>
      ${typeof ep.datasharing_history_bytes === 'number' ? `<dt>data-sharing history</dt><dd>${humanBytes(ep.datasharing_history_bytes, 'B')} in /dev/shm</dd>` : ''}
      <dt>qos</dt><dd>${escapeHtml(ep.qos.reliability)}, ${escapeHtml(ep.qos.durability)}, data-sharing ${escapeHtml(ep.qos.data_sharing)}${ep.qos.data_sharing_domain_ids && ep.qos.data_sharing_domain_ids.length ? ` [${ep.qos.data_sharing_domain_ids.join(', ')}]` : ''}${qosExtras(ep.qos)}</dd>
    </dl>`;
  }

  function pairCard(vp, selected) {
    const p = vp.pair;
    return `<div class="pair ${selected ? 'selected' : ''}">
      <div><b>${escapeHtml(vp.topic.topic)}</b> <span class="muted">${escapeHtml(vp.topic.type)}</span></div>
      <div style="margin:4px 0">${badge(p)} confidence ${p.confidence}${p.measured && p.measured.available ? ` · measured ${escapeHtml(measuredText(p.measured))}` : ''}${rateText(p.measured) ? ` · rate ${escapeHtml(rateText(p.measured))}` : ''}</div>
      <div>${escapeHtml(p.writer_node || vp.writerNode)}@${escapeHtml(p.writer_host)} → ${escapeHtml(p.reader_node || vp.readerNode)}@${escapeHtml(p.reader_host)}</div>
      ${codeList(p.reasons, false)}${codeList(p.warnings, true)}
      ${endpointDetails('Writer', vp.writer)}${endpointDetails('Reader', vp.reader)}
    </div>`;
  }

  function renderPanel(model) {
    const sel = state.selection;
    if (!sel) { panel.html('<div class="panel-empty">Click a node or an edge for details.</div>'); return; }
    if (sel.kind === 'edge') {
      const e = bundle(visiblePairs(model, state.filter)).find(x => x.id === sel.id);
      if (!e) { select(null); return; }
      panel.html(`<h2>${escapeHtml(e.source)} → ${escapeHtml(e.target)}</h2><div>${e.pairs.length} pair(s), ${e.transport}${e.confidence === 'likely' ? ' (likely)' : ''}</div>` +
        e.pairs.map(vp => pairCard(vp, false)).join(''));
    } else if (sel.kind === 'pair') {
      const vp = model.pairs.find(x => x.id === sel.id);
      if (!vp) { select(null); return; }
      panel.html(`<h2>Pair</h2>${pairCard(vp, true)}`);
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
      '<span class="item"><span class="sw warn"></span>warning</span>');
  }

  function renderMeta() {
    const d = state.doc;
    if (!d) { d3.select('#meta').text('no document loaded'); return; }
    const n = d.topics.reduce((a, t) => a + t.pairs.length, 0);
    d3.select('#meta').text(`domain ${d.domain} · ${d.observed_at} · ${d.topics.length} topics, ${n} pairs · ` +
      (d.stats && d.stats.enabled ? `statistics: ${d.stats.samples} samples` : 'no statistics'));
    d3.select('#shm').html(shmText(d.shm, d.reason_code_descriptions));
  }

  function render() {
    renderMeta();
    renderToolbar();
    d3.selectAll('.tab').classed('active', function () { return this.dataset.view === state.view; });
    d3.select('#graph-view').attr('hidden', state.view === 'graph' ? null : true);
    d3.select('#table-view').attr('hidden', state.view === 'table' ? null : true);
    if (!state.doc) return;
    const model = buildModel(state.doc);
    if (state.view === 'graph') renderGraph(model); else renderTable(model);
    renderPanel(model);
  }

  function isSelected(kind, id) { return state.selection && state.selection.kind === kind && state.selection.id === id; }
  function select(sel) { state.selection = sel; render(); }

  function setDocument(doc, sourceName, keepSelection) {
    if (!doc || doc.schema_version !== 1 || !Array.isArray(doc.topics)) {
      alert(`Not a transport_viz --json document (schema_version 1): ${sourceName}`);
      return;
    }
    state.doc = doc;
    if (!keepSelection) state.selection = null;   // live updates keep the selection; render() drops it if gone
    render();
    document.title = `transport_viz viewer – ${sourceName}`;
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
      setDocument(doc, 'live', true);
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
      if (!live.paused && live.pending) { setDocument(live.pending, 'live', true); live.pending = null; }
      liveStatus(live.paused ? 'paused' : '', live.paused ? 'live: paused' : 'live: resumed');
    });
  }

  function loadFile(file) {
    const reader = new FileReader();
    reader.onload = () => { try { setDocument(JSON.parse(reader.result), file.name); } catch (e) { alert(`Invalid JSON: ${e.message}`); } };
    reader.readAsText(file);
  }

  function loadUrl(url) {
    fetch(url).then(r => { if (!r.ok) throw new Error(`${r.status} ${r.statusText}`); return r.json(); })
      .then(doc => setDocument(doc, url))
      .catch(e => { d3.select('#meta').text(`failed to load ${url}: ${e.message} (fetch does not work from file://; use "Open JSON…")`); });
  }

  // wiring
  d3.select('#file').on('change', function () { if (this.files[0]) loadFile(this.files[0]); this.value = ''; });
  d3.select('#load-sample').on('click', () => loadSample());
  d3.selectAll('.tab').on('click', function () { state.view = this.dataset.view; render(); });
  const onFilterInput = (key) => function () {
    state.filter[key] = this.value;
    this.classList.toggle('invalid', !!this.value && filterRegex(this.value) === null);
    render();
  };
  d3.select('#filter-topic').on('input', onFilterInput('topic'));
  d3.select('#filter-node').on('input', onFilterInput('node'));
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
  if (params.get('live')) connectLive();
  else if (params.get('src')) loadUrl(params.get('src'));
  else loadSample();
})();
