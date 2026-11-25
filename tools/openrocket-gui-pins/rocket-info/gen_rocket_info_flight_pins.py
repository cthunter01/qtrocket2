#!/usr/bin/env python3
"""Writes tests/data/goldens/gui/rocket-info/flight-blocks.json from RocketInfoFlightProbe's output.

usage: gen_rocket_info_flight_pins.py PROBE_OUTPUT JSON COMMIT

PROBE_OUTPUT is the probe's standard output (generate.sh sends OpenRocket's log to standard error). Every double
of the probe is Double.toHexString (exact); NaN and the infinities go into the JSON as the strings "NaN",
"Infinity" and "-Infinity" (tests/core/goldens/GoldenData.h, goldenNumber()). The layout of a text of the flight
block is reduced to what does not depend on the font: its row above the bottom one ((y2 - y) / line, a whole
number: RocketInfo draws the block at y2 - n * line) and its column, the left margin (x = x1) or the values'
(x = x1 + the widest label's visual width + 10, RocketInfo.drawFlightData()); the generator checks that every
text is on such a row and in such a column. The design files are written relative to the repository.

A design whose estimate runs (the probe's R lines) gets a "run": the document's saved state and undo history before
and after the estimate's runs, each simulation run with its status after its run, whether the chain ended without a
failure, and the overlay after the runs with its texts, of which the values of the fresh flight (the apogee, the
maximum velocity and the maximum acceleration, in the values' column of the block's three bottom rows) are written
as null: OpenRocket's flights at the default time step do not repeat from one set of component ids to another
(tools/openrocket-goldens/README.md), and no test may pin a value of a fresh flight. Every other design's "run" is
null.
"""
import json
import math
import pathlib
import sys

SCHEMA_VERSION = 2
# The labels of the block (RocketInfo.FlightConfig, .Apogee, .Maxvelocity, .Maxacceleration, each with " ").
LABELS = ("Flight configuration: ", "Apogee: ", "Max. velocity: ", "Max. acceleration: ")
FONTS = {"regular": "REGULAR", "bold": "BOLD"}
COLOUR_KEYS = ("FLIGHTDATA_TEXT_ACTIVE", "FLIGHTDATA_TEXT_INACTIVE", "ERROR", "TEXT")
# The rows of the block whose values are those of the flight (Apogee, Max. velocity, Max. acceleration).
FLIGHT_VALUE_ROWS = (0, 1, 2)


def number(text):
    """A double of the transcript (Double.toHexString, or NaN and the infinities)."""
    if text in ("NaN", "Infinity", "-Infinity"):
        return float(text.replace("Infinity", "inf"))
    return float.fromhex(text)


def as_json_number(value):
    if math.isnan(value):
        return "NaN"
    if math.isinf(value):
        return "Infinity" if value > 0 else "-Infinity"
    return value


def repository_path(file):
    """@p file relative to the repository (data/examples/..., tests/data/goldens/...), with '/'."""
    repo = pathlib.Path(__file__).resolve().parents[3]
    return pathlib.Path(file).resolve().relative_to(repo).as_posix()


def rgba(text):
    return [int(c) for c in text.split(",")]


def document_state(fields):
    """isSaved(), isUndoAvailable() and isRedoAvailable() of an R line."""
    saved, undo, redo = fields
    return {"saved": saved == "true", "undoAvailable": undo == "true", "redoAvailable": redo == "true"}


def parse_run(current, rest):
    """An R line of @p current's estimate's runs."""
    what, _, fields = rest.partition(" ")
    if what == "before":
        current["run"] = {"before": document_state(fields.split(" ")), "ran": [], "succeeded": True, "status": None,
                          "texts": []}
    elif what == "ran":
        index, status, has_data = fields.split(" ")
        current["run"]["ran"].append({"index": int(index), "status": status, "hasData": has_data == "true"})
    elif what == "failed":
        index, _, failure = fields.partition(" | ")
        current["run"]["succeeded"] = False
        current["run"]["failed"] = {"index": int(index), "throwable": failure}
    elif what == "after":
        current["run"]["after"] = document_state(fields.split(" "))
    else:
        raise SystemExit("unexpected R line: " + rest)


