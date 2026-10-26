import java.nio.charset.StandardCharsets;
import java.text.Collator;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Prints tests/core/util/JavaCollatorPins.h: what JDK 17's Collator.getInstance() in en_US (a
 * RuleBasedCollator, NO_DECOMPOSITION) answers, at PRIMARY, SECONDARY and TERTIARY strength, for
 * - kJavaCollatorCases: the pairs of texts of CASES below (the scouts' findings and the corners of
 *   the rules), each with a note;
 * - a corpus of random pairs that tests/core/util/JavaCollatorTests.cpp makes again from the seed:
 *   the texts are drawn from ALPHABET by SplitMix64, and three pairs in four are a text and a copy
 *   with one to three characters replaced, inserted or removed, so that most pairs share a long
 *   prefix and the secondary and tertiary rules decide. Only the answers are stored, one character
 *   a pair (kJavaCollatorResultCodes).
 * Needs nothing but the JDK. See README.md. "raw <pairs>" as the arguments prints only the
 * answers of that many pairs, one character each, for a larger check by hand.
 */
public class JavaCollatorPins {

    static final long SEED = 0x51A7E5C011A70EL;
    static final int PAIRS = 30000;
    static final int MAX_LENGTH = 8;

    /**
     * The characters of the random texts: ignorable ones, spaces and accents (primary-ignorable,
     * secondary only), hyphens (secondary and tertiary), the contraction U+0308 U+0301, punctuation
     * and symbols, digits, letters in both cases, precomposed and expanding letters, characters
     * the rules do not map (superscripts, Greek, Cyrillic, CJK, Thai, U+FFFD, U+FFFF) and code
     * points above U+FFFF (unmapped, and plane-4 and plane-16 repeats of mapped chars).
     */
    static final int[] ALPHABET = {
        0x0000, 0x0001, 0x001F, 0x007F, 0x200B, 0x0009, 0x000D, 0x0020, 0x00A0, 0x3000, 0xFEFF, 0x0300,
        0x0301, 0x0302, 0x0308, 0x0327, 0x0344, 0x20D0, 0x002D, 0x00AD, 0x2010, 0x2212, 0x005F, 0x002C,
        0x002E, 0x002F, 0x0028, 0x0029, 0x0022, 0x0027, 0x0026, 0x0023, 0x0025, 0x002B, 0x003C, 0x003D,
        0x007C, 0x0024, 0x00B0, 0x00B5, 0x00A3, 0x20AC, 0x0030, 0x0031, 0x0032, 0x0038, 0x0039, 0x00BC,
        0x0061, 0x0041, 0x0062, 0x0042, 0x0065, 0x0045, 0x0067, 0x0047, 0x0069, 0x0049, 0x006C, 0x004C,
        0x0073, 0x0053, 0x0074, 0x0054, 0x0078, 0x0058, 0x007A, 0x005A, 0x00E0, 0x00C0, 0x00E9, 0x00C9,
        0x00C5, 0x212B, 0x00F1, 0x01D7, 0x01E2, 0x01FC, 0x1E00, 0x0385, 0x00E6, 0x00C6, 0x00DF, 0x00FE,
        0x00DE, 0x0153, 0x0152, 0x00B2, 0x00B3, 0x03B1, 0x0391, 0x0430, 0x4E00, 0x0E40, 0xFFFD, 0xFFFF,
        0x1F600, 0x10000, 0x40041, 0x40308, 0x40301, 0x100020,
    };

