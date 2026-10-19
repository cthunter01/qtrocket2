#!/usr/bin/env python3
"""Writes tests/core/figure/FinMassShapePins.h and tests/core/figure/FigureGeometryPins.h from
FinMassShapesProbe.java's output.

usage: gen_fin_mass_shape_pins.py DUMP OUT_SHAPE_HEADER OUT_GEOMETRY_HEADER
"""
import math
import sys

# The components whose shapes are pinned, by design: a pre-order tree index (all its active
# instances in the five views of the "I" records: side0, side30, top0, back0, back30), or
# (index, contexts or None, views or None) for some instances or views only.
SELECTION = [
    ("example", "3D printable nose cone and fins.ork", [3]),
    ("example", "A simple model rocket.ork", [4, 5, 6, 8]),
    # A rail button turned by PI.
    ("example", "Chute release.ork", [(10, None, {"side0"})]),
    ("example", "Clustered motors.ork", [(16, [0], None)]),
    ("example", "Deployable payload.ork", [4]),
    # A booster set; a rail button turned by -PI/2.
    ("example", "Parallel booster staging.ork", [5, (17, None, {"side30", "back30"})]),
    ("example", "Pods--airframes and winglets.ork", [4, (9, [0], None), 10, 15]),
    ("example", "Pods--powered with recovery deployment.ork", [(4, [0], None), (9, None, {"side0", "back0"})]),
    # Canted fins.
    ("example", "Simulation extensions.ork", [(8, [0], None)]),
    ("example", "Three stage low power rocket.ork", [(19, [0], None)]),
    # Fin tabs.
    ("example", "Two stage high power rocket.ork", [(22, [0], None), (49, [0], None)]),
    ("testrocket", "makeBigBlue", [(4, [0], None)]),
    ("testrocket", "makeEndPlateRocket", [(7, [0], None)]),
    ("testrocket", "makeEstesAlphaIIIWithPods", [(9, [0], None)]),
    ("testrocket", "makeFalcon9Heavy", [(11, [0], None), (15, [0], None)]),
    ("testrocket", "makeMultiStageEventTestRocket", [(8, [0], None)]),
]

# The hand-built cases that are not pinned (they repeat others).
HAND_SKIPPED = {
    ("rail-screw/rot0", "side"), ("rail-screw/rot0", "back"),
    ("rail-screw/rot1", "back"),
    ("rail-screw/rot2", "side"), ("rail-screw/rot2", "back"),
    ("rail-screw/rot4", "side"), ("rail-screw/rot4", "back"), ("rail-screw/rot5", "back"),
    ("rail-screw/rot6", "side"), ("rail-screw/rot6", "back"),
    ("rail-angle/axial", "side"), ("rail-angle/axial", "back"),
    ("rail-default/off", "side"), ("rail-default/off", "back"),
    ("rail-no-base/off", "side"), ("rail-no-base/off", "back"),
    ("rail-no-flange/off", "side"), ("rail-no-flange/off", "back"),
    ("fin-canted-thin/axial", "side"), ("fin-canted-thin/axial", "back"),
    ("fin-canted-negative/off", "side"), ("fin-canted-negative/off", "back"),
    ("fin-canted-tab-on-transition/axial", "side"), ("fin-canted-tab-on-transition/axial", "back"),
}


def view_key(record):
    return record["view"] + ("0" if record["rotation"] == 0 else "30")


def selected(record, choices):
    for choice in choices:
        if isinstance(choice, int):
            if record["index"] == choice:
                return True
        else:
            index, contexts, views = choice
            if (record["index"] == index and (contexts is None or record["ctx"] in contexts)
                    and (views is None or view_key(record) in views)):
                return True
    return False


