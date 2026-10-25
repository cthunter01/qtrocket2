#include "file/preset/OrcEdgeCases.h"

#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace QtRocket::Test
{

namespace
{

// The probe's constants and builders (OrcEdgeProbe.HEAD, MATS, DIMS, doc(), tube()).

constexpr std::string_view kHead =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
constexpr std::string_view kMats =
    R"(<Materials><Material UnitsOfMeasure="kg/m3"><Name>Cardboard</Name><Density>680.0</Density>)"
    R"(<Type>BULK</Type></Material><Material UnitsOfMeasure="g/cm3"><Name>Paper test</Name>)"
    "<Density>0.8</Density><Type>BULK</Type></Material></Materials>";
constexpr std::string_view kDims =
    R"(<InsideDiameter Unit="in">0.95</InsideDiameter><OutsideDiameter Unit="in">0.976)"
    R"(</OutsideDiameter><Length Unit="in">18.0</Length>)";

[[nodiscard]] std::string doc(std::string_view components)
{
    return std::string(kHead) + "<OpenRocketComponent><Version>0.1</Version>" + std::string(kMats) +
           "<Components>" + std::string(components) + "</Components></OpenRocketComponent>";
}

[[nodiscard]] std::string tube(std::string_view inner)
{
    return "<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>BT-X</PartNumber>"
           R"(<Description>d</Description><Material Type="BULK">Cardboard</Material>)" +
           std::string(inner) + "</BodyTube>";
}

/// Java's String.replace(target, replacement): every occurrence, from the left.
[[nodiscard]] std::string replace(std::string_view text, std::string_view target,
                                  std::string_view replacement)
{
    std::string result;
    std::size_t start = 0;
    for (std::size_t found = text.find(target); found != std::string_view::npos;
         found             = text.find(target, start))
    {
        result.append(text.substr(start, found - start)).append(replacement);
        start = found + target.size();
    }
    return result.append(text.substr(start));
}

/// What the probe writes as escapes when it names a case after its text.
enum class NameEscapes
{
    NONE,
    LINE_FEEDS,           ///< "\n"
    LINE_FEEDS_AND_TABS,  ///< "\n" and "\t"
};

/// @p text as the probe shows it in the name of a case.
[[nodiscard]] std::string shown(std::string_view text, NameEscapes escapes)
{
    if (escapes == NameEscapes::NONE)
    {
        return std::string(text);
    }
    const std::string lines = replace(text, "\n", "\\n");
    return escapes == NameEscapes::LINE_FEEDS ? lines : replace(lines, "\t", "\\t");
}

/// The bytes Java's String.getBytes(StandardCharsets.UTF_16) makes of the ASCII text @p text: a
/// byte order mark FE FF, then each character big-endian.
[[nodiscard]] std::string utf16(std::string_view text)
{
    std::string bytes = "\xFE\xFF";
    for (const char c : text)
    {
        bytes += '\0';
        bytes += c;
    }
    return bytes;
}

class Cases
{
public:
    void add(std::string name, std::string bytes)
    {
        std::string id = name.substr(0, name.find(' '));
        m_cases.push_back(
            OrcEdgeCase{.id = std::move(id), .name = std::move(name), .bytes = std::move(bytes)});
    }

    [[nodiscard]] std::vector<OrcEdgeCase> take() { return std::move(m_cases); }

private:
    std::vector<OrcEdgeCase> m_cases;
};

/// Adds a case for each of @p texts, as the probe's loops do: named "<prefix><index> <kind>
/// [<text>]" (the index of two digits, the text shown()), its document made by @p document.
template <class Document>
void addFamily(Cases& cases, std::string_view prefix, std::string_view kind,
               std::span<const std::string_view> texts, NameEscapes escapes, Document document)
{
    std::size_t index = 0;
    for (const std::string_view text : texts)
    {
        cases.add(std::format("{}{:02} {} [{}]", prefix, index, kind, shown(text, escapes)),
                  document(text));
        index++;
    }
}

constexpr std::string_view kLength18In = R"(<Length Unit="in">18.0</Length>)";

/// A body tube whose Length of the unit "m" has the text @p text (batches 2 and 3).
[[nodiscard]] std::string tubeOfLength(std::string_view text)
{
    return doc(tube(
        replace(kDims, kLength18In, R"(<Length Unit="m">)" + std::string(text) + "</Length>")));
}

constexpr std::string_view kBatch2Chute =
    "<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Material "
    R"(Type="SURFACE">Ripstop nylon</Material><Diameter>0.3</Diameter><Sides>SIDES</Sides>)"
    "<LineCount>6</LineCount><LineLength>0.3</LineLength></Parachute>";
constexpr std::string_view kBulkHead =
    "<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>FILLED"
    "</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>";
constexpr std::string_view kNoseCone =
    "<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material "
    R"(Type="BULK">Cardboard</Material><Shape>SHAPE</Shape><OutsideDiameter>0.02)"
    "</OutsideDiameter><ShoulderDiameter>0.019</ShoulderDiameter><ShoulderLength>0.01"
    "</ShoulderLength><Length>0.05</Length></NoseCone>";

/// The parachute of batches 2 and 3 with @p sides as the text of its Sides.
[[nodiscard]] std::string chuteWithSides(std::string_view sides)
{
    return doc(replace(kBatch2Chute, "SIDES", sides));
}

/// The bulkhead of batches 2 and 3 with @p filled as the text of its Filled.
[[nodiscard]] std::string bulkHeadFilled(std::string_view filled)
{
    return doc(replace(kBulkHead, "FILLED", filled));
}

/// The baseline body tube with a Thumbnail of the text @p thumbnail (batch 3).
[[nodiscard]] std::string tubeWithThumbnail(std::string_view thumbnail)
{
    return doc(tube(std::string(kDims) + "<Thumbnail>" + std::string(thumbnail) + "</Thumbnail>"));
}

/// The nose cone of batches 2 and 3 with @p shape as the text of its Shape.
[[nodiscard]] std::string noseConeOfShape(std::string_view shape)
{
    return doc(replace(kNoseCone, "SHAPE", shape));
}

void addBatch1Structure(Cases& cases)
{
    const std::string dims(kDims);
    cases.add("01 baseline", doc(tube(dims)));
    cases.add("02 unknown element in component", doc(tube(dims + "<Color>red</Color>")));
    cases.add("03 unknown component kind",
              doc("<FinSet><Manufacturer>Estes</Manufacturer></FinSet>" + tube(dims)));
    cases.add("04 missing Length",
              doc(tube(R"(<InsideDiameter Unit="in">0.95</InsideDiameter>)"
                       R"(<OutsideDiameter Unit="in">0.976</OutsideDiameter>)")));
    cases.add("05 missing Manufacturer",
              doc("<BodyTube><PartNumber>BT-X</PartNumber>" + dims + "</BodyTube>"));
    cases.add("06 missing PartNumber",
              doc("<BodyTube><Manufacturer>Estes</Manufacturer>" + dims + "</BodyTube>"));
    cases.add("07 bad number abc", doc(tube(replace(dims, ">18.0<", ">abc<"))));
    cases.add("08 empty number", doc(tube(replace(dims, ">18.0<", "><"))));
    cases.add("09 number with spaces", doc(tube(replace(dims, ">18.0<", ">  18.0 \n<"))));
    cases.add("10 number 1.8e1", doc(tube(replace(dims, ">18.0<", ">1.8e1<"))));
    cases.add("11 number +18", doc(tube(replace(dims, ">18.0<", ">+18<"))));
    cases.add("12 number NaN", doc(tube(replace(dims, ">18.0<", ">NaN<"))));
    cases.add("13 number INF", doc(tube(replace(dims, ">18.0<", ">INF<"))));
    cases.add("14 number Infinity", doc(tube(replace(dims, ">18.0<", ">Infinity<"))));
    cases.add("15 number 18d", doc(tube(replace(dims, ">18.0<", ">18d<"))));
    cases.add("16 number 0x12p0", doc(tube(replace(dims, ">18.0<", ">0x12p0<"))));
    cases.add("17 number 18,0", doc(tube(replace(dims, ">18.0<", ">18,0<"))));
    cases.add("18 number .5", doc(tube(replace(dims, ">18.0<", ">.5<"))));
    cases.add("19 number -0", doc(tube(replace(dims, ">18.0<", ">-0<"))));
    cases.add("20 unknown Unit",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="furlong">)"))));
    cases.add("21 no Unit attribute",
              doc(tube(replace(dims, R"(<Length Unit="in">)", "<Length>"))));
    cases.add("22 Unit in/64",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="in/64">)"))));
    cases.add("23 Unit ft",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="ft">)"))));
    cases.add("24 Unit with space",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit=" in">)"))));
    cases.add("25 two Length elements", doc(tube(dims + R"(<Length Unit="in">20.0</Length>)")));
    cases.add("26 material not in section",
              doc(replace(tube(dims), ">Cardboard<", ">Unobtainium<")));
    cases.add("27 material bad Type",
              doc(replace(tube(dims), R"(Type="BULK">Cardboard)", R"(Type="FOO">Cardboard)")));
    cases.add("28 material no Type",
              doc(replace(tube(dims), R"( Type="BULK">Cardboard)", ">Cardboard")));
    cases.add("29 material g/cm3 section", doc(replace(tube(dims), ">Cardboard<", ">Paper test<")));
    cases.add("30 material name with spaces",
              doc(replace(tube(dims), ">Cardboard<", "> Cardboard <")));
    cases.add("31 no Material",
              doc(replace(tube(dims), R"(<Material Type="BULK">Cardboard</Material>)", "")));
    cases.add("32 Mass in g", doc(tube(dims + R"(<Mass Unit="g">10</Mass>)")));
    cases.add("33 Mass unknown unit", doc(tube(dims + R"(<Mass Unit="stone">10</Mass>)")));
}

void addBatch1Documents(Cases& cases)
{
    const std::string dims(kDims);
    const std::string head(kHead);
    const std::string mats(kMats);
    cases.add("34 wrong root", head + "<Foo><Components>" + tube(dims) + "</Components></Foo>");
    cases.add("35 malformed", head + "<OpenRocketComponent><Components>" + tube(dims));
    cases.add("36 empty", "");
    cases.add("37 namespace root", head + R"(<OpenRocketComponent xmlns="urn:x"><Components>)" +
                                       tube(dims) + "</Components></OpenRocketComponent>");
    cases.add("38 no Version", head + "<OpenRocketComponent>" + mats + "<Components>" + tube(dims) +
                                   "</Components></OpenRocketComponent>");
    cases.add("39 Version 9.9",
              replace(doc(tube(dims)), "<Version>0.1</Version>", "<Version>9.9</Version>"));
    cases.add("40 Legacy element", replace(doc(tube(dims)), "<Version>0.1</Version>",
                                           "<Version>0.1</Version><Legacy>no</Legacy>"));
    cases.add("41 no Components", head + "<OpenRocketComponent><Version>0.1</Version>" + mats +
                                      "</OpenRocketComponent>");
    cases.add("42 components outside wrapper",
              head + "<OpenRocketComponent>" + tube(dims) + "</OpenRocketComponent>");
    cases.add("43 two Components wrappers", head + "<OpenRocketComponent><Components>" +
                                                tube(dims) + "</Components><Components>" +
                                                replace(tube(dims), "BT-X", "BT-Y") +
                                                "</Components></OpenRocketComponent>");
    cases.add("44 material section bad uom",
              replace(doc(tube(dims)), R"(UnitsOfMeasure="kg/m3")", R"(UnitsOfMeasure="lb/in3")"));
    cases.add("45 material section bad density",
              replace(doc(tube(dims)), "<Density>680.0</Density>", "<Density>x</Density>"));
    cases.add("46 material section no uom",
              replace(doc(tube(dims)), R"( UnitsOfMeasure="kg/m3")", ""));
    cases.add("47 material section Group",
              replace(doc(tube(dims)), "<Type>BULK</Type></Material>",
                      "<Type>BULK</Type><Group>Woods</Group></Material>"));
    cases.add("48 material section bad Group",
              replace(doc(tube(dims)), "<Type>BULK</Type></Material>",
                      "<Type>BULK</Type><Group>Rocks</Group></Material>"));
    cases.add("49 material section bad Type",
              replace(doc(tube(dims)), "<Type>BULK</Type></Material><Material",
                      "<Type>ROCK</Type></Material><Material"));
    cases.add("50 Filled yes",
              doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber>"
                  "<Filled>yes</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01"
                  "</Length></BulkHead>"));
    cases.add("51 Filled 1", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH"
                                 "</PartNumber><Filled>1</Filled><OutsideDiameter>0.02"
                                 "</OutsideDiameter><Length>0.01</Length></BulkHead>"));
    cases.add("52 Filled TRUE", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH"
                                    "</PartNumber><Filled>TRUE</Filled><OutsideDiameter>0.02"
                                    "</OutsideDiameter><Length>0.01</Length></BulkHead>"));
}

