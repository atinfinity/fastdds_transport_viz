#!/usr/bin/env python3
# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Render terminal output with ANSI SGR colors (as printed by transport_viz --color always)
into an SVG that looks like a dark terminal. Standard library only.

    ansi2svg.py input.ansi output.svg [--title "text"] [--max-cols N]

Supported sequences: reset (0), bold (1), dim (2), foreground 30-37 / 90-97, background
40-47 (ignored), and truecolor 38;2;r;g;b. Anything else is dropped.
"""
import argparse
import html
import re

PALETTE = {
    30: '#3b4252', 31: '#e06c75', 32: '#4ec97a', 33: '#e5c07b', 34: '#61afef', 35: '#c678dd', 36: '#56b6c2', 37: '#d4d4d4',
    90: '#7f848e', 91: '#f28b82', 92: '#7ee787', 93: '#f2cc60', 94: '#8ac4ff', 95: '#d8a8ff', 96: '#8be9fd', 97: '#ffffff',
}
FG_DEFAULT = '#d4d4d4'
BG = '#1e1e1e'
CHAR_W, LINE_H, FONT_SIZE, PAD = 8.4, 19, 14, 14
SGR = re.compile(r'\x1b\[([0-9;]*)m')


def parse(text):
    """Yield lines; each line is a list of (text, fg, bold, dim) runs."""
    for raw in text.split('\n'):
        runs, fg, bold, dim, pos = [], None, False, False, 0
        for m in SGR.finditer(raw):
            if m.start() > pos:
                runs.append((raw[pos:m.start()], fg, bold, dim))
            params = [int(p) if p else 0 for p in m.group(1).split(';')] or [0]
            i = 0
            while i < len(params):
                p = params[i]
                if p == 0:
                    fg, bold, dim = None, False, False
                elif p == 1:
                    bold = True
                elif p == 2:
                    dim = True
                elif p == 22:
                    bold = dim = False
                elif p == 39:
                    fg = None
                elif p in PALETTE:
                    fg = PALETTE[p]
                elif p == 38 and i + 4 < len(params) and params[i + 1] == 2:
                    fg = '#%02x%02x%02x' % tuple(params[i + 2:i + 5])
                    i += 4
                i += 1
            pos = m.end()
        if pos < len(raw):
            runs.append((raw[pos:], fg, bold, dim))
        yield runs


def truncate(line, max_cols):
    """Cut a run list to max_cols visible characters, ending with an ellipsis."""
    if max_cols <= 0 or sum(len(r[0]) for r in line) <= max_cols:
        return line
    out, used = [], 0
    for s, fg, bold, dim in line:
        room = max_cols - 1 - used
        if room <= 0:
            break
        out.append((s[:room], fg, bold, dim))
        used += min(len(s), room)
    out.append(('…', None, False, True))
    return out


def render(text, title=None, max_cols=0):
    lines = [truncate(line, max_cols) for line in parse(text)]
    while lines and not ''.join(r[0] for r in lines[-1]).strip():
        lines.pop()
    cols = max((sum(len(r[0]) for r in line) for line in lines), default=1)
    title_h = LINE_H if title else 0
    width = PAD * 2 + cols * CHAR_W
    height = PAD * 2 + title_h + len(lines) * LINE_H
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" '
        f'viewBox="0 0 {width:.0f} {height:.0f}" font-family="SFMono-Regular, Menlo, Consolas, \'DejaVu Sans Mono\', monospace" '
        f'font-size="{FONT_SIZE}">',
        f'<rect width="100%" height="100%" rx="8" fill="{BG}"/>',
    ]
    y = PAD + FONT_SIZE
    if title:
        out.append(f'<text x="{PAD}" y="{y}" fill="#7f848e">{html.escape(title)}</text>')
        y += LINE_H
    for line in lines:
        parts, x = [], PAD
        for s, fg, bold, dim in line:
            if not s:
                continue
            attrs = [f'fill="{fg or FG_DEFAULT}"']
            if bold:
                attrs.append('font-weight="bold"')
            if dim:
                attrs.append('opacity="0.55"')
            # fixed advance per character keeps columns aligned whatever font the viewer picks
            attrs.append(f'textLength="{len(s) * CHAR_W:.1f}" lengthAdjust="spacingAndGlyphs"')
            parts.append(f'<tspan x="{x:.1f}" {" ".join(attrs)} xml:space="preserve">{html.escape(s)}</tspan>')
            x += len(s) * CHAR_W
        out.append(f'<text y="{y}" xml:space="preserve">{"".join(parts)}</text>')
        y += LINE_H
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('input')
    ap.add_argument('output')
    ap.add_argument('--title', help='dimmed first line, e.g. the command that produced the output')
    ap.add_argument('--max-cols', type=int, default=0, help='truncate lines to this many visible columns (0 = never)')
    a = ap.parse_args()
    with open(a.input, encoding='utf-8', errors='replace') as f:
        text = f.read()
    with open(a.output, 'w', encoding='utf-8') as f:
        f.write(render(text, a.title, a.max_cols))


if __name__ == '__main__':
    main()