# The figures (RocketFigureGeometry) that are pinned: (source, name, all six views or FEW_VIEWS only,
# also with the markers preference on in MARKER_VIEWS, without and with a selection).
GEOMETRY_DESIGNS = [
    ("testrocket", "makeEstesAlphaIII", True, False),
    ("testrocket", "makeBeta", True, False),
    ("testrocket", "makeSimple2Stage", True, False),
    ("testrocket", "makeBigBlue", True, False),
    ("testrocket", "makeIsoHaisu", True, False),
    ("testrocket", "makeFalcon9Heavy", True, True),
    ("testrocket", "makeMultiStageEventTestRocket", True, False),
    ("testrocket", "makeEndPlateRocket", True, False),
    ("testrocket", "makeEstesAlphaIIIWithPods", True, True),
    ("testrocket", "makeEstesAlphaIIIWithMotorPods", True, False),
    ("testrocket", "makeEstesAlphaIIIWithSecondMotor", True, False),
    ("testrocket", "makeEstesAlphaIIIwithInlinePod", True, False),
    ("testrocket", "makeClusterPods", True, False),
    ("example", "Clustered motors.ork", False, False),
    ("example", "Parallel booster staging.ork", False, True),
    ("hand-built", "empty booster sets", True, True),
    ("new", "design", True, False),
]
# The views of the figures with the markers preference on, and of the designs marked False above.
FEW_VIEWS = {("side", 0.0), ("back", math.pi / 6)}
MARKER_VIEWS = {("side", 0.0)}

VIEWS = {("side", 0.0), ("back", math.pi / 6)}

VIEWS = {"side": "FigureView::SIDE", "top": "FigureView::TOP", "back": "FigureView::BACK"}
ARC_TYPES = {"0": "ARC_OPEN", "1": "ARC_CHORD", "2": "ARC_PIE"}
LETTER_TYPES = {"RR": "ROUND_RECT", "R": "RECT", "E": "ELLIPSE", "N": "LINE", "PF": "PATH_FLOAT",
                "PD": "PATH_DOUBLE", "A0": "ARC_OPEN", "A1": "ARC_CHORD", "A2": "ARC_PIE"}


def fx(token):
    v = float.fromhex(token)
    assert math.isfinite(v), token
    return v


def lit(v):
    if v == 0 and math.copysign(1.0, v) < 0:
        return "-0.0"
    r = repr(v)
    assert float(r) == v
    return r


def parse_shape(tokens):
    kind = tokens[0]
    if kind in ("R", "E", "N"):
        assert len(tokens) == 5, tokens
        values = [fx(t) for t in tokens[1:5]]
        return ({"R": "RECT", "E": "ELLIPSE", "N": "LINE"}[kind], tuple(values), "")
    if kind == "RR":
        assert len(tokens) == 7, tokens
        return ("ROUND_RECT", tuple(fx(t) for t in tokens[1:7]), "")
    if kind == "A":
        assert len(tokens) == 8, tokens
        return (ARC_TYPES[tokens[7]], tuple(fx(t) for t in tokens[1:7]), "")
    assert kind == "P", tokens
    precision = tokens[1]
    assert tokens[2] == "1", "winding rule NON_ZERO expected"
    values = []
    letters = []
    i = 3
    counts = {"M": 2, "L": 2, "Q": 4, "C": 6, "Z": 0}
    while i < len(tokens):
        letter = tokens[i]
        letters.append(letter)
        n = counts[letter]
        values.extend(fx(t) for t in tokens[i + 1 : i + 1 + n])
        i += 1 + n
    return ("PATH_FLOAT" if precision == "F" else "PATH_DOUBLE", tuple(values), "".join(letters))


def parse(path):
    designs = {}
    geometries = {}
    hand = []
    current = None
    current_geometry = None
    record = None
    figure = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            tokens = line.split(" ")
            tag = tokens[0]
            if tag == "D":
                record = None
                figure = None
                if line == "D hand-built":
                    current = None
                    current_geometry = None
                else:
                    source, name = line[2:].split(" ", 1)
                    current = designs.setdefault((source, name), [])
                    current_geometry = geometries.setdefault((source, name), [])
            elif tag in ("C", "X"):
                record = None
            elif tag == "I":
                index, ctx, number, view, rot = tokens[1:6]
                assert tokens[6] == "T"
                record = {
                    "index": int(index),
                    "ctx": int(ctx),
                    "number": int(number),
                    "view": view,
                    "rotation": fx(rot),
                    "transform": tuple(fx(t) for t in tokens[7:20]),
                    "shapes": [],
                }
                current.append(record)
            elif tag == "H":
                name, view = tokens[1], tokens[2]
                assert tokens[3] == "T"
                record = {"name": name, "view": view, "transform": tuple(fx(t) for t in tokens[4:17]),
                          "shapes": []}
                hand.append(record)
            elif tag == "S":
                if tokens[1] == "null":
                    continue
                kind, values, letters = parse_shape(tokens[2:])
                record["shapes"].append((kind, values, letters, tokens[1] == "v"))
            elif tag == "G":
                view, rot, markers_word, markers, selection_word = tokens[1:6]
                assert markers_word == "markers" and selection_word == "selection"
                selection = [int(t) for t in tokens[6:]]
                assert len(selection) <= 1
                figure = {"view": view, "rotation": fx(rot), "markers": markers == "true",
                          "selection": selection[0] if selection else -1, "shapes": [], "motors": []}
                current_geometry.append(figure)
            elif tag == "B":
                figure["bounds"] = tuple(fx(t) for t in tokens[1:5])
            elif tag == "E":
                tree, extra, ctx, number, order, k, vis, letter = tokens[1:9]
                figure["shapes"].append((int(tree), extra == "1", int(ctx), int(number), int(order), int(k),
                                         vis == "v", LETTER_TYPES[letter],
                                         tuple(fx(t) for t in tokens[9:13])))
            elif tag == "M":
                mount, loc = int(tokens[1]), int(tokens[2])
                kind, values, _ = parse_shape(tokens[3:])
                figure["motors"].append((mount, loc, kind, values))
            else:
                raise ValueError(line)
    return designs, geometries, hand


