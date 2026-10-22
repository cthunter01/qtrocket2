#!/usr/bin/env python3
"""Writes ActionsRulesProbe's transcripts into kTranscripts of tests/gui/main/RocketActionsTests.cpp.

usage: gen_actions_rules.py TRANSCRIPT TEST_FILE

The transcript holds a block per test rocket, "== <maker>" and its lines, after a "#" line that names
the states; each block becomes a row {.method, .transcript}. The rows replace the body of
"    static constexpr std::array<RulesTranscript, <count>> kTranscripts{{" up to the next "    }};"
in TEST_FILE (but for the empty lines at its end), with <count> set to theirs; the caller formats
the file afterwards (clang-format joins the first lines of each row as the committed file has them).
"""
import re
import sys


def main():
    transcript_path, test_path = sys.argv[1], sys.argv[2]
    blocks = []
    with open(transcript_path, encoding="utf-8") as f:
        for line in f.read().splitlines():
            if line.startswith("#") or line.startswith("[openrocket]"):
                continue
            if line.startswith("== "):
                blocks.append((line[3:], []))
            elif blocks:
                blocks[-1][1].append(line)
            else:
                raise SystemExit(f"error: {transcript_path}: a line before the first block: {line}")
    if not blocks:
        raise SystemExit(f"error: {transcript_path} has no block")
    rows = []
    for method, lines in blocks:
        if not lines:
            raise SystemExit(f"error: {transcript_path}: the block '{method}' is empty")
        for line in lines:
            if ')"' in line:
                raise SystemExit(f"error: a transcript line ends the raw string: {line}")
        text = "\n".join(lines)
        rows.append(f'        {{.method     = "{method}",\n         .transcript = R"({text})"}},')

    with open(test_path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    pattern = re.compile(r"^    static constexpr std::array<RulesTranscript, \d+> kTranscripts\{\{$")
    starts = [i for i, line in enumerate(lines) if pattern.match(line)]
    if len(starts) != 1:
        raise SystemExit(f"error: {test_path} has {len(starts)} definitions of kTranscripts, not one")
    start = starts[0]
    end = next((i for i in range(start + 1, len(lines)) if lines[i] == "    }};"), None)
    if end is None:
        raise SystemExit(f"error: no '    }}}};' after kTranscripts in {test_path}")
    # The empty lines at the end of the table stay (the committed table ends with one).
    while end - 1 > start and lines[end - 1] == "":
        end -= 1
    lines[start] = f"    static constexpr std::array<RulesTranscript, {len(rows)}> kTranscripts{{{{"
    lines[start + 1:end] = "\n".join(rows).split("\n")
    with open(test_path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
