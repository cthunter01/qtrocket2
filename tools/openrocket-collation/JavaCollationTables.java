import java.text.CollationElementIterator;
import java.text.Collator;
import java.text.RuleBasedCollator;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Prints src/util/JavaCollationTables.h: the collation elements JDK 17's Collator.getInstance()
 * gives every character in en_US (a RuleBasedCollator at TERTIARY strength, NO_DECOMPOSITION), for
 * QtRocket's util/JavaCollator. Needs nothing but the JDK. See README.md.
 *
 * Java collates UTF-16 chars, and what this prints describes every string:
 * - kJavaCollationRuns and kJavaCollationElements: the elements CollationElementIterator returns for
 *   each char of the Basic Multilingual Plane the rules map (surrogates excluded), as runs of
 *   consecutive chars with the same elements; each element is the full 32-bit order (primary in the
 *   high 16 bits, then the secondary and the tertiary byte), an ignorable char's 0 included;
 * - kJavaCollationContractions: the pairs of chars that give other elements together than one
 *   after the other;
 * - the rest are rules the C++ code applies and this program checks: a char the rules do not map
 *   gives two elements, kJavaUnmappedOrder and the char in the primary, and a code point above
 *   U+FFFF gives the elements of the char of its low 18 bits when that is a mapped char of the
 *   Basic Multilingual Plane (planes 4, 8, 12 and 16 repeat plane 0), else kJavaUnmappedOrder and
 *   its two surrogates in the primary.
 *
 * It fails when the JDK says otherwise: the collator's class, strength and decomposition; the
 * elements of every unmapped char; no context but the contractions (every mapped char before every
 * char of the Basic Multilingual Plane, a contraction before and after every char, and a seeded
 * sample of other pairs and of triples, the Thai and Lao blocks in full, the scripts whose vowels a
 * collator may reorder); the elements of every code point above U+FFFF. THOROUGH=1 in the
 * environment (an argument "thorough") adds every triple of mapped chars (about 580 million
 * strings, three minutes); the output is the same.
 */
public class JavaCollationTables {

    /** What the iterator returns for an unmapped char before the char itself. */
    private static final int UNMAPPED = 0x7FFF0000;

    /** The tables of structs are written row by row, not with designated initializers. */
    private static final String NOLINT_BEGIN =
            "// NOLINTBEGIN(modernize-use-designated-initializers): a generated table, one entry per row\n";
    private static final String NOLINT_END = "// NOLINTEND(modernize-use-designated-initializers)\n";

    private static RuleBasedCollator collator;
    private static final int[][] elements = new int[0x10000][];
    private static final boolean[] mapped = new boolean[0x10000];

    private static void fail(String message) {
        System.err.println("error: " + message);
        System.exit(1);
    }

    private static String hex4(int c) {
        return String.format("0x%04X", c);
    }

    private static String hex8(int order) {
        return String.format("0x%08X", order);
    }

    private static String name(int c) {
        return String.format("U+%04X", c);
    }

    private static boolean isSurrogate(int c) {
        return c >= 0xD800 && c <= 0xDFFF;
    }

    /** The elements of s, in order, NULLORDER excluded. */
    private static int[] elementsOf(String s) {
        CollationElementIterator iterator = collator.getCollationElementIterator(s);
        int[] buffer = new int[8];
        int count = 0;
        for (int order = iterator.next(); order != CollationElementIterator.NULLORDER; order = iterator.next()) {
            if (count == buffer.length) {
                buffer = Arrays.copyOf(buffer, 2 * count);
            }
            buffer[count++] = order;
        }
        return Arrays.copyOf(buffer, count);
    }

    private static int[] concat(int[]... parts) {
        int length = 0;
        for (int[] part : parts) {
            length += part.length;
        }
        int[] all = new int[length];
        int at = 0;
        for (int[] part : parts) {
            System.arraycopy(part, 0, all, at, part.length);
            at += part.length;
        }
        return all;
    }

    /** The elements of a code point alone, by the rules the C++ code applies. */
    private static int[] expectedOf(int codePoint) {
        if (codePoint < 0x10000) {
            return elements[codePoint];
        }
        int alias = codePoint & 0x3FFFF;
        if (alias < 0x10000 && !isSurrogate(alias) && mapped[alias]) {
            return elements[alias];
        }
        String s = new String(Character.toChars(codePoint));
        return new int[] {UNMAPPED, s.charAt(0) << 16, s.charAt(1) << 16};
    }

