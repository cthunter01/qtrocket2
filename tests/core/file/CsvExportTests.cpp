// Tests of CsvExport on branches built by hand. OpenRocket has no test of its CSVExport; the
// texts expected here are what OpenRocket's CSVExport.exportCSV() writes for the same branches,
// simulations and options (a probe of tier 10a, CsvCasesProbe.java, on TestRockets'
// makeEstesAlphaIII() with TEST_FCID_0 selected, OpenRocket's test preferences and the default
// units). The examples' own flight data is compared with OpenRocket's files in
// csv_export_golden_tests.cpp.

#include "QtRocket/file/CsvExport.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <ios>
#include <limits>
#include <memory>
#include <numbers>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{
namespace
{

using Status = Simulation::Status;
using Type   = FlightEvent::Type;

// ===================================================================== OpenRocket's texts

/// The probe's main_defaults.csv
constexpr std::string_view kMainDefaults =
    "# Main simulation (Data loaded from a file)\n"
    "# 7 data points written for 5 variables.\n"
    "# Simulation warnings:\n"
    "#   Large angle of attack encountered (22.9\xC2\xB0)\n"
    "#   A warning of its own\n"
    "#\n"
    "# Time (s),Altitude (m),Angle of attack (\xC2\xB0),Mach number (\xE2\x80\x8B),Air temperature "
    "(\xC2\xB0"
    "C)\n"
    "# Event IGNITION occurred at t=0 seconds\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "0,0,0,0,15\n"
    "# Event LIFTOFF occurred at t=0.25 seconds\n"
    "0.25,0,5.73,0.3,0\n"
    "# Event LAUNCHROD occurred at t=0.3 seconds\n"
    "# Event TUMBLE occurred at t=0.5 seconds\n"
    "# Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (22.9\xC2\xB0)\n"
    "# Event BURNOUT occurred at t=0.5 seconds\n"
    "# Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "# Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "# Event ALTITUDE occurred at t=0.5 seconds\n"
    "0.5,1e-4,90,9.995e-4,26.85\n"
    "0.75,1e4,-14.324,1.235e4,-273.15\n"
    "# Event APOGEE occurred at t=1 seconds\n"
    "# Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "1,NaN,5.73e-6,10000,9.973e4\n"
    "1.25,Inf,171.887,0,NaN\n"
    "1.5,-Inf,NaN,-2.5,-22.65\n"
    "# Event EXCEPTION occurred at t=1.6 seconds\n"
    "# Event GROUND_HIT occurred at t=10 seconds\n"
    "# Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_semicolon_dp0.csv
constexpr std::string_view kMainSemicolonDp0 =
    "% Main simulation (Data loaded from a file)\n"
    "% 7 data points written for 5 variables.\n"
    "% Simulation warnings:\n"
    "%   Large angle of attack encountered (22.9\xC2\xB0)\n"
    "%   A warning of its own\n"
    "%\n"
    "% Time (s);Altitude (m);Angle of attack (\xC2\xB0);Mach number (\xE2\x80\x8B);Air temperature "
    "(\xC2\xB0"
    "C)\n"
    "% Event IGNITION occurred at t=0 seconds\n"
    "% Event LAUNCH occurred at t=0 seconds\n"
    "0;0;0;0;15\n"
    "% Event LIFTOFF occurred at t=0.25 seconds\n"
    "0;0;6;0;0\n"
    "% Event LAUNCHROD occurred at t=0.3 seconds\n"
    "% Event TUMBLE occurred at t=0.5 seconds\n"
    "% Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (22.9\xC2\xB0)\n"
    "% Event BURNOUT occurred at t=0.5 seconds\n"
    "% Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "% Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "% Event ALTITUDE occurred at t=0.5 seconds\n"
    "1;1e-4;90;1e-3;27\n"
    "1;1e4;-14;1e4;-273\n"
    "% Event APOGEE occurred at t=1 seconds\n"
    "% Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "1;NaN;6e-6;10000;1e5\n"
    "1;Inf;172;0;NaN\n"
    "2;-Inf;NaN;-3;-23\n"
    "% Event EXCEPTION occurred at t=1.6 seconds\n"
    "% Event GROUND_HIT occurred at t=10 seconds\n"
    "% Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_tab_dp15_fixed.csv
constexpr std::string_view kMainTabDp15Fixed =
    "# Main simulation (Data loaded from a file)\n"
    "# 7 data points written for 5 variables.\n"
    "# Simulation warnings:\n"
    "#   Large angle of attack encountered (22.9\xC2\xB0)\n"
    "#   A warning of its own\n"
    "#\n"
    "# Time (s)\tAltitude (m)\tAngle of attack (\xC2\xB0)\tMach number (\xE2\x80\x8B)\tAir "
    "temperature (\xC2\xB0"
    "C)\n"
    "# Event IGNITION occurred at t=0 seconds\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "0\t0\t0\t0\t15\n"
    "# Event LIFTOFF occurred at t=0.25 seconds\n"
    "0.25\t0\t5.729577951308232\t0.3\t0\n"
    "# Event LAUNCHROD occurred at t=0.3 seconds\n"
    "# Event TUMBLE occurred at t=0.5 seconds\n"
    "# Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (22.9\xC2\xB0)\n"
    "# Event BURNOUT occurred at t=0.5 seconds\n"
    "# Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "# Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "# Event ALTITUDE occurred at t=0.5 seconds\n"
    "0.5\t0.0001\t90\t0.0009995\t26.850000000000023\n"
    "0.75\t10000\t-14.32394487827058\t12345.678\t-273.15\n"
    "# Event APOGEE occurred at t=1 seconds\n"
    "# Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "1\tNaN\t0.000005729577951\t9999.9996\t99726.85\n"
    "1.25\tInf\t171.88733853924697\t0\tNaN\n"
    "1.5\t-Inf\tNaN\t-2.5\t-22.649999999999977\n"
    "# Event EXCEPTION occurred at t=1.6 seconds\n"
    "# Event GROUND_HIT occurred at t=10 seconds\n"
    "# Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_space_no_sim_comments.csv
constexpr std::string_view kMainSpaceNoSimComments =
    "// Time (s) Altitude (m) Angle of attack (\xC2\xB0) Mach number (\xE2\x80\x8B) Air "
    "temperature (\xC2\xB0"
    "C)\n"
    "// Event IGNITION occurred at t=0 seconds\n"
    "// Event LAUNCH occurred at t=0 seconds\n"
    "0 0 0 0 15\n"
    "// Event LIFTOFF occurred at t=0.25 seconds\n"
    "0.25 0 5.73 0.3 0\n"
    "// Event LAUNCHROD occurred at t=0.3 seconds\n"
    "// Event TUMBLE occurred at t=0.5 seconds\n"
    "// Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered "
    "(22.9\xC2\xB0)\n"
    "// Event BURNOUT occurred at t=0.5 seconds\n"
    "// Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "// Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "// Event ALTITUDE occurred at t=0.5 seconds\n"
    "0.5 1e-4 90 9.995e-4 26.85\n"
    "0.75 1e4 -14.324 1.235e4 -273.15\n"
    "// Event APOGEE occurred at t=1 seconds\n"
    "// Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "1 NaN 5.73e-6 10000 9.973e4\n"
    "1.25 Inf 171.887 0 NaN\n"
    "1.5 -Inf NaN -2.5 -22.65\n"
    "// Event EXCEPTION occurred at t=1.6 seconds\n"
    "// Event GROUND_HIT occurred at t=10 seconds\n"
    "// Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_custom_no_field_comments.csv
constexpr std::string_view kMainCustomNoFieldComments =
    "# Main simulation (Data loaded from a file)\n"
    "# 7 data points written for 5 variables.\n"
    "# Simulation warnings:\n"
    "#   Large angle of attack encountered (22.9\xC2\xB0)\n"
    "#   A warning of its own\n"
    "# Event IGNITION occurred at t=0 seconds\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "0||0||0||0||15\n"
    "# Event LIFTOFF occurred at t=0.25 seconds\n"
    "0.25||0||5.73||0.3||0\n"
    "# Event LAUNCHROD occurred at t=0.3 seconds\n"
    "# Event TUMBLE occurred at t=0.5 seconds\n"
    "# Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (22.9\xC2\xB0)\n"
    "# Event BURNOUT occurred at t=0.5 seconds\n"
    "# Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "# Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "# Event ALTITUDE occurred at t=0.5 seconds\n"
    "0.5||1e-4||90||1e-3||26.85\n"
    "0.75||1e4||-14.32||1.23e4||-273.15\n"
    "# Event APOGEE occurred at t=1 seconds\n"
    "# Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "1||NaN||5.73e-6||10000||9.97e4\n"
    "1.25||Inf||171.89||0||NaN\n"
    "1.5||-Inf||NaN||-2.5||-22.65\n"
    "# Event EXCEPTION occurred at t=1.6 seconds\n"
    "# Event GROUND_HIT occurred at t=10 seconds\n"
    "# Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_no_comments.csv
constexpr std::string_view kMainNoComments =
    "0,0,0,0,15\n"
    "0.25,0,5.73,0.3,0\n"
    "0.5,1e-4,90,9.995e-4,26.85\n"
    "0.75,1e4,-14.324,1.235e4,-273.15\n"
    "1,NaN,5.73e-6,10000,9.973e4\n"
    "1.25,Inf,171.887,0,NaN\n"
    "1.5,-Inf,NaN,-2.5,-22.65\n";

/// The probe's main_no_fields.csv
constexpr std::string_view kMainNoFields =
    "# Main simulation (Data loaded from a file)\n"
    "# 7 data points written for 0 variables.\n"
    "# Simulation warnings:\n"
    "#   Large angle of attack encountered (22.9\xC2\xB0)\n"
    "#   A warning of its own\n"
    "#\n"
    "# \n"
    "# Event IGNITION occurred at t=0 seconds\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "\n"
    "# Event LIFTOFF occurred at t=0.25 seconds\n"
    "\n"
    "# Event LAUNCHROD occurred at t=0.3 seconds\n"
    "# Event TUMBLE occurred at t=0.5 seconds\n"
    "# Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (22.9\xC2\xB0)\n"
    "# Event BURNOUT occurred at t=0.5 seconds\n"
    "# Event EJECTION_CHARGE occurred at t=0.5 seconds\n"
    "# Event RECOVERY_DEVICE_DEPLOYMENT occurred at t=0.5 seconds\n"
    "# Event ALTITUDE occurred at t=0.5 seconds\n"
    "\n"
    "\n"
    "# Event APOGEE occurred at t=1 seconds\n"
    "# Event STAGE_SEPARATION occurred at t=1 seconds\n"
    "\n"
    "\n"
    "\n"
    "# Event EXCEPTION occurred at t=1.6 seconds\n"
    "# Event GROUND_HIT occurred at t=10 seconds\n"
    "# Event SIMULATION_END occurred at t=10 seconds\n";

/// The probe's main_reordered.csv
constexpr std::string_view kMainReordered =
    "# Mach number (\xE2\x80\x8B),Time (s)\n"
    "0,0\n"
    "0.3,0.25\n"
    "9.995e-4,0.5\n"
    "1.235e4,0.75\n"
    "10000,1\n"
    "0,1.25\n"
    "-2.5,1.5\n";

/// The probe's main_other_units.csv
constexpr std::string_view kMainOtherUnits =
    "# Time (min),Altitude (ft),Angle of attack (rad),Mach number (\xE2\x80\x8B),Air temperature "
    "(\xC2\xB0"
    "F)\n"
    "0,0,0,0,59\n"
    "0.004,0,0.1,0.3,32\n"
    "0.008,3.281e-4,1.571,9.995e-4,80.33\n"
    "0.013,3.281e4,-0.25,1.235e4,-459.67\n"
    "0.017,NaN,1e-7,10000,1.795e5\n"
    "0.021,Inf,3,0,NaN\n"
    "0.025,-Inf,NaN,-2.5,-8.77\n";

/// The probe's status_UPTODATE.csv
constexpr std::string_view kStatusUpToDate =
    "# Status UPTODATE (Up to date)\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_LOADED.csv
constexpr std::string_view kStatusLoaded =
    "# Status LOADED (Data loaded from a file)\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_OUTDATED.csv
constexpr std::string_view kStatusOutdated =
    "# Status OUTDATED (Out of date)\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_EXTERNAL.csv
constexpr std::string_view kStatusExternal =
    "# Status EXTERNAL (Imported data)\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_NOT_SIMULATED.csv
constexpr std::string_view kStatusNotSimulated =
    "# Status NOT_SIMULATED (Not simulated yet)\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_CANT_RUN.csv
constexpr std::string_view kStatusCantRun =
    "# Status CANT_RUN\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's status_ABORTED.csv
constexpr std::string_view kStatusAborted =
    "# Status ABORTED\n"
    "# 1 data points written for 1 variables.\n"
    "#\n"
    "# Time (s)\n"
    "0\n";

/// The probe's no_data.csv
constexpr std::string_view kNoData =
    "# Never run (Not simulated yet)\n"
    "# 2 data points written for 2 variables.\n"
    "# No simulation data available.\n"
    "#\n"
    "# Time (s),Altitude (m)\n"
    "0,1.5\n"
    "# Event SIM_WARN occurred at t=0.5 seconds: Large angle of attack encountered (17.2\xC2\xB0)\n"
    "1,2.5\n";

/// The probe's no_time_column.csv
constexpr std::string_view kNoTimeColumn =
    "# Imported (Imported data)\n"
    "# 2 data points written for 2 variables.\n"
    "#\n"
    "# Altitude (m),Mach number (\xE2\x80\x8B)\n"
    "# Event IGNITION occurred at t=0 seconds\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "# Event APOGEE occurred at t=3 seconds\n"
    "10,0.1\n"
    "20,0.2\n";

/// The probe's no_rows.csv
constexpr std::string_view kNoRows =
    "# Empty (Up to date)\n"
    "# 0 data points written for 2 variables.\n"
    "#\n"
    "# Time (s),Altitude (m)\n"
    "# Event LAUNCH occurred at t=0 seconds\n"
    "# Event SIMULATION_END occurred at t=2 seconds\n";

// ============================================================================ the cases

[[nodiscard]] const FlightDataType& builtin(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

[[nodiscard]] const FlightDataType& timeType()
{
    return builtin(FlightDataTypeId::TYPE_TIME);
}

[[nodiscard]] const FlightDataType& altitudeType()
{
    return builtin(FlightDataTypeId::TYPE_ALTITUDE);
}

[[nodiscard]] const FlightDataType& aoaType()
{
    return builtin(FlightDataTypeId::TYPE_AOA);
}

[[nodiscard]] const FlightDataType& machType()
{
    return builtin(FlightDataTypeId::TYPE_MACH_NUMBER);
}

[[nodiscard]] const FlightDataType& temperatureType()
{
    return builtin(FlightDataTypeId::TYPE_AIR_TEMPERATURE);
}

/// The fields of the main branch, in the order of the probe's MAIN_TYPES.
[[nodiscard]] std::vector<const FlightDataType*> mainFields()
{
    return {&timeType(), &altitudeType(), &aoaType(), &machType(), &temperatureType()};
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// The probe's main branch: seven rows of time, altitude, angle of attack, Mach number and air
/// temperature (SI units), with zeros, a negative zero, small and large values, NaN and the
/// infinities.
[[nodiscard]] std::shared_ptr<FlightDataBranch> mainBranch()
{
    auto branch = std::make_shared<FlightDataBranch>(
        "Main", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                    timeType(), altitudeType(), aoaType(), machType(), temperatureType()});
    const std::vector<double> time{0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5};
    const std::vector<double> altitude{0.0, -0.0, 1.0e-4, 1.0e4, kNaN, kInf, -kInf};
    const std::vector<double> aoa{0.0, 0.1, std::numbers::pi / 2, -0.25, 1.0e-7, 3.0, kNaN};
    const std::vector<double> mach{0.0, 0.3, 0.0009995, 12345.678, 9999.9996, 1.0e-9, -2.5};
    const std::vector<double> temperature{288.15, 273.15, 300.0, 0.0, 1.0e5, kNaN, 250.5};
    for (std::size_t i = 0; i < time.size(); i++)
    {
        branch->addPoint();
        branch->setValue(timeType(), time[i]);
        branch->setValue(altitudeType(), altitude[i]);
        branch->setValue(aoaType(), aoa[i]);
        branch->setValue(machType(), mach[i]);
        branch->setValue(temperatureType(), temperature[i]);
    }
    return branch;
}

/// TestRockets' Estes Alpha III with the first test configuration selected, which has a motor,
/// so that a simulation keeps the status it is made with (one without motors is CANT_RUN).
struct CaseRocket
{
    TestEstesAlphaIII alpha;

    CaseRocket() { alpha.rocket->setSelectedConfiguration(testFcid(0)); }

    [[nodiscard]] Rocket& rocket() const { return *alpha.rocket; }
};

/// A simulation as a file describes it: with @p status, @p name and @p data (null: none).
[[nodiscard]] std::unique_ptr<Simulation> simulation(Rocket& rocket, Status status,
                                                     std::string                 name,
                                                     std::shared_ptr<FlightData> data)
{
    return std::make_unique<Simulation>(
        nullptr, rocket, status, std::move(name), SimulationOptions(),
        std::vector<std::shared_ptr<SimulationExtension>>{}, std::move(data));
}

/// An event whose source is known by its id only, as the .ork loader makes them.
[[nodiscard]] FlightEvent detached(Type type, double time, const RocketComponent& source)
{
    Result<FlightEvent> event = FlightEvent::createDetached(type, time, &source);
    EXPECT_TRUE(event.has_value());
    EXPECT_EQ(event->getSource(), nullptr);
    return std::move(*event);
}

/// The probe's main case: the main branch with its fifteen events (in the probe's order, some
/// with a pointer to their source and some detached), in flight data whose warning set holds a
/// large angle of attack of 0.2 rad, which a later one of 0.4 rad replaces in place, and a
/// warning of its own; a LOADED simulation named "Main simulation".
struct MainCase
{
    DefaultUnitsGuard                 units;
    CaseRocket                        caseRocket;
    std::shared_ptr<FlightDataBranch> branch = mainBranch();
    std::shared_ptr<FlightData>       data   = std::make_shared<FlightData>(
        std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch});
    std::unique_ptr<Simulation> sim;

    MainCase()
    {
        const TestEstesAlphaIII& alpha  = caseRocket.alpha;
        const Rocket&            rocket = caseRocket.rocket();
        const Warning::LargeAOA  first(0.2);
        data->getWarningSet().add(first);
        const Warning* stored = data->getWarningSet().find(first);
        EXPECT_NE(stored, nullptr);

        branch->addEvent(FlightEvent(Type::APOGEE, 1.0));
        branch->addEvent(FlightEvent(Type::IGNITION, 0.0, alpha.inner));
        branch->addEvent(detached(Type::LAUNCH, 0.0, rocket));
        branch->addEvent(FlightEvent(Type::LIFTOFF, 0.25));
        branch->addEvent(FlightEvent(Type::LAUNCHROD, 0.3));
        branch->addEvent(detached(Type::BURNOUT, 0.5, *alpha.inner));
        branch->addEvent(
            FlightEvent(Type::SIM_WARN, 0.5, nullptr, FlightEvent::warningData(*stored)));
        branch->addEvent(FlightEvent(Type::EJECTION_CHARGE, 0.5, alpha.stage));
        branch->addEvent(detached(Type::RECOVERY_DEVICE_DEPLOYMENT, 0.5, *alpha.chute));
        branch->addEvent(detached(Type::ALTITUDE, 0.5, rocket));
        branch->addEvent(FlightEvent(Type::TUMBLE, 0.5));
        branch->addEvent(FlightEvent(Type::STAGE_SEPARATION, 1.0, alpha.stage));
        branch->addEvent(FlightEvent(Type::EXCEPTION, 1.6, nullptr,
                                     FlightEvent::Data{std::string("a message")}));
        branch->addEvent(FlightEvent(Type::SIMULATION_END, 10.0));
        branch->addEvent(FlightEvent(Type::GROUND_HIT, 10.0));

        // Replaces the angle of the stored warning; the event still holds 0.2 rad.
        data->getWarningSet().add(Warning::LargeAOA(0.4));
        data->getWarningSet().add(Warning::fromString("A warning of its own"));
        sim = simulation(caseRocket.rocket(), Status::LOADED, "Main simulation", data);
    }

    /// The export of the main branch with @p fields in the default units and @p exportOptions.
    [[nodiscard]] std::string exported(const std::vector<const FlightDataType*>& fields,
                                       const CsvExport::Options& exportOptions) const
    {
        return exportedIn(fields, CsvExport::defaultUnits(fields), exportOptions);
    }

    /// The export of the main branch with @p fields in @p fieldUnits and @p exportOptions.
    [[nodiscard]] std::string exportedIn(const std::vector<const FlightDataType*>& fields,
                                         const std::vector<const Unit*>&           fieldUnits,
                                         const CsvExport::Options& exportOptions) const
    {
        std::ostringstream out;
        const Result<void> result =
            CsvExport::exportCsv(out, *sim, *branch, fields, fieldUnits, exportOptions);
        EXPECT_TRUE(result.has_value()) << result.error().toString();
        return out.str();
    }
};

/// The options the probe passes, in the order of Java's arguments.
[[nodiscard]] CsvExport::Options options(std::string_view separator, int decimalPlaces,
                                         bool exponentialNotation, std::string_view comment,
                                         bool simulationComments, bool fieldComments,
                                         bool eventComments)
{
    return CsvExport::Options{.fieldSeparator      = std::string(separator),
                              .decimalPlaces       = decimalPlaces,
                              .exponentialNotation = exponentialNotation,
                              .commentStarter      = std::string(comment),
                              .simulationComments  = simulationComments,
                              .fieldComments       = fieldComments,
                              .eventComments       = eventComments};
}

/// The export of @p branch of @p sim with every field of @p fields in its default unit.
[[nodiscard]] std::string exportBranch(Simulation& sim, const FlightDataBranch& branch,
                                       const std::vector<const FlightDataType*>& fields,
                                       const CsvExport::Options&                 exportOptions)
{
    std::ostringstream out;
    const Result<void> result = CsvExport::exportCsv(
        out, sim, branch, fields, CsvExport::defaultUnits(fields), exportOptions);
    EXPECT_TRUE(result.has_value()) << result.error().toString();
    return out.str();
}

/// The probe's status case: a branch of one row (time 0) in a simulation with @p status named
/// "Status <NAME>", exported with the defaults.
[[nodiscard]] std::string statusCase(Status status)
{
    const CaseRocket caseRocket;
    const auto       branch = std::make_shared<FlightDataBranch>(
        "Tiny", std::initializer_list<std::reference_wrapper<const FlightDataType>>{timeType()});
    branch->addPoint();
    branch->setValue(timeType(), 0.0);
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), status, std::format("Status {}", name(status)),
                   std::make_shared<FlightData>(
                       std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    return exportBranch(*sim, *branch, {&timeType()}, {});
}

/// The probe's numbers case: one altitude column of twenty values, without comments, with
/// @p decimalPlaces and @p exponentialNotation.
[[nodiscard]] std::string numbersCase(int decimalPlaces, bool exponentialNotation)
{
    const CaseRocket caseRocket;
    const auto       branch = std::make_shared<FlightDataBranch>(
        "Numbers",
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{altitudeType()});
    for (const double value :
         {0.0,         -0.0,      1.0e-4,   -1.0e-4, 1.0e4,
          9999.9996,   0.0009995, 123.4567, 2.5e-9,  1.0e-8,
          kNaN,        kInf,      -kInf,    1.0e300, std::numeric_limits<double>::denorm_min(),
          -987654.321, 0.5,       1.5,      2.0005,  1.0 / 3.0})
    {
        branch->addPoint();
        branch->setValue(altitudeType(), value);
    }
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), Status::LOADED, "Numbers",
                   std::make_shared<FlightData>(
                       std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    return exportBranch(*sim, *branch, {&altitudeType()},
                        options(",", decimalPlaces, exponentialNotation, "#", false, false, false));
}

/// @p lines, each followed by '\n'.
[[nodiscard]] std::string lines(std::initializer_list<std::string_view> values)
{
    std::string text;
    for (const std::string_view value : values)
    {
        text += value;
        text += '\n';
    }
    return text;
}

/// 1e300 written out: "1" and 300 zeros.
[[nodiscard]] std::string oneE300()
{
    return "1" + std::string(300, '0');
}

// ==================================================================== the dialog's defaults

TEST(CsvExportOptions, AreTheDefaultsOfOpenRocketsExportDialog)
{
    // CsvOptionPanel: separator ",", ExportDecimalPlaces TextUtil.DEFAULT_DECIMAL_PLACES,
    // ExportExponentialNotation true, comment character "#", every comment option true.
    const CsvExport::Options defaults;
    EXPECT_EQ(defaults.fieldSeparator, ",");
    EXPECT_EQ(defaults.decimalPlaces, 3);
    EXPECT_TRUE(defaults.exponentialNotation);
    EXPECT_EQ(defaults.commentStarter, "#");
    EXPECT_TRUE(defaults.simulationComments);
    EXPECT_TRUE(defaults.fieldComments);
    EXPECT_TRUE(defaults.eventComments);
}

// ============================================================================ the main case

// The probe's main_defaults: every part of the file. The warning of the SIM_WARN event is the
// one the set holds now (22.9 degrees), not the event's copy (11.5 degrees), as in Java, where
// the two are one object. The events at 0.5 s: those without a source first, then by stage, the
// Rocket (-1) last, each group by type; detached events (LAUNCH, BURNOUT, the deployment,
// ALTITUDE) take the stage of the component their id names.
TEST(CsvExport, WritesTheMainCaseAsOpenRocket)
{
    const MainCase main;
    EXPECT_EQ(main.exported(mainFields(), {}), kMainDefaults);
}

// The probe's main_* variants: separators, decimals, notation, comment starters and the three
// comment options.
TEST(CsvExport, WritesTheOptionsAsOpenRocket)
{
    const MainCase main;
    EXPECT_EQ(main.exported(mainFields(), options(";", 0, true, "%", true, true, true)),
              kMainSemicolonDp0);
    EXPECT_EQ(main.exported(mainFields(), options("\t", 15, false, "#", true, true, true)),
              kMainTabDp15Fixed);
    EXPECT_EQ(main.exported(mainFields(), options(" ", 3, true, "//", false, true, true)),
              kMainSpaceNoSimComments);
    EXPECT_EQ(main.exported(mainFields(), options("||", 2, true, "#", true, false, true)),
              kMainCustomNoFieldComments);
    EXPECT_EQ(main.exported(mainFields(), options(",", 3, true, "#", false, false, false)),
              kMainNoComments);
}

// The probe's main_no_fields, main_reordered and main_other_units: no field at all (empty field
// line and empty rows), the fields in an order of the caller's, and units other than the
// defaults (minutes, feet, radians, degrees Fahrenheit).
TEST(CsvExport, WritesTheFieldsAndUnitsTheCallerGives)
{
    const MainCase main;
    EXPECT_EQ(main.exported({}, {}), kMainNoFields);
    EXPECT_EQ(
        main.exported({&machType(), &timeType()}, options(",", 3, true, "#", false, true, false)),
        kMainReordered);
    const std::vector<const Unit*> otherUnits{
        timeType().getUnitGroup().getUnit("min"), altitudeType().getUnitGroup().getUnit("ft"),
        aoaType().getUnitGroup().getUnit("rad"), &machType().getUnitGroup().getDefaultUnit(),
        temperatureType().getUnitGroup().getUnit("\xC2\xB0"
                                                 "F")};
    EXPECT_EQ(
        main.exportedIn(mainFields(), otherUnits, options(",", 3, true, "#", false, true, false)),
        kMainOtherUnits);
}

// The probe's status_*: the suffix of each status after the simulation's name; none for
// CANT_RUN and ABORTED.
TEST(CsvExport, WritesTheSuffixOfEveryStatus)
{
    const DefaultUnitsGuard units;
    EXPECT_EQ(statusCase(Status::UPTODATE), kStatusUpToDate);
    EXPECT_EQ(statusCase(Status::LOADED), kStatusLoaded);
    EXPECT_EQ(statusCase(Status::OUTDATED), kStatusOutdated);
    EXPECT_EQ(statusCase(Status::EXTERNAL), kStatusExternal);
    EXPECT_EQ(statusCase(Status::NOT_SIMULATED), kStatusNotSimulated);
    EXPECT_EQ(statusCase(Status::CANT_RUN), kStatusCantRun);
    EXPECT_EQ(statusCase(Status::ABORTED), kStatusAborted);
}

// The probe's no_data: a simulation without simulated data says so and writes no warnings; the
// branch (which is not the simulation's) is written all the same, and its SIM_WARN event, whose
// warning no set holds, shows the event's own warning.
TEST(CsvExport, WritesABranchOfASimulationWithoutData)
{
    const DefaultUnitsGuard units;
    const CaseRocket        caseRocket;
    FlightDataBranch        orphan("Orphan", {timeType(), altitudeType()});
    orphan.addPoint();
    orphan.setValue(timeType(), 0.0);
    orphan.setValue(altitudeType(), 1.5);
    orphan.addPoint();
    orphan.setValue(timeType(), 1.0);
    orphan.setValue(altitudeType(), 2.5);
    orphan.addEvent(FlightEvent(Type::SIM_WARN, 0.5, nullptr,
                                FlightEvent::warningData(Warning::LargeAOA(0.3))));
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), Status::NOT_SIMULATED, "Never run", nullptr);
    EXPECT_EQ(exportBranch(*sim, orphan, {&timeType(), &altitudeType()}, {}), kNoData);
}

