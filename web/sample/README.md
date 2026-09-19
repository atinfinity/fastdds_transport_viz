Real `transport_viz --json --stats` captures used as the viewer's initial document and as
schema test fixtures (`sample_all.json` was taken with `--all`). Both were re-captured on
Jazzy after the `RATE` column was removed ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137)),
so their `throughput_bytes_per_s` are null and their `stats.throughput` is empty, and since
#113 the binary fills `participants_with_stats` itself: nothing in them is edited by hand.

`shm_split.json` is `scripts/integration_test.sh hostnet_split_shm` on Jazzy (no `--stats`),
re-taken for [#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125): a
talker and a listener on the host network, each in an IPC namespace of its own, and the
tool in a third. `/chatter` is `NONE` with `shm-ipc-namespace-split` on
`shm-port-collision` (both nodes and the tool announce 7000), and the `participants` section
shows each node's other port `absent` from the tool's namespace. Open
`index.html?src=sample/shm_split.json` to see it in the viewer.

`easy_mode.json` is `scripts/integration_test.sh easy_mode_tcp` on Lyrical (no `--stats`),
taken for [#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86): a talker
and a listener in two bridged containers under `ROS2_EASY_MODE`, the tool a third client
of the same network. Each host runs a `DiscoveryServerAuto` participant (a `SERVER` with
no endpoints), every node is a `SUPER_CLIENT` attributed to the server of its host, and
`discovery.easy_mode` / `observer_protocol` record how the tool itself joined. Open
`index.html?src=sample/easy_mode.json` to see the server pills and the dotted
client edges in the viewer.

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