def parse(lines):
    colours = None
    scenarios = []
    current = None
    # Where the O, A, F and T lines go: the scenario, or its run once its R lines began.
    target = None
    for line in lines:
        line = line.rstrip("\n")
        if not line:
            continue
        tag, _, rest = line.partition(" ")
        if tag == "COLORS":
            colours = dict(zip(COLOUR_KEYS, rest.split(" ")))
        elif tag == "S":
            head, _, file = rest.partition(" | ")
            name, kind = head.split(" ")
            current = {"name": name, "kind": kind, "file": repository_path(file), "simulations": [], "texts": [],
                       "status": None, "run": None}
            scenarios.append(current)
            target = current
        elif tag == "P":
            estimated, units, select = rest.split(" ")
            current["setup"] = {"estimated": {"estimated": True, "set": False}[estimated],
                                "units": units, "selectConfigurationOf": None if select == "-" else int(select)}
        elif tag == "C":
            motors, _, name = rest.partition(" | ")
            current["configuration"] = {"hasMotors": motors == "true", "name": name}
        elif tag == "M":
            head, _, name = rest.partition(" | ")
            fields = head.split(" ")
            simulation = {
                "name": name,
                "status": fields[1],
                "hasData": fields[2] == "true",
                "selectedConfiguration": fields[3] == "true",
                "configurationHasMotors": fields[4] == "true",
                "maxima": None,
            }
            if int(fields[0]) != len(current["simulations"]):
                raise SystemExit("simulations out of order in " + current["name"])
            if simulation["hasData"]:
                simulation["maxima"] = {key: as_json_number(number(value)) for key, value in
                                        zip(("altitude", "velocity", "machNumber", "acceleration"), fields[5:9])}
            current["simulations"].append(simulation)
        elif tag == "U":
            which, _, indices = rest.partition(" ")
            current.setdefault("runs", {})[which] = [int(i) for i in indices.split()]
        elif tag == "R":
            parse_run(current, rest)
            target = current["run"]
        elif tag == "O":
            shown, calculating, index = rest.split(" ")
            target["overlay"] = {"shown": shown, "calculating": calculating == "true",
                                 "simulation": None if index == "-" else int(index)}
        elif tag == "A":
            status, _, text = rest.partition(" | ")
            target["status"] = {"status": status, "text": text}
        elif tag == "F":
            target["frame"] = [number(v) for v in rest.split(" ")]
        elif tag == "T":
            head, _, text = rest.partition(" | ")
            font, colour, x, y, width = head.split(" ")
            target["texts"].append({"font": font, "colour": colour, "x": number(x), "y": number(y),
                                    "width": number(width), "text": text})
        elif tag == "E":
            current = None
            target = None
        else:
            raise SystemExit("unexpected line: " + line)
    if colours is None:
        raise SystemExit("no COLORS line")
    return colours, scenarios


def colour_key(colours, text):
    for key in COLOUR_KEYS:
        if colours[key] == text:
            return key
    raise SystemExit("a colour of no key: " + text)


def layout(scenario, colours, name, fresh=False):
    """The texts of the block of @p scenario (a scenario, or its run) with their rows above the bottom one and their
    columns; with @p fresh, the flight's values are null (see the top of the file)."""
    line, x1, y2 = scenario["frame"]
    labels = [t for t in scenario["texts"] if t["text"] in LABELS and t["font"] == "regular"]
    value_x = None
    if labels:
        if len(labels) != len(LABELS):
            raise SystemExit("not every label of the block in " + name)
        value_x = x1 + (max(t["width"] for t in labels) + 10)
    texts = []
    for t in scenario["texts"]:
        row = (y2 - t["y"]) / line
        if abs(row - round(row)) > 1e-6:
            raise SystemExit("a text between rows in %s: %r" % (name, t))
        if abs(t["x"] - x1) < 1e-3:
            column = "LEFT"
        elif value_x is not None and abs(t["x"] - value_x) < 1e-3:
            column = "VALUE"
        else:
            raise SystemExit("a text in no column in %s: %r" % (name, t))
        text = t["text"]
        if fresh and column == "VALUE" and int(round(row)) in FLIGHT_VALUE_ROWS:
            text = None
        texts.append({"text": text, "font": FONTS[t["font"]], "colour": colour_key(colours, t["colour"]),
                      "rgba": rgba(t["colour"]), "rowFromBottom": int(round(row)), "column": column})
    if fresh and sum(1 for t in texts if t["text"] is None) != len(FLIGHT_VALUE_ROWS):
        raise SystemExit("not the three values of the flight in the run of " + name)
    return texts


