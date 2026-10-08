#include "file/openrocket/DocumentHandlerTestSupport.h"

#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/openrocket/DatatypeHandler.h"
#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"
#include "QtRocket/file/openrocket/DocumentPreferencesHandler.h"
#include "QtRocket/file/openrocket/PhotoStudioHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/HandlerTestSupport.h"

namespace QtRocket::Test
{

namespace
{

/// @p text in single quotes, a line break written as backslash n.
[[nodiscard]] std::string quote(std::string_view text)
{
    std::string quoted = "'";
    for (const char c : text)
    {
        if (c == '\n')
        {
            quoted += "\\n";
        }
        else
        {
            quoted += c;
        }
    }
    quoted += '\'';
    return quoted;
}

/// @p text with every line break written as backslash n.
[[nodiscard]] std::string oneLine(std::string_view text)
{
    const std::string quoted = quote(text);
    return quoted.substr(1, quoted.size() - 2);
}

/// "{name=value, name2=value2}": the attributes as Java prints a sorted map.
[[nodiscard]] std::string mapText(const ElementHandler::Attributes& map)
{
    std::string text = "{";
    for (const auto& [key, value] : map)
    {
        text += std::format("{}{}={}", text.size() > 1 ? ", " : "", key, value);
    }
    return text + "}";
}

/// A preference as the probe prints one: the simple name of the Java class of its value, and
/// the value, a text in quotes and a number as Java prints it.
[[nodiscard]] std::string preferenceText(const DocumentPreferences::Value& value)
{
    return std::visit(
        [](const auto& held) -> std::string {
            using Held = std::decay_t<decltype(held)>;
            if constexpr (std::is_same_v<Held, bool>)
            {
                return std::format("Boolean {}", held);
            }
            else if constexpr (std::is_same_v<Held, int>)
            {
                return std::format("Integer {}", held);
            }
            else if constexpr (std::is_same_v<Held, double>)
            {
                return std::format("Double {}", Strings::javaDoubleToString(held));
            }
            else
            {
                return std::format("String {}", quote(held));
            }
        },
        value);
}

/// The handler for the element @p xml starts with, as the document's content handler and the
/// handler of <docprefs> make theirs.
[[nodiscard]] std::unique_ptr<ElementHandler> handlerFor(std::string_view xml,
                                                         DocumentFixture& fixture)
{
    if (xml.starts_with("<docprefs"))
    {
        return std::make_unique<DocumentPreferencesHandler>(fixture.context());
    }
    if (xml.starts_with("<docmaterials"))
    {
        return std::make_unique<DocumentMaterialHandler>(fixture.context());
    }
    if (xml.starts_with("<photostudio"))
    {
        return std::make_unique<PhotoStudioHandler>(fixture.document().getPhotoSettings());
    }
    if (xml.starts_with("<datatypes"))
    {
        return std::make_unique<DatatypeHandler>(fixture.context());
    }
    bug(std::format("no handler for {}", xml));
}

/// Counts the change events of a document, as the probe's listener does.
class DocumentEventCounter
{
public:
    explicit DocumentEventCounter(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect(
            [this](const DocumentChangeEvent& /*event*/) { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                                  m_count{0};
    Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

}  // namespace

DocumentFixture::DocumentFixture()
{
    addBuiltinMaterials(m_fixture.materials());
}

HandlerRun DocumentFixture::load(std::string_view xml)
{
    const std::string_view                element = Strings::trim(xml);
    const std::unique_ptr<ElementHandler> handler = handlerFor(element, *this);
    return runHandler(*handler, element);
}

std::vector<std::string> DocumentFixture::describe()
{
    std::vector<std::string>  lines;
    const OpenRocketDocument& doc = document();
    for (const auto& [key, preference] : doc.getDocumentPreferences().preferences())
    {
        lines.push_back(std::format("pref {} {}", quote(key), preferenceText(preference.value())));
    }
    for (const Material& material : doc.getDocumentMaterials().allMaterials())
    {
        lines.push_back(std::format("material [{}] document={} user={}",
                                    material.toStorableString(), material.isDocumentMaterial(),
                                    material.isUserDefined()));
    }
    for (const auto& [key, value] : doc.getPhotoSettings())
    {
        lines.push_back(std::format("photo {}={}", key, quote(value)));
    }
    for (const CustomExpression& expression : doc.getCustomExpressions())
    {
        lines.push_back(std::format("expression name={} symbol={} unit={} expression={}",
                                    quote(expression.getName()), quote(expression.getSymbol()),
                                    quote(expression.getUnit()),
                                    quote(expression.getExpressionString())));
    }
    return lines;
}

std::string DocumentFixture::loadAndDescribe(std::string_view xml)
{
    const DocumentEventCounter events(document());
    const HandlerRun           run = load(xml);

    std::string text = run.result.has_value() ? std::string("RESULT ok\n")
                                              : std::format("RESULT FAILED {}: {}\n",
                                                            toString(run.result.error().code),
                                                            oneLine(run.result.error().message));
    for (const std::string& warning : run.texts())
    {
        text += std::format("W {}\n", oneLine(warning));
    }
    if (!run.element.empty())
    {
        text += std::format("ROOT {} {} [{}]\n", run.element, mapText(run.attributes),
                            Strings::trim(run.content));
    }
    text += std::format("EVENTS {}\n", events.count());
    for (const std::string& line : describe())
    {
        text += std::format("| {}\n", line);
    }
    // The last line has no line break, as the expectations are written.
    text.pop_back();
    return text;
}

std::string elementOfDocument(std::string_view document, std::string_view name)
{
    const std::string open  = std::format("<{}", name);
    const std::string close = std::format("</{}>", name);
    for (std::size_t begin = document.find(open); begin != std::string_view::npos;
         begin             = document.find(open, begin + 1))
    {
        // "<name" may be the start of a longer name.
        const std::size_t behind = begin + open.size();
        if (behind >= document.size() || (document[behind] != '>' && document[behind] != '/' &&
                                          document[behind] != ' ' && document[behind] != '\n'))
        {
            continue;
        }
        const std::size_t tagEnd = document.find('>', behind);
        if (tagEnd == std::string_view::npos)
        {
            return {};
        }
        if (document[tagEnd - 1] == '/')
        {
            return std::string(document.substr(begin, tagEnd + 1 - begin));
        }
        const std::size_t end = document.find(close, tagEnd);
        if (end == std::string_view::npos)
        {
            return {};
        }
        return std::string(document.substr(begin, end + close.size() - begin));
    }
    return {};
}

std::string runDocumentCase(std::string_view xml)
{
    DocumentFixture fixture;
    return fixture.loadAndDescribe(xml);
}

std::vector<std::string> failedDocumentCases(std::span<const DocumentCase> cases)
{
    std::vector<std::string> failed;
    for (const DocumentCase& one : cases)
    {
        const std::string found = runDocumentCase(one.xml);
        if (found != one.expected)
        {
            failed.push_back(std::format("case {}\n{}\n--- expected\n{}\n--- found\n{}\n", one.name,
                                         one.xml, one.expected, found));
        }
    }
    return failed;
}

std::string printedDocumentCases(std::span<const DocumentCase> cases)
{
    std::string printed;
    for (const DocumentCase& one : cases)
    {
        printed += std::format("=== {}\n{}\n", one.name, runDocumentCase(one.xml));
    }
    return printed;
}

std::string whatReadingTheElementThrows(std::string_view xml)
{
    try
    {
        DocumentFixture fixture;
        static_cast<void>(fixture.load(xml));
        static_cast<void>(fixture.describe());
        fixture.document().clearUndo();
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return {};
}

std::vector<std::string> documentCasesThatThrowWhenCutOff(std::span<const DocumentCase> cases)
{
    std::vector<std::string> thrown;
    for (const DocumentCase& one : cases)
    {
        const std::string_view element = Strings::trim(one.xml);
        for (std::size_t end = element.find('>'); end != std::string_view::npos;
             end             = element.find('>', end + 1))
        {
            const std::string_view cut   = element.substr(0, end + 1);
            const std::string      wrong = whatReadingTheElementThrows(cut);
            if (!wrong.empty())
            {
                thrown.push_back(
                    std::format("case {}, cut off as\n{}\n{}\n", one.name, cut, wrong));
                break;
            }
        }
    }
    return thrown;
}

}  // namespace QtRocket::Test
