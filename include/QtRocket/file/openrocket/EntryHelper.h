#pragma once

#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

/// Reads the value of an entry element with a type attribute, such as
/// `<entry key="key" type="string">value</entry>` (OpenRocket's
/// file/openrocket/importt/EntryHelper).
///
/// getValueFromEntry() by the type attribute, compared exactly:
/// - "boolean": Boolean.valueOf(content), true for "true" in any case and false for everything
///   else; the content is not trimmed, so " true" is false.
/// - "string": the content as it is.
/// - "number": parseNumber(), below; a text that is no number gives no value.
/// - "list": the values of the list entry the handler opened and is closing now
///   (EntryHandler::takeNestedList()); no value when it opened none.
/// - any other type, or none: no value.
/// Whatever the type, the nested list of the handler is taken: it belongs to the entry that
/// is being closed, and no later entry may find it (see EntryHandler).
///
/// parseNumber() trims the content (String.trim()) and chooses the type as OpenRocket does, so
/// that a saver writes the number back as it was read:
/// - an optional sign and ASCII digits only: an Integer when the value fits 32 bits ("250",
///   "+5", "007", "-0"), else a Long when it fits 64 bits ("4294967296"), else a BigDecimal of
///   scale 0 (Java makes a BigInteger, which a Config turns into such a BigDecimal);
/// - anything else is read as a BigDecimal (BigDecimal::parse()), and becomes a Double when it
///   equals BigDecimal.valueOf(its double value), scale included: "2.5", ".5", "1e-1" and
///   "1.0E10" are Doubles (and "-0.0" is the Double 0.0, the sign lost on the way), while
///   "2.50", "5.", "1e1", "0.00001" and a value beyond a double's precision stay BigDecimals;
/// - no value where Java catches a NumberFormatException: a text that is no decimal number
///   ("", "abc", "NaN", "Infinity", "0x10", "1.0d"), and a decimal number beyond the double
///   range ("1e400": BigDecimal.valueOf() refuses the infinity it rounds to).
///
/// Deviations from OpenRocket:
/// - The value is a Config::Value (Java: an Object), which is what both callers store.
/// - The handler's nested list is moved out of it, where Java hands the list object out and
///   leaves it with the handler: see EntryHandler for why.
/// - BigDecimal::parse() takes ASCII digits only. Java's BigDecimal also reads the decimal
///   digits of other scripts in the second branch, so that the Arabic-Indic digit one (U+0661)
///   is the BigDecimal 1 there and no value here.
class EntryHelper final
{
public:
    EntryHelper() = delete;

    /// The value of the entry with @p attributes and @p content that @p handler is closing
    /// (getValueFromEntry()), or nullopt (Java: null) when it has none. Call it once for every
    /// entry that closes: it takes the handler's nested list.
    [[nodiscard]] static std::optional<Config::Value> getValueFromEntry(
        EntryHandler& handler, const ElementHandler::Attributes& attributes,
        std::string_view content);

    /// The number @p text stands for, with the type OpenRocket gives it (parseNumber(), private
    /// in Java), or nullopt.
    [[nodiscard]] static std::optional<Config::Value> parseNumber(std::string_view text);
};

}  // namespace QtRocket
