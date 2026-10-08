#include "QtRocket/file/openrocket/DocumentPreferencesHandler.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"
#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/openrocket/EntryHelper.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

DocumentPreferencesHandler::DocumentPreferencesHandler(const DocumentLoadingContext& context)
  : m_context(&context), m_document(context.getOpenRocketDocument())
{
    if (m_document == nullptr)
    {
        bug("The loading context of a document preferences handler has no document");
    }
}

Result<ElementHandler*> DocumentPreferencesHandler::openElement(std::string_view  element,
                                                                const Attributes& attributes,
                                                                WarningSet& /*warnings*/)
{
    if (element == "pref" && DocumentConfig::attribute(attributes, "type") == "list")
    {
        // The entries of the list stand in one list entry, this one.
        return openList(std::make_unique<ConfigHandler>(1));
    }
    if (element == "docmaterials")
    {
        m_materialHandler = std::make_unique<DocumentMaterialHandler>(*m_context);
        return m_materialHandler.get();
    }
    return &PlainTextHandler::instance();
}

Result<void> DocumentPreferencesHandler::closeElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      std::string_view  content,
                                                      WarningSet&       warnings)
{
    if (element != "pref")
    {
        return EntryHandler::closeElement(element, attributes, content, warnings);
    }
    const std::optional<std::string_view> key  = DocumentConfig::attribute(attributes, "key");
    const std::optional<std::string_view> type = DocumentConfig::attribute(attributes, "type");
    // Called for every <pref> that closes: it takes the list this <pref> opened.
    std::optional<Config::Value> value = EntryHelper::getValueFromEntry(*this, attributes, content);
    if (!value.has_value())
    {
        // Not OpenRocket's, which has no value here and drops the preference.
        value = explicitNumber(type, content, warnings);
    }
    if (!value.has_value())
    {
        return {};
    }
    if (key.has_value())
    {
        // With a value the element has a type.
        addValueToDocumentPreferences(*key, *value, type.value_or(std::string_view{}), warnings);
    }
    else
    {
        addToList(std::move(*value));
    }
    return {};
}

std::optional<Config::Value> DocumentPreferencesHandler::explicitNumber(
    std::optional<std::string_view> type, std::string_view content, WarningSet& warnings)
{
    if (type == "integer")
    {
        // What OpenRocket's saver writes: Integer.toString().
        if (const std::optional<int> value = Strings::parseInt(Strings::trim(content)))
        {
            return Config::Value(static_cast<std::int32_t>(*value));
        }
        warnings.add(Warning::kFileInvalidParameter);
    }
    else if (type == "double")
    {
        // What OpenRocket's saver writes: Double.toString(), "NaN" and "Infinity" included.
        if (const std::optional<double> value = Strings::javaParseDouble(content))
        {
            return Config::Value(*value);
        }
        warnings.add(Warning::kFileInvalidParameter);
    }
    return std::nullopt;
}

void DocumentPreferencesHandler::addValueToDocumentPreferences(std::string_view     key,
                                                               const Config::Value& value,
                                                               std::string_view     type,
                                                               WarningSet&          warnings)
{
    DocumentPreferences& docPrefs = m_document->getDocumentPreferences();

    // The value has the type its element names: both come from the same attribute.
    if (type == "boolean")
    {
        if (const bool* const flag = std::get_if<bool>(&value.variant()))
        {
            docPrefs.putBoolean(key, *flag);
        }
    }
    else if (type == "string")
    {
        if (const std::string* const text = std::get_if<std::string>(&value.variant()))
        {
            docPrefs.putString(key, *text);
        }
    }
    else if (type == "integer")
    {
        if (const std::int32_t* const number = std::get_if<std::int32_t>(&value.variant()))
        {
            docPrefs.putInt(key, *number);
        }
    }
    else if (type == "double")
    {
        if (const double* const number = std::get_if<double>(&value.variant()))
        {
            docPrefs.putDouble(key, *number);
        }
    }
    else if (type == "number")
    {
        // Java: a RuntimeException with this text, which nothing catches.
        warnings.add(Warning::fromString("Number preferences are not supported"));
    }
    else if (type == "list")
    {
        // We don't support nested preferences
        warnings.add(Warning::fromString("Nested preferences are not supported"));
    }
}

}  // namespace QtRocket
