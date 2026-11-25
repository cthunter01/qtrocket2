#!/usr/bin/env python3
"""Writes tests/data/goldens/gui/simulation-table/simulation-table.json from SimulationTableProbe's transcript (see
the probe's class comment for its lines and README.md, "simulation-table", for the table).

The table holds what OpenRocket's simulations table shows of each design the probe was given (the 66 designs of
OpenDesignsExitGolden, open-designs/designs.py's list, and a new design), of two status scenarios and of a Copy whose rows include one without data, in
QtRocket's terms where QtRocket fixes one of OpenRocket's bugs: the warnings column's orders are QtRocket's (bug
23: OpenRocket's own, which keep the model's order, are kept beside them), and so is the Copy text of the pairing
case (bug 25: OpenRocket's own is kept beside it). A simulation a scenario ran has no cells, warnings or counts in
the table: QtRocket's tests pin no value of a fresh flight.

One design, scenario or simulation per line (each column and each order too), so that a change shows as the lines
it changes.

usage: gen_simulation_table_pins.py TRANSCRIPT OUTPUT COMMIT
"""
import json
import sys

SCHEMA_VERSION = 1

# The column ids of the model (SimulationPanel.SIMULATION_TABLE_COLUMN_IDS), in its order.
COLUMN_IDS = ["status", "warnings", "name", "configuration", "simulationStepper", "launchRodVelocity", "apogee",
              "deploymentVelocity", "optimumCoastTime", "maxVelocity", "maxAcceleration", "timeToApogee",
              "flightTime", "groundHitVelocity"]

ACTIONS = ["newSimulation", "editSimulation", "cutSimulation", "copySimulation", "pasteSimulation", "runSimulation",
           "landingDispersion", "plotSimulation", "duplicateSimulation", "deleteSimulation", "simTableExport",
           "selectedSimsExport"]


def fail(message):
    raise SystemExit("gen_simulation_table_pins.py: " + message)


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


def order(text):
    return [int(r) for r in text.split(",")] if text else []


def boolean(text):
    if text not in ("true", "false"):
        fail("not a boolean: %r" % text)
    return text == "true"


class Block:
    """A design, a scenario or the pairing case being read."""

    def __init__(self, head):
        self.head = head
        self.simulations = {}
        self.fresh = set()
        self.java_orders = {}
        self.qt_orders = {}

    def orders(self):
        """The orders by column: QtRocket's (SQ) where the probe gives them, else OpenRocket's (S), and
        OpenRocket's own beside a column QtRocket sorts otherwise, as "openRocket:<column>"."""
        out = {}
        for column in COLUMN_IDS:
            if column in self.qt_orders:
                out[column] = self.qt_orders[column]
            elif column in self.java_orders:
                out[column] = self.java_orders[column]
        for column in COLUMN_IDS:
            if column in self.qt_orders:
                if column not in self.java_orders:
                    fail("no S for column %s of %s" % (column, self.head))
                out["openRocket:" + column] = self.java_orders[column]
        return out

    def simulation(self, row):
        return self.simulations.setdefault(int(row), {"row": int(row)})


def read_row_line(block, fields):
    kind = fields[0]
    if kind == "R":
        row, status, icon, sort_text, description, tool_tip = fields[1:]
        sim = block.simulation(row)
        sim.update({"status": status, "statusIcon": unescape(icon), "statusSortText": unescape(sort_text),
                    "description": unescape(description), "statusToolTip": unescape(tool_tip)})
    elif kind == "W":
        row, critical, normal, informational, box, tool_tip = fields[1:]
        counts = [int(critical), int(normal), int(informational)]
        block.simulation(row)["warnings"] = {
            "counts": None if counts == [-1, -1, -1] else counts,
            "box": unescape(box),
            "toolTip": unescape(tool_tip),
            "messages": [],
        }
    elif kind == "WS":
        row, priority, text = fields[1:]
        block.simulation(row)["warnings"]["messages"].append([priority, unescape(text)])
    elif kind == "C":
        row = fields[1]
        cells = [unescape(f) for f in fields[2:]]
        if len(cells) != len(COLUMN_IDS) - 2:
            fail("row %s has %d cells" % (row, len(cells)))
        block.simulation(row)["cells"] = cells
    elif kind == "FRESH":
        block.fresh.add(int(fields[1]))
        block.simulation(fields[1])["fresh"] = True
    else:
        return False
    return True


