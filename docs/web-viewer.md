# Web viewer

`web/index.html` renders a `transport_viz --json` document as a graph: hosts are columns,
ROS nodes are boxes, and every writer → reader pair is an arrow colored by transport.
It is a static page (plain HTML/JS plus a vendored copy of d3) — no build step, no
server, and it works offline from `file://`. (`model.js` holds the document model and
formatting, `app.js` the rendering.)

![graph view](images/web-viewer-graph.jpg)

## Open it

```
transport_viz --json --stats > snapshot.json
open web/index.html            # macOS; or double-click the file
```

Then load the document in one of three ways:

- **Open JSON…** button (file picker)
- drag & drop the file anywhere on the page
- `index.html?src=<URL>` fetches the document (only when the page is served over HTTP,
  e.g. `python3 -m http.server` in `web/`; browsers block `fetch` from `file://`)

The page starts with `web/sample/sample.json`, a real capture of talker/listener nodes
plus the bounded verification nodes with statistics enabled.

## Reading the graph

| Element | Meaning |
|---|---|
| Column | a host (`local`, `host:<id>`, or the host name from statistics) |
| Box | a ROS node (`process id` below the name when statistics are available); a red `+N unmatched` marks topics without a peer |
| Arrow | writer → reader pairs between two nodes with the same transport and confidence, bundled; the label is the number of pairs |
| Color | UDPv4 blue · UDPv6 cyan · TCP purple · SHM green · DATA_SHARING orange · NONE grey (legend in the toolbar) |
| Dashed | confidence `likely` |
| Red halo | at least one warning, e.g. `measured-transport-mismatch` |

