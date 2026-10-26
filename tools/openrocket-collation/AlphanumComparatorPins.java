import info.openrocket.core.util.AlphanumComparator;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Prints tests/core/util/AlphanumComparatorPins.h: what OpenRocket's util/AlphanumComparator
 * (compiled from the checkout by generate.sh; the argument is the checkout's commit) answers, its
 * int as it is, in en_US (its collator is Collator.getInstance() at TERTIARY strength with
 * CANONICAL_DECOMPOSITION), for
 * - kAlphanumCases: the pairs of texts of CASES below, each with a note;
 * - kAlphanumDeviationCases: texts that NFD changes, on which QtRocket's comparator (no NFD)
 *   differs, with Java's answer;
 * - a corpus of random pairs that tests/core/util/AlphanumComparatorTests.cpp makes again from the
 *   seed, as tools/openrocket-collation/JavaCollatorPins.java makes its own (SplitMix64; three
 *   pairs in four a text and an edited copy), the texts drawn from ALPHABET, which NFD leaves as
 *   they are (checked: every character and every pair of them), so that QtRocket's comparator must
 *   give Java's answers. Only the answers are stored, one character a pair.
 * See README.md.
 */
public class AlphanumComparatorPins {

    static final long SEED = 0xA1FA_2C0A_9A7EL;
    static final int PAIRS = 20000;
    static final int MAX_LENGTH = 10;

    /** Digits (each twice, so that numbers are frequent), part-number letters and punctuation, and a few others. */
    static final int[] ALPHABET = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'A', 'b', 'B', 'T', 't', 'x', 'X', ' ', '-', '.', '/', '"', '(', ')', ',', 0x0001, 0x00E6,
        0x00DF, 0x00B5, 0x00B0, 0x03B1, 0x1F600,
    };

    /** The pairs of kAlphanumCases: text, text, note. */
    static final String[][] CASES = {
        {"file1", "file2", "AlphanumComparatorTest"},
        {"file2", "file10", ""},
        {"img2.png", "img12.png", ""},
        {"section9b", "section10a", ""},
        {"chapter10", "chapter10a", ""},
        {"alpha", "beta", ""},
        {"gamma", "gamma", ""},
        {"item2", "item002", ""},
        {"item002", "item2", ""},
        {"item01", "item002", ""},
        {"BT-5", "BT-20", "part numbers"},
        {"BT-20", "BT-20J", ""},
        {"BT-20", "bt-20", "case is the collator's tertiary difference"},
        {"PNC-20A", "PNC-20", ""},
        {"BT-55", "BT-5.5", ""},
        {"1.1\" (29)", "1.10\" (29)", ""},
        {"10", "9", "numbers by length, then digit by digit"},
        {"007", "7", ""},
        {"19", "21", ""},
        {"x1y", "x01y", ""},
        {"a1", "a 1", "a space is part of the letter chunk"},
        {"a-1", "a1", ""},
        {"", "", "empty texts"},
        {"", "a", ""},
        {"1", "", ""},
        {"a", "1", "a letter chunk against a digit chunk: the collator"},
        {"\uD83D\uDE00", "a", "a code point above U+FFFF counts two UTF-16 chars"},
        {"a\uD83D\uDE00", "a\uD83D\uDE001", ""},
        {"ab\u0001", "ab", "an ignorable character still counts in the length"},
        {"\u00E6", "ae", "an expanding letter"},
    };

    /** Texts that NFD changes: kAlphanumDeviationCases. */
    static final String[][] DEVIATION_CASES = {
        {"\u03AC", "\u03B1", "Greek alpha with tonos decomposes to alpha and an accent"},
        {"\u01D7", "U\u0308\u0301", "NFD makes the contraction U+0308 U+0301 of both"},
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

    private static String string(int... codePoints) {
        StringBuilder b = new StringBuilder();
        for (int c : codePoints) {
            b.appendCodePoint(c);
        }
        return b.toString();
    }

    private static String string(List<Integer> codePoints) {
        StringBuilder b = new StringBuilder();
        for (int c : codePoints) {
            b.appendCodePoint(c);
        }
        return b.toString();
    }

    /** One character a pair: the answer plus 32 as an index into this text. */
    private static final String CODES = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-";
    private static final int CODE_OFFSET = 32;

    private static void fail(String message) {
        System.err.println("error: " + message);
        System.exit(1);
    }

    /** text as a C++ string literal of UTF-8 (no NUL, no C1 control). */
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
            } else if (c == 0 || (c >= 0x80 && c < 0xA0)) {
                fail("no NUL or C1 control in a case");
            } else if (c < 0x20 || c == 0x7F) {
                b.append(String.format("\\x%02X", c));
                afterHexEscape = true;
            } else if (c < 0x10000) {
                b.append(String.format("\\u%04X", c));
            } else {
                b.append(String.format("\\U%08X", c));
            }
        }
        return b.append('"').toString();
    }

    private static boolean isNfd(String s) {
        return Normalizer.normalize(s, Normalizer.Form.NFD).equals(s);
    }

    public static void main(String[] args) {
        if (!System.getProperty("java.specification.version").equals("17")) {
            fail("the pins are of Java 17; this is Java " + System.getProperty("java.version"));
        }
        if (!Locale.getDefault().equals(Locale.US)) {
            fail("the default locale must be en_US; it is " + Locale.getDefault());
        }
        if (args.length != 1) {
            fail("usage: AlphanumComparatorPins <OpenRocket commit>");
        }
        for (int c : ALPHABET) {
            for (int d : ALPHABET) {
                if (!isNfd(string(c, d))) {
                    fail("NFD changes " + string(c, d));
                }
            }
        }
        for (String[] c : CASES) {
            if (!isNfd(c[0]) || !isNfd(c[1])) {
                fail("NFD changes a case: " + c[0] + " / " + c[1]);
            }
        }
        for (String[] c : DEVIATION_CASES) {
            if (isNfd(c[0]) && isNfd(c[1])) {
                fail("NFD changes neither text of a deviation case: " + c[0] + " / " + c[1]);
            }
        }
        AlphanumComparator comparator = new AlphanumComparator();

        state = SEED;
        StringBuilder results = new StringBuilder();
        for (int i = 0; i < PAIRS; i++) {
            List<Integer> a = randomText();
            List<Integer> b = below(4) == 0 ? randomText() : edited(a);
            int answer = comparator.compare(string(a), string(b));
            if (answer < -CODE_OFFSET || answer >= CODES.length() - CODE_OFFSET) {
                fail("the answer " + answer + " has no code");
            }
            results.append(CODES.charAt(answer + CODE_OFFSET));
        }

        StringBuilder out = new StringBuilder();
        out.append("#pragma once\n\n");
        out.append("// GENERATED by tools/openrocket-collation/AlphanumComparatorPins.java (Java ")
                .append(System.getProperty("java.version"))
                .append("): do not edit by hand; README.md there says how to regenerate it. What OpenRocket's"
                        + " util/AlphanumComparator answers in en_US, for AlphanumComparatorTests.cpp. Test-only.\n\n");
        out.append("#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n");
        out.append("namespace QtRocket::Test\n{\n\n");
        out.append("/// The OpenRocket commit the pins come from.\n");
        out.append("inline constexpr std::string_view kAlphanumPinsCommit = \"").append(args[0]).append("\";\n\n");
        out.append("/// Two texts and AlphanumComparator.compare(first, second).\n");
        out.append("struct AlphanumCase\n{\n    std::string_view first;\n    std::string_view second;\n"
                + "    int              result;\n};\n\n");
        appendCases(out, "kAlphanumCases", CASES, comparator);
        out.append("/// Texts that NFD changes, with Java's answer, which QtRocket's comparator (no NFD) need not give.\n");
        appendCases(out, "kAlphanumDeviationCases", DEVIATION_CASES, comparator);

        out.append("/// The seed of the corpus's SplitMix64.\n");
        out.append("inline constexpr std::uint64_t kAlphanumCorpusSeed = ").append(String.format("0x%XU", SEED))
                .append(";\n\n");
        out.append("/// The most characters of a random text.\n");
        out.append("inline constexpr std::size_t kAlphanumCorpusMaxLength = ").append(MAX_LENGTH).append(";\n\n");
        out.append("/// The characters the random texts are drawn from.\n");
        out.append("// clang-format off\n");
        out.append("inline constexpr std::array<char32_t, ").append(ALPHABET.length).append("> kAlphanumCorpusAlphabet{{\n");
        for (int i = 0; i < ALPHABET.length; i++) {
            out.append(i % 8 == 0 ? "    " : " ").append(String.format("0x%04X", ALPHABET[i])).append(",");
            if (i % 8 == 7 || i == ALPHABET.length - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n// clang-format on\n\n");
        out.append("/// A pair's answer as one character: its index in this text, minus ").append(CODE_OFFSET)
                .append(".\n");
        out.append("inline constexpr std::string_view kAlphanumResultCodes = \"").append(CODES).append("\";\n\n");
        out.append("/// The offset of the answers in kAlphanumResultCodes.\n");
        out.append("inline constexpr int kAlphanumResultOffset = ").append(CODE_OFFSET).append(";\n\n");
        out.append("/// The answers for the ").append(PAIRS).append(" pairs of the corpus, in order.\n");
        out.append("// clang-format off\n");
        int lines = (results.length() + 79) / 80;
        out.append("inline constexpr std::array<std::string_view, ").append(lines)
                .append("> kAlphanumCorpusResults{{\n");
        for (int i = 0; i < results.length(); i += 80) {
            out.append("    \"").append(results, i, Math.min(results.length(), i + 80)).append("\",\n");
        }
        out.append("}};\n// clang-format on\n\n");
        out.append("}  // namespace QtRocket::Test\n");
        System.out.print(out);
    }

    private static void appendCases(StringBuilder out, String name, String[][] cases, AlphanumComparator comparator) {
        out.append("// clang-format off\n");
        out.append("// NOLINTBEGIN(modernize-use-designated-initializers): a generated table, one entry per row\n");
        out.append("inline constexpr std::array<AlphanumCase, ").append(cases.length).append("> ").append(name)
                .append("{{\n");
        for (String[] c : cases) {
            if (!c[2].isEmpty()) {
                out.append("    // ").append(c[2]).append("\n");
            }
            out.append("    {").append(literal(c[0])).append(", ").append(literal(c[1])).append(", ")
                    .append(comparator.compare(c[0], c[1])).append("},\n");
        }
        out.append("}};\n// NOLINTEND(modernize-use-designated-initializers)\n// clang-format on\n\n");
    }
}
