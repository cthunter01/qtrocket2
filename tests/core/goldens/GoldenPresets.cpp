#include "goldens/GoldenPresets.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Test
{

namespace
{

using Json = nlohmann::ordered_json;

/// The double @p value of the golden: a number with a fraction or an exponent, or one of the
/// strings "NaN", "Infinity" and "-Infinity"; nullopt for anything else. An integer is no double:
/// PresetDumper writes every Double with Double.toString(), which always writes a '.' or an 'E'.
[[nodiscard]] std::optional<double> goldenDouble(const Json& value)
{
    if (value.is_number_float())
    {
        return value.get<double>();
    }
    if (value.is_string())
    {
        const auto& text = value.get_ref<const std::string&>();
        if (text == "NaN")
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (text == "Infinity")
        {
            return std::numeric_limits<double>::infinity();
        }
        if (text == "-Infinity")
        {
            return -std::numeric_limits<double>::infinity();
        }
    }
    return std::nullopt;
}

/// Whether @p actual is @p expected bit for bit (any NaN for a NaN).
[[nodiscard]] bool sameDouble(double actual, double expected)
{
    if (std::isnan(actual) || std::isnan(expected))
    {
        return std::isnan(actual) && std::isnan(expected);
    }
    return std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected);
}

/// @p bytes in standard base64 with padding (Java's Base64.getEncoder()).
[[nodiscard]] std::string base64(std::span<const std::byte> bytes)
{
    constexpr std::string_view kDigits =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for (std::size_t i = 0; i < bytes.size(); i += 3)
    {
        const std::size_t count = std::min<std::size_t>(3, bytes.size() - i);
        std::uint32_t     group = 0;
        for (std::size_t j = 0; j < 3; j++)
        {
            group = (group << 8U) | (j < count ? std::to_integer<std::uint32_t>(bytes[i + j]) : 0U);
        }
        for (std::size_t j = 0; j < 4; j++)
        {
            text += j <= count ? kDigits[(group >> (18U - (6U * j))) & 0x3FU] : '=';
        }
    }
    return text;
}

/// The differences of one preset, written as lines that name it.
class Mismatches
{
public:
    Mismatches(const ComponentPreset& preset, bool tolerantDensity)
      : m_name(
            std::format("{} '{}'", componentPresetTypeName(preset.getType()), preset.getPartNo())),
        m_tolerantDensity(tolerantDensity)
    {
    }

    void add(std::string_view what, std::string_view expected, std::string_view actual)
    {
        m_text += std::format("{}: {}: expected {}, found {}\n", m_name, what, expected, actual);
    }

    /// @p actual against the golden string @p expected.
    void text(std::string_view what, const Json& expected, std::string_view actual)
    {
        if (!expected.is_string() || expected.get_ref<const std::string&>() != actual)
        {
            add(what, expected.dump(), Json(actual).dump());
        }
    }

    /// @p actual against the golden double @p expected, within kTranscendentalDensityTolerance
    /// when @p tolerant.
    void number(std::string_view what, const Json& expected, double actual, bool tolerant = false)
    {
        const std::optional<double> value = goldenDouble(expected);
        const bool                  same =
            value && (tolerant ? std::abs(actual - *value) <=
                                     kTranscendentalDensityTolerance * std::abs(*value)
                               : sameDouble(actual, *value));
        if (!same)
        {
            add(what, expected.dump(), FloatingDecimal::toJavaFormatString(actual));
        }
    }

    /// @p actual against the golden material @p expected.
    void material(std::string_view what, const Json& expected, const Material& actual)
    {
        if (!expected.is_object() || expected.size() != 7)
        {
            add(what, expected.dump(), "a material");
            return;
        }
        text(std::format("{}.type", what), expected.value("type", Json()),
             toString(actual.getType()));
        text(std::format("{}.name", what), expected.value("name", Json()), actual.getName());
        number(std::format("{}.density", what), expected.value("density", Json()),
               actual.getDensity(), m_tolerantDensity && what == "Material");
        number(std::format("{}.shearModulus", what), expected.value("shearModulus", Json()),
               actual.getInPlaneShearModulus());
        text(std::format("{}.group", what), expected.value("group", Json()),
             databaseString(actual.getGroup()));
        flag(std::format("{}.userDefined", what), expected.value("userDefined", Json()),
             actual.isUserDefined());
        flag(std::format("{}.documentMaterial", what), expected.value("documentMaterial", Json()),
             actual.isDocumentMaterial());
    }

    /// @p actual against the golden boolean @p expected.
    void flag(std::string_view what, const Json& expected, bool actual)
    {
        if (!expected.is_boolean() || expected.get<bool>() != actual)
        {
            add(what, expected.dump(), actual ? "true" : "false");
        }
    }

    /// @p actual against the golden integer @p expected.
    void integer(std::string_view what, const Json& expected, int actual)
    {
        if (!expected.is_number_integer() || expected.get<std::int64_t>() != actual)
        {
            add(what, expected.dump(), std::to_string(actual));
        }
    }

    [[nodiscard]] const std::string& lines() const noexcept { return m_text; }

private:
    std::string m_name;
    bool        m_tolerantDensity;
    std::string m_text;
};

/// The comparison of one property's value with its golden value.
struct PropertyCheck
{
    Mismatches*      mismatches;
    std::string_view key;
    const Json*      expected;

    void operator()(bool value) const { mismatches->flag(key, *expected, value); }
    void operator()(int value) const { mismatches->integer(key, *expected, value); }
    void operator()(double value) const { mismatches->number(key, *expected, value); }
    void operator()(const std::string& value) const { mismatches->text(key, *expected, value); }
    void operator()(ManufacturerRef value) const
    {
        mismatches->text(key, *expected, value.get().getSimpleName());
    }
    void operator()(ComponentPresetType value) const
    {
        mismatches->text(key, *expected, componentPresetTypeName(value));
    }
    void operator()(TransitionShape value) const
    {
        mismatches->text(key, *expected, transitionShapeName(value));
    }
    void operator()(const Material& value) const { mismatches->material(key, *expected, value); }
    void operator()(Finish value) const { mismatches->text(key, *expected, finishName(value)); }
    void operator()(const std::vector<std::byte>& value) const
    {
        mismatches->text(key, *expected, base64(value));
    }
};

/// The keys of the golden properties @p properties, in their order, joined by ", ".
[[nodiscard]] std::string goldenKeys(const Json& properties)
{
    std::string keys;
    for (const auto& [key, value] : properties.items())
    {
        keys += (keys.empty() ? "" : ", ") + key;
    }
    return keys;
}

/// The keys of @p preset's properties, in their order, joined by ", ".
[[nodiscard]] std::string presetKeys(const ComponentPreset& preset)
{
    std::string keys;
    for (const TypedPropertyMap::Entry& entry : preset.getProperties().entries())
    {
        keys += (keys.empty() ? "" : ", ") + std::string(entry.key.getName());
    }
    return keys;
}

/// Reads and parses presets.json (goldenPresets()).
[[nodiscard]] Result<nlohmann::ordered_json> readGoldenPresets()
{
    const std::filesystem::path file = goldensDir() / "presets" / "presets.json";
    const Result<std::string>   text = readTextFile(file);
    if (!text)
    {
        return std::unexpected(text.error());
    }
    nlohmann::ordered_json json = nlohmann::ordered_json::parse(*text, nullptr, false);
    if (json.is_discarded())
    {
        return fail(ErrorCode::PARSE, pathToUtf8(file) + " is not JSON");
    }
    return json;
}

}  // namespace

