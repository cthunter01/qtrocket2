#!/usr/bin/env python3
"""Compares two RocketInfoPins.h apart from Java's run-to-run last bits.

usage: compare_info_pins.py COMMITTED REGENERATED

OpenRocket sums the CG and the CP over hash maps whose keys hash the components' UUIDs: random for a
design the probe builds (the TestRockets makers), so the order of the sums, and with it the last bits
of a CG or CP (seen: up to 2 units in the last place for makeFalcon9Heavy and makeClusterPods), change
from run to run. rocket_info_golden_tests.cpp compares those values within a relative 1e-9. This
compares every line exactly but its hexadecimal floating literals, which may differ by up to
kMaxUlps units in the last place. Exit status 0 when the files are equal so, 1 otherwise.
"""
import re
import struct
import sys

HEX_FLOAT = re.compile(r"-?0x[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?p[-+]?\d+")
kMaxUlps = 4


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
    differing = 0
    largest = 0
    for number, (left, right) in enumerate(zip(a, b), start=1):
        if left == right:
            continue
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
    print(f"equal apart from {differing} values, at most {largest} ulp apart (Java's hash order)", file=sys.stderr)
    sys.exit(0)


if __name__ == "__main__":
    main()
