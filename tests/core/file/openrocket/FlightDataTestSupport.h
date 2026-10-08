#pragma once

// What the tests of the handlers of a simulation's stored results share (WarningHandler,
// FlightDataBranchHandler, FlightDataHandler): a document with a small rocket whose components
// the warnings and events of a case can name, a way to run a <flightdata> or a <warning> element
// through its handler, and a description of the outcome in the notation of the Java probe the
// expectations come from. Test-only.
//
// The probe is FdProbe (run 9b, part S2): it runs the same element through OpenRocket's
// FlightDataHandler (a <flightdata> element) or WarningHandler (a <warning> element) alone, as
// runFlightDataCase() does here, and prints the warnings of the load, the exception, what the
// parent's closeElement() was given, and the flight data or the warning set. Everything is
// printed in ASCII (see ascii()), so that an expectation is a plain string literal.
//
// The flight data types: a column name no type has is registered process-wide under the symbol
// "Unknown" (see FlightDataBranchHandler), and so is the type of a custom expression under its
// symbol. The cases therefore say what they need of that state themselves (the directive
// "@unknown"), use symbols of their own for their custom expressions ("qtr..."), and a
// description names a type by its name, symbol, group, save key and unit, never by identity.

#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/FlightDataHandler.h"
#include "QtRocket/file/openrocket/WarningHandler.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

