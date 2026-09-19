// Web viewer timings for the scale verification (#74, docs/development.md "Scale verification").
// Paste into the DevTools console (or Claude in Chrome) of any page served from the repository
// root, e.g. http://localhost:8000/web/index.html, after setting SRC:
//
//   const SRC = '/build/jazzy/scale/large.viz.json';
//
// The viewer is loaded RUNS times in a same-origin iframe (1400x900): first render is the time
// from creating the iframe until its graph holds edges (page, scripts, fetch, JSON parse, model
// and SVG); filter response the time from an `input` event on a filter box until the viewer has
// rendered (its `document.body.dataset.render` counter moves, which includes the 100 ms typing
// debounce, #136) and one animation frame has painted; select the same from a click on the
// first arrow, and from the click on the background that clears it. Resolves to medians and
// the raw values.
(async (src, runs = 5) => {
  const nextFrame = (w) => new Promise((r) => w.requestAnimationFrame(() => r()));
  const median = (v) => [...v].sort((a, b) => a - b)[Math.floor(v.length / 2)];
  const firstRender = [], filters = {topic: [], node: [], clear: []}, selects = {select: [], deselect: []};
  let edges = 0, nodes = 0;
  for (let i = 0; i < runs; ++i) {
    const frame = document.createElement('iframe');
    frame.style.cssText = 'position:fixed;left:0;top:0;width:1400px;height:900px;border:0;z-index:9999;background:#fff';
    const t0 = performance.now();
    frame.src = `/web/index.html?src=${encodeURIComponent(src)}`;
    document.body.appendChild(frame);
    for (;;) {
      await nextFrame(window);
      const d = frame.contentDocument;
      if (d && d.querySelectorAll('#graph g.edge').length > 0) break;
      if (performance.now() - t0 > 120000) throw new Error('no edges after 120 s');
    }
    firstRender.push(performance.now() - t0);
    const w = frame.contentWindow, d = frame.contentDocument;
    edges = d.querySelectorAll('#graph g.edge').length;
    nodes = d.querySelectorAll('#graph g.node').length;
    await nextFrame(w);
    // time from `act()` until the viewer's render counter moves, plus one frame to paint
    const timed = async (act) => {
      const before = d.body.dataset.render;
      const t = performance.now();
      act();
      for (;;) {
        await nextFrame(w);
        if (d.body.dataset.render !== before) break;
        if (performance.now() - t > 30000) throw new Error('no render after 30 s');
      }
      await nextFrame(w);
      return performance.now() - t;
    };
    const filter = (id, value) => timed(() => {
      const input = d.getElementById(id);
      input.value = value;
      input.dispatchEvent(new w.Event('input', {bubbles: true}));
    });
    filters.topic.push(await filter('filter-topic', 't00'));
    filters.clear.push(await filter('filter-topic', ''));
    filters.node.push(await filter('filter-node', 'p00'));
    filters.clear.push(await filter('filter-node', ''));
    selects.select.push(await timed(() => d.querySelector('#graph g.edge').dispatchEvent(new w.MouseEvent('click', {bubbles: true}))));
    selects.deselect.push(await timed(() => d.getElementById('graph').dispatchEvent(new w.MouseEvent('click', {bubbles: true}))));
    frame.remove();
  }
  const r = (v) => Math.round(v);
  return {
    src, runs, edges, nodes,
    first_render_ms: r(median(firstRender)),
    filter_topic_ms: r(median(filters.topic)),
    filter_node_ms: r(median(filters.node)),
    filter_clear_ms: r(median(filters.clear)),
    filter_worst_ms: r(Math.max(...filters.topic, ...filters.node, ...filters.clear)),
    select_ms: r(median(selects.select)),
    deselect_ms: r(median(selects.deselect)),
    raw: {first_render: firstRender.map(r), topic: filters.topic.map(r), node: filters.node.map(r),
      clear: filters.clear.map(r), select: selects.select.map(r), deselect: selects.deselect.map(r)},
  };
})(typeof SRC !== 'undefined' ? SRC : '/build/jazzy/scale/small.viz.json');
