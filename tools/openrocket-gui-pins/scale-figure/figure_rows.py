"""Turns the '== figure' lines of ScaleProbe's output into the rows of ScaleFigureTests.cpp's table.

usage: python3 -I figure_rows.py PROBE_OUTPUT > ROWS   (generate.sh splices ROWS in with splice_figure_rows.py)
Rows where Java accepted a zero fit scale (exactly 60 x 40 visible) are given the values of a figure that
refused it (QtRocket refuses a zero scale): those of the 59 x 100 row of the same design and view.
"""
import re
import sys

rows = []
with open(sys.argv[1], encoding='utf-8') as f:
    for line in f:
        if not line.startswith('== figure '):
            continue
        m = re.match(r'== figure (.*?) (Side|Top|Back) view visible (\d+)x(\d+) base \S+ subject \(([^)]*)\) '
                     r'fit (\S+) changed (\S+) user (\S+) abs \S+ pref (\d+)x(\d+) size (\d+)x(\d+) '
                     r'origin (-?\d+),(-?\d+) zoompt (-?\d+),(-?\d+)', line)
        assert m, line
        rows.append(m.groups())
by_key = {(r[0], r[1], r[2], r[3]): r for r in rows}
names = {'A simple model rocket.ork': 'simple', 'Two stage high power rocket.ork': 'twoStage', 'empty': 'empty'}


def d(x):
    return {'NaN': 'kNaN', 'Infinity': 'kInfinity', '-Infinity': '-kInfinity'}.get(x, x)


for r in rows:
    name, view, vw, vh, subj, fit, changed, user, pw, ph, sw, sh, ox, oy, zx, zy = r
    zero = fit == '0.0'
    if zero:
        ref = by_key[(name, view, '59', '100')]
        changed, user, pw, ph = 'false', '1.0', ref[8], ref[9]
        sw, sh = str(max(int(pw), int(vw) + 3)), str(max(int(ph), int(vh) + 3))
        ox, oy, zx, zy = ref[12], ref[13], ref[14], ref[15]
        assert (sw, sh) == (ref[10], ref[11]), (r, ref)
    x, y, w, h = subj.split(',')
    print('    {"%s", FigureView::%s, {%s, %s, %s, %s}, {%s, %s}, %s, %s, %s, {%s, %s}, {%s, %s}, {%s, %s}, {%s, %s}, %s},'
          % (names[name], view.upper(), x, y, w, h, vw, vh, d(fit), changed, d(user), pw, ph, sw, sh, ox, oy, zx, zy,
             'true' if zero else 'false'))
