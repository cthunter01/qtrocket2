#!/usr/bin/env python3
"""Writes tests/data/goldens/gui/simulation-dialog/simulation-dialog.json from SimulationDialogProbe's transcript
(see the probe's class comment for its lines and README.md, "simulation-dialog", for the table).

The table holds, for each design the probe was given (the 66 designs of OpenDesignsExitGolden, open-designs/
designs.py's list), what OpenRocket's simulation settings dialog shows of each of its simulations as the tab's Edit
opens it (its title, its walk, the tabs' states, the extensions' buttons, a multi-level wind's levels in the wind
editor), whether it was modified, the change events of the document's simulations while it was open, and the
document's undo state, saved state and statuses before the dialog and after its Cancel.

The walks share their lines: "lines" lists every distinct walk line once, in the order of its first appearance, and
a simulation's "walk" is the indices of its lines (the 196 walks have about 23,000 lines, of which a few hundred
differ). One design per line of its head, one simulation per line.

usage: gen_simulation_dialog_pins.py TRANSCRIPT OUTPUT COMMIT
"""
import json
import sys

SCHEMA_VERSION = 1

STATUSES = {"UPTODATE", "LOADED", "OUTDATED", "EXTERNAL", "NOT_SIMULATED", "CANT_RUN", "ABORTED"}


def fail(message):
    raise SystemExit("gen_simulation_dialog_pins.py: " + message)


def unescape(text):
    """The probe's esc() undone: \\\\, \\n and \\t; <null> is None."""
    if text == "<null>":
        return None
    out = []
    i = 0
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            n = text[i + 1]
            out.append({"\\": "\\", "n": "\n", "t": "\t"}.get(n))
            if out[-1] is None:
                fail("unknown escape \\%s in %r" % (n, text))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def boolean(text):
    if text not in ("true", "false"):
        fail("not a boolean: %r" % text)
    return text == "true"


def undo_state(fields):
    """An UNDO line's fields after "before"/"after": position descriptions next stored clean saved."""
    if len(fields) != 6:
        fail("an UNDO line of %d fields" % len(fields))
    position, descriptions, next_description, stored, clean, saved = fields
    return {
        "position": int(position),
        # The descriptions are joined by "|" with each escaped; a description holds no "|" in these designs.
        "descriptions": [unescape(d) for d in descriptions.split("|")],
        "next": unescape(next_description),
        "stored": unescape(stored),
        "clean": boolean(clean),
        "saved": boolean(saved),
    }


def statuses(text):
    names = text.split(" ") if text else []
    for name in names:
        if name not in STATUSES:
            fail("not a status: %r" % name)
    return names