void addBatch1Components(Cases& cases)
{
    const std::string chute =
        "<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Description>"
        R"(chute</Description><Material Type="SURFACE">Ripstop nylon</Material><Diameter )"
        R"(Unit="in">12</Diameter><Sides>6</Sides><LineCount>6</LineCount><LineLength )"
        R"(Unit="in">12</LineLength><LineMaterial Type="LINE">Carpet Thread</LineMaterial>)"
        "</Parachute>";
    cases.add("53 parachute baseline", doc(chute));
    cases.add("54 parachute no LineCount", doc(replace(chute, "<LineCount>6</LineCount>", "")));
    cases.add("55 parachute LineCount 6.0",
              doc(replace(chute, "<LineCount>6</LineCount>", "<LineCount>6.0</LineCount>")));
    cases.add("56 parachute LineCount abc",
              doc(replace(chute, "<LineCount>6</LineCount>", "<LineCount>abc</LineCount>")));
    cases.add("57 parachute no LineLength",
              doc(replace(chute, R"(<LineLength Unit="in">12</LineLength>)", "")));
    cases.add("58 parachute DragCoefficient Unit in",
              doc(replace(chute, "<Sides>",
                          R"(<DragCoefficient Unit="in">0.8</DragCoefficient><Sides>)")));
    cases.add("59 parachute line material BULK",
              doc(replace(chute, R"(Type="LINE">Carpet Thread)", R"(Type="BULK">Cardboard)")));
    cases.add("60 parachute canopy BULK",
              doc(replace(chute, R"(Type="SURFACE">Ripstop nylon)", R"(Type="BULK">Cardboard)")));
    cases.add("61 nose cone bad shape", doc(replace(kNoseCone, "SHAPE", "BLUNT")));
    cases.add("62 nose cone shape lowercase", doc(replace(kNoseCone, "SHAPE", "ogive")));
    cases.add("63 nose cone no shoulder",
              doc("<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber>"
                  R"(<Material Type="BULK">Cardboard</Material><Shape>OGIVE</Shape>)"
                  "<OutsideDiameter>0.02</OutsideDiameter><Length>0.05</Length><Mass>0.01</Mass>"
                  "</NoseCone>"));
    cases.add("64 transition no fore shoulder",
              doc("<Transition><Manufacturer>Estes</Manufacturer><PartNumber>TR</PartNumber>"
                  R"(<Material Type="BULK">Cardboard</Material><Shape>CONICAL</Shape>)"
                  "<ForeOutsideDiameter>0.02</ForeOutsideDiameter><AftOutsideDiameter>0.03"
                  "</AftOutsideDiameter><AftShoulderDiameter>0.029</AftShoulderDiameter>"
                  "<AftShoulderLength>0.01</AftShoulderLength><Length>0.05</Length>"
                  "</Transition>"));
    cases.add("65 streamer no thickness",
              doc("<Streamer><Manufacturer>Estes</Manufacturer><PartNumber>ST</PartNumber>"
                  R"(<Material Type="SURFACE">Ripstop nylon</Material><Length>1</Length><Width>)"
                  "0.05</Width></Streamer>"));
    cases.add("66 rail button no NutMass",
              doc("<RailButton><Manufacturer>Estes</Manufacturer><PartNumber>RB</PartNumber>"
                  R"(<Material Type="BULK">Cardboard</Material><InnerDiameter>0.005)"
                  "</InnerDiameter><OuterDiameter>0.01</OuterDiameter><Height>0.01</Height>"
                  "<BaseHeight>0.002</BaseHeight><FlangeHeight>0.002</FlangeHeight><ScrewHeight>"
                  "0.001</ScrewHeight><ScrewMass>0.001</ScrewMass></RailButton>"));
    cases.add("67 tube no OD and no ID",
              doc("<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>BT</PartNumber>"
                  "<Length>0.1</Length></BodyTube>"));
    cases.add("68 element order reversed",
              doc(R"(<BodyTube><Length Unit="in">18.0</Length><OutsideDiameter Unit="in">)"
                  R"(0.976</OutsideDiameter><InsideDiameter Unit="in">0.95</InsideDiameter>)"
                  R"(<Material Type="BULK">Cardboard</Material><PartNumber>BT-X</PartNumber>)"
                  "<Manufacturer>Estes</Manufacturer></BodyTube>"));
}

