# JDK 17's collator: the tables and pins of util/JavaCollator and util/AlphanumComparator

`tools/openrocket-collation` writes the generated files of QtRocket's `util/JavaCollator` (JDK 17's
`java.text.Collator.getInstance()` in en_US, which OpenRocket's tables sort their text columns with) and
`util/AlphanumComparator` (the port of OpenRocket's `util/AlphanumComparator`, the parts library's Part
Number sort). The files are committed; neither CMake nor CI ever needs Java. Never edit them by hand:
change a program here and regenerate. No source of the JDK is copied into QtRocket (its GPLv2-only licence
is not QtRocket's): the programs ask the running JDK for its collation elements and answers through its
public API, and `src/util/JavaCollator.cpp` reimplements the comparison from the collator's documented
behaviour, checked against those answers.

| Part | File | Program | Needs |
|---|---|---|---|
| `tables` | `src/util/JavaCollationTables.h` | `JavaCollationTables.java` | the JDK |
| `collator` | `tests/core/util/JavaCollatorPins.h` | `JavaCollatorPins.java` | the JDK |
| `alphanum` | `tests/core/util/AlphanumComparatorPins.h` | `AlphanumComparatorPins.java` | the JDK and OpenRocket's `AlphanumComparator.java` |

## What the tables hold, and why they suffice

`Collator.getInstance()` in en_US is a `RuleBasedCollator` at TERTIARY strength without decomposition
(the program checks this, and that it is the collator of `Collator.getInstance(Locale.US)`, the one
OpenRocket's motor sorting uses at PRIMARY strength). Java collates UTF-16 chars: its
`CollationElementIterator` turns a String into 32-bit elements (a 16-bit primary weight, an 8-bit
secondary and an 8-bit tertiary one), and the comparison only looks at those. So the tables describe every
string once they give the elements of every char and of every context in which a char's elements change:

- `kJavaCollationRuns` and `kJavaCollationElements`: the elements of each of the 835 chars of the Basic
  Multilingual Plane (U+0000 to U+FFFF, surrogates excluded) that the en_US rules map (Latin with its
  accented and expanding letters, digits, punctuation, symbols, currency signs, combining marks, spaces and
  the ignorable controls), as 774 runs of consecutive chars with the same elements, 1378 elements in all
  (at most three a char). The range is the whole plane, char by char: nothing is left out.
- `kJavaCollationContractions`: the pairs of chars whose elements together differ from those of one after
  the other. The program tries every mapped char before every char of the plane (53 million strings, about
  ten seconds) and finds one: U+0308 U+0301 (the decomposition of U+0344).
- The rules `JavaCollator.cpp` applies to everything else, which the program checks for every case: a char
  the rules do not map (62,653 of them: Greek, Cyrillic, CJK, the superscript digits, U+FFFD, ...) gives
  the elements 0x7FFF0000 and its code unit in the primary weight; a code point above U+FFFF (all 1,048,576
  of them) gives the elements of the mapped char of its low 18 bits when there is one (a quirk of the JDK:
  planes 4, 8, 12 and 16 repeat the mapped chars of plane 0, 3340 code points, but not its contraction),
  else 0x7FFF0000 and its two surrogates.
- No other context: the contraction before and after every char; the Thai and Lao blocks in all pairs
  (the scripts whose vowels a collator may reorder; en_US's rules hold neither); a seeded sample
  of two million pairs and one million triples of arbitrary code points. With `THOROUGH=1`, also every
  triple of mapped chars (582 million strings, about three minutes more; run for the committed
  tables).

A UTF-8 text has no lone surrogate, so these cover every text QtRocket compares. The program fails when
the JDK says otherwise about any of it.

## What the pins hold

- `collator`: Java's `compare()` at PRIMARY, SECONDARY and TERTIARY strength for the cases of
  `JavaCollatorPins.java` (the scouts' findings: the motor database's only pair of case texts that
  differ at TERTIARY but not PRIMARY, "Pro-98 6g XL" and "Pro98-6GXL"; the parts library's description and
  material pairs of that kind; and the corners of the rules), and for a corpus of 30,000 random pairs.
  The texts of the corpus are not stored: `tests/core/util/RandomTextPairs.h` makes them again from the
  seed (SplitMix64, as the program does) over the program's alphabet of 102 characters (ignorables,
  spaces, accents, hyphens, the contraction, punctuation, digits, letters, expanding letters, unmapped
  characters and code points above U+FFFF); three pairs in four are a text and an edited copy, so that most
  pairs share a long prefix and the secondary and tertiary rules decide. One character holds a pair's
  three answers.
- `alphanum`: OpenRocket's `AlphanumComparator.compare()`, its int as it is, for the cases of
  `AlphanumComparatorPins.java` (`AlphanumComparatorTest`'s, part numbers, numbers, UTF-16 lengths), for
  the two documented deviations (texts that NFD changes; QtRocket's comparator does not decompose), and
  for a corpus of 20,000 random pairs over an alphabet that NFD leaves alone (checked: every character and
  every pair of them). The script compiles `AlphanumComparator.java` from the OpenRocket checkout (it
  needs nothing but the JDK); the header records the checkout's commit, and
  `AlphanumComparatorPinsGolden` requires the goldens' (`tests/data/goldens/manifest.json`).

Measured once by hand beyond the committed corpora (scratch copies of the pin programs with larger
counts and other alphabets): five million random pairs at each of the three strengths, over the default
alphabet, over all 835 mapped chars with the plane repeats, and over a secondary-heavy alphabet, and two
million `AlphanumComparator` pairs, all equal to Java's answers.

## Regenerating

With Java 17 (the collation data is JDK 17's, and OpenRocket's releases bundle Java 17), clang-format (the
version of the repository's `.claude/hooks/format-cpp.sh` hook, which the files were formatted with) and,
for `alphanum`, the OpenRocket checkout at the commit of `tests/data/goldens/manifest.json`:

```sh
tools/openrocket-collation/generate.sh [--check] [tables] [collator] [alphanum]   # default: all three
THOROUGH=1 tools/openrocket-collation/generate.sh tables
```

The head of `generate.sh` lists the environment variables (`JAVA`, `JAVAC`, `CLANG_FORMAT`, `THOROUGH`,
`OPENROCKET_DIR`, `ALLOW_OTHER_COMMIT`). Each file is written into a temporary directory, formatted, and
compared with the committed one byte for byte: an equal file leaves the committed one alone, another
replaces it (`--check` only compares and exits with status 1). The programs run in en_US (the collator
follows the default locale) and print the same on every run: a regeneration with another update of Java
17 changes only the Java version in the first comment of each file. All three take about fifteen seconds
(`THOROUGH=1` three minutes more). The committed files were written with Temurin 17.0.20.1 and
`THOROUGH=1`.
