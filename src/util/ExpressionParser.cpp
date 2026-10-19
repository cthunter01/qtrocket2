#include "QtRocket/util/ExpressionParser.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <format>
#include <mutex>
#include <optional>
#include <random>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ExpressionParserTables.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

// The port follows OpenRocket's ExpressionParser.parse() and modify() and, for the expression
// they hand over, the scalar path of exp4j as ExpressionBuilder.build() and
// PostfixExpression.calculate() take it: PostfixExpression.fromInfix() (the "f(x)=" part),
// InfixTranslator (unary operators, whitespace, the shunting-yard algorithm of the tokens'
// mutateStackForInfixTranslation()), Tokenizer.tokenize() and the tokens'
// mutateStackForCalculation(). Every exception Java throws on the way (InvalidExpressionException,
// UnparsableExpressionException, UnknownFunctionException, NumberFormatException,
// EmptyStackException, IllegalArgumentException, StringIndexOutOfBoundsException,
// ClassCastException, NullPointerException) is caught by ExpressionParser.parse(), so each one is
// an Error here.

namespace QtRocket
{
namespace
{

/// Java's String: UTF-16 code units.
using JavaString = std::u16string;

/// Chars.FRACTION, U+2044 FRACTION SLASH.
constexpr char16_t kFractionSlash = 0x2044;

/// The cause of a failure; parse() puts the text in front of it.
[[nodiscard]] std::unexpected<Error> invalid(
    std::string reason, std::source_location where = std::source_location::current())
{
    return fail(ErrorCode::PARSE, std::move(reason), where);
}

// -----------------------------------------------------------------------------------------------
// What java.lang.Character and java.util.regex say about a char (ExpressionParserTables.h)

[[nodiscard]] constexpr bool isAsciiDigit(char16_t c) noexcept
{
    return c >= u'0' && c <= u'9';
}

/// [0-9.] of java.util.regex; \d is [0-9] there without UNICODE_CHARACTER_CLASS.
[[nodiscard]] constexpr bool isDigitOrPoint(char16_t c) noexcept
{
    return isAsciiDigit(c) || c == u'.';
}

/// \s of java.util.regex without UNICODE_CHARACTER_CLASS: [ \t\n\x0B\f\r].
[[nodiscard]] constexpr bool isRegexSpace(char16_t c) noexcept
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == 0x000B || c == u'\f' || c == u'\r';
}

/// The line terminators of java.util.regex, which '.' does not match (no DOTALL, no
/// UNIX_LINES): \n, \r, U+0085 NEXT LINE, U+2028 LINE SEPARATOR and U+2029 PARAGRAPH
/// SEPARATOR.
[[nodiscard]] constexpr bool isLineTerminator(char16_t c) noexcept
{
    return c == u'\n' || c == u'\r' || c == 0x0085 || c == 0x2028 || c == 0x2029;
}

/// The ASCII digit of the value of Character.getNumericValue(c) when that is 0 to 9.
[[nodiscard]] std::optional<char16_t> javaDigitOf(char16_t c) noexcept
{
    for (const Detail::JavaDigitRun& run : Detail::kJavaDigitRuns)
    {
        if (c < run.first)
        {
            break;
        }
        if (c <= run.last)
        {
            return static_cast<char16_t>(u'0' + run.firstValue + (c - run.first));
        }
    }
    return std::nullopt;
}

/// Character.isWhitespace(c).
[[nodiscard]] bool isJavaWhitespace(char16_t c) noexcept
{
    return std::ranges::binary_search(Detail::kJavaWhitespace, c);
}

/// Character.isLetter(c) for a char of the text that modify() returned. The only digits left there
/// are ASCII ones (every char of Character.isDigit had a numeric value of 0 to 9 and was
/// replaced; the generator of the tables checks it), and Character.isLetterOrDigit is isLetter
/// for every other char. A surrogate is neither.
[[nodiscard]] bool isJavaLetter(char16_t c) noexcept
{
    return !isAsciiDigit(c) && Strings::javaIsLetterOrDigit(static_cast<char32_t>(c));
}

/// A char of a name of the Tokenizer after its first: Character.isLetter, Character.isDigit, '_'
/// or '$'.
[[nodiscard]] bool isNamePart(char16_t c) noexcept
{
    return isJavaLetter(c) || isAsciiDigit(c) || c == u'_' || c == u'$';
}

/// The ASCII text that @p mappings give @p c, or nullopt when they do not list it.
[[nodiscard]] std::optional<std::string_view> asciiMappingOf(
    std::span<const Detail::JavaCaseMapping> mappings, char16_t c) noexcept
{
    for (const Detail::JavaCaseMapping& mapping : mappings)
    {
        if (mapping.character == c)
        {
            return mapping.ascii;
        }
    }
    return std::nullopt;
}

/// String.toUpperCase() in en_US of a name of the Tokenizer when that is ASCII, else nullopt. In
/// en_US a String is mapped char by char (the conditional special casing is the Lithuanian,
/// Turkish and Azeri locales' and the final sigma's, none of which gives ASCII).
[[nodiscard]] std::optional<std::string> javaUpperCaseAscii(const JavaString& name)
{
    std::string upper;
    for (const char16_t c : name)
    {
        if (c < 0x80)
        {
            const auto ascii = static_cast<char>(c);
            upper.push_back(ascii >= 'a' && ascii <= 'z' ? static_cast<char>(ascii - 'a' + 'A')
                                                         : ascii);
        }
        else if (const std::optional<std::string_view> mapped =
                     asciiMappingOf(Detail::kJavaUpperCaseToAscii, c))
        {
            upper.append(*mapped);
        }
        else
        {
            return std::nullopt;
        }
    }
    return upper;
}

/// String.toLowerCase() in en_US of @p text when that is ASCII, else nullopt.
[[nodiscard]] std::optional<std::string> javaLowerCaseAscii(const JavaString& text)
{
    std::string lower;
    for (const char16_t c : text)
    {
        if (c < 0x80)
        {
            const auto ascii = static_cast<char>(c);
            lower.push_back(ascii >= 'A' && ascii <= 'Z' ? static_cast<char>(ascii - 'A' + 'a')
                                                         : ascii);
        }
        else if (const std::optional<std::string_view> mapped =
                     asciiMappingOf(Detail::kJavaLowerCaseToAscii, c))
        {
            lower.append(*mapped);
        }
        else
        {
            return std::nullopt;
        }
    }
    return lower;
}

/// The end of the run of chars from @p begin for which @p predicate holds.
template <class Predicate>
[[nodiscard]] std::size_t skipWhile(const JavaString& text, std::size_t begin,
                                    Predicate predicate) noexcept
{
    std::size_t end = begin;
    while (end < text.size() && predicate(text[end]))
    {
        ++end;
    }
    return end;
}

// -----------------------------------------------------------------------------------------------
// ExpressionParser.modify()

/// The loop at the start of modify(): a char whose numeric value is 0 to 9 becomes that ASCII
/// digit, Chars.FRACTION '/', and DECIMAL_SEPARATOR or ',' a '.'. The DECIMAL_SEPARATOR and
/// MINUS_SIGN of en_US are '.' and '-' (the class comment says why en_US), so the replacements of
/// '.' and of the minus sign by '-' change nothing.
void normalizeChars(JavaString& text) noexcept
{
    for (char16_t& c : text)
    {
        if (const std::optional<char16_t> digit = javaDigitOf(c))
        {
            c = *digit;
        }
        if (c == kFractionSlash)
        {
            c = u'/';
        }
        if (c == u',')
        {
            c = u'.';
        }
    }
}

/// A match of the mixed-fraction pattern and what replaces it.
struct MixedFraction
{
    /// Where the match ends.
    std::size_t end;
    /// "($1+$2/$3)".
    JavaString replacement;
};

/// The match of "(\d+)\s+(\d+)\s*/\s*(\d+)(?![\d.])" at @p begin, if any (the look-behind is the
/// caller's). Each group and each run of whitespace takes the whole run of its kind: a shorter one
/// would leave the pattern a char it cannot take next, so the greedy match is the only one.
[[nodiscard]] std::optional<MixedFraction> matchMixedFraction(const JavaString& text,
                                                              std::size_t       begin)
{
    const std::size_t wholeEnd = skipWhile(text, begin, isAsciiDigit);
    if (wholeEnd == begin)
    {
        return std::nullopt;
    }
    const std::size_t numeratorBegin = skipWhile(text, wholeEnd, isRegexSpace);
    if (numeratorBegin == wholeEnd)
    {
        return std::nullopt;
    }
    const std::size_t numeratorEnd = skipWhile(text, numeratorBegin, isAsciiDigit);
    const std::size_t slash        = skipWhile(text, numeratorEnd, isRegexSpace);
    if (numeratorEnd == numeratorBegin || slash == text.size() || text[slash] != u'/')
    {
        return std::nullopt;
    }
    const std::size_t denominatorBegin = skipWhile(text, slash + 1, isRegexSpace);
    const std::size_t denominatorEnd   = skipWhile(text, denominatorBegin, isAsciiDigit);
    if (denominatorEnd == denominatorBegin ||
        (denominatorEnd < text.size() && isDigitOrPoint(text[denominatorEnd])))
    {
        return std::nullopt;
    }
    JavaString replacement = u"(";
    replacement.append(text, begin, wholeEnd - begin);
    replacement.push_back(u'+');
    replacement.append(text, numeratorBegin, numeratorEnd - numeratorBegin);
    replacement.push_back(u'/');
    replacement.append(text, denominatorBegin, denominatorEnd - denominatorBegin);
    replacement.push_back(u')');
    return MixedFraction{.end = denominatorEnd, .replacement = std::move(replacement)};
}

/// exp.replaceAll("(?<![\\d.])(\\d+)\\s+(\\d+)\\s*/\\s*(\\d+)(?![\\d.])", "($1+$2/$3)"):
/// std::regex has no look-behind, so the match is written out. As replaceAll does, the search
/// goes on after each match, and the look-behind looks at the text, never at a replacement.
[[nodiscard]] JavaString replaceMixedFractions(const JavaString& text)
{
    JavaString  out;
    std::size_t at = 0;
    while (at < text.size())
    {
        if (at == 0 || !isDigitOrPoint(text[at - 1]))
        {
            if (std::optional<MixedFraction> match = matchMixedFraction(text, at))
            {
                out += match->replacement;
                at = match->end;
                continue;
            }
        }
        out.push_back(text[at]);
        ++at;
    }
    return out;
}

/// exp.matches(".*[0-9.]\\s+[0-9.].*"): somewhere a [0-9.], a run of \s and another [0-9.], with
/// every line terminator of the text (which '.' does not match) inside that run.
[[nodiscard]] bool hasExcessSpace(const JavaString& text) noexcept
{
    std::optional<std::size_t> firstTerminator;
    std::size_t                lastTerminator = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (isLineTerminator(text[at]))
        {
            if (!firstTerminator)
            {
                firstTerminator = at;
            }
            lastTerminator = at;
        }
    }
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (!isDigitOrPoint(text[at]))
        {
            continue;
        }
        const std::size_t next = skipWhile(text, at + 1, isRegexSpace);
        if (next == at + 1 || next == text.size() || !isDigitOrPoint(text[next]))
        {
            continue;
        }
        if (!firstTerminator || (*firstTerminator > at && lastTerminator < next))
        {
            return true;
        }
    }
    return false;
}