def wrapped(items, per_line, indent):
    lines = []
    for i in range(0, len(items), per_line):
        lines.append(indent + ", ".join(items[i : i + per_line]))
    return (",\n").join(lines)


def run_length(text):
    out = []
    i = 0
    while i < len(text):
        j = i
        while j < len(text) and text[j] == text[i]:
            j += 1
        out.append(text[i] + (str(j - i) if j - i > 1 else ""))
        i = j
    return '"' + "".join(out) + '"'


def array4(values):
    return "{" + ", ".join(lit(v) for v in values) + "}"


class Writer:
    def __init__(self):
        self.values = {}
        self.value_lines = []
        self.shape_lists = {}
        self.shape_lines = []
        self.transforms = {}
        self.transform_lines = []

    def transform_name(self, t):
        if t not in self.transforms:
            name = f"kTransform{len(self.transforms)}"
            self.transforms[t] = name
            self.transform_lines.append(f"inline constexpr TransformPin {name}{{")
            self.transform_lines.append(wrapped([lit(v) for v in t], 3, "    "))
            self.transform_lines.append("};")
        return self.transforms[t]

    def values_name(self, values):
        if values not in self.values:
            name = f"kValues{len(self.values)}"
            self.values[values] = name
            items = [lit(v) for v in values]
            one_line = f"inline constexpr std::array<double, {len(values)}> {name}{{{', '.join(items)}}};"
            if len(one_line) <= 100:
                self.value_lines.append(one_line)
            else:
                self.value_lines.append(f"inline constexpr std::array<double, {len(values)}> {name}{{")
                self.value_lines.append(wrapped(items, 4, "    "))
                self.value_lines.append("};")
        return self.values[values]

    def shapes_name(self, shapes):
        key = tuple(shapes)
        if key not in self.shape_lists:
            name = f"kShapes{len(self.shape_lists)}"
            self.shape_lists[key] = name
            if not shapes:
                self.shape_lines.append(f"inline constexpr std::array<ShapePin, 0> {name}{{}};")
                return name
            self.shape_lines.append(f"inline constexpr std::array<ShapePin, {len(shapes)}> {name}{{{{")
            for kind, values, letters, visible in shapes:
                vis = "" if visible else ", false"
                self.shape_lines.append(
                    f"    ShapePin{{PinShape::{kind}, {self.values_name(values)}, {run_length(letters)}{vis}}},"
                )
            self.shape_lines.append("}};")
        return self.shape_lists[key]


SHAPE_HEADER = """#pragma once

// GENERATED by tools/openrocket-goldens/figure-pins/generate.sh (gen_fin_mass_shape_pins.py, from
// the output of FinMassShapesProbe.java, run against the classes the golden harness compiles of
// OpenRocket's core and figure shape providers, with JDK 17):
// what OpenRocket's shape providers FinSetShapes, MassComponentShapes, ParachuteShapes,
// StreamerShapes, ShockCordShapes, RailButtonShapes and ComponentAssemblyShapes (for pod sets,
// booster sets and the rocket) return. Do not edit by hand: change the probe or the generator's
// selection and run both again. Test-only.
//
// - kDesigns: components of examples (loaded without presets) and of test rockets, both with their
//   automatic dimensions settled as the golden harness settles them, each active instance in the
//   views side 0, side 30 deg, top 0, back 0 and back 30 deg, with the transformation RocketFigure
//   hands the provider;
// - kHandBuilt: components the probe builds, under transformations of its own. The rays of a
//   deployment charge are random in Java (an unseeded java.util.Random): its side pins hold the
//   first two shapes only.
// Every number is the exact double Java computed (written by Double.toHexString), here in its
// shortest decimal form; a Path2D.Float's coordinates are the doubles of its floats. A shape whose
// pin ends in false is invisible (ORColor.INVISIBLE).

#include <array>

#include "QtRocket/figure/FigureView.h"
#include "figure/ShapePins.h"

namespace QtRocket::Test::FinMassShapePins
{

// clang-format off
// NOLINTBEGIN(modernize-use-std-numbers): Java's values, of which some lie near a constant
"""

