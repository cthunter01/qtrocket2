#!/usr/bin/env python3
"""Writes tests/gui/figure/RocketFigurePaintPins.h from FigurePaintProbe's output.

usage: gen_rocket_figure_paint_pins.py PROBE_OUTPUT HEADER COMMIT

Every double of the probe is Double.toHexString (exact); the header holds Python's shortest
round-trip decimal of it. Hit tests: Java's order among components of equal display order follows
its hash maps, so each run of equal display orders is sorted by tree index, the order QtRocket's
stable geometry gives.
"""
import os
import pathlib
import re
import subprocess
import sys

# The scenarios of FigurePaintProbe.main(): name -> (source, design, view, modification)
SCENARIOS = {
    "simple-side": ("example", "A simple model rocket.ork"),
    "simple-side-selected": ("example", "A simple model rocket.ork"),
    "simple-back": ("example", "A simple model rocket.ork"),
    "simple-top30": ("example", "A simple model rocket.ork"),
    "simple-side30": ("example", "A simple model rocket.ork"),
    "simple-styled": ("example", "A simple model rocket.ork"),
    "simple-hidden-body": ("example", "A simple model rocket.ork"),
    "pods-side": ("example", "Pods--airframes and winglets.ork"),
    "pods-back": ("example", "Pods--airframes and winglets.ork"),
    "pods-side-markers": ("example", "Pods--airframes and winglets.ork"),
    "cluster-back": ("example", "Clustered motors.ork"),
    "cluster-side": ("example", "Clustered motors.ork"),
    "boosters-side": ("example", "Parallel booster staging.ork"),
    "boosters-back30": ("example", "Parallel booster staging.ork"),
    "dual-side": ("example", "Dual parachute deployment.ork"),
    "twostage-side": ("example", "Two stage high power rocket.ork"),
    "tubefin-back": ("example", "Tube fin rocket.ork"),
    "beta-side": ("testrocket", "makeBeta"),
    "new-side": ("new", "design"),
    "new-back": ("new", "design"),
}

VIEWS = {"Side view": "FigureView::SIDE", "Top view": "FigureView::TOP", "Back view": "FigureView::BACK"}


def num(hexstring):
    value = float.fromhex(hexstring) if hexstring not in ("NaN",) else float("nan")
    if value != value:
        return "kNaN"
    text = repr(value)
    if "e" in text or "E" in text:
        mantissa, exponent = text.split("e")
        if "." not in mantissa:
            mantissa += ".0"
        text = mantissa + "e" + exponent
    return text


def camel(name):
    return "".join(part[:1].upper() + part[1:] for part in re.split(r"[-_]", name))


def parse(path):
    scenarios = []
    current = None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("S "):
            m = re.match(r"S (\S+) view=(.+?) rotation=(\S+) markers=(\S+) modification=(\S+)$", line)
            current = {"name": m.group(1), "view": m.group(2), "rotation": m.group(3),
                       "markers": m.group(4) == "true",
                       "modification": "" if m.group(5) == "null" else m.group(5),
                       "paints": [], "carets": [], "hits": []}
            scenarios.append(current)
        elif line.startswith("Z selection"):
            current["selection"] = line[len("Z selection"):].strip()
        elif line.startswith("F "):
            m = re.match(r"F user=(\S+) scale=(\S+) origin=(-?\d+),(-?\d+) size=(\d+),(\d+)$", line)
            current["figure"] = m.groups()
        elif line.startswith("K "):
            m = re.match(r"K cg=(\S+),(\S+) cp=(\S+),(\S+)$", line)
            current["carets_at"] = m.groups()
        elif line.startswith("P "):
            m = re.match(r"P (\S+)(?: comp=(\d+) order=(-?\d+))? color=(\d+),(\d+),(\d+),(\d+) width=(\S+) "
                         r"cap=(\d+) join=(\d+) dashes=(\S+) device=(\S+) (\S+) (\S+) (\S+)$", line)
            kind, comp, order, r, g, b, a, width, cap, join, dashes, x, y, w, h = m.groups()
            if cap != "0" or join != "2":
                raise SystemExit("unexpected cap/join: " + line)
            dash_list = [] if dashes == "none" else [float(d) for d in dashes.split(",")]
            # No extent (an empty path, Java's bounds (0, 0, 0, 0), or all its points equal, as the tip
            # of a nose cone in the back view): Qt drops the segments, QtRocket draws nothing.
            empty = float.fromhex(w) == 0 and float.fromhex(h) == 0
            current["paints"].append({
                "kind": kind, "comp": int(comp) if comp else -1, "order": int(order) if order else 0,
                "rgba": (int(r), int(g), int(b), int(a)), "width": float(width), "dashes": dash_list,
                "device": (x, y, w, h),
                # Java fills a caret at NaN (an empty area); QtRocket paints nothing there.
                "drawn": not ((kind == "shape" and int(a) == 0) or empty),
            })
        elif line.startswith("Q "):
            m = re.match(r"Q centre=(\S+),(\S+) samples=(\d+)$", line)
            if current["paints"][-1]["drawn"]:
                current["carets"].append(m.groups())
        elif line.startswith("H "):
            m = re.match(r"H (\d+),(\d+) robust=(\S+) ->(.*)$", line)
            x, y, robust, rest = m.groups()
            if robust != "true":
                continue
            hits = [tuple(int(v) for v in item.split(":")) for item in rest.split()]
            # Sort each run of equal display order by tree index.
            ordered = []
            i = 0
            while i < len(hits):
                j = i
                while j < len(hits) and hits[j][1] == hits[i][1]:
                    j += 1
                ordered.extend(sorted(hits[i:j]))
                i = j
            current["hits"].append((int(x), int(y), " ".join(str(c) for c, _ in ordered)))
    return scenarios


