Real `transport_viz --json --stats` captures used as the viewer's initial document and as
schema test fixtures (`sample_all.json` was taken with `--all`). Both were re-captured on
Jazzy after the `RATE` column was removed ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137)),
so their `throughput_bytes_per_s` are null and their `stats.throughput` is empty, and since
#113 the binary fills `participants_with_stats` itself: nothing in them is edited by hand.

`shm_split.json` is an older capture, left as it is rather than re-taken: its
`participants_with_stats` was trimmed by hand to the participants that published statistics
(#113), and it still carries the real `throughput_bytes_per_s` values and the populated
`stats.throughput` of the time before #137 (the viewer ignores both and the schema allows
them). It was taken on Jazzy with a talker and a listener on the host network, each in an
IPC namespace of its own, and the tool in the talker's: `/chatter` is `NONE` with
`shm-ipc-namespace-split` (the capture predates #112: the listener's `ros_discovery_info`
samples were lost the same way, so its node name is `""` and the viewer shows its
participant instead). Open `index.html?src=sample/shm_split.json` to see it in the viewer.

`diff_before.json` / `diff_after.json` are a hand-made pair for `transport_viz diff` (a
profile change with every node restarted in between, one raw DDS pair untouched) and
`diff.json` is what the binary prints for `transport_viz diff --all --json` on them;
`test_cli_args.py` and `web/test/model.test.js` both assert that they reproduce it. Open
`index.html?src=sample/diff.json` or `?src=sample/diff_before.json&diff=sample/diff_after.json`
to see the comparison in the viewer.

`sample.js` is generated from `sample.json` so that the viewer can show it when opened
from `file://`. Regenerate after replacing `sample.json`:

```
python3 -c "import json; d=json.load(open('sample.json')); open('sample.js','w').write('// Generated from sample.json so that the viewer can show it when opened from file://\nwindow.TRANSPORT_VIZ_SAMPLE = ' + json.dumps(d, indent=1) + ';\n')"
```