GEOMETRY_HEADER = """#pragma once

// GENERATED by tools/openrocket-goldens/figure-pins/generate.sh (gen_fin_mass_shape_pins.py, from
// the output of FinMassShapesProbe.java, run against the classes the golden harness compiles of
// OpenRocket's core and figure shape providers, with JDK 17):
// what OpenRocket's RocketFigure assembles for whole designs (updateShapes, addThisShape,
// drawMotors and updateSubjectDimensions, replicated line by line by the probe). Do not edit by
// hand: change the probe or the generator's selection and run both again. Test-only.
//
// kFigures: the test rockets, a few examples (loaded without presets, settled as the golden harness
// settles them), a rocket the probe builds with booster sets without children (the extra render
// instances) and a new design (OpenRocketDocumentFactory.createNewRocket), with the markers
// preference off in the views side, top and back, each at 0 and 30 deg (the examples in side 0 and
// back 30 deg only); for the designs with pod sets or booster sets also with it on, without a
// selection and with the first such assembly selected, in the side view at 0. Each holds the
// subject bounds, every instance that has shapes (its component's tree index, whether it is an
// extra render instance, its context index and instance number, its display order, how many
// shapes it has, and its first shape's primitive and Shape.getBounds2D()), sorted by extra render,
// component and context since Java's order follows the hash order of its instance maps, and the
// motors (the mount's tree index, the location index and the shape) sorted by the mount's tree
// index.
// Every number is the exact double Java computed (written by Double.toHexString), here in its
// shortest decimal form.

#include <array>

#include "QtRocket/figure/FigureView.h"
#include "figure/ShapePins.h"

namespace QtRocket::Test::FigureGeometryPins
{

// clang-format off
// NOLINTBEGIN(modernize-use-std-numbers): Java's values, of which some lie near a constant
"""

FOOTER_LINES = ["// NOLINTEND(modernize-use-std-numbers)", "// clang-format on", ""]


def write_shape_header(designs, hand, out_path):
    w = Writer()
    design_lines = []
    design_entries = []
    for source, name, indices in SELECTION:
        records = [r for r in designs[(source, name)] if selected(r, indices)]
        assert {r["index"] for r in records} == {c if isinstance(c, int) else c[0] for c in indices}, name
        var = "kDesign" + "".join(ch for ch in name.replace(".ork", "").title() if ch.isalnum())
        entries = []
        for r in records:
            shapes = w.shapes_name(r["shapes"])
            entries.append(
                f"InstancePin{{{r['index']}, {r['ctx']}, {r['number']}, {VIEWS[r['view']]}, {lit(r['rotation'])},\n"
                f"        {w.transform_name(r['transform'])}, {shapes}}}"
            )
        design_lines.append(f"inline constexpr std::array<InstancePin, {len(entries)}> {var}{{{{")
        design_lines.extend("    " + e + "," for e in entries)
        design_lines.append("}};")
        design_entries.append(f'DesignPin{{"{source}", "{name}", {var}}}')
    hand_entries = []
    for r in hand:
        if (r["name"], r["view"]) in HAND_SKIPPED:
            continue
        shapes = r["shapes"]
        if r["name"].startswith("mass-DEPLOYMENTCHARGE/") and r["view"] == "side":
            # The rays: 14 moves and 14 lines from the centre, at random.
            kind, values, letters, visible = shapes[2]
            assert len(shapes) == 3 and kind == "PATH_DOUBLE" and letters == "ML" * 14, shapes[2]
            shapes = shapes[:2]
        name = w.shapes_name(shapes)
        hand_entries.append(
            f'HandBuiltPin{{"{r["name"]}", {VIEWS[r["view"]]}, 1.0,\n'
            f"        {w.transform_name(r['transform'])}, {name}}}"
        )
    out = [SHAPE_HEADER]
    out.extend(w.value_lines)
    out.append("")
    out.extend(w.shape_lines)
    out.append("")
    out.extend(w.transform_lines)
    out.append("")
    out.extend(design_lines)
    out.append("")
    out.append(f"inline constexpr std::array<DesignPin, {len(design_entries)}> kDesigns{{{{")
    out.extend("    " + e + "," for e in design_entries)
    out.append("}};")
    out.append("")
    out.append(f"inline constexpr std::array<HandBuiltPin, {len(hand_entries)}> kHandBuilt{{{{")
    out.extend("    " + e + "," for e in hand_entries)
    out.append("}};")
    out.extend(FOOTER_LINES)
    out.append("}  // namespace QtRocket::Test::FinMassShapePins")
    out.append("")
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))