void addBatch1Text(Cases& cases)
{
    const std::string dims(kDims);
    const std::string head(kHead);
    cases.add("69 CDATA and comment in number",
              doc(tube(replace(dims, ">18.0<", "><![CDATA[18]]><!-- c -->.5<"))));
    cases.add("70 attribute with namespace prefix",
              doc(tube(replace(dims, R"(<Length Unit="in">)",
                               R"(<Length xmlns:q="urn:q" q:Unit="ft">)"))));
    cases.add("71 element in other namespace",
              doc(tube(dims + R"(<q:Length xmlns:q="urn:q">99</q:Length>)")));
    cases.add("72 DOCTYPE with entity",
              head +
                  R"(<!DOCTYPE OpenRocketComponent [<!ENTITY len "18.0">]><OpenRocketComponent>)"
                  "<Components>" +
                  tube(replace(dims, ">18.0<", ">&len;<")) + "</Components></OpenRocketComponent>");
    cases.add("73 Manufacturer with spaces", doc(replace(tube(dims), ">Estes<", ">  Estes  <")));
    cases.add("74 same preset twice", doc(tube(dims) + tube(dims)));
    cases.add("75 thumbnail", doc(tube(dims + "<Thumbnail>aGVsbG8=</Thumbnail>")));
    cases.add("76 thumbnail bad base64", doc(tube(dims + "<Thumbnail>!!!</Thumbnail>")));
    cases.add("77 version attribute on root", replace(doc(tube(dims)), "<OpenRocketComponent>",
                                                      R"(<OpenRocketComponent version="2">)"));
    cases.add("78 utf8 BOM", "\uFEFF" + doc(tube(dims)));
    cases.add("79 negative length", doc(tube(replace(dims, ">18.0<", ">-18.0<"))));
    cases.add("80 huge exponent", doc(tube(replace(dims, ">18.0<", ">1e999<"))));
    // ISO-8859-1: the e with an acute accent is the one byte E9.
    cases.add("81 latin1 bytes in name (invalid UTF-8)",
              replace(doc(tube(dims)), ">d<", ">caf\xE9<"));
    cases.add("82 UTF-16 document", utf16(replace(doc(tube(dims)), "UTF-8", "UTF-16")));
}

