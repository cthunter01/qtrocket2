#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "document/TestDocuments.h"
#include "goldens/GoldenResaves.h"
#include "simulation/SimulationRunSupport.h"

// The document makers of TestRockets (document/TestDocuments.h) against OpenRocket's: QtRocket's
// save of each maker's document is OpenRocket's save of the same maker
// (tests/data/goldens/test-documents/<maker>.ork, written by OpenRocketSaver with the simulation
// data, in the set-up of OpenRocketSaverTest but for the translator, which is
// ServicesForTesting's English one: probe TestDocumentsProbe of run 10b), line for line, after
// three normalisations of both texts: the creator attribute (QtRocket writes its own), the
// random ids (each version-4 UUID becomes "#<n>", numbered in the order of first appearance, so
// that the references between ids are still compared) and the computation times of the flight
// data (the wall-clock time of each step). A line that differs only in numbers of the same value
// counts as a difference too: the sixteen files hold no number that JDK 17 writes longer than
// QtRocket does (decision U10). No decided deviation shows in these documents: v110's script is
// written as OpenRocket writes it (decision D1 acts on the load, which
// OpenRocketSaver.TestCreateLoadSave covers), and they hold no mass component (E1) and no text
// that E3 changes.
//
// And what the saves cannot show: the motors, the parts of the documents that are no file's
// (a motor of a configuration the rocket does not have) and the normalisation itself.

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::StorageOptions;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::TestDocumentMaker;

