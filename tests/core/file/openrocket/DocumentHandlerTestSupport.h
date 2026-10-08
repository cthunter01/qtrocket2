#pragma once

// What the tests of the handlers of a design file's own elements share (<docprefs>,
// <docmaterials>, <photostudio>, <datatypes>): one such element is read by its handler for the
// document a load starts from, through the test root of HandlerTestSupport.h, and what came of
// it is printed as the Java probe of tier 9b, part R4 prints it for OpenRocket
// (DocumentProbe.java under the scratchpad's probes/tier9b-document-handlers; its outputs are
// the expectations of the case tables). Test-only.
//
// What a case gives (runDocumentCase()), one line each:
//
//     RESULT ok                                   or  RESULT FAILED <code>: <message>
//     W <warning>                                 every warning, in order
//     ROOT <element> {name=value, ...} [<text>]   what the element's parent is told at its end
//     EVENTS <n>                                  the document's change events
//     | pref '<key>' <Java type> <value>          the document's preferences, by key
//     | material [<storable string>] document=<flag> user=<flag>
//                                                 the document's materials, as a saver lists them
//     | photo <key>='<text>'                      the photo settings, by key
//     | expression name='..' symbol='..' unit='..' expression='..'
//                                                 the custom expressions, in their order

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "file/openrocket/HandlerTestSupport.h"

namespace QtRocket::Test
{

/// The environment of a load, as the probe has it: a HandlerFixture whose application materials
/// are OpenRocket's built-in ones.
class DocumentFixture
{
public:
    DocumentFixture();
    ~DocumentFixture() = default;

    // The context points at the members.
    DocumentFixture(const DocumentFixture&)            = delete;
    DocumentFixture& operator=(const DocumentFixture&) = delete;
    DocumentFixture(DocumentFixture&&)                 = delete;
    DocumentFixture& operator=(DocumentFixture&&)      = delete;

    [[nodiscard]] HandlerFixture&         fixture() noexcept { return m_fixture; }
    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_fixture.context(); }
    [[nodiscard]] OpenRocketDocument&     document() noexcept { return m_fixture.document(); }

    /// Reads @p xml, one element, with the handler the name of the element asks for, as the
    /// handler of the content of a design file makes it: a DocumentPreferencesHandler for
    /// <docprefs>, a DocumentMaterialHandler for <docmaterials>, a PhotoStudioHandler over the
    /// document's photo settings for <photostudio> and a DatatypeHandler for <datatypes>.
    /// @throws BugError for any other element
    [[nodiscard]] HandlerRun load(std::string_view xml);

    /// load(), and what came of it in the probe's notation (see the top of this file).
    [[nodiscard]] std::string loadAndDescribe(std::string_view xml);

    /// The state of the document in the probe's notation, one line each without the "| ".
    [[nodiscard]] std::vector<std::string> describe();

private:
    HandlerFixture m_fixture;
};

/// The first element named @p name of the XML @p document, with everything in it, as text:
/// from its "<name" to its "</name>", or to its "/>" when it has no content. Empty when the
/// document has none. (For an element that does not hold one of its own name.)
[[nodiscard]] std::string elementOfDocument(std::string_view document, std::string_view name);

/// DocumentFixture().loadAndDescribe(xml) in a fixture of its own.
[[nodiscard]] std::string runDocumentCase(std::string_view xml);

/// One case of a table: an element of a design file and what reading it gives.
struct DocumentCase
{
    /// The name of the case in the probe's case file.
    std::string_view name;
    /// The element (DocumentFixture::load()).
    std::string_view xml;
    /// What runDocumentCase() has to give.
    std::string_view expected;
};

/// What reading each case of @p cases gives that is not what the case expects: per case its
/// name, its text, and the lines expected and found. Empty when every case agrees.
[[nodiscard]] std::vector<std::string> failedDocumentCases(std::span<const DocumentCase> cases);

/// The cases of @p cases as the probe's case file has them, each followed by what QtRocket
/// makes of it: the text a DISABLED_ test prints for the script that makes the tables.
[[nodiscard]] std::string printedDocumentCases(std::span<const DocumentCase> cases);

/// What reading @p xml (DocumentFixture::load()) and then describing the document throws, or
/// "": the failure policy of the loader is that nothing a file can hold makes it throw.
[[nodiscard]] std::string whatReadingTheElementThrows(std::string_view xml);

/// The cases of @p cases that make their handler throw when their text is cut off behind any
/// of its '>' (a document that ends too early is one a file can hold), each with the text and
/// what was thrown. Empty when none does.
[[nodiscard]] std::vector<std::string> documentCasesThatThrowWhenCutOff(
    std::span<const DocumentCase> cases);

}  // namespace QtRocket::Test
