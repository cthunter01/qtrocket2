import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import info.openrocket.core.util.ExpressionParser;

/**
 * Prints tests/core/util/ExpressionParserParityCases.h: what OpenRocket's own ExpressionParser
 * (info.openrocket.core.util.ExpressionParser, with its exp4j copy) makes of every input of
 * parity-inputs.txt, and of every char of the Basic Multilingual Plane alone (surrogates excluded),
 * for the parity tests of QtRocket's util/ExpressionParser. Run it in en_US: the parser takes its
 * decimal separator and minus sign from the default locale. See README.md.
 *
 * Usage: java ExpressionParserDumper <parity-inputs.txt>
 */
public class ExpressionParserDumper {

    private static final ExpressionParser PARSER = new ExpressionParser();

    /** The tables are written row by row, not with designated initializers. */
    private static final String NOLINT_BEGIN =
            "// NOLINTBEGIN(modernize-use-designated-initializers): a generated table, one entry per row\n";
    private static final String NOLINT_END = "// NOLINTEND(modernize-use-designated-initializers)\n";

    private static void fail(String message) {
        System.err.println("error: " + message);
        System.exit(1);
    }

    /** The input of a line: \t, \n, \r, \\ and \\uXXXX unescaped; anything else must be printable ASCII. */
    private static String unescape(String text, int lineNumber) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c < 0x20 || c >= 0x7F) {
                fail("line " + lineNumber + ": write U+" + String.format("%04X", (int) c) + " as an escape");
            }
            if (c != '\\') {
                out.append(c);
                continue;
            }
            if (i + 1 >= text.length()) {
                fail("line " + lineNumber + ": a lone backslash");
            }
            char e = text.charAt(++i);
            switch (e) {
                case 't' -> out.append('\t');
                case 'n' -> out.append('\n');
                case 'r' -> out.append('\r');
                case '\\' -> out.append('\\');
                case 'u' -> {
                    if (i + 4 >= text.length()) {
                        fail("line " + lineNumber + ": a short \\u escape");
                    }
                    out.append((char) Integer.parseInt(text.substring(i + 1, i + 5), 16));
                    i += 4;
                }
                default -> fail("line " + lineNumber + ": unknown escape \\" + e);
            }
        }
        String s = out.toString();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (Character.isHighSurrogate(c) && i + 1 < s.length() && Character.isLowSurrogate(s.charAt(i + 1))) {
                i++;
            } else if (Character.isSurrogate(c)) {
                fail("line " + lineNumber + ": a lone surrogate has no UTF-8 encoding");
            }
        }
        return s;
    }

    /**
     * A C++ string literal of the UTF-8 encoding of s: printable ASCII as itself, the rest in octal
     * (a text of printable ASCII alone as cppAsciiLiteral() writes it). A digit right after an octal
     * escape is an octal escape too: the escape is complete after three digits, but MSVC warns
     * (C4125, an error with /WX) that a decimal digit ends it.
     */
    private static String cppUtf8Literal(String s) {
        if (s.chars().allMatch(c -> c >= 0x20 && c < 0x7F)) {
            return cppAsciiLiteral(s);
        }
        StringBuilder out = new StringBuilder("\"");
        boolean afterOctal = false;
        for (byte b : s.getBytes(StandardCharsets.UTF_8)) {
            int v = b & 0xFF;
            if (v == '"' || v == '\\') {
                out.append('\\').append((char) v);
                afterOctal = false;
            } else if (v >= 0x20 && v < 0x7F && !(afterOctal && v >= '0' && v <= '9')) {
                out.append((char) v);
                afterOctal = false;
            } else {
                out.append(String.format("\\%03o", v));
                afterOctal = true;
            }
        }
        return out.append('"').toString();
    }

    /**
     * A C++ string literal of printable ASCII text: a raw string literal when it holds a backslash
     * or a quote (clang-tidy's modernize-raw-string-literal), a plain one otherwise.
     */
    private static String cppAsciiLiteral(String s) {
        if (s.indexOf('\\') < 0 && s.indexOf('"') < 0) {
            return "\"" + s + "\"";
        }
        String delimiter = "";
        while (s.contains(")" + delimiter + "\"")) {
            delimiter += "x";
        }
        return "R\"" + delimiter + "(" + s + ")" + delimiter + "\"";
    }

    private static Double parse(String s) {
        try {
            return PARSER.parse(s);
        } catch (Exception e) {
            return null;
        }
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            fail("usage: ExpressionParserDumper <parity-inputs.txt>");
        }
        if (!java.util.Locale.getDefault().equals(java.util.Locale.US)) {
            fail("run in en_US (-Duser.language=en -Duser.country=US), not " + java.util.Locale.getDefault());
        }
        List<String> lines = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8);
        StringBuilder cases = new StringBuilder();
        Set<String> seen = new HashSet<>();
        int count = 0;
        for (int n = 0; n < lines.size(); n++) {
            String line = lines.get(n);
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            int open = line.indexOf('[');
            int close = line.lastIndexOf(']');
            if (line.length() < 4 || line.charAt(1) != ' ' || open != 2 || close != line.length() - 1) {
                fail("line " + (n + 1) + ": not '<comparison> [<input>]'");
            }
            String comparison = switch (line.charAt(0)) {
                case '=' -> "EXACT";
                case '~' -> "TOLERANCE";
                case '?' -> "RANDOM";
                default -> null;
            };
            if (comparison == null) {
                fail("line " + (n + 1) + ": the comparison is one of = ~ ?");
            }
            String escaped = line.substring(open + 1, close);
            String input = unescape(escaped, n + 1);
            if (!seen.add(input)) {
                fail("line " + (n + 1) + ": the input [" + escaped + "] is there twice");
            }
            Double value = parse(input);
            String result;
            if (value == null) {
                result = "false, 0x0ULL";
            } else if (comparison.equals("RANDOM")) {
                result = "true, 0x0ULL";
            } else {
                result = "true, " + String.format("0x%016XULL", Double.doubleToRawLongBits(value));
            }
            cases.append("    {").append(cppUtf8Literal(input)).append(", ").append(cppAsciiLiteral(escaped))
                    .append(", ParityComparison::").append(comparison).append(", ").append(result).append("},\n");
            count++;
        }

        // Every char alone: the runs of those that parse, each to the value of a digit.
        List<int[]> runs = new ArrayList<>();
        for (int c = 0; c <= 0xFFFF; c++) {
            if (Character.isSurrogate((char) c)) {
                continue;
            }
            Double value = parse(String.valueOf((char) c));
            if (value == null) {
                continue;
            }
            double v = value;
            if (v != Math.rint(v) || v < 0 || v > 9) {
                fail("U+" + String.format("%04X", c) + " alone parses to " + v + ", not to a digit");
            }
            int digit = (int) v;
            int[] last = runs.isEmpty() ? null : runs.get(runs.size() - 1);
            if (last != null && last[1] == c - 1 && last[2] + (c - last[0]) == digit) {
                last[1] = c;
            } else {
                runs.add(new int[] {c, c, digit});
            }
        }

        StringBuilder out = new StringBuilder();
        out.append("#pragma once\n\n");
        out.append("// GENERATED by tools/openrocket-expression-parser/ExpressionParserDumper.java from\n");
        out.append("// tools/openrocket-expression-parser/parity-inputs.txt: do not edit by hand (README.md there says\n");
        out.append("// how to regenerate it). What OpenRocket's ExpressionParser.parse() (commit of\n");
        out.append("// tests/data/goldens/manifest.json, Java ").append(System.getProperty("java.version"))
                .append(", en_US) gives for each input, and\n");
        out.append("// which chars of the Basic Multilingual Plane it takes alone. Test-only.\n\n");
        out.append("#include <array>\n#include <cstdint>\n#include <string_view>\n\n");
        out.append("namespace QtRocket::Test\n{\n\n");
        out.append("/// How a parity case compares QtRocket's result with OpenRocket's.\n");
        out.append("enum class ParityComparison\n{\n");
        out.append("    EXACT,      ///< bit for bit, a NaN as any NaN\n");
        out.append("    TOLERANCE,  ///< within a tolerance: Math.pow or a transcendental function is involved\n");
        out.append("    RANDOM,     ///< only whether the input parses: random()\n");
        out.append("};\n\n");
        out.append("/// An input and what OpenRocket makes of it.\n");
        out.append("struct ExpressionParityCase\n{\n");
        out.append("    /// The input, UTF-8.\n    std::string_view input;\n");
        out.append("    /// The input as parity-inputs.txt writes it (escapes for anything but printable ASCII).\n");
        out.append("    std::string_view escaped;\n");
        out.append("    ParityComparison comparison;\n");
        out.append("    /// False where OpenRocket throws InvalidExpressionException.\n    bool valid;\n");
        out.append("    /// Double.doubleToRawLongBits() of OpenRocket's result (0 when invalid or RANDOM).\n");
        out.append("    std::uint64_t bits;\n};\n\n");
        out.append("/// The inputs of parity-inputs.txt, in its order.\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<ExpressionParityCase, ").append(count)
                .append("> kExpressionParityCases{{\n");
        out.append(cases);
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");
        out.append("/// A run of chars that OpenRocket parses alone: first gives firstValue, and the char and its\n");
        out.append("/// value rise by one together up to last.\n");
        out.append("struct SingleCharacterDigits\n{\n    char16_t     first;\n    char16_t     last;\n");
        out.append("    std::uint8_t firstValue;\n};\n\n");
        out.append("/// Every char of the Basic Multilingual Plane (surrogates excluded) that parses alone, in\n");
        out.append("/// ascending order; every other char alone is invalid.\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<SingleCharacterDigits, ").append(runs.size())
                .append("> kSingleCharacterDigits{{\n");
        for (int i = 0; i < runs.size(); i++) {
            int[] r = runs.get(i);
            out.append(i % 3 == 0 ? "    " : " ");
            out.append(String.format("{0x%04X, 0x%04X, %d},", r[0], r[1], r[2]));
            if (i % 3 == 2 || i == runs.size() - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");
        out.append("}  // namespace QtRocket::Test\n");
        System.out.print(out);
    }
}