void addBatch2Lexical(Cases& cases)
{
    constexpr auto kNumbers = std::to_array<std::string_view>({"18",
                                                               "18.",
                                                               "1.8E1",
                                                               "1.8e+1",
                                                               "0x1.2p4",
                                                               "0X12",
                                                               "18f",
                                                               "18 ",
                                                               "\t18\n",
                                                               "1 8",
                                                               "--18",
                                                               "+-18",
                                                               "1e",
                                                               "e1",
                                                               ".",
                                                               "-.5",
                                                               "5.-",
                                                               "\u00A018",
                                                               "\u200918",
                                                               "\u0661\u0668",
                                                               "18\u00A0",
                                                               "1_8",
                                                               "0018",
                                                               "-INF",
                                                               " INF ",
                                                               "NaN ",
                                                               "+NaN",
                                                               "1e-400",
                                                               "4.9e-324",
                                                               "2.2250738585072011e-308",
                                                               "9007199254740993",
                                                               "0.1",
                                                               "1.7976931348623157e309"});
    addFamily(cases, "N", "double", kNumbers, NameEscapes::LINE_FEEDS_AND_TABS, tubeOfLength);
    constexpr auto kInts = std::to_array<std::string_view>(
        {"6", " 6 ", "+6", "-6", "6.0", "1 2", "5-", "--5", "2147483648", "99999999999", "", "0x6",
         "\u0666", "6e0", "+", "-"});
    addFamily(cases, "I", "int", kInts, NameEscapes::NONE, chuteWithSides);
    constexpr auto kBooleans = std::to_array<std::string_view>(
        {"true",  "false", "1",    "0",      " true ", "\ntrue\n", "True", "t",  "tr", "truex",
         "true1", "f",     "fals", "falsey", "2",      "",         "yes",  "01", "10", " 1 "});
    addFamily(cases, "B", "bool", kBooleans, NameEscapes::LINE_FEEDS, bulkHeadFilled);
    constexpr auto kShapes = std::to_array<std::string_view>(
        {"OGIVE", " OGIVE ", "\nOGIVE\n", "OGIVE\u00A0", "Ogive", "HAACK", "POWER"});
    addFamily(cases, "S", "shape", kShapes, NameEscapes::LINE_FEEDS, noseConeOfShape);
}

