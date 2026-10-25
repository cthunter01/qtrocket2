#include "QtRocket/file/preset/JaxbLexical.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

// JAXB's lexical rules for the .orc DTOs, as the probe tools/openrocket-probes/orc-loader measured
// them (the case ids of its expected.txt in brackets); orc_edge_case_tests.cpp replays the probe's
// documents themselves.

namespace
{

namespace JaxbLexical = QtRocket::JaxbLexical;
using JaxbLexical::ParsedBoolean;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(JaxbLexical, WhiteSpaceIsSpaceTabLineFeedAndCarriageReturn)
{
    EXPECT_TRUE(JaxbLexical::isWhiteSpace(' '));
    EXPECT_TRUE(JaxbLexical::isWhiteSpace('\t'));
    EXPECT_TRUE(JaxbLexical::isWhiteSpace('\n'));
    EXPECT_TRUE(JaxbLexical::isWhiteSpace('\r'));
    EXPECT_FALSE(JaxbLexical::isWhiteSpace('\v'));
    EXPECT_FALSE(JaxbLexical::isWhiteSpace('\f'));
    EXPECT_FALSE(JaxbLexical::isWhiteSpace('\0'));
    // The first byte of the no-break space in UTF-8.
    EXPECT_FALSE(JaxbLexical::isWhiteSpace(std::string_view("\u00A0").front()));
    EXPECT_EQ(JaxbLexical::trim(" \t\r\n18 \n"), "18");
    EXPECT_EQ(JaxbLexical::trim("\u00A018\u00A0"), "\u00A018\u00A0");
    EXPECT_EQ(JaxbLexical::trim("\v18"), "\v18");
    EXPECT_EQ(JaxbLexical::trim(" \n"), "");
    EXPECT_TRUE(JaxbLexical::isWhiteSpaceOnly(""));
    EXPECT_TRUE(JaxbLexical::isWhiteSpaceOnly(" \t\r\n"));
    EXPECT_FALSE(JaxbLexical::isWhiteSpaceOnly(" x "));
    EXPECT_FALSE(JaxbLexical::isWhiteSpaceOnly("\u00A0"));
}

TEST(JaxbLexical, DoublesAreReadByJavasParserAfterAScreen)
{
    EXPECT_EQ(JaxbLexical::parseDouble("18"), 18.0);                              // [N00]
    EXPECT_EQ(JaxbLexical::parseDouble("18."), 18.0);                             // [N01]
    EXPECT_EQ(JaxbLexical::parseDouble("1.8E1"), 18.0);                           // [N02]
    EXPECT_EQ(JaxbLexical::parseDouble("1.8e+1"), 18.0);                          // [N03]
    EXPECT_EQ(JaxbLexical::parseDouble("0x1.2p4"), 18.0);                         // [N04]
    EXPECT_EQ(JaxbLexical::parseDouble("0x12p0"), 18.0);                          // [16]
    EXPECT_EQ(JaxbLexical::parseDouble("-0x1.8p1"), -3.0);                        // [XN08]
    EXPECT_EQ(JaxbLexical::parseDouble("  18.0 \n"), 18.0);                       // [09]
    EXPECT_EQ(JaxbLexical::parseDouble("\t18\n"), 18.0);                          // [N08]
    EXPECT_EQ(JaxbLexical::parseDouble("\r18\r"), 18.0);                          // [XN04]
    EXPECT_EQ(JaxbLexical::parseDouble("+18"), 18.0);                             // [11]
    EXPECT_EQ(JaxbLexical::parseDouble(".5"), 0.5);                               // [18]
    EXPECT_EQ(JaxbLexical::parseDouble("+.5"), 0.5);                              // [XN06]
    EXPECT_EQ(JaxbLexical::parseDouble("-.5"), -0.5);                             // [N15]
    EXPECT_EQ(JaxbLexical::parseDouble("0018"), 18.0);                            // [N22]
    EXPECT_EQ(JaxbLexical::parseDouble("1e-400"), 0.0);                           // [N27]
    EXPECT_EQ(JaxbLexical::parseDouble("4.9e-324"), 4.9e-324);                    // [N28]
    EXPECT_EQ(JaxbLexical::parseDouble("9007199254740993"), 9007199254740992.0);  // [N30]
    EXPECT_EQ(JaxbLexical::parseDouble("1e999"), kInf);                           // [80]
    EXPECT_EQ(JaxbLexical::parseDouble("1.7976931348623157e309"), kInf);          // [N32]
}

TEST(JaxbLexical, MinusZeroIsTheNegativeZero)
{
    const std::optional<double> negativeZero = JaxbLexical::parseDouble("-0");  // [19]
    // A plain if: clang-tidy's unchecked-optional-access check cannot see through ASSERT_TRUE.
    if (!negativeZero.has_value())
    {
        FAIL() << "-0 is rejected";
    }
    EXPECT_TRUE(std::signbit(*negativeZero));
}

TEST(JaxbLexical, NaNAndTheInfinitiesAreSpelledAsXmlSchemaSpellsThem)
{
    EXPECT_TRUE(std::isnan(JaxbLexical::parseDouble("NaN").value_or(0.0)));   // [12]
    EXPECT_TRUE(std::isnan(JaxbLexical::parseDouble("NaN ").value_or(0.0)));  // [N25]
    EXPECT_EQ(JaxbLexical::parseDouble("INF"), kInf);                         // [13]
    EXPECT_EQ(JaxbLexical::parseDouble(" INF "), kInf);                       // [N24]
    EXPECT_EQ(JaxbLexical::parseDouble("INF\t"), kInf);                       // [XN03]
    EXPECT_EQ(JaxbLexical::parseDouble("-INF"), -kInf);                       // [N23]
    EXPECT_EQ(JaxbLexical::parseDouble("Infinity"), std::nullopt);            // [14]
    EXPECT_EQ(JaxbLexical::parseDouble("inf"), std::nullopt);                 // [XN01]
    EXPECT_EQ(JaxbLexical::parseDouble("+INF"), std::nullopt);                // [XN00]
    EXPECT_EQ(JaxbLexical::parseDouble("+NaN"), std::nullopt);                // [N26]
    EXPECT_EQ(JaxbLexical::parseDouble("-NaN"), std::nullopt);                // [XN02]
}

TEST(JaxbLexical, DoublesAreRejected)
{
    for (const std::string_view text :
         {"abc",      "",     " ",   "\n",       "18d",      "18f",
          "18,0",     "0X12", "1 8", "--18",     "+-18",     "1e",
          "e1",       ".",    "5.-", "\u00A018", "\u200918", "\u0661\u0668",
          "18\u00A0", "1_8",  "1e+", "1\u00A0",  "\v18",     "18\v"})
    {
        EXPECT_EQ(JaxbLexical::parseDouble(text), std::nullopt) << '[' << text << ']';
    }
}

TEST(JaxbLexical, IntegersSkipWhiteSpaceAndSignsAnywhere)
{
    EXPECT_EQ(JaxbLexical::parseInt("6"), 6);      // [I00]
    EXPECT_EQ(JaxbLexical::parseInt(" 6 "), 6);    // [I01]
    EXPECT_EQ(JaxbLexical::parseInt("+6"), 6);     // [I02]
    EXPECT_EQ(JaxbLexical::parseInt("-6"), -6);    // [I03]
    EXPECT_EQ(JaxbLexical::parseInt("1 2"), 12);   // [I05]
    EXPECT_EQ(JaxbLexical::parseInt("5-"), -5);    // [I06]
    EXPECT_EQ(JaxbLexical::parseInt("--5"), -5);   // [I07]
    EXPECT_EQ(JaxbLexical::parseInt("+-6"), -6);   // [XI00]
    EXPECT_EQ(JaxbLexical::parseInt("-+6"), -6);   // [XI01]
    EXPECT_EQ(JaxbLexical::parseInt("6 -"), -6);   // [XI09]
    EXPECT_EQ(JaxbLexical::parseInt("6\t"), 6);    // [XI02]
    EXPECT_EQ(JaxbLexical::parseInt("\n6\n"), 6);  // [XI03]
    EXPECT_EQ(JaxbLexical::parseInt("\r6"), 6);    // [XI04]
    EXPECT_EQ(JaxbLexical::parseInt("0006"), 6);   // [XI10]
    EXPECT_EQ(JaxbLexical::parseInt("-0"), 0);     // [XI05]
    EXPECT_EQ(JaxbLexical::parseInt(""), 0);       // [I10]
    EXPECT_EQ(JaxbLexical::parseInt(" "), 0);      // [XI08]
    EXPECT_EQ(JaxbLexical::parseInt("+"), 0);      // [I14]
    EXPECT_EQ(JaxbLexical::parseInt("-"), 0);      // [I15]
    EXPECT_EQ(JaxbLexical::parseInt("2147483647"), std::numeric_limits<int>::max());
    EXPECT_EQ(JaxbLexical::parseInt("-2147483648"), std::numeric_limits<int>::min());
}

TEST(JaxbLexical, IntegersWrapAroundAsJavasInt)
{
    EXPECT_EQ(JaxbLexical::parseInt("2147483648"), std::numeric_limits<int>::min());   // [I08]
    EXPECT_EQ(JaxbLexical::parseInt("99999999999"), 1215752191);                       // [I09]
    EXPECT_EQ(JaxbLexical::parseInt("4294967302"), 6);                                 // [XI06]
    EXPECT_EQ(JaxbLexical::parseInt("-2147483649"), std::numeric_limits<int>::max());  // [XI07]
    // 10^29 - 1 modulo 2^32 is 2684354559, which is -1610612737 as an int.
    EXPECT_EQ(JaxbLexical::parseInt("99999999999999999999999999999"), -1610612737);
}

TEST(JaxbLexical, IntegersAreRejectedForAnyOtherCharacter)
{
    for (const std::string_view text :
         {"6.0", "abc", "0x6", "\u0666", "6e0", "\u00A06", "\v6", "6_000", "1,000"})
    {
        EXPECT_EQ(JaxbLexical::parseInt(text), std::nullopt) << '[' << text << ']';
    }
}

TEST(JaxbLexical, BooleansAreReadFromTheirFirstCharacter)
{
    EXPECT_EQ(JaxbLexical::parseBoolean("true"), ParsedBoolean::IS_TRUE);      // [B00]
    EXPECT_EQ(JaxbLexical::parseBoolean("false"), ParsedBoolean::IS_FALSE);    // [B01]
    EXPECT_EQ(JaxbLexical::parseBoolean("1"), ParsedBoolean::IS_TRUE);         // [B02]
    EXPECT_EQ(JaxbLexical::parseBoolean("0"), ParsedBoolean::IS_FALSE);        // [B03]
    EXPECT_EQ(JaxbLexical::parseBoolean(" true "), ParsedBoolean::IS_TRUE);    // [B04]
    EXPECT_EQ(JaxbLexical::parseBoolean("\ntrue\n"), ParsedBoolean::IS_TRUE);  // [B05]
    EXPECT_EQ(JaxbLexical::parseBoolean(" 1 "), ParsedBoolean::IS_TRUE);       // [B19]
    EXPECT_EQ(JaxbLexical::parseBoolean("\r1"), ParsedBoolean::IS_TRUE);       // [XB06]
    EXPECT_EQ(JaxbLexical::parseBoolean("true\t"), ParsedBoolean::IS_TRUE);    // [XB07]
    EXPECT_EQ(JaxbLexical::parseBoolean("false "), ParsedBoolean::IS_FALSE);   // [XB08]
    EXPECT_EQ(JaxbLexical::parseBoolean("1\n"), ParsedBoolean::IS_TRUE);       // [XB10]
    // Any other first character is false.
    EXPECT_EQ(JaxbLexical::parseBoolean("2"), ParsedBoolean::IS_FALSE);          // [B14]
    EXPECT_EQ(JaxbLexical::parseBoolean("x"), ParsedBoolean::IS_FALSE);          // [XB03]
    EXPECT_EQ(JaxbLexical::parseBoolean("x "), ParsedBoolean::IS_FALSE);         // [XB04]
    EXPECT_EQ(JaxbLexical::parseBoolean(" 2 "), ParsedBoolean::IS_FALSE);        // [XB05]
    EXPECT_EQ(JaxbLexical::parseBoolean("Z"), ParsedBoolean::IS_FALSE);          // [XB11]
    EXPECT_EQ(JaxbLexical::parseBoolean("-"), ParsedBoolean::IS_FALSE);          // [XB15]
    EXPECT_EQ(JaxbLexical::parseBoolean("\u00A0"), ParsedBoolean::IS_FALSE);     // [XB33]
    EXPECT_EQ(JaxbLexical::parseBoolean("\u00E9"), ParsedBoolean::IS_FALSE);     // [XB35]
    EXPECT_EQ(JaxbLexical::parseBoolean("\U0001F680"), ParsedBoolean::IS_NULL);  // [XB34]
    // White space only is false, an empty text null.
    EXPECT_EQ(JaxbLexical::parseBoolean(" "), ParsedBoolean::IS_FALSE);     // [XB00]
    EXPECT_EQ(JaxbLexical::parseBoolean("\t\t"), ParsedBoolean::IS_FALSE);  // [XB01]
    EXPECT_EQ(JaxbLexical::parseBoolean("\n"), ParsedBoolean::IS_FALSE);    // [XB02]
    EXPECT_EQ(JaxbLexical::parseBoolean(""), ParsedBoolean::IS_NULL);       // [B15]
}

TEST(JaxbLexical, BooleansAreNullForWhatFollowsTheirValue)
{
    for (const std::string_view text :
         {"True", "tr", "truex", "true1", "fals",    "falsey",    "yes",
          "01",   "10", "0 0",   "t rue", "\u00A01", "falsetrue", "trux",
          "tru",  "fa", "fal",   "tx",    "fx",      "t ",        "TRUE"})
    {
        EXPECT_EQ(JaxbLexical::parseBoolean(text), ParsedBoolean::IS_NULL) << '[' << text << ']';
    }
}

TEST(JaxbLexical, BooleansAreRejectedWhenTheTextEndsAfterATOrAnF)
{
    // [B07], [B11], [XB14], [XB20], [XB26], [XB31]: rejected, so a field keeps its value.
    for (const std::string_view text : {"t", " t", "f", " f", "\nt"})
    {
        EXPECT_EQ(JaxbLexical::parseBoolean(text), ParsedBoolean::REJECTED) << '[' << text << ']';
    }
}

TEST(JaxbLexical, EnumsMatchTheirNamesExactly)
{
    constexpr std::array<std::string_view, 6> kShapes{"CONICAL", "OGIVE",     "ELLIPSOID",
                                                      "POWER",   "PARABOLIC", "HAACK"};
    EXPECT_EQ(JaxbLexical::parseEnum("OGIVE", kShapes), 1U);  // [S00]
    EXPECT_EQ(JaxbLexical::parseEnum("HAACK", kShapes), 5U);  // [S05]
    EXPECT_EQ(JaxbLexical::parseEnum("POWER", kShapes), 3U);  // [S06]
    for (const std::string_view text :
         {" OGIVE ", "\nOGIVE\n", "OGIVE\u00A0", "Ogive", "ogive", "BLUNT", "", "OGIVE "})
    {
        EXPECT_EQ(JaxbLexical::parseEnum(text, kShapes), std::nullopt) << '[' << text << ']';
    }
}

/// The bytes @p values, for comparing with parseBase64Binary().
[[nodiscard]] std::vector<std::byte> bytesOf(std::initializer_list<unsigned int> values)
{
    std::vector<std::byte> bytes;
    for (const unsigned int value : values)
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

TEST(JaxbLexical, Base64TakesGroupsOfFourCharacters)
{
    using Bytes = std::optional<std::vector<std::byte>>;
    EXPECT_EQ(JaxbLexical::parseBase64Binary("aGVsbG8="),
              Bytes(bytesOf({0x68, 0x65, 0x6c, 0x6c, 0x6f})));                  // [75]
    EXPECT_EQ(JaxbLexical::parseBase64Binary(""), Bytes(bytesOf({})));          // [XH03]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QQ=="), Bytes(bytesOf({0x41})));  // [XH04]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJD"),
              Bytes(bytesOf({0x41, 0x42, 0x43})));  // [XH09]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("4pyI"),
              Bytes(bytesOf({0xe2, 0x9c, 0x88})));                                    // [XH13]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QU=="), Bytes(bytesOf({0x41})));        // [XH14]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUI="), Bytes(bytesOf({0x41, 0x42})));  // [XH15]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJDQQ=="),
              Bytes(bytesOf({0x41, 0x42, 0x43, 0x41})));                           // [XH16]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("AAAA"), Bytes(bytesOf({0, 0, 0})));  // [XH22]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("////"),
              Bytes(bytesOf({0xff, 0xff, 0xff})));  // [XH23]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("++++"),
              Bytes(bytesOf({0xfb, 0xef, 0xbe})));                              // [XH24]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("AA=="), Bytes(bytesOf({0x00})));  // [XH31]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJDQUJD"),
              Bytes(bytesOf({0x41, 0x42, 0x43, 0x41, 0x42, 0x43})));                  // [XH33]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QR=="), Bytes(bytesOf({0x41})));        // [XH35]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUK="), Bytes(bytesOf({0x41, 0x42})));  // [XH36]
}

