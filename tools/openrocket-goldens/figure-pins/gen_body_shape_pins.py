#!/usr/bin/env python3
"""Writes tests/core/figure/BodyShapePins.h from BodyShapesProbe.java's output.

usage: gen_body_shape_pins.py DUMP OUT_HEADER OPENROCKET_COMMIT DIRTY
"""
import math
import sys

# The components of the designs that are pinned, by design and pre-order tree index (all their
# active instances in the five views).
SELECTION = [
    ("example", "A simple model rocket.ork", [2, 3, 7, 9, 10, 11, 12]),
    ("example", "Clustered motors.ork", [4, 5, 6]),
    ("example", "Parallel booster staging.ork", [6, 7]),
    # The pod's tail cone: one instance, off the axis, in the rotated side view only.
    ("example", "Pods--airframes and winglets.ork", [12, (13, 0, "side30")]),
    ("example", "Tube fin rocket.ork", [7]),
    ("example", "Two stage high power rocket.ork", [4, 5, 21]),
    ("testrocket", "makeBeta", [9, 12, 16]),
    ("testrocket", "makeBigBlue", [2]),
    ("testrocket", "makeFalcon9Heavy", [2, 4]),
]

# The hand-built outlines that are not pinned (a sampled outline is some 9 KB of pins).
HAND_SKIPPED = {
    ("transition-power", "side"),
    ("transition-parabolic", "side"),
    ("transition-haack-third", "side"),
    ("transition-ellipsoid-unclipped", "side"),
    ("transition-boattail/off", "side"),
    ("transition-zero-length", "side"),
    ("nose-ogive", "side"),
    ("nose-power", "side"),
    ("nose-haack", "side"),
    ("nose-ogive-off", "side"),
    ("nose-ellipsoid-flipped", "side"),
    ("refine-fat-ogive", "side"),
    ("refine-haack-transition", "side"),
    ("refine-fat-ellipsoid", "side"),
    ("refine-fat-haack", "side"),
    ("refine-parabolic-nose", "side"),
}


def view_key(record):
    return record["view"] + ("0" if record["rotation"] == 0 else "30")


def selected(record, choices):
    for choice in choices:
        if isinstance(choice, int):
            if record["index"] == choice:
                return True
        else:
            index, ctx, view = choice
            if record["index"] == index and record["ctx"] == ctx and view_key(record) == view:
                return True
    return False


VIEWS = {"side": "FigureView::SIDE", "top": "FigureView::TOP", "back": "FigureView::BACK"}


def fx(token):
    v = float.fromhex(token)
    assert math.isfinite(v), token
    return v


def lit(v):
    r = repr(v)
    assert float(r) == v
    return r


def parse_shape(tokens):
    kind = tokens[0]
    if kind in ("R", "E", "N"):
        values = [fx(t) for t in tokens[1:5]]
        assert len(tokens) == 5
        return ({"R": "RECT", "E": "ELLIPSE", "N": "LINE"}[kind], tuple(values), "")
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
    hand = []
    current = None
    record = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            tokens = line.split(" ")
            tag = tokens[0]
            if tag == "K":
                continue
            if tag == "D":
                if line == "D hand-built":
                    current = None
                else:
                    source, name = line[2:].split(" ", 1)
                    current = designs.setdefault((source, name), [])
                record = None
            elif tag == "C" or tag == "X":
                record = None
            elif tag == "I":
                index, ctx, number, view, rot = tokens[1:6]
                assert tokens[6] == "T"
                transform = tuple(fx(t) for t in tokens[7:20])
                record = {
                    "index": int(index),
                    "ctx": int(ctx),
                    "number": int(number),
                    "view": view,
                    "rotation": fx(rot),
                    "transform": transform,
                    "shapes": [],
                }
                current.append(record)
            elif tag == "H":
                name, view = tokens[1], tokens[2]
                scale = 1.0
                rest = tokens[3:]
                if rest[0] == "scale":
                    scale = fx(rest[1])
                    rest = rest[2:]
                assert rest[0] == "T"
                transform = tuple(fx(t) for t in rest[1:14])
                record = {"name": name, "view": view, "scale": scale, "transform": transform, "shapes": []}
                hand.append(record)
            elif tag == "S":
                record["shapes"].append(parse_shape(tokens[1:]))
            else:
                raise ValueError(line)
    return designs, hand