    private static String text(int... codePoints) {
        StringBuilder b = new StringBuilder();
        for (int c : codePoints) {
            b.appendCodePoint(c);
        }
        return b.toString();
    }

    private static String names(int... codePoints) {
        StringBuilder b = new StringBuilder();
        for (int c : codePoints) {
            b.append(b.length() == 0 ? "" : " ").append(c < 0x10000 ? name(c) : String.format("U+%06X", c));
        }
        return b.toString();
    }

    /** Fails unless the code points give the elements of contraction or code point after code point. */
    private static void requireNoContext(int[] codePoints, Map<Long, int[]> contractions) {
        List<int[]> parts = new ArrayList<>();
        for (int i = 0; i < codePoints.length; i++) {
            int c = codePoints[i];
            if (i + 1 < codePoints.length && c < 0x10000 && codePoints[i + 1] < 0x10000) {
                int[] contraction = contractions.get(((long) c << 16) | codePoints[i + 1]);
                if (contraction != null) {
                    parts.add(contraction);
                    i++;
                    continue;
                }
            }
            parts.add(expectedOf(c));
        }
        int[] expected = concat(parts.toArray(new int[0][]));
        int[] actual = elementsOf(text(codePoints));
        if (!Arrays.equals(expected, actual)) {
            fail(names(codePoints) + " gives " + Arrays.toString(actual) + ", not " + Arrays.toString(expected));
        }
    }

    /** The elements of the runs and contractions, an equal sequence stored once. */
    private static final List<Integer> pool = new ArrayList<>();
    private static final Map<String, Integer> offsets = new HashMap<>();

    /** The offset of the sequence in the pool, which gets it unless it has it already. */
    private static int intern(int[] sequence) {
        String key = Arrays.toString(sequence);
        Integer offset = offsets.get(key);
        if (offset == null) {
            offset = pool.size();
            offsets.put(key, offset);
            for (int order : sequence) {
                pool.add(order);
            }
        }
        return offset;
    }

    /** SplitMix64, for the seeded samples. */
    private static long state = 0x5EED_C011_A7E5L;

    private static long nextLong() {
        long z = (state += 0x9E3779B97F4A7C15L);
        z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
        z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
        return z ^ (z >>> 31);
    }

    /** A code point of the Basic Multilingual Plane (no surrogate), or now and then one above it. */
    private static int randomCodePoint() {
        long r = nextLong() >>> 1;
        if (r % 16 == 0) {
            return 0x10000 + (int) ((r >>> 8) % 0x100000);
        }
        int c = (int) ((r >>> 8) % 0xF800);
        return c < 0xD800 ? c : c + 0x800;
    }