/// ExpressionParser.modify().
[[nodiscard]] Result<JavaString> modify(JavaString text)
{
    normalizeChars(text);
    JavaString replaced = replaceMixedFractions(text);
    if (hasExcessSpace(replaced))
    {
        return invalid("the expression contains excess space");
    }
    return replaced;
}

// -----------------------------------------------------------------------------------------------
// PostfixExpression.fromInfix() and the Tokenizer's constructor

/// What PostfixExpression.fromInfix() makes of the text: the expression, and the variable it names.
struct Infix
{
    JavaString                expression;
    std::optional<JavaString> variable;
};

/// The Tokenizer's own list of function names (functionNames), which its constructor checks a
/// variable against in lower case: FunctionToken's functions but round, random and log10.
constexpr std::array<std::string_view, 17> kTokenizerFunctionNames{
    "abs",   "acos",  "asin", "atan", "cbrt", "ceil", "cos", "cosh", "exp",
    "expm1", "floor", "log",  "sin",  "sinh", "sqrt", "tan", "tanh",
};

/// PostfixExpression.fromInfix(): when the text has a '=' after its first char, the expression is
/// what follows the first '=', and the text before it names a variable between its first '(' and
/// its first ')' (split(",") of that is the whole of it: modify() left no ','), when both come
/// after its first char. A ')' before the '(' is Java's StringIndexOutOfBoundsException. Then the
/// Tokenizer's constructor refuses a variable named like one of its functions.
[[nodiscard]] Result<Infix> splitFunctionDefinition(JavaString text)
{
    const std::size_t equals = text.find(u'=');
    if (equals == JavaString::npos || equals == 0)
    {
        return Infix{.expression = std::move(text), .variable = std::nullopt};
    }
    Infix             infix{.expression = text.substr(equals + 1), .variable = std::nullopt};
    const std::size_t open  = text.find(u'(');
    const std::size_t close = text.find(u')');
    if (open >= equals || open == 0 || close >= equals || close == 0)
    {
        return infix;
    }
    if (close < open)
    {
        return invalid("')' before '(' in the part before '='");
    }
    infix.variable = text.substr(open + 1, close - open - 1);
    if (const std::optional<std::string> lower = javaLowerCaseAscii(*infix.variable);
        lower &&
        std::ranges::find(kTokenizerFunctionNames, *lower) != kTokenizerFunctionNames.end())
    {
        return invalid(std::format("the variable has the name of the function {}", *lower));
    }
    return infix;
}

