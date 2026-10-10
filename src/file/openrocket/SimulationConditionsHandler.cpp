#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/AtmosphereHandler.h"
#include "QtRocket/file/openrocket/CsvLookupHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/GravityHandler.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/openrocket/WindHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The preference store of @p context, which a handler of conditions cannot do without.
[[nodiscard]] Preferences& preferencesOf(const DocumentLoadingContext& context)
{
    Preferences* preferences = context.getPreferences();
    if (preferences == nullptr)
    {
        bug("The loading context has no preference store");
    }
    return *preferences;
}

/// What a child that is one number must be to be applied. Where OpenRocket tests for a NaN
/// these test for a number that is not finite (decision U3), and every number must lie in the
/// range a simulation holds its option to (decision U9; SimulationInputRanges).
enum class Rule
{
    /// Finite; otherwise the element's warning.
    FINITE,
    /// Finite and above zero; otherwise the element's warning.
    POSITIVE,
    /// Finite and above zero; otherwise nothing, but for what OpenRocket would store: for
    /// positive infinity LoaderWarnings::optionNotFinite(), and for a number above the range
    /// LoaderWarnings::optionOutOfRange().
    POSITIVE_SILENT,
};

/// A child of <conditions> that is one number.
struct NumberElement
{
    std::string_view name;
    Rule             rule;
    /// The range the number has to lie in besides, or null where the option's setter keeps
    /// the option in its range itself (it clamps), or apply looks at what the option would
    /// hold.
    const SimulationInputRanges::Range* range;
    /// Applies the number, which is as the rule and the range want it, to the options. False
    /// when it was not applied because an option would not be finite or not in its range with
    /// it; the element's warning is then given.
    bool (*apply)(SimulationOptions& options, double d);
    /// The warning for a number that is refused; none under Rule::POSITIVE_SILENT.
    std::string_view warning;
};

/// Hands the number to the setter @p Setter of the options.
template <void (SimulationOptions::*Setter)(double)>
bool set(SimulationOptions& options, double d)
{
    (options.*Setter)(d);
    return true;
}

/// Hands the number to the setter @p Setter of the average wind model of the options, unless
/// the model would then hold a number that is not finite or not in its range.
template <void (PinkNoiseWindModel::*Setter)(double)>
bool setAverageWind(SimulationOptions& options, double d)
{
    return WindHandler::setAverageWind(options, Setter, d) == WindHandler::AverageWind::APPLIED;
}

/// <launchrodangle>: degrees. Beyond about 5.7e307 degrees the angle in radians is past the
/// range of a double; the options clamp that infinity to 60 degrees like every large angle.
bool setLaunchRodAngleInDegrees(SimulationOptions& options, double d)
{
    options.setLaunchRodAngle(d * std::numbers::pi / 180);
    return true;
}

/// <launchroddirection>: degrees. The options store the direction reduced to one turn, and
/// that is what has to lie in the range of a direction: a huge number is reduced to nonsense
/// (1.7e308 rad to -2e292 rad), and beyond about 2.8e307 degrees the direction in radians is
/// past the range of a double, which reduced is a NaN. OpenRocket stores either; here the
/// number is refused (decisions U3 and U9).
bool setLaunchRodDirectionInDegrees(SimulationOptions& options, double d)
{
    const double direction = d * 2.0 * std::numbers::pi / 360;
    if (!SimulationInputRanges::kDirection.contains(MathUtil::reduce2Pi(direction)))
    {
        return false;
    }
    options.setLaunchRodDirection(direction);
    return true;
}

/// <launchaltitude>: the options store nothing above the highest site of the atmospheric
/// model, as in OpenRocket, so only an altitude below the range is refused.
bool setLaunchAltitude(SimulationOptions& options, double d)
{
    if (d < SimulationInputRanges::kLaunchAltitude.min)
    {
        return false;
    }
    options.setLaunchAltitude(d);
    return true;
}

