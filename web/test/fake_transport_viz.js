#!/usr/bin/env node
// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Stands in for `transport_viz --watch --json` in the live browser test (#81): prints the
// given documents as JSON Lines, one per step, with a fresh `observed_at` each time.
//
//   fake_transport_viz.js --watch --json A.json B.json --step-file PATH
//
// serve.py forwards everything after `--watch --json` verbatim. The test drives the stream
// by writing into the step file: an integer is the index of the last frame to print (the
// last document repeats for higher indices), `stop` exits 0 so serve.py emits its status
// event. Nothing is printed until the file says so, which keeps the browser test free of
// races with the SSE connection.
'use strict';
const fs = require('node:fs');

const argv = process.argv.slice(2);
if (argv[0] !== '--watch' || argv[1] !== '--json') {
  console.error(`fake_transport_viz: unexpected arguments ${JSON.stringify(argv)}`);
  process.exit(2);
}
const files = [];
let stepFile = null;
for (let i = 2; i < argv.length; ++i) {
  if (argv[i] === '--step-file') stepFile = argv[++i];
  else files.push(argv[i]);
}
if (!files.length || !stepFile) {
  console.error('fake_transport_viz: need at least one document and --step-file PATH');
  process.exit(2);
}

const docs = files.map(f => JSON.parse(fs.readFileSync(f, 'utf8')));
const step = () => { try { return fs.readFileSync(stepFile, 'utf8').trim(); } catch { return ''; } };

let printed = 0;   // frames already on stdout
const tick = () => {
  const want = step();
  if (want === 'stop') process.exit(0);
  const upto = want === '' ? -1 : Number(want);
  if (!Number.isNaN(upto)) {
    while (printed <= upto) {
      const doc = { ...docs[Math.min(printed, docs.length - 1)], observed_at: `frame-${printed}` };
      process.stdout.write(JSON.stringify(doc) + '\n');
      ++printed;
    }
  }
  setTimeout(tick, 50);
};
tick();
