# OpenRocket's expression parser: character tables and parity data

`tools/openrocket-expression-parser` writes the two generated files of QtRocket's
`util/ExpressionParser` (the port of OpenRocket's `info.openrocket.core.util.ExpressionParser` and of the
exp4j copy it evaluates with, the input language of every number field of the GUI). Both files are
committed; neither CMake nor CI ever needs Java. Never edit them by hand: change the generator or its
input and regenerate.

- `JavaCharacterTables.java` writes `src/util/ExpressionParserTables.h` from the JDK alone (no OpenRocket):
  what `java.lang.Character` and `java.lang.String` of JDK 17 (Unicode 13.0) say about the chars the
  parser looks at. Java works on UTF-16 chars, so every table is of the Basic Multilingual Plane:
  - `kJavaDigitRuns`: the 550 chars whose `Character.getNumericValue(char)` is 0 to 9, in 73 runs, which
    `ExpressionParser.modify()` replaces with ASCII digits (the digits of every script, superscript and
    subscript digits, circled and Roman numerals up to nine, ...);
  - `kJavaWhitespace`: the chars of `Character.isWhitespace(char)`, which exp4j's `InfixTranslator`
    removes;
  - `kJavaUpperCaseToAscii` and `kJavaLowerCaseToAscii`: the chars beyond ASCII whose
    `String.toUpperCase()` / `toLowerCase()` in en_US is ASCII (in upper case the sharp s, the dotless
    i, the long s and the seven Latin ligatures U+FB00 to U+FB06; in lower case the Kelvin sign), which
    decide how exp4j compares function names and checks a variable's name.

  It fails when the JDK contradicts what the C++ code takes for granted: that every char of
  `Character.isDigit(char)` has a numeric value of 0 to 9 (so the ASCII digits are the only digits left
  after `modify()`), and which of `java.util.regex`'s line terminators are whitespace. Letters are not
  tabulated here: the parser uses `Strings::javaIsLetterOrDigit()`, JDK 17's table of `util/Strings`.
- `ExpressionParserDumper.java` writes `tests/core/util/ExpressionParserParityCases.h` from OpenRocket's
  own `ExpressionParser`, run in en_US (it takes its decimal separator and minus sign from the default
  locale):
  - `kExpressionParityCases`: OpenRocket's result for every input of `parity-inputs.txt` (the M2 bindings
    scout's 83, the rest of OpenRocket's `ExpressionParserTest`, the quirks, every function, long
    numbers and texts, deep brackets), as raw double bits or invalid;
  - `kSingleCharacterDigits`: every char of the Basic Multilingual Plane (surrogates excluded) that
    OpenRocket's parser takes alone, with its value. It ends up equal to `kJavaDigitRuns`, but by way of
    the parser rather than `Character`.

  `tests/core/util/ExpressionParserTests.cpp` compares QtRocket's parser with both, exactly where
  OpenRocket's value comes from `+ - * / %`, the parsing of numbers, `abs`, `ceil`, `floor`, `round` and
  `sqrt`, and within a tolerance where `^` (`Math.pow`) or a transcendental function is involved
  (`CLAUDE.md`, Portability).
- `parity-inputs.txt`: the inputs, one per line as `<comparison> [<input>]`, the input between the first
  `[` and the last `]` (so leading and trailing spaces survive editors). `<comparison>` is `=` (exactly;
  a NaN as any NaN), `~` (within a tolerance) or `?` (only whether it parses: `random()`). The input is
  printable ASCII with the escapes `\t`, `\n`, `\r`, `\\` and `\uXXXX` (a UTF-16 code unit; a character
  beyond U+FFFF is written as its surrogate pair). Lines starting with `#` are comments. The dumper
  refuses an input that is there twice or a lone surrogate (which UTF-8 cannot carry).
- `logback.xml`: turns off `ExpressionParser`'s log, which warns about every invalid input (the dumper
  asks for over 60,000 of them on purpose), and keeps the rest of OpenRocket's log to its warnings and
  errors.
- `generate.sh`: compiles and runs both and writes the two files, formatted with the repository's
  `.clang-format`.

## Regenerating

With Java 17 (the character data is JDK 17's, Unicode 13.0) and, for the parity data, the OpenRocket
checkout at the commit of `tests/data/goldens/manifest.json`:

```sh
tools/openrocket-expression-parser/generate.sh            # both files
tools/openrocket-expression-parser/generate.sh tables     # src/util/ExpressionParserTables.h only (JDK only)
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-expression-parser/generate.sh parity
```

The head of `generate.sh` lists the environment variables (`JAVA`, `JAVAC`, `CLANG_FORMAT`,
`OPENROCKET_DIR`, `OPENROCKET_CLASSPATH`). Without `OPENROCKET_CLASSPATH`, the parity part builds
OpenRocket's core in the checkout with OpenRocket's Gradle wrapper and asks Gradle for its class path, as
`tools/openrocket-test-documents/generate.sh` does; the dumper needs only the core's classes and
resources, `slf4j-api` and `logback-classic`/`logback-core`. The committed files were written with
Temurin 17.0.20.1 and `OPENROCKET_CLASSPATH` set to the compiled classes and resources of a core built
before (commit 5f164fd0e) and the jars of `slf4j-api` 2.0.17 and logback 1.5.20 from Gradle's cache; the
Gradle fallback has not been run.

The output is deterministic: a regeneration with another update of Java 17 changes only the Java version
in the first comment of each file. `random()` inputs (`?`) store no value.
