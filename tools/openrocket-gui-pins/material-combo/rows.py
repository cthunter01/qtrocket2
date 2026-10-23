"""Turns the B4 probe's output (B4Probe.java) into C++ string literals for MaterialComboBoxTests.cpp:
non-ASCII characters as \\uXXXX escapes, quotes escaped. A group line is
"<title>[ [checked]]: <item> | <item> ..." with "[x] " before a checked item; a search is
{"<text>", {"<row>", ...}} with Java's check mark before the selected row."""
import sys

def lit(text):
    out = []
    for ch in text:
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ord(ch) > 127:
            out.append('\\u%04x' % ord(ch))
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'

lines = open(sys.argv[1], encoding='utf-8').read().splitlines()
for line in lines:
    if line.endswith('groups popup:'):
        print('// probe: "%s"' % line)
        continue
    if line.startswith('  menu '):
        start = len('  menu \'')
        end = line.index('\'', start)
        name = line[start:end]
        rest = line[end + 1:]
        print('    %s,' % lit(name + rest))
        continue
    if line.startswith('search \''):
        text = line[len('search \''):line.index('\':')]
        rest = line[line.index('\':') + 2:].strip()
        count, _, items = rest.partition(':')
        items = [i for i in items.strip().split(' | ') if i]
        print('        Search{.text = %s, .rows = {%s}},' % (lit(text), ', '.join(lit(i) for i in items)))
