#include "QtRocket/file/openrocket/ConfigHandler.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/openrocket/EntryHelper.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

ConfigHandler::ConfigHandler(int listDepth) noexcept : m_listDepth(listDepth) { }

Result<ElementHandler*> ConfigHandler::openElement(std::string_view  element,
                                                   const Attributes& attributes,
                                                   WarningSet&       warnings)
{
    const auto type = attributes.find("type");
    if (element == "entry" && type != attributes.end() && type->second == "list")
    {
        if (m_listDepth < kMaxListDepth)
        {
            return openList(std::make_unique<ConfigHandler>(m_listDepth + 1));
        }
        // Not OpenRocket's, which reads no list at all: see the class comment.
        closeList();
        warnings.add(Warning::fromString("List entries nested too deeply, ignoring."));
    }
    return &PlainTextHandler::instance();
}

Result<void> ConfigHandler::closeElement(std::string_view element, const Attributes& attributes,
                                         std::string_view content, WarningSet& warnings)
{
    if (element != "entry")
    {
        return EntryHandler::closeElement(element, attributes, content, warnings);
    }
    std::optional<Config::Value> value = EntryHelper::getValueFromEntry(*this, attributes, content);
    if (value.has_value())
    {
        const auto key = attributes.find("key");
        if (key != attributes.end())
        {
            m_config.put(key->second, std::move(*value));
        }
        else
        {
            addToList(std::move(*value));
        }
    }
    return {};
}

}  // namespace QtRocket
