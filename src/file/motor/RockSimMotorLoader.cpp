#include "QtRocket/file/motor/RockSimMotorLoader.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/NullElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Attributes = ElementHandler::Attributes;

/// The attribute @p name of @p attributes, when present.
[[nodiscard]] std::optional<std::string_view> attribute(const Attributes& attributes,
                                                        std::string_view  name)
{
    const auto it = attributes.find(name);
    if (it == attributes.end())
    {
        return std::nullopt;
    }
    return it->second;
}

/// RSEMotorDataHandler.parseDouble(): the number, or NaN when missing or not a number.
[[nodiscard]] double parseOrNaN(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return Strings::javaParseDouble(*text).value_or(std::numeric_limits<double>::quiet_NaN());
}

/// RockSimMotorLoader.hasIllegalValue(): a NaN or infinite value.
[[nodiscard]] bool hasIllegalValue(const std::vector<double>& list)
{
    return std::ranges::any_of(list, [](double d) { return std::isnan(d) || std::isinf(d); });
}

/// "0" or "false" (any case) turns an automatic calculation off; anything else, absence included,
/// leaves it on.
[[nodiscard]] bool isAutomatic(std::optional<std::string_view> value)
{
    return !(value.has_value() &&
             (*value == "0" || Strings::javaEqualsIgnoreCase(*value, "false")));
}

/// The engine's delays attribute: comma-separated numbers, those from
/// RockSimMotorLoader::kDelayLimit up plugged; "P" or "plugged" as the whole attribute is plugged.
[[nodiscard]] std::vector<double> parseDelays(std::optional<std::string_view> delays)
{
    std::vector<double> parsed;
    if (!delays.has_value())
    {
        return parsed;
    }
    for (const std::string& delay : Strings::splitJava(*delays, ','))
    {
        if (const std::optional<double> value = Strings::javaParseDouble(delay))
        {
            parsed.push_back(*value >= RockSimMotorLoader::kDelayLimit ? Motor::kPluggedDelay
                                                                       : *value);
        }
        // OpenRocket tests the whole attribute here, not the piece that failed to parse, so a "P"
        // among other delays is dropped.
        else if (Strings::javaEqualsIgnoreCase(*delays, "P") ||
                 Strings::javaEqualsIgnoreCase(*delays, "plugged"))
        {
            parsed.push_back(Motor::kPluggedDelay);
        }
    }
    return parsed;
}

/// The attribute @p name, a length in mm or a mass in g, in m or kg; OpenRocket's @p missing
/// message when it is absent, "Invalid <what> <value>" when it is no number.
[[nodiscard]] Result<double> readNumber(const Attributes& attributes, std::string_view name,
                                        std::string_view what, std::string_view missing)
{
    const std::optional<std::string_view> text = attribute(attributes, name);
    if (!text.has_value())
    {
        return fail(ErrorCode::PARSE, std::string(missing));
    }
    const std::optional<double> value = Strings::javaParseDouble(*text);
    if (!value.has_value())
    {
        return fail(ErrorCode::PARSE, std::format("Invalid {} {}", what, *text));
    }
    return *value / 1000.0;
}

/// The engine's Type attribute: the type, and whether it is explicitly "unknown".
[[nodiscard]] std::pair<Motor::Type, bool> parseType(std::optional<std::string_view> type)
{
    const auto is = [&type](std::string_view name) {
        return type.has_value() && Strings::javaEqualsIgnoreCase(*type, name);
    };
    const bool explicitlyUnknown = is("unknown");
    if (is("single-use"))
    {
        return {Motor::Type::SINGLE, explicitlyUnknown};
    }
    if (is("hybrid"))
    {
        return {Motor::Type::HYBRID, explicitlyUnknown};
    }
    if (is("reloadable"))
    {
        return {Motor::Type::RELOAD, explicitlyUnknown};
    }
    return {Motor::Type::UNKNOWN, explicitlyUnknown};
}

}  // namespace

