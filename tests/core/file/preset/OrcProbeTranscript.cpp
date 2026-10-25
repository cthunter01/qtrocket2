#include "file/preset/OrcProbeTranscript.h"

#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/FloatingDecimal.h"

namespace QtRocket::Test
{

namespace
{

/// orcProbeEscape().
[[nodiscard]] std::string escapeAsProbe(std::string_view text)
{
    std::string escaped;
    for (const char c : text)
    {
        switch (c)
        {
            case '\\':
                escaped += "\\\\";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

/// A property's value as the probe's fmt() writes it.
struct ProbeValue
{
    [[nodiscard]] std::string operator()(bool value) const { return value ? "true" : "false"; }
    [[nodiscard]] std::string operator()(int value) const { return std::to_string(value); }
    [[nodiscard]] std::string operator()(double value) const
    {
        return FloatingDecimal::toJavaFormatString(value);
    }
    [[nodiscard]] std::string operator()(const std::string& value) const
    {
        return escapeAsProbe(value);
    }
    [[nodiscard]] std::string operator()(ManufacturerRef value) const
    {
        return escapeAsProbe(value.get().getDisplayName());
    }
    [[nodiscard]] std::string operator()(ComponentPresetType value) const
    {
        return std::string(componentPresetTypeName(value));
    }
    [[nodiscard]] std::string operator()(TransitionShape value) const
    {
        return std::string(displayName(value));
    }
    [[nodiscard]] std::string operator()(const Material& value) const
    {
        return std::format(
            "[{} '{}' {} shear {} {}{}]", displayName(value.getType()),
            escapeAsProbe(value.getName()), FloatingDecimal::toJavaFormatString(value.getDensity()),
            FloatingDecimal::toJavaFormatString(value.getInPlaneShearModulus()),
            databaseString(value.getGroup()), value.isUserDefined() ? " user" : " builtin");
    }
    [[nodiscard]] std::string operator()(Finish value) const { return toString(value); }
    [[nodiscard]] std::string operator()(const std::vector<std::byte>& value) const
    {
        std::string text = std::format("bytes{}:", value.size());
        for (const std::byte b : value)
        {
            text += std::format("{:02x}", static_cast<unsigned int>(b));
        }
        return text;
    }
};

}  // namespace

std::string orcProbeEscape(std::string_view text)
{
    return escapeAsProbe(text);
}

std::string orcProbeLine(const ComponentPreset& preset)
{
    std::string line;
    for (const TypedPropertyMap::Entry& entry : preset.getProperties().entries())
    {
        line += std::format("{}={}; ", entry.key.getName(), std::visit(ProbeValue{}, entry.value));
    }
    return line + "digest=" + preset.getDigest();
}

std::vector<std::string> orcProbeLines(
    std::span<const std::shared_ptr<const ComponentPreset>> presets)
{
    std::vector<std::string> lines;
    lines.reserve(presets.size());
    for (const std::shared_ptr<const ComponentPreset>& preset : presets)
    {
        lines.push_back(orcProbeLine(*preset));
    }
    return lines;
}

}  // namespace QtRocket::Test
