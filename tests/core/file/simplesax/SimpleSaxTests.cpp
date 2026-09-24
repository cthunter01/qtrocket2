#include "QtRocket/file/simplesax/SimpleSax.h"

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "file/simplesax/RecordingHandler.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::SimpleSax;
using QtRocket::WarningSet;
using QtRocket::Test::RecordingHandler;

/// The calls @p document makes, and the failure's message after them ("" when it reads).
[[nodiscard]] std::vector<std::string> calls(std::string_view document,
                                             std::string_view failOn = "")
{
    RecordingHandler handler;
    handler.failOn = std::string(failOn);
    WarningSet               warnings;
    const Result<void>       read = SimpleSax::readXml(document, handler, warnings);
    std::vector<std::string> log  = handler.log;
    log.push_back(read ? "" : read.error().message);
    return log;
}

TEST(SimpleSax, HandsTheElementsToTheHandlers)
{
    EXPECT_EQ(calls(R"(<?xml version="1.0"?><!-- c --><root a="1"><item b="x &amp; y">t&lt;1)"
                    R"(<![CDATA[<2>]]><?pi skipped?><!-- no text -->3</item></root>)"),
              (std::vector<std::string>{"open root a=1", "open item b=x & y", "end item",
                                        "close item b=x & y [t<1<2>3]", "end root",
                                        "close root a=1 []", ""}));
}

TEST(SimpleSax, ReportsLocalNamesWithoutNamespaceDeclarations)
{
    // As the namespace-aware SAX parser reports them; ":c" has no prefix.
    EXPECT_EQ(
        calls(R"(<r:root xmlns:r="urn:r" xmlns="urn:d" r:a="1" :c="2"/>)"),
        (std::vector<std::string>{"open root :c=2 a=1", "end root", "close root :c=2 a=1 []", ""}));
}

TEST(SimpleSax, NormalisesLineEndsAndAttributeValues)
{
    EXPECT_EQ(calls("<a v=\"1\r\n2\t3&#10;4\">x\r\ny\rz</a>"),
              (std::vector<std::string>{"open a v=1 2 3\n4", "end a",
                                        "close a v=1 2 3\n4 [x\ny\nz]", ""}));
    // XML 1.1 also ends lines at U+0085 and U+2028.
    EXPECT_EQ(calls("<?xml version=\"1.1\"?><a>x\r\u0085y\u0085z w&#x85;</a>"),
              (std::vector<std::string>{"open a", "end a", "close a [x\ny\nz\nw\u0085]", ""}));
}

TEST(SimpleSax, HandsOverTheElementsBeforeAnXmlError)
{
    // The JDK's parser reports elements as it reads, so the handler sees those before an error,
    // and a failure of its own there comes first.
    EXPECT_EQ(calls("<a><b/><c>&bad;</c></a>"),
              (std::vector<std::string>{"open a", "open b", "end b", "close b []", "open c",
                                        R"(The entity "bad" was referenced, but not declared.)"}));
    EXPECT_EQ(calls("<a><b/><c></d></a>", "b"),
              (std::vector<std::string>{"open a", "open b", "fail b"}));
    EXPECT_EQ(calls("<a><b/></a>junk", "b"),
              (std::vector<std::string>{"open a", "open b", "fail b"}));
    // An error in a start tag comes before the element is handed over.
    EXPECT_EQ(calls(R"(<a><b x="1" x="2"/></a>)", "b"),
              (std::vector<std::string>{
                  "open a", R"(Attribute "x" was already specified for element "b".)"}));
}

TEST(SimpleSax, FailsWithTheJdkParsersMessage)
{
    RecordingHandler   handler;
    WarningSet         warnings;
    const Result<void> read = SimpleSax::readXml("<a>AT&T</a>", handler, warnings);
    ASSERT_FALSE(read);
    EXPECT_EQ(read.error().code, ErrorCode::PARSE);
    EXPECT_EQ(read.error().message,
              R"(The reference to entity "T" must end with the ';' delimiter.)");
}

TEST(SimpleSax, LeavesDocumentTypeDeclarationsWithDeclarationsUnread)
{
    RecordingHandler   handler;
    WarningSet         warnings;
    const Result<void> read =
        SimpleSax::readXml(R"(<!DOCTYPE a [<!ENTITY e "x">]><a>&e;</a>)", handler, warnings);
    ASSERT_FALSE(read);
    EXPECT_EQ(read.error().code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_TRUE(handler.log.empty());
    EXPECT_EQ(calls("<!DOCTYPE a [ <!-- only a comment --> ]><a/>"),
              (std::vector<std::string>{"open a", "end a", "close a []", ""}));
}

TEST(SimpleSax, PassesTheHandlersWarningsOn)
{
    // PlainTextHandler ignores child elements with a warning.
    WarningSet         warnings;
    const Result<void> read =
        SimpleSax::readXml("<a>x<b/>y</a>", QtRocket::PlainTextHandler::instance(), warnings);
    ASSERT_TRUE(read);
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->messageDescription(), "Unknown element a, ignoring.");
}

TEST(SimpleSax, ReadsMalformedUtf8AsReplacementCharacters)
{
    EXPECT_EQ(calls("<a>x\xffy</a>"),
              (std::vector<std::string>{"open a", "end a", "close a [x�y]", ""}));
}

}  // namespace