/// Handler for the <data> element in a RockSim engine file motor definition.
class RockSimMotorLoader::RseMotorDataHandler final : public AbstractElementHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& warnings) override
    {
        if (element == "eng-data")
        {
            return &NullElementHandler::instance();
        }

        warnings.add("Unknown element '" + std::string(element) + "' encountered, ignoring.");
        return nullptr;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view /*element*/,
                                            const Attributes& attributes,
                                            std::string_view /*content*/,
                                            WarningSet& /*warnings*/) override
    {
        const double t = parseOrNaN(attribute(attributes, "t"));
        const double f = parseOrNaN(attribute(attributes, "f"));
        const double m = parseOrNaN(attribute(attributes, "m")) / 1000.0;
        const double g = parseOrNaN(attribute(attributes, "cg")) / 1000.0;

        if (std::isnan(t) || std::isnan(f))
        {
            return fail(ErrorCode::PARSE, "Illegal motor data point encountered");
        }

        m_time.push_back(t);
        m_force.push_back(f);
        m_mass.push_back(m);
        m_cg.push_back(g);
        return {};
    }

    std::vector<double> takeTime() { return std::exchange(m_time, {}); }
    std::vector<double> takeForce() { return std::exchange(m_force, {}); }
    std::vector<double> takeMass() { return std::exchange(m_mass, {}); }
    std::vector<double> takeCg() { return std::exchange(m_cg, {}); }

private:
    std::vector<double> m_time;
    std::vector<double> m_force;
    std::vector<double> m_mass;
    std::vector<double> m_cg;
};

/// Handler for a RockSim engine file <engine> element.
class RockSimMotorLoader::RseMotorHandler final : public AbstractElementHandler
{
public:
    /// An engine without attributes; create() reads them.
    RseMotorHandler() = default;

    /// The handler of an engine with @p attributes, or OpenRocket's SAXException for a missing or
    /// invalid one.
    [[nodiscard]] static Result<std::unique_ptr<RseMotorHandler>> create(
        const Attributes& attributes)
    {
        auto handler = std::make_unique<RseMotorHandler>();
        if (Result<void> read = handler->readAttributes(attributes); !read)
        {
            return std::unexpected(std::move(read.error()));
        }
        return handler;
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& warnings) override
    {
        if (element == "comments")
        {
            return &PlainTextHandler::instance();
        }

        if (element == "data")
        {
            if (m_dataHandler != nullptr)
            {
                return fail(ErrorCode::PARSE,
                            "Multiple data elements encountered in motor definition");
            }
            m_dataHandler = std::make_unique<RseMotorDataHandler>();
            return m_dataHandler.get();
        }

        warnings.add("Unknown element '" + std::string(element) + "' encountered, ignoring.");
        return nullptr;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element,
                                            const Attributes& /*attributes*/,
                                            std::string_view content,
                                            WarningSet& /*warnings*/) override
    {
        if (element == "comments")
        {
            if (!m_description.empty())
            {
                m_description += "\n\n";
            }
            m_description += Strings::trim(content);
            return {};
        }

        if (element == "data")
        {
            QTROCKET_ASSERT(m_dataHandler != nullptr);
            m_time    = m_dataHandler->takeTime();
            m_force   = m_dataHandler->takeForce();
            m_mass    = m_dataHandler->takeMass();
            m_cg      = m_dataHandler->takeCg();
            m_hasData = true;

            sortLists(m_time, {m_force, m_mass, m_cg});

            if (std::ranges::any_of(m_mass, [](double d) { return std::isnan(d); }))
            {
                m_calculateMass = true;
            }
            if (std::ranges::any_of(m_cg, [](double d) { return std::isnan(d); }))
            {
                m_calculateCg = true;
            }
        }
        return {};
    }

