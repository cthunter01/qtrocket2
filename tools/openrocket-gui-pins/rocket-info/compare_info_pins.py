#!/usr/bin/env python3
"""Compares two RocketInfoPins.h apart from Java's run-to-run last bits in the designs the probe builds.

usage: compare_info_pins.py COMMITTED REGENERATED

OpenRocket sums the CG and the CP over hash maps whose keys hash the components' UUIDs. The UUIDs of
a design the probe builds (the TestRockets makers and its own designs, BUILT below) were random when
the committed table was made, so the order of the sums, and with it the last bits of a CG or CP
(seen: up to 2 units in the last place for makeFalcon9Heavy and makeClusterPods), changed from run
to run; the probe now seeds them (shared/probe/DeterministicUuids.java), so a regenerated table is
the same on every run but may still differ from the committed one in those bits.
rocket_info_golden_tests.cpp compares those values within a relative 1e-9. This compares every line
exactly but, in the tables and the scenario row of a design of BUILT, the hexadecimal floating
literals, which may differ by up to kMaxUlps units in the last place; a design loaded from a file
(the examples, special:simple-*) keeps the UUIDs stored in it and is compared byte for byte. Exit
status 0 when the files are equal so, 1 otherwise.
"""
import re
import struct
import sys

HEX_FLOAT = re.compile(r"-?0x[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?p[-+]?\d+")
kMaxUlps = 4

# The designs RocketInfoProbe builds (their scenario keys): every TestRockets maker ("testrocket:")
# and these.
BUILT = {"special:alpha-discontinuity", "special:body-tube-only", "special:new-design",
         "special:boosters-on-boosters-only-inner", "special:boosters-on-boosters-toggled"}
TABLE = re.compile(r"^inline constexpr .* kInfo(?:Warnings|Carets|PaintHidden|Paint)(\w+)\b")
SCENARIO = re.compile(r'^\s*RocketInfoScenarioPin\{"([^"]+)", "(\w+)"')


def ordered_bits(value):
    """The bits of @p value as an integer that orders like the doubles (adjacent doubles differ by 1)."""
    (bits,) = struct.unpack("<q", struct.pack("<d", value))
    return bits if bits >= 0 else -(bits & 0x7FFFFFFFFFFFFFFF)


def main():
    committed, regenerated = sys.argv[1], sys.argv[2]
    with open(committed, encoding="utf-8") as f:
        a = f.read().split("\n")
    with open(regenerated, encoding="utf-8") as f:
        b = f.read().split("\n")
    if a == b:
        print("byte for byte", file=sys.stderr)
        sys.exit(0)
    if len(a) != len(b):
        print(f"the files differ in length: {len(a)} and {len(b)} lines", file=sys.stderr)
        sys.exit(1)
    # The identifiers of the built designs' tables, from the scenario rows (key, identifier).
    built = set()
    for line in a:
        m = SCENARIO.match(line)
        if m and (m.group(1).startswith("testrocket:") or m.group(1) in BUILT):
            built.add(m.group(2))
    if not built:
        print("no scenario row of a built design in the committed file", file=sys.stderr)
        sys.exit(1)
    differing = 0
    largest = 0
    table = None
    for number, (left, right) in enumerate(zip(a, b), start=1):
        m = TABLE.match(left)
        if m:
            table = m.group(1)
        elif left.startswith("inline constexpr") or left == "});":
            table = None
        if left == right:
            continue
        m = SCENARIO.match(left)
        noisy = table in built if table is not None else bool(m and m.group(2) in built)
        if not noisy:
            print(f"line {number} differs (a design loaded from a file: compared exactly):\n  {left}\n  {right}",
                  file=sys.stderr)
            sys.exit(1)
        if HEX_FLOAT.sub("#", left) != HEX_FLOAT.sub("#", right):
            print(f"line {number} differs:\n  {left}\n  {right}", file=sys.stderr)
            sys.exit(1)
        for x, y in zip(HEX_FLOAT.findall(left), HEX_FLOAT.findall(right)):
            if x == y:
                continue
            ulps = abs(ordered_bits(float.fromhex(x)) - ordered_bits(float.fromhex(y)))
            if ulps > kMaxUlps:
                print(f"line {number}: {x} and {y} are {ulps} units in the last place apart", file=sys.stderr)
                sys.exit(1)
            differing += 1
            largest = max(largest, ulps)
    print(f"equal apart from {differing} values of built designs, at most {largest} ulp apart (Java's hash order)",
          file=sys.stderr)
    sys.exit(0)


if __name__ == "__main__":
    main()