def simulations_of(block):
    rows = sorted(block.simulations)
    if rows != list(range(len(rows))):
        fail("the rows of %s are %s" % (block.head, rows))
    out = []
    for row in rows:
        sim = block.simulations[row]
        if row in block.fresh:
            if "cells" in sim or "warnings" in sim:
                fail("the fresh row %d of %s has values" % (row, block.head))
        elif "cells" not in sim or "warnings" not in sim:
            fail("row %d of %s is incomplete" % (row, block.head))
        out.append(sim)
    return out


def read_transcript(path):
    colors = None
    columns = []
    designs = []
    scenarios = []
    pairings = []
    block = None
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line:
                continue
            fields = line.split("\t")
            kind = fields[0]
            if kind == "COLORS":
                colors = dict(zip(["text", "textDim", "warning", "error", "info"], fields[1:]))
            elif kind == "H":
                column, column_id, name, tool_tip, unit = fields[1:]
                if COLUMN_IDS[int(column)] != column_id or int(column) != len(columns):
                    fail("unexpected column %s %s" % (column, column_id))
                columns.append({"id": column_id, "header": unescape(name), "toolTip": unescape(tool_tip),
                                "unit": unescape(unit)})
            elif kind == "D":
                name, design_kind, file = fields[1:]
                block = Block(name)
                block.design = {"name": name, "kind": design_kind, "file": unescape(file), "widths": [],
                                "actions": {}}
            elif kind == "V":
                valid, next_name, count = fields[1:]
                block.design.update({"hasValidConfig": boolean(valid), "nextSimulationName": unescape(next_name),
                                     "simulationCount": int(count)})
            elif kind == "HV":
                column, column_id, preferred, minimum, maximum = fields[1:]
                block.design["widths"].append([column_id, int(preferred), int(minimum), int(maximum)])
            elif kind in ("S", "SQ"):
                column, direction, rows = fields[1:]
                target = block.java_orders if kind == "S" else block.qt_orders
                target.setdefault(COLUMN_IDS[int(column)], {})[direction] = order(rows)
            elif kind == "A":
                what = fields[1]
                states = {}
                for field in fields[2:]:
                    name, value = field.split("=")
                    states[name] = value == "1"
                if list(states) != ACTIONS:
                    fail("unexpected actions %s" % list(states))
                block.design["actions"][what] = states
            elif kind in ("CSV", "CSVE"):
                block.design["csv" if kind == "CSV" else "csvExponential"] = unescape(fields[1])
            elif kind == "COPY":
                if block.design is not None:
                    block.design["copy"] = unescape(fields[1])
                else:
                    block.pairing["openRocketCopy"] = unescape(fields[1])
            elif kind == "COPYQ":
                block.pairing["copy"] = unescape(fields[1])
            elif kind == "SCENARIO":
                name, file = fields[1:]
                block = Block(name)
                block.design = None
                block.scenario = {"name": name, "file": unescape(file), "initial": [], "steps": []}
            elif kind == "PAIRING":
                name, file = fields[1:]
                block = Block(name)
                block.design = None
                block.scenario = None
                block.pairing = {"name": name, "file": unescape(file), "steps": []}
            elif kind == "SIM":
                index, name, id_prefix, configuration = fields[1:]
                block.scenario["initial"].append({"name": unescape(name), "configurationId": id_prefix,
                                                  "configuration": unescape(configuration)})
            elif kind == "ST":
                what, statuses, undo, redo, saved = fields[1:]
                step = {"step": unescape(what), "statuses": statuses.split(" "), "undo": unescape(undo),
                        "redo": unescape(redo), "saved": boolean(saved)}
                (block.scenario if block.scenario is not None else block.pairing)["steps"].append(step)
            elif kind == "SIMULATE-EXCEPTION":
                fail("a scenario's run threw: " + fields[1])
            elif read_row_line(block, fields):
                pass
            elif kind == "END":
                if block.design is not None:
                    design = block.design
                    design["orders"] = block.orders()
                    design["simulations"] = simulations_of(block)
                    if len(design["simulations"]) != design["simulationCount"]:
                        fail("%s has %d rows" % (block.head, len(design["simulations"])))
                    designs.append(design)
                elif block.scenario is not None:
                    scenario = block.scenario
                    scenario["simulations"] = simulations_of(block)
                    scenario["orders"] = block.orders()
                    scenarios.append(scenario)
                else:
                    pairings.append(block.pairing)
                block = None
            else:
                fail("unknown line: " + line)
    if block is not None:
        fail("the transcript ends inside " + block.head)
    if colors is None or len(columns) != len(COLUMN_IDS):
        fail("no colours or columns")
    return colors, columns, designs, scenarios, pairings