class Reader:
    def __init__(self):
        self.colors = None
        self.designs = []
        self.design = None
        self.simulation = None
        self.lines = []
        self.line_index = {}

    def line_id(self, text):
        if text not in self.line_index:
            self.line_index[text] = len(self.lines)
            self.lines.append(text)
        return self.line_index[text]

    def finish_simulation(self):
        sim = self.simulation
        if sim is None:
            return
        for key in ("title", "modified", "changes", "undoBefore", "undoAfter", "statusesBefore", "statusesAfter"):
            if key not in sim:
                fail("simulation %s of %s has no %s" % (sim["index"], self.design["name"], key))
        if [t["index"] for t in sim["tabs"]] != list(range(5)):
            fail("simulation %s of %s: not five tabs" % (sim["index"], self.design["name"]))
        sim["tabs"] = [t["state"] for t in sim["tabs"]]
        self.design["simulations"].append(sim)
        self.simulation = None

    def read(self, path):
        with open(path, encoding="utf-8") as transcript:
            for number, raw in enumerate(transcript, 1):
                line = raw.rstrip("\n")
                if not line:
                    continue
                kind, *fields = line.split("\t")
                try:
                    self.dispatch(kind, fields)
                except SystemExit:
                    raise
                except Exception as error:  # noqa: BLE001 - reported with the line
                    fail("line %d (%r): %s" % (number, line[:80], error))
        if self.design is not None:
            fail("the transcript ends inside the design %s" % self.design["name"])
        if self.colors is None:
            fail("no COLORS line")

    def dispatch(self, kind, fields):
        if kind == "COLORS":
            text, warning, error = fields
            self.colors = {"text": text, "warning": warning, "error": error}
        elif kind == "D":
            name, design_kind, file, count = fields
            self.design = {"name": name, "kind": design_kind, "file": unescape(file), "count": int(count),
                           "simulations": []}
        elif kind == "END":
            self.finish_simulation()
            if len(self.design["simulations"]) != self.design["count"]:
                fail("%s: %d simulations, not %d" % (self.design["name"], len(self.design["simulations"]),
                                                     self.design["count"]))
            del self.design["count"]
            self.designs.append(self.design)
            self.design = None
        elif kind == "S":
            self.finish_simulation()
            index, name = fields
            if int(index) != len(self.design["simulations"]):
                fail("%s: simulation %s out of order" % (self.design["name"], index))
            self.simulation = {"index": int(index), "name": unescape(name), "walk": [], "tabs": [],
                               "extensions": [], "wind": []}
        elif kind == "UNDO":
            key = {"before": "undoBefore", "after": "undoAfter"}[fields[0]]
            self.simulation[key] = undo_state(fields[1:])
        elif kind == "STATUS":
            key = {"before": "statusesBefore", "after": "statusesAfter"}[fields[0]]
            self.simulation[key] = statuses(fields[1] if len(fields) > 1 else "")
        elif kind == "TITLE":
            (title,) = fields
            self.simulation["title"] = unescape(title)
        elif kind == "W":
            (text,) = fields
            self.simulation["walk"].append(self.line_id(unescape(text)))
        elif kind == "TAB":
            index, title, enabled, tip = fields
            state = unescape(title) + (" enabled" if boolean(enabled) else " disabled")
            if unescape(tip) is not None:
                state += " tip=" + unescape(tip)
            self.simulation["tabs"].append({"index": int(index), "state": state})
        elif kind == "EXT":
            (buttons,) = fields
            self.simulation["extensions"].append(buttons)
        elif kind == "WIND":
            (text,) = fields
            self.simulation["wind"].append(unescape(text))
        elif kind == "MODIFIED":
            modified, title = fields
            self.simulation["modified"] = boolean(modified)
            self.simulation["modifiedTitle"] = unescape(title)
        elif kind == "CHANGES":
            (counts,) = fields
            self.simulation["changes"] = [int(c) for c in counts.split(" ")] if counts else []
        else:
            fail("unknown line kind %r" % kind)


def to_json(value):
    return json.dumps(value, ensure_ascii=True, separators=(", ", ": "), allow_nan=False)


SIMULATION_KEYS = ["index", "name", "title", "modified", "modifiedTitle", "tabs", "extensions", "wind", "changes",
                   "undoBefore", "undoAfter", "statusesBefore", "statusesAfter", "walk"]


def write_document(path, head, reader):
    lines = ["{"]
    for key, value in head.items():
        lines.append(" {}: {},".format(json.dumps(key), to_json(value)))
    lines.append(' "lines": [')
    for i, text in enumerate(reader.lines):
        lines.append("  " + to_json(text) + ("," if i + 1 < len(reader.lines) else ""))
    lines.append(" ],")
    lines.append(' "designs": [')
    for i, design in enumerate(reader.designs):
        lines.append("  {")
        for key in ("name", "kind", "file"):
            lines.append("   {}: {},".format(json.dumps(key), to_json(design[key])))
        lines.append('   "simulations": [')
        sims = design["simulations"]
        for j, sim in enumerate(sims):
            ordered = {key: sim[key] for key in SIMULATION_KEYS}
            lines.append("    " + to_json(ordered) + ("," if j + 1 < len(sims) else ""))
        lines.append("   ]")
        lines.append("  }" + ("," if i + 1 < len(reader.designs) else ""))
    lines.append(" ]")
    lines.append("}")
    text = "\n".join(lines) + "\n"
    json.loads(text)  # well-formed
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(text)


def main():
    if len(sys.argv) != 4:
        fail("usage: gen_simulation_dialog_pins.py TRANSCRIPT OUTPUT COMMIT")
    transcript, output, commit = sys.argv[1:]
    reader = Reader()
    reader.read(transcript)
    head = {
        "schema": "simulation-dialog",
        "schemaVersion": SCHEMA_VERSION,
        "generator": "tools/openrocket-gui-pins/generate.sh simulation-dialog (SimulationDialogProbe)",
        "openrocket": {"commit": commit},
        "colors": reader.colors,
    }
    write_document(output, head, reader)


if __name__ == "__main__":
    main()