/// Whether @p c is a lowercase hexadecimal digit.
[[nodiscard]] bool isHexDigit(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/// Whether the 36 characters of @p text at @p at are a random (version 4, variant 1) UUID as
/// Java and QtRocket write one: "xxxxxxxx-xxxx-4xxx-[89ab]xxx-xxxxxxxxxxxx".
[[nodiscard]] bool isRandomUuidAt(std::string_view text, std::size_t at) noexcept
{
    constexpr std::size_t kLength = 36;
    if (text.size() - at < kLength)
    {
        return false;
    }
    for (std::size_t i = 0; i < kLength; ++i)
    {
        const char c       = text[at + i];
        const bool hyphen  = i == 8 || i == 13 || i == 18 || i == 23;
        const bool matches = hyphen ? c == '-' : isHexDigit(c);
        if (!matches)
        {
            return false;
        }
    }
    if (text.size() - at > kLength && isHexDigit(text[at + kLength]))
    {
        return false;
    }
    const char variant = text[at + 19];
    return text[at + 14] == '4' &&
           (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

/// @p text with each random UUID replaced by "#<n>", n counting the distinct ones from 1 in the
/// order of their first appearance.
[[nodiscard]] std::string withNumberedIds(std::string_view text)
{
    std::vector<std::string_view> seen;
    std::string                   result;
    result.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size())
    {
        const bool boundary = at == 0 || !isHexDigit(text[at - 1]);
        if (boundary && isRandomUuidAt(text, at))
        {
            const std::string_view uuid = text.substr(at, 36);
            std::size_t            n    = 0;
            while (n < seen.size() && seen[n] != uuid)
            {
                ++n;
            }
            if (n == seen.size())
            {
                seen.push_back(uuid);
            }
            result += std::format("#{}", n + 1);
            at += uuid.size();
            continue;
        }
        result += text[at];
        ++at;
    }
    return result;
}

/// The index of the column "computation_time" in the types="..." attribute of the <databranch>
/// line @p line, or npos when it has none.
[[nodiscard]] std::size_t computationTimeColumn(std::string_view line)
{
    constexpr std::string_view kTypes = "types=\"";
    const std::size_t          start  = line.find(kTypes);
    if (start == std::string_view::npos)
    {
        return std::string_view::npos;
    }
    const std::size_t      end   = line.find('"', start + kTypes.size());
    const std::string_view types = line.substr(start + kTypes.size(), end - start - kTypes.size());
    std::size_t            index = 0;
    std::size_t            from  = 0;
    while (true)
    {
        const std::size_t comma = types.find(',', from);
        if (types.substr(from, comma - from) == "computation_time")
        {
            return index;
        }
        if (comma == std::string_view::npos)
        {
            return std::string_view::npos;
        }
        from = comma + 1;
        ++index;
    }
}

/// The <datapoint> line @p line with its field @p column replaced by "T"; the line as it is when
/// it has no such field.
[[nodiscard]] std::string withoutField(std::string_view line, std::size_t column)
{
    constexpr std::string_view kOpen = "<datapoint>";
    std::size_t                start = line.find(kOpen) + kOpen.size();
    for (std::size_t i = 0; i < column; ++i)
    {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos)
        {
            return std::string(line);
        }
        start = comma + 1;
    }
    const std::size_t comma = line.find(',', start);
    const std::size_t close = line.find('<', start);
    const std::size_t end   = comma < close ? comma : close;
    std::string       result(line.substr(0, start));
    result += 'T';
    result += line.substr(end);
    return result;
}

/// @p text with the computation time of every data point of a flight data branch replaced by
/// "T" (the column "computation_time" of the branch, wherever its types put it).
[[nodiscard]] std::string withoutComputationTimes(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    std::size_t column = std::string_view::npos;
    for (const std::string_view line : QtRocket::Test::lineViewsOf(text))
    {
        if (line.contains("<databranch "))
        {
            column = computationTimeColumn(line);
        }
        else if (line.contains("</databranch>"))
        {
            column = std::string_view::npos;
        }
        if (column != std::string_view::npos && line.contains("<datapoint>"))
        {
            result += withoutField(line, column);
        }
        else
        {
            result += line;
        }
        result += '\n';
    }
    // A text without a final line feed stays without one (compareOrkTexts() reports that).
    if (!text.ends_with('\n') && !result.empty())
    {
        result.pop_back();
    }
    return result;
}

/// Both normalisations but the creator's, which compareOrkTexts() makes.
[[nodiscard]] std::string normalised(std::string_view text)
{
    return withoutComputationTimes(withNumberedIds(text));
}

/// QtRocket's save of @p document with the simulation data, as OpenRocketSaverTest asks, with
/// @p preferences; "ERROR <error>" when the save fails, and a failure for any save warning.
[[nodiscard]] std::string savedText(OpenRocketDocument&                  document,
                                    const QtRocket::InMemoryPreferences& preferences)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    QtRocket::WarningSet      warnings;
    QtRocket::ErrorSet        errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(errors.empty());
    return text.has_value() ? *text : "ERROR " + text.error().toString();
}

/// What differs between QtRocket's save of @p maker's document and OpenRocket's, after the
/// normalisations; empty when nothing does.
[[nodiscard]] std::string differencesOf(const TestDocumentMaker& maker)
{
    const Result<std::string> theirs =
        QtRocket::readTextFile(QtRocket::Test::testDataDir() / "goldens" / "test-documents" /
                               (std::string(maker.name) + ".ork"));
    if (!theirs.has_value())
    {
        return theirs.error().toString();
    }
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> document = maker.make(preferences.store);
    const std::string                         ours     = savedText(*document, preferences.store);
    const QtRocket::Test::OrkTextComparison   comparison =
        QtRocket::Test::compareOrkTexts(normalised(ours), normalised(*theirs));
    if (comparison.differing == 0 && comparison.byValue == 0)
    {
        return {};
    }
    return std::format("{} of {} lines differ ({} by value only):\n{}", comparison.differing,
                       comparison.lines, comparison.byValue, comparison.differences);
}

}  // namespace

