// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Pure recording functions of the web viewer (#82): a recording is the JSON Lines of
// `transport_viz --watch --json` (or `transport_viz_web --record`), one existing
// schema_version 1 document per line. The viewer keeps the byte range of every frame and
// a few numbers per pair and frame, never the documents themselves: a frame is parsed
// again when it is shown. No DOM, no d3, so this runs under Node for the unit tests
// (web/test/replay.test.js) and in the browser as globalThis.TransportVizReplay.

(function (root, factory) {
  const model = typeof module !== 'undefined' && module.exports ? require('./model.js') : root.TransportVizModel;
  const api = factory(model);
  root.TransportVizReplay = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, (M) => {
  'use strict';
  const { TRANSPORTS, keyId, forEachIdent } = M;

  /**
   * Splits byte chunks into lines. push(bytes, base) takes the next chunk, which starts at
   * file offset `base`, and returns the complete lines in it as {start, end, bytes} (end
   * excludes the newline); finish() returns the last line when the file does not end in one.
   */
  function lineSplitter() {
    let carry = null;       // bytes of a line that started in an earlier chunk
    let carryStart = 0;
    const joined = (tail) => {
      if (!carry) return tail;
      const all = new Uint8Array(carry.length + tail.length);
      all.set(carry);
      all.set(tail, carry.length);
      return all;
    };
    return {
      push(bytes, base) {
        const lines = [];
        let from = 0;
        for (let i = bytes.indexOf(10); i !== -1; i = bytes.indexOf(10, i + 1)) {
          const start = carry ? carryStart : base + from;
          lines.push({ start, end: base + i, bytes: joined(bytes.subarray(from, i)) });
          carry = null;
          from = i + 1;
        }
        if (from < bytes.length) {
          if (!carry) carryStart = base + from;
          carry = joined(bytes.slice(from));
        }
        return lines;
      },
      finish() {
        if (!carry) return [];
        const line = { start: carryStart, end: carryStart + carry.length, bytes: carry };
        carry = null;
        return [line];
      },
    };
  }

  /** A transport_viz --json document (schema_version 1), or null for any other line. */
  function parseFrame(text) {
    const s = text.trim();
    if (!s) return null;
    let doc;
    try { doc = JSON.parse(s); } catch { return null; }
    return doc && doc.schema_version === 1 && Array.isArray(doc.topics) ? doc : null;
  }

  // transport codes of the strip: 0 = the pair is not in that frame
  const transportCode = t => TRANSPORTS.indexOf(t) + 1;
  const transportOf = c => (c ? TRANSPORTS[c - 1] : null);

  /** FNV-1a of a string: tells two frames' GUIDs apart without keeping them. */
  function hash32(str) {
    let h = 0x811c9dc5;
    for (let i = 0; i < str.length; ++i) { h ^= str.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; }
    return h;
  }

  function grow(a, n, fill) {
    const b = new a.constructor(n);
    b.set(a);
    if (fill !== undefined) b.fill(fill, a.length);
    return b;
  }

  /**
   * An empty recording whose series are keyed like `transport_viz diff` (`key` = 'node' or
   * 'guid'): with 'node' a restarted node's new pair continues the series of the old one.
   */
  function createRecording(key = 'node') {
    return { key, frames: 0, starts: [], ends: [], observedAt: [], changed: [], series: new Map(), capacity: 16 };
  }

  // every series is rec.capacity long, so a pair gone for good reads absent (0 / NaN) to the end
  function newSeries(rec) {
    const n = rec.capacity;
    return {
      transport: new Uint8Array(n),
      guids: new Uint32Array(n),   // hash of the pair's real GUIDs: counters restart with them
      hz: new Float32Array(n).fill(NaN),
      // cumulative values stay doubles: the latency of a frame is a difference of two of them
      latencyMean: new Float64Array(n).fill(NaN),
      latencySamples: new Float64Array(n).fill(NaN),
      lost: new Float64Array(n).fill(NaN),
    };
  }

  function growAll(rec, n) {
    rec.capacity = n;
    for (const s of rec.series.values()) {
      s.transport = grow(s.transport, n);
      s.guids = grow(s.guids, n);
      s.hz = grow(s.hz, n, NaN);
      s.latencyMean = grow(s.latencyMean, n, NaN);
      s.latencySamples = grow(s.latencySamples, n, NaN);
      s.lost = grow(s.lost, n, NaN);
    }
  }

  const num = v => (typeof v === 'number' && Number.isFinite(v) ? v : NaN);

  /** Append one frame (a parsed document found at bytes [start, end) of the file). */
  function addFrame(rec, doc, start, end) {
    const i = rec.frames;
    rec.starts.push(start);
    rec.ends.push(end);
    rec.observedAt.push(String(doc.observed_at || ''));
    const c = doc.changes;
    rec.changed.push(!!(c && ((c.added_pairs || []).length || (c.removed_pairs || []).length || (c.changed_pairs || []).length)));
    if (i >= rec.capacity) growAll(rec, rec.capacity * 2);
    forEachIdent(doc, rec.key, (t, p, real, ident) => {
      const id = keyId(ident);
      let s = rec.series.get(id);
      if (!s) { s = newSeries(rec); rec.series.set(id, s); }
      s.transport[i] = transportCode(p.transport);
      s.guids[i] = hash32(keyId(real));
      const m = p.measured;
      if (m) {
        s.hz[i] = num(m.delivered_per_s);
        if (m.latency_s) { s.latencyMean[i] = num(m.latency_s.mean); s.latencySamples[i] = num(m.latency_s.samples); }
        if (m.reliability) s.lost[i] = num(m.reliability.lost_packets);
      }
    });
    rec.frames = i + 1;
  }

  /** Ident id (the series key) of every pair of `doc`, by keyId of its real key. */
  function identsOf(doc, key) {
    const out = new Map();
    forEachIdent(doc, key, (t, p, real, ident) => out.set(keyId(real), keyId(ident)));
    return out;
  }

  // the previous frame's counters continue into frame i: same pair, same GUIDs
  const continues = (s, i) => i > 0 && s.transport[i - 1] !== 0 && s.guids[i - 1] === s.guids[i];

  /**
   * The mean latency of each frame's own interval, in seconds: `latency_s` is a mean over
   * the whole observation under --watch, so the frame's share is the difference of
   * mean x samples of two frames over the difference of their samples. A frame whose
   * previous frame lacks the pair, or had other GUIDs behind the same node key (a
   * restart, whose counters start over), takes its own mean; a frame without new samples
   * is NaN.
   */
  function frameLatency(s, n) {
    const out = new Float64Array(n).fill(NaN);
    for (let i = 0; i < n; ++i) {
      const m = s.latencyMean[i];
      const k = s.latencySamples[i];
      if (Number.isNaN(m) || Number.isNaN(k)) continue;
      const pm = continues(s, i) ? s.latencyMean[i - 1] : NaN;
      const pk = continues(s, i) ? s.latencySamples[i - 1] : NaN;
      if (Number.isNaN(pm) || Number.isNaN(pk) || k < pk) { if (k > 0) out[i] = m; continue; }
      if (k > pk) out[i] = (m * k - pm * pk) / (k - pk);
    }
    return out;
  }

  /** Packets lost during each frame's interval: the difference of the cumulative counts. */
  function frameLoss(s, n) {
    const out = new Float64Array(n).fill(NaN);
    for (let i = 0; i < n; ++i) {
      const v = s.lost[i];
      if (Number.isNaN(v)) continue;
      const pv = continues(s, i) ? s.lost[i - 1] : NaN;
      out[i] = Number.isNaN(pv) || v < pv ? v : v - pv;
    }
    return out;
  }

  /** Whether a series has any finite value in its first n entries. */
  function hasValues(a, n) {
    for (let i = 0; i < n; ++i) if (!Number.isNaN(a[i])) return true;
    return false;
  }

  /** Runs of one transport over the frames: [{from, to (exclusive), transport | null}]. */
  function transportRuns(s, n) {
    const runs = [];
    for (let i = 0; i < n; ++i) {
      const t = transportOf(s.transport[i]);
      const last = runs[runs.length - 1];
      if (last && last.transport === t) last.to = i + 1;
      else runs.push({ from: i, to: i + 1, transport: t });
    }
    return runs;
  }

  /** The next (dir = 1) or previous (dir = -1) frame after `from` with changes, or -1. */
  function nextChange(rec, from, dir) {
    for (let i = from + dir; i >= 0 && i < rec.frames; i += dir) if (rec.changed[i]) return i;
    return -1;
  }

  /** Milliseconds of an observed_at, NaN when it does not parse. */
  const frameTime = s => Date.parse(s);

  /**
   * X positions (0..1) of the frames: by observed_at when every frame's time parses and
   * they do not go backwards, else evenly by index.
   */
  function frameX(rec) {
    const n = rec.frames;
    const x = new Float64Array(n);
    if (n < 2) return x;
    const t = rec.observedAt.map(frameTime);
    const ok = t.every((v, i) => Number.isFinite(v) && (!i || v >= t[i - 1])) && t[n - 1] > t[0];
    for (let i = 0; i < n; ++i) x[i] = ok ? (t[i] - t[0]) / (t[n - 1] - t[0]) : i / (n - 1);
    return x;
  }

  /** The frame whose x is nearest to `x` (0..1). */
  function nearestFrame(xs, x) {
    let best = 0;
    for (let i = 1; i < xs.length; ++i) if (Math.abs(xs[i] - x) < Math.abs(xs[best] - x)) best = i;
    return best;
  }

  // ------------------------------------------------------------ live history (#218)

  /**
   * The SSE ids of a live stream (serve.py numbers its documents from 1): trackId(t, id)
   * says what a document event is. 'duplicate' = the id already seen (the latest document
   * sent again on a reconnect), 'restart' = a lower id (a restarted server numbers from 1
   * again), else 'new'; `skipped` counts the documents the stream left out between two new
   * ids (serve.py sends a slow client only the newest). The first id, and the first after a
   * restart, count nothing: the documents before them were before the page listened. An
   * event without an id (NaN) is new and counts nothing.
   */
  function streamIds() {
    return { last: NaN, skipped: 0, restarts: 0 };
  }

  function trackId(t, id) {
    if (!Number.isFinite(id)) return 'new';
    if (!Number.isFinite(t.last)) { t.last = id; return 'new'; }
    if (id === t.last) return 'duplicate';
    if (id < t.last) { t.last = id; t.restarts++; return 'restart'; }
    t.skipped += id - t.last - 1;
    t.last = id;
    return 'new';
  }

  /**
   * How many of the oldest frames to drop so the kept bytes fit `limit`: a tenth of the
   * frames at a time (at least one), so a full history is compacted once per tenth rather
   * than once per frame; the newest frame always stays. `sizes` are the frames' bytes.
   */
  function dropCount(sizes, limit) {
    let bytes = 0;
    for (const b of sizes) bytes += b;
    let n = 0;
    while (bytes > limit && n < sizes.length - 1) {
      const step = Math.min(sizes.length - 1 - n, Math.max(1, Math.ceil((sizes.length - n) / 10)));
      for (let k = n; k < n + step; ++k) bytes -= sizes[k];
      n += step;
    }
    return n;
  }

  /**
   * Drop the oldest n frames: the per-frame arrays shift, every series moves down in
   * place (copyWithin, no rescan of the documents) and its freed tail is reset; a series
   * whose pair is in none of the kept frames goes.
   */
  function dropFrames(rec, n) {
    n = Math.min(n, rec.frames);
    if (n <= 0) return;
    const old = rec.frames;
    const left = old - n;
    for (const a of [rec.starts, rec.ends, rec.observedAt, rec.changed]) a.splice(0, n);
    for (const [id, s] of rec.series) {
      for (const k of ['transport', 'guids', 'hz', 'latencyMean', 'latencySamples', 'lost']) {
        s[k].copyWithin(0, n, old);
        s[k].fill(k === 'transport' || k === 'guids' ? 0 : NaN, left, old);
      }
      if (!s.transport.subarray(0, left).some(Boolean)) rec.series.delete(id);
    }
    rec.frames = left;
  }

  /**
   * The frame index `i` after the oldest n frames were dropped: {index, dropped}, where a
   * dropped frame moves to the oldest kept one.
   */
  function shiftIndex(i, n) {
    return i < n ? { index: 0, dropped: true } : { index: i - n, dropped: false };
  }

  return { lineSplitter, parseFrame, createRecording, addFrame, identsOf, frameLatency, frameLoss, hasValues,
    transportRuns, nextChange, frameX, nearestFrame, transportCode, transportOf,
    streamIds, trackId, dropCount, dropFrames, shiftIndex };
});
