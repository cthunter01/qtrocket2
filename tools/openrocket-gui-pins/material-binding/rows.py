# Turns the MaterialModel part of B2Probe's output into C++ string literal rows (non-ASCII as \u escapes).
# usage: python3 -I rows.py PROBE_OUTPUT > ROWS   (generate.sh splices ROWS in with splice_material_rows.py)
import sys, re
lines = open(sys.argv[1], encoding='utf-8').read().split('\n')
def esc(t):
    out = []
    for ch in t:
        if ch == '"' or ch == '\\':
            out.append('\\' + ch)
        elif ord(ch) > 127:
            out.append('\\u%04x' % ord(ch))
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'
section = None
blocks = {}
for line in lines:
    m = re.match(r'^(BULK \(with user and document materials\)|LINE|SURFACE): size=', line)
    if m:
        section = m.group(1).split(' ')[0] + '_ROWS'
        blocks[section] = []
        continue
    m = re.match(r'^(BULK|LINE|SURFACE) groups:', line)
    if m:
        section = m.group(1) + '_GROUPS'
        blocks[section] = []
        continue
    if line.startswith('BULK (built-in only)') or line.startswith('=='):
        section = None
        continue
    if section and section.endswith('_ROWS'):
        m = re.match(r'^  (\d+) (.*)$', line)
        if m:
            blocks[section].append(m.group(2))
    elif section and section.endswith('_GROUPS'):
        m = re.match(r'^  (\S+: .*)$', line)
        if m:
            blocks[section].append(m.group(1))
        else:
            section = None
for name, rows in blocks.items():
    print('// %s %d' % (name, len(rows)))
    for r in rows:
        print('    ' + esc(r) + ',')
