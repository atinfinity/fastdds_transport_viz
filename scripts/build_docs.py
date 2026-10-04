#!/usr/bin/env python3
# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Builds the documentation site with Zensical, in English and Japanese.

  pip install -r docs/requirements.txt
  python3 scripts/build_docs.py            # site/ (English) and site/ja/ (Japanese), strict
  python3 scripts/build_docs.py --serve    # the same, served at
                                           # http://127.0.0.1:8000/fastdds_transport_viz/
  zensical serve                           # live preview of the English site only

Zensical has no i18n plugin (#243), so the two languages are two builds. The English site
is zensical.toml as it is, which leaves out docs/*.ja.md. The Japanese site is built from a
staging copy of docs/ in which each x.ja.md becomes x.md (its links to the other .ja.md
pages follow) and the pages without a translation stay in English, with a config derived
from zensical.toml: language ja, the nav titles of [i18n.ja.nav] and site_url + ja/. The
page URLs are those of the former mkdocs-static-i18n layout (/x/ and /ja/x/).
"""

import argparse
import functools
import http.server
import json
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / 'docs'
SITE = ROOT / 'site'
NOT_PAGES = ('requirements.txt',)


def zensical(config, hint=''):
    result = subprocess.run(['zensical', 'build', '--strict', '--clean', '-f', str(config)],
                            cwd=config.parent)
    if result.returncode:
        sys.exit(f'build_docs: {config.name} failed{hint}')


def translate_nav(nav, titles):
    out = []
    for item in nav:
        (title, value), = item.items()
        if isinstance(value, list):
            value = translate_nav(value, titles)
        out.append({titles.get(title, title): value})
    return out


def stage_ja(stage, project, titles):
    """Copy docs/ into stage/docs as the Japanese source tree, write its config."""
    translated = sorted(p.name[:-len('.ja.md')] for p in DOCS.glob('*.ja.md'))
    names = '|'.join(map(re.escape, translated))
    # Link targets only: the link text of a Japanese page may show the file name.
    link = re.compile(r'(\]\(|\]:[ \t]*|href=")(' + names + r')\.ja\.md(?=[)#\s"])')
    shutil.copytree(DOCS, stage / 'docs', ignore=shutil.ignore_patterns('*.ja.md', *NOT_PAGES))
    for name in translated:
        text = (DOCS / f'{name}.ja.md').read_text(encoding='utf-8')
        (stage / 'docs' / f'{name}.md').write_text(link.sub(r'\1\2.md', text), encoding='utf-8')

    config = dict(project, docs_dir='docs', site_dir='site',
                  site_url=project['site_url'] + 'ja/',
                  nav=translate_nav(project['nav'], titles),
                  theme=dict(project['theme'], language='ja'))
    # The staging tree has no .ja.md files: nothing to exclude.
    config['plugins'] = {k: v for k, v in project.get('plugins', {}).items() if k != 'exclude'}
    # JSON is YAML, which Zensical reads for any config name not ending in .toml.
    path = stage / 'zensical.yml'
    path.write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding='utf-8')
    return path


def serve(port):
    # The site links its own absolute path (site_url), so serve it under that path.
    prefix = Path(tempfile.mkdtemp())
    (prefix / 'fastdds_transport_viz').symlink_to(SITE)
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=prefix)
    print(f'http://127.0.0.1:{port}/fastdds_transport_viz/', flush=True)
    http.server.ThreadingHTTPServer(('127.0.0.1', port), handler).serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--serve', action='store_true', help='serve site/ after the build')
    parser.add_argument('--port', type=int, default=8000)
    args = parser.parse_args()

    with open(ROOT / 'zensical.toml', 'rb') as f:
        toml = tomllib.load(f)
    project, titles = toml['project'], toml['i18n']['ja']['nav']

    zensical(ROOT / 'zensical.toml')
    with tempfile.TemporaryDirectory() as tmp:
        config = stage_ja(Path(tmp), project, titles)
        zensical(config, ' (the Japanese site: a staged x.md is docs/x.ja.md when it exists)')
        shutil.rmtree(SITE / 'ja', ignore_errors=True)
        shutil.copytree(Path(tmp) / 'site', SITE / 'ja')

    if args.serve:
        serve(args.port)


if __name__ == '__main__':
    main()