// The probe's no_time_column: without a TIME column every event comes before the first row.
TEST(CsvExport, WritesEveryEventFirstWithoutATimeColumn)
{
    const DefaultUnitsGuard units;
    const CaseRocket        caseRocket;
    const auto              branch = std::make_shared<FlightDataBranch>(
        "Timeless", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                        altitudeType(), machType()});
    branch->addPoint();
    branch->setValue(altitudeType(), 10.0);
    branch->setValue(machType(), 0.1);
    branch->addPoint();
    branch->setValue(altitudeType(), 20.0);
    branch->setValue(machType(), 0.2);
    branch->addEvent(FlightEvent(Type::APOGEE, 3.0));
    branch->addEvent(FlightEvent(Type::IGNITION, 0.0, caseRocket.alpha.inner));
    branch->addEvent(FlightEvent(Type::LAUNCH, 0.0, &caseRocket.rocket()));
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), Status::EXTERNAL, "Imported",
                   std::make_shared<FlightData>(
                       std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    EXPECT_EQ(exportBranch(*sim, *branch, {&altitudeType(), &machType()}, {}), kNoTimeColumn);
}

// The probe's no_rows: a branch without rows writes every event after the field line.
TEST(CsvExport, WritesEveryEventAfterTheFieldLineWithoutRows)
{
    const DefaultUnitsGuard units;
    const CaseRocket        caseRocket;
    const auto              branch = std::make_shared<FlightDataBranch>(
        "Empty", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                     timeType(), altitudeType()});
    branch->addEvent(FlightEvent(Type::SIMULATION_END, 2.0));
    branch->addEvent(FlightEvent(Type::LAUNCH, 0.0));
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), Status::UPTODATE, "Empty",
                   std::make_shared<FlightData>(
                       std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    EXPECT_EQ(exportBranch(*sim, *branch, {&timeType(), &altitudeType()}, {}), kNoRows);
}

