import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Prints src/util/ExpressionParserTables.h: the facts of java.lang.Character and java.lang.String
 * that OpenRocket's ExpressionParser and its exp4j copy rely on, as JDK 17 (Unicode 13.0) has them,
 * for QtRocket's util/ExpressionParser. Java works on UTF-16 chars, so every table is of the Basic
 * Multilingual Plane (a char), surrogates excluded. Needs nothing but the JDK. See README.md.
 *
 * The tables:
 * - kJavaDigitRuns: the chars whose Character.getNumericValue(char) is 0 to 9, which
 *   ExpressionParser.modify() replaces with the ASCII digit of that value, as runs in which the
 *   char and its value both rise by one.
 * - kJavaWhitespace: the chars of Character.isWhitespace(char), which InfixTranslator removes.
 * - kJavaUpperCaseToAscii: the chars beyond ASCII that can be part of a name of the Tokenizer
 *   (Character.isLetter, Character.isDigit, '_' or '$') and whose String.toUpperCase() in en_US is
 *   ASCII: they spell a function name of FunctionToken.Function as ASCII letters do.
 * - kJavaLowerCaseToAscii: the chars beyond ASCII whose String.toLowerCase() in en_US is ASCII,
 *   for the Tokenizer's check of a variable name against its function names.
 *
 * It also checks what the C++ code takes for granted and fails when the JDK says otherwise: every
 * char of Character.isDigit(char) has a numeric value of 0 to 9 (so that, once modify() has
 * replaced those, the ASCII digits are the only digits left), and of the line terminators of
 * java.util.regex (\n, \r, U+0085, U+2028, U+2029) all but U+0085 are Character.isWhitespace.
 * String.toUpperCase() and toLowerCase() map a String char by char in en_US (the conditional
 * special casing applies to the Lithuanian, Turkish and Azeri locales and to the final sigma, none
 * of which gives ASCII), so the mappings of single chars decide how a name compares.
 */
public class JavaCharacterTables {

    /** The tables of structs are written row by row, not with designated initializers. */
    private static final String NOLINT_BEGIN =
            "// NOLINTBEGIN(modernize-use-designated-initializers): a generated table, one entry per row\n";
    private static final String NOLINT_END = "// NOLINTEND(modernize-use-designated-initializers)\n";

    private static boolean isSurrogate(int c) {
        return c >= 0xD800 && c <= 0xDFFF;
    }

    private static boolean isNameChar(char c) {
        return Character.isLetter(c) || Character.isDigit(c) || c == '_' || c == '$';
    }

