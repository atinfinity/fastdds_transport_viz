// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Browser-level test of the viewer's live mode (#81): web/serve.py with a fake
// transport_viz (web/test/fake_transport_viz.js), whose frames the test releases one at a
// time through a step file - so the EventSource wiring, the kept selection, the held
// marks, Pause / Resume and the end-of-stream banner are asserted without a race.
//
// The live history (#218): the timeline grows with the frames and follows the newest one,
// a move on it stops on a frame while the frames keep coming, `live ▶|` / End go back,
// the charts and Save recording work on the kept frames, and ?history= bounds them.
//
// The #191 test breaks the connection under the browser: a TCP proxy in front of
// serve.py drops the SSE socket on demand, which is the only way to reach the reconnect
// banner and the recovery after it without killing a server and racing for its port.
// Run: node --test web/test  (skips when Chrome or python3 is missing).
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { spawn, spawnSync } = require('node:child_process');
const fs = require('node:fs');
const net = require('node:net');
const os = require('node:os');
const path = require('node:path');
const readline = require('node:readline');

const M = require('../model.js');
const S = require('../scene.js');
const cdp = require('./cdp.js');

const WEB = path.join(__dirname, '..');
const SAMPLE = path.join(WEB, 'sample');

function skipReason() {
  const browser = cdp.browserSkip();
  if (browser) return browser;
  if (spawnSync('python3', ['--version']).status !== 0) {
    const why = 'python3 not found (needed to run web/serve.py)';
    if (process.env.FTV_REQUIRE_BROWSER) throw new Error(`FTV_REQUIRE_BROWSER is set but ${why}`);
    return why;
  }
  return '';
}
const skip = skipReason();
const opts = skip ? { skip } : {};

const load = name => {
  const doc = JSON.parse(fs.readFileSync(path.join(SAMPLE, name), 'utf8'));
  M.normalizeDocument(doc);
  return doc;
};
const filter = () => ({ topic: '', node: '', transports: new Set(M.TRANSPORTS), hideInternal: true });
const edgeIdsOf = doc => S.sceneEdges(S.visibleScene(M.buildModel(doc), filter(), false, { marks: new Map(), ghosts: [] }))
  .map(e => e.id).sort();

const EDGE_IDS = 'd3.selectAll("#graph g.edge").data().map(d => d.id).sort()';
const SELECTED = 'd3.selectAll("#graph g.edge.selected").data().map(d => d.id)';

/** Start serve.py with the fake producer; resolves to {base, stop()}. */
async function startServer(stepFile) {
  const proc = spawn('python3', [path.join(WEB, 'serve.py'), '--port', '0',
    '--transport-viz', path.join(__dirname, 'fake_transport_viz.js'),
    path.join(SAMPLE, 'diff_before.json'), path.join(SAMPLE, 'diff.json'),
    '--step-file', stepFile], { stdio: ['ignore', 'pipe', 'pipe'] });
  let stderr = '';
  proc.stderr.setEncoding('utf8');
  proc.stderr.on('data', chunk => { stderr += chunk; });
  const base = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`serve.py did not start in 20 s:\n${stderr}`)), 20000);
    readline.createInterface({ input: proc.stdout }).on('line', line => {
      const m = line.match(/listening on (http:\/\/[^/]+)\//);
      if (m) { clearTimeout(timer); resolve(m[1]); }
    });
    proc.once('exit', code => { clearTimeout(timer); reject(new Error(`serve.py exited with code ${code}:\n${stderr}`)); });
  });
  return {
    base,
    // SIGTERM is enough: serve.py reaps the producer it started before it exits (#195),
    // so nothing is left holding this process's stderr pipe open.
    stop: () => new Promise(done => {
      const gone = () => { proc.stdout.destroy(); proc.stderr.destroy(); done(); };
      if (proc.exitCode !== null) return gone();
      proc.once('exit', gone);
      proc.kill();
    }),
  };
}

