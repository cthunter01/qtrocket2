#pragma once

#include <string_view>

/// JDK 17's java.text.Collator.getInstance() of the en_US locale: the RuleBasedCollator that
/// OpenRocket's tables sort their text columns with (JTable's TableRowSorter compares a column's
/// strings with Collator.getInstance(): the motor chooser's Manufacturer and Case columns, the
/// parts library's text columns) and that AlphanumComparator compares the letters of part numbers
/// with. Its strength is TERTIARY and its decomposition NO_DECOMPOSITION; PRIMARY is
/// Strings::javaPrimaryCollatorCompare(), OpenRocket's Collator.getInstance(Locale.US) at PRIMARY
/// strength.
///
/// The collation elements (CollationElementIterator's 32-bit orders: a 16-bit primary weight, an
/// 8-bit secondary and an 8-bit tertiary one) are JDK 17's, generated from the JDK by
/// tools/openrocket-collation into src/util/JavaCollationTables.h for every char of the Basic
/// Multilingual Plane, with the rules that cover the rest of Unicode (see there). No source of the
/// JDK is copied: the comparison is reimplemented from the collator's documented behaviour and
/// checked against JDK 17 on random strings (tests/core/util/JavaCollatorPins.h).
///
/// What the en_US rules say, in short: the ASCII control characters and a few format characters
/// are ignorable; spaces, combining accents and hyphens are primary-ignorable (they weigh only at
/// the secondary level); punctuation and symbols sort before the digits, the digits before the
/// letters; a lower-case letter sorts before its capital at the tertiary level ("a" < "A" <
/// "b"); an accented Latin letter is its base letter followed by the accent (U+00E9 equals "e"
/// followed by U+0301), and U+00E6, U+00DF, U+00FE and the like expand to two letters. Characters
/// the rules leave out (Greek, Cyrillic, CJK, the superscript digits, ...) sort after all others
/// by their UTF-16 code units.
///
/// Java's comparison is not that of the Unicode Collation Algorithm, which compares all primary
/// weights first: here the elements of the two strings are compared in step. A primary difference
/// decides at once. The first difference at the secondary level decides unless a primary one comes
/// later, and the first tertiary difference decides unless a primary or secondary one comes later.
/// A primary-ignorable element (a space, an accent, a hyphen) facing an element with a primary
/// weight counts as a secondary difference that makes its string the greater one, and the other
/// string's element is compared with the next element. So "Pro-98 6g XL" sorts after
/// "Pro98-6GXL", the hyphen deciding before the case of "g", and "3/16 in" after "3/16in". An
/// ignorable element (a control character) facing an element with a primary weight is passed
/// over, but facing a primary-ignorable one it is compared as weights of 0 (U+0001 sorts before a
/// space). When one string runs out of elements, a primary weight left in the other makes that one
/// the greater, a secondary weight left counts as a secondary difference, and a tertiary weight
/// left counts not ("a" before "a-", "a" equal to "a" followed by U+0001).
///
/// Sort with a stable sort (std::ranges::stable_sort), as Java's sorts are: equal texts keep their
/// order.
///
/// Deviations: Java's Collator.getInstance() follows the default locale; this is always en_US's
/// (OpenRocket's probes and pins run in en_US). The text is UTF-8: a byte that does not start a
/// well-formed sequence counts as U+FFFD, as Java's decoder reads it.
namespace QtRocket::JavaCollator
{

/// Collator.PRIMARY, SECONDARY and TERTIARY: which differences count. PRIMARY ignores case and
/// accents ("A" equals "a", U+00E9 equals "e"), SECONDARY counts accents but not case, TERTIARY
/// counts both.
enum class Strength
{
    PRIMARY,
    SECONDARY,
    TERTIARY,
};

/// Collator.getInstance().compare(a, b) in en_US with the strength set to @p strength (TERTIARY,
/// as Collator.getInstance() has it, unless given): -1 when @p a sorts before @p b, 1 when it
/// sorts after, 0 when the collator calls them equal.
[[nodiscard]] int compare(std::string_view a, std::string_view b,
                          Strength strength = Strength::TERTIARY);

}  // namespace QtRocket::JavaCollator