// The probe's numbers_*: TextUtil.doubleToString() with 0, 3 and 15 decimals, with and without
// the exponential notation: zeros (also a value within MathUtil.EPSILON / 2 of zero and the
// smallest subnormal), the sign of a negative value whose digits round to zero, rounding half
// up, 1e300 written out in full.
TEST(CsvExport, WritesTheNumbersAsOpenRocket)
{
    const DefaultUnitsGuard units;
    EXPECT_EQ(numbersCase(0, true),
              lines({"0",   "0",   "1e-4", "-1e-4", "1e4", "10000", "1e-3", "123", "0", "1e-8",
                     "NaN", "Inf", "-Inf", "1e300", "0",   "-1e6",  "1",    "2",   "2", "0"}));
    EXPECT_EQ(numbersCase(0, false),
              lines({"0",   "0",   "0",    "-0",      "10000", "10000",   "0", "123", "0", "0",
                     "NaN", "Inf", "-Inf", oneE300(), "0",     "-987654", "1", "2",   "2", "0"}));
    EXPECT_EQ(numbersCase(3, true),
              lines({"0",       "0",        "1e-4", "-1e-4", "1e4",   "10000", "9.995e-4",
                     "123.457", "0",        "1e-8", "NaN",   "Inf",   "-Inf",  "1e300",
                     "0",       "-9.877e5", "0.5",  "1.5",   "2.001", "0.333"}));
    EXPECT_EQ(numbersCase(3, false),
              lines({"0",       "0",           "0",   "-0",  "10000", "10000", "0.001",
                     "123.457", "0",           "0",   "NaN", "Inf",   "-Inf",  oneE300(),
                     "0",       "-987654.321", "0.5", "1.5", "2.001", "0.333"}));
    EXPECT_EQ(numbersCase(15, true), lines({"0",    "0",         "1e-4",     "-1e-4",
                                            "1e4",  "9999.9996", "9.995e-4", "123.4567",
                                            "0",    "1e-8",      "NaN",      "Inf",
                                            "-Inf", "1e300",     "0",        "-9.87654321e5",
                                            "0.5",  "1.5",       "2.0005",   "0.333333333333333"}));
    EXPECT_EQ(numbersCase(15, false),
              lines({"0",           "0",         "0.0001",   "-0.0001", "10000",
                     "9999.9996",   "0.0009995", "123.4567", "0",       "0.00000001",
                     "NaN",         "Inf",       "-Inf",     oneE300(), "0",
                     "-987654.321", "0.5",       "1.5",      "2.0005",  "0.333333333333333"}));
}