void addBatch2Text(Cases& cases)
{
    const std::string dims(kDims);
    const std::string description = "<Description>d</Description>";
    const std::string sectionType = "<Type>BULK</Type></Material><Material";
    cases.add("T01 text split by child element", doc(tube(replace(dims, ">18.0<", ">1<b/>8<"))));
    cases.add("T02 empty Description", doc(replace(tube(dims), description, "<Description/>")));
    cases.add(
        "T03 Description with entity and CR LF",
        doc(replace(tube(dims), description, "<Description> a&amp;b\r\nc&#x9;</Description>")));
    cases.add("T04 Material type attribute with spaces",
              doc(replace(tube(dims), R"(Type="BULK")", R"(Type=" BULK")")));
    cases.add("T05 Unit attribute lb on Mass", doc(tube(dims + R"(<Mass Unit="lb">0.5</Mass>)")));
    cases.add("T06 Unit kg on Length",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="kg">)"))));
    cases.add("T07 two Materials wrappers",
              replace(doc(replace(tube(dims), ">Cardboard<", ">Paper test<")), "</Materials>",
                      R"(</Materials><Materials><Material UnitsOfMeasure="kg/m3"><Name>Other)"
                      "</Name><Density>1.0</Density><Type>BULK</Type></Material></Materials>"));
    cases.add("T08 Legacy empty", replace(doc(tube(dims)), "<Version>0.1</Version>", "<Legacy/>"));
    cases.add("T09 text inside Components", doc("stray &gt; text" + tube(dims) + "-->"));
    cases.add("T10 Material element with child element",
              doc(replace(tube(dims), ">Cardboard<", ">Card<x/>board<")));
    cases.add("T11 Manufacturer empty", doc(replace(tube(dims), ">Estes<", "><")));
    cases.add("T12 PartNumber empty", doc(replace(tube(dims), ">BT-X<", "><")));
    cases.add("T13 section density uom g/m2 for BULK",
              replace(doc(tube(dims)), R"(UnitsOfMeasure="kg/m3")", R"(UnitsOfMeasure="g/m2")"));
    cases.add("T14 section Material Type with spaces",
              replace(doc(tube(dims)), sectionType, "<Type> BULK </Type></Material><Material"));
    cases.add("T15 section ShearModulus",
              replace(doc(tube(dims)), sectionType,
                      "<Type>BULK</Type><ShearModulus>1e9</ShearModulus></Material><Material"));
    cases.add("T16 attribute Unit on root ignored and unknown attribute",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Foo="bar" Unit="in">)"))));
    cases.add("T17 component with xmlns default",
              doc(replace(tube(dims), "<BodyTube>", R"(<BodyTube xmlns="urn:x">)")));
    cases.add("T18 Length with xmlns empty",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length xmlns="" Unit="in">)"))));
    cases.add(
        "T19 Length with xmlns default other",
        doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length xmlns="urn:x" Unit="in">)"))));
    cases.add("T20 Version element with child",
              replace(doc(tube(dims)), "<Version>0.1</Version>", "<Version><x/></Version>"));
}