namespace QtRocket::Test
{

/// The ids of the components of FlightDataFixture's rocket, as it is made: the rocket "R", its
/// stage "S", the body tube "B" in the stage and the parachute "P" in the tube. (FdProbe's
/// rocket has the same components, ids and names.)
inline constexpr std::string_view kFlightRocketId = "aaaaaaaa-0000-0000-0000-000000000001";
inline constexpr std::string_view kFlightStageId  = "aaaaaaaa-0000-0000-0000-000000000002";
inline constexpr std::string_view kFlightTubeId   = "aaaaaaaa-0000-0000-0000-000000000003";
inline constexpr std::string_view kFlightChuteId  = "aaaaaaaa-0000-0000-0000-000000000004";

/// The id @p text is, as java.util.UUID.fromString() reads it.
/// @throws BugError when @p text is no id (a mistake in a test)
[[nodiscard]] inline Uuid uuidOf(std::string_view text)
{
    const Result<Uuid> id = Uuid::javaFromString(text);
    if (!id)
    {
        bug("not a UUID: " + std::string(text));
    }
    return *id;
}

/// @p text in ASCII, as FdProbe prints everything: a line feed as "<LF>", a tab as "<TAB>", a
/// carriage return as "<CR>" and every other character outside the printable ASCII range as
/// "<U+XXXX>" with its code point in at least four hexadecimal digits (a degree sign is
/// "<U+00B0>").
[[nodiscard]] inline std::string ascii(std::string_view text)
{
    std::string result;
    for (const char32_t codePoint : Strings::toCodePoints(text))
    {
        if (codePoint == U'\n')
        {
            result += "<LF>";
        }
        else if (codePoint == U'\t')
        {
            result += "<TAB>";
        }
        else if (codePoint == U'\r')
        {
            result += "<CR>";
        }
        else if (codePoint < 32 || codePoint > 126)
        {
            result += std::format("<U+{:04X}>", static_cast<unsigned int>(codePoint));
        }
        else
        {
            result.push_back(static_cast<char>(codePoint));
        }
    }
    return result;
}

/// How a description prints.
struct FlightDescription
{
    /// Whether every id is printed as it is. Else an id that Uuid::random() made (version 4)
    /// prints as "(random)": every id the loader makes for an element without one is such an
    /// id, and the ids of the cases have version 0.
    bool allIds{false};
    /// Whether the columns of a branch are printed as one digest each in place of the rows.
    bool digest{false};
};

/// An id as a description prints it (see FlightDescription::allIds).
[[nodiscard]] inline std::string describeId(const Uuid& id, const FlightDescription& how)
{
    return how.allIds || id.version() != 4 ? id.toString() : "(random)";
}

/// The component with the id @p id as a description prints the source of a warning or an
/// event: "<name>@<id>" for a component of @p rocket and "<i>Component Removed From
/// Rocket</i>(REMOVED)" for an id that names none (FdProbe prints OpenRocket's removed
/// component so, whose id is not the one of the file).
[[nodiscard]] inline std::string describeSource(const Uuid& id, const Rocket& rocket)
{
    const RocketComponent* const component = rocket.findComponent(id);
    if (component == nullptr)
    {
        return std::string(MessageSource::kRemovedComponentName) + "(REMOVED)";
    }
    return std::format("{}@{}", component->getName(), id.toString());
}

/// One warning of flight data as FdProbe prints it:
/// "fw[<class>,<priority>] id=<id> text='<text with sources>' desc='<text>' sources=<source>;..."
/// followed by " param=<number>" for a LargeAOA (its angle) and a RecoveryHighSpeedDeployment
/// (its speed) and by " event=<event type>@<event id>" (or "event=null") for an
/// EventAfterLanding. The name of a source is the one the warning holds.
[[nodiscard]] inline std::string describeFlightWarning(const Warning& warning, const Rocket& rocket,
                                                       const FlightDescription& how)
{
    std::string sources;
    for (const MessageSource& source : warning.sources())
    {
        sources += rocket.findComponent(source.id) != nullptr
                       ? std::format("{}@{};", source.name, source.id.toString())
                       : std::format("{}(REMOVED);", source.name);
    }
    std::string extra;
    if (const auto* aoa = dynamic_cast<const Warning::LargeAOA*>(&warning))
    {
        extra = " param=" + Strings::javaDoubleToString(aoa->aoa());
    }
    else if (const auto* speed =
                 dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(&warning))
    {
        extra = " param=" + Strings::javaDoubleToString(speed->speed());
    }
    else if (const auto* landing = dynamic_cast<const Warning::EventAfterLanding*>(&warning))
    {
        extra = " event=null";
        if (landing->eventType().has_value() || landing->eventId().has_value())
        {
            extra = std::format(" event={}@{}", landing->eventType().value_or("null"),
                                landing->eventId().has_value()
                                    ? describeId(landing->eventId().value_or(Uuid::nil()), how)
                                    : "null");
        }
    }
    return ascii(std::format("fw[{},{}] id={} text='{}' desc='{}' sources={}{}", warning.typeName(),
                             exportLabel(warning.priority()), describeId(warning.id(), how),
                             warning.toString(), warning.messageDescription(), sources, extra));
}

/// A flight data type as FdProbe prints it: "<name>{<symbol>,<group>,<save key>,<SI unit>}",
/// with "-" for the save key of a type that is not built in.
[[nodiscard]] inline std::string describeFlightDataType(const FlightDataType& type)
{
    const bool builtin = FlightDataType::getTypeBySaveKey(type.getSaveKey()) == &type;
    return std::format("{}{{{},{},{},{}}}", type.getName(), type.getSymbol(),
                       displayName(type.getGroup()), builtin ? type.getSaveKey() : "-",
                       type.getUnitGroup().getSIUnit().getUnit());
}

/// The data of an event as FdProbe prints it between brackets: "null", or the class and the
/// text of the warning or the abort ("Other:A stored warning"). The warning is the one @p data
/// has now for the
/// event (FlightData::findWarning()), which is what OpenRocket's event shows, since it shares
/// the object with the warning set; the event's own copy is as it was when the event was read.
[[nodiscard]] inline std::string describeEventData(const FlightEvent& event, const FlightData& data)
{
    if (const Warning* const current = data.findWarning(event))
    {
        return std::format("{}:{}", current->typeName(), current->toString());
    }
    if (const std::shared_ptr<const Warning> own = event.getWarning())
    {
        return std::format("{}:{}", own->typeName(), own->toString());
    }
    if (const SimulationAbort* const abort = event.getAbort())
    {
        return std::format("{}:{}", abort->typeName(), abort->toString());
    }
    return event.hasData() ? "?" : "null";
}

/// One column of a branch as FdProbe prints its digest: its minimum, maximum, first and last
/// value, the sum of its values that are no NaN, added up in their order, and the number of NaN.
[[nodiscard]] inline std::string describeColumnDigest(const FlightDataBranch& branch,
                                                      const FlightDataType&   type)
{
    const std::vector<double> values = branch.get(type).value_or(std::vector<double>{});
    double                    sum    = 0;
    int                       nans   = 0;
    for (const double value : values)
    {
        if (std::isnan(value))
        {
            nans++;
        }
        else
        {
            sum += value;
        }
    }
    return std::format("col {}: min={} max={} first={} last={} sum={} nans={}", type.getSaveKey(),
                       Strings::javaDoubleToString(branch.getMinimum(type)),
                       Strings::javaDoubleToString(branch.getMaximum(type)),
                       values.empty() ? "-" : Strings::javaDoubleToString(values.front()),
                       values.empty() ? "-" : Strings::javaDoubleToString(values.back()),
                       Strings::javaDoubleToString(sum), nans);
}

/// One row of a branch as FdProbe prints it: "row:" and every value of the row in the order of
/// @p types.
[[nodiscard]] inline std::string describeRow(const FlightDataBranch&                   branch,
                                             const std::vector<const FlightDataType*>& types,
                                             std::size_t                               row)
{
    std::string line = "row:";
    for (const FlightDataType* type : types)
    {
        line += " " + Strings::javaDoubleToString(branch.getByIndex(*type, row).value_or(0.0));
    }
    return line;
}

/// The lines of a branch as FdProbe prints them, each indented by six spaces and ended: the
/// types in the order of FlightDataBranch::getTypes(), the events in their order, and the
/// rows (describeRow()), or in their place one digest per column (describeColumnDigest()).
[[nodiscard]] inline std::string describeBranchLines(const FlightDataBranch& branch,
                                                     const FlightData& data, const Rocket& rocket,
                                                     const FlightDescription& how)
{
    const std::vector<const FlightDataType*> types = branch.getTypes();
    std::string                              names;
    for (const FlightDataType* type : types)
    {
        names += (names.empty() ? "" : " | ") + describeFlightDataType(*type);
    }
    std::string text = std::format("      types={}\n", ascii(names));

    for (const FlightEvent& event : branch.getEvents())
    {
        text += ascii(std::format(
                    "      event {} t={} id={} src={} data=[{}]", name(event.getType()),
                    Strings::javaDoubleToString(event.getTime()), describeId(event.getId(), how),
                    event.getSourceId().has_value()
                        ? describeSource(event.getSourceId().value_or(Uuid::nil()), rocket)
                        : "null",
                    describeEventData(event, data))) +
                "\n";
    }

    if (how.digest)
    {
        for (const FlightDataType* type : types)
        {
            text += "      " + ascii(describeColumnDigest(branch, *type)) + "\n";
        }
        return text;
    }
    for (std::size_t row = 0; row < branch.getLength(); row++)
    {
        text += "      " + describeRow(branch, types, row) + "\n";
    }
    return text;
}

/// Flight data as FdProbe prints them, every line indented and ended: "  data: null" for
/// none; else the number of branches and the ten summary values, then indented by four spaces
/// the warnings (describeFlightWarning()) and for each branch its name, its number of rows,
/// its optimum altitude, the time to it, the optimum delay, the separation time and the id of
/// its source component, followed by describeBranchLines().
[[nodiscard]] inline std::string describeFlightData(const FlightData* data, const Rocket& rocket,
                                                    const FlightDescription& how = {})
{
    if (data == nullptr)
    {
        return "  data: null\n";
    }
    const auto  number = [](double value) { return Strings::javaDoubleToString(value); };
    std::string text   = std::format(
        "  data: branches={} maxAlt={} maxVel={} maxAcc={} maxMach={} tApogee={} tFlight={} "
        "vGround={} vRod={} vDeploy={} optDelay={}\n",
        data->getBranchCount(), number(data->getMaxAltitude()), number(data->getMaxVelocity()),
        number(data->getMaxAcceleration()), number(data->getMaxMachNumber()),
        number(data->getTimeToApogee()), number(data->getFlightTime()),
        number(data->getGroundHitVelocity()), number(data->getLaunchRodVelocity()),
        number(data->getDeploymentVelocity()), number(data->getOptimumDelay()));
    for (const Warning& warning : data->getWarningSet())
    {
        text += "    " + describeFlightWarning(warning, rocket, how) + "\n";
    }
    for (std::size_t index = 0; index < data->getBranchCount(); index++)
    {
        const FlightDataBranch& branch = data->getBranch(index);
        text += ascii(std::format(
                    "    branch[{}] '{}' rows={} optAlt={} tOptAlt={} optDelay={} sepTime={} "
                    "srcId={}",
                    index, branch.getName(), branch.getLength(),
                    number(branch.getOptimumAltitude()), number(branch.getTimeToOptimumAltitude()),
                    number(branch.getOptimumDelay()), number(branch.getSeparationTime()),
                    branch.getSourceComponentId().has_value()
                        ? branch.getSourceComponentId().value_or(Uuid::nil()).toString()
                        : "null")) +
                "\n";
        text += describeBranchLines(branch, *data, rocket, how);
    }
    return text;
}

/// What a run of a handler gave before its result is looked at, as FdProbe prints it, every
/// line indented by two spaces and ended:
/// - "W[<class>,<priority>] <text>" for every warning of the load, in order;
/// - "FAILED <code> [<message>]" for a failure. (The probe prints "THROWN <exception>:
///   <message>"; for the two exceptions that fail a load with their message,
///   IllegalArgumentException and NumberFormatException, the expectations of the cases have it
///   as "FAILED INVALID_ARGUMENT [<message>]", which is the failure the loader's rules ask
///   for.)
/// - "closed=<element> {<attributes>} [<text, trimmed>]", what the parent's closeElement() was
///   given, or "closed=(not closed)" after a failure.
[[nodiscard]] inline std::string describeRun(const HandlerRun& run)
{
    std::string text;
    for (const Warning& warning : run.warnings)
    {
        text += ascii(std::format("  W[{},{}] {}", warning.typeName(),
                                  exportLabel(warning.priority()), warning.toString())) +
                "\n";
    }
    if (!run.result.has_value())
    {
        text += ascii(std::format("  FAILED {} [{}]", toString(run.result.error().code),
                                  run.result.error().message)) +
                "\n";
    }
    text += ascii(run.element.empty() ? "  closed=(not closed)"
                                      : std::format("  closed={} {} [{}]", run.element,
                                                    describeAttributes(run.attributes),
                                                    Strings::trim(run.content))) +
            "\n";
    return text;
}

/// A HandlerFixture whose document has a rocket that warnings and events can name: the rocket
/// "R" with a stage "S", in it a body tube "B" (a motor mount by its class) and in that a
/// parachute "P", with the ids kFlightRocketId, kFlightStageId, kFlightTubeId and
/// kFlightChuteId. apply() changes it as the directives of a case ask.
class FlightDataFixture : public HandlerFixture
{
public:
    FlightDataFixture()
    {
        rocket().setName("R");
        rocket().setId(uuidOf(kFlightRocketId));
        m_stage = &rocket().addChild(std::make_unique<AxialStage>());
        m_stage->setName("S");
        m_stage->setId(uuidOf(kFlightStageId));
        m_tube = &m_stage->addChild(std::make_unique<BodyTube>());
        m_tube->setName("B");
        m_tube->setId(uuidOf(kFlightTubeId));
        m_chute = &m_tube->addChild(std::make_unique<Parachute>());
        m_chute->setName("P");
        m_chute->setId(uuidOf(kFlightChuteId));
    }