    public static void main(String[] args) {
        if (!System.getProperty("java.specification.version").equals("17")) {
            fail("the tables are of Java 17; this is Java " + System.getProperty("java.version"));
        }
        if (!Locale.getDefault().equals(Locale.US)) {
            fail("the default locale must be en_US (run with -Duser.language=en -Duser.country=US); it is "
                    + Locale.getDefault());
        }
        boolean thorough = args.length > 0 && args[0].equals("thorough");
        Collator instance = Collator.getInstance();
        if (!(instance instanceof RuleBasedCollator)) {
            fail("Collator.getInstance() is a " + instance.getClass().getName());
        }
        collator = (RuleBasedCollator) instance;
        if (collator.getStrength() != Collator.TERTIARY
                || collator.getDecomposition() != Collator.NO_DECOMPOSITION) {
            fail("Collator.getInstance() has strength " + collator.getStrength() + " and decomposition "
                    + collator.getDecomposition());
        }
        if (!collator.equals(Collator.getInstance(Locale.US))) {
            fail("Collator.getInstance() differs from Collator.getInstance(Locale.US)");
        }

        // Every char alone.
        List<Integer> mappedChars = new ArrayList<>();
        int maxElements = 0;
        for (int c = 0; c <= 0xFFFF; c++) {
            if (isSurrogate(c)) {
                continue;
            }
            int[] e = elementsOf(String.valueOf((char) c));
            elements[c] = e;
            boolean unmapped = e.length == 2 && e[0] == UNMAPPED && e[1] == c << 16;
            if (!unmapped) {
                if (e.length == 0 || (e[0] >>> 16) == 0x7FFF) {
                    fail(name(c) + " gives " + Arrays.toString(e));
                }
                mapped[c] = true;
                mappedChars.add(c);
                maxElements = Math.max(maxElements, e.length);
            }
        }

        // Contractions: a mapped char before every char of the Basic Multilingual Plane. (A char the
        // rules do not map starts none.)
        Map<Long, int[]> contractions = new HashMap<>();
        List<int[]> contractionList = new ArrayList<>();
        for (int c : mappedChars) {
            for (int d = 0; d <= 0xFFFF; d++) {
                if (isSurrogate(d)) {
                    continue;
                }
                int[] both = elementsOf("" + (char) c + (char) d);
                if (!Arrays.equals(both, concat(elements[c], elements[d]))) {
                    contractions.put(((long) c << 16) | d, both);
                    contractionList.add(new int[] {c, d});
                    maxElements = Math.max(maxElements, both.length);
                }
            }
        }
        // A contraction is the same before and after every char, and takes no third char.
        for (int[] pair : contractionList) {
            for (int e = 0; e <= 0xFFFF; e++) {
                if (isSurrogate(e)) {
                    continue;
                }
                requireNoContext(new int[] {pair[0], pair[1], e}, contractions);
                requireNoContext(new int[] {e, pair[0], pair[1]}, contractions);
            }
            // Planes 4, 8, 12 and 16 repeat plane 0 char by char, but not its contractions.
            requireNoContext(new int[] {pair[0] + 0x40000, pair[1]}, contractions);
            requireNoContext(new int[] {pair[0], pair[1] + 0x40000}, contractions);
        }

        // Every code point above U+FFFF alone.
        for (int c = 0x10000; c <= 0x10FFFF; c++) {
            requireNoContext(new int[] {c}, contractions);
        }

        // The Thai and Lao blocks in pairs, and a seeded sample of pairs and triples.
        for (int c = 0x0E00; c <= 0x0EFF; c++) {
            for (int d = 0x0E00; d <= 0x0EFF; d++) {
                requireNoContext(new int[] {c, d}, contractions);
            }
        }
        for (int i = 0; i < 2_000_000; i++) {
            requireNoContext(new int[] {randomCodePoint(), randomCodePoint()}, contractions);
        }
        for (int i = 0; i < 1_000_000; i++) {
            requireNoContext(new int[] {randomCodePoint(), randomCodePoint(), randomCodePoint()}, contractions);
        }
        if (thorough) {
            for (int a : mappedChars) {
                for (int b : mappedChars) {
                    for (int c : mappedChars) {
                        requireNoContext(new int[] {a, b, c}, contractions);
                    }
                }
            }
        }

        // The runs: consecutive mapped chars with the same elements; the elements of equal runs shared.
        List<int[]> runs = new ArrayList<>(); // first, last, offset, count
        int[] run = null;
        for (int c = 0; c <= 0xFFFF; c++) {
            if (!mapped[c]) {
                run = null;
            } else if (run != null && Arrays.equals(elements[c], elements[run[0]])) {
                run[1] = c;
            } else {
                run = new int[] {c, c, intern(elements[c]), elements[c].length};
                runs.add(run);
            }
        }
        List<int[]> contractionRows = new ArrayList<>(); // first, second, offset, count
        contractionList.sort((x, y) -> x[0] != y[0] ? Integer.compare(x[0], y[0]) : Integer.compare(x[1], y[1]));
        for (int[] pair : contractionList) {
            int[] e = contractions.get(((long) pair[0] << 16) | pair[1]);
            contractionRows.add(new int[] {pair[0], pair[1], intern(e), e.length});
        }
        if (pool.size() > 0xFFFF || maxElements > 0xFF) {
            fail("the tables outgrow their types: " + pool.size() + " elements, " + maxElements + " per char");
        }

        StringBuilder out = new StringBuilder();
        out.append("#pragma once\n\n");
        out.append("// GENERATED by tools/openrocket-collation/JavaCollationTables.java (Java ")
                .append(System.getProperty("java.version"))
                .append("): do not edit by hand; README.md there says how to regenerate it. The collation"
                        + " elements of JDK 17's Collator.getInstance() in en_US (a RuleBasedCollator at TERTIARY"
                        + " strength without decomposition), for util/JavaCollator.cpp only. Each element is"
                        + " CollationElementIterator's 32-bit order: the primary weight in the high 16 bits, then"
                        + " the secondary and the tertiary byte. The generator checked the rules"
                        + " util/JavaCollator.cpp applies beyond the tables: an unmapped char of the Basic"
                        + " Multilingual Plane gives kJavaUnmappedOrder, then the char in the primary; a code point"
                        + " above U+FFFF gives the elements of the mapped char of its low 18 bits if there is one"
                        + " (no contraction), else kJavaUnmappedOrder and its two surrogates in the primary; and no"
                        + " char changes the elements of another but by a contraction")
                .append(".\n\n");
        out.append("#include <array>\n#include <cstddef>\n#include <cstdint>\n\n");
        out.append("namespace QtRocket::Detail\n{\n\n");
        out.append("/// What CollationElementIterator returns for an unmapped char before the char itself.\n");
        out.append("inline constexpr std::uint32_t kJavaUnmappedOrder = ").append(hex8(UNMAPPED)).append(";\n\n");
        out.append("/// The most elements a char or a contraction gives.\n");
        out.append("inline constexpr std::size_t kJavaMaxCollationElements = ").append(maxElements).append(";\n\n");
        out.append("/// The elements of the runs and contractions, each run's or contraction's in order.\n");
        out.append("// clang-format off\n");
        out.append("inline constexpr std::array<std::uint32_t, ").append(pool.size())
                .append("> kJavaCollationElements{{\n");
        for (int i = 0; i < pool.size(); i++) {
            out.append(i % 6 == 0 ? "    " : " ").append(hex8(pool.get(i))).append(",");
            if (i % 6 == 5 || i == pool.size() - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n// clang-format on\n\n");
        out.append("/// Consecutive chars of the Basic Multilingual Plane, first to last, that the rules map to\n"
                + "/// the same elements: count of kJavaCollationElements from offset.\n");
        out.append("struct JavaCollationRun\n{\n    char16_t      first;\n    char16_t      last;\n"
                + "    std::uint16_t offset;\n    std::uint8_t  count;\n};\n\n");
        out.append("/// The ").append(mappedChars.size()).append(" chars the rules map, in ascending order.\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<JavaCollationRun, ").append(runs.size())
                .append("> kJavaCollationRuns{{\n");
        for (int i = 0; i < runs.size(); i++) {
            int[] r = runs.get(i);
            out.append(i % 3 == 0 ? "    " : " ").append("{").append(hex4(r[0])).append(", ").append(hex4(r[1]))
                    .append(", ").append(r[2]).append(", ").append(r[3]).append("},");
            if (i % 3 == 2 || i == runs.size() - 1) {
                out.append("\n");
            }
        }
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");
        out.append("/// Two chars that give count of kJavaCollationElements from offset together.\n");
        out.append("struct JavaCollationContraction\n{\n    char16_t      first;\n    char16_t      second;\n"
                + "    std::uint16_t offset;\n    std::uint8_t  count;\n};\n\n");
        out.append("/// The contractions, in ascending order of the first char, then the second.\n");
        out.append("// clang-format off\n").append(NOLINT_BEGIN);
        out.append("inline constexpr std::array<JavaCollationContraction, ").append(contractionRows.size())
                .append("> kJavaCollationContractions{{\n");
        for (int[] r : contractionRows) {
            out.append("    {").append(hex4(r[0])).append(", ").append(hex4(r[1])).append(", ").append(r[2])
                    .append(", ").append(r[3]).append("},\n");
        }
        out.append("}};\n").append(NOLINT_END).append("// clang-format on\n\n");
        out.append("}  // namespace QtRocket::Detail\n");
        System.out.print(out);
        System.err.println("mapped chars " + mappedChars.size() + ", runs " + runs.size() + ", elements "
                + pool.size() + ", contractions " + contractionRows.size() + ", at most " + maxElements
                + " elements");
    }
}