// -----------------------------------------------------------------------------------------------
// InfixTranslator.substituteUnaryOperators()

/// OperatorToken.isOperator(c).
[[nodiscard]] constexpr bool isOperatorChar(char16_t c) noexcept
{
    return c == u'+' || c == u'-' || c == u'*' || c == u'/' || c == u'^' || c == u'#' || c == u'%';
}

/// InfixTranslator.substituteUnaryOperators(): whitespace removed, a unary minus written '#' and a
/// unary plus dropped. Unary means at index 0, after '(' or after an operator; lastChar is the
/// last char written so far, or a space while nothing is written yet, so a sign after leading
/// whitespace is binary.
[[nodiscard]] JavaString substituteUnaryOperators(const JavaString& expression)
{
    JavaString out;
    char16_t   lastChar = u' ';
    for (std::size_t at = 0; at < expression.size(); ++at)
    {
        if (!out.empty())
        {
            lastChar = out.back();
        }
        const char16_t c      = expression[at];
        const bool     binary = at > 0 && lastChar != u'(' && !isOperatorChar(lastChar);
        if (c == u'+')
        {
            if (binary)
            {
                out.push_back(c);
            }
        }
        else if (c == u'-')
        {
            out.push_back(binary ? u'-' : u'#');
        }
        else if (!isJavaWhitespace(c))
        {
            out.push_back(c);
        }
    }
    return out;
}