    /** The pairs of kJavaCollatorCases: text, text, note. */
    static final String[][] CASES = {
        {"Pro-98 6g XL", "Pro98-6GXL",
            "COLLATORTIE: the motor database's one pair of case texts equal at PRIMARY"},
        {"Estes CYL-Ogive", "Estes-CYL-Ogive", "nose cone descriptions (228 pairs equal at PRIMARY)"},
        {"Balsa, bulk, 7lb/ft3 (0.112 g/cm\u00B3)", "Balsa, bulk, 7 lb/ft3 (0.112 g/cm\u00B3)",
            "nose cone materials (514 pairs)"},
        {"Balsa Transition BT-2.5 to BT-3 (Reversed)", "Balsa Transition BT-2.5 to BT-3(Reversed)",
            "a transition description"},
        {"1.283x.736x.125thk lite-ply", "1.283x.736x.125thk lite ply", "centering ring descriptions"},
        {"3/16 in x 2in Launch lug", "3/16in x 2in Launch Lug", "launch lug descriptions"},
        {"18 in. Nylon Chute", "18 In. Nylon Chute", "parachute descriptions"},
        {"Ripstop nylon, 1.9 oz 5 mil (58.9 g/m\u00B2)", "Ripstop Nylon, 1.9 oz 5 mil (58.9 g/m\u00B2)",
            "parachute materials (504 pairs)"},
        {"a", "A", "lower case first"},
        {"A", "b", "case only at the tertiary level"},
        {"ab", "Ab", ""},
        {"aB", "Ab", "the first tertiary difference counts"},
        {"a", "\u00E1", "an accent at the secondary level"},
        {"\u00E1", "a\u0301", "a precomposed letter equals its decomposition"},
        {"\u00C1", "a\u0301", ""},
        {"\u00E1b", "aB", "a secondary difference beats an earlier tertiary one"},
        {"Ab", "a\u0301b", "the secondary difference of the accent beats the tertiary one"},
        {"cote", "c\u00F4te", "the classic French cases (no French secondary order in en_US)"},
        {"c\u00F4te", "cot\u00E9", ""},
        {"cot\u00E9", "c\u00F4t\u00E9", ""},
        {"ab", "a b", "a space facing a letter"},
        {"a b", "a-b", "a space against a hyphen"},
        {"ab", "a-b", ""},
        {"a b", "ab ", ""},
        {"a\u0001b", "ab", "an ignorable character"},
        {"\u0001", " ", "an ignorable character against a space"},
        {"\u0001a", " a", ""},
        {"", " ", "a text of one space"},
        {"", "-", ""},
        {"", "\u0001", ""},
        {"-", " ", ""},
        {"a", "a\u0301", "the accent left over"},
        {"a\u0301", "a", ""},
        {"a", "a-", "a hyphen left over"},
        {"a", "a ", "a space left over"},
        {"A", "a ", "a space left over against a tertiary difference"},
        {"a ", "A", ""},
        {"a", "a\u0001", "an ignorable character left over"},
        {"\u00C6ther", "aether", "an expanding letter"},
        {"\u00E6", "ae", ""},
        {"stra\u00DFe", "strasse", ""},
        {"\u00FE", "th", ""},
        {"\u01D7", "U\u0308\u0301", "the contraction U+0308 U+0301"},
        {"\u0344", "\u0308\u0301", ""},
        {"\u03B1", "\u0391", "Greek is not in the rules"},
        {"z", "\u03B1", ""},
        {"\u4E00", "\uD83D\uDE00", "CJK against an emoji"},
        {"\uFFFF", "\uD800\uDC00", "U+FFFF against U+10000"},
        {"\uD8C0\uDC41", "A", "U+40041 repeats A"},
        {"\uD8C0\uDF08\u0301", "\u0308\u0301", "no contraction for the repeat of U+0308"},
        {"AeroTech", "Apogee", "manufacturers"},
        {"Kosdon by AeroTech", "Kosdon", ""},
        {"LOC/Precision", "Loki Research", ""},
        {"Public Missiles, Ltd.", "Propulsion Polymers", ""},
        {"b2 Rocketry Company", "BalsaMachining", ""},
        {"SEMROC", "SEMROC Astronautics", ""},
        {"MPC", "Madcow", ""},
    };

    private static long state;

    private static long nextLong() {
        long z = (state += 0x9E3779B97F4A7C15L);
        z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
        z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
        return z ^ (z >>> 31);
    }

