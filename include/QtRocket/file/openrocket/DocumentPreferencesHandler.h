#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"
#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class OpenRocketDocument;

/// Reads the <docprefs> element of a design file: the preferences that are saved with the
/// document, each a <pref key="..." type="...">value</pref>, and the document's own materials
/// in a <docmaterials> child (OpenRocket's file/openrocket/importt/DocumentPreferencesHandler).
/// A file may hold several <docprefs>; each adds to what the document has, a later value of a
/// key replacing the earlier one.
///
/// A <pref> that closes is read by its type attribute, compared exactly (see EntryHelper), and
/// stored under its key in the document's preferences (OpenRocketDocument::
/// getDocumentPreferences()), which tells the document of the change:
/// - "boolean": Boolean.valueOf of the text, which is not trimmed: putBoolean();
/// - "string": the text as it is: putString();
/// - "integer": the text trimmed and read as Integer.parseInt reads it: putInt();
/// - "double": the text as Double.parseDouble reads it, a NaN and the infinities included (a
///   preference is no input of a simulation): putDouble();
/// - any other type, or none, is passed over without a word, and so is a "number" that is none.
/// A <pref> without a key is the element of a list: its value goes into the handler's own
/// list (EntryHandler::getList()), which nobody reads. A <pref> of type "list" has a
/// ConfigHandler for what it holds, as in OpenRocket, so the elements of such a list are
/// <entry> elements, and a <pref> in it gives AbstractElementHandler's warnings.
///
/// <docmaterials> is handed to a DocumentMaterialHandler. Any other child is plain text and
/// gives AbstractElementHandler's warnings for its text and attributes; so does <docmaterials>
/// itself.
///
/// Deviations from OpenRocket:
/// - Preferences of type "integer" and "double" are loaded. OpenRocket's loader knows neither
///   type (EntryHelper reads "boolean", "string", "number" and "list") and drops such a
///   preference without a word, although its own saver writes these two types and no other for
///   the numbers of a document. One that cannot be read (an "integer" that is no int, a
///   "double" that is no number) gives Warning::kFileInvalidParameter and is dropped.
/// - A <pref> with a key whose type is "number" (and whose text is a number) gives the warning
///   "Number preferences are not supported", and one whose type is "list" the warning "Nested
///   preferences are not supported"; neither is stored. OpenRocket throws a RuntimeException
///   with that text, which nothing catches.
/// - The values of a list are really read by the list's handler and then dropped with the
///   list (see ConfigHandler; OpenRocket's list is always empty).
/// - The constructor takes the loading context (Java: the document), which the handler of
///   <docmaterials> needs. Without a document in the context it is a BugError.
class DocumentPreferencesHandler final : public EntryHandler
{
public:
    /// The handler of a <docprefs> element of the document of @p context. The context and
    /// what it points at must outlive the handler.
    /// @throws BugError when @p context has no document
    explicit DocumentPreferencesHandler(const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    explicit DocumentPreferencesHandler(const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    /// The value of a <pref> of type "integer" or "double" (not OpenRocket's: see the class
    /// comment); nullopt for another type, and with a warning for a text that cannot be read.
    [[nodiscard]] static std::optional<Config::Value> explicitNumber(
        std::optional<std::string_view> type, std::string_view content, WarningSet& warnings);

    /// Stores @p value under @p key as the preference its @p type says
    /// (addValueToDocumentPreferences()).
    void addValueToDocumentPreferences(std::string_view key, const Config::Value& value,
                                       std::string_view type, WarningSet& warnings);

    const DocumentLoadingContext* m_context;
    OpenRocketDocument*           m_document;
    /// The handler of the <docmaterials> read last; the next one replaces it.
    std::unique_ptr<DocumentMaterialHandler> m_materialHandler;
};

}  // namespace QtRocket
