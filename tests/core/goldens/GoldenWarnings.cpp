#include "goldens/GoldenWarnings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// The complete nlohmann::json, whose members the comparisons call; the include-cleaner check
// counts only the name, which json_fwd.hpp declares.
// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"

namespace QtRocket::Test
{

namespace
{

/// The keys of a golden warning.
constexpr std::array<std::string_view, 6> kWarningKeys{"type", "priority", "description",
                                                       "text", "sources",  "parameter"};

/// The paths of the golden list @p sources, separated by spaces.
[[nodiscard]] std::string joinedPaths(const nlohmann::json& sources)
{
    std::string paths;
    for (const nlohmann::json& source : sources)
    {
        paths += paths.empty() ? "" : " ";
        paths += source.get<std::string>();
    }
    return paths;
}

/// The speed of @p warning when it is of the class @p T, which has one.
template <class T>
[[nodiscard]] std::optional<double> speedOf(const Warning& warning)
{
    if (const auto* typed = dynamic_cast<const T*>(&warning))
    {
        return typed->speed();
    }
    return std::nullopt;
}

}  // namespace

void noteUncomparedKeys(GoldenMismatches& m, std::string_view what, const nlohmann::json& object,
                        std::span<const std::string_view> compared)
{
    for (const auto& [key, value] : object.items())
    {
        if (std::ranges::find(compared, std::string_view{key}) != compared.end())
        {
            continue;
        }
        m.note(what.empty() ? std::format("{}: not compared", key)
                            : std::format("{}.{}: not compared", what, key));
    }
}

std::optional<double> parameterOf(const Warning& warning)
{
    if (const auto* largeAoa = dynamic_cast<const Warning::LargeAOA*>(&warning))
    {
        return largeAoa->aoa();
    }
    for (const std::optional<double> speed :
         {speedOf<Warning::RecoveryHighSpeedDeployment>(warning),
          speedOf<Warning::HighSpeedMainDeployment>(warning),
          speedOf<Warning::LowSpeedMainDeployment>(warning),
          speedOf<Warning::LowSpeedDrogueDeployment>(warning)})
    {
        if (speed.has_value())
        {
            return speed;
        }
    }
    return std::nullopt;
}

std::string sourcePaths(const Warning& warning, const Rocket& rocket)
{
    std::string paths;
    for (const MessageSource& source : warning.sources())
    {
        const RocketComponent* component = rocket.findComponent(source.id);
        paths += paths.empty() ? "" : " ";
        paths += component != nullptr ? goldenPathOf(*component) : std::string{"?"};
    }
    return paths;
}

void compareWarningIdentity(GoldenMismatches& m, const std::string& field,
                            const nlohmann::json& expected, const Warning& actual,
                            const Rocket& rocket)
{
    m.text(field + ".type", expected.at("type").get<std::string>(), actual.typeName());
    m.text(field + ".priority", expected.at("priority").get<std::string>(),
           exportLabel(actual.priority()));
    m.text(field + ".sources", joinedPaths(expected.at("sources")), sourcePaths(actual, rocket));
    noteUncomparedKeys(m, field, expected, kWarningKeys);
}

void compareWarning(GoldenMismatches& m, const std::string& field, const nlohmann::json& expected,
                    const Warning& actual, const Rocket& rocket, double parameterTolerance)
{
    m.text(field + ".type", expected.at("type").get<std::string>(), actual.typeName());
    m.text(field + ".priority", expected.at("priority").get<std::string>(),
           exportLabel(actual.priority()));
    m.text(field + ".description", expected.at("description").get<std::string>(),
           actual.messageDescription());
    m.text(field + ".text", expected.at("text").get<std::string>(), actual.toString());
    m.text(field + ".sources", joinedPaths(expected.at("sources")), sourcePaths(actual, rocket));

    const std::optional<double> parameter = parameterOf(actual);
    m.boolean(field + " has a parameter", expected.contains("parameter"), parameter.has_value());
    if (expected.contains("parameter") && parameter.has_value())
    {
        m.within(field + ".parameter", goldenValue(expected.at("parameter")), *parameter,
                 parameterTolerance, 0.0);
    }
    noteUncomparedKeys(m, field, expected, kWarningKeys);
}

int compareWarnings(GoldenMismatches& m, std::string_view field, const nlohmann::json& expected,
                    const WarningSet& actual, const Rocket& rocket, double parameterTolerance)
{
    m.integer(std::format("{}: number", field), static_cast<std::int64_t>(expected.size()),
              static_cast<std::int64_t>(actual.size()));
    int         compared = 0;
    std::size_t index    = 0;
    for (const Warning& warning : actual)
    {
        if (index < expected.size())
        {
            compareWarning(m, std::format("{}[{}]", field, index), expected.at(index), warning,
                           rocket, parameterTolerance);
            compared++;
        }
        else
        {
            m.note(std::format("{}[{}]: not in the golden file: {}", field, index,
                               warning.toString()));
        }
        index++;
    }
    return compared;
}

}  // namespace QtRocket::Test
