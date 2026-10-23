#!/usr/bin/env python3
"""Checks the lines of a probe's transcript that tests pin by hand against the committed snapshot.

usage: check_hand_pins.py TRANSCRIPT SNAPSHOT NEW [--exclude REGEX | --exclude-in SECTION REGEX]...

A probe prints more than its family's table: values that hand-written tests pin (the scale
selector's items, the bindings' values, the enum texts, ...). Those cannot be rewritten in place,
so the family keeps the lines of the transcript that no table holds in SNAPSHOT, as the probe
printed them when the tests were written: every line but those that match an --exclude pattern and
those that match the REGEX of an --exclude-in inside its section (from a line that matches SECTION
to the last of the lines after it that start with two spaces). This writes the lines of TRANSCRIPT
so selected to NEW and compares them with SNAPSHOT: exit status 0 when they are equal, 1 (with the
difference on standard error) when OpenRocket now prints something else. The snapshot is never
rewritten here: review the tests that pin the changed lines (README.md names them), then copy NEW
over SNAPSHOT.
"""
import difflib
import re
import sys


def main():
    transcript_path, snapshot_path, new_path = sys.argv[1:4]
    excludes = []
    sectioned = []  # (section pattern, line pattern)
    arguments = sys.argv[4:]
    while arguments:
        if arguments[0] == "--exclude" and len(arguments) >= 2:
            excludes.append(re.compile(arguments[1]))
            arguments = arguments[2:]
        elif arguments[0] == "--exclude-in" and len(arguments) >= 3:
            sectioned.append((re.compile(arguments[1]), re.compile(arguments[2])))
            arguments = arguments[3:]
        else:
            raise SystemExit("usage: check_hand_pins.py TRANSCRIPT SNAPSHOT NEW "
                             "[--exclude REGEX | --exclude-in SECTION REGEX]...")
    lines = []
    section = None  # the line pattern of the section the line is in
    with open(transcript_path, encoding="utf-8") as f:
        for line in f.read().splitlines():
            if section is not None and not line.startswith("  "):
                section = None
            starts = [inner for outer, inner in sectioned if outer.search(line)]
            if starts:
                section = starts[0]
            elif section is not None and section.search(line):
                continue
            if not any(p.search(line) for p in excludes):
                lines.append(line)
    if not lines:
        print(f"error: {transcript_path} has no line outside the tables", file=sys.stderr)
        sys.exit(1)
    with open(new_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines) + "\n")
    with open(snapshot_path, encoding="utf-8") as f:
        snapshot = f.read().splitlines()
    if lines == snapshot:
        print(f"{len(lines)} hand-pinned lines as in {snapshot_path}", file=sys.stderr)
        sys.exit(0)
    diff = list(difflib.unified_diff(snapshot, lines, snapshot_path, new_path, lineterm="", n=1))
    for line in diff[:40]:
        print(line, file=sys.stderr)
    if len(diff) > 40:
        print(f"... ({len(diff) - 40} more lines of difference)", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