// Beyond the dialog's 0 to 15 decimal places (CsvExport takes any number): OpenRocket's output with
// 16, 17 and 20 is that of 15 but for 1/3, which shows all its shortest digits (Java's Formatter
// pads those with zeros, which are trimmed). Pinned by the Java probe CsvDecimalsProbe (tier 10a
// csv-cli fixer). With -1 Java throws (an UnknownFormatConversionException at the first value
// that is not 0, NaN or infinite); QtRocket counts it as 0 (see CsvExport.h).
TEST(CsvExport, WritesMoreDecimalsThanTheDialogOffersAsOpenRocket)
{
    const DefaultUnitsGuard units;
    const std::string exponential = lines({"0",    "0",         "1e-4",     "-1e-4",
                                           "1e4",  "9999.9996", "9.995e-4", "123.4567",
                                           "0",    "1e-8",      "NaN",      "Inf",
                                           "-Inf", "1e300",     "0",        "-9.87654321e5",
                                           "0.5",  "1.5",       "2.0005",   "0.3333333333333333"});
    const std::string fixed =
        lines({"0",           "0",         "0.0001",   "-0.0001", "10000",
               "9999.9996",   "0.0009995", "123.4567", "0",       "0.00000001",
               "NaN",         "Inf",       "-Inf",     oneE300(), "0",
               "-987654.321", "0.5",       "1.5",      "2.0005",  "0.3333333333333333"});
    EXPECT_EQ(numbersCase(16, true), exponential);
    EXPECT_EQ(numbersCase(16, false), fixed);
    EXPECT_EQ(numbersCase(17, true), exponential);
    EXPECT_EQ(numbersCase(17, false), fixed);
    EXPECT_EQ(numbersCase(20, true), exponential);
    EXPECT_EQ(numbersCase(20, false), fixed);
    EXPECT_EQ(numbersCase(-1, true), numbersCase(0, true));
    EXPECT_EQ(numbersCase(-1, false), numbersCase(0, false));
}

