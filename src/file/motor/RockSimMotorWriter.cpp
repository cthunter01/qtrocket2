#include "QtRocket/file/motor/RockSimMotorWriter.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

[[nodiscard]] std::string_view motorTypeString(Motor::Type type)
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "single-use";
        case Motor::Type::HYBRID:
            return "hybrid";
        case Motor::Type::RELOAD:
            return "reloadable";
        case Motor::Type::UNKNOWN:
            return "unknown";
    }
    QTROCKET_UNREACHABLE();
}

[[nodiscard]] std::string delaysString(std::span<const double> delays)
{
    std::string sb;
    for (std::size_t i = 0; i < delays.size(); i++)
    {
        if (i > 0)
        {
            sb += ',';
        }
        if (delays[i] == Motor::kPluggedDelay)
        {
            sb += "1000";
        }
        else
        {
            sb += ThrustCurveMotor::getDelayString(delays[i], "1000");
        }
    }
    return sb;
}

/// OpenRocket's escapeXmlAttr: &, ", < and > as entities. One pass gives the same result as its
/// chain of replace() calls, which starts with '&'.
[[nodiscard]] std::string escapeXmlAttr(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

/// OpenRocket's escapeXmlContent: &, < and > as entities.
[[nodiscard]] std::string escapeXmlContent(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

/// One attribute written with TextUtil.doubleToString: ` name="value"`.
void appendNumber(std::string& sb, std::string_view name, double value)
{
    sb += ' ';
    sb += name;
    sb += "=\"";
    sb += Strings::doubleToString(value);
    sb += '"';
}

void writeData(std::string& sb, const ThrustCurveMotor& motor)
{
    const std::vector<double>&     time     = motor.getTimePoints();
    const std::vector<double>&     thrust   = motor.getThrustPoints();
    const std::vector<Coordinate>& cgPoints = motor.getCGPoints();
    const std::size_t              count = std::min({time.size(), thrust.size(), cgPoints.size()});

    sb += "   <data>\n";
    for (std::size_t i = 0; i < count; i++)
    {
        const Coordinate& cg        = cgPoints[i];
        const double      massGrams = cg.weight * 1000.0;
        const double      cgMM      = cg.x * 1000.0;

        sb += "    <eng-data";
        appendNumber(sb, "t", time[i]);
        appendNumber(sb, "f", thrust[i]);
        appendNumber(sb, "m", massGrams);
        appendNumber(sb, "cg", cgMM);
        sb += "/>\n";
    }
    sb += "   </data>\n";
}

void writeEngine(std::string& sb, const ThrustCurveMotor& motor)
{
    const double diameterMM  = motor.getDiameter() * 1000.0;
    const double lengthMM    = motor.getLength() * 1000.0;
    const double initWtGrams = motor.getLaunchMass() * 1000.0;
    const double propWtGrams = motor.getPropellantMass() * 1000.0;

    sb += "  <engine";
    sb += " mfg=\"" + escapeXmlAttr(motor.getManufacturer().getSimpleName()) + "\"";
    sb += " code=\"" + escapeXmlAttr(motor.getDesignation()) + "\"";
    sb += " Type=\"";
    sb += motorTypeString(motor.getMotorType());
    sb += "\"";
    appendNumber(sb, "dia", diameterMM);
    appendNumber(sb, "len", lengthMM);
    appendNumber(sb, "initWt", initWtGrams);
    appendNumber(sb, "propWt", propWtGrams);

    // Delays
    const std::vector<double>& delays = motor.getStandardDelays();
    if (!delays.empty())
    {
        sb += " delays=\"" + delaysString(delays) + "\"";
    }

    // Explicit mass and CG data
    sb += " auto-calc-mass=\"0\"";
    sb += " auto-calc-cg=\"0\"";

    sb += ">\n";

    // Description
    const std::string& desc = motor.getDescription();
    if (!desc.empty())
    {
        sb += "   <comments>" + escapeXmlContent(desc) + "</comments>\n";
    }

    // Thrust curve data
    writeData(sb, motor);

    sb += "  </engine>\n";
}

}  // namespace

std::string RockSimMotorWriter::write(const ThrustCurveMotor& motor)
{
    std::string sb;
    sb += "<engine-database>\n";
    sb += " <engine-list>\n";
    writeEngine(sb, motor);
    sb += " </engine-list>\n";
    sb += "</engine-database>\n";
    return sb;
}

}  // namespace QtRocket
