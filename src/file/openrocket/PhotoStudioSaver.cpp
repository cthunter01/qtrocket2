#include "QtRocket/file/openrocket/PhotoStudioSaver.h"

#include <array>
#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Java's getColor(): the colour of "R G B A", or nullopt for fewer than four fields and (a
/// deviation, see the header) for a field Integer.parseInt refuses.
[[nodiscard]] std::optional<Color> colorOf(std::string_view content)
{
    const std::vector<std::string> values = Strings::splitJava(content, ' ');
    if (values.size() < 4)
    {
        return std::nullopt;
    }
    std::array<int, 4> parts{};
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        const std::optional<int> part = Strings::parseInt(values[i]);
        if (!part.has_value())
        {
            return std::nullopt;
        }
        parts.at(i) = *part;
    }
    return Color(parts[0], parts[1], parts[2], parts[3]);
}

/// Java's emitColor().
void emitColor(std::string_view elementName, std::vector<std::string>& elements,
               std::string_view content)
{
    const std::optional<Color> color = colorOf(content);
    if (color.has_value())
    {
        elements.push_back(std::format("<{} {}/>", elementName, color->toXmlAttributes()));
    }
    else
    {
        elements.push_back(std::format("<{}></{}>", elementName, elementName));
    }
}

/// Java's emitValue().
void emitValue(std::string_view elementName, std::vector<std::string>& elements,
               std::string_view content, OrkWriter& writer)
{
    elements.push_back(
        std::format("<{}>{}</{}>", elementName, writer.escape(content), elementName));
}

}  // namespace

std::vector<std::string> PhotoStudioSaver::getElements(
    const std::map<std::string, std::string>& photoSettings, OrkWriter& writer)
{
    std::vector<std::string> elements;
    for (const PhotoStudioSetting& setting : kSettings)
    {
        const auto found = photoSettings.find(std::string(setting.key));
        if (found == photoSettings.end())
        {
            continue;  // Java's emit methods write nothing for null
        }
        if (setting.color)
        {
            emitColor(setting.key, elements, found->second);
        }
        else
        {
            emitValue(setting.key, elements, found->second, writer);
        }
    }
    return elements;
}

}  // namespace QtRocket