// ======================================================================= QtRocket's own rules

/// A small case of QtRocket's own: a LOADED simulation "Small" of a branch of two rows of time
/// and altitude.
struct SmallCase
{
    DefaultUnitsGuard                 units;
    CaseRocket                        caseRocket;
    std::shared_ptr<FlightDataBranch> branch = std::make_shared<FlightDataBranch>(
        "Small", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                     timeType(), altitudeType()});
    std::unique_ptr<Simulation> sim;

    SmallCase()
    {
        branch->addPoint();
        branch->setValue(timeType(), 0.0);
        branch->setValue(altitudeType(), 1.0);
        branch->addPoint();
        branch->setValue(timeType(), 1.0);
        branch->setValue(altitudeType(), 2.0);
        sim = simulation(caseRocket.rocket(), Status::LOADED, "Small",
                         std::make_shared<FlightData>(
                             std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    }

    /// The fields the branch has.
    [[nodiscard]] static std::vector<const FlightDataType*> fields()
    {
        return {&timeType(), &altitudeType()};
    }

    /// The export into @p stream with @p fields in their default units and the defaults.
    [[nodiscard]] Result<void> exportTo(std::ostream&                             stream,
                                        const std::vector<const FlightDataType*>& exported) const
    {
        return CsvExport::exportCsv(stream, *sim, *branch, exported,
                                    CsvExport::defaultUnits(exported), {});
    }
};

/// What SmallCase writes with the defaults.
constexpr std::string_view kSmall =
    "# Small (Data loaded from a file)\n"
    "# 2 data points written for 2 variables.\n"
    "#\n"
    "# Time (s),Altitude (m)\n"
    "0,1\n"
    "1,2\n";

/// A stream buffer that takes @p limit characters and then fails, as a full disk does.
class LimitedBuffer : public std::streambuf
{
public:
    explicit LimitedBuffer(std::size_t limit) : m_limit(limit) { }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

protected:
    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
        {
            return traits_type::not_eof(ch);
        }
        if (m_text.size() >= m_limit)
        {
            return traits_type::eof();
        }
        m_text.push_back(traits_type::to_char_type(ch));
        return ch;
    }

    std::streamsize xsputn(const char_type* chars, std::streamsize count) override
    {
        const std::size_t taken =
            std::min(static_cast<std::size_t>(count), m_limit - m_text.size());
        m_text.append(chars, taken);
        return static_cast<std::streamsize>(taken);
    }

private:
    std::size_t m_limit;
    std::string m_text;
};

