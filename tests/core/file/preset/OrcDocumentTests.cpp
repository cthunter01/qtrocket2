#include "QtRocket/file/preset/OrcDocument.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/OrcElement.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::OrcDocument;
using QtRocket::OrcElement;
using QtRocket::Result;

/// The document @p text, which must parse.
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

TEST(OrcDocument, ReadsTheRootAndCountsTheElements)
{
    const OrcDocument document = parsed(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<OpenRocketComponent>"
        "<Version>0.1</Version><Components><BodyTube/><NoseCone/></Components>"
        "</OpenRocketComponent>");
    EXPECT_EQ(document.elementCount(), 5U);
    EXPECT_TRUE(document.root().isNamed("OpenRocketComponent"));
    EXPECT_EQ(document.root().children().size(), 2U);
}

TEST(OrcDocument, ReadsBytesAsJavasUtf8Reader)
{
    // The XML declaration's encoding is ignored: the bytes are UTF-8, malformed ones U+FFFD.
    const std::string bytes =
        "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><a>caf\xE9 \xC3\xA9</a>";
    const Result<OrcDocument> document = OrcDocument::parse(QtRocket::stringToBytes(bytes));
    ASSERT_TRUE(document) << document.error().toString();
    EXPECT_EQ(document->root().text(), "caf\uFFFD \u00E9");
}

TEST(OrcDocument, RefusesWhatTheJdksParserRefuses)
{
    const Result<OrcDocument> empty = OrcDocument::parseText("");
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code, ErrorCode::PARSE);
    EXPECT_EQ(empty.error().message, "Premature end of file.");

    const Result<OrcDocument> unclosed = OrcDocument::parseText("<a><b></b>");
    ASSERT_FALSE(unclosed);
    EXPECT_EQ(unclosed.error().code, ErrorCode::PARSE);
    EXPECT_EQ(unclosed.error().message,
              "XML document structures must start and end within the same entity.");

    // A byte order mark is a character to a reader of characters.
    const Result<OrcDocument> bom = OrcDocument::parseText("\uFEFF<a/>");
    ASSERT_FALSE(bom);
    EXPECT_EQ(bom.error().message, "Content is not allowed in prolog.");
    const Result<OrcDocument> bomBytes =
        OrcDocument::parse(QtRocket::stringToBytes("\xEF\xBB\xBF<a/>"));
    ASSERT_FALSE(bomBytes);
    EXPECT_EQ(bomBytes.error().message, "Content is not allowed in prolog.");

    const Result<OrcDocument> unbound = OrcDocument::parseText("<p:a/>");
    ASSERT_FALSE(unbound);
    EXPECT_EQ(unbound.error().code, ErrorCode::PARSE);
}

TEST(OrcDocument, RefusesADocumentTypeThatDeclaresAnEntity)
{
    // Java expands the entity (the probe's case 72); XmlScanner does not read such declarations.
    const Result<OrcDocument> document =
        OrcDocument::parseText(R"(<!DOCTYPE a [<!ENTITY len "18.0">]><a>&len;</a>)");
    ASSERT_FALSE(document);
    EXPECT_EQ(document.error().code, ErrorCode::UNSUPPORTED_FORMAT);
    // One with only comments in its internal subset is read.
    EXPECT_TRUE(OrcDocument::parseText("<!DOCTYPE a [<!-- c -->]><a/>"));
}

TEST(OrcDocument, RefusesMoreElementsThanItsBound)
{
    const std::string_view text = "<a><b/><c><d/></c></a>";
    EXPECT_TRUE(OrcDocument::parseText(text, 4));
    const Result<OrcDocument> document = OrcDocument::parseText(text, 3);
    ASSERT_FALSE(document);
    EXPECT_EQ(document.error().code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_EQ(document.error().message, "The document has more than 3 elements.");
    EXPECT_EQ(OrcDocument::kMaxElements, 1000000U);
}

TEST(OrcDocument, ReadsDeepDocumentsWithoutRecursion)
{
    constexpr int kDepth = 100000;
    std::string   text;
    for (int i = 0; i < kDepth; i++)
    {
        text += "<a>";
    }
    text += 'x';
    for (int i = 0; i < kDepth; i++)
    {
        text += "</a>";
    }
    const OrcDocument document = parsed(text);
    EXPECT_EQ(document.elementCount(), static_cast<std::size_t>(kDepth));
    std::optional<OrcElement> element = document.root();
    for (int i = 1; i < kDepth && element; i++)
    {
        element = element->lastChild("a");
    }
    // A plain if: clang-tidy's unchecked-optional-access check cannot see through ASSERT_TRUE.
    if (!element.has_value())
    {
        FAIL() << "the innermost element is missing";
    }
    EXPECT_EQ(element->text(), "x");
}

TEST(OrcDocument, NormalisesTheLineEndsOfXml11)
{
    // U+0085 and U+2028 end a line in XML 1.1 only.
    const OrcDocument xml11 = parsed("<?xml version=\"1.1\"?><a>1\u00852\u20283\r\u00854</a>");
    EXPECT_EQ(xml11.root().text(), "1\n2\n3\n4");
    const OrcDocument xml10 = parsed("<?xml version=\"1.0\"?><a>1\u00852\r\n3\r4</a>");
    EXPECT_EQ(xml10.root().text(), "1\u00852\n3\n4");
}

TEST(OrcDocument, ItsElementsSurviveAMove)
{
    OrcDocument                     document = parsed("<a><b>text</b></a>");
    const std::optional<OrcElement> child    = document.root().lastChild("b");
    const OrcDocument               moved    = std::move(document);
    if (!child.has_value())
    {
        FAIL() << "no element b";
    }
    EXPECT_EQ(child->text(), "text");
    EXPECT_EQ(moved.root().localName(), "a");
}

TEST(OrcDocument, AMovedFromDocumentHasNoRoot)
{
    OrcDocument       document = parsed("<a/>");
    const OrcDocument moved    = std::move(document);
    EXPECT_EQ(moved.elementCount(), 1U);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): what the test is about
    EXPECT_EQ(document.elementCount(), 0U);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): what the test is about
    EXPECT_THROW(static_cast<void>(document.root()), QtRocket::BugError);
}

}  // namespace
