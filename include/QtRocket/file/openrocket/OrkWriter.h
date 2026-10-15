#pragma once

#include <concepts>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/util/Config.h"

namespace QtRocket
{

/// The integer types OrkWriter::element() writes in decimal: every integral type but bool and
/// the character types (Java writes an Integer or a Long with its toString()).
template <typename T>
concept OrkInteger = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
                     !std::same_as<T, wchar_t> && !std::same_as<T, char8_t> &&
                     !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

/// The text writer of the .ork saver: OpenRocketSaver.java's writeln(), its indent counter,
/// writeElement() and writeEntry(), which write to a BufferedWriter over the output stream, and
/// TextUtil.escapeXML() as the savers call it. Every part of the saver (the document level, the
/// component savers, the simulations) writes through one writer, which appends to a string.
///
/// Lines (Java's writeln()): line() writes an empty text as a bare '\n' and any other text as
/// kIndent once per indent level, the text and '\n'. Only '\n' is written, on every platform: the
/// text is meant to be written to a file in binary mode (FileIo), so that the file has the same
/// bytes on Windows. Nothing else is done to the text: a text with embedded newlines (a comment)
/// is written as it is, so that its continuation lines are not indented, and the lines that
/// OpenRocket's component savers hand over with leading spaces of their own ("  <paint/>",
/// "    <separationconfiguration ...>") get the indentation in front of those, both as in Java.
/// A negative indent level writes no indentation (Java: Math.max(0, indent)).
///
/// Texts (decision E3 of tier 10, "never write a file that neither program can read"): every
/// text and attribute value a saver writes goes through escape(), also where OpenRocket writes it
/// without TextUtil.escapeXML() (custom expressions, a <pref> key, the photo-studio values, the
/// document materials, the keys of an extension's configuration, the attributes of a landing
/// dispersion and of a wind level). escape() makes any text safe, in this order: a byte sequence
/// that is not well-formed UTF-8 becomes U+FFFD (Strings::toValidUtf8(), as Java's decoder would
/// have read it); a character that XML 1.0 does not allow in a document, a C0 control character
/// other than tab, line feed and carriage return or one of the noncharacters U+FFFE and U+FFFF,
/// is dropped and counted (droppedCharacters()); then Strings::escapeXml() escapes the rest
/// (&, <, >, " as entities, ' and DEL as numeric references), as TextUtil.escapeXML() does. The
/// saver gives one warning for a save that dropped anything. For every text that OpenRocket's
/// own save can read back the result is OpenRocket's text, except where OpenRocket writes a text
/// unescaped and the text holds ', ", > or DEL: there escape() writes the reference
/// (&#39;, &quot;, &gt;, &#127;) where OpenRocket writes the character, which reads back the
/// same.
///
/// Deviations from OpenRocket:
/// - escape() as above: Java writes the texts named there unescaped, so that a '&' or '<' in them
///   makes a file neither program can read; it writes a control character as "&#1;", which both
///   readers refuse, U+FFFE and U+FFFF as they are, which both readers refuse too, and a lone
///   surrogate as '?' (its UTF-8 encoder).
/// - entry() escapes the key (Java writes it as it is).
/// - The text is a std::string in memory (Java streams it through a BufferedWriter). The loader
///   and the zip writer hold whole buffers anyway; GeneralRocketLoader::kMaxDocumentBytes bounds
///   what QtRocket reads back, and the saver warns beyond it.
class OrkWriter final
{
public:
    /// One level of indentation (Java: INDENT).
    static constexpr std::string_view kIndent = "  ";

    OrkWriter() = default;

    // -------------------------------------------------------------------------------- lines

    /// Java's writeln(): '\n' for an empty @p text, otherwise kIndent once per indent level,
    /// @p text and '\n'.
    void line(std::string_view text);

    /// line(""): a bare '\n'.
    void blankLine();

    /// One level more (Java: indent++).
    void indent() noexcept { ++m_indent; }

    /// One level less (Java: indent--); it may go below zero, which writes no indentation.
    void outdent() noexcept { --m_indent; }

    /// The indent level.
    [[nodiscard]] int indentLevel() const noexcept { return m_indent; }

    // -------------------------------------------------------------------------------- texts

    /// @p text made safe for the file (see the class comment): what every text and attribute
    /// value of the saver goes through. Counts the characters it drops.
    [[nodiscard]] std::string escape(std::string_view text);

    /// escape() without a writer: adds the number of characters dropped to @p dropped.
    [[nodiscard]] static std::string escapeText(std::string_view text, std::size_t& dropped);

    /// How many characters escape() has dropped so far.
    [[nodiscard]] std::size_t droppedCharacters() const noexcept { return m_dropped; }

    // ----------------------------------------------------------------------------- elements

    /// Java's writeElement(name, text): line("<name>" + escape(text) + "</name>").
    void element(std::string_view name, std::string_view text);

    /// Java's writeElement(name, Double): the value as Double.toString() prints it
    /// (Strings::javaDoubleToString()).
    void element(std::string_view name, double value);

    /// Not written as a double: a float would print with Double.toString() of the widened value
    /// where Java prints Float.toString(). No saver writes a float element.
    void element(std::string_view name, float value) = delete;

    /// Java's writeElement(name, Boolean): "true" or "false". Only a bool itself: nothing that
    /// merely converts to one (a pointer, a C string).
    template <std::same_as<bool> T>
    void element(std::string_view name, T value)
    {
        elementText(name, value ? "true" : "false");
    }

    /// Java's writeElement(name, Integer) and of a Long: the decimal digits.
    template <OrkInteger T>
    void element(std::string_view name, T value)
    {
        elementText(name, std::format("{}", value));
    }

    // ------------------------------------------------------------------------------ entries

    /// Java's writeEntry(tagName, key, value, saveNumbersWithExplicitType): one line
    /// `<tagName key="key" type="T">text</tagName>`, or without the key attribute when @p key is
    /// nullopt (Java: null; the elements of a list have none). The type and the text by the
    /// value's Java type:
    /// - Boolean: "boolean", "true" or "false";
    /// - Integer: "integer" when @p explicitNumberTypes, otherwise "number", in decimal;
    /// - Double: "double" when @p explicitNumberTypes, otherwise "number",
    ///   Strings::javaDoubleToString();
    /// - Byte, Short, Long: "number", in decimal; Float: "number", Strings::javaFloatToString();
    ///   BigDecimal: "number", BigDecimal::toString();
    /// - String: "string", the text through escape();
    /// - List: `<tagName key="key" type="list">`, then one entry per element without a key, one
    ///   level deeper, then `</tagName>`.
    /// The document preferences are written with explicit number types (<pref>), the
    /// configurations of the simulation extensions without (<entry>). The key goes through
    /// escape() (a deviation, see the class comment).
    void entry(std::string_view tagName, std::optional<std::string_view> key,
               const Config::Value& value, bool explicitNumberTypes);

    // ----------------------------------------------------------------------------- the text

    /// What has been written so far.
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    /// The number of bytes written so far.
    [[nodiscard]] std::size_t size() const noexcept { return m_text.size(); }

    /// Hands the text over, leaving the writer empty (the indent level and the count of
    /// dropped characters stay).
    [[nodiscard]] std::string release() noexcept;

private:
    /// line("<name>" + text + "</name>") of a text that needs no escaping.
    void elementText(std::string_view name, std::string_view text);

    std::string m_text;
    int         m_indent{0};
    std::size_t m_dropped{0};
};

}  // namespace QtRocket
