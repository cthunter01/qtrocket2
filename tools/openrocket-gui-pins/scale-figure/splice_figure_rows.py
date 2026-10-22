#!/usr/bin/env python3
"""Puts the rows figure_rows.py wrote into the table kFigureRows of ScaleFigureTests.cpp.

usage: splice_figure_rows.py ROWS TEST_FILE

The rows replace every line between "constexpr auto kFigureRows = std::to_array<FigureRow>({" and
the next "});" of TEST_FILE (a table in a "clang-format off" block, so the rows stay as written);
the rest of the file is left as it is.
"""
import sys

START = "constexpr auto kFigureRows = std::to_array<FigureRow>({"
END = "});"


def main():
    rows_path, test_path = sys.argv[1], sys.argv[2]
    with open(rows_path, encoding="utf-8") as f:
        rows = f.read().splitlines()
    with open(test_path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    starts = [i for i, line in enumerate(lines) if line == START]
    if len(starts) != 1:
        raise SystemExit(f"error: {test_path} has {len(starts)} lines '{START}', not one")
    start = starts[0]
    end = next((i for i in range(start + 1, len(lines)) if lines[i] == END), None)
    if end is None:
        raise SystemExit(f"error: no '{END}' after '{START}' in {test_path}")
    lines[start + 1:end] = rows
    with open(test_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
