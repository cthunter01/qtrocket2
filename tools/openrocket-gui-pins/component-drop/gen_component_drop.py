#!/usr/bin/env python3
"""Writes DndProbe's scenarios into the table of tests/gui/tree/ComponentDropTests.cpp.

usage: gen_component_drop.py DND_JAVA DND_TRANSCRIPT TEST_FILE

The scenarios (their names, dragged selections, destinations, indexes and actions) come from the
probe's source, their transcripts from what it printed: a block per scenario, from its
"== <label>" line to the next. EDITS below holds the rows where QtRocket's expectation differs
from OpenRocket's transcript on purpose: the lines replaced, the comment the test file has above
the row and the reason. The rows replace the body of dndScenarios()'s kScenarios (up to "    };")
in TEST_FILE; the caller formats the file afterwards (clang-format aligns the designated
initializers as the committed file has them).
"""
import re
import sys

U4_DROP = (
    "the user's decision U4: a drop is validated whole and then moves every dragged component, in tree "
    "order; Java's importData() stops at the first component already at its place or whose source "
    "index the earlier moves changed (the comment above the row)")
TREE_ORDER = (
    "QtRocket drags the selected components in tree order; Java's transferable has them in descending "
    "child position (the selection's order between equal positions); the same result (the comment "
    "above the row)")

# Row name -> (the comment lines above the row; [(OpenRocket's line, QtRocket's line)...]; the
# reason). Every edit must apply: an OpenRocket line that is no longer in the transcript stops the
# generator.
EDITS = {
    "MoveNCBT2ToSustainer1": ([
        "        // Decision U4: validated whole, then moved together, in tree order. Java drags [BT2, NC],",
        "        // moves BT2 and then stops at NC, already at its place: \"importData=false\" with this tree.",
    ], [
        ("  dragged (transferable order): [BT2, NC]",
         "  dragged (transferable order): [NC, BT2]"),
        ("  importData=false tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], Booster] "
         "undo='Move components'",
         "  importData=true tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], Booster] "
         "undo='Move components'"),
    ], U4_DROP),
    "MoveNCBT2ToSustainer3": ([
        "        // Decision U4: Java drags [BT2, NC] and stops at once, BT2 being at its place",
        "        // (\"importData=false\", nothing moved, undo 'null'); here both go to the end, next to each",
        "        // other.",
    ], [
        ("  dragged (transferable order): [BT2, NC]",
         "  dragged (transferable order): [NC, BT2]"),
        ("  importData=false tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='null'",
         "  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster] "
         "undo='Move components'"),
    ], U4_DROP),
    "MoveBT2NCToSustainer3": ([
        "        // As the one before: the order of the selection does not matter.",
    ], [
        ("  dragged (transferable order): [BT2, NC]",
         "  dragged (transferable order): [NC, BT2]"),
        ("  importData=false tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='null'",
         "  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster] "
         "undo='Move components'"),
    ], U4_DROP),
    "MoveCR1FinsToBT21": ([
        "        // The dragged components in tree order (Java: descending child position, [Fins, CR1]); the",
        "        // same result.",
    ], [
        ("  dragged (transferable order): [Fins, CR1]",
         "  dragged (transferable order): [CR1, Fins]"),
    ], TREE_ORDER),
    "MoveCR1FinsToSustainer0FinsNotAllowedThere": ([
        "        // The dragged components in tree order (Java: [Fins, CR1]).",
    ], [
        ("  dragged (transferable order): [Fins, CR1]",
         "  dragged (transferable order): [CR1, Fins]"),
    ], TREE_ORDER),
    "MoveITChuteToBT20DifferentParents": ([
        "        // Decision U4: Java moves IT, then stops at Chute with the source index it took before the",
        "        // move (\"importData=false\" with this tree); here both move, with the same tree.",
    ], [
        ("  importData=false tree=Rocket[Sustainer[NC, BT1[CR1, Fins], BT2[IT[EB], Chute]], Booster] "
         "undo='Move components'",
         "  importData=true tree=Rocket[Sustainer[NC, BT1[CR1, Fins], BT2[IT[EB], Chute]], Booster] "
         "undo='Move components'"),
    ], U4_DROP),
    "MoveChuteITToBT10DifferentParents": ([
        "        // Decision U4: Java drags [Chute, IT] (equal child positions: the selection's order), moves",
        "        // Chute to BT1's front and stops at IT (\"importData=false tree=Rocket[Sustainer[NC,",
        "        // BT1[Chute, IT[EB], CR1, Fins], BT2], Booster]\"); here both move in tree order.",
    ], [
        ("  dragged (transferable order): [Chute, IT]",
         "  dragged (transferable order): [IT, Chute]"),
        ("  importData=false tree=Rocket[Sustainer[NC, BT1[Chute, IT[EB], CR1, Fins], BT2], Booster] "
         "undo='Move components'",
         "  importData=true tree=Rocket[Sustainer[NC, BT1[IT[EB], Chute, CR1, Fins], BT2], Booster] "
         "undo='Move components'"),
    ], U4_DROP),
    "CopyITCR1ToBT21": ([
        "        // The dragged components in tree order (Java: [CR1, IT], copied in turn at index 1); the",
        "        // same result.",
    ], [
        ("  dragged (transferable order): [CR1, IT]",
         "  dragged (transferable order): [IT, CR1]"),
    ], TREE_ORDER),
}