constexpr std::array<NumberElement, 15> kNumberElements{{
    {.name    = "launchrodlength",
     .rule    = Rule::FINITE,
     .range   = &SimulationInputRanges::kLaunchRodLength,
     .apply   = &set<&SimulationOptions::setLaunchRodLength>,
     .warning = "Illegal launch rod length defined, ignoring."},
    {.name    = "launchrodangle",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setLaunchRodAngleInDegrees,
     .warning = "Illegal launch rod angle defined, ignoring."},
    {.name    = "launchroddirection",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setLaunchRodDirectionInDegrees,
     .warning = "Illegal launch rod direction defined, ignoring."},

    // The wind elements of OpenRocket 23.09 and older (Java: to be removed once those files
    // are no longer supported). The direction is in radians.
    {.name    = "windaverage",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setAverageWind<&PinkNoiseWindModel::setAverage>,
     .warning = "Illegal average windspeed defined, ignoring."},
    {.name    = "windturbulence",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setAverageWind<&PinkNoiseWindModel::setTurbulenceIntensity>,
     .warning = "Illegal wind turbulence intensity defined, ignoring."},
    {.name    = "winddirection",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setAverageWind<&PinkNoiseWindModel::setDirection>,
     .warning = "Illegal wind direction defined, ignoring."},

    {.name    = "launchaltitude",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &setLaunchAltitude,
     .warning = "Illegal launch altitude defined, ignoring."},
    // The setters clamp to the latitudes and longitudes there are.
    {.name    = "launchlatitude",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &set<&SimulationOptions::setLaunchLatitude>,
     .warning = "Illegal launch latitude defined, ignoring."},
    {.name    = "launchlongitude",
     .rule    = Rule::FINITE,
     .range   = nullptr,
     .apply   = &set<&SimulationOptions::setLaunchLongitude>,
     .warning = "Illegal launch longitude."},
    {.name    = "timestep",
     .rule    = Rule::POSITIVE,
     .range   = &SimulationInputRanges::kTimeStep,
     .apply   = &set<&SimulationOptions::setTimeStep>,
     .warning = "Illegal time step defined, ignoring."},
    {.name    = "maxtime",
     .rule    = Rule::POSITIVE,
     .range   = &SimulationInputRanges::kMaxSimulationTime,
     .apply   = &set<&SimulationOptions::setMaxSimulationTime>,
     .warning = "Illegal max simulation time defined, ignoring."},
    {.name    = "recoveryspeedwarning",
     .rule    = Rule::POSITIVE_SILENT,
     .range   = &SimulationInputRanges::kRecoverySpeedThreshold,
     .apply   = &set<&SimulationOptions::setRecoverySpeedWarning>,
     .warning = {}},
    {.name    = "drogueLowspeedwarning",
     .rule    = Rule::POSITIVE_SILENT,
     .range   = &SimulationInputRanges::kRecoverySpeedThreshold,
     .apply   = &set<&SimulationOptions::setDrogueLowSpeedWarning>,
     .warning = {}},
    {.name    = "recoverydroguemainhighspeedwarning",
     .rule    = Rule::POSITIVE_SILENT,
     .range   = &SimulationInputRanges::kRecoverySpeedThreshold,
     .apply   = &set<&SimulationOptions::setRecoveryDrogueMainHighSpeedWarning>,
     .warning = {}},
    {.name    = "recoverydroguemainlowspeedwarning",
     .rule    = Rule::POSITIVE_SILENT,
     .range   = &SimulationInputRanges::kRecoverySpeedThreshold,
     .apply   = &set<&SimulationOptions::setRecoveryDrogueMainLowSpeedWarning>,
     .warning = {}},
}};

/// Whether the number @p d of the element @p number is one to apply: as its rule and its range
/// want it.
[[nodiscard]] bool isWanted(const NumberElement& number, double d) noexcept
{
    return std::isfinite(d) && (number.rule == Rule::FINITE || d > 0) &&
           (number.range == nullptr || number.range->contains(d));
}

}  // namespace