// Every maker's document is saved as OpenRocket saves the document of the same maker.
TEST(TestDocumentsGolden, EveryDocumentIsSavedAsOpenRocketSavesIt)
{
    std::size_t compared = 0;
    for (const TestDocumentMaker& maker : QtRocket::Test::testDocumentMakers())
    {
        EXPECT_EQ(differencesOf(maker), "") << maker.name;
        ++compared;
    }
    EXPECT_EQ(compared, 16U);
}

// The normalisation numbers the random ids by their first appearance and keeps every other id:
// the default configuration's, one made from a text ("testParachute"), a random id inside a
// longer word and an id of another variant.
TEST(TestDocuments, TheNormalisationNumbersTheRandomIdsOnly)
{
    EXPECT_EQ(withoutComputationTimes("a\n\nb"), "a\n\nb");
    EXPECT_EQ(withNumberedIds("<a id=\"d010716e-ce0e-469d-ae46-190f3653ebbf\">"
                              "8c9aafcb-9a2f-4cea-ac5d-7c602303d388 "
                              "d010716e-ce0e-469d-ae46-190f3653ebbf</a>"),
              "<a id=\"#1\">#2 #1</a>");
    EXPECT_EQ(withNumberedIds("ffffffff-f4f2-f1f0-0000-00000000162c "
                              "00000000-0000-0000-ffff-ffff9bde0acf "
                              "a8c9aafcb-9a2f-4cea-ac5d-7c602303d388 "
                              "8c9aafcb-9a2f-4cea-ac5d-7c602303d388f "
                              "8c9aafcb-9a2f-4cea-cc5d-7c602303d388"),
              "ffffffff-f4f2-f1f0-0000-00000000162c 00000000-0000-0000-ffff-ffff9bde0acf "
              "a8c9aafcb-9a2f-4cea-ac5d-7c602303d388 8c9aafcb-9a2f-4cea-ac5d-7c602303d388f "
              "8c9aafcb-9a2f-4cea-cc5d-7c602303d388");
    EXPECT_EQ(
        withoutComputationTimes("<databranch name=\"S\" types=\"time,computation_time,altitude\">\n"
                                "<datapoint>0,0.0123,1</datapoint>\n"
                                "<datapoint>1,0.5,2</datapoint>\n"
                                "</databranch>\n"
                                "<datapoint>2,0.7,3</datapoint>\n"),
        "<databranch name=\"S\" types=\"time,computation_time,altitude\">\n"
        "<datapoint>0,T,1</datapoint>\n"
        "<datapoint>1,T,2</datapoint>\n"
        "</databranch>\n"
        "<datapoint>2,0.7,3</datapoint>\n");
}

// TestRockets.getTestMotor() and the database of OpenRocketSaverTest (MotorDbProvider), whose
// F12X is the test motor without its case info. The manufacturer "A" is one of AeroTech's codes
// (Manufacturer.getManufacturer("A") gives AeroTech in Java too).
TEST(TestDocuments, TheTestMotorAndTheDatabaseOfTheSaverTest)
{
    const std::shared_ptr<const QtRocket::ThrustCurveMotor> motor = QtRocket::Test::getTestMotor();
    EXPECT_EQ(motor->getManufacturer().getSimpleName(), "AeroTech");
    EXPECT_EQ(motor->getDesignation(), "F12X");
    EXPECT_EQ(motor->getDescription(), "Desc");
    EXPECT_EQ(motor->getCaseInfo(), "info");
    EXPECT_EQ(motor->getMotorType(), QtRocket::Motor::Type::UNKNOWN);
    EXPECT_TRUE(motor->getStandardDelays().empty());
    EXPECT_EQ(motor->getDiameter(), 0.024);
    EXPECT_EQ(motor->getLength(), 0.07);
    EXPECT_EQ(motor->getTimePoints(), (std::vector<double>{0, 1, 2}));
    EXPECT_EQ(motor->getThrustPoints(), (std::vector<double>{0, 1, 0}));
    EXPECT_EQ(motor->getCGPoints(),
              (std::vector<Coordinate>{Coordinate::kNul, Coordinate::kNul, Coordinate::kNul}));
    EXPECT_EQ(motor->getDigest(), "digestA");

    const std::unique_ptr<QtRocket::ThrustCurveMotorSetDatabase> database =
        QtRocket::Test::openRocketSaverTestMotors();
    EXPECT_EQ(database->getMotorSets().size(), 2U);
}