def run_of(scenario, colours):
    """The "run" of @p scenario, or None."""
    run = scenario["run"]
    if run is None:
        return None
    if "after" not in run or "overlay" not in run:
        raise SystemExit("an unfinished run in " + scenario["name"])
    result = {"before": run["before"], "after": run["after"], "ran": run["ran"], "succeeded": run["succeeded"]}
    if "failed" in run:
        result["failed"] = run["failed"]
    result["overlay"] = run["overlay"]
    result["status"] = run["status"]
    result["texts"] = layout(run, colours, scenario["name"] + " (run)", fresh=True)
    return result


def main():
    probe, path, commit = sys.argv[1:4]
    with open(probe, encoding="utf-8") as f:
        colours, scenarios = parse(f.readlines())
    document = {
        "schema": SCHEMA_VERSION,
        "openrocket": commit,
        "generator": "tools/openrocket-gui-pins/generate.sh rocket-info (RocketInfoFlightProbe)",
        "colours": {key: rgba(value) for key, value in colours.items()},
        "scenarios": [],
    }
    for s in scenarios:
        document["scenarios"].append({
            "name": s["name"],
            "kind": s["kind"],
            "file": s["file"],
            "setup": s["setup"],
            "configuration": s["configuration"],
            "simulations": s["simulations"],
            "runs": s["runs"],
            "overlay": s["overlay"],
            "status": s["status"],
            "texts": layout(s, colours, s["name"]),
            "run": run_of(s, colours),
        })
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(write_document(document))


def compact(value):
    return json.dumps(value, ensure_ascii=True, separators=(", ", ": "), allow_nan=False)


def write_list(lines, indent, key, entries, comma):
    """The member @p key of a list of objects, each on a line of its own."""
    if not entries:
        lines.append('{}"{}": []{}'.format(indent, key, comma))
        return
    lines.append('{}"{}": ['.format(indent, key))
    for j, entry in enumerate(entries):
        lines.append(indent + " " + compact(entry) + ("," if j + 1 < len(entries) else ""))
    lines.append(indent + "]" + comma)


def write_document(document):
    """One scenario per block, each simulation and each text on a line of its own; a run after them, its head on a
    line and each of its texts on one."""
    lines = ["{"]
    for key, value in document.items():
        if key != "scenarios":
            lines.append(" {}: {},".format(json.dumps(key), compact(value)))
    lines.append(' "scenarios": [')
    scenarios = document["scenarios"]
    for i, scenario in enumerate(scenarios):
        head = {k: v for k, v in scenario.items() if k not in ("simulations", "texts", "run")}
        lines.append("  " + compact(head)[:-1] + ",")
        write_list(lines, "   ", "simulations", scenario["simulations"], ",")
        write_list(lines, "   ", "texts", scenario["texts"], ",")
        run = scenario["run"]
        if run is None:
            lines.append('   "run": null')
        else:
            run_head = {k: v for k, v in run.items() if k != "texts"}
            lines.append('   "run": ' + compact(run_head)[:-1] + ",")
            write_list(lines, "    ", "texts", run["texts"], "")
            lines.append("   }")
        lines.append("  }" + ("," if i + 1 < len(scenarios) else ""))
    lines.append(" ]")
    lines.append("}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    main()