SimulationConditionsHandler::SimulationConditionsHandler(const DocumentLoadingContext& context)
  : m_options(preferencesOf(context)), m_designDirectory(context.getDesignDirectory())
{
    // Set up default loading settings (which may differ from the new defaults)
    m_options.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
}

Result<ElementHandler*> SimulationConditionsHandler::openElement(std::string_view  element,
                                                                 const Attributes& attributes,
                                                                 WarningSet&       warnings)
{
    const auto                 modelAttribute = attributes.find("model");
    std::optional<std::string> model;
    if (modelAttribute != attributes.end())
    {
        model = modelAttribute->second;
    }

    if (element == "wind")
    {
        m_windHandler =
            std::make_unique<WindHandler>(std::move(model), m_options, attributes, warnings);
        return m_windHandler.get();
    }
    if (element == "atmosphere")
    {
        m_atmosphereHandler = std::make_unique<AtmosphereHandler>(std::move(model));
        return m_atmosphereHandler.get();
    }
    if (element == "gravity")
    {
        m_gravityHandler = std::make_unique<GravityHandler>(std::move(model));
        return m_gravityHandler.get();
    }
    if (element == "draglookup")
    {
        m_dragLookupHandler = std::make_unique<CsvLookupHandler>(
            m_options, std::vector<std::string>{"cd"}, true, m_designDirectory);
        return m_dragLookupHandler.get();
    }
    if (element == "stabilitylookup")
    {
        m_stabilityLookupHandler = std::make_unique<CsvLookupHandler>(
            m_options, std::vector<std::string>{"cn", "cm", "cp"}, false, m_designDirectory);
        return m_stabilityLookupHandler.get();
    }
    return &PlainTextHandler::instance();
}

Result<void> SimulationConditionsHandler::closeElement(std::string_view element,
                                                       const Attributes& /*attributes*/,
                                                       std::string_view content,
                                                       WarningSet&      warnings)
{
    if (element == "windmodeltype")
    {
        const std::optional<WindModelType> type = windModelTypeFromString(content);
        if (!type.has_value())
        {
            // Java: the IllegalArgumentException of WindModelType.fromString(), which ends the
            // load.
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("No enum constant info.openrocket.core.models.wind."
                                    "WindModelType for string value: {}",
                                    content));
        }
        m_options.setWindModelType(*type);
        return {};
    }

    if (closeNumberElement(element, content, warnings) ||
        closeTextElement(element, content, warnings) || closeHandledElement(element, warnings))
    {
        return {};
    }
    // draglookupcsv and stabilitylookupcsv are now handled by CsvLookupHandler. This case is
    // for backward compatibility with old file format (simple text content).
    closeLegacyLookupElement(element, content, warnings);
    return {};
}

bool SimulationConditionsHandler::closeNumberElement(std::string_view element,
                                                     std::string_view content, WarningSet& warnings)
{
    // Through a span, whose iterator is a class with every standard library.
    const std::span<const NumberElement> numbers(kNumberElements);
    const auto number = std::ranges::find(numbers, element, &NumberElement::name);
    if (number == numbers.end())
    {
        return false;
    }

    const double d =
        Strings::javaParseDouble(content).value_or(std::numeric_limits<double>::quiet_NaN());
    if (isWanted(*number, d) && number->apply(m_options, d))
    {
        return true;
    }
    if (number->rule != Rule::POSITIVE_SILENT)
    {
        warnings.add(number->warning);
    }
    else if (d > 0)
    {
        // What OpenRocket stores, positive infinity and a number above the range: refused,
        // and said.
        warnings.add(std::isfinite(d) ? LoaderWarnings::optionOutOfRange(element)
                                      : LoaderWarnings::optionNotFinite(element));
    }
    return true;
}