KINDS = {"shape": "PaintPinKind::SHAPE", "motor-fill": "PaintPinKind::MOTOR_FILL",
         "motor-draw": "PaintPinKind::MOTOR_OUTLINE", "caret": "PaintPinKind::CARET"}


def dash_text(dashes):
    values = list(dashes) + [0.0] * (4 - len(dashes))
    return "{" + ", ".join(repr(v) for v in values) + "}, " + str(len(dashes))


def main():
    probe, header, commit = sys.argv[1], sys.argv[2], sys.argv[3]
    scenarios = parse(probe)
    out = []
    w = out.append
    w("#pragma once")
    w("")
    w("// GENERATED by tools/openrocket-gui-pins/generate.sh (figure-paint/gen_rocket_figure_paint_pins.py")
    w("// from the output of figure-paint/probe/FigurePaintProbe.java: OpenRocket's RocketFigure painted")
    w("// headless, see RocketFigurePaintGolden in rocket_figure_paint_golden_tests.cpp); do not edit.")
    w("")
    w("#include <array>")
    w("#include <limits>")
    w("#include <ostream>")
    w("#include <span>")
    w("#include <string_view>")
    w("")
    w('#include "QtRocket/figure/FigureView.h"')
    w("")
    w("namespace QtRocket::Gui::Test")
    w("{")
    w("")
    w("/// The OpenRocket commit the pins come from.")
    w(f'inline constexpr std::string_view kRocketFigurePaintPinsCommit = "{commit}";')
    w("")
    w("/// What one recorded call of RocketFigure.paintComponent() drew.")
    w("enum class PaintPinKind")
    w("{")
    w("    /// g2.draw() of a component's shape.")
    w("    SHAPE,")
    w("    /// drawMotors(): g2.fill() of a motor, then MOTOR_OUTLINE, its g2.draw().")
    w("    MOTOR_FILL,")
    w("    MOTOR_OUTLINE,")
    w("    /// Caret.paint(): g2.fill() of a caret's area (blue CG, red CP).")
    w("    CARET,")
    w("};")
    w("")
    w("/// One recorded call, in the order of the paint: the colour, the stroke in device pixels (its")
    w("/// width and dashes times the scale; Java's SOLID is the dashes 10, 0, and a fill has no dashes)")
    w("/// and the bounds of the shape in device pixels (getBounds2D() of the transformed shape).")
    w("struct PaintPin")
    w("{")
    w("    PaintPinKind kind;")
    w("    /// The component's index in the rocket's tree (pre-order, the rocket 0), -1 for the others.")
    w("    int component;")
    w("    /// The component's display order in the view.")
    w("    int displayOrder;")
    w("    int red;")
    w("    int green;")
    w("    int blue;")
    w("    int alpha;")
    w("    double                width;")
    w("    std::array<double, 4> dashes;")
    w("    int                   dashCount;")
    w("    double                x;")
    w("    double                y;")
    w("    double                w;")
    w("    double                h;")
    w("    /// False for what QtRocket does not draw: a shape of a colour of alpha 0 (an invisible hit")
    w("    /// test shape, a component coloured so), a shape without extent (an empty path, Java's bounds")
    w("    /// (0, 0, 0, 0), or one whose points are all equal: the tip of a nose cone in the back view) and a")
    w("    /// caret at NaN (Java fills an empty area).")
    w("    bool drawn;")
    w("};")
    w("")
    w("/// A caret's centre in device pixels and, for each of ten offsets from it, whether Java's area")
    w("/// holds the point there ('1' or '0'): (3, 3), (-3, -3), (3, -3), (-3, 3), (6.6, 0), (0, 6.6),")
    w("/// (7.5, 0), (5.8, 0), (0, 0.5) and (4.5, 0).")
    w("struct CaretPin")
    w("{")
    w("    double           centreX;")
    w("    double           centreY;")
    w("    std::string_view samples;")
    w("};")
    w("")
    w("/// getComponentsByPoint() at a pixel of the figure: the tree indices of the components, in")
    w("/// QtRocket's order (Java's, each run of equal display order sorted by tree index). Only points")
    w("/// that give the same components a hundredth of a pixel away in every diagonal direction.")
    w("struct HitPin")
    w("{")
    w("    int              x;")
    w("    int              y;")
    w("    std::string_view components;")
    w("};")
    w("")
    w("/// One figure: the design, how it was shown and what Java painted. The figure is that of")
    w("/// RocketPanel.create2DPreviewFigure() at 96 dots per inch: scaleTo(1000 x 700), updateFigure(),")
    w("/// its preferred size; the carets where RocketPanel.updateExtras() puts them.")
    w("struct PaintScenarioPin")
    w("{")
    w("    std::string_view name;")
    w('    /// "example" (a file of data/examples), "testrocket" (a TestRockets maker) or "new"')
    w("    /// (OpenRocketDocumentFactory.createNewRocket()).")
    w("    std::string_view source;")
    w("    std::string_view design;")
    w("    FigureView       view;")
    w("    double           rotation;")
    w("    /// The tree indices of the selected components, space separated.")
    w("    std::string_view selection;")
    w("    /// The preference ShowMarkers.")
    w("    bool showMarkers;")
    w('    /// "styled": the body tube (tree index 3) coloured (10, 20, 30, 255) and dotted, the parachute')
    w("    /// (4) dash-dotted, the nose cone (2) coloured (200, 100, 50, 0); \"hide3\": the body tube")
    w('    /// hidden; "": nothing.')
    w("    std::string_view          modification;")
    w("    double                    userScale;")
    w("    double                    absoluteScale;")
    w("    int                       originX;")
    w("    int                       originY;")
    w("    int                       width;")
    w("    int                       height;")
    w("    double                    cgX;")
    w("    double                    cgY;")
    w("    double                    cpX;")
    w("    double                    cpY;")
    w("    std::span<const PaintPin> paints;")
    w("    std::span<const CaretPin> carets;")
    w("    std::span<const HitPin>   hits;")
    w("};")
    w("")
    w("/// What GoogleTest prints for the scenario of a test that failed: its name.")
    w("// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for")
    w("inline void PrintTo(const PaintScenarioPin& pin, std::ostream* out)")
    w("{")
    w("    *out << pin.name;")
    w("}")
    w("")
    w("inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();")
    w("")
    w("// clang-format off")
    w("// NOLINTBEGIN(modernize-use-designated-initializers): tables printed by the Java probe")
    w("// NOLINTBEGIN(modernize-use-std-numbers): Java's values, some of them near a constant")
    for s in scenarios:
        c = camel(s["name"])
        w(f"inline constexpr auto kPaints{c} = std::to_array<PaintPin>({{")
        for p in s["paints"]:
            x, y, ww, h = (num(v) for v in p["device"])
            r, g, b, a = p["rgba"]
            w(f"    {{{KINDS[p['kind']]}, {p['comp']}, {p['order']}, {r}, {g}, {b}, {a}, {p['width']!r}, "
              f"{dash_text(p['dashes'])}, {x}, {y}, {ww}, {h}, {'true' if p['drawn'] else 'false'}}},")
        w("});")
        if s["carets"]:
            w(f"inline constexpr auto kCarets{c} = std::to_array<CaretPin>({{")
            for cx, cy, samples in s["carets"]:
                w(f'    {{{num(cx)}, {num(cy)}, "{samples}"}},')
            w("});")
        w(f"inline constexpr auto kHits{c} = std::to_array<HitPin>({{")
        for x, y, comps in s["hits"]:
            w(f'    {{{x}, {y}, "{comps}"}},')
        w("});")
    w("")
    w("inline constexpr auto kPaintScenarios = std::to_array<PaintScenarioPin>({")
    for s in scenarios:
        c = camel(s["name"])
        source, design = SCENARIOS[s["name"]]
        user, scale, ox, oy, width, height = s["figure"]
        cgx, cgy, cpx, cpy = s["carets_at"]
        carets = f"kCarets{c}" if s["carets"] else "std::span<const CaretPin>{}"
        w(f'    {{"{s["name"]}", "{source}", "{design}", {VIEWS[s["view"]]}, {num(s["rotation"])}, '
          f'"{s.get("selection", "")}", {"true" if s["markers"] else "false"}, "{s["modification"]}", '
          f"{num(user)}, {num(scale)}, {ox}, {oy}, {width}, {height}, {num(cgx)}, {num(cgy)}, {num(cpx)}, "
          f"{num(cpy)}, kPaints{c}, {carets}, kHits{c}}},")
    w("});")
    w("// NOLINTEND(modernize-use-std-numbers)")
    w("// NOLINTEND(modernize-use-designated-initializers)")
    w("// clang-format on")
    w("")
    w("}  // namespace QtRocket::Gui::Test")
    open(header, "w", encoding="utf-8").write("\n".join(out) + "\n")
    # The repository's style (its .clang-format, wherever the header is written: generate.sh
    # --check writes it into a temporary directory), so that a run reproduces the committed
    # header byte for byte.
    style = pathlib.Path(__file__).resolve().parents[3] / ".clang-format"
    clang_format = os.environ.get("CLANG_FORMAT", "clang-format")
    subprocess.run([clang_format, "-i", f"--style=file:{style}", header], check=True)


if __name__ == "__main__":
    main()
