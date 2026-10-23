#!/usr/bin/env python3
"""Writes ActionsProbe's and ClipboardProbe's scenarios into tests/gui/main/ComponentEditingTests.cpp.

usage: gen_component_editing.py ACTIONS_JAVA ACTIONS_TRANSCRIPT CLIPBOARD_JAVA CLIPBOARD_TRANSCRIPT TEST_FILE

The scenarios (their names, selections, actions and pastes) come from the probes' sources, their
transcripts from what the probes printed: a block per scenario, from its "== <label>" line to the
next, without the exception each action records at its end (parentFrame.selectTab() on the frame
the probe has not; " [NullPointerException at RocketActions$CutAction:...]"). ACTIONS_EDITS and
CLIPBOARD_EDITS below hold the rows where QtRocket's expectation differs from OpenRocket's
transcript on purpose: the lines replaced, the comment the test file has above the row and the
reason. The rows replace the body of
makeActionsScenarios() ("    return {" up to "    };") and of clipboardScenarios()'s kScenarios
(up to "    };") in TEST_FILE; the caller formats the file afterwards (clang-format aligns the
designated initializers as the committed file has them).
"""
import re
import sys

ACTIONS = {"CutAction": "CUT", "CopyAction": "COPY", "DuplicateAction": "DUPLICATE",
           "DeleteAction": "DELETE", "MoveUpAction": "MOVE_UP", "MoveDownAction": "MOVE_DOWN"}

# ActionsProbe's blocks that are no scenario of the table: ComponentEditingTests.cpp pins each in a
# test of its own.
UNTABLED = {"delete the only stage", "copy [BT1] in one document, paste into another",
            "the clipboard holds copies: changing the original after copy does not change the paste"}

# Why a row differs from OpenRocket's transcript (the test file's comments say the same).
REMOVED_SELECTION = (
    "after Cut and Delete the probe's selection model, which belongs to no tree, still names the removed "
    "components; OpenRocket's tree selects each selected component again, or the nearest of its "
    "ancestors that remains (ComponentTreeModel.fireTreeStructureChanged(), UndoRedoSelectionTest), and "
    "so does QtRocket's ComponentTreeView: the selection after a Cut or a Delete is the view's (the "
    "comment above the table)")
CUT_ALL_STAGES = (
    "the user's decision U4: Java's Cut of every stage throws half-way (the comment above the row)")
DISABLED_MOVE = (
    "the move is disabled for this selection and QtRocket's operation does nothing; the probe calls "
    "Java's action all the same, which moves or adds an empty undo step (the comment above the row)")

# The tree of the probe's design, for EDITS.
TREE = "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]"

