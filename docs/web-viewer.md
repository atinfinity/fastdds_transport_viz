# Web viewer

`web/index.html` renders a `transport_viz --json` document as a graph: hosts are columns,
ROS nodes are boxes, and every writer → reader pair is an arrow colored by transport.
It is a static page (plain HTML/JS plus a vendored copy of d3) — no build step, no
server, and it works offline from `file://`.

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
| Arrow | writer → reader pairs between two nodes with the same transport, bundled with the number of topics |
| Color | UDPv4 blue · UDPv6 cyan · TCP purple · SHM green · DATA_SHARING orange · NONE grey (legend in the toolbar) |
| Dashed | confidence `likely` |
| Red halo | at least one warning, e.g. `measured-transport-mismatch` |

Click an arrow to list its pairs in the side panel: transport, confidence, measured
traffic, reason codes with their descriptions (taken from `reason_code_descriptions` in
the document), locators and QoS of both endpoints. Click a node for its publishers,
subscriptions and unmatched topics.

The **Table** tab shows one row per pair (sortable by clicking a header).

![table view](images/web-viewer-table.jpg)
 The filters
(topic regex, transport checkboxes, "hide ROS internal topics" for `/parameter_events`
and `/rosout`) apply to the graph, the table and the panel.

## JSON schema

`schema/transport_viz.schema.json` (JSON Schema 2020-12) is the contract the viewer relies
on. It lists the required keys and enumerations and allows unknown keys, so the tool can
add fields without bumping `schema_version`; an incompatible change bumps it. The sample
documents and the live `--json` output are validated against it in `colcon test`
(`test_json_schema` and `test_json_schema_live.py`, using `python3-jsonschema`).