    [[nodiscard]] AxialStage& stage() noexcept { return *m_stage; }
    [[nodiscard]] BodyTube&   tube() noexcept { return *m_tube; }
    [[nodiscard]] Parachute&  chute() noexcept { return *m_chute; }

    /// How the case asked for its outcome to be printed (the directives "@ids" and "@digest").
    [[nodiscard]] const FlightDescription& description() const noexcept { return m_description; }

    /// Applies the directives of a case, one per line of @p setup (FdProbe reads the same):
    /// - "@rocket <id> <name>", "@stage ...", "@tube ...", "@chute ...": another id and name
    ///   for that component;
    /// - "@expression <name>|<symbol>|<unit>|<expression>": a custom expression of the document;
    /// - "@unknown <name>": registers the flight data type <name> under the symbol "Unknown",
    ///   as a load that met that column name would have left it;
    /// - "@ids" and "@digest": see FlightDescription.
    /// @throws BugError for anything else
    void apply(std::string_view setup)
    {
        for (const std::string& line : Strings::split(setup, '\n'))
        {
            if (!line.empty())
            {
                applyDirective(line);
            }
        }
    }

private:
    void applyDirective(std::string_view line)
    {
        const std::size_t      space = line.find(' ');
        const std::string_view word  = line.substr(0, space);
        const std::string_view rest =
            space == std::string_view::npos ? std::string_view{} : line.substr(space + 1);
        if (word == "@rocket")
        {
            rename(rocket(), rest);
        }
        else if (word == "@stage")
        {
            rename(*m_stage, rest);
        }
        else if (word == "@tube")
        {
            rename(*m_tube, rest);
        }
        else if (word == "@chute")
        {
            rename(*m_chute, rest);
        }
        else if (word == "@expression")
        {
            const std::vector<std::string> fields = Strings::split(rest, '|');
            if (fields.size() != 4)
            {
                bug("an expression has four fields: " + std::string(line));
            }
            document().addCustomExpression(
                CustomExpression(fields.at(0), fields.at(1), fields.at(2), fields.at(3)));
        }
        else if (word == "@unknown")
        {
            [[maybe_unused]] const FlightDataType& registered =
                FlightDataType::getType(rest, "Unknown", UnitGroupId::NONE);
        }
        else if (word == "@ids")
        {
            m_description.allIds = true;
        }
        else if (word == "@digest")
        {
            m_description.digest = true;
        }
        else
        {
            bug("unknown directive: " + std::string(line));
        }
    }