    /// The motor (getMotor()), or OpenRocket's failure for missing or unusable data.
    [[nodiscard]] Result<ThrustCurveMotor::Builder> getMotor()
    {
        if (!m_hasData || m_time.empty())
        {
            return fail(ErrorCode::PARSE, "Illegal motor data");
        }

        if (Result<void> finished = finalizeThrustCurve(m_time, m_force, {m_mass, m_cg}); !finished)
        {
            return std::unexpected(std::move(finished.error()));
        }

        // The thrusts are removed with the times, so they stay as many.
        const std::size_t n = m_time.size();
        QTROCKET_ASSERT(m_force.size() == n);

        if (hasIllegalValue(m_mass))
        {
            m_calculateMass = true;
        }
        if (hasIllegalValue(m_cg))
        {
            m_calculateCg = true;
        }

        if (m_calculateMass)
        {
            m_mass = calculateMass(m_time, m_force, m_initMass, m_propMass);
        }

        if (m_calculateCg)
        {
            for (std::size_t i = 0; i < n; i++)
            {
                m_cg[i] = m_length / 2;
            }
        }

        std::vector<Coordinate> cgArray;
        cgArray.reserve(n);
        for (std::size_t i = 0; i < n; i++)
        {
            cgArray.emplace_back(m_cg[i], 0, 0, m_mass[i]);
        }

        // Create the motor digest from all data available in the file. The mass and CG lists can
        // be one longer than the curve (see finalizeThrustCurve) and are digested whole, as in
        // OpenRocket.
        MotorDigest motorDigest;
        motorDigest.update(MotorDigest::DataType::TIME_ARRAY, m_time);
        if (!m_calculateMass)
        {
            motorDigest.update(MotorDigest::DataType::MASS_PER_TIME, m_mass);
        }
        else
        {
            motorDigest.update(MotorDigest::DataType::MASS_SPECIFIC,
                               {m_initMass, m_initMass - m_propMass});
        }
        if (!m_calculateCg)
        {
            motorDigest.update(MotorDigest::DataType::CG_PER_TIME, m_cg);
        }
        motorDigest.update(MotorDigest::DataType::FORCE_PER_TIME, m_force);

        const Manufacturer& m = Manufacturer::getManufacturer(m_manufacturer);
        Motor::Type         t = m_type;
        // Preserve an explicit unknown type. Manufacturer inference remains useful for older RSE
        // files that omit the type or contain an unrecognized value. (OpenRocket logs a warning
        // when the type contradicts the manufacturer's; that log is not ported.)
        if (t == Motor::Type::UNKNOWN && !m_explicitlyUnknownType)
        {
            t = m.getMotorType();
        }

        ThrustCurveMotor::Builder builder;
        builder.setManufacturer(m)
            .setDesignation(m_designation)
            .setDescription(m_description)
            .setMotorType(t)
            .setStandardDelays(m_delays)
            .setDiameter(m_diameter)
            .setLength(m_length)
            .setTimePoints(m_time)
            .setThrustPoints(m_force)
            .setCGPoints(std::move(cgArray))
            .setDigest(motorDigest.getDigest());
        return builder;
    }

private:
    /// The constructor of RSEMotorHandler: reads the engine's attributes in OpenRocket's order.
    [[nodiscard]] Result<void> readAttributes(const Attributes& attributes)
    {
        // Manufacturer
        const std::optional<std::string_view> mfg = attribute(attributes, "mfg");
        if (!mfg.has_value())
        {
            return fail(ErrorCode::PARSE, "Manufacturer missing");
        }
        m_manufacturer = *mfg;

        // Designation
        const std::optional<std::string_view> code = attribute(attributes, "code");
        if (!code.has_value())
        {
            return fail(ErrorCode::PARSE, "Designation missing");
        }
        m_designation = removeDelay(*code);

        // Delays
        m_delays = parseDelays(attribute(attributes, "delays"));

        // Diameter, length, initial mass and propellant mass
        const Result<double> diameter =
            readNumber(attributes, "dia", "diameter", "Diameter missing");
        if (!diameter)
        {
            return std::unexpected(diameter.error());
        }
        m_diameter                  = *diameter;
        const Result<double> length = readNumber(attributes, "len", "length", "Length missing");
        if (!length)
        {
            return std::unexpected(length.error());
        }
        m_length = *length;
        const Result<double> initMass =
            readNumber(attributes, "initWt", "initial mass", "Initial mass missing");
        if (!initMass)
        {
            return std::unexpected(initMass.error());
        }
        m_initMass = *initMass;
        const Result<double> propMass =
            readNumber(attributes, "propWt", "propellant mass", "Propellant mass missing");
        if (!propMass)
        {
            return std::unexpected(propMass.error());
        }
        m_propMass = *propMass;

        if (m_propMass > m_initMass)
        {
            return fail(ErrorCode::PARSE,
                        "Propellant weight exceeds total weight in RockSim engine format");
        }

        // Motor type
        std::tie(m_type, m_explicitlyUnknownType) = parseType(attribute(attributes, "Type"));

        // Calculate mass and CG
        m_calculateMass = isAutomatic(attribute(attributes, "auto-calc-mass"));
        m_calculateCg   = isAutomatic(attribute(attributes, "auto-calc-cg"));
        return {};
    }