TEST(JaxbLexical, Base64PaddingsCountAs127AndEndNothing)
{
    using Bytes = std::optional<std::vector<std::byte>>;
    EXPECT_EQ(JaxbLexical::parseBase64Binary("===="), Bytes(bytesOf({0xff})));  // [XH08]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("Q==="), Bytes(bytesOf({0x47})));  // [XH17]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("=QUJ"),
              Bytes(bytesOf({0xfd, 0x05, 0x09})));                                    // [XH18]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QU=D"), Bytes(bytesOf({0x41, 0xc3})));  // [XH19]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QQ==QUJD"),
              Bytes(bytesOf({0x41, 0x41, 0x42, 0x43})));  // [XH20]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJD===="),
              Bytes(bytesOf({0x41, 0x42, 0x43, 0xff})));                              // [XH21]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QQ=A"), Bytes(bytesOf({0x41, 0xc0})));  // [XH29]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("A==="), Bytes(bytesOf({0x07})));        // [XH30]
}

TEST(JaxbLexical, Base64SkipsOtherCharactersAndDropsAnIncompleteGroup)
{
    using Bytes = std::optional<std::vector<std::byte>>;
    EXPECT_EQ(JaxbLexical::parseBase64Binary("-_-_"), Bytes(bytesOf({})));  // [XH25]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QU!D"), Bytes(bytesOf({})));  // [XH26]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QU D"), Bytes(bytesOf({})));  // [XH27]
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJD QU!D"), std::nullopt);
    EXPECT_EQ(JaxbLexical::parseBase64Binary("QUJD!QUJ"), Bytes(bytesOf({0x41, 0x42, 0x43})));
}

TEST(JaxbLexical, Base64RejectsALengthThatIsNoMultipleOfFourAndNonAsciiCharacters)
{
    // [76], [XH00] to [XH02], [XH05] to [XH07], [XH10] to [XH12], [XH28], [XH32], [XH34].
    for (const std::string_view text :
         {"!!!", "aGVsbG8", "aGVs bG8=", "aGVsbG8=\n", "QQ", "Q", "aGV!sbG8=", " QUJD ",
          "QUJD=", "QU=JD", "QQ=", "QUJ\u00E9", "QUJD\tQUJD", "\u00E9\u00E9"})
    {
        EXPECT_EQ(JaxbLexical::parseBase64Binary(text), std::nullopt) << '[' << text << ']';
    }
}

}  // namespace