    /// Gives @p component the id and the name of "<id> <name>".
    static void rename(RocketComponent& component, std::string_view idAndName)
    {
        const std::size_t space = idAndName.find(' ');
        if (space == std::string_view::npos)
        {
            bug("an id and a name: " + std::string(idAndName));
        }
        component.setId(uuidOf(idAndName.substr(0, space)));
        component.setName(idAndName.substr(space + 1));
    }

    AxialStage*       m_stage{nullptr};
    BodyTube*         m_tube{nullptr};
    Parachute*        m_chute{nullptr};
    FlightDescription m_description;
};

/// Whether @p xml is a <warning> element (and not a <flightdata> element).
[[nodiscard]] inline bool isWarningElement(std::string_view xml)
{
    return Strings::trim(xml).starts_with("<warning");
}

/// Runs @p xml through its handler in a new FlightDataFixture that @p setup was applied to, and
/// describes the outcome as FdProbe prints it: a line end (so that an expectation written as a
/// raw string starts on a line of its own), describeRun(), and
/// - for a <warning> element, run through a WarningHandler with a set of its own: "  set:
///   <number of warnings>" and the warnings of the set, each indented by four spaces
///   (describeFlightWarning());
/// - for any other element, run through a FlightDataHandler: describeFlightData() of its
///   flight data.
[[nodiscard]] inline std::string runFlightDataCase(std::string_view setup, std::string_view xml)
{
    FlightDataFixture fixture;
    fixture.apply(setup);
    std::string text = "\n";
    if (isWarningElement(xml))
    {
        WarningSet       set;
        WarningHandler   handler(fixture.rocket(), set);
        const HandlerRun run = runHandler(handler, xml);
        text += describeRun(run);
        text += std::format("  set: {}\n", set.size());
        for (const Warning& warning : set)
        {
            text += "    " +
                    describeFlightWarning(warning, fixture.rocket(), fixture.description()) + "\n";
        }
        return text;
    }
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run = runHandler(handler, xml);
    text += describeRun(run);
    text +=
        describeFlightData(handler.getFlightData().get(), fixture.rocket(), fixture.description());
    return text;
}

/// One <flightdata> or <warning> element that FdProbe ran through OpenRocket, with what
/// OpenRocket made of it and, where QtRocket's handlers do something else on purpose, what
/// they make of it. The tables of the handler tests are written by the probe's script
/// (gen_tables.py) from the probe's output; what QtRocket does differently is written by hand
/// from the decision that asks for it.
struct FlightDataCase
{
    /// The probe's name of the case ("fd: ..." and "fd2: ..." for the cases of the scout's
    /// EdgeProbe, "w: ..." and "s2: ..." for the ones added with the handlers, "rw: ..." and
    /// "rb: ..." for the ones added after the review of run 9b).
    std::string_view name;
    /// The directives of the case, one per line (FlightDataFixture::apply()).
    std::string_view setup;
    /// The element.
    std::string_view xml;
    /// What OpenRocket makes of it, as runFlightDataCase() prints an outcome.
    std::string_view java;
    /// What QtRocket makes of it, when that is not what OpenRocket makes of it; else empty.
    std::string_view qtrocket;
    /// Why QtRocket differs; empty when it does not.
    std::string_view why;
};

/// What GoogleTest prints for the case of a test that failed: its name (and not its bytes).
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
inline void PrintTo(const FlightDataCase& flightDataCase, std::ostream* out)
{
    *out << flightDataCase.name;
}

/// The name of the test of a case, for INSTANTIATE_TEST_SUITE_P (conditionsTestName()).
[[nodiscard]] inline std::string flightDataCaseTestName(
    const ::testing::TestParamInfo<FlightDataCase>& info)
{
    return conditionsTestName(info.param.name);
}

/// Expects that running the element of @p flightDataCase gives what the case says: OpenRocket's
/// outcome, or QtRocket's own where the case states one with its reason. A case that fails
/// because a text is no UUID has the message of java.util.UUID.fromString() for that text, as
/// the probe printed it: the handlers pass on what Uuid::javaFromString() gives.
inline void expectFlightDataCase(const FlightDataCase& flightDataCase)
{
    const std::string_view expected =
        flightDataCase.qtrocket.empty() ? flightDataCase.java : flightDataCase.qtrocket;
    EXPECT_EQ(runFlightDataCase(flightDataCase.setup, flightDataCase.xml), std::string(expected))
        << flightDataCase.name;
    // A deviation comes with its reason, and only a deviation has one.
    EXPECT_EQ(flightDataCase.qtrocket.empty(), flightDataCase.why.empty()) << flightDataCase.name;
    EXPECT_NE(flightDataCase.qtrocket, flightDataCase.java) << flightDataCase.name;
}

}  // namespace QtRocket::Test
