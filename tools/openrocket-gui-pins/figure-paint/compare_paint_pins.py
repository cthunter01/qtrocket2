#!/usr/bin/env python3
"""Compares two RocketFigurePaintPins.h apart from Java's run-to-run order in the designs the probe builds.

usage: compare_paint_pins.py COMMITTED REGENERATED

RocketFigure paints the shapes of equal display order and the motors in the order of hash maps whose
keys hash the components' UUIDs. The UUIDs of a design the probe builds (TestRockets.makeBeta and
new designs, BUILT below) were random when the committed table was made, so that order, and the
order of the hit points the probe picks from the painted shapes, changed from run to run; the probe
now seeds them (shared/probe/DeterministicUuids.java), so a regenerated table is the same on every
run but may still be another permutation of the committed one. rocket_figure_paint_golden_tests.cpp
compares each run of shapes of equal display order as a set and looks each hit point up on its own,
but compares the motors in order (README.md, "Reproducibility"). This compares every line exactly,
except that in the kPaints and kHits tables of a scenario of BUILT a run of consecutive SHAPE pins of
one display order and a run of motors (MOTOR_FILL and MOTOR_OUTLINE pairs) are compared as sorted
lists, and a kHits table as a whole; a design loaded from a file keeps the UUIDs stored in it and is
compared byte for byte, motors and all. Exit status 0 when the files are equal so, 1 otherwise.
"""
import re
import sys

PAINT = re.compile(r"^\s*\{PaintPinKind::(\w+), -?\d+, (-?\d+),")

# The scenarios whose design FigurePaintProbe builds (TestRockets.makeBeta, new designs).
BUILT = ("beta-side", "new-side", "new-back")
BUILT_TABLES = {prefix + "".join(part.capitalize() for part in name.split("-"))
                for name in BUILT for prefix in ("kPaints", "kHits")}


def kind_of(line):
    m = PAINT.match(line)
    return (m.group(1), m.group(2)) if m else (None, None)


def normalized_paints(table):
    """The lines of a kPaints table: runs of SHAPE pins of one display order sorted, and runs of
    motors (a MOTOR_FILL and its MOTOR_OUTLINE) sorted as pairs."""
    out = []
    index = 0
    while index < len(table):
        kind, order = kind_of(table[index])
        end = index + 1
        if kind == "SHAPE":
            while end < len(table) and kind_of(table[end]) == ("SHAPE", order):
                end += 1
            out.extend(sorted(table[index:end]))
        elif kind == "MOTOR_FILL":
            pairs = []
            end = index
            while (end + 1 < len(table) and kind_of(table[end])[0] == "MOTOR_FILL"
                   and kind_of(table[end + 1])[0] == "MOTOR_OUTLINE"):
                pairs.append((table[end], table[end + 1]))
                end += 2
            if not pairs:
                out.append(table[index])
                end = index + 1
            for pair in sorted(pairs):
                out.extend(pair)
        else:
            out.append(table[index])
        index = end
    return out


def normalized(path):
    """The lines of @p path with the runs Java orders by hash sorted in the tables of BUILT."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    out = []
    table = None
    body = []
    for line in lines:
        if table is None:
            out.append(line)
            m = re.match(r"^inline constexpr auto ((kPaints|kHits)\w+) = ", line)
            if m and m.group(1) in BUILT_TABLES:
                table = "paints" if m.group(2) == "kPaints" else "hits"
            continue
        if line == "});":
            out.extend(normalized_paints(body) if table == "paints" else sorted(body))
            out.append(line)
            table = None
            body = []
            continue
        body.append(line)
    if table is not None:
        raise SystemExit(f"error: a table of {path} does not end in a line '}});'")
    return out


def main():
    committed, regenerated = sys.argv[1], sys.argv[2]
    with open(committed, encoding="utf-8") as f:
        exact = f.read()
    with open(regenerated, encoding="utf-8") as f:
        if f.read() == exact:
            print("byte for byte", file=sys.stderr)
            sys.exit(0)
    a, b = normalized(committed), normalized(regenerated)
    if a == b:
        print("equal apart from the order of shapes of equal display order, of motors and of hit "
              f"points in {', '.join(BUILT)} (Java's hash order)", file=sys.stderr)
        sys.exit(0)
    for index, (left, right) in enumerate(zip(a, b)):
        if left != right:
            print(f"first difference after normalising, line {index + 1}:\n  {left}\n  {right}",
                  file=sys.stderr)
            break
    else:
        print(f"the files differ in length: {len(a)} and {len(b)} lines", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
