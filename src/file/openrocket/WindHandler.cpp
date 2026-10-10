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
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
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
        warnings.add(LoaderWarnings::unknownAltitudeReference(reference->second));
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
    // Not OpenRocket's, which stores an infinity, what the model makes of it, and any finite
    // number (see the class comment).
    if (std::isinf(*d))
    {
        warnings.add(LoaderWarnings::optionNotFinite(element));
        return;
    }
    switch (setAverageWind(*m_options, setter, *d))
    {
        case AverageWind::APPLIED:
            return;
        case AverageWind::NOT_FINITE:
            warnings.add(LoaderWarnings::optionNotFinite(element));
            return;
        case AverageWind::OUT_OF_RANGE:
            warnings.add(LoaderWarnings::optionOutOfRange(element));
            return;
    }
}

WindHandler::AverageWind WindHandler::setAverageWind(SimulationOptions& options,
                                                     void (PinkNoiseWindModel::*setter)(double),
                                                     double value)
{
    PinkNoiseWindModel& average = options.getAverageWindModel();
    // Tried on a copy first: a setter has no way back.
    PinkNoiseWindModel trial(average);
    (trial.*setter)(value);
    if (!std::isfinite(trial.getAverage()) || !std::isfinite(trial.getDirection()) ||
        !std::isfinite(trial.getStandardDeviation()))
    {
        return AverageWind::NOT_FINITE;
    }
    if (!inRange(trial))
    {
        return AverageWind::OUT_OF_RANGE;
    }
    (average.*setter)(value);
    return AverageWind::APPLIED;
}

bool WindHandler::inRange(const PinkNoiseWindModel& wind) noexcept
{
    return SimulationInputRanges::kWindSpeed.contains(wind.getAverage()) &&
           SimulationInputRanges::kDirection.contains(wind.getDirection()) &&
           SimulationInputRanges::kWindSpeedDeviation.contains(wind.getStandardDeviation());
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
            warnings.add(LoaderWarnings::kWindLevelIncomplete);
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
            warnings.add(LoaderWarnings::kWindLevelNotFinite);
            return {};
        }
    }
    // Not OpenRocket's either: a level whose numbers a simulation would refuse for their
    // size. The level is what addWindLevel() would make of the numbers: a negative speed
    // turns the direction, a negative deviation is none. (The seed is of no account here.)
    PinkNoiseWindModel level(0);
    level.setDirection(values[2]);
    level.setAverage(values[1]);
    level.setStandardDeviation(values[3]);
    if (!SimulationInputRanges::kWindLevelAltitude.contains(values[0]) || !inRange(level))
    {
        warnings.add(LoaderWarnings::kWindLevelOutOfRange);
        return {};
    }
    // Not OpenRocket's either: the levels of one element are bounded (see the class comment).
    if (m_levelCount >= DocumentConfig::kMaxWindLevels)
    {
        warnings.add(LoaderWarnings::tooManyWindLevels());
        return {};
    }
    if (Result<void> added = m_options->getMultiLevelWindModel().addWindLevel(values[0], values[1],
                                                                              values[2], values[3]);
        !added)
    {
        return added;
    }
    ++m_levelCount;
    return {};
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