// -----------------------------------------------------------------------------------------------
// The tokens and Tokenizer.tokenize()

/// OperatorToken.Operation; UNARY_PLUS has no char and never occurs.
enum class Operation : std::uint8_t
{
    ADDITION,
    SUBTRACTION,
    MULTIPLICATION,
    DIVISION,
    MODULO,
    EXPONENTIATION,
    UNARY_MINUS,
};

/// FunctionToken.Function, in its order.
enum class Function : std::uint8_t
{
    ABS,
    ACOS,
    ASIN,
    ATAN,
    CBRT,
    CEIL,
    COS,
    COSH,
    EXP,
    EXPM1,
    FLOOR,
    ROUND,
    RANDOM,
    LOG,
    SIN,
    SINH,
    SQRT,
    TAN,
    TANH,
    LOG10,
};

/// The names of FunctionToken.Function, in its order (Function::valueOf of the upper-case name).
constexpr std::array<std::string_view, 20> kFunctionNames{
    "ABS",   "ACOS",  "ASIN",   "ATAN", "CBRT", "CEIL", "COS",  "COSH", "EXP",  "EXPM1",
    "FLOOR", "ROUND", "RANDOM", "LOG",  "SIN",  "SINH", "SQRT", "TAN",  "TANH", "LOG10",
};