    private static int below(int n) {
        return (int) ((nextLong() >>> 1) % n);
    }

    private static int randomCharacter() {
        return ALPHABET[below(ALPHABET.length)];
    }

    private static List<Integer> randomText() {
        int length = below(MAX_LENGTH + 1);
        List<Integer> text = new ArrayList<>();
        for (int i = 0; i < length; i++) {
            text.add(randomCharacter());
        }
        return text;
    }

    private static List<Integer> edited(List<Integer> original) {
        List<Integer> text = new ArrayList<>(original);
        int edits = 1 + below(3);
        for (int i = 0; i < edits; i++) {
            int kind = below(3);
            if (kind == 0 && !text.isEmpty()) {
                int at = below(text.size());
                int c = randomCharacter();
                text.set(at, c);
            } else if (kind == 1) {
                int at = below(text.size() + 1);
                int c = randomCharacter();
                text.add(at, c);
            } else if (kind == 2 && !text.isEmpty()) {
                text.remove(below(text.size()));
            }
        }
        return text;
    }

    private static String string(List<Integer> codePoints) {
        StringBuilder b = new StringBuilder();
        for (int c : codePoints) {
            b.appendCodePoint(c);
        }
        return b.toString();
    }

    private static final String CODES = "0123456789ABCDEFGHIJKLMNOPQ";

    private static final Collator[] COLLATORS = new Collator[3];

    /** The answers at the three strengths as one character of CODES. */
    private static char code(String a, String b) {
        int code = 0;
        for (Collator collator : COLLATORS) {
            code = 3 * code + collator.compare(a, b) + 1;
        }
        return CODES.charAt(code);
    }

    private static void fail(String message) {
        System.err.println("error: " + message);
        System.exit(1);
    }

    /** text as a C++ string literal of UTF-8 (a std::string_view with its length if it holds a NUL). */
    private static String literal(String text) {
        StringBuilder b = new StringBuilder("\"");
        boolean afterHexEscape = false;
        for (int i = 0; i < text.length(); i = text.offsetByCodePoints(i, 1)) {
            int c = text.codePointAt(i);
            boolean hexDigit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (afterHexEscape && hexDigit) {
                b.append("\" \"");
            }
            afterHexEscape = false;
            if (c == '"' || c == '\\') {
                b.append('\\').append((char) c);
            } else if (c >= 0x20 && c < 0x7F) {
                b.append((char) c);
            } else if (c < 0x20 || c == 0x7F) {
                b.append(String.format("\\x%02X", c));
                afterHexEscape = true;
            } else if (c >= 0x80 && c < 0xA0) {
                fail("no C1 control in a case: U+" + Integer.toHexString(c));
            } else if (c < 0x10000) {
                b.append(String.format("\\u%04X", c));
            } else {
                b.append(String.format("\\U%08X", c));
            }
        }
        b.append('"');
        if (text.indexOf('\0') >= 0) {
            return "std::string_view(" + b + ", " + text.getBytes(StandardCharsets.UTF_8).length + ")";
        }
        return b.toString();
    }

