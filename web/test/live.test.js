// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Browser-level test of the viewer's live mode (#81): web/serve.py with a fake
// transport_viz (web/test/fake_transport_viz.js), whose frames the test releases one at a
// time through a step file - so the EventSource wiring, the kept selection, the held
// marks, Pause / Resume and the end-of-stream banner are asserted without a race.
//
// The second test (#191) breaks the connection under the browser: a TCP proxy in front of
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
    // `grace` waits that long for serve.py to exit by itself first: SIGTERM kills it
    // before its cleanup runs, which orphans the transport_viz it started - and that
    // orphan holds this process's stderr pipe open, so node would never exit.
    stop: (grace = 0) => new Promise(done => {
      const gone = () => { proc.stdout.destroy(); proc.stderr.destroy(); done(); };
      if (proc.exitCode !== null) return gone();
      proc.once('exit', gone);
      if (grace) setTimeout(() => { if (proc.exitCode === null) proc.kill(); }, grace).unref();
      else proc.kill();
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

test('live mode: frames, kept selection, Pause / Resume and the end of the stream', opts, async (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ftv-live-'));
  const stepFile = path.join(dir, 'step');
  const step = value => fs.writeFileSync(stepFile, String(value));
  const browser = await cdp.launch();
  const server = await startServer(stepFile);
  const page = await browser.open(`${server.base}/index.html?live=1`);
  try {
    await t.test('connects and renders the first frame', async () => {
      step(0);
      await page.waitFor('/live: updated frame-0 \\(#1\\)/.test(document.getElementById("live-text").textContent)',
        'the first live frame');
      assert.equal(await page.evaluate('document.getElementById("live").hasAttribute("hidden")'), false);
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff_before.json')));
      assert.match(await page.text('#meta'), /frame-0/);
    });

    await t.test('a new frame keeps the selection and draws the changes', async () => {
      // an arrow that both documents have, so the selection has something to survive on
      const id = '/teleop→/base|SHM|certain';
      await page.evaluate(`(() => {
        const el = d3.selectAll('#graph g.edge').filter(d => d.id === ${JSON.stringify(id)}).node();
        el.dispatchEvent(new MouseEvent('click', {bubbles: true}));
        return true;
      })()`);
      assert.deepEqual(await page.evaluate(SELECTED), [id]);

      step(1);
      await page.waitFor('/\\(#2\\)/.test(document.getElementById("live-text").textContent)', 'the second live frame');
      assert.deepEqual(await page.evaluate(SELECTED), [id], 'a live update keeps the selection');
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff.json')));
      assert.ok(await page.count('#graph g.edge.added') > 0, 'the frame\'s changes are marked');
      assert.match(await page.text('#meta'), /frame-1/);
    });

    await t.test('Pause holds the newest frame back, Resume applies it', async () => {
      await page.click('#live-pause');
      assert.equal(await page.text('#live-pause'), 'Resume');
      assert.equal(await page.text('#live-text'), 'live: paused');

      step(2);
      await page.waitFor('/paused \\(\\d+ updates, newest frame-2\\)/.test(document.getElementById("live-text").textContent)',
        'the pending frame');
      assert.match(await page.text('#meta'), /frame-1/, 'the paused viewer keeps showing the old frame');

      await page.click('#live-pause');
      await page.waitFor('document.getElementById("meta").textContent.includes("frame-2")', 'the resumed frame');
      assert.equal(await page.text('#live-pause'), 'Pause');
      assert.equal(await page.text('#live-text'), 'live: resumed');
    });

    await t.test('the end of the stream is reported', async () => {
      step('stop');
      await page.waitFor('/live: transport_viz exited with code 0/.test(document.getElementById("live-text").textContent)',
        'the status event');
      assert.equal(await page.evaluate('document.getElementById("live").className'), 'live ended');
    });

    page.checkErrors();
  } finally {
    await page.close();
    await browser.close();
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
      // default is 3 s), on a runner that may be loaded.
      await page.waitFor(`/live: updated frame-0 \\(#2\\)/.test(${liveText})`, 'the recovery', 15000);
      assert.equal(await page.evaluate(liveClass), 'live ', 'the banner is gone');
      assert.match(await page.text('#meta'), /frame-0/);
    });

    await t.test('the stream is live again, not merely reconnected', async () => {
      step(1);
      await page.waitFor(`/live: updated frame-1 \\(#3\\)/.test(${liveText})`, 'a frame after the recovery');
      assert.deepEqual(await page.evaluate(EDGE_IDS), edgeIdsOf(load('diff.json')));
      assert.match(await page.text('#meta'), /frame-1/);
    });

    page.checkErrors();
  } finally {
    await page.close();
    await browser.close();
    await proxy.stop();
    step('stop');            // the producer exits, serve.py follows; see stop()'s grace
    await server.stop(3000);
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
