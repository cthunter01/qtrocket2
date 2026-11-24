#!/usr/bin/env python3
"""Turns the transcript of SimulationPlotsProbe into the core's plot goldens (see README.md):

    gen_simulation_plots.py <transcript> <output directory> <commit> <dirty>

writes simulation-plots.json (every plot of the examples' stored simulations, compact) and
simulation-plot-details.json (the plots of the simulations of DETAILS in detail) into the output
directory. Every double of the transcript is Java's Double.toHexString; the JSON holds it as the
shortest decimal that reads back as the same double (Python's repr), NaN and the infinities as the
strings "NaN", "Infinity" and "-Infinity" (as the goldens of tools/openrocket-goldens). The script
refuses a transcript that breaks what the tests rely on (see check_*()).
"""

import hashlib
import json
import math
import struct
import sys

SCHEMA_VERSION = 1

# The simulations whose plots are recorded in detail, (file, index in the document's simulations),
# chosen for what they cover (README.md, "The tables").
DETAILS = [
    ("A simple model rocket.ork", 0),  # one stage, the plot model's hand pins (P2) beside it
    ("3D printable nose cone and fins.ork", 2),  # warnings: SIM_WARN groups and icons
    ("ARC payload rocket.ork", 0),  # two branches of a payload
    ("Deployable payload.ork", 2),  # two branches, the too short delay
    ("Dual parachute deployment.ork", 0),  # bug 45's plot (time step preset)
    ("Parallel booster staging.ork", 0),  # a parallel booster's branch
    ("Pods--powered with recovery deployment.ork", 0),  # powered pods
    ("Simulation extensions.ork", 2),  # roll control and air-start: roll data, an unknown type
    ("Three stage low power rocket.ork", 0),  # three branches, two of the same name
    ("Two stage high power rocket.ork", 0),  # two branches
]


def fail(message):
    sys.stderr.write("gen_simulation_plots.py: error: " + message + "\n")
    sys.exit(1)