    std::string         m_manufacturer;
    std::string         m_designation;
    std::vector<double> m_delays;
    double              m_diameter{0};
    double              m_length{0};
    double              m_initMass{0};
    double              m_propMass{0};
    Motor::Type         m_type{Motor::Type::UNKNOWN};
    bool                m_explicitlyUnknownType{false};
    bool                m_calculateMass{false};
    bool                m_calculateCg{false};

    std::string m_description;

    bool                m_hasData{false};
    std::vector<double> m_time;
    std::vector<double> m_force;
    std::vector<double> m_mass;
    std::vector<double> m_cg;

    std::unique_ptr<RseMotorDataHandler> m_dataHandler;
};

/// Initial handler for the RockSim engine files.
class RockSimMotorLoader::RseHandler final : public AbstractElementHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet& /*warnings*/) override
    {
        if (element == "engine-database" || element == "engine-list")
        {
            // Ignore <engine-database> and <engine-list> elements
            return this;
        }

        if (element == "version")
        {
            // Ignore <version> elements completely
            return nullptr;
        }

        if (element == "engine")
        {
            Result<std::unique_ptr<RseMotorHandler>> handler = RseMotorHandler::create(attributes);
            if (!handler)
            {
                return std::unexpected(std::move(handler.error()));
            }
            m_motorHandler = std::move(*handler);
            return m_motorHandler.get();
        }

        return nullptr;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/,
                                            WarningSet& /*warnings*/) override
    {
        if (element == "engine")
        {
            QTROCKET_ASSERT(m_motorHandler != nullptr);
            Result<ThrustCurveMotor::Builder> motor = m_motorHandler->getMotor();
            if (!motor)
            {
                return std::unexpected(std::move(motor.error()));
            }
            m_motors.push_back(std::move(*motor));
        }
        return {};
    }

    [[nodiscard]] std::vector<ThrustCurveMotor::Builder> takeMotors()
    {
        return std::exchange(m_motors, {});
    }

private:
    std::vector<ThrustCurveMotor::Builder> m_motors;
    std::unique_ptr<RseMotorHandler>       m_motorHandler;
};

Result<std::vector<ThrustCurveMotor::Builder>> RockSimMotorLoader::loadText(
    std::string_view text, std::string_view /*filename*/, WarningSet& warnings)
{
    RseHandler handler;
    if (Result<void> read = SimpleSax::readXml(text, handler, warnings); !read)
    {
        return std::unexpected(std::move(read.error()));
    }
    return handler.takeMotors();
}

Result<std::vector<ThrustCurveMotor::Builder>> RockSimMotorLoader::loadText(
    std::string_view text, std::string_view filename) const
{
    WarningSet warnings;  // OpenRocket discards the handlers' warnings
    return loadText(text, filename, warnings);
}

}  // namespace QtRocket
