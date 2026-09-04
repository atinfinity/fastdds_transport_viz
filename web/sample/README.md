Real `transport_viz --json --stats` captures used as the viewer's initial document and as
schema test fixtures (`sample_all.json` was taken with `--all`).

`sample.js` is generated from `sample.json` so that the viewer can show it when opened
from `file://`. Regenerate after replacing `sample.json`:

```
python3 -c "import json; d=json.load(open('sample.json')); open('sample.js','w').write('// Generated from sample.json so that the viewer can show it when opened from file://\nwindow.TRANSPORT_VIZ_SAMPLE = ' + json.dumps(d, indent=1) + ';\n')"
```