def unescape(field):
    if field == "\\0":
        return None
    out = []
    i = 0
    while i < len(field):
        c = field[i]
        if c == "\\" and i + 1 < len(field):
            n = field[i + 1]
            out.append({"\\": "\\", "t": "\t", "n": "\n"}.get(n, "\\" + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


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


def bits(value):
    """Java's Double.doubleToLongBits(): the bits, every NaN as 0x7ff8000000000000."""
    if math.isnan(value):
        return 0x7FF8000000000000
    return struct.unpack("<Q", struct.pack("<d", value))[0]


def short_digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def parse_axes(text):
    return [int(a) for a in text.split(",")] if text else []


def ranges(fields):
    return [[number(fields[0]), number(fields[1])], [number(fields[2]), number(fields[3])]]


def parse_event(text):
    """"TYPE@<hex time>/b<branch>" as [type, time, branch]."""
    kind, rest = text.split("@", 1)
    time, branch = rest.rsplit("/b", 1)
    return [kind, number(time), int(branch)]


def point(text):
    if text == "-":
        return None
    x, y = text.split(",")
    return [number(x), number(y)]


def read_transcript(path):
    presets = []
    simulations = []
    simulation = None
    plot = None
    meta = None
    ended = False
    with open(path, encoding="utf-8") as transcript:
        for raw in transcript:
            fields = [unescape(f) for f in raw.rstrip("\n").split("\t")]
            tag, args = fields[0], fields[1:]
            if tag == "META":
                meta = {"version": args[0], "javaVersion": args[1]}
            elif tag == "PRESET":
                presets.append({"name": args[1], "domain": [args[2], args[3]], "rows": [],
                                "events": args[4].split(",") if args[4] else []})
            elif tag == "PRESET-ROW":
                presets[-1]["rows"].append([args[1], args[2], int(args[3])])
            elif tag == "SIM":
                simulation = {"file": args[0], "simulation": int(args[1]), "name": args[2],
                              "branches": [], "seriesLabels": [], "plots": []}
                simulations.append(simulation)
            elif tag == "BRANCH":
                simulation["branches"].append({"name": args[1], "points": int(args[2]),
                                               "separationTime": number(args[3]),
                                               "separationIndex": int(args[4])})
            elif tag == "SERIES-LABELS":
                simulation["seriesLabels"] = args
            elif tag == "PLOT":
                plot = {"preset": int(args[0]), "presetName": args[1], "name": args[2],
                        "domain": [args[3], args[4]], "rows": [], "candidates": [],
                        "series": [], "toolTips": [], "groups": {}, "icons": []}
                simulation["plots"].append(plot)
            elif tag == "ROW":
                plot["rows"].append({"type": args[1], "unit": args[2], "axis": int(args[3]),
                                     "color": args[4], "lineStyle": args[5]})
            elif tag == "EVENTS":
                plot["events"] = args[0].split(",") if args[0] else []
            elif tag == "AUTO":
                plot["auto"] = {"axes": parse_axes(args[0]), "ranges": ranges(args[1:5])}
            elif tag == "GOODNESS":
                plot["candidates"].append({"axes": parse_axes(args[0]), "goodness": number(args[1]),
                                           "ranges": ranges(args[2:6])})
            elif tag == "PANEL-COLORS":
                plot["panelColors"] = [int(c) if c.isdigit() else c for c in args]
            elif tag == "FILLED":
                plot["filled"] = {"axes": parse_axes(args[0]), "ranges": ranges(args[1:5])}
            elif tag == "JAVA-AUTO":
                plot.setdefault("openRocket", {})["auto"] = {"axes": parse_axes(args[0]),
                                                             "ranges": ranges(args[1:5])}
            elif tag == "JAVA-GOODNESS":
                plot.setdefault("openRocket", {}).setdefault("candidates", []).append({
                    "axes": parse_axes(args[0]), "goodness": number(args[1]), "ranges": ranges(args[2:6])})
            elif tag == "JAVA-FILLED":
                plot.setdefault("openRocket", {})["filled"] = {"axes": parse_axes(args[0]),
                                                               "ranges": ranges(args[1:5])}
            elif tag == "TITLE":
                plot["title"], plot["subtitle"] = args[0], args[1]
            elif tag == "DOMAIN":
                plot["domainLabel"] = args[0]
                plot["domainRange"] = [number(args[1]), number(args[2])]
            elif tag == "DOMAIN-WITH-EVENT-LINES":
                plot["domainRangeWithEventLines"] = [number(args[0]), number(args[1])]
            elif tag == "Y-AXIS":
                plot.setdefault("yAxes", []).append({
                    "series": int(args[1]), "label": None if args[2] == "-" else args[2],
                    "range": [number(args[3]), number(args[4])]})
            elif tag == "SERIES":
                plot["series"].append({
                    "index": int(args[0]), "axis": int(args[1]), "name": args[2],
                    "branch": int(args[3]), "row": int(args[4]), "unit": args[5],
                    "points": int(args[6]), "digest": args[7],
                    "min": [number(args[8]), number(args[10])], "max": [number(args[9]), number(args[11])],
                    "first": point(args[12]), "middle": point(args[13]), "last": point(args[14])})
            elif tag == "TOOLTIP":
                plot["toolTips"].append({"series": int(args[0]), "point": int(args[1]), "html": args[2],
                                         "name": args[3], "risky": args[4] == "risky"})
            elif tag == "LEGEND":
                plot["legend"] = args
            elif tag == "EVENT-LIST":
                plot["eventList"] = [parse_event(e) for e in args if e]
            elif tag == "GROUP":
                plot["groups"].setdefault(args[0], []).append({
                    "time": number(args[1]), "look": args[2],
                    "events": [parse_event(e) for e in args[3].split(" ")], "label": args[4]})
            elif tag == "ICON":
                plot["icons"].append({
                    "series": int(args[0]), "group": int(args[1]), "time": number(args[2]),
                    "x": number(args[3]), "y": number(args[4]), "yOnLeftAxis": number(args[5]),
                    "look": args[6], "sample": int(args[7]), "toolTip": args[8],
                    "risky": args[9] == "risky"})
            elif tag == "END":
                ended = True
            else:
                fail("unknown record " + tag)
    if not ended:
        fail("the transcript has no END: the probe did not finish")
    if meta is None:
        fail("the transcript has no META record")
    return meta, presets, simulations


def icon_digests(icons):
    """The digests of a plot's icons (README.md): of the positions and looks, and of the tool tips."""
    positions = "".join("{} {} {:016x} {:016x} {:016x} {:016x} {}\n".format(
        i["series"], i["group"], bits(i["time"]), bits(i["x"]), bits(i["y"]), bits(i["yOnLeftAxis"]),
        i["look"]) for i in icons)
    tool_tips = "".join(i["toolTip"] + "\n" for i in icons)
    return short_digest(positions), short_digest(tool_tips)


def check(simulations, presets):
    for simulation in simulations:
        where = "{} simulation {}".format(simulation["file"], simulation["simulation"])
        if len(simulation["plots"]) != len(presets):
            fail(where + ": not one plot per preset")
        for plot in simulation["plots"]:
            here = "{} preset {}".format(where, plot["preset"])
            preset = presets[plot["preset"]]
            # The panel's copy of the preset: the preset's rows in the default units (which the presets
            # have), its name and events; no appearance.
            if plot["name"] != preset["name"] or plot["presetName"] != preset["name"]:
                fail(here + ": the panel renamed the preset")
            if [[r["type"], r["unit"], r["axis"]] for r in plot["rows"]] != preset["rows"]:
                fail(here + ": the panel changed the preset's rows")
            if any(r["color"] != "-" or r["lineStyle"] != "SOLID" for r in plot["rows"]):
                fail(here + ": a row has an appearance")
            if plot["events"] != preset["events"]:
                fail(here + ": the panel changed the preset's events")
            if plot["auto"]["axes"] != plot["filled"]["axes"]:
                fail(here + ": the filled configuration's axes are not the automatic ones")
            # Bug 45: OpenRocket's own values are recorded only where its common zero gave a candidate an
            # infinite bound, which the fix leaves out.
            if "openRocket" in plot:
                java = plot["openRocket"]
                if not any(math.isinf(v) for c in java["candidates"] for r in c["ranges"] for v in r):
                    fail(here + ": OpenRocket's values differ, but not by bug 45")
            if any(math.isinf(v) for c in plot["candidates"] for r in c["ranges"] for v in r):
                fail(here + ": a candidate's range is not finite after bug 45's fix")
            for tip in plot["toolTips"]:
                if tip["risky"]:
                    fail(here + ": a readout tool tip formats a value whose digits may depend on the libm")
            for icon in plot["icons"]:
                if icon["risky"]:
                    fail(here + ": an icon's tool tip formats a value whose digits may depend on the libm")


def compact_plot(plot):
    positions, tool_tips = icon_digests(plot["icons"])
    out = {
        "preset": plot["preset"],
        "name": plot["name"],
        "domain": plot["domain"],
        "rows": [[r["type"], r["unit"], r["axis"]] for r in plot["rows"]],
        "auto": plot["auto"],
        "candidates": plot["candidates"],
        "filled": plot["filled"],
    }
    if "openRocket" in plot:
        out["openRocket"] = dict(bug=45, **plot["openRocket"])
    out.update({
        "title": plot["title"],
        "subtitle": plot["subtitle"],
        "domainLabel": plot["domainLabel"],
        "domainRange": plot["domainRange"],
        "domainRangeWithEventLines": plot.get("domainRangeWithEventLines"),
        "yAxes": plot["yAxes"],
        "panelColors": plot["panelColors"],
        "series": [{k: s[k] for k in ("name", "axis", "branch", "row", "unit", "points", "digest")}
                   for s in plot["series"]],
        "legend": plot["legend"],
        "groups": plot["groups"].get("all", []),
        "icons": {"count": len(plot["icons"]), "digest": positions, "toolTipDigest": tool_tips},
    })
    return out


def detailed_plot(plot, branch_count):
    return {
        "preset": plot["preset"],
        "series": [{k: s[k] for k in ("min", "max", "first", "middle", "last")} for s in plot["series"]],
        "toolTips": [{k: t[k] for k in ("series", "point", "name", "html")} for t in plot["toolTips"]],
        "eventList": plot["eventList"],
        "branchGroups": [plot["groups"].get(str(b), []) for b in range(branch_count)],
        "icons": [{k: i[k] for k in ("series", "group", "time", "x", "y", "yOnLeftAxis", "look", "toolTip")}
                  for i in plot["icons"]],
    }


def to_json(value):
    """Compact JSON with Java's special doubles as strings."""
    def convert(v):
        if isinstance(v, float):
            return as_json_number(v)
        if isinstance(v, dict):
            return {k: convert(x) for k, x in v.items()}
        if isinstance(v, list):
            return [convert(x) for x in v]
        return v
    return json.dumps(convert(value), ensure_ascii=True, separators=(", ", ": "), allow_nan=False)


def write_document(path, head, simulations, simulation_body):
    """One simulation per block, one plot per line, so that a change shows as the lines of its plots."""
    lines = ["{"]
    for key, value in head.items():
        lines.append(" {}: {},".format(json.dumps(key), to_json(value)))
    lines.append(' "simulations": [')
    for i, (simulation, plots) in enumerate(simulations):
        lines.append("  {" + to_json(simulation_body(simulation))[1:-1] + ', "plots": [')
        for j, plot in enumerate(plots):
            lines.append("   " + to_json(plot) + ("," if j + 1 < len(plots) else ""))
        lines.append("  ]}" + ("," if i + 1 < len(simulations) else ""))
    lines.append(" ]")
    lines.append("}")
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")


def main():
    if len(sys.argv) != 5:
        fail("usage: gen_simulation_plots.py <transcript> <output directory> <commit> <dirty>")
    transcript, out_dir, commit, dirty = sys.argv[1:]
    meta, presets, simulations = read_transcript(transcript)
    check(simulations, presets)
    head = {
        "schema": None,
        "schemaVersion": SCHEMA_VERSION,
        "generator": "tools/openrocket-plots/generate.sh (SimulationPlotsProbe)",
        "openrocket": {"commit": commit, "version": meta["version"], "dirty": dirty == "true",
                       "javaVersion": meta["javaVersion"]},
    }

    def identity(simulation):
        return {"file": simulation["file"], "simulation": simulation["simulation"], "name": simulation["name"]}

    def compact_simulation(simulation):
        body = identity(simulation)
        body["branches"] = simulation["branches"]
        body["seriesLabels"] = simulation["seriesLabels"]
        return body

    compact_head = dict(head, schema="simulation-plots")
    compact_head["presets"] = presets
    write_document(out_dir + "/simulation-plots.json", compact_head,
                   [(s, [compact_plot(p) for p in s["plots"]]) for s in simulations], compact_simulation)

    chosen = []
    for file, index in DETAILS:
        found = [s for s in simulations if s["file"] == file and s["simulation"] == index]
        if len(found) != 1:
            fail("no stored simulation {} of {}".format(index, file))
        chosen.append(found[0])
    detail_head = dict(head, schema="simulation-plot-details")
    write_document(out_dir + "/simulation-plot-details.json", detail_head,
                   [(s, [detailed_plot(p, len(s["branches"])) for p in s["plots"]]) for s in chosen],
                   identity)


if __name__ == "__main__":
    main()