/// NumberToken, VariableToken, OperatorToken, FunctionToken and ParenthesisToken.
enum class TokenKind : std::uint8_t
{
    NUMBER,
    VARIABLE,
    OPERATOR,
    FUNCTION,
    OPEN_BRACKET,
    CLOSE_BRACKET,
};

struct Token
{
    TokenKind kind{TokenKind::NUMBER};
    double    number{0};                       ///< of a NUMBER
    Operation operation{Operation::ADDITION};  ///< of an OPERATOR
    Function  function{Function::ABS};         ///< of a FUNCTION
};

/// OperatorToken.getOperation(c) for an operator char.
[[nodiscard]] Operation operationOf(char16_t c)
{
    switch (c)
    {
        case u'+':
            return Operation::ADDITION;
        case u'-':
            return Operation::SUBTRACTION;
        case u'*':
            return Operation::MULTIPLICATION;
        case u'/':
            return Operation::DIVISION;
        case u'^':
            return Operation::EXPONENTIATION;
        case u'#':
            return Operation::UNARY_MINUS;
        case u'%':
            return Operation::MODULO;
        default:
            QTROCKET_UNREACHABLE();
    }
}

/// The function a name of the Tokenizer names (isFunction(), FunctionToken's constructor).
[[nodiscard]] std::optional<Function> functionNamed(const JavaString& name)
{
    const std::optional<std::string> upper = javaUpperCaseAscii(name);
    if (!upper)
    {
        return std::nullopt;
    }
    std::size_t index = 0;
    for (const std::string_view functionName : kFunctionNames)
    {
        if (functionName == *upper)
        {
            return static_cast<Function>(index);
        }
        ++index;
    }
    return std::nullopt;
}

/// The number at @p at, a run of [0-9.] through Double.parseDouble; moves @p at past it.
[[nodiscard]] Result<Token> readNumber(const JavaString& infix, std::size_t& at)
{
    const std::size_t end = skipWhile(infix, at, isDigitOrPoint);
    std::string       ascii;
    ascii.reserve(end - at);
    for (std::size_t i = at; i < end; ++i)
    {
        ascii.push_back(static_cast<char>(infix[i]));
    }
    at                                 = end;
    const std::optional<double> number = Strings::javaParseDouble(ascii);
    if (!number)
    {
        return invalid(std::format("'{}' is not a number", ascii));
    }
    return Token{.kind = TokenKind::NUMBER, .number = *number};
}

/// The name at @p at, a variable or a function; moves @p at past it.
[[nodiscard]] Result<Token> readName(const JavaString& infix, std::size_t& at,
                                     const std::optional<JavaString>& variable)
{
    const std::size_t end  = skipWhile(infix, at + 1, isNamePart);
    const JavaString  name = infix.substr(at, end - at);
    at                     = end;
    if (variable && name == *variable)
    {
        return Token{.kind = TokenKind::VARIABLE};
    }
    if (const std::optional<Function> function = functionNamed(name))
    {
        return Token{.kind = TokenKind::FUNCTION, .function = *function};
    }
    return invalid(std::format("unknown name '{}'",
                               Strings::fromCodePoints(std::u32string(name.begin(), name.end()))));
}

/// The token of a char that is no number and no name. ',' (FunctionSeparatorToken) cannot occur:
/// modify() turned every ',' into '.'.
[[nodiscard]] Result<Token> readSymbol(char16_t c)
{
    if (isOperatorChar(c))
    {
        return Token{.kind = TokenKind::OPERATOR, .operation = operationOf(c)};
    }
    if (c == u'(' || c == u'[' || c == u'{')
    {
        return Token{.kind = TokenKind::OPEN_BRACKET};
    }
    if (c == u')' || c == u']' || c == u'}')
    {
        return Token{.kind = TokenKind::CLOSE_BRACKET};
    }
    return invalid(std::format("unknown symbol U+{:04X}", static_cast<unsigned>(c)));
}