# Row name -> (the comment lines above the row, or None; [(OpenRocket's line, QtRocket's line)...];
# the reason). Every edit must apply: an OpenRocket line that is no longer in the transcript stops
# the generator.
ACTIONS_EDITS = {
    "CutBT1": (None, [
        ("  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1] undo='Cut components'",
         "  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[Sustainer] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "CutBT1IT": (None, [
        ("  clipboard=[BT1*[IT*[EB*]]] selection=[BT1, IT] undo='Cut components'",
         "  clipboard=[BT1*[IT*[EB*]]] selection=[Sustainer] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "CutBooster": (None, [
        ("  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]]] selection=[Booster] undo='Cut components'",
         "  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]]] selection=[Rocket] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "CutSustainerBoosterAllStages": ([
        "        // Decision U4: Java deletes the completed selection one by one and throws (Report bug!",
        "        // Component Stage not deletable, IllegalArgumentException) with",
        "        // \"tree=Rocket[Sustainer[NC]]\"; here the tops go by Delete's rule and the last stage",
        "        // stays, as Delete keeps it.",
    ], [
        ("  CutAction: tree=Rocket[Sustainer[NC]]",
         "  CutAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]]]"),
        ("  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], "
         "BT2*[Chute*]]] selection=[Sustainer, Booster] undo='Cut components'",
         "  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], "
         "BT2*[Chute*]]] selection=[Rocket, Booster] undo='Cut components'"),
    ], CUT_ALL_STAGES),
    "CutPodsBoosterParallelStage": (None, [
        ("  clipboard=[Side boosters*[PBT*]] selection=[Side boosters] undo='Cut components'",
         "  clipboard=[Side boosters*[PBT*]] selection=[BBT] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "DeleteBT1": (None, [
        ("  clipboard=[] selection=[BT1] undo='Delete Body Tube'",
         "  clipboard=[] selection=[Sustainer] undo='Delete Body Tube'"),
    ], REMOVED_SELECTION),
    "DeleteNCBT2IT": (None, [
        ("  clipboard=[] selection=[NC, BT2, IT] undo='Delete components'",
         "  clipboard=[] selection=[Sustainer, BT1] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "DeleteBT1ITParentAndChild": (None, [
        ("  clipboard=[] selection=[BT1, IT] undo='Delete components'",
         "  clipboard=[] selection=[Sustainer] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "DeleteBooster": (None, [
        ("  clipboard=[] selection=[Booster] undo='Delete Stage'",
         "  clipboard=[] selection=[Rocket] undo='Delete Stage'"),
    ], REMOVED_SELECTION),
    "DeleteSustainerBoosterAllStages": (None, [
        ("  clipboard=[] selection=[Sustainer, Booster] undo='Delete components'",
         "  clipboard=[] selection=[Rocket, Booster] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "MoveUpNCFirstChild": ([
        "        // Move up is disabled here, and the operation does nothing. The probe calls Java's",
        "        // action all the same, which adds the undo position \"Move Nose Cone\" and fires its",
        "        // TREE_CHANGE: an undo step that changes nothing.",
    ], [
        ("  clipboard=[] selection=[NC] undo='Move Nose Cone'",
         "  clipboard=[] selection=[NC] undo='null'"),
        ("  after undo: tree=" + TREE + " redo='Move Nose Cone'",
         "  after undo: tree=Rocket[Sustainer] redo='null'"),
    ], DISABLED_MOVE),
    "MoveUpITBT2ITIsFirstInBT1": ([
        "        // Move up is disabled (IT cannot move), and the operation does nothing; Java's action,",
        "        // called all the same, moves BT2 (\"tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB],",
        "        // CR1, Fins]], ...\", undo 'Move components').",
    ], [
        ("  MoveUpAction: tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], "
         "Booster[BBT[Side boosters[PBT]]]]",
         "  MoveUpAction: tree=" + TREE),
        ("  clipboard=[] selection=[IT, BT2] undo='Move components'",
         "  clipboard=[] selection=[IT, BT2] undo='null'"),
        ("  after undo: tree=" + TREE + " redo='Move components'",
         "  after undo: tree=Rocket[Sustainer] redo='null'"),
    ], DISABLED_MOVE),
    "MoveDownBT2LastChild": ([
        "        // Move down is disabled here, and the operation does nothing; Java's action, called all",
        "        // the same, makes the empty undo step \"Move Body Tube\".",
    ], [
        ("  clipboard=[] selection=[BT2] undo='Move Body Tube'",
         "  clipboard=[] selection=[BT2] undo='null'"),
        ("  after undo: tree=" + TREE + " redo='Move Body Tube'",
         "  after undo: tree=Rocket[Sustainer] redo='null'"),
    ], DISABLED_MOVE),
}

CLIPBOARD_EDITS = {
    "CutCR1": (None, [
        ("  clipboard=[CR1*] selection=[CR1] undo='Cut Centering Ring'",
         "  clipboard=[CR1*] selection=[BT1] undo='Cut Centering Ring'"),
    ], REMOVED_SELECTION),
    "CutITChute": (None, [
        ("  clipboard=[IT*[EB*], Chute*] selection=[IT, Chute] undo='Cut components'",
         "  clipboard=[IT*[EB*], Chute*] selection=[BT1, BT2] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "CutFinsNC": (None, [
        ("  clipboard=[Fins*, NC*] selection=[Fins, NC] undo='Cut components'",
         "  clipboard=[Fins*, NC*] selection=[BT1, Sustainer] undo='Cut components'"),
    ], REMOVED_SELECTION),
    "DeleteITBT1ChildFirst": (None, [
        ("  clipboard=[] selection=[IT, BT1] undo='Delete components'",
         "  clipboard=[] selection=[Sustainer] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "DeleteFinsNCChute": (None, [
        ("  clipboard=[] selection=[Fins, NC, Chute] undo='Delete components'",
         "  clipboard=[] selection=[BT1, Sustainer, BT2] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "DeleteBoosterSustainerAllStagesReverse": (None, [
        ("  clipboard=[] selection=[Booster, Sustainer] undo='Delete components'",
         "  clipboard=[] selection=[Booster, Rocket] undo='Delete components'"),
    ], REMOVED_SELECTION),
    "DeleteSideBoosters": (None, [
        ("  clipboard=[] selection=[Side boosters] undo='Delete Booster Set'",
         "  clipboard=[] selection=[BBT] undo='Delete Booster Set'"),
    ], REMOVED_SELECTION),
    "MoveUpFinsITITFirstInBT1": ([
        "        // Move up is disabled (IT cannot move), and the operation does nothing; Java's action,",
        "        // called all the same, moves Fins (\"tree=Rocket[Sustainer[NC, BT1[IT[EB], Fins, CR1],",
        "        // BT2[Chute]], ...\", selection=[IT, Fins] undo='Move components').",
    ], [
        ("  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], Fins, CR1], BT2[Chute]], "
         "Booster[BBT[Side boosters[PBT]]]]",
         "  MoveUpAction: tree=" + TREE),
        ("  clipboard=[] selection=[IT, Fins] undo='Move components'",
         "  clipboard=[] selection=[Fins, IT] undo='null'"),
    ], DISABLED_MOVE),
    "MoveDownNCChute": ([
        "        // Move down is disabled (Chute cannot move), and the operation does nothing; Java's action,",
        "        // called all the same, moves NC (\"tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC,",
        "        // BT2[Chute]], ...\", undo 'Move components').",
    ], [
        ("  MoveDownAction: tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], "
         "Booster[BBT[Side boosters[PBT]]]]",
         "  MoveDownAction: tree=" + TREE),
        ("  clipboard=[] selection=[NC, Chute] undo='Move components'",
         "  clipboard=[] selection=[NC, Chute] undo='null'"),
    ], DISABLED_MOVE),
}


def blocks(path):
    """The blocks of a transcript: label -> its lines ("== <label>" first), the exception notes cut."""
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
                continue
            if current is None:
                continue
            result[current].append(re.sub(r" \[[A-Za-z]+Exception at [^\]]*\]", "", line))
    return result


def ident(label):
    return "".join(w[:1].upper() + w[1:] for w in re.findall(r"[A-Za-z0-9]+", label))


def names(text):
    return re.findall(r'"([^"]*)"', text)


def quoted(values):
    return "{" + ", ".join(f'"{v}"' for v in values) + "}"


def apply_edits(name, transcript, edits, used):
    """The transcript of row @p name with its EDITS applied, and the comment lines above the row."""
    if name not in edits:
        return transcript, []
    comment, replacements, _reason = edits[name]
    used.add(name)
    for old, new in replacements:
        hits = [i for i, line in enumerate(transcript) if line == old]
        if len(hits) != 1:
            raise SystemExit(f"error: {name}: the line to edit is {len(hits)} times in the transcript, "
                             f"not once: {old}")
        transcript[hits[0]] = new
    return transcript, comment or []


def row(comment, fields, transcript):
    for line in transcript:
        if ')"' in line:
            raise SystemExit(f"error: a transcript line ends the raw string: {line}")
    text = "\n".join(transcript)
    body = ",\n     ".join(f"{key} = {value}" for key, value in fields)
    return comment + [f'        {{{body},\n     .transcript = R"({text})"}},']


def actions_rows(java_path, transcript_path):
    with open(java_path, encoding="utf-8") as f:
        source = f.read()
    found = blocks(transcript_path)
    pattern = re.compile(r'scenario\("([^"]*)", new String\[\] \{([^}]*)\}, "(\w+)", "(\w+)", '
                         r'(null|new String\[\] \{([^}]*)\}), (null|"paste")\);')
    rows, labels, used = [], set(), set()
    for m in pattern.finditer(source):
        label, selection, action, _method, _paste, paste_on, then = m.groups()
        if label not in found:
            raise SystemExit(f"error: {transcript_path} has no block '{label}'")
        labels.add(label)
        name = ident(label)
        paste = "std::nullopt" if then == "null" else "Names" + quoted(names(paste_on or ""))
        transcript, comment = apply_edits(name, list(found[label]), ACTIONS_EDITS, used)
        rows += row(comment, [(".name", f'"{name}"'), (".selection", quoted(names(selection))),
                              (".action", "Action::" + ACTIONS[action]), (".pasteOn", paste)], transcript)
    check_complete("ActionsProbe", found, labels | UNTABLED, ACTIONS_EDITS, used)
    return rows


def clipboard_rows(java_path, transcript_path):
    with open(java_path, encoding="utf-8") as f:
        source = f.read()
    found = blocks(transcript_path)
    scenarios = []
    for line in source.splitlines():
        line = line.strip()
        m = re.match(r'copy\("([^"]*)", (.*)\);$', line)
        if m:
            scenarios.append((m.group(1), names(m.group(2)), "COPY", None))
            continue
        m = re.match(r'copyPaste\("([^"]*)", new String\[\] \{([^}]*)\}, "([^"]*)"\);$', line)
        if m:
            scenarios.append((m.group(1), names(m.group(2)), "COPY", m.group(3)))
            continue
        m = re.match(r'action\("([^"]*)", "(\w+)", "(\w+)", (.*)\);$', line)
        if m:
            scenarios.append((m.group(1), names(m.group(4)), ACTIONS[m.group(2)], None))
    rows, labels, used = [], set(), set()
    for label, selection, action, paste_on in scenarios:
        if label not in found:
            raise SystemExit(f"error: {transcript_path} has no block '{label}'")
        labels.add(label)
        name = ident(label)
        paste = "std::nullopt" if paste_on is None else f'"{paste_on}"'
        transcript, comment = apply_edits(name, list(found[label]), CLIPBOARD_EDITS, used)
        rows += row(comment, [(".name", f'"{name}"'), (".selection", quoted(selection)),
                              (".action", "Action::" + action), (".pasteOn", paste)], transcript)
    check_complete("ClipboardProbe", found, labels, CLIPBOARD_EDITS, used)
    return rows


def check_complete(probe, found, labels, edits, used):
    if not labels:
        raise SystemExit(f"error: no scenario of {probe} found in its source")
    unknown = sorted(set(found) - labels)
    if unknown:
        raise SystemExit(f"error: {probe} printed blocks that are no scenario of the table: {unknown}")
    missing = sorted(set(edits) - used)
    if missing:
        raise SystemExit(f"error: {probe}: edits for rows that are not in the table: {missing}")


def splice(lines, start_line, start_body, rows, path):
    """Replaces the lines between @p start_body (the first after @p start_line) and the next "    };"."""
    starts = [i for i, line in enumerate(lines) if line == start_line]
    if len(starts) != 1:
        raise SystemExit(f"error: {path} has {len(starts)} lines '{start_line}', not one")
    begin = next((i for i in range(starts[0], len(lines)) if lines[i] == start_body), None)
    end = None if begin is None else next((i for i in range(begin + 1, len(lines)) if lines[i] == "    };"),
                                          None)
    if begin is None or end is None:
        raise SystemExit(f"error: no table '{start_body}' ... '    }};' after '{start_line}' in {path}")
    lines[begin + 1:end] = "\n".join(rows).split("\n")


def main():
    actions_java, actions_out, clipboard_java, clipboard_out, test_path = sys.argv[1:6]
    with open(test_path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    splice(lines, "[[nodiscard]] std::vector<ActionsScenario> makeActionsScenarios()", "    return {",
           actions_rows(actions_java, actions_out), test_path)
    splice(lines, "[[nodiscard]] const std::vector<ClipboardScenario>& clipboardScenarios()",
           "    static const std::vector<ClipboardScenario> kScenarios{",
           clipboard_rows(clipboard_java, clipboard_out), test_path)
    with open(test_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