def wrapped(items, per_line, indent):
    """The items, per_line to a line, each line but the last ending in a comma."""
    lines = []
    for i in range(0, len(items), per_line):
        lines.append(indent + ", ".join(items[i : i + per_line]))
    return (",\n").join(lines)


def run_length(text):
    """The segment letters with each run of more than one letter as the letter and its count:
    "MLLLZ" is "ML3Z"."""
    out = []
    i = 0
    while i < len(text):
        j = i
        while j < len(text) and text[j] == text[i]:
            j += 1
        out.append(text[i] + (str(j - i) if j - i > 1 else ""))
        i = j
    return '"' + "".join(out) + '"'


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
            for kind, values, letters in shapes:
                self.shape_lines.append(
                    f"    ShapePin{{PinShape::{kind}, {self.values_name(values)}, {run_length(letters)}}},"
                )
            self.shape_lines.append("}};")
        return self.shape_lists[key]


HEADER = """#pragma once

// GENERATED by tools/openrocket-goldens/figure-pins/generate.sh (gen_body_shape_pins.py, from the
// output of BodyShapesProbe.java, run against the classes the golden harness compiles of
// OpenRocket's core and figure shape providers, with JDK 17): what OpenRocket's shape
// providers BodyTubeShapes, TransitionShapes (with SymmetricComponentShapes and TubeShapes),
// RingComponentShapes, LaunchLugShapes and TubeFinSetShapes return, and EmptyShapes. Do not edit by
// hand: change the probe or the generator's selection and run both again. Test-only.
//
// - kDesigns: components of examples (loaded without presets) and of test rockets, both with their
//   automatic dimensions settled as the golden harness settles them, each active instance in the
//   views side 0, side 30 deg, top 0, back 0 and back 30 deg (the pod's tail cone in side 30 deg
//   only), with the transformation RocketFigure hands the provider;
// - kHandBuilt: components the probe builds, and EmptyShapes, under transformations of its own.
// Every number is the exact double Java computed (written by Double.toHexString), here in its
// shortest decimal form; a Path2D.Float's coordinates are the doubles of its floats.

#include <array>

#include "QtRocket/figure/FigureView.h"
#include "figure/ShapePins.h"

namespace QtRocket::Test::BodyShapePins
{

// clang-format off
// NOLINTBEGIN(modernize-use-std-numbers): Java's values, of which some lie near a constant
"""


def provenance_args(args):
    """The OpenRocket commit and the dirty flag of the command line (generate.sh passes them)."""
    commit, dirty = args
    if len(commit) != 40 or any(ch not in "0123456789abcdef" for ch in commit):
        sys.exit(f"not a commit: {commit}")
    if dirty not in ("true", "false"):
        sys.exit(f"dirty is true or false, not {dirty}")
    return commit, dirty == "true"


def with_provenance(header, commit, dirty):
    """@p header with the OpenRocket commit the pins come from: a comment line and the constants
    kOpenRocketCommit and kOpenRocketDirty (inside the clang-format off region)."""
    checkout = "a checkout with local changes (ALLOW_DIRTY=1)" if dirty else "a clean checkout"
    paragraph_end = "\n\n#include <array>\n"
    assert header.count(paragraph_end) == 1
    header = header.replace(
        paragraph_end,
        f"\n//\n// From OpenRocket {commit}, {checkout}.\n\n#include <array>\n#include <string_view>\n")
    nolint = "// NOLINTBEGIN(modernize-use-std-numbers): Java's values, of which some lie near a constant\n"
    assert header.endswith(nolint)
    return header + (
        "\n/// The OpenRocket commit the pins come from, and whether its checkout had local changes.\n"
        f'inline constexpr std::string_view kOpenRocketCommit = "{commit}";\n'
        f"inline constexpr bool kOpenRocketDirty = {'true' if dirty else 'false'};\n")


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    commit, dirty = provenance_args(sys.argv[3:])
    designs, hand = parse(sys.argv[1])
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
        shapes = w.shapes_name(r["shapes"])
        hand_entries.append(
            f'HandBuiltPin{{"{r["name"]}", {VIEWS[r["view"]]}, {lit(r["scale"])},\n'
            f"        {w.transform_name(r['transform'])}, {shapes}}}"
        )
    out = [with_provenance(HEADER, commit, dirty)]
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
    out.append("// NOLINTEND(modernize-use-std-numbers)")
    out.append("// clang-format on")
    out.append("")
    out.append("}  // namespace QtRocket::Test::BodyShapePins")
    out.append("")
    with open(sys.argv[2], "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