/**
 * A TCP proxy in front of `port`, on a free port of its own; resolves to {base, cut(),
 * stop()}. `cut()` destroys every connection the browser holds without closing the
 * listening socket, so the EventSource retry finds a server again - a lost connection
 * that serve.py never hears about, which is what the browser does when a cable, a Wi-Fi
 * link or a laptop lid interrupts it.
 */
async function startProxy(port) {
  const sockets = new Set();
  const server = net.createServer(client => {
    const upstream = net.connect(port, '127.0.0.1');
    const pair = { client, upstream };
    sockets.add(pair);
    client.pipe(upstream);
    upstream.pipe(client);
    const drop = () => { sockets.delete(pair); client.destroy(); upstream.destroy(); };
    for (const s of [client, upstream]) { s.on('error', drop); s.on('close', drop); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const cut = () => {
    for (const { client, upstream } of sockets) { client.destroy(); upstream.destroy(); }
    sockets.clear();
  };
  return {
    base: `http://127.0.0.1:${server.address().port}`,
    cut,
    // close() alone hangs while the page's keep-alive socket is open
    stop: () => new Promise(done => { cut(); server.close(() => done()); }),
  };
}

const LIVE_TEXT = 'document.getElementById("live-text").textContent';
const liveIs = text => `${LIVE_TEXT} === ${JSON.stringify(text)}`;

test('live mode: frames, kept selection, the history timeline and the end of the stream', opts, async (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ftv-live-'));
  const stepFile = path.join(dir, 'step');
  const step = value => fs.writeFileSync(stepFile, String(value));
  const browser = await cdp.launch();
  const server = await startServer(stepFile);
  const page = await browser.open(`${server.base}/index.html?live=1`);
  try {
    await t.test('connects and renders the first frame', async () => {
      step(0);
      await page.waitFor(liveIs('live: updated frame-0 (#1)'), 'the first live frame');
      assert.equal(await page.evaluate('document.getElementById("live").hasAttribute("hidden")'), false);
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-0")', 'the first frame on screen');
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff_before.json')));
      assert.equal(await page.evaluate('document.getElementById("timeline").hidden'), true, 'one frame has no timeline');
    });

    const id = '/teleop→/base|SHM|certain';
    await t.test('a new frame keeps the selection, draws the changes and grows the timeline', async () => {
      // an arrow that both documents have, so the selection has something to survive on
      await page.evaluate(`(() => {
        const el = d3.selectAll('#graph g.edge').filter(d => d.id === ${JSON.stringify(id)}).node();
        el.dispatchEvent(new MouseEvent('click', {bubbles: true}));
        return true;
      })()`);
      assert.deepEqual(await page.evaluate(SELECTED), [id]);

      step(1);
      await page.waitFor(liveIs('live: updated frame-1 (#2)'), 'the second live frame');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-1")', 'the second frame on screen');
      assert.deepEqual(await page.evaluate(SELECTED), [id], 'a live update keeps the selection');
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff.json')));
      assert.ok(await page.count('#graph g.edge.added') > 0, 'the frame\'s changes are marked');
      assert.equal(await page.evaluate('document.getElementById("timeline").hidden'), false);
      assert.equal(await page.text('#tl-label'), 'frame-1 · 2 / 2');
      assert.equal(await page.evaluate('document.getElementById("tl-live").disabled'), true, 'following already');
    });

    await t.test('the timeline follows the newest frame, the charts grow with it', async () => {
      step(2);
      await page.waitFor(liveIs('live: updated frame-2 (#3)'), 'the third live frame');
      assert.equal(await page.text('#tl-label'), 'frame-2 · 3 / 3');
      assert.equal(await page.evaluate('document.getElementById("tl-slider").value'), '2');
      assert.ok(await page.count('#panel .replay-charts') > 0, 'the pair cards carry the charts');
      assert.equal(await page.evaluate('document.getElementById("tl-status").textContent'), '', 'no frame was skipped');
    });

    await t.test('◀ stops on the previous frame while the frames keep coming', async () => {
      await page.click('#tl-prev');
      await page.waitFor(liveIs('live: viewing #2 of 3 (newest frame-2)'), 'the paused header');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-1")', 'the previous frame');
      assert.equal(await page.text('#live-pause'), 'Resume');
      assert.equal(await page.evaluate('document.getElementById("live").className'), 'live paused');
      assert.equal(await page.evaluate('document.getElementById("tl-live").disabled'), false);

      step(3);
      await page.waitFor(liveIs('live: viewing #2 of 4 (newest frame-3)'), 'a frame arriving while paused');
      assert.match(await page.text('#meta'), /frame-1/, 'the screen stays on the frame');
      assert.equal(await page.text('#tl-label'), 'frame-1 · 2 / 4');
      assert.deepEqual(await page.evaluate(SELECTED), [id]);
    });

    await t.test('live ▶| goes back to the newest frame and follows it', async () => {
      await page.click('#tl-live');
      await page.waitFor(liveIs('live: updated frame-3 (#4)'), 'following again');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-3")', 'the newest frame');
      assert.equal(await page.text('#live-pause'), 'Pause');
      assert.equal(await page.text('#tl-label'), 'frame-3 · 4 / 4');
    });

    await t.test('Pause stops on the frame shown, End goes back', async () => {
      await page.click('#live-pause');
      await page.waitFor(liveIs('live: viewing #4 of 4 (newest frame-3)'), 'paused');
      step(4);
      await page.waitFor(liveIs('live: viewing #4 of 5 (newest frame-4)'), 'a frame arriving while paused');
      assert.match(await page.text('#meta'), /frame-3/);
      await page.evaluate('document.body.dispatchEvent(new KeyboardEvent("keydown", {key: "End", bubbles: true})), true');
      await page.waitFor(liveIs('live: updated frame-4 (#5)'), 'End');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-4")', 'the newest frame');
    });

    await t.test('Save recording downloads the kept frames as JSON Lines', async () => {
      await page.evaluate(`(() => {
        const create = URL.createObjectURL;
        URL.createObjectURL = (blob) => { window.savedBlob = blob; return create(blob); };
        HTMLAnchorElement.prototype.click = function () { window.savedName = this.download; };
        return true;
      })()`);
      await page.click('#tl-save');
      assert.equal(await page.evaluate('window.savedName'), 'transport_viz-frame-0.jsonl');
      const text = await page.evaluate('window.savedBlob.text()');
      assert.ok(text.endsWith('\n'));
      const docs = text.trimEnd().split('\n').map(l => JSON.parse(l));
      assert.deepEqual(docs.map(d => d.observed_at), ['frame-0', 'frame-1', 'frame-2', 'frame-3', 'frame-4']);
      assert.ok(docs.every(d => d.schema_version === 1));
    });

    await t.test('the end of the stream is reported, the history stays', async () => {
      step('stop');
      await page.waitFor('/live: transport_viz exited with code 0/.test(document.getElementById("live-text").textContent)',
        'the status event');
      assert.equal(await page.evaluate('document.getElementById("live").className'), 'live ended');
      await page.click('#tl-prev');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-3")', 'a kept frame after the end');
      assert.equal(await page.text('#live-text'), 'live: transport_viz exited with code 0 · viewing #4 of 5');
    });

    page.checkErrors();
  } finally {
    await page.close();
    await browser.close();
    await server.stop();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test('live mode: ?history= drops the oldest frames, the one on screen too', opts, async (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ftv-history-'));
  const stepFile = path.join(dir, 'step');
  const step = value => fs.writeFileSync(stepFile, String(value));
  const browser = await cdp.launch();
  const server = await startServer(stepFile);
  // 0.05 MB: three frames of the sample documents (15-19 KB each)
  const page = await browser.open(`${server.base}/index.html?live=1&history=0.05`);
  try {
    // one frame at a time: serve.py sends a client only the newest document
    step(0);
    await page.waitFor(liveIs('live: updated frame-0 (#1)'), 'the first frame');
    step(1);
    await page.waitFor(liveIs('live: updated frame-1 (#2)'), 'two frames');

    await t.test('paused on the oldest frame, it is dropped under the bound', async () => {
      await page.click('#tl-prev');
      await page.waitFor(liveIs('live: viewing #1 of 2 (newest frame-1)'), 'the oldest frame');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-0")', 'the oldest frame on screen');
      for (let k = 2; k <= 9; ++k) {
        step(k);
        await page.waitFor(`${LIVE_TEXT}.endsWith("(newest frame-${k})")`, `frame ${k}`);
      }
      const status = await page.text('#tl-status');
      assert.match(status, /history: 0\.05 MB, oldest dropped/);
      assert.match(status, /the frame on screen was dropped/);
      const kept = Number(await page.evaluate('document.getElementById("tl-slider").max')) + 1;
      assert.ok(kept >= 2 && kept <= 4, `${kept} frames kept`);
      assert.match(await page.text('#live-text'), new RegExp(`^live: viewing #1 of ${kept} `), 'still paused, on the oldest kept frame');
      const oldest = (await page.text('#tl-label')).split(' · ')[0];
      assert.notEqual(oldest, 'frame-0');
      await page.waitFor(`document.getElementById("meta").textContent.includes(${JSON.stringify(oldest)})`, 'the oldest kept frame on screen');
    });

    await t.test('live ▶| clears the note and follows again', async () => {
      await page.click('#tl-live');
      await page.waitFor(`/^live: updated frame-9 /.test(${LIVE_TEXT})`, 'following');
      assert.doesNotMatch(await page.text('#tl-status'), /the frame on screen was dropped/);
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-9")', 'the newest frame');
    });

    page.checkErrors();
  } finally {
    await page.close();
    await browser.close();
    step('stop');
    await server.stop();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test('live mode: the reconnect banner and the recovery after it', opts, async (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ftv-reconnect-'));
  const stepFile = path.join(dir, 'step');
  const step = value => fs.writeFileSync(stepFile, String(value));
  const browser = await cdp.launch();
  const server = await startServer(stepFile);
  // serve.py keeps listening throughout: only the browser's connection to it is severed
  const proxy = await startProxy(Number(new URL(server.base).port));
  const page = await browser.open(`${proxy.base}/index.html?live=1`);
  const liveText = 'document.getElementById("live-text").textContent';
  const liveClass = 'document.getElementById("live").className';
  try {
    step(0);
    await page.waitFor(`/live: updated frame-0 \\(#1\\)/.test(${liveText})`, 'the first live frame');

    await t.test('a lost connection raises the banner', async () => {
      proxy.cut();
      await page.waitFor(`${liveClass}.includes("reconnecting")`, 'the reconnect banner');
      assert.equal(await page.text('#live-text'), 'live: connection lost, reconnecting…');
      assert.match(await page.text('#meta'), /frame-0/, 'the disconnected viewer keeps showing the last frame');
    });

    await t.test('the next document clears it, without a new frame', async () => {
      // serve.py sends the latest document to every new connection, so the viewer heals
      // itself even when the next real frame is an --interval away. 15 s: the wait covers
      // a delay the browser owns (serve.py asks for 1 s with `retry:`, Chrome's own
      // default is 3 s), on a runner that may be loaded. The document is the one already
      // kept (the same SSE id, #218): the banner goes, no frame is added.
      await page.waitFor(`${liveClass} === "live "`, 'the recovery', 15000);
      assert.equal(await page.text('#live-text'), 'live: updated frame-0 (#1)');
      assert.match(await page.text('#meta'), /frame-0/);
    });

    await t.test('the stream is live again, not merely reconnected', async () => {
      step(1);
      await page.waitFor(`/live: updated frame-1 \\(#2\\)/.test(${liveText})`, 'a frame after the recovery');
      assert.equal(await page.text('#tl-label'), 'frame-1 · 2 / 2', 'the reconnect added no frame');
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff.json')));
      assert.match(await page.text('#meta'), /frame-1/);
    });

    page.checkErrors();
  } finally {
    await page.close();
    await browser.close();
    await proxy.stop();
    step('stop');            // the producer exits and serve.py follows by itself
    await server.stop();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