// Events whose source is known by an id the simulation's rocket does not hold (a component
// removed after the load, here one of another rocket) count as events without a source: at the
// same time they come among those without a source, by type. (Java would ask the removed
// component for its stage, which fails for most components.)
TEST(CsvExport, OrdersAnEventWhoseSourceIsNotInTheRocketAsOneWithoutASource)
{
    const DefaultUnitsGuard units;
    const CaseRocket        caseRocket;
    const CaseRocket        otherRocket;
    const auto              branch = std::make_shared<FlightDataBranch>(
        "Gone", std::initializer_list<std::reference_wrapper<const FlightDataType>>{timeType()});
    branch->addPoint();
    branch->setValue(timeType(), 0.0);
    branch->addEvent(detached(Type::EJECTION_CHARGE, 0.0, *caseRocket.alpha.stage));
    branch->addEvent(FlightEvent(Type::TUMBLE, 0.0));
    branch->addEvent(detached(Type::IGNITION, 0.0, *otherRocket.alpha.inner));
    const std::unique_ptr<Simulation> sim =
        simulation(caseRocket.rocket(), Status::LOADED, "Gone",
                   std::make_shared<FlightData>(
                       std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    EXPECT_EQ(
        exportBranch(*sim, *branch, {&timeType()}, options(",", 3, true, "#", false, false, true)),
        lines({"# Event IGNITION occurred at t=0 seconds", "# Event TUMBLE occurred at t=0 seconds",
               "# Event EJECTION_CHARGE occurred at t=0 seconds", "0"}));
}

// A field the branch has no column for fails before anything is written (Java: a
// NullPointerException after the comments), into a stream and into a file, which is then not
// created.
TEST(CsvExport, AFieldTheBranchLacksIsAnInvalidArgument)
{
    const SmallCase small;
    const TempDir   dir;

    std::ostringstream out;
    const Result<void> result = small.exportTo(out, {&timeType(), &machType()});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message,
              "The flight data branch 'Small' has no data of type 'Mach number'.");
    EXPECT_EQ(out.str(), "");

    const std::filesystem::path              file = dir.resolve("lacking.csv");
    const std::vector<const FlightDataType*> fields{&machType()};
    const Result<void> toFile = CsvExport::exportCsv(file, *small.sim, *small.branch, fields,
                                                     CsvExport::defaultUnits(fields), {});
    ASSERT_FALSE(toFile.has_value());
    EXPECT_EQ(toFile.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_FALSE(std::filesystem::exists(file));
}

// A stream that fails is an I/O error: one that has failed already, one that fails part of the
// way (it holds what it took), and one that throws on failure (std::ios::exceptions()), whose
// exception does not leave the export.
TEST(CsvExport, AStreamThatFailsIsAnIoError)
{
    const SmallCase small;

    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    const Result<void> result = small.exportTo(failed, SmallCase::fields());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::IO);
    EXPECT_EQ(result.error().message, "error while writing the CSV data");

    LimitedBuffer      partBuffer(kSmall.size() - 3);
    std::ostream       part(&partBuffer);
    const Result<void> partResult = small.exportTo(part, SmallCase::fields());
    ASSERT_FALSE(partResult.has_value());
    EXPECT_EQ(partResult.error().code, ErrorCode::IO);
    EXPECT_EQ(partBuffer.text(), kSmall.substr(0, kSmall.size() - 3));

    LimitedBuffer throwingBuffer(10);
    std::ostream  throwing(&throwingBuffer);
    throwing.exceptions(std::ios::badbit | std::ios::failbit);
    Result<void> throwingResult;
    EXPECT_NO_THROW(throwingResult = small.exportTo(throwing, SmallCase::fields()));
    ASSERT_FALSE(throwingResult.has_value());
    EXPECT_EQ(throwingResult.error().code, ErrorCode::IO);
}

/// The message of the BugError an export of @p small with @p fields and @p units throws
/// (bugText()), "<none>" when it throws none.
[[nodiscard]] std::string exportBugText(const SmallCase&                          small,
                                        const std::vector<const FlightDataType*>& fields,
                                        const std::vector<const Unit*>&           units)
{
    std::ostringstream out;
    std::string        text = bugText([&] {
        const Result<void> result =
            CsvExport::exportCsv(out, *small.sim, *small.branch, fields, units, {});
        EXPECT_FALSE(result.has_value()) << "no BugError, and the export succeeded";
    });
    EXPECT_EQ(out.str(), "") << "nothing is written";
    return text;
}

// Fields and units of different lengths, and a null field or unit, are programming errors (Java:
// IllegalArgumentException with the same message, and a NullPointerException).
TEST(CsvExport, FieldsAndUnitsThatDoNotMatchAreABug)
{
    const SmallCase                small;
    const std::vector<const Unit*> oneUnit{&Unit::noUnit()};
    EXPECT_EQ(exportBugText(small, SmallCase::fields(), oneUnit),
              "fields and units lengths must be equal (2 vs 1)");
    EXPECT_NE(exportBugText(small, {nullptr}, oneUnit), "<none>");
    EXPECT_NE(exportBugText(small, SmallCase::fields(), {nullptr, nullptr}), "<none>");
    const std::vector<const FlightDataType*> nullField{nullptr};
    EXPECT_THROW((void)CsvExport::defaultUnits(nullField), BugError);
}

// A file is written in binary mode, so that every line ends with '\n' on every platform (a
// deviation: OpenRocket's println() writes "\r\n" on Windows), and replaces what the file held.
TEST(CsvExport, WritesAFileWithNewlinesOnly)
{
    const SmallCase                          small;
    const TempDir                            dir;
    const std::filesystem::path              file   = dir.write("out.csv", std::string(500, 'x'));
    const std::vector<const FlightDataType*> fields = SmallCase::fields();
    const Result<void> result = CsvExport::exportCsv(file, *small.sim, *small.branch, fields,
                                                     CsvExport::defaultUnits(fields), {});
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    const Result<std::string> text = readTextFile(file);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, kSmall);
}