    public static void main(String[] args) {
        if (!System.getProperty("java.specification.version").equals("17")) {
            fail("the pins are of Java 17; this is Java " + System.getProperty("java.version"));
        }
        if (!Locale.getDefault().equals(Locale.US)) {
            fail("the default locale must be en_US; it is " + Locale.getDefault());
        }
        int[] strengths = {Collator.PRIMARY, Collator.SECONDARY, Collator.TERTIARY};
        for (int i = 0; i < 3; i++) {
            COLLATORS[i] = Collator.getInstance();
            if (COLLATORS[i].getDecomposition() != Collator.NO_DECOMPOSITION) {
                fail("Collator.getInstance() decomposes");
            }
            COLLATORS[i].setStrength(strengths[i]);
        }
        boolean raw = args.length == 2 && args[0].equals("raw");
        int pairs = raw ? Integer.parseInt(args[1]) : PAIRS;

        state = SEED;
        StringBuilder results = new StringBuilder();
        for (int i = 0; i < pairs; i++) {
            List<Integer> a = randomText();
            List<Integer> b = below(4) == 0 ? randomText() : edited(a);
            results.append(code(string(a), string(b)));
        }
        if (raw) {
            System.out.println(results);
            return;
        }

        StringBuilder out = new StringBuilder();
        out.append("#pragma once\n\n");
        out.append("// GENERATED by tools/openrocket-collation/JavaCollatorPins.java (Java ")
                .append(System.getProperty("java.version"))
                .append("): do not edit by hand; README.md there says how to regenerate it. What JDK 17's"
                        + " Collator.getInstance() in en_US answers at PRIMARY, SECONDARY and TERTIARY strength,"
                        + " for JavaCollatorTests.cpp. Test-only.\n\n");
        out.append("#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n");
        out.append("namespace QtRocket::Test\n{\n\n");
        out.append("/// Two texts and Java's compare(first, second) at PRIMARY, SECONDARY and TERTIARY strength.\n");
        out.append("struct JavaCollatorCase\n{\n    std::string_view first;\n    std::string_view second;\n"
                + "    int              primary;\n    int              secondary;\n    int              tertiary;\n"
                + "};\n\n");
        out.append("// clang-format off\n");
        out.append("// NOLINTBEGIN(modernize-use-designated-initializers): a generated table, one entry per row\n");
        out.append("inline constexpr std::array<JavaCollatorCase, ").append(CASES.length)
                .append("> kJavaCollatorCases{{\n");
        for (String[] c : CASES) {
            if (!c[2].isEmpty()) {
                out.append("    // ").append(c[2]).append("\n");
            }
            out.append("    {").append(literal(c[0])).append(", ").append(literal(c[1]));
            for (Collator collator : COLLATORS) {
                out.append(", ").append(collator.compare(c[0], c[1]));
            }
            out.append("},\n");
        }
        out.append("}};\n// NOLINTEND(modernize-use-designated-initializers)\n// clang-format on\n\n");

        out.append("/// The seed of the corpus's SplitMix64.\n");
        out.append("inline constexpr std::uint64_t kJavaCollatorCorpusSeed = ")
                .append(String.format("0x%XU", SEED)).append(";\n\n");
        out.append("/// The most characters of a random text.\n");
        out.append("inline constexpr std::size_t kJavaCollatorCorpusMaxLength = ").append(MAX_LENGTH).append(";\n\n");
        out.append("/// The characters the random texts are drawn from.\n");
        out.append("// clang-format off\n");
        out.append("inline constexpr std::array<char32_t, ").append(ALPHABET.length)
                .append("> kJavaCollatorCorpusAlphabet{{\n");
        for (int i = 0; i < ALPHABET.length; i++) {
            out.append(i % 8 == 0 ? "    " : " ").append(String.format("0x%04X", ALPHABET[i])).append(",");
            if (i % 8 == 7 || i == ALPHABET.length - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n// clang-format on\n\n");
        out.append("/// A pair's answers as one character: its index in this text is 9 * (primary + 1) + 3 *\n"
                + "/// (secondary + 1) + (tertiary + 1).\n");
        out.append("inline constexpr std::string_view kJavaCollatorResultCodes = \"").append(CODES).append("\";\n\n");
        out.append("/// The answers for the ").append(pairs).append(" pairs of the corpus, in order.\n");
        out.append("// clang-format off\n");
        int lines = (results.length() + 79) / 80;
        out.append("inline constexpr std::array<std::string_view, ").append(lines)
                .append("> kJavaCollatorCorpusResults{{\n");
        for (int i = 0; i < results.length(); i += 80) {
            out.append("    \"").append(results, i, Math.min(results.length(), i + 80)).append("\",\n");
        }
        out.append("}};\n// clang-format on\n\n");
        out.append("}  // namespace QtRocket::Test\n");
        System.out.print(out);
    }
}