const Result<nlohmann::ordered_json>& goldenPresets()
{
    static const Result<nlohmann::ordered_json> kPresets = readGoldenPresets();
    return kPresets;
}

bool hasTranscendentalDensity(const ComponentPreset& preset)
{
    const TransitionShape* const shape = preset.getProperties().get(ComponentPreset::kShape);
    return preset.getType() == ComponentPresetType::NOSE_CONE && shape != nullptr &&
           (*shape == TransitionShape::HAACK || *shape == TransitionShape::POWER) &&
           preset.has(ComponentPreset::kMass);
}

std::string goldenPresetMismatches(const ComponentPreset&        preset,
                                   const nlohmann::ordered_json& golden)
{
    const bool tolerant = hasTranscendentalDensity(preset);
    Mismatches mismatches(preset, tolerant);
    if (!golden.is_object())
    {
        mismatches.add("the golden entry", golden.dump(), "a preset");
        return mismatches.lines();
    }
    mismatches.text("type", golden.value("type", Json()),
                    componentPresetTypeName(preset.getType()));
    mismatches.text("manufacturer", golden.value("manufacturer", Json()),
                    preset.getManufacturer().getSimpleName());
    mismatches.text("manufacturerDisplayName", golden.value("manufacturerDisplayName", Json()),
                    preset.getManufacturer().getDisplayName());
    mismatches.text("partNo", golden.value("partNo", Json()), preset.getPartNo());
    if (!tolerant)
    {
        mismatches.text("digest", golden.value("digest", Json()), preset.getDigest());
    }

    const Json properties = golden.value("properties", Json::object());
    if (goldenKeys(properties) != presetKeys(preset))
    {
        mismatches.add("the properties", goldenKeys(properties), presetKeys(preset));
        return mismatches.lines();
    }
    for (const TypedPropertyMap::Entry& entry : preset.getProperties().entries())
    {
        const Json& expected = properties.at(std::string(entry.key.getName()));
        std::visit(
            PropertyCheck{
                .mismatches = &mismatches, .key = entry.key.getName(), .expected = &expected},
            entry.value);
    }
    return mismatches.lines();
}

}  // namespace QtRocket::Test
