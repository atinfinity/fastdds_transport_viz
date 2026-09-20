// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Minimal Chrome DevTools Protocol driver for the browser tests (#81). No dependencies:
// headless Chrome is launched from the PATH, one tab per test is opened through the
// browser's HTTP /json/new endpoint, and every interaction is an in-page
// `Runtime.evaluate` - the same way scripts/scale_viewer.js drives the viewer by hand.
// Needs Node >= 22 for the global WebSocket.
'use strict';
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const MAC_CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const NAMES = ['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser'];
const DEADLINE_MS = 10000;

function onPath(name) {
  for (const dir of (process.env.PATH || '').split(path.delimiter)) {
    const p = path.join(dir, name);
    try { fs.accessSync(p, fs.constants.X_OK); return p; } catch { /* keep looking */ }
  }
  return null;
}

const executable = (p) => { try { fs.accessSync(p, fs.constants.X_OK); return true; } catch { return false; } };

/** The browser to drive: $CHROME, then the usual Linux names, then the macOS bundle. */
function findBrowser() {
  if (process.env.CHROME) {
    // an explicit path that does not work is a mistake to report, never a reason to skip
    if (!executable(process.env.CHROME)) throw new Error(`$CHROME=${process.env.CHROME} is not an executable`);
    return process.env.CHROME;
  }
  for (const name of NAMES) { const found = onPath(name); if (found) return found; }
  return executable(MAC_CHROME) ? MAC_CHROME : null;
}

/**
 * '' when the browser tests can run, else the reason to skip them. FTV_REQUIRE_BROWSER
 * (set by CI) turns a missing browser into a failure instead: the job must not silently
 * shrink back to the unit tests.
 */
function browserSkip() {
  if (findBrowser()) return '';
  const why = 'no Chrome found (set $CHROME, or install google-chrome / chromium)';
  if (process.env.FTV_REQUIRE_BROWSER) throw new Error(`FTV_REQUIRE_BROWSER is set but ${why}`);
  return why;
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

/** Wait for `predicate()` to return a truthy value, reporting `message` and the last value on timeout. */
async function poll(predicate, message, timeout = DEADLINE_MS) {
  const until = Date.now() + timeout;
  let last;
  for (;;) {
    last = await predicate();
    if (last) return last;
    if (Date.now() > until) throw new Error(`timed out after ${timeout} ms waiting for ${message} (last value: ${JSON.stringify(last)})`);
    await sleep(25);
  }
}

/** The text of a console argument or an exception, as CDP reports it. */
const argText = a => (a && (a.description || (a.value !== undefined ? String(a.value) : a.unserializableValue))) || '';

class Page {
  constructor(browser, target) {
    this.browser = browser;
    this.id = target.id;
    this.wsUrl = target.webSocketDebuggerUrl;
    this.nextId = 0;
    this.pending = new Map();
    this.errors = [];       // uncaught exceptions and console.error output
    this.allowed = [];      // patterns a test expects, see allow()
  }

  async open() {
    this.ws = new WebSocket(this.wsUrl);
    await new Promise((resolve, reject) => {
      this.ws.addEventListener('open', resolve, { once: true });
      this.ws.addEventListener('error', () => reject(new Error(`cannot connect to ${this.wsUrl}`)), { once: true });
    });
    this.ws.addEventListener('message', ev => this.onMessage(JSON.parse(ev.data)));
    this.ws.addEventListener('close', () => {
      for (const { reject } of this.pending.values()) reject(new Error('devtools connection closed'));
      this.pending.clear();
    });
    // Runtime before the first navigation, so an exception thrown while the viewer boots
    // is reported rather than missed
    await this.send('Runtime.enable');
    await this.send('Page.enable');
  }

  onMessage(msg) {
    if (msg.id !== undefined) {
      const entry = this.pending.get(msg.id);
      if (!entry) return;
      this.pending.delete(msg.id);
      if (msg.error) entry.reject(new Error(`${entry.method}: ${msg.error.message}`));
      else entry.resolve(msg.result);
      return;
    }
    if (msg.method === 'Runtime.consoleAPICalled' && msg.params.type === 'error') {
      this.errors.push('console.error: ' + msg.params.args.map(argText).join(' '));
    } else if (msg.method === 'Runtime.exceptionThrown') {
      const d = msg.params.exceptionDetails;
      this.errors.push('uncaught: ' + (argText(d.exception) || d.text));
    }
  }

  send(method, params = {}) {
    const id = ++this.nextId;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject, method });
      this.ws.send(JSON.stringify({ id, method, params }));
      setTimeout(() => {
        if (this.pending.delete(id)) reject(new Error(`${method} did not answer within ${DEADLINE_MS} ms`));
      }, DEADLINE_MS);
    });
  }

  /** Evaluate `expression` in the page and return its value (promises are awaited). */
  async evaluate(expression) {
    const r = await this.send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
    if (r.exceptionDetails) {
      const d = r.exceptionDetails;
      throw new Error(`page threw while evaluating ${expression}: ${argText(d.exception) || d.text}`);
    }
    return r.result.value;
  }

  /** Load `url` and wait until the viewer has rendered once. */
  async goto(url) {
    await this.send('Page.navigate', { url });
    await poll(async () => {
      try { return await this.evaluate('document.readyState === "complete" && document.body.dataset.render !== undefined'); } catch { return false; }
    }, `${url} to render`, 30000);
  }

  /** The viewer's render counter (app.js increments it on every render and selection change). */
  render() { return this.evaluate('document.body.dataset.render'); }

  /** Wait until the render counter moved past `before` and return the new value. */
  waitForRender(before, what = 'a render') {
    return poll(async () => {
      const now = await this.render();
      return now !== before ? now : false;
    }, what);
  }

  waitFor(expression, what = expression) { return poll(() => this.evaluate(expression), what); }

  count(selector) { return this.evaluate(`document.querySelectorAll(${JSON.stringify(selector)}).length`); }

  text(selector) {
    return this.evaluate(`(document.querySelector(${JSON.stringify(selector)}) || {}).textContent ?? null`);
  }

  /** Click through a synthetic event: the viewer wires every handler with d3's .on(). */
  click(selector) {
    return this.evaluate(`(() => {
      const el = document.querySelector(${JSON.stringify(selector)});
      if (!el) throw new Error('no element for ' + ${JSON.stringify(selector)});
      el.dispatchEvent(new MouseEvent('click', {bubbles: true}));
      return true;
    })()`);
  }

  /** Expect console output matching `pattern` in this test instead of failing on it. */
  allow(pattern) { this.allowed.push(pattern); }

  checkErrors() {
    const unexpected = this.errors.filter(e => !this.allowed.some(p => p.test(e)));
    if (unexpected.length) throw new Error(`the page reported ${unexpected.length} error(s):\n  ${unexpected.join('\n  ')}`);
  }

  async close() {
    try { this.ws.close(); } catch { /* already gone */ }
    try { await fetch(`${this.browser.base}/json/close/${this.id}`); } catch { /* browser gone */ }
  }
}