void addBatch3Lexical(Cases& cases)
{
    constexpr auto kNumbers =
        std::to_array<std::string_view>({"+INF", "inf", "-NaN", "INF\t", "&#13;18&#13;", " ", "+.5",
                                         "1e+", "-0x1.8p1", "1&#160;", "\n", "18<b/>x", "<b/>18"});
    addFamily(cases, "XN", "double", kNumbers, NameEscapes::LINE_FEEDS_AND_TABS, tubeOfLength);
    constexpr auto kInts = std::to_array<std::string_view>(
        {"+-6", "-+6", "6\t", "\n6\n", "&#13;6", "-0", "4294967302", "-2147483649", " ", "6 -",
         "0006", "&#160;6", "6<b/>x", "<b/>x", "6<b/>", "x<b/>6", "6<b/><c/>x", " <b/>x"});
    addFamily(cases, "XI", "int", kInts, NameEscapes::LINE_FEEDS_AND_TABS, chuteWithSides);
    constexpr auto kBooleans = std::to_array<std::string_view>(
        {" ",          "\t\t",        "\n",         "x",          "x ",           " 2 ",
         "&#13;1",     "true\t",      "false ",     "0 0",        "1\n",          "Z",
         "t rue",      "&#160;1",     " f",         "-",          "falsetrue",    "true<b/>x",
         "true<b/>",   "x<b/>true",   "true<b/>t",  "true<b/>01", "true<b/>trux", "true<b/>fals",
         "true<b/>tr", "true<b/>tru", "true<b/>f",  "true<b/>fa", "true<b/>fal",  "true<b/>tx",
         "true<b/>fx", "true<b/> t",  "true<b/>t ", "\u00A0",     "\U0001F680",   "\u00E9"});
    addFamily(cases, "XB", "bool", kBooleans, NameEscapes::LINE_FEEDS_AND_TABS, bulkHeadFilled);
    constexpr auto kShapes =
        std::to_array<std::string_view>({"<![CDATA[OGIVE]]>", "OG<!-- c -->IVE", "X<b/>OGIVE",
                                         "OGIVE<b/>", "", "&#79;GIVE", "OGIVE<b>x</b>"});
    addFamily(cases, "XS", "shape", kShapes, NameEscapes::NONE, noseConeOfShape);
}