// A file that cannot be opened (its directory does not exist) is an I/O error.
TEST(CsvExport, AFileThatCannotBeOpenedIsAnIoError)
{
    const SmallCase                          small;
    const TempDir                            dir;
    const std::filesystem::path              file   = dir.resolve("missing") / "out.csv";
    const std::vector<const FlightDataType*> fields = SmallCase::fields();
    const Result<void> result = CsvExport::exportCsv(file, *small.sim, *small.branch, fields,
                                                     CsvExport::defaultUnits(fields), {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::IO);
    EXPECT_EQ(result.error().message,
              std::format("cannot open '{}' for writing", pathToUtf8(file)));
    EXPECT_FALSE(std::filesystem::exists(file));
}

// Every text is written as valid UTF-8 (Java's Strings hold no malformed UTF-8): a malformed
// sequence in the simulation's name, the field separator or the comment starter becomes U+FFFD.
TEST(CsvExport, WritesValidUtf8)
{
    const SmallCase small;
    small.sim->setName("Bad \xFF name");
    std::ostringstream                       out;
    const std::vector<const FlightDataType*> fields = SmallCase::fields();
    ASSERT_TRUE(CsvExport::exportCsv(out, *small.sim, *small.branch, fields,
                                     CsvExport::defaultUnits(fields),
                                     options(",\xC3", 3, true, "\xE2\x82", true, false, false)));
    EXPECT_EQ(out.str(), lines({"\xEF\xBF\xBD Bad \xEF\xBF\xBD name (Data loaded from a file)",
                                "\xEF\xBF\xBD 2 data points written for 2 variables.",
                                "0,\xEF\xBF\xBD"
                                "1",
                                "1,\xEF\xBF\xBD"
                                "2"}));
}

// The default units are those of the fields' unit groups when they are asked for; a later change
// of a default unit does not change a list already made.
TEST(CsvExport, DefaultUnitsAreThoseOfTheUnitGroups)
{
    const DefaultUnitsGuard                  units;
    const std::vector<const FlightDataType*> fields{&timeType(), &altitudeType(), &machType()};
    const std::vector<const Unit*>           before = CsvExport::defaultUnits(fields);
    ASSERT_EQ(before.size(), 3U);
    EXPECT_EQ(before[0], &timeType().getUnitGroup().getDefaultUnit());
    EXPECT_EQ(before[1]->getUnit(), "m");
    EXPECT_EQ(before[2]->getUnit(), "\xE2\x80\x8B");

    UnitGroup& distance = unitGroup(UnitGroupId::DISTANCE);
    ASSERT_EQ(&altitudeType().getUnitGroup(), &distance);
    ASSERT_TRUE(distance.setDefaultUnit("ft"));
    const std::vector<const Unit*> after = CsvExport::defaultUnits(fields);
    EXPECT_EQ(after[1]->getUnit(), "ft");
    EXPECT_EQ(before[1]->getUnit(), "m");
    EXPECT_TRUE(CsvExport::defaultUnits({}).empty());
}

}  // namespace
}  // namespace QtRocket::Test