def in_views(figure, views):
    return any(figure["view"] == v and abs(figure["rotation"] - r) < 1e-15 for v, r in views)


def geometry_selected(figure, all_views, markers):
    if figure["markers"]:
        return markers and in_views(figure, MARKER_VIEWS)
    return all_views or in_views(figure, FEW_VIEWS)


def figure_instances(figure):
    """The instances of a figure, sorted by (extra, tree, context): each with its number, display
    order, shape count, and its first shape's primitive and bounds."""
    by_instance = {}
    for tree, extra, ctx, number, order, k, visible, kind, bounds in figure["shapes"]:
        key = (extra, tree, ctx)
        entry = by_instance.setdefault(key, {"number": number, "order": order, "shapes": {}})
        assert entry["number"] == number and entry["order"] == order
        assert k not in entry["shapes"]
        entry["shapes"][k] = (kind, bounds)
    result = []
    for key in sorted(by_instance):
        entry = by_instance[key]
        count = len(entry["shapes"])
        assert sorted(entry["shapes"]) == list(range(count)), key
        kind, bounds = entry["shapes"][0]
        result.append((key, entry["number"], entry["order"], count, kind, bounds))
    return result


def write_geometry_header(geometries, out_path):
    blocks = []
    entries = []
    for source, name, all_views, markers in GEOMETRY_DESIGNS:
        figures = [g for g in geometries[(source, name)] if geometry_selected(g, all_views, markers)]
        assert figures, name
        base = "".join(ch for ch in (name.replace(".ork", "")).title() if ch.isalnum())
        for n, g in enumerate(figures):
            var = f"k{base}{n}"
            instances = figure_instances(g)
            blocks.append(f"inline constexpr std::array<FigureInstancePin, {len(instances)}> {var}Instances{{{{")
            for (extra, tree, ctx), number, order, count, kind, bounds in instances:
                blocks.append(
                    f"    FigureInstancePin{{{tree}, {str(extra).lower()}, {ctx}, {number}, {order}, {count}, "
                    f"PinShape::{kind},\n        {array4(bounds)}}},")
            blocks.append("}};")
            motors = sorted(g["motors"], key=lambda m: (m[0], m[1]))
            blocks.append(f"inline constexpr std::array<FigureMotorPin, {len(motors)}> {var}Motors{{{{")
            for mount, loc, kind, values in motors:
                blocks.append(f"    FigureMotorPin{{{mount}, {loc}, PinShape::{kind}, {array4(values)}}},")
            blocks.append("}};")
            entries.append(
                f'FigurePin{{"{source}", "{name}", {VIEWS[g["view"]]}, {lit(g["rotation"])}, '
                f'{str(g["markers"]).lower()}, {g["selection"]},\n        {array4(g["bounds"])}, {var}Instances, '
                f'{var}Motors}}')
    out = [GEOMETRY_HEADER]
    out.extend(blocks)
    out.append("")
    out.append(f"inline constexpr std::array<FigurePin, {len(entries)}> kFigures{{{{")
    out.extend("    " + e + "," for e in entries)
    out.append("}};")
    out.extend(FOOTER_LINES)
    out.append("}  // namespace QtRocket::Test::FigureGeometryPins")
    out.append("")
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))


def main():
    designs, geometries, hand = parse(sys.argv[1])
    write_shape_header(designs, hand, sys.argv[2])
    write_geometry_header(geometries, sys.argv[3])


if __name__ == "__main__":
    main()