/// Tokenizer.tokenize() of the text substituteUnaryOperators() made, with the variable of
/// fromInfix() if there is one (no custom functions).
[[nodiscard]] Result<std::vector<Token>> tokenize(const JavaString&                infix,
                                                  const std::optional<JavaString>& variable)
{
    std::vector<Token> tokens;
    std::size_t        at = 0;
    while (at < infix.size())
    {
        const char16_t c = infix[at];
        if (c == u' ')
        {
            ++at;
            continue;
        }
        Result<Token> token = [&]() -> Result<Token> {
            if (isDigitOrPoint(c))
            {
                return readNumber(infix, at);
            }
            if (isJavaLetter(c) || c == u'_' || c == u'$')
            {
                return readName(infix, at, variable);
            }
            ++at;
            return readSymbol(c);
        }();
        if (!token)
        {
            return std::unexpected(std::move(token.error()));
        }
        tokens.push_back(*token);
    }
    return tokens;
}

// -----------------------------------------------------------------------------------------------
// InfixTranslator.toPostfixExpression(): the tokens' mutateStackForInfixTranslation()

/// Operation.precedence.
[[nodiscard]] int precedenceOf(Operation operation) noexcept
{
    switch (operation)
    {
        case Operation::ADDITION:
        case Operation::SUBTRACTION:
            return 1;
        case Operation::MULTIPLICATION:
        case Operation::DIVISION:
        case Operation::MODULO:
            return 2;
        case Operation::EXPONENTIATION:
            return 3;
        case Operation::UNARY_MINUS:
            return 4;
    }
    return 0;
}

/// Operation.leftAssociative.
[[nodiscard]] bool isLeftAssociative(Operation operation) noexcept
{
    return operation != Operation::EXPONENTIATION && operation != Operation::UNARY_MINUS;
}

/// OperatorToken.mutateStackForInfixTranslation(): functions on top of the stack go to the output,
/// and so do operators while this one does not bind tighter; then this one goes on the stack.
void translateOperator(const Token& token, std::vector<Token>& stack, std::vector<Token>& output)
{
    while (!stack.empty())
    {
        const Token before = stack.back();
        if (before.kind == TokenKind::OPERATOR)
        {
            const int  precedence = precedenceOf(token.operation);
            const int  above      = precedenceOf(before.operation);
            const bool pops =
                isLeftAssociative(token.operation) ? precedence <= above : precedence < above;
            if (!pops)
            {
                break;
            }
        }
        else if (before.kind != TokenKind::FUNCTION)
        {
            break;
        }
        output.push_back(before);
        stack.pop_back();
    }
    stack.push_back(token);
}

/// ParenthesisToken.mutateStackForInfixTranslation() of a closing bracket: operators and functions
/// go to the output down to the nearest opening bracket of any kind, which is dropped. Without
/// one, Java's peek() of the empty stack throws EmptyStackException.
[[nodiscard]] Result<void> translateClosingBracket(std::vector<Token>& stack,
                                                   std::vector<Token>& output)
{
    while (true)
    {
        if (stack.empty())
        {
            return invalid("a closing bracket without an opening one");
        }
        if (stack.back().kind != TokenKind::OPERATOR && stack.back().kind != TokenKind::FUNCTION)
        {
            break;
        }
        output.push_back(stack.back());
        stack.pop_back();
    }
    stack.pop_back();
    return {};
}