// What no save shows. makeTestRocket_v104_withMotor() gives the motor to the default
// configuration, which keeps no motor (FlightConfigurableParameterSet.set() of the default id
// does nothing). makeTestRocket_v106_withMotorMountIgnitionConfig() gives it to an id of no
// configuration of the rocket, which the mount keeps with the ignition delay set after it was
// stored. The preset of makeTestRocket_v105_withComponentPreset() has OpenRocket's digest.
TEST(TestDocuments, WhatTheMakersHoldBesideTheirSaves)
{
    const std::unique_ptr<OpenRocketDocument> withMotor =
        QtRocket::Test::makeTestRocketV104WithMotor();
    const auto& mount =
        dynamic_cast<const InnerTube&>(withMotor->getRocket().getStage(0)->getChild(0).getChild(0));
    EXPECT_TRUE(mount.isMotorMount());
    EXPECT_FALSE(mount.getMotorConfig(FlightConfigurationId::defaultValueId()).hasMotor());
    const JavaTestPreferences preferences;
    EXPECT_EQ(withMotor->getRocket().getSelectedConfiguration().getNameRaw(preferences.store),
              "F12X");

    const std::unique_ptr<OpenRocketDocument> ignition =
        QtRocket::Test::makeTestRocketV106WithMotorMountIgnitionConfig();
    const auto& ignitionMount =
        dynamic_cast<const InnerTube&>(ignition->getRocket().getStage(0)->getChild(0).getChild(0));
    const std::vector<FlightConfigurationId> ids =
        ignitionMount.getMotorConfigurationSet().getIds();
    ASSERT_EQ(ids.size(), 1U);
    EXPECT_FALSE(ignition->getRocket().containsFlightConfigurationId(ids.front()));
    const QtRocket::MotorConfiguration& config = ignitionMount.getMotorConfig(ids.front());
    ASSERT_TRUE(config.hasMotor());
    EXPECT_EQ(config.getMotor()->getDigest(), "digestA");
    EXPECT_EQ(config.getIgnitionDelay(), 2.0);

    const std::unique_ptr<OpenRocketDocument> preset =
        QtRocket::Test::makeTestRocketV105WithComponentPreset();
    const QtRocket::ComponentPreset* bodyTubePreset =
        preset->getRocket().getStage(0)->getChild(0).getPresetComponent();
    ASSERT_NE(bodyTubePreset, nullptr);
    EXPECT_EQ(bodyTubePreset->getDigest(), QtRocket::Test::kV105PresetDigest);
}

// The two simulations of makeTestRocket_for_estimateFileSize() ran and aborted at once: the
// rocket has no motor.
TEST(TestDocuments, TheSimulationsOfTheEstimateDocumentAborted)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> document =
        QtRocket::Test::makeTestRocketForEstimateFileSize(preferences.store);
    ASSERT_EQ(document->getSimulationCount(), 2U);
    const std::vector<SimulationAbort::Cause> noMotors{SimulationAbort::Cause::NO_MOTORS_DEFINED};
    Simulation&                               first = *document->getSimulation(0);
    EXPECT_EQ(first.getStatus(), Simulation::Status::ABORTED);
    EXPECT_EQ(QtRocket::Test::abortCauses(QtRocket::Test::simulatedData(first), 0), noMotors);
    Simulation& second = *document->getSimulation(1);
    EXPECT_EQ(second.getStatus(), Simulation::Status::ABORTED);
    EXPECT_EQ(QtRocket::Test::abortCauses(QtRocket::Test::simulatedData(second), 0), noMotors);
}
