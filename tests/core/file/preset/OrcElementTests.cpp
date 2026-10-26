#include "QtRocket/file/preset/OrcElement.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/OrcDocument.h"
#include "QtRocket/util/Error.h"

// OrcElement's reading of names, texts and values, as the probe tools/openrocket-probes/orc-loader
// measured JAXB's (the case ids of its expected.txt in brackets); orc_edge_case_tests.cpp replays
// the probe's documents themselves.

namespace
{

using QtRocket::OrcDocument;
using QtRocket::OrcElement;
using QtRocket::Result;

[[nodiscard]] OrcDocument parsed(std::string_view text)
{
    Result<OrcDocument> document = OrcDocument::parseText(text);
    if (!document)
    {
        ADD_FAILURE() << document.error().toString();
        document = OrcDocument::parseText("<failed/>");
    }
    return std::move(*document);
}

/// The local names of @p elements.
[[nodiscard]] std::vector<std::string> namesOf(const std::vector<OrcElement>& elements)
{
    std::vector<std::string> names;
    names.reserve(elements.size());
    for (const OrcElement& element : elements)
    {
        names.emplace_back(element.localName());
    }
    return names;
}

/// The texts of @p element.
[[nodiscard]] std::vector<std::string> textsOf(const OrcElement& element)
{
    return {element.texts().begin(), element.texts().end()};
}

/// The text() of the last child @p name of @p element, nullopt when there is none.
[[nodiscard]] std::optional<std::string> lastChildText(const OrcElement& element,
                                                       std::string_view  name)
{
    const std::optional<OrcElement> child = element.lastChild(name);
    return child ? std::optional<std::string>(child->text()) : std::nullopt;
}

/// The namespace of the last child @p name of @p element, nullopt when there is none.
[[nodiscard]] std::optional<std::string> lastChildNamespace(const OrcElement& element,
                                                            std::string_view  name)
{
    const std::optional<OrcElement> child = element.lastChild(name);
    return child ? std::optional<std::string>(child->namespaceUri()) : std::nullopt;
}

/// The texts of the root of @p text.
[[nodiscard]] std::vector<std::string> rootTexts(std::string_view text)
{
    const OrcDocument document = parsed(text);
    return textsOf(document.root());
}

TEST(OrcElement, ResolvesTheNamespacesOfElements)
{
    const OrcDocument document =
        parsed(R"(<r xmlns:q="urn:q"><a/><q:a/><b xmlns="urn:x"><c/><d xmlns=""/></b>)"
               "<xml:e/></r>");
    const OrcElement              root     = document.root();
    const std::vector<OrcElement> children = root.children();
    ASSERT_EQ(children.size(), 4U);
    EXPECT_EQ(children[0].namespaceUri(), "");
    EXPECT_TRUE(children[0].isNamed("a"));
    EXPECT_EQ(children[1].namespaceUri(), "urn:q");
    EXPECT_EQ(children[1].localName(), "a");
    EXPECT_FALSE(children[1].isNamed("a"));
    EXPECT_EQ(children[2].namespaceUri(), "urn:x");
    const std::vector<OrcElement> inner = children[2].children();
    ASSERT_EQ(inner.size(), 2U);
    EXPECT_EQ(inner[0].namespaceUri(), "urn:x");  // the default namespace is inherited
    EXPECT_TRUE(inner[1].isNamed("d"));           // and xmlns="" undoes it
    EXPECT_EQ(children[3].namespaceUri(), "http://www.w3.org/XML/1998/namespace");
    // Only the elements of no namespace are found by name.
    EXPECT_EQ(namesOf(root.children("a")), (std::vector<std::string>{"a"}));
    EXPECT_FALSE(root.lastChild("b").has_value());
    EXPECT_EQ(lastChildNamespace(root, "a"), "");
}

TEST(OrcElement, ScopesNamespaceDeclarationsToTheirElement)
{
    const OrcDocument document =
        parsed(R"(<r><x xmlns="urn:x" xmlns:q="urn:q"><q:y/></x><y/><p:z xmlns:p="urn:p"/>)"
               R"(<w xmlns:p="urn:other"><p:z/></w></r>)");
    const std::vector<OrcElement> children = document.root().children();
    ASSERT_EQ(children.size(), 4U);
    EXPECT_EQ(children[0].children().front().namespaceUri(), "urn:q");
    EXPECT_TRUE(children[1].isNamed("y"));
    EXPECT_EQ(children[2].namespaceUri(), "urn:p");
    EXPECT_EQ(children[3].children().front().namespaceUri(), "urn:other");
}

TEST(OrcElement, FindsTheLastChildOfAName)
{
    // [25], [43], [T07]: a repeated element replaces the one before.
    const OrcDocument document = parsed(R"(<r><a>1</a><b/><a>2</a><a xmlns="urn:x">3</a></r>)");
    const OrcElement  root     = document.root();
    EXPECT_EQ(lastChildText(root, "a"), "2");
    EXPECT_EQ(root.children("a").size(), 2U);
    EXPECT_EQ(namesOf(root.children()), (std::vector<std::string>{"a", "b", "a", "a"}));
    EXPECT_FALSE(root.lastChild("c").has_value());
    EXPECT_TRUE(root.children("c").empty());
}

TEST(OrcElement, ReadsAttributesWithoutAPrefix)
{
    // [70], [XT14]: a prefixed attribute is another namespace's; [XT11] to [XT13]: values are
    // normalised (references expanded, a literal tab a space); [24]: not trimmed.
    const OrcDocument document = parsed(
        "<a xmlns=\"urn:x\" xmlns:q=\"urn:q\" q:Unit=\"ft\" Unit=\" i&#110;\t&#9;\" Other=\"\"/>");
    const OrcElement root = document.root();
    EXPECT_EQ(root.attribute("Unit"), " in \t");
    EXPECT_EQ(root.attribute("Other"), "");
    EXPECT_EQ(root.attribute("xmlns"), std::nullopt);
    EXPECT_EQ(root.attribute("q:Unit"), std::nullopt);
    EXPECT_EQ(root.attribute("Missing"), std::nullopt);
}

TEST(OrcElement, HandsOverTheTextAfterTheLastChildElement)
{
    // No child element: one text, the whole content ([09], [T02], [T03], [69]).
    EXPECT_EQ(rootTexts("<a>  18.0 \n</a>"), (std::vector<std::string>{"  18.0 \n"}));
    EXPECT_EQ(rootTexts("<a/>"), (std::vector<std::string>{""}));
    EXPECT_EQ(rootTexts("<a> a&amp;b\r\nc&#x9;</a>"), (std::vector<std::string>{" a&b\nc\t"}));
    EXPECT_EQ(rootTexts("<a><![CDATA[18]]><!-- c -->.5</a>"), (std::vector<std::string>{"18.5"}));
    EXPECT_EQ(rootTexts("<a>1<?pi x?>8</a>"), (std::vector<std::string>{"18"}));       // [XT03]
    EXPECT_EQ(rootTexts("<a>a&#13;b\rc</a>"), (std::vector<std::string>{"a\rb\nc"}));  // [XT48]
    // Each child element hands over the text before it, unless that is empty or white space
    // ([T01], [XT01], [XT02], [XI13], [XI16], [XI17], [XT45] to [XT47]); the end always does.
    EXPECT_EQ(rootTexts("<a>1<b/>8</a>"), (std::vector<std::string>{"1", "8"}));
    EXPECT_EQ(rootTexts("<a>1<b>2</b>8</a>"), (std::vector<std::string>{"1", "8"}));
    EXPECT_EQ(rootTexts("<a>18<b/></a>"), (std::vector<std::string>{"18", ""}));
    EXPECT_EQ(rootTexts("<a><b/>x</a>"), (std::vector<std::string>{"x"}));
    EXPECT_EQ(rootTexts("<a> \n<b/>x</a>"), (std::vector<std::string>{"x"}));
    EXPECT_EQ(rootTexts("<a>6<b/><c/>x</a>"), (std::vector<std::string>{"6", "x"}));
    EXPECT_EQ(rootTexts("<a>a<b/>c<d/></a>"), (std::vector<std::string>{"a", "c", ""}));
    EXPECT_EQ(rootTexts("<a>a<b/> </a>"), (std::vector<std::string>{"a", " "}));
    const OrcDocument document = parsed("<a>1<b>2</b>8</a>");
    EXPECT_EQ(document.root().text(), "8");
    EXPECT_EQ(lastChildText(document.root(), "b"), "2");
}

TEST(OrcElement, ReadsTheLastValueItsTextsGive)
{
    // [XN11], [XN12], [T01], [XT02]: a rejected text leaves the double it had.
    EXPECT_EQ(parsed("<a>18<b/>x</a>").root().doubleValue(), 18.0);
    EXPECT_EQ(parsed("<a><b/>18</a>").root().doubleValue(), 18.0);
    EXPECT_EQ(parsed("<a>1<b/>8</a>").root().doubleValue(), 8.0);
    EXPECT_EQ(parsed("<a>18<b/></a>").root().doubleValue(), 18.0);
    EXPECT_EQ(parsed("<a>x</a>").root().doubleValue(), std::nullopt);
    // [XI12], [XI14], [XI15]: the same for an Integer, whose empty text is 0.
    EXPECT_EQ(parsed("<a>6<b/>x</a>").root().intValue(), 6);
    EXPECT_EQ(parsed("<a>6<b/></a>").root().intValue(), 0);
    EXPECT_EQ(parsed("<a>x<b/>6</a>").root().intValue(), 6);
    EXPECT_EQ(parsed("<a>x</a>").root().intValue(), std::nullopt);
    // [XB17] to [XB21]: a Boolean takes false and null too, and keeps its value where a text
    // ends after a 't' or an 'f'.
    EXPECT_EQ(parsed("<a>true<b/>x</a>").root().booleanValue(), false);
    EXPECT_EQ(parsed("<a>true<b/></a>").root().booleanValue(), std::nullopt);
    EXPECT_EQ(parsed("<a>x<b/>true</a>").root().booleanValue(), true);
    EXPECT_EQ(parsed("<a>true<b/>t</a>").root().booleanValue(), true);
    EXPECT_EQ(parsed("<a>true<b/>01</a>").root().booleanValue(), std::nullopt);
    EXPECT_EQ(parsed("<a>t</a>").root().booleanValue(), std::nullopt);
}

TEST(OrcElement, ReadsThePropertiesOfRepeatedElements)
{
    const OrcDocument document = parsed(
        "<r><s>a</s><s></s><d>680</d><d>x</d><e>x</e><e>680</e><m>1e9</m><m>x</m><m></m>"
        "<i>6</i><i>x</i><j>6</j><j></j><f>true</f><f>t</f><g>true</g><g></g></r>");
    const OrcElement root = document.root();
    // A String: the last element's text ([XT35]); none when there is no such element.
    EXPECT_EQ(root.stringProperty("s"), "");
    EXPECT_EQ(root.stringProperty("none"), std::nullopt);
    // The primitive double: the last element alone ([XT33], [XT40]).
    EXPECT_EQ(root.doubleProperty("d"), std::nullopt);
    EXPECT_EQ(root.doubleProperty("e"), 680.0);
    EXPECT_EQ(root.doubleProperty("none"), std::nullopt);
    // A Double, an Integer, a Boolean: the last value any element gives ([XT37], [XT41],
    // [XT31], [XT32], [XT34]).
    EXPECT_EQ(root.boxedDoubleProperty("m"), 1e9);
    EXPECT_EQ(root.boxedDoubleProperty("none"), std::nullopt);
    EXPECT_EQ(root.integerProperty("i"), 6);
    EXPECT_EQ(root.integerProperty("j"), 0);
    EXPECT_EQ(root.integerProperty("none"), std::nullopt);
    EXPECT_EQ(root.booleanProperty("f"), true);
    EXPECT_EQ(root.booleanProperty("g"), std::nullopt);
    EXPECT_EQ(root.booleanProperty("none"), std::nullopt);
}

TEST(OrcElement, IgnoresPropertiesInANamespace)
{
    // [T19], [XT17]: an element of another namespace is no property.
    const OrcDocument document =
        parsed(R"(<r xmlns:q="urn:q"><q:s>a</q:s><s xmlns="urn:x">b</s><q:i>6</q:i></r>)");
    EXPECT_EQ(document.root().stringProperty("s"), std::nullopt);
    EXPECT_EQ(document.root().integerProperty("i"), std::nullopt);
}

TEST(OrcElement, ReadsTheLastBase64TextThatDecodes)
{
    // [75], [XH03]: the thumbnail's image; a rejected text leaves an earlier value, as for an
    // Integer (the probe has one element with one text only).
    const OrcDocument document = parsed(
        "<r><t>QUJD</t><t>QQ</t><u></u><v>QQ</v><w>QQ==<b/>!!!</w><x xmlns=\"urn:x\">QUJD</x></r>");
    const OrcElement             root = document.root();
    const std::vector<std::byte> abc{std::byte{0x41}, std::byte{0x42}, std::byte{0x43}};
    EXPECT_EQ(root.base64BinaryProperty("t"), abc);
    EXPECT_EQ(root.base64BinaryProperty("u"), std::vector<std::byte>{});
    EXPECT_EQ(root.base64BinaryProperty("v"), std::nullopt);
    EXPECT_EQ(root.base64BinaryProperty("w"), std::vector<std::byte>{std::byte{0x41}});
    EXPECT_EQ(root.base64BinaryProperty("x"), std::nullopt);
    EXPECT_EQ(root.base64BinaryProperty("none"), std::nullopt);
}

}  // namespace
