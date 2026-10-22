#!/usr/bin/env python3
"""Puts the rows rows.py wrote into the six tables of MaterialBindingTests.cpp.

usage: splice_material_rows.py ROWS TEST_FILE

ROWS holds blocks "// <NAME> <count>" (BULK_ROWS, BULK_GROUPS, LINE_ROWS, LINE_GROUPS, SURFACE_ROWS,
SURFACE_GROUPS), each followed by its rows, one string literal per line. A block replaces the body
of the table k<Name> of TEST_FILE (BULK_ROWS: kBulkRows, a "constexpr std::array<std::string_view,
<count>> kBulkRows{" up to the next "};"), with the table's size and the count of the comment above
it ("// probe: ..., <count> rows.") set to the block's. The caller formats the file afterwards
(clang-format splits the long literals as the committed file has them).
"""
import re
import sys


def table_name(block):
    return "k" + "".join(part.capitalize() for part in block.split("_"))


def main():
    rows_path, test_path = sys.argv[1], sys.argv[2]
    blocks = {}
    order = []
    with open(rows_path, encoding="utf-8") as f:
        for line in f.read().splitlines():
            m = re.match(r"^// ([A-Z_]+) (\d+)$", line)
            if m:
                current = m.group(1)
                blocks[current] = []
                order.append(current)
                continue
            blocks[current].append(line)
    with open(test_path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    for block in order:
        name = table_name(block)
        pattern = re.compile(r"^constexpr std::array<std::string_view, \d+> " + name + r"\{$")
        starts = [i for i, line in enumerate(lines) if pattern.match(line)]
        if len(starts) != 1:
            raise SystemExit(f"error: {test_path} has {len(starts)} definitions of {name}, not one")
        start = starts[0]
        end = next((i for i in range(start + 1, len(lines)) if lines[i] == "};"), None)
        if end is None:
            raise SystemExit(f"error: no '}};' after {name} in {test_path}")
        rows = blocks[block]
        lines[start] = f"constexpr std::array<std::string_view, {len(rows)}> {name}{{"
        if start > 0:
            lines[start - 1] = re.sub(r", \d+ rows\.$", f", {len(rows)} rows.", lines[start - 1])
        lines[start + 1:end] = rows
    with open(test_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