/// InfixTranslator.toPostfixExpression(): the tokens in postfix order. What is left on the stack
/// at the end goes to the output too, opening brackets included. PostfixExpression tokenizes the
/// postfix text again, the token values separated by spaces; that gives these same tokens, so they
/// are evaluated as they are.
[[nodiscard]] Result<std::vector<Token>> toPostfix(const std::vector<Token>& tokens)
{
    std::vector<Token> output;
    std::vector<Token> stack;
    std::size_t        depth = 0;
    for (const Token& token : tokens)
    {
        switch (token.kind)
        {
            case TokenKind::NUMBER:
            case TokenKind::VARIABLE:
                output.push_back(token);
                break;
            case TokenKind::FUNCTION:
                stack.push_back(token);
                break;
            case TokenKind::OPERATOR:
                translateOperator(token, stack, output);
                break;
            case TokenKind::OPEN_BRACKET:
                if (++depth > ExpressionParser::kMaxDepth)
                {
                    return invalid(std::format("more than {} brackets open at once",
                                               ExpressionParser::kMaxDepth));
                }
                stack.push_back(token);
                break;
            case TokenKind::CLOSE_BRACKET:
                if (Result<void> closed = translateClosingBracket(stack, output); !closed)
                {
                    return std::unexpected(std::move(closed.error()));
                }
                --depth;
                break;
        }
    }
    while (!stack.empty())
    {
        output.push_back(stack.back());
        stack.pop_back();
    }
    return output;
}

// -----------------------------------------------------------------------------------------------
// PostfixExpression.calculate(): the tokens' mutateStackForCalculation()

/// A value on the stack of calculate(); a variable pushes nullopt, Java's null of
/// VariableSet.getVariableNamed(), as ExpressionBuilder gives the variable no value.
using StackValue = std::optional<double>;

/// Stack.pop() and the use of the value: an empty stack is Java's EmptyStackException, a variable
/// its NullPointerException.
[[nodiscard]] Result<double> popValue(std::vector<StackValue>& stack)
{
    if (stack.empty())
    {
        return invalid("an operator or a function without its operand");
    }
    const StackValue value = stack.back();
    stack.pop_back();
    if (!value)
    {
        return invalid("the variable has no value");
    }
    return *value;
}

/// Math.random(): a double from [0, 1) in steps of 2^-53 (the class comment says which generator).
[[nodiscard]] double nextRandom()
{
    static std::mutex      s_mutex;
    static std::mt19937_64 s_engine = [] {
        std::array<std::random_device::result_type, 8> entropy{};
        try
        {
            // std::random_device yields 32 bits per call; a seed sequence spreads several of them
            // over the engine's whole state.
            std::random_device device;
            for (auto& word : entropy)
            {
                word = device();
            }
        }
        catch (const std::exception&)
        {
            // No source of random numbers on this platform (std::random_device may throw then).
            const auto ticks = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            entropy[0] = static_cast<std::random_device::result_type>(ticks);
            entropy[1] = static_cast<std::random_device::result_type>(ticks >> 32U);
        }
        std::seed_seq seed(entropy.begin(), entropy.end());
        return std::mt19937_64{seed};
    }();
    std::uint64_t bits = 0;
    {
        const std::scoped_lock lock{s_mutex};
        bits = s_engine();
    }
    return static_cast<double>(bits >> 11U) * 0x1.0p-53;
}

/// FunctionToken.applyFunction(double).
[[nodiscard]] double applyFunction(Function function, double x)
{
    switch (function)
    {
        case Function::ABS:
            return std::fabs(x);
        case Function::ACOS:
            return std::acos(x);
        case Function::ASIN:
            return std::asin(x);
        case Function::ATAN:
            return std::atan(x);
        case Function::CBRT:
            return std::cbrt(x);
        case Function::CEIL:
            return std::ceil(x);
        case Function::COS:
            return std::cos(x);
        case Function::COSH:
            return std::cosh(x);
        case Function::EXP:
            return std::exp(x);
        case Function::EXPM1:
            return std::expm1(x);
        case Function::FLOOR:
            return std::floor(x);
        case Function::ROUND:
            // Math.round() is a long, which Java widens to a double here.
            return static_cast<double>(MathUtil::javaRound(x));
        case Function::RANDOM:
            return nextRandom() * x;
        case Function::LOG:
            return std::log(x);
        case Function::SIN:
            return std::sin(x);
        case Function::SINH:
            return std::sinh(x);
        case Function::SQRT:
            return std::sqrt(x);
        case Function::TAN:
            return std::tan(x);
        case Function::TANH:
            return std::tanh(x);
        case Function::LOG10:
            return std::log10(x);
    }
    QTROCKET_UNREACHABLE();
}

