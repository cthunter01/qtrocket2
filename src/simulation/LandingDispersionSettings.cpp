#include "QtRocket/simulation/LandingDispersionSettings.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace QtRocket
{

namespace
{

/// @p attributes as Java prints a TreeMap: "{a=1, b=2}".
[[nodiscard]] std::string describe(const LandingDispersionSettings::Attributes& attributes)
{
    std::string text = "{";
    for (const auto& [name, value] : attributes)
    {
        if (text.size() > 1)
        {
            text += ", ";
        }
        text += name;
        text += '=';
        text += value;
    }
    return text + "}";
}

}  // namespace

LandingDispersionSettings::LandingDispersionSettings(Attributes              attributes,
                                                     std::vector<Attributes> uncertainties)
  : m_attributes(std::move(attributes)), m_uncertainties(std::move(uncertainties))
{
}

std::optional<std::string_view> LandingDispersionSettings::getAttribute(std::string_view name) const
{
    const auto found = m_attributes.find(name);
    if (found == m_attributes.end())
    {
        return std::nullopt;
    }
    return std::string_view{found->second};
}

void LandingDispersionSettings::addUncertainty(Attributes attributes)
{
    m_uncertainties.push_back(std::move(attributes));
}

bool LandingDispersionSettings::operator==(const LandingDispersionSettings& other) const = default;

std::string LandingDispersionSettings::toString() const
{
    std::string text  = describe(m_attributes) + " [";
    bool        first = true;
    for (const Attributes& uncertainty : m_uncertainties)
    {
        if (!first)
        {
            text += ", ";
        }
        first = false;
        text += describe(uncertainty);
    }
    return text + "]";
}

}  // namespace QtRocket
