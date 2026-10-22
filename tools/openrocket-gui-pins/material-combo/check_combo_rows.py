#!/usr/bin/env python3
"""Checks that MaterialComboBoxTests.cpp holds the texts rows.py wrote.

usage: check_combo_rows.py ROWS TEST_FILE

The test assembles its tables by hand from rows.py's output (the groups popup with Doc foam
selected, its searches, and single lines of the popups with Balsa and with a loose material
selected), so they cannot be rewritten in place. This compares what the strings say instead:
every group line of ROWS must be one string of TEST_FILE (adjacent literals joined, as the compiler
joins them), and the rows of every search must follow one another there (a search without rows:
its text must be there). Exit status 1, with the missing texts on standard error, when one is not.
"""
import re
import sys

ESCAPES = {'"': '"', "\\": "\\", "'": "'", "n": "\n", "t": "\t", "0": "\0", "?": "?"}


def decode(body):
    """The text of a C++ string literal's body (no prefix, no raw strings)."""
    out = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ch != "\\":
            out.append(ch)
            i += 1
            continue
        kind = body[i + 1]
        if kind == "u":
            out.append(chr(int(body[i + 2:i + 6], 16)))
            i += 6
        elif kind == "U":
            out.append(chr(int(body[i + 2:i + 10], 16)))
            i += 10
        elif kind == "x":
            m = re.match(r"[0-9a-fA-F]+", body[i + 2:])
            out.append(chr(int(m.group(0), 16)))
            i += 2 + len(m.group(0))
        else:
            out.append(ESCAPES[kind])
            i += 2
    return "".join(out)


def literals(source):
    """The strings of @p source in order, adjacent literals joined, comments skipped."""
    found = []  # (start, end, body)
    i = 0
    n = len(source)
    while i < n:
        if source.startswith("//", i):
            i = source.find("\n", i)
            i = n if i < 0 else i
        elif source.startswith("/*", i):
            i = source.index("*/", i) + 2
        elif source[i] == "'":
            m = re.compile(r"'(?:[^'\\\n]|\\.)*'").match(source, i)
            i = m.end()
        elif source[i] == '"':
            m = re.compile(r'"((?:[^"\\\n]|\\.)*)"').match(source, i)
            found.append((m.start(), m.end(), m.group(1)))
            i = m.end()
        else:
            i += 1
    joined = []
    previous_end = None
    for start, end, body in found:
        if previous_end is not None and source[previous_end:start].strip() == "":
            joined[-1] += decode(body)
        else:
            joined.append(decode(body))
        previous_end = end
    return joined


def main():
    rows_path, test_path = sys.argv[1], sys.argv[2]
    with open(rows_path, encoding="utf-8") as f:
        rows = f.read().splitlines()
    with open(test_path, encoding="utf-8") as f:
        texts = literals(f.read())
    present = set(texts)
    missing = []
    groups = searches = 0
    for line in rows:
        if line.startswith("// probe: "):
            continue
        if line.startswith("        Search{"):
            searches += 1
            values = literals(line)
            text, found = values[0], values[1:]
            if not found:
                if text not in present:
                    missing.append(f"search '{text}': its text")
                continue
            runs = [i for i in range(len(texts) - len(found) + 1) if texts[i:i + len(found)] == found]
            if not runs:
                missing.append(f"search '{text}': its {len(found)} rows in a row")
        else:
            groups += 1
            (value,) = literals(line)
            if value not in present:
                missing.append(f"group line {value!r}")
    for item in missing:
        print(f"missing in {test_path}: {item}", file=sys.stderr)
    print(f"{groups} group lines and {searches} searches checked, {len(missing)} missing", file=sys.stderr)
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
