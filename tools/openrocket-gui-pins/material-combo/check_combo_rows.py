#!/usr/bin/env python3
"""Checks that MaterialComboBoxTests.cpp holds the groups and searches rows.py wrote.

usage: check_combo_rows.py ROWS TEST_FILE

The test assembles its tables by hand from rows.py's output, so they cannot be rewritten in place;
this compares what its strings say (adjacent literals joined, as the compiler joins them) with the
probe's three popups, exactly and in order:
- "Doc foam groups popup": its group lines are kDocFoamGroups, its searches docFoamSearches()
  (each Search{.text, .rows}: the text and every row), and the rows of its search 'nylon' are
  {kSearchNylon};
- "Balsa groups popup": its group lines are kDocFoamGroups' but the Woods and Foams lines, which are
  kBalsaWoods and kBalsaFoams; its one search is 'bal', whose rows are {kSearchBalsa};
- "Loose groups popup": the same with kLooseWoods and kLooseFoams, and no search.
A popup or a table missing on either side is a difference. Exit status 1, with the differences on
standard error, when there is one.
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


def constant(source, name, kind):
    """The strings of the constant @p name of @p source: "constexpr <kind> <name>{" up to the first "};"
    that ends a line."""
    m = re.search(r"^constexpr " + re.escape(kind) + r" " + name + r"\{(.*?)\};$", source, re.S | re.M)
    if not m:
        raise SystemExit(f"error: the test has no constant {name}")
    return literals(m.group(1))


def searches_of(source):
    """docFoamSearches()'s searches: (text, rows) for each Search{.text = ..., .rows = {...}}."""
    m = re.search(r"docFoamSearches\(\)\n\{\n    return \{\n(.*?)\n    \};\n\}", source, re.S)
    if not m:
        raise SystemExit("error: the test has no docFoamSearches() of the form 'return {...};'")
    chunks = m.group(1).split("Search{")
    if chunks[0].strip():
        raise SystemExit("error: docFoamSearches() holds something before its first Search")
    result = []
    for chunk in chunks[1:]:
        if not re.match(r"\s*\.text = ", chunk) or ".rows = {" not in chunk:
            raise SystemExit(f"error: a search of docFoamSearches() is not Search{{.text, .rows}}: {chunk!r}")
        values = literals(chunk)
        result.append((values[0], values[1:]))
    return result


def popups_of(rows):
    """The popups of rows.py's output: name -> (group lines, [(search text, rows)])."""
    popups = {}
    current = None
    for line in rows:
        m = re.match(r'^// probe: "(.*) groups popup:"$', line)
        if m:
            current = m.group(1)
            if current in popups:
                raise SystemExit(f"error: the popup {current!r} twice in the rows")
            popups[current] = ([], [])
        elif current is None:
            raise SystemExit(f"error: a row before the first popup: {line}")
        elif line.startswith("        Search{"):
            values = literals(line)
            popups[current][1].append((values[0], values[1:]))
        else:
            (value,) = literals(line)
            popups[current][0].append(value)
    return popups


def compare(what, probe, test, differences):
    """Records the items of the lists @p probe and @p test that differ (by index), and their lengths."""
    if probe == test:
        return
    lines = [f"{what}: {len(probe)} items in the probe, {len(test)} in the test"]
    for index in range(max(len(probe), len(test))):
        left = probe[index] if index < len(probe) else "(none)"
        right = test[index] if index < len(test) else "(none)"
        if left != right:
            lines.append(f"  [{index}] the probe: {left!r}\n  [{index}] the test:  {right!r}")
    differences.append("\n".join(lines))


def main():
    rows_path, test_path = sys.argv[1], sys.argv[2]
    with open(rows_path, encoding="utf-8") as f:
        popups = popups_of(f.read().splitlines())
    with open(test_path, encoding="utf-8") as f:
        source = f.read()
    if sorted(popups) != ["Balsa", "Doc foam", "Loose"]:
        raise SystemExit(f"error: the probe's popups are {sorted(popups)}, not Balsa, Doc foam and Loose")
    groups = constant(source, "kDocFoamGroups", "std::array<std::string_view, 8>")
    differences = []
    doc_groups, doc_searches = popups["Doc foam"]
    compare("Doc foam: the group lines (kDocFoamGroups)", doc_groups, groups, differences)
    compare("Doc foam: the searches (docFoamSearches())", doc_searches, searches_of(source), differences)
    nylon = [found for text, found in doc_searches if text == "nylon"]
    compare("Doc foam: the search 'nylon' (kSearchNylon)", nylon,
            [constant(source, "kSearchNylon", "std::string_view")], differences)
    for popup, name, searches in (("Balsa", "Balsa", [("bal", constant(source, "kSearchBalsa", "std::string_view"))]),
                                  ("Loose", "Loose", [])):
        lines, found = popups[popup]
        expected = list(groups)
        if len(expected) == 8:
            (expected[1],) = constant(source, f"k{name}Woods", "std::string_view")
            (expected[4],) = constant(source, f"k{name}Foams", "std::string_view")
        compare(f"{popup}: the group lines (kDocFoamGroups' with k{name}Woods and k{name}Foams)", lines,
                expected, differences)
        compare(f"{popup}: the searches", found, searches, differences)
    for item in differences:
        print(f"{test_path} differs from the probe: {item}", file=sys.stderr)
    checked = sum(len(lines) + len(found) for lines, found in popups.values())
    print(f"{checked} group lines and searches of {len(popups)} popups checked, {len(differences)} differences",
          file=sys.stderr)
    sys.exit(1 if differences else 0)


if __name__ == "__main__":
    main()
