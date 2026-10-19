#pragma once

#include <cstddef>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// OpenRocket's ExpressionParser (info.openrocket.core.util.ExpressionParser) together with the
/// scalar path of the copy of exp4j it evaluates with (de.congrace.exp4j: ExpressionBuilder,
/// PostfixExpression, InfixTranslator, Tokenizer and the number, operator, function, variable and
/// bracket tokens): what a number field of the GUI makes of the text typed into it, a value in the
/// field's unit (DoubleModel's spinner). Ported line by line, quirks included. The rest of exp4j
/// (variables with values, arrays, custom functions) is not ported: OpenRocket's ExpressionParser
/// never uses it.
///
/// The language, as OpenRocket has it in the en_US locale (in every locale here: see the first two
/// deviations):
/// - A number is a run of ASCII digits and points, read as Double.parseDouble reads it
///   (Strings::javaParseDouble(), correctly rounded): "1.", ".5" and "01.200" are numbers, "1.5.2"
///   and "." are not, and there is no sign, exponent or suffix ("1e3", "NaN", "Infinity", "0x10"
///   and "1d" are invalid). Before anything else, every char whose Character.getNumericValue is
///   0 to 9 in JDK 17 becomes the ASCII digit of that value (the digits of every script, the
///   superscript and subscript digits, the circled and Roman numerals up to nine, ...: 550 chars,
///   ExpressionParserTables.h), U+2044 FRACTION SLASH becomes '/', and ',' becomes '.': "1,5" is
///   1.5, and the superscript fractions FractionalUnit writes read back.
/// - A mixed fraction, digits, whitespace, digits, '/' (whitespace around it allowed) and digits,
///   is their sum: "1 3/4" is 1.75, "-1 1/2" is -1.5. Any other whitespace between two numbers
///   (between [0-9.] and [0-9.]) makes the text invalid ("1 2", "1. 2"); elsewhere whitespace is
///   ignored.
/// - The operators + - * / % ^, where % is Java's remainder (as std::fmod) and ^ is Math.pow,
///   right-associative ("2^3^2" is 512). A '-' at the start, after '(' or after an operator is a
///   unary minus, which binds tighter than ^ ("-2^2" is 4); a '+' there is dropped. Brackets (),
///   [] and {} are interchangeable ("(2]" is 2).
/// - The 20 functions of exp4j's FunctionToken, of one argument and in any case: abs, acos, asin,
///   atan, cbrt, ceil, cos, cosh, exp, expm1, floor, round (Math.round, ties up), random (a random
///   number from [0, 1) times the argument), log, sin, sinh, sqrt, tan, tanh, log10. Their names
///   are compared as String.toUpperCase() spells them, so "sqrt" with U+017F LATIN SMALL LETTER
///   LONG S for its "s" is sqrt too. There are no constants (no pi, no e) and no units ("5 cm"
///   is invalid).
///
/// OpenRocket's quirks, all kept:
/// - Whatever the evaluation leaves on the stack below the result is dropped: "2(3)" and "(2)(3)"
///   are 3, "sqrt(16)(4)" is 2 (the function takes the last value), "1 1/2 3/4" is 0.75.
/// - A text "f(x)=..." drops everything up to its first '=' (when that is not its first char), so
///   "x=5" is 5; the text between the first '(' and the first ')' before the '=' is a variable
///   without a value, which makes the text invalid wherever it is used ("f(x)=x+1") but not where
///   it is dropped ("f(x)=(x)(2)" is 2); a variable named like one of 17 of the functions is
///   invalid ("f(sin)=5").
/// - A '-' or '+' after whitespace at the start of the text, or after '[' or '{', is a binary
///   operator, so " -5", " +5" and "[-2]" are invalid while "-5" and "(-2)" are not.
/// - '#' is exp4j's own unary minus ("#3" is -3, "2#3" is -3: the 2 is dropped).
/// - The excess-space check is a regular expression whose '.' matches no line terminator: a line
///   terminator outside the whitespace between the two numbers turns it off ("1 2\n" is 12).
/// - "1/0" is Infinity and "0/0" NaN; the caller decides what to make of them.
///
/// Deviations:
/// - Java takes the decimal separator and the minus sign that it replaces by '.' and '-' from the
///   default locale (DecimalFormatSymbols); here they are en_US's, '.' and '-', in every locale.
///   In a locale whose minus sign is U+2212 MINUS SIGN (Swedish, Finnish, ...) OpenRocket also
///   takes that sign followed by "3" as -3; here that is invalid, as in en_US. ',' and '.' are
///   decimal points in either.
/// - The function names and the Tokenizer's check of a variable's name are mapped to upper and
///   lower case as String.toUpperCase() and toLowerCase() map them in en_US (no conditional
///   special casing), in every locale; Java maps them in the default locale (Tokenizer
///   .isFunction(), FunctionToken's Function.valueOf(name.toUpperCase()), the Tokenizer
///   constructor's varName.toLowerCase()). In Turkish and Azerbaijani, where 'i' becomes U+0130
///   LATIN CAPITAL LETTER I WITH DOT ABOVE and 'I' becomes U+0131 LATIN SMALL LETTER DOTLESS I,
///   OpenRocket knows no sin, asin, sinh or ceil ("sin(0.5)" is invalid there) and takes
///   "f(SIN)=5" as 5; here "sin(0.5)" is 0.479... and "f(SIN)=5" is invalid, as in en_US.
/// - Limits against hostile input, where Java has none: a text longer than kMaxLength bytes, or
///   with more than kMaxDepth brackets open at once, is invalid. The parser itself uses no
///   recursion, so the limits bound its time and memory, not its stack.
/// - The text is UTF-8; one that is not valid UTF-8 is invalid. Java works on UTF-16 chars, and
///   so does the parser after decoding: a character beyond U+FFFF is two surrogates, neither of
///   which is a digit, a letter or whitespace (so U+1D7CE MATHEMATICAL BOLD DIGIT ZERO is
///   invalid, as in Java).
/// - random() draws from QtRocket's own generator (std::mt19937_64 seeded from
///   std::random_device, one for the process, guarded by a mutex), not from java.util.Random;
///   like Math.random(), it gives the upper 53 bits of a draw times 2^-53, from [0, 1).
/// - The transcendental functions are the C++ standard library's, and ^ is std::pow with Java's
///   special cases (MathUtil::javaPow()); they may differ from Java's Math in the last bit, as
///   everywhere in QtRocket.
/// - Java's message is "Invalid expression: <text>" with the cause in the log; here the Error's
///   message adds the cause.
class ExpressionParser
{
public:
    /// The longest text parse() takes, in bytes of UTF-8.
    static constexpr std::size_t kMaxLength = 1000;

    /// The most brackets parse() takes open at once.
    static constexpr std::size_t kMaxDepth = 100;

    /// The value of @p expression (UTF-8), or ErrorCode::PARSE where OpenRocket throws
    /// InvalidExpressionException (and for the limits above). Safe to call from any thread.
    [[nodiscard]] static Result<double> parse(std::string_view expression);
};

}  // namespace QtRocket