void addBatch3TextAndNames(Cases& cases)
{
    const std::string dims(kDims);
    const std::string description = "<Description>d</Description>";
    const std::string xsi         = R"(xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance")";
    cases.add("XT01 text around a child element with text",
              doc(tube(replace(dims, ">18.0<", ">1<b>2</b>8<"))));
    cases.add("XT02 text before an empty child only",
              doc(tube(replace(dims, ">18.0<", ">18<b/><"))));
    cases.add("XT03 processing instruction in number",
              doc(tube(replace(dims, ">18.0<", ">1<?pi x?>8<"))));
    cases.add("XT04 comment between digits", doc(tube(replace(dims, ">18.0<", ">1<!-- c -->8<"))));
    cases.add("XT05 Description with child element text",
              doc(replace(tube(dims), description, "<Description>a<b>c</b></Description>")));
    cases.add("XT06 Description in CDATA",
              doc(replace(tube(dims), description, "<Description><![CDATA[x<y]]></Description>")));
    cases.add("XT07 Description in two text runs around a comment",
              doc(replace(tube(dims), description, "<Description>a<!-- c --> b</Description>")));
    cases.add(
        "XT08 Description with characters beyond the BMP",
        doc(replace(tube(dims), description, "<Description>\U0001F680&#x1F680;</Description>")));
    cases.add("XT09 xsi:nil on Length",
              doc(tube(replace(dims, kLength18In,
                               "<Length " + xsi + R"( xsi:nil="true" Unit="in">18.0</Length>)"))));
    cases.add("XT10 xsi:nil on Description",
              doc(replace(tube(dims), description,
                          "<Description " + xsi + R"( xsi:nil="true">d</Description>)")));
    cases.add("XT11 character reference in Unit",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="&#105;n">)"))));
    cases.add("XT12 tab in Unit",
              doc(tube(replace(dims, R"(<Length Unit="in">)", "<Length Unit=\"in\t\">"))));
    cases.add("XT13 tab reference in Unit",
              doc(tube(replace(dims, R"(<Length Unit="in">)", R"(<Length Unit="in&#9;">)"))));
    cases.add("XT14 two Unit attributes by prefix",
              doc(tube(replace(dims, R"(<Length Unit="in">)",
                               R"(<Length xmlns:q="urn:q" q:Unit="ft" Unit="cm">)"))));
    cases.add("XT15 Material Type attribute by reference",
              doc(replace(tube(dims), R"(Type="BULK")", R"(Type="&#66;ULK")")));
    cases.add("XT16 PartNumber with trailing space and tab",
              doc(replace(tube(dims), ">BT-X<", ">BT-X \t<")));
    cases.add("XT17 Legacy with a namespace",
              replace(doc(tube(dims)), "<Version>0.1</Version>", R"(<Legacy xmlns="urn:x"/>)"));
    cases.add("XT18 Materials wrapper in another namespace",
              replace(doc(replace(tube(dims), ">Cardboard<", ">Paper test<")), "<Materials>",
                      R"(<Materials xmlns="urn:x">)"));
    cases.add(
        "XT19 root with an empty default namespace",
        replace(doc(tube(dims)), "<OpenRocketComponent>", R"(<OpenRocketComponent xmlns="">)"));
    cases.add("XT20 root in a prefixed namespace",
              std::string(kHead) + R"(<p:OpenRocketComponent xmlns:p="urn:p"><Components>)" +
                  tube(dims) + "</Components></p:OpenRocketComponent>");
    cases.add("XT21 empty Components wrapper after a full one",
              std::string(kHead) + "<OpenRocketComponent><Components>" + tube(dims) +
                  "</Components><Components/></OpenRocketComponent>");
    cases.add("XT22 component element with text only", doc("<BodyTube>text</BodyTube>"));
    cases.add("XT23 document with only a root", std::string(kHead) + "<OpenRocketComponent/>");
    cases.add("XT24 comment and PI before the root",
              std::string(kHead) + "<!-- c --><?pi x?>\n" + doc(tube(dims)).substr(kHead.size()));
    cases.add("XT25 Description with an empty child only",
              doc(replace(tube(dims), description, "<Description><b/></Description>")));
}

void addBatch3Repetitions(Cases& cases)
{
    const std::string dims(kDims);
    const std::string description = "<Description>d</Description>";
    const std::string sectionType = "<Type>BULK</Type></Material><Material";
    const std::string xml11       = R"(version="1.1")";
    cases.add("XT26 XML 1.1 control character before a number",
              replace(doc(tube(replace(dims, ">18.0<", ">&#11;18<"))), R"(version="1.0")", xml11));
    cases.add("XT27 XML 1.1 control character after a number",
              replace(doc(tube(replace(dims, ">18.0<", ">18&#11;<"))), R"(version="1.0")", xml11));
    cases.add("XT28 XML 1.1 control character before an int",
              replace(doc(replace(kBatch2Chute, "SIDES", "&#11;6")), R"(version="1.0")", xml11));
    cases.add("XT29 XML 1.1 control character before a boolean",
              replace(doc(replace(kBulkHead, "FILLED", "&#11;1")), R"(version="1.0")", xml11));
    cases.add("XT30 two Length elements, the second not a number",
              doc(tube(dims + R"(<Length Unit="in">x</Length>)")));
    cases.add(
        "XT31 two Sides elements, the second not a number",
        doc(replace(kBatch2Chute, "<Sides>SIDES</Sides>", "<Sides>6</Sides><Sides>x</Sides>")));
    cases.add(
        "XT32 two Sides elements, the second empty",
        doc(replace(kBatch2Chute, "<Sides>SIDES</Sides>", "<Sides>6</Sides><Sides></Sides>")));
    cases.add("XT33 two Density elements, the second not a number",
              replace(doc(tube(dims)), "<Density>680.0</Density>",
                      "<Density>680.0</Density><Density>x</Density>"));
    cases.add("XT34 two Filled elements, the second malformed",
              doc(replace(kBulkHead, "<Filled>FILLED</Filled>",
                          "<Filled>true</Filled><Filled>t</Filled>")));
    cases.add("XT35 two Description elements, the second empty",
              doc(replace(tube(dims), description,
                          "<Description>d</Description><Description></Description>")));
    cases.add("XT36 two Shape elements, the second unknown",
              doc(replace(kNoseCone, "<Shape>SHAPE</Shape>",
                          "<Shape>OGIVE</Shape><Shape>BLUNT</Shape>")));
    cases.add("XT37 two ShearModulus elements, the second not a number",
              replace(doc(tube(dims)), sectionType,
                      "<Type>BULK</Type><ShearModulus>1e9</ShearModulus><ShearModulus>x"
                      "</ShearModulus></Material><Material"));
    cases.add(
        "XT38 Density with text after a child element",
        replace(doc(tube(dims)), "<Density>680.0</Density>", "<Density>680.0<b/>x</Density>"));
    cases.add("XT39 two Density elements, the second empty",
              replace(doc(tube(dims)), "<Density>680.0</Density>",
                      "<Density>680.0</Density><Density></Density>"));
    cases.add("XT40 two Density elements, the first not a number",
              replace(doc(tube(dims)), "<Density>680.0</Density>",
                      "<Density>x</Density><Density>680.0</Density>"));
    cases.add("XT41 two ShearModulus elements, the second empty",
              replace(doc(tube(dims)), sectionType,
                      "<Type>BULK</Type><ShearModulus>1e9</ShearModulus><ShearModulus>"
                      "</ShearModulus></Material><Material"));
    cases.add(
        "XT42 ShearModulus with text after a child element",
        replace(doc(tube(dims)), sectionType,
                "<Type>BULK</Type><ShearModulus>1e9<b/>x</ShearModulus></Material><Material"));
    cases.add("XT43 ShearModulus not a number",
              replace(doc(tube(dims)), sectionType,
                      "<Type>BULK</Type><ShearModulus>x</ShearModulus></Material><Material"));
    cases.add("XT44 two Length elements, the first not a number",
              doc(tube(replace(dims, kLength18In,
                               R"(<Length Unit="in">x</Length><Length Unit="in">18.0</Length>)"))));
    cases.add("XT45 Description with white space before a child element",
              doc(replace(tube(dims), description, "<Description> <b/>x</Description>")));
    cases.add("XT46 Description with white space after a child element",
              doc(replace(tube(dims), description, "<Description>a<b/> </Description>")));
    cases.add("XT47 Description with text between two child elements",
              doc(replace(tube(dims), description, "<Description>a<b/>c<d/></Description>")));
    cases.add(
        "XT48 Description with a carriage return by reference",
        doc(replace(tube(dims), description, "<Description>a&#13;b&#13;&#10;c\rd</Description>")));
}

void addBatch3Thumbnails(Cases& cases)
{
    constexpr auto kThumbnails = std::to_array<std::string_view>(
        {"aGVsbG8",   "aGVs bG8=", "aGVsbG8=\n", "",       "QQ==",      "QQ",       "Q",
         "aGV!sbG8=", "====",      "QUJD",       " QUJD ", "QUJD=",     "QU=JD",    "4pyI",
         "QU==",      "QUI=",      "QUJDQQ==",   "Q===",   "=QUJ",      "QU=D",     "QQ==QUJD",
         "QUJD====",  "AAAA",      "////",       "++++",   "-_-_",      "QU!D",     "QU D",
         "QQ=",       "QQ=A",      "A===",       "AA==",   "QUJ\u00E9", "QUJDQUJD", "QUJD\tQUJD",
         "QR==",      "QUK="});
    addFamily(cases, "XH", "thumbnail", kThumbnails, NameEscapes::LINE_FEEDS_AND_TABS,
              tubeWithThumbnail);
}

[[nodiscard]] std::vector<OrcEdgeCase> buildCases()
{
    Cases cases;
    addBatch1Structure(cases);
    addBatch1Documents(cases);
    addBatch1Components(cases);
    addBatch1Text(cases);
    addBatch2Lexical(cases);
    addBatch2Text(cases);
    addBatch3Lexical(cases);
    addBatch3TextAndNames(cases);
    addBatch3Repetitions(cases);
    addBatch3Thumbnails(cases);
    return cases.take();
}

}  // namespace

const std::vector<OrcEdgeCase>& orcEdgeCases()
{
    static const std::vector<OrcEdgeCase> kCases = buildCases();
    return kCases;
}

const OrcEdgeCase& orcEdgeCase(std::string_view id)
{
    for (const OrcEdgeCase& edgeCase : orcEdgeCases())
    {
        if (edgeCase.id == id)
        {
            return edgeCase;
        }
    }
    ADD_FAILURE() << "no edge case " << id;
    static const OrcEdgeCase kNone;
    return kNone;
}

std::span<const std::byte> orcEdgeCaseBytes(std::string_view id)
{
    return std::as_bytes(std::span<const char>(orcEdgeCase(id).bytes));
}

}  // namespace QtRocket::Test