def to_json(value):
    return json.dumps(value, ensure_ascii=True, separators=(", ", ": "), allow_nan=False)


def write_document(path, head, columns, designs, scenarios, pairings):
    lines = ["{"]
    for key, value in head.items():
        lines.append(" {}: {},".format(json.dumps(key), to_json(value)))
    lines.append(' "columns": [')
    for i, column in enumerate(columns):
        lines.append("  " + to_json(column) + ("," if i + 1 < len(columns) else ""))
    lines.append(" ],")

    def blocks(name, items, last):
        lines.append(' "{}": ['.format(name))
        for i, item in enumerate(items):
            body = {k: v for k, v in item.items() if k not in ("simulations", "orders", "steps")}
            lines.append("  {" + to_json(body)[1:-1] + ",")
            for list_name in ("steps", "simulations"):
                if list_name in item:
                    lines.append('   "{}": ['.format(list_name))
                    for j, entry in enumerate(item[list_name]):
                        lines.append("    " + to_json(entry) + ("," if j + 1 < len(item[list_name]) else ""))
                    lines.append("   ]" + ("," if "orders" in item or (list_name == "steps" and
                                                                          "simulations" in item) else ""))
            if "orders" in item:
                lines.append('   "orders": {')
                keys = list(item["orders"])
                for j, key in enumerate(keys):
                    lines.append("    {}: {}".format(json.dumps(key), to_json(item["orders"][key])) +
                                 ("," if j + 1 < len(keys) else ""))
                lines.append("   }")
            lines.append("  }" + ("," if i + 1 < len(items) else ""))
        lines.append(" ]" + ("" if last else ","))

    blocks("designs", designs, False)
    blocks("scenarios", scenarios, False)
    blocks("pairings", pairings, True)
    lines.append("}")
    text = "\n".join(lines) + "\n"
    json.loads(text)  # well-formed
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(text)


def main():
    if len(sys.argv) != 4:
        fail("usage: gen_simulation_table_pins.py TRANSCRIPT OUTPUT COMMIT")
    transcript, output, commit = sys.argv[1:]
    colors, columns, designs, scenarios, pairings = read_transcript(transcript)
    head = {
        "schema": "simulation-table",
        "schemaVersion": SCHEMA_VERSION,
        "generator": "tools/openrocket-gui-pins/generate.sh simulation-table (SimulationTableProbe)",
        "openrocket": {"commit": commit},
        "colors": colors,
    }
    write_document(output, head, columns, designs, scenarios, pairings)


if __name__ == "__main__":
    main()
