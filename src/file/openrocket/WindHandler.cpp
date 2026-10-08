#include "QtRocket/file/openrocket/WindHandler.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr std::string_view kAverage    = "average";
constexpr std::string_view kMultiLevel = "multilevel";

/// The attributes of a <windlevel>, in the order Java reads them.
constexpr std::array<std::string_view, 4> kLevelAttributes{"altitude", "speed", "direction",
                                                           "standarddeviation"};

}  // namespace

WindHandler::WindHandler(std::optional<std::string> model, SimulationOptions& options,
                         const Attributes& attributes, WarningSet& warnings)
  : m_model(std::move(model)), m_options(&options)
{
    if (m_model != kMultiLevel)
    {
        return;
    }
    // For multilevel wind model, clear the levels (clear the initial level) to fill it up with
    // actual data
    MultiLevelPinkNoiseWindModel& multiLevel = options.getMultiLevelWindModel();
    multiLevel.clearLevels();

    // Set the altitude reference
    const auto reference = attributes.find("altituderef");
    if (reference == attributes.end())
    {
        return;
    }
    std::optional<WindModel::AltitudeReference> altitudeReference =
        altitudeReferenceFromString(reference->second);
    if (!altitudeReference.has_value())
    {
        // Not OpenRocket's, which stores null here (see the class comment).
        warnings.add(
            std::format("Unknown wind altitude reference '{}', using MSL.", reference->second));
        altitudeReference = WindModel::AltitudeReference::MSL;
    }
    multiLevel.setAltitudeReference(*altitudeReference);
}

Result<ElementHandler*> WindHandler::openElement(std::string_view /*element*/,
                                                 const Attributes& /*attributes*/,
                                                 WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> WindHandler::closeElement(std::string_view element, const Attributes& attributes,
                                       std::string_view content, WarningSet& warnings)
{
    if (m_model == kAverage)
    {
        closeAverageElement(element, content, warnings);
    }
    else if (m_model == kMultiLevel && element == "windlevel")
    {
        return closeWindLevel(attributes, warnings);
    }
    return {};
}

void WindHandler::closeAverageElement(std::string_view element, std::string_view content,
                                      WarningSet& warnings)
{
    void (PinkNoiseWindModel::*setter)(double) = nullptr;
    if (element == "speed")
    {
        setter = &PinkNoiseWindModel::setAverage;
    }
    else if (element == "direction")
    {
        setter = &PinkNoiseWindModel::setDirection;
    }
    else if (element == "standarddeviation")
    {
        setter = &PinkNoiseWindModel::setStandardDeviation;
    }
    else
    {
        return;
    }
    const std::optional<double> d = Strings::javaParseDouble(content);
    if (!d.has_value() || std::isnan(*d))
    {
        return;
    }
    // Not OpenRocket's, which stores an infinity and what the model makes of it (see the class
    // comment).
    if (std::isinf(*d) || !setAverageWind(*m_options, setter, *d))
    {
        warnings.add(Warning::kFileInvalidParameter);
    }
}

bool WindHandler::setAverageWind(SimulationOptions& options,
                                 void (PinkNoiseWindModel::*setter)(double), double value)
{
    PinkNoiseWindModel& average = options.getAverageWindModel();
    // Tried on a copy first: a setter has no way back.
    PinkNoiseWindModel trial(average);
    (trial.*setter)(value);
    if (!std::isfinite(trial.getAverage()) || !std::isfinite(trial.getDirection()) ||
        !std::isfinite(trial.getStandardDeviation()))
    {
        return false;
    }
    (average.*setter)(value);
    return true;
}

Result<void> WindHandler::closeWindLevel(const Attributes& attributes, WarningSet& warnings)
{
    // altitude, speed, direction, standard deviation
    std::array<double, kLevelAttributes.size()> values{};
    for (std::size_t i = 0; i < kLevelAttributes.size(); i++)
    {
        const auto attribute = attributes.find(kLevelAttributes.at(i));
        if (attribute == attributes.end())
        {
            // Java: a NullPointerException. The level is skipped (see the class comment).
            warnings.add(Warning::kFileInvalidParameter);
            return {};
        }
        const Result<double> value = DocumentConfig::parseDouble(attribute->second);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        values.at(i) = *value;
    }
    for (const double value : values)
    {
        if (!std::isfinite(value))
        {
            // Not OpenRocket's, which adds such a level (see the class comment).
            warnings.add(Warning::kFileInvalidParameter);
            return {};
        }
    }
    return m_options->getMultiLevelWindModel().addWindLevel(values[0], values[1], values[2],
                                                            values[3]);
}

void WindHandler::storeSettings(SimulationOptions& options, WarningSet& warnings) const
{
    if (m_model == kAverage)
    {
        options.setWindModelType(WindModelType::AVERAGE);
    }
    else if (m_model == kMultiLevel)
    {
        options.setWindModelType(WindModelType::MULTI_LEVEL);
    }
    else
    {
        warnings.add(std::format("Unknown wind model type '{}', using default.",
                                 m_model.has_value() ? std::string_view(*m_model) : "null"));
    }
}

}  // namespace QtRocket
