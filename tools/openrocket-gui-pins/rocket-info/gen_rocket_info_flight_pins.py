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
"""
import json
import math
import pathlib
import sys

SCHEMA_VERSION = 1
# The labels of the block (RocketInfo.FlightConfig, .Apogee, .Maxvelocity, .Maxacceleration, each with " ").
LABELS = ("Flight configuration: ", "Apogee: ", "Max. velocity: ", "Max. acceleration: ")
FONTS = {"regular": "REGULAR", "bold": "BOLD"}
COLOUR_KEYS = ("FLIGHTDATA_TEXT_ACTIVE", "FLIGHTDATA_TEXT_INACTIVE", "ERROR", "TEXT")


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


def parse(lines):
    colours = None
    scenarios = []
    current = None
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
                       "status": None}
            scenarios.append(current)
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
        elif tag == "O":
            shown, calculating, index = rest.split(" ")
            current["overlay"] = {"shown": shown, "calculating": calculating == "true",
                                  "simulation": None if index == "-" else int(index)}
        elif tag == "A":
            status, _, text = rest.partition(" | ")
            current["status"] = {"status": status, "text": text}
        elif tag == "F":
            current["frame"] = [number(v) for v in rest.split(" ")]
        elif tag == "T":
            head, _, text = rest.partition(" | ")
            font, colour, x, y, width = head.split(" ")
            current["texts"].append({"font": font, "colour": colour, "x": number(x), "y": number(y),
                                     "width": number(width), "text": text})
        elif tag == "E":
            current = None
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


def layout(scenario, colours):
    """The texts of the block with their rows above the bottom one and their columns."""
    line, x1, y2 = scenario["frame"]
    labels = [t for t in scenario["texts"] if t["text"] in LABELS and t["font"] == "regular"]
    value_x = None
    if labels:
        if len(labels) != len(LABELS):
            raise SystemExit("not every label of the block in " + scenario["name"])
        value_x = x1 + (max(t["width"] for t in labels) + 10)
    texts = []
    for t in scenario["texts"]:
        row = (y2 - t["y"]) / line
        if abs(row - round(row)) > 1e-6:
            raise SystemExit("a text between rows in %s: %r" % (scenario["name"], t))
        if abs(t["x"] - x1) < 1e-3:
            column = "LEFT"
        elif value_x is not None and abs(t["x"] - value_x) < 1e-3:
            column = "VALUE"
        else:
            raise SystemExit("a text in no column in %s: %r" % (scenario["name"], t))
        texts.append({"text": t["text"], "font": FONTS[t["font"]], "colour": colour_key(colours, t["colour"]),
                      "rgba": rgba(t["colour"]), "rowFromBottom": int(round(row)), "column": column})
    return texts


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
            "texts": layout(s, colours),
        })
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(write_document(document))


def compact(value):
    return json.dumps(value, ensure_ascii=True, separators=(", ", ": "), allow_nan=False)


def write_document(document):
    """One scenario per block, each simulation and each text on a line of its own."""
    lines = ["{"]
    for key, value in document.items():
        if key != "scenarios":
            lines.append(" {}: {},".format(json.dumps(key), compact(value)))
    lines.append(' "scenarios": [')
    scenarios = document["scenarios"]
    for i, scenario in enumerate(scenarios):
        head = {k: v for k, v in scenario.items() if k not in ("simulations", "texts")}
        lines.append("  " + compact(head)[:-1] + ",")
        for key in ("simulations", "texts"):
            entries = scenario[key]
            if not entries:
                lines.append('   "{}": []{}'.format(key, "," if key == "simulations" else ""))
                continue
            lines.append('   "{}": ['.format(key))
            for j, entry in enumerate(entries):
                lines.append("    " + compact(entry) + ("," if j + 1 < len(entries) else ""))
            lines.append("   ]" + ("," if key == "simulations" else ""))
        lines.append("  }" + ("," if i + 1 < len(scenarios) else ""))
    lines.append(" ]")
    lines.append("}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    main()