    private static boolean isAscii(String s) {
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) >= 0x80) {
                return false;
            }
        }
        return true;
    }

    private static void fail(String message) {
        System.err.println("error: " + message);
        System.exit(1);
    }

    private static String hex(int c) {
        return String.format("0x%04X", c);
    }

    public static void main(String[] args) {
        if (!System.getProperty("java.specification.version").equals("17")) {
            fail("the tables are of Java 17; this is Java " + System.getProperty("java.version"));
        }
        Locale locale = Locale.US;

        // Runs of chars whose numeric value is 0..9, the char and its value rising together.
        List<int[]> runs = new ArrayList<>();
        int digitCount = 0;
        for (int c = 0; c <= 0xFFFF; c++) {
            if (isSurrogate(c)) {
                continue;
            }
            int value = Character.getNumericValue((char) c);
            if (Character.isDigit((char) c) && (value < 0 || value > 9)) {
                fail("U+" + hex(c) + " is a digit with the numeric value " + value);
            }
            if (value < 0 || value > 9) {
                continue;
            }
            digitCount++;
            int[] last = runs.isEmpty() ? null : runs.get(runs.size() - 1);
            if (last != null && last[1] == c - 1 && last[2] + (c - last[0]) == value) {
                last[1] = c;
            } else {
                runs.add(new int[] {c, c, value});
            }
        }

        List<Integer> whitespace = new ArrayList<>();
        for (int c = 0; c <= 0xFFFF; c++) {
            if (!isSurrogate(c) && Character.isWhitespace((char) c)) {
                whitespace.add(c);
            }
        }
        // java.util.regex's line terminators, which '.' does not match: \n, \r, U+0085, U+2028 and
        // U+2029. The C++ code knows them; check which of them are whitespace as it expects.
        int[] terminators = {0x000A, 0x000D, 0x0085, 0x2028, 0x2029};
        boolean[] expectedWhitespace = {true, true, false, true, true};
        for (int i = 0; i < terminators.length; i++) {
            if (Character.isWhitespace((char) terminators[i]) != expectedWhitespace[i]) {
                fail("Character.isWhitespace(U+" + hex(terminators[i]) + ") is not "
                        + expectedWhitespace[i]);
            }
        }

        List<String[]> upper = new ArrayList<>();
        List<String[]> lower = new ArrayList<>();
        for (int c = 0x80; c <= 0xFFFF; c++) {
            if (isSurrogate(c)) {
                continue;
            }
            String s = String.valueOf((char) c);
            String u = s.toUpperCase(locale);
            String l = s.toLowerCase(locale);
            if (isNameChar((char) c) && isAscii(u)) {
                upper.add(new String[] {hex(c), u});
            }
            if (isAscii(l)) {
                lower.add(new String[] {hex(c), l});
            }
        }

        StringBuilder out = new StringBuilder();
        out.append("#pragma once\n\n");
        out.append("// GENERATED by tools/openrocket-expression-parser/JavaCharacterTables.java (Java ")
                .append(System.getProperty("java.version")).append(", Unicode 13.0):\n");
        out.append("// do not edit by hand; README.md there says how to regenerate it. What java.lang.Character\n");
        out.append("// and java.lang.String of JDK 17 say about the chars (UTF-16 code units of the Basic\n");
        out.append("// Multilingual Plane, surrogates excluded) that OpenRocket's ExpressionParser and its exp4j\n");
        out.append("// copy look at, for util/ExpressionParser.cpp only. The generator also checked that every\n");
        out.append("// char of Character.isDigit(char) is in kJavaDigitRuns, so that the ASCII digits are the\n");
        out.append("// only digits left once ExpressionParser.modify() has replaced those.\n\n");
        out.append("#include <array>\n#include <cstdint>\n#include <string_view>\n\n");
        out.append("namespace QtRocket::Detail\n{\n\n");

        out.append("/// A run of chars whose Character.getNumericValue(char) is 0 to 9: first has firstValue, and\n");
        out.append("/// the char and its value rise by one together up to last.\n");
        out.append("struct JavaDigitRun\n{\n    char16_t     first;\n    char16_t     last;\n    std::uint8_t firstValue;\n};\n\n");
        out.append("/// The ").append(digitCount).append(" chars of Character.getNumericValue(char) 0 to 9, in ascending order.\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<JavaDigitRun, ").append(runs.size()).append("> kJavaDigitRuns{{\n");
        for (int i = 0; i < runs.size(); i++) {
            int[] r = runs.get(i);
            out.append(i % 3 == 0 ? "    " : " ");
            out.append("{").append(hex(r[0])).append(", ").append(hex(r[1])).append(", ").append(r[2]).append("},");
            if (i % 3 == 2 || i == runs.size() - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");

        out.append("/// The chars of Character.isWhitespace(char), in ascending order.\n");
        out.append("// clang-format off\n");
        out.append("inline constexpr std::array<char16_t, ").append(whitespace.size()).append("> kJavaWhitespace{{\n");
        for (int i = 0; i < whitespace.size(); i++) {
            out.append(i % 8 == 0 ? "    " : " ");
            out.append(hex(whitespace.get(i))).append(",");
            if (i % 8 == 7 || i == whitespace.size() - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n// clang-format on\n\n");

        out.append("/// A char beyond ASCII and the ASCII text a case mapping of String makes of it.\n");
        out.append("struct JavaCaseMapping\n{\n    char16_t         character;\n    std::string_view ascii;\n};\n\n");
        appendMappings(out, "kJavaUpperCaseToAscii",
                "The chars beyond ASCII that can be part of a name (Character.isLetter, Character.isDigit,\n"
                        + "/// '_' or '$') whose String.toUpperCase() in en_US is ASCII, in ascending order.",
                upper);
        appendMappings(out, "kJavaLowerCaseToAscii",
                "The chars beyond ASCII whose String.toLowerCase() in en_US is ASCII, in ascending order.",
                lower);
        out.append("}  // namespace QtRocket::Detail\n");
        System.out.print(out);
    }

    private static void appendMappings(StringBuilder out, String name, String comment, List<String[]> mappings) {
        out.append("/// ").append(comment).append("\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<JavaCaseMapping, ").append(mappings.size()).append("> ")
                .append(name).append("{{\n");
        for (String[] m : mappings) {
            out.append("    {").append(m[0]).append(", \"").append(m[1]).append("\"},\n");
        }
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");
    }
}
