#include "QtRocket/file/openrocket/OrkWriter.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/EntryHelper.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/JavaRandom.h"

// OrkWriter: the text writer of the .ork saver (OpenRocketSaver.java's writeln(), writeElement()
// and writeEntry()). The texts expected are OpenRocket's own, written by those private methods
// through reflection (the probe of tier 10a S1, DocLevelProbe "writeln" and "entries", at the
// indent level the test sets), except where decision E3 of tier 10 makes QtRocket escape what
// OpenRocket does not; those cases say what OpenRocket writes.

namespace
{

using QtRocket::BigDecimal;
using QtRocket::Config;
using QtRocket::EntryHelper;
using QtRocket::OrkWriter;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// A writer at indent level @p level.
[[nodiscard]] OrkWriter writerAt(int level)
{
    OrkWriter writer;
    for (int i = 0; i < level; ++i)
    {
        writer.indent();
    }
    return writer;
}

// Java's writeln() and writeElement() at indent level 3: an empty line is a bare newline; a text
// with newlines in it keeps its continuation lines unindented; the lines the component savers
// hand over with two or four spaces of their own get the indentation in front of those;
// doubles, ints and booleans are written as Java writes them; a text is escaped; a negative
// level writes no indentation.
TEST(OrkWriter, WritesLinesAndElementsAsOpenRocketsWriteln)
{
    OrkWriter writer = writerAt(3);
    writer.blankLine();
    writer.line("<comment>first\nsecond\n  third</comment>");
    writer.line("  <paint/>");
    writer.line("    <separationconfiguration/>");
    writer.element("d", 0.1);
    writer.element("i", 7);
    writer.element("b", true);
    writer.element("s", "x<&\"'>\x7f\t");
    writer.element("n", "");
    writer.outdent();
    writer.outdent();
    writer.outdent();
    writer.outdent();
    writer.outdent();
    EXPECT_EQ(writer.indentLevel(), -2);
    writer.line("<negative/>");
    EXPECT_EQ(writer.text(),
              "\n      <comment>first\nsecond\n  third</comment>\n        <paint/>\n          "
              "<separationconfiguration/>\n      <d>0.1</d>\n      <i>7</i>\n      "
              "<b>true</b>\n      <s>x&lt;&amp;&quot;&#39;&gt;&#127;\t</s>\n      <n></n>\n"
              "<negative/>\n");
    EXPECT_EQ(writer.droppedCharacters(), 0U);
}

TEST(OrkWriter, LineIsTheTextIndentedByTwoSpacesPerLevel)
{
    OrkWriter writer;
    writer.line("<a>");
    writer.indent();
    writer.line("<b>");
    writer.indent();
    writer.line("<c/>");
    writer.outdent();
    writer.line("</b>");
    writer.line("");
    writer.outdent();
    writer.line("</a>");
    EXPECT_EQ(writer.text(), "<a>\n  <b>\n    <c/>\n  </b>\n\n</a>\n");
    EXPECT_EQ(writer.size(), writer.text().size());
    EXPECT_EQ(OrkWriter::kIndent, "  ");
}

TEST(OrkWriter, ElementsOfEveryKindOfNumber)
{
    OrkWriter writer;
    writer.element("double", 1.0E7);
    writer.element("small", 1.0E-4);
    writer.element("nan", kNaN);
    writer.element("inf", -kInf);
    writer.element("negativezero", -0.0);
    writer.element("long", std::int64_t{-4294967296});
    writer.element("size", std::size_t{12});
    writer.element("false", false);
    EXPECT_EQ(writer.text(),
              "<double>1.0E7</double>\n<small>1.0E-4</small>\n<nan>NaN</nan>\n"
              "<inf>-Infinity</inf>\n<negativezero>-0.0</negativezero>\n"
              "<long>-4294967296</long>\n<size>12</size>\n<false>false</false>\n");
}

TEST(OrkWriter, ReleaseHandsTheTextOver)
{
    OrkWriter writer = writerAt(1);
    writer.line("<x/>");
    const std::string text = writer.release();
    EXPECT_EQ(text, "  <x/>\n");
    EXPECT_EQ(writer.text(), "");
    EXPECT_EQ(writer.indentLevel(), 1);
    writer.line("<y/>");
    EXPECT_EQ(writer.text(), "  <y/>\n");
}

/// The BigDecimal of @p text.
[[nodiscard]] BigDecimal big(std::string_view text)
{
    return BigDecimal::parse(text).value_or(BigDecimal{});
}

/// A value of each Java type of DocLevelProbe "entries", in its order.
[[nodiscard]] std::vector<Config::Value> probeValues()
{
    return {
        Config::Value(true),
        Config::Value(false),
        Config::Value(5),
        Config::Value(-7),
        Config::Value(std::int64_t{5}),
        Config::Value(std::int64_t{4294967296}),
        Config::Value(std::int8_t{7}),
        Config::Value(std::int16_t{-300}),
        Config::Value(1.5F),
        Config::Value(1.0E10F),
        Config::Value(0.1F),
        Config::Value(std::numeric_limits<float>::quiet_NaN()),
        Config::Value(5.0),
        Config::Value(1.0E-5),
        Config::Value(0.1),
        Config::Value(1.0E7),
        Config::Value(kNaN),
        Config::Value(kInf),
        Config::Value(-0.0),
        Config::Value(big("0.10")),
        Config::Value(big("1E+3")),
        Config::Value(big("123456789012345678901234567890")),
        Config::Value("plain"),
        Config::Value("a<b&c\"d'e>f"),
        Config::Value(""),
        Config::Value("two\nlines"),
        Config::Value("tab\there"),
    };
}

/// The entries of probeValues() with the key "key" at indent level 2.
[[nodiscard]] std::string probeEntries(bool explicitNumberTypes)
{
    OrkWriter writer = writerAt(2);
    for (const Config::Value& value : probeValues())
    {
        writer.entry("entry", "key", value, explicitNumberTypes);
    }
    return writer.text();
}

/// OpenRocket's writeEntry("entry", "key", value, false) of probeValues(): every number is of
/// type "number".
constexpr std::string_view kEntriesWithoutExplicitTypes =
    "    <entry key=\"key\" type=\"boolean\">true</entry>\n"
    "    <entry key=\"key\" type=\"boolean\">false</entry>\n"
    "    <entry key=\"key\" type=\"number\">5</entry>\n"
    "    <entry key=\"key\" type=\"number\">-7</entry>\n"
    "    <entry key=\"key\" type=\"number\">5</entry>\n"
    "    <entry key=\"key\" type=\"number\">4294967296</entry>\n"
    "    <entry key=\"key\" type=\"number\">7</entry>\n"
    "    <entry key=\"key\" type=\"number\">-300</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.5</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.0E10</entry>\n"
    "    <entry key=\"key\" type=\"number\">0.1</entry>\n"
    "    <entry key=\"key\" type=\"number\">NaN</entry>\n"
    "    <entry key=\"key\" type=\"number\">5.0</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.0E-5</entry>\n"
    "    <entry key=\"key\" type=\"number\">0.1</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.0E7</entry>\n"
    "    <entry key=\"key\" type=\"number\">NaN</entry>\n"
    "    <entry key=\"key\" type=\"number\">Infinity</entry>\n"
    "    <entry key=\"key\" type=\"number\">-0.0</entry>\n"
    "    <entry key=\"key\" type=\"number\">0.10</entry>\n"
    "    <entry key=\"key\" type=\"number\">1E+3</entry>\n"
    "    <entry key=\"key\" type=\"number\">123456789012345678901234567890</entry>\n"
    "    <entry key=\"key\" type=\"string\">plain</entry>\n"
    "    <entry key=\"key\" type=\"string\">a&lt;b&amp;c&quot;d&#39;e&gt;f</entry>\n"
    "    <entry key=\"key\" type=\"string\"></entry>\n"
    "    <entry key=\"key\" type=\"string\">two\nlines</entry>\n"
    "    <entry key=\"key\" type=\"string\">tab\there</entry>\n";

/// The same with explicit number types: an Integer is "integer" and a Double "double"; Long,
/// Byte, Short, Float and BigDecimal stay "number".
constexpr std::string_view kEntriesWithExplicitTypes =
    "    <entry key=\"key\" type=\"boolean\">true</entry>\n"
    "    <entry key=\"key\" type=\"boolean\">false</entry>\n"
    "    <entry key=\"key\" type=\"integer\">5</entry>\n"
    "    <entry key=\"key\" type=\"integer\">-7</entry>\n"
    "    <entry key=\"key\" type=\"number\">5</entry>\n"
    "    <entry key=\"key\" type=\"number\">4294967296</entry>\n"
    "    <entry key=\"key\" type=\"number\">7</entry>\n"
    "    <entry key=\"key\" type=\"number\">-300</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.5</entry>\n"
    "    <entry key=\"key\" type=\"number\">1.0E10</entry>\n"
    "    <entry key=\"key\" type=\"number\">0.1</entry>\n"
    "    <entry key=\"key\" type=\"number\">NaN</entry>\n"
    "    <entry key=\"key\" type=\"double\">5.0</entry>\n"
    "    <entry key=\"key\" type=\"double\">1.0E-5</entry>\n"
    "    <entry key=\"key\" type=\"double\">0.1</entry>\n"
    "    <entry key=\"key\" type=\"double\">1.0E7</entry>\n"
    "    <entry key=\"key\" type=\"double\">NaN</entry>\n"
    "    <entry key=\"key\" type=\"double\">Infinity</entry>\n"
    "    <entry key=\"key\" type=\"double\">-0.0</entry>\n"
    "    <entry key=\"key\" type=\"number\">0.10</entry>\n"
    "    <entry key=\"key\" type=\"number\">1E+3</entry>\n"
    "    <entry key=\"key\" type=\"number\">123456789012345678901234567890</entry>\n"
    "    <entry key=\"key\" type=\"string\">plain</entry>\n"
    "    <entry key=\"key\" type=\"string\">a&lt;b&amp;c&quot;d&#39;e&gt;f</entry>\n"
    "    <entry key=\"key\" type=\"string\"></entry>\n"
    "    <entry key=\"key\" type=\"string\">two\nlines</entry>\n"
    "    <entry key=\"key\" type=\"string\">tab\there</entry>\n";

TEST(OrkWriter, WritesAnEntryOfEveryTypeAsOpenRocket)
{
    EXPECT_EQ(probeValues().size(), 27U);
    EXPECT_EQ(probeEntries(false), kEntriesWithoutExplicitTypes);
    EXPECT_EQ(probeEntries(true), kEntriesWithExplicitTypes);
}

/// The list of DocLevelProbe: [1, "x&y", [true, 2.5], []].
[[nodiscard]] Config::Value probeList()
{
    return Config::Value(Config::List{
        Config::Value(1),
        Config::Value("x&y"),
        Config::Value(Config::List{Config::Value(true), Config::Value(2.5)}),
        Config::Value(Config::List{}),
    });
}

// A list is an entry of type "list" whose elements are entries without a key, one level deeper;
// an empty list is the open and the close tag.
TEST(OrkWriter, WritesAListAsNestedEntriesWithoutKeys)
{
    OrkWriter entries = writerAt(2);
    entries.entry("entry", "list", probeList(), false);
    EXPECT_EQ(entries.text(),
              "    <entry key=\"list\" type=\"list\">\n"
              "      <entry type=\"number\">1</entry>\n"
              "      <entry type=\"string\">x&amp;y</entry>\n"
              "      <entry type=\"list\">\n"
              "        <entry type=\"boolean\">true</entry>\n"
              "        <entry type=\"number\">2.5</entry>\n"
              "      </entry>\n"
              "      <entry type=\"list\">\n"
              "      </entry>\n"
              "    </entry>\n");
    EXPECT_EQ(entries.indentLevel(), 2);

    OrkWriter preferences = writerAt(2);
    preferences.entry("pref", "list", probeList(), true);
    EXPECT_EQ(preferences.text(),
              "    <pref key=\"list\" type=\"list\">\n"
              "      <pref type=\"integer\">1</pref>\n"
              "      <pref type=\"string\">x&amp;y</pref>\n"
              "      <pref type=\"list\">\n"
              "        <pref type=\"boolean\">true</pref>\n"
              "        <pref type=\"double\">2.5</pref>\n"
              "      </pref>\n"
              "      <pref type=\"list\">\n"
              "      </pref>\n"
              "    </pref>\n");
}

// Without a key there is no key attribute. A key with markup in it is escaped (decision E3):
// OpenRocket writes `key="k"q<&"`, which neither program can read.
TEST(OrkWriter, EscapesTheKeyOfAnEntry)
{
    OrkWriter writer = writerAt(2);
    writer.entry("entry", std::nullopt, Config::Value("no key"), false);
    writer.entry("entry", "k\"q<&", Config::Value("markup in the key"), false);
    EXPECT_EQ(writer.text(),
              "    <entry type=\"string\">no key</entry>\n"
              "    <entry key=\"k&quot;q&lt;&amp;\" type=\"string\">markup in the key</entry>\n");
}

// Decision E3, first case: what TextUtil.escapeXML() escapes is escaped as there; tab, line
// feed and carriage return stay; DEL is a reference, which both readers take.
TEST(OrkWriter, EscapeEscapesMarkupAsTextUtil)
{
    OrkWriter writer;
    EXPECT_EQ(writer.escape("a<b&c\"d'e>f"), "a&lt;b&amp;c&quot;d&#39;e&gt;f");
    EXPECT_EQ(writer.escape("tab\tline\nreturn\r"), "tab\tline\nreturn\r");
    EXPECT_EQ(writer.escape("del\x7f"), "del&#127;");
    EXPECT_EQ(writer.escape("&amp;"), "&amp;amp;");
    EXPECT_EQ(writer.escape(""), "");
    // UTF-8 text passes as it is: U+00E9, U+00B0, U+20AC, U+1F680, U+D7FF, U+E000, U+FFFD,
    // U+10FFFF.
    const std::string_view text =
        "\xC3\xA9\xC2\xB0\xE2\x82\xAC\xF0\x9F\x9A\x80\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBD"
        "\xF4\x8F\xBF\xBF";
    EXPECT_EQ(writer.escape(text), text);
    EXPECT_EQ(writer.droppedCharacters(), 0U);
}

// Decision E3, second case: a control character that XML 1.0 forbids (every C0 control but tab,
// line feed and carriage return), and the noncharacters U+FFFE and U+FFFF, are dropped and
// counted. OpenRocket writes "&#1;" and the noncharacters as they are; both readers refuse
// either.
TEST(OrkWriter, EscapeDropsWhatAnXmlDocumentCannotHold)
{
    OrkWriter              writer;
    const std::string_view controls(
        "a\x01"
        "b\x1f"
        "c\x00"
        "d\x0b"
        "e\x0c"
        "f",
        11);
    EXPECT_EQ(writer.escape(controls), "abcdef");
    EXPECT_EQ(writer.droppedCharacters(), 5U);
    EXPECT_EQ(writer.escape("x\xEF\xBF\xBEy\xEF\xBF\xBFz"), "xyz");
    EXPECT_EQ(writer.droppedCharacters(), 7U);
    // The count goes on over every text the writer escapes, elements and entries included.
    writer.element("e", "\x02");
    writer.entry("entry", "k\x03", Config::Value("v\x04"), false);
    EXPECT_EQ(writer.text(), "<e></e>\n<entry key=\"k\" type=\"string\">v</entry>\n");
    EXPECT_EQ(writer.droppedCharacters(), 10U);

    std::size_t dropped = 4;
    EXPECT_EQ(OrkWriter::escapeText("\x05<\x06", dropped), "&lt;");
    EXPECT_EQ(dropped, 6U);
}

// Decision E3, third case: bytes that are no well-formed UTF-8 become U+FFFD, as Java's decoder
// reads them (Strings::toValidUtf8()); nothing is dropped.
TEST(OrkWriter, EscapeReplacesMalformedUtf8)
{
    OrkWriter writer;
    EXPECT_EQ(writer.escape("a\xFF"
                            "b"),
              "a\xEF\xBF\xBD"
              "b");
    EXPECT_EQ(writer.escape("cut\xC3"), "cut\xEF\xBF\xBD");
    // An encoded surrogate (CESU-8) is one replacement for its three bytes.
    EXPECT_EQ(writer.escape("\xED\xA0\x80<"), "\xEF\xBF\xBD&lt;");
    // A code point beyond U+10FFFF.
    EXPECT_EQ(writer.escape("\xF4\x90\x80\x80"),
              "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
    EXPECT_EQ(writer.droppedCharacters(), 0U);
}

// sanitizeText() is escape() without the escaping: what XML cannot hold is dropped and counted,
// malformed UTF-8 is U+FFFD, the markup stays (the RockSim engine files of an archive).
TEST(OrkWriter, SanitizeTextLeavesTheMarkupAsItIs)
{
    std::size_t dropped = 1;
    EXPECT_EQ(OrkWriter::sanitizeText("<a b=\"&amp;\">\tx\x01y\xFF</a>\n", dropped),
              "<a b=\"&amp;\">\txy\xEF\xBF\xBD</a>\n");
    EXPECT_EQ(dropped, 2U);
    EXPECT_EQ(OrkWriter::sanitizeText("", dropped), "");
    EXPECT_EQ(dropped, 2U);
}

// The name under which a decal image is stored, in the archive and in rocket.ork alike: as
// sanitizeText() gives it, with a backslash as a slash, which a zip entry cannot hold; tab, line
// feed and carriage return stay. The attribute writes those three as character references,
// which an XML reader reads back as they are (as they are, it turns them into spaces), and
// escapes the markup.
TEST(OrkWriter, NamesADecalImageAsTheArchiveHoldsIt)
{
    std::size_t dropped = 0;
    EXPECT_EQ(OrkWriter::decalImageName("decals/a.png", dropped), "decals/a.png");
    EXPECT_EQ(OrkWriter::decalImageName("decals/caf\xE9.png", dropped),
              "decals/caf\xEF\xBF\xBD.png");
    EXPECT_EQ(OrkWriter::decalImageName("decals\\sub\\b.png", dropped), "decals/sub/b.png");
    EXPECT_EQ(dropped, 0U);
    EXPECT_EQ(OrkWriter::decalImageName("decals/c\x01\t\n\r&.png", dropped), "decals/c\t\n\r&.png");
    EXPECT_EQ(dropped, 1U);

    OrkWriter writer;
    EXPECT_EQ(writer.decalName("decals/c\x01\t\n\r<&>\".png"),
              "decals/c&#9;&#10;&#13;&lt;&amp;&gt;&quot;.png");
    EXPECT_EQ(writer.decalName("decals\\b.png"), "decals/b.png");
    EXPECT_EQ(writer.droppedCharacters(), 1U);
}

/// The text of the one entry @p writer wrote: what stands between "> and </entry>.
[[nodiscard]] std::string entryContent(const std::string& entry)
{
    const std::size_t open  = entry.find('>');
    const std::size_t close = entry.rfind("</entry>");
    return entry.substr(open + 1, close - open - 1);
}

/// What OrkWriter::entry() writes of the Double @p value, read back as the .ork reader reads a
/// number (EntryHelper::parseNumber()): nullopt unless it is a Double.
[[nodiscard]] std::optional<double> doubleThroughEntry(double value)
{
    OrkWriter writer;
    writer.entry("entry", "key", Config::Value(value), false);
    const std::optional<Config::Value> read = EntryHelper::parseNumber(entryContent(writer.text()));
    if (!read.has_value())
    {
        return std::nullopt;
    }
    const double* d = std::get_if<double>(&read->variant());
    return d != nullptr ? std::optional<double>(*d) : std::nullopt;
}

/// The doubles of @p values that do not come back as the same Double through
/// doubleThroughEntry().
[[nodiscard]] std::vector<std::string> doublesThatChangeType(const std::vector<double>& values)
{
    std::vector<std::string> changed;
    for (const double value : values)
    {
        const std::optional<double> back = doubleThroughEntry(value);
        if (!back.has_value() ||
            std::bit_cast<std::uint64_t>(*back) != std::bit_cast<std::uint64_t>(value))
        {
            changed.push_back(std::format("{:016x}", std::bit_cast<std::uint64_t>(value)));
        }
    }
    return changed;
}

/// 20000 finite doubles of random bits.
[[nodiscard]] std::vector<double> randomDoubles()
{
    QtRocket::JavaRandom random(10);
    std::vector<double>  values;
    while (values.size() < 20000)
    {
        const auto high  = static_cast<std::uint32_t>(random.nextInt());
        const auto low   = static_cast<std::uint32_t>(random.nextInt());
        const auto value = std::bit_cast<double>((std::uint64_t{high} << 32U) | low);
        if (std::isfinite(value) && value != 0.0)
        {
            values.push_back(value);
        }
    }
    return values;
}

// Decision O9 of tier 10: an extension's Double is written with Strings::javaDoubleToString()
// and BigDecimal::valueOfDouble() follows the same digits, so the reader takes the text back as
// the same Double. With JDK 17's digits (BigDecimal.valueOf() as OpenRocket's releases have it)
// these doubles, among others, came back as BigDecimals: 1.0E23, 2^60, 2^-61, 6.84232346E19
// and twice Double.MIN_VALUE.
TEST(OrkWriter, ADoubleOfAnEntryReadsBackAsTheSameDouble)
{
    const double smallest = std::numeric_limits<double>::denorm_min();
    EXPECT_EQ(doublesThatChangeType(
                  {1.0E23, std::ldexp(1.0, 60), std::ldexp(1.0, -61), 6.84232346E19, 2 * smallest,
                   smallest, std::numeric_limits<double>::max(), std::numeric_limits<double>::min(),
                   0.1, 5.0, -2.5, 1.0E7, 9999999.0, 0.001, 1.0E-4}),
              std::vector<std::string>{});
    EXPECT_EQ(doublesThatChangeType(randomDoubles()), std::vector<std::string>{});
    // -0.0 is "-0.0", which the reader takes as the Double 0.0, as OpenRocket's does.
    EXPECT_EQ(doubleThroughEntry(-0.0), 0.0);
    // JDK 17's text of 1.0E23 is no Double for the reader any more.
    const std::optional<Config::Value> jdk17 = EntryHelper::parseNumber("9.999999999999999E22");
    EXPECT_TRUE(jdk17.has_value() && std::holds_alternative<BigDecimal>(jdk17->variant()));
}

}  // namespace
