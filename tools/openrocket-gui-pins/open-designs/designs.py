#!/usr/bin/env python3
"""Prints the designs of QtRocket's exit suite OpenDesignsExitGolden (tests/gui/exit/open_designs_exit_tests.cpp,
makeOpenDesigns()) in its order, for OpenDesignsTabProbe: one line each, its fields separated by tabs,

    <name> <kind> <file>

<name> the name of the design's tests ("example_a_simple_model_rocket", "resave_example_...", "legacy_v1_6_apocd"),
<kind> example (a file of data/examples, an archive), example-resave, testrocket, legacy, coverage (OpenRocket's
re-saves of the golden harness, tests/data/goldens/*/resave/rocket.ork) or gui (the design OpenRocket's GUI built
from scratch, tests/data/goldens/gui/from-scratch/rocket.ork), and <file> its absolute path. The examples come
from the manifest's inputs of kind "example", then their re-saves, then its test rockets, then its re-saves of kind
"legacy" and "coverage", then the GUI's design: the 66 of plan section 9.5.

usage: designs.py REPOSITORY
"""
import json
import pathlib
import sys


def test_name(text):
    return text.replace("-", "_")


def main():
    repo = pathlib.Path(sys.argv[1]).resolve()
    goldens = repo / "tests" / "data" / "goldens"
    with open(goldens / "manifest.json", encoding="utf-8") as f:
        manifest = json.load(f)
    designs = []
    examples = [i for i in manifest["inputs"] if i["kind"] == "example"]
    for example in examples:
        designs.append((test_name(example["name"]), "example", repo / example["source"]))
    for example in examples:
        designs.append(("resave_" + test_name(example["name"]), "example-resave", goldens / example["resave"]))
    for rocket in (i for i in manifest["inputs"] if i["kind"] == "testrocket"):
        designs.append((test_name(rocket["name"]), "testrocket", goldens / rocket["resave"]))
    for kind in ("legacy", "coverage"):
        for resave in (i for i in manifest["resaves"]["inputs"] if i["kind"] == kind):
            designs.append((test_name(resave["name"]), kind, goldens / resave["resave"]))
    designs.append(("gui_from_scratch", "gui", goldens / "gui" / "from-scratch" / "rocket.ork"))
    for name, kind, path in designs:
        if not path.is_file():
            raise SystemExit("no file %s for %s" % (path, name))
        print("%s\t%s\t%s" % (name, kind, path))


if __name__ == "__main__":
    main()