bool SimulationConditionsHandler::closeTextElement(std::string_view element,
                                                   std::string_view content, WarningSet& warnings)
{
    if (element == "configid")
    {
        m_idToSet = FlightConfigurationId::fromString(content);
    }
    else if (element == "launchintowind")
    {
        // Boolean.parseBoolean
        m_options.setLaunchIntoWind(Strings::javaEqualsIgnoreCase(content, "true"));
    }
    else if (element == "geodeticmethod")
    {
        const std::optional<GeodeticComputationStrategy> gcs = DocumentConfig::findEnum(
            content, kAllGeodeticComputationStrategies,
            [](GeodeticComputationStrategy strategy) { return name(strategy); });
        if (gcs.has_value())
        {
            m_options.setGeodeticComputation(*gcs);
        }
        else
        {
            warnings.add(std::format("Unknown geodetic computation method '{}'", content));
        }
    }
    else if (element == "simulationsteppermethod")
    {
        const std::optional<SimulationStepperMethod> stepperMethod = DocumentConfig::findEnum(
            content, kAllSimulationStepperMethods, &simulationStepperMethodName);
        if (stepperMethod.has_value())
        {
            // This also stores the choice in the preference store (see the class comment).
            m_options.setSimulationStepperMethodChoice(*stepperMethod);
        }
        else
        {
            warnings.add(std::format("Unknown Simulation Stepper '{}'", content));
        }
    }
    else if (element == "randomseed")
    {
        if (const std::optional<int> seed = Strings::parseInt(Strings::trim(content)))
        {
            m_options.setRandomSeed(*seed);
            m_options.setRandomSeedFixed(true);
        }
        else
        {
            warnings.add("Illegal random seed defined, ignoring.");
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool SimulationConditionsHandler::closeHandledElement(std::string_view element,
                                                      WarningSet&      warnings)
{
    // A handler is always there when its element closes: openElement() made it.
    if (element == "wind")
    {
        if (m_windHandler != nullptr)
        {
            m_windHandler->storeSettings(m_options, warnings);
        }
    }
    else if (element == "atmosphere")
    {
        if (m_atmosphereHandler != nullptr)
        {
            m_atmosphereHandler->storeSettings(m_options, warnings);
        }
    }
    else if (element == "gravity")
    {
        if (m_gravityHandler != nullptr)
        {
            m_gravityHandler->storeSettings(m_options, warnings);
        }
    }
    else
    {
        return false;
    }
    return true;
}

void SimulationConditionsHandler::closeLegacyLookupElement(std::string_view element,
                                                           std::string_view content,
                                                           WarningSet&      warnings)
{
    const bool isDrag = element == "draglookupcsv";
    if (!isDrag && element != "stabilitylookupcsv")
    {
        return;
    }
    // Only handle if we didn't use the CsvLookupHandler (old format)
    if ((isDrag ? m_dragLookupHandler : m_stabilityLookupHandler) != nullptr)
    {
        return;
    }
    const std::string_view trimmed = Strings::trim(content);
    if (trimmed.empty())
    {
        return;
    }
    // Not OpenRocket's, which reads any path: only a file inside the design's directory (see
    // CsvLookupHandler).
    const std::expected<std::filesystem::path, CsvLookupHandler::Refusal> csvPath =
        CsvLookupHandler::fileOfDesign(m_designDirectory, pathFromUtf8(trimmed));
    if (!csvPath.has_value())
    {
        warnings.add(CsvLookupHandler::refusalWarning(csvPath.error(), element, trimmed));
        return;
    }
    const Result<void> loaded = CsvLookupHandler::loadFromFile(m_options, *csvPath, isDrag);
    if (!loaded)
    {
        warnings.add(std::format("Failed to load {} lookup CSV '{}', ignoring. Reason: {}",
                                 isDrag ? "drag" : "stability", trimmed, loaded.error().message));
    }
}

}  // namespace QtRocket