def blocks(path):
    """The blocks of a transcript: label -> its lines ("== <label>" first)."""
    result = {}
    current = None
    with open(path, encoding="utf-8") as f:
        for line in f.read().splitlines():
            if line.startswith("[openrocket]"):
                continue
            if line.startswith("== "):
                current = line[3:]
                if current in result:
                    raise SystemExit(f"error: {path}: the block '{current}' twice")
                result[current] = [line]
            elif current is not None:
                result[current].append(line)
    return result


def ident(label):
    return "".join(w[:1].upper() + w[1:] for w in re.findall(r"[A-Za-z0-9]+", label))


def main():
    java_path, transcript_path, test_path = sys.argv[1:4]
    with open(java_path, encoding="utf-8") as f:
        source = f.read()
    found = blocks(transcript_path)
    pattern = re.compile(r'drop\("([^"]*)", new String\[\] \{([^}]*)\}, "([^"]*)", (-?\d+), '
                         r'TransferHandler\.(MOVE|COPY)\);')
    rows, labels, used = [], set(), set()
    for m in pattern.finditer(source):
        label, selection, destination, index, action = m.groups()
        if label not in found:
            raise SystemExit(f"error: {transcript_path} has no block '{label}'")
        labels.add(label)
        name = ident(label)
        transcript = list(found[label])
        comment = []
        if name in EDITS:
            comment, replacements, _reason = EDITS[name]
            used.add(name)
            for old, new in replacements:
                hits = [i for i, line in enumerate(transcript) if line == old]
                if len(hits) != 1:
                    raise SystemExit(f"error: {name}: the line to edit is {len(hits)} times in the "
                                     f"transcript, not once: {old}")
                transcript[hits[0]] = new
        for line in transcript:
            if ')"' in line:
                raise SystemExit(f"error: a transcript line ends the raw string: {line}")
        selected = ", ".join(f'"{s}"' for s in re.findall(r'"([^"]*)"', selection))
        fields = [
            f'.name = "{name}"',
            f".selection = {{{selected}}}",
            f'.destination = "{destination}"',
            # DndProbe's index -1 (a drop on the component) is the index 0 of its importData().
            f".index = {max(int(index), 0)}",
            ".action = Qt::" + ("MoveAction" if action == "MOVE" else "CopyAction"),
            '.transcript = R"(' + "\n".join(transcript) + ')"',
        ]
        rows += comment + ["        {" + ",\n     ".join(fields) + "},"]
    if not rows:
        raise SystemExit(f"error: no scenario found in {java_path}")
    unknown = sorted(set(found) - labels)
    if unknown:
        raise SystemExit(f"error: DndProbe printed blocks that are no scenario of the table: {unknown}")
    missing = sorted(set(EDITS) - used)
    if missing:
        raise SystemExit(f"error: edits for rows that are not in the table: {missing}")

    with open(test_path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    start = "    static const std::vector<DndScenario> kScenarios{"
    starts = [i for i, line in enumerate(lines) if line == start]
    if len(starts) != 1:
        raise SystemExit(f"error: {test_path} has {len(starts)} lines '{start}', not one")
    end = next((i for i in range(starts[0] + 1, len(lines)) if lines[i] == "    };"), None)
    if end is None:
        raise SystemExit(f"error: no '    }};' after '{start}' in {test_path}")
    lines[starts[0] + 1:end] = "\n".join(rows).split("\n")
    with open(test_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