Click an arrow to list its pairs in the side panel: transport, confidence, measured
traffic, reason codes with their descriptions and remedies (taken from
`reason_code_descriptions` / `reason_code_remedies` in the document), locators, QoS (reliability, durability, data-sharing, and the deadline,
liveliness, ownership and partitions when set), a `data-sharing` row with the size of the
writer's history and whether the endpoint's data-sharing segment is in the tool's `/dev/shm`
([#163](https://github.com/atinfinity/fastdds_transport_viz/issues/163)) and, when the document carries a `participants` section, an `shm` row per
endpoint: its participant's SHM visibility from the tool's IPC namespace and the announced
SHM ports with their lock state ([#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125)).
Click a node for its publishers, subscriptions and unmatched topics.

The first header line names the domain, the observation time and the counts, and ends with
the statistics summary: how many samples the document holds and, when the tool lost some of
them, `N lost`. When the loss also cost a measurement, a `!stats-samples-lost` marker follows
whose tooltip carries the description and the remedy: a pair can then read `(idle)` although
it carries traffic. The marker follows the document's `stats.warnings`, so `N lost` without it
is a loss that did no harm
([statistics.md](statistics.md#large-systems)).

The second header line summarizes the shared memory of the environment `transport_viz`
ran in (the document's `shm` object, see [how-it-works.md](how-it-works.md#shared-memory-of-the-environment)):
capacity of `/dev/shm`, what Fast DDS keeps there, stale files and the `shm-*` warnings.

The **Table** tab has the CLI's `--verbose` shape: a header row per topic with the pair rows
beneath it. The header carries the document's topic aggregates (`topics[]`): the writer and
reader counts, the transports its pairs use, the latency of the slowest pair, the loss
(RTPS_LOST counted once per topic, resends summed) and the unmatched reasons; it never
sums the pair rows, so a filter that hides pairs leaves the header's numbers alone, and
the `Hz` cell stays empty because the rate exists only per pair. Click a header to fold or
unfold its pairs, or **Collapse all** / **Expand all** in the toolbar. Clicking a column
header sorts topics by their aggregate and pairs within a topic by their own value
(numbers numerically, missing values last). With statistics the pair rows include the
packets and bytes carried during the observation, the latency, the delivered samples per
second (`Hz`, hover for the window) and the loss.

![table view](images/web-viewer-table.jpg)

## Comparing two documents

The viewer runs the comparison of `transport_viz diff` (see
[how-it-works.md](how-it-works.md#comparing-two-snapshots)) in the browser, in three ways:

- **Compare with…** loads a second document and compares the one on screen (before) with
  it (after); the after document is then shown. **Open JSON…** keeps replacing the
  document, which also ends a comparison.
- `index.html?src=before.json&diff=after.json` fetches both (served over HTTP, like
  `?src=`); `&key=guid` selects the GUID key.
- A document that already carries a `changes` object, that is the output of
  `transport_viz diff --json` or a frame of `--watch --json`, is highlighted as it is.

The header names the before document (`vs 2026-09-13T09:00:00Z by node key`), the toolbar
shows the `changes:` summary of the CLI, a **changes only** switch that keeps the marked
pairs and their nodes, and, when the comparison was made here, the **key** (`node`, the
default, or `guid`, see [how-it-works.md](how-it-works.md#comparing-two-snapshots) for what
each survives). The comparison itself is `diffDocuments()` in `web/model.js`, a port of
the C++ function that is tested against the binary's output on the same fixture pair
(`web/sample/diff_before.json`, `diff_after.json`, `diff.json`), so both agree.

| Element | Meaning |
|---|---|
| `+` green | pair appeared (table row, edge halo and label) |
| `~` orange | transport, confidence, measured transport, selected locator, measured locators or warnings changed; the table shows `before → after` transports and the pair card lists what changed |
| `-` grey, dotted, italic | pair disappeared: a ghost row at the end of the table with the transport it had (when the before document is at hand), and a dotted ghost edge when both of its nodes still exist |

In live mode every mark and ghost stays for three frames after its change and then
clears, like the CLI's `--watch`.

The filters
(topic regex, node regex, transport checkboxes, "hide ROS internal topics" for
`/parameter_events`, `/rosout` and the native-buffer companion topics whose every endpoint
is folded into the parent topic, `buffer_parent_guid`) apply to the graph, the table and the edge panel (the
node panel always lists every topic of the node). The node
filter has the semantics of `--node`: pairs whose writer or reader belongs to a matching
node stay, the graph keeps the matching nodes (highlighted, even without visible pairs)
and the partner nodes of the remaining pairs, and hides the rest. An invalid regex is
shown with a red border and filters nothing.

## Live mode

`transport_viz_web` (installed from `web/serve.py`, Python standard library only) runs
`transport_viz --watch --json` as a subprocess and serves the viewer together with a
Server-Sent Events stream of every new document:

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/  (serving .../share/fastdds_transport_viz/web)
```

Open the printed URL: `/` redirects to `index.html?live=1`, which connects to `/events`
and re-renders on every document while keeping the selection, filters and zoom (the
layout is deterministic, so nothing jumps). The header shows the live state and the
time of the last update; **Pause** stops applying frames until **Resume**. `/latest.json`
always returns the most recent document (usable with `?src=/latest.json`; `?live=1` takes
precedence when both are given).

![live mode](images/web-viewer-live.jpg)

The server takes its own options; every other argument is forwarded to `transport_viz`
(a literal `--` is forwarded too, and rejected by the binary):

| Option | Meaning |
|---|---|
| `--bind ADDR` | listen address, default `127.0.0.1`; use `0.0.0.0` to view from another machine (e.g. a laptop looking at a robot) |
| `--port N` | default `8765`, `0` picks a free port |
| `--transport-viz PATH` | executable to run (default: next to the script, then `$PATH`) |
| `--verbose` | log requests and received documents |
| anything else | forwarded: `--stats`, `--interval S`, `--domain N`, `--all`, `--topic REGEX`, `--timeout S` |

If `transport_viz` exits, the server sends a `status` event (shown as "live: transport_viz
exited …") and stops; its exit code is 1 if `transport_viz` failed, 0 otherwise. In the Docker environment, `docker compose run
--rm --service-ports dev` publishes port 8765, so `transport_viz_web --bind 0.0.0.0` inside
the container is reachable from the host browser.

`transport_viz --watch --json` itself prints one compact document per line (JSON Lines),
so any other consumer can read the same stream. The `changes` object of those documents is
also what `transport_viz diff --json before.json after.json` emits (see
[how-it-works.md](how-it-works.md#comparing-two-snapshots)); the viewer highlights it in
both cases ([Comparing two documents](#comparing-two-documents)).

## Large documents

Measured in Chrome on an Apple M3 with documents of the scale verification (details: [development.md](development.md#scale-results)):

| Document | First render | Filtering down | Clearing a filter or clicking |
|---|---|---|---|
| Nav2 + TurtleBot3: 244 arrows, 1195 pairs, 2.4 MB | 25 ms | < 0.1 s | < 0.1 s |
| 1467 arrows, 5600 pairs, 15 MB | 0.16 s | < 0.1 s | 0.14 s |
| 4217 arrows, 13 800 pairs, 31 MB | 0.7 s | < 0.1 s | 0.7 s |

Every change of a filter, and every click, draws all visible arrows again. Above a few
thousand arrows, narrow the view by topic or node before clicking around; clearing a filter
or selecting costs about 0.16 ms per visible arrow ([#136](https://github.com/atinfinity/fastdds_transport_viz/issues/136)).

## JSON schema

`schema/transport_viz.schema.json` (JSON Schema 2020-12) is the contract the viewer relies
on. It lists the required keys and enumerations and allows unknown keys, so the tool can
add fields without bumping `schema_version`; an incompatible change bumps it. The sample
documents and the live `--json` output are validated against it in `colcon test`
(`test_json_schema` and `test_json_schema_live.py`, using `python3-jsonschema`).