class Browser {
  constructor(proc, base, userDataDir) {
    this.proc = proc;
    this.base = base;
    this.userDataDir = userDataDir;
  }

  /** A fresh tab, so no test inherits another's filters, selection or collapsed rows. */
  async open(url) {
    const res = await fetch(`${this.base}/json/new?url=about:blank`, { method: 'PUT' });
    if (!res.ok) throw new Error(`cannot open a tab: ${res.status} ${res.statusText}`);
    const page = new Page(this, await res.json());
    await page.open();
    if (url) await page.goto(url);
    return page;
  }

  /** Open `url`, run `fn(page)`, then fail if the page reported an error; always closes the tab. */
  async withPage(url, fn) {
    const page = await this.open(url);
    try {
      await fn(page);
      page.checkErrors();
    } finally {
      await page.close();
    }
  }

  async close() {
    this.proc.kill();
    await new Promise(resolve => this.proc.once('exit', resolve));
    // Chrome's helper processes (zygote, crashpad) may still be writing into the profile
    // for a moment after the browser exits: retry, and never fail a run over a temp directory
    try {
      fs.rmSync(this.userDataDir, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
    } catch (e) {
      console.warn(`could not remove ${this.userDataDir}: ${e.message}`);
    }
  }
}

/** Start headless Chrome and return a Browser talking to it. */
async function launch() {
  const bin = findBrowser();
  if (!bin) throw new Error('no Chrome found');
  const userDataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ftv-chrome-'));
  const args = ['--headless=new', '--disable-gpu', '--window-size=1400,900', '--remote-debugging-port=0',
    `--user-data-dir=${userDataDir}`, '--no-first-run', '--no-default-browser-check', '--disable-extensions'];
  // the Chrome sandbox refuses to run as root (a dev container), and it needs unprivileged
  // user namespaces, which the Ubuntu 24.04 runner image forbids (FTV_CHROME_NO_SANDBOX=1)
  if (process.env.FTV_CHROME_NO_SANDBOX || (process.getuid && process.getuid() === 0)) args.push('--no-sandbox');
  const proc = spawn(bin, args, { stdio: ['ignore', 'ignore', 'pipe'] });
  const base = await new Promise((resolve, reject) => {
    let out = '';
    const timer = setTimeout(() => reject(new Error(`${bin} did not report a debugging port in 20 s:\n${out}`)), 20000);
    proc.stderr.setEncoding('utf8');
    proc.stderr.on('data', chunk => {
      out += chunk;
      const m = out.match(/DevTools listening on ws:\/\/([^/\s]+)\//);
      if (m) { clearTimeout(timer); resolve(`http://${m[1]}`); }
    });
    proc.once('exit', code => { clearTimeout(timer); reject(new Error(`${bin} exited with code ${code}:\n${out}`)); });
  });
  return new Browser(proc, base, userDataDir);
}

module.exports = { browserSkip, findBrowser, launch, poll };