/// OperatorToken.applyOperation(double[]) of a binary operation.
[[nodiscard]] double applyBinaryOperation(Operation operation, double a, double b)
{
    switch (operation)
    {
        case Operation::ADDITION:
            return a + b;
        case Operation::SUBTRACTION:
            return a - b;
        case Operation::MULTIPLICATION:
            return a * b;
        case Operation::DIVISION:
            return a / b;
        case Operation::MODULO:
            // Java's % of doubles truncates, as fmod does.
            return std::fmod(a, b);
        case Operation::EXPONENTIATION:
            return MathUtil::javaPow(a, b);
        case Operation::UNARY_MINUS:
            break;
    }
    QTROCKET_UNREACHABLE();
}

/// The mutateStackForCalculation() of @p token. An opening bracket left in the postfix form is
/// Java's ClassCastException (a ParenthesisToken is no CalculationToken).
[[nodiscard]] Result<void> calculate(const Token& token, std::vector<StackValue>& stack)
{
    switch (token.kind)
    {
        case TokenKind::NUMBER:
            stack.emplace_back(token.number);
            return {};
        case TokenKind::VARIABLE:
            stack.emplace_back(std::nullopt);
            return {};
        case TokenKind::FUNCTION:
        {
            const Result<double> x = popValue(stack);
            if (!x)
            {
                return std::unexpected(x.error());
            }
            stack.emplace_back(applyFunction(token.function, *x));
            return {};
        }
        case TokenKind::OPERATOR:
        {
            const Result<double> second = popValue(stack);
            if (!second)
            {
                return std::unexpected(second.error());
            }
            if (token.operation == Operation::UNARY_MINUS)
            {
                stack.emplace_back(-*second);
                return {};
            }
            const Result<double> first = popValue(stack);
            if (!first)
            {
                return std::unexpected(first.error());
            }
            stack.emplace_back(applyBinaryOperation(token.operation, *first, *second));
            return {};
        }
        case TokenKind::OPEN_BRACKET:
        case TokenKind::CLOSE_BRACKET:
            break;
    }
    return invalid("a bracket that is not closed");
}

/// PostfixExpression.calculate(): the value on top of the stack at the end.
[[nodiscard]] Result<double> evaluate(const std::vector<Token>& postfix)
{
    std::vector<StackValue> stack;
    for (const Token& token : postfix)
    {
        if (Result<void> step = calculate(token, stack); !step)
        {
            return std::unexpected(std::move(step.error()));
        }
    }
    return popValue(stack);
}

/// ExpressionParser.parse() of a text in Java's chars.
[[nodiscard]] Result<double> parseJavaString(JavaString text)
{
    Result<JavaString> modified = modify(std::move(text));
    if (!modified)
    {
        return std::unexpected(std::move(modified.error()));
    }
    Result<Infix> infix = splitFunctionDefinition(std::move(*modified));
    if (!infix)
    {
        return std::unexpected(std::move(infix.error()));
    }
    const Result<std::vector<Token>> tokens =
        tokenize(substituteUnaryOperators(infix->expression), infix->variable);
    if (!tokens)
    {
        return std::unexpected(tokens.error());
    }
    const Result<std::vector<Token>> postfix = toPostfix(*tokens);
    if (!postfix)
    {
        return std::unexpected(postfix.error());
    }
    return evaluate(*postfix);
}

}  // namespace

Result<double> ExpressionParser::parse(std::string_view expression)
{
    if (expression.size() > kMaxLength)
    {
        return fail(ErrorCode::PARSE,
                    std::format("Invalid expression: longer than {} bytes", kMaxLength));
    }
    if (Strings::toValidUtf8(expression) != expression)
    {
        return fail(ErrorCode::PARSE, "Invalid expression: the text is not valid UTF-8");
    }
    Result<double> value = parseJavaString(Strings::toUtf16(expression));
    if (!value)
    {
        value.error().message =
            std::format("Invalid expression: {} ({})", expression, value.error().message);
    }
    return value;
}

}  // namespace QtRocket
