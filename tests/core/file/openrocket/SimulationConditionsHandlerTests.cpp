#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"

#include <array>
#include <cstddef>
#include <exception>
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationOptionsSupport.h"

// The expectations are what OpenRocket's SimulationConditionsHandler makes of the same
// elements, measured with the Java probe CondProbe of run 9b, part S1 (see
// ConditionsTestSupport.h for the notation), but where a case states that QtRocket differs.
// The cases about <wind>, <atmosphere>, <gravity> and the lookup elements are in the tests of
// their handlers; this file has the plain elements of <conditions>, the table of numbers that
// are not finite, and what the notation does not show.

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::SimulationConditionsHandler;
using QtRocket::SimulationOptions;
using QtRocket::Test::ConditionsCase;
using QtRocket::Test::conditionsCaseTestName;
using QtRocket::Test::ConditionsFixture;
using QtRocket::Test::conditionsTestName;
using QtRocket::Test::describe;
using QtRocket::Test::describeConditions;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::isJavaValue;
using QtRocket::Test::kBaselineOptions;
using QtRocket::Test::replaceAll;
using QtRocket::Test::runConditions;
using QtRocket::Test::runHandler;
using QtRocket::Test::storeEverySimulationKey;

using Texts = std::vector<std::string>;

// ---- the cases of the probe -----------------------------------------------------------------

constexpr auto kConditionsCases = std::to_array<ConditionsCase>({
    // BEGIN GENERATED: conditions
    {.name     = "cond: NaN for every plain number",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchrodlength>NaN</launchrodlength>
  <launchrodangle>NaN</launchrodangle>
  <launchroddirection>NaN</launchroddirection>
  <windaverage>NaN</windaverage>
  <windturbulence>NaN</windturbulence>
  <winddirection>NaN</winddirection>
  <launchaltitude>NaN</launchaltitude>
  <launchlatitude>NaN</launchlatitude>
  <launchlongitude>NaN</launchlongitude>
  <timestep>NaN</timestep>
  <maxtime>NaN</maxtime>
  <recoveryspeedwarning>NaN</recoveryspeedwarning>
  <drogueLowspeedwarning>NaN</drogueLowspeedwarning>
  <recoverydroguemainhighspeedwarning>NaN</recoverydroguemainhighspeedwarning>
  <recoverydroguemainlowspeedwarning>NaN</recoverydroguemainlowspeedwarning>
  <randomseed>NaN</randomseed>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
  W[Other,NORMAL] Illegal launch longitude.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  W[Other,NORMAL] Illegal random seed defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: Infinity for every plain number",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchrodlength>Infinity</launchrodlength>
  <launchrodangle>Infinity</launchrodangle>
  <launchroddirection>90</launchroddirection>
  <windaverage>Infinity</windaverage>
  <winddirection>1</winddirection>
  <launchaltitude>Infinity</launchaltitude>
  <launchlatitude>Infinity</launchlatitude>
  <launchlongitude>Infinity</launchlongitude>
  <timestep>Infinity</timestep>
  <maxtime>Infinity</maxtime>
  <recoveryspeedwarning>Infinity</recoveryspeedwarning>
  <drogueLowspeedwarning>Infinity</drogueLowspeedwarning>
  <recoverydroguemainhighspeedwarning>Infinity</recoverydroguemainhighspeedwarning>
  <recoverydroguemainlowspeedwarning>Infinity</recoverydroguemainlowspeedwarning>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  rod=Infinity intoWind=false angle=1.0471975511965976 dir=1.5707963267948966
  wind=AVERAGE avg=(Infinity,1.0,NaN) multi=MSL(0.0,0.0,0.0,0.0)
  site=11018.064362274883,90.0,180.0 geo=FLAT
  stepper=RK4 dt=Infinity tmax=Infinity maxAngle=0.05235987755982988
  thresholds=Infinity/Infinity/Infinity/Infinity
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
  W[Other,NORMAL] Illegal launch longitude.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  rod=0.0 intoWind=false angle=0.0 dir=1.5707963267948966
  wind=AVERAGE avg=(0.0,1.0,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why = "decision U3: a number that is not finite is not applied; the four thresholds share "
            "one warning"},
    {.name     = "cond: -Infinity for every plain number",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchrodlength>-Infinity</launchrodlength>
  <launchrodangle>-Infinity</launchrodangle>
  <launchroddirection>90</launchroddirection>
  <windaverage>-Infinity</windaverage>
  <winddirection>1</winddirection>
  <launchaltitude>-Infinity</launchaltitude>
  <launchlatitude>-Infinity</launchlatitude>
  <launchlongitude>-Infinity</launchlongitude>
  <timestep>-Infinity</timestep>
  <maxtime>-Infinity</maxtime>
  <recoveryspeedwarning>-Infinity</recoveryspeedwarning>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  rod=-Infinity intoWind=false angle=-1.0471975511965976 dir=1.5707963267948966
  wind=AVERAGE avg=(Infinity,1.0,NaN) multi=MSL(0.0,0.0,0.0,0.0)
  site=-Infinity,-90.0,-180.0 geo=FLAT
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
  W[Other,NORMAL] Illegal launch longitude.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  rod=0.0 intoWind=false angle=0.0 dir=1.5707963267948966
  wind=AVERAGE avg=(0.0,1.0,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: launchroddirection Infinity",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchintowind>false</launchintowind>
  <launchroddirection>Infinity</launchroddirection>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  rod=0.0 intoWind=false angle=0.0 dir=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: winddirection Infinity",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <winddirection>Infinity</winddirection>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: windturbulence Infinity with zero wind, then with wind",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <windaverage>0</windaverage>
  <windturbulence>Infinity</windturbulence>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,1.5707963267948966,NaN) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: windturbulence Infinity with wind",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <windaverage>2</windaverage>
  <windturbulence>Infinity</windturbulence>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(2.0,1.5707963267948966,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name =
         "cond: OpenRocket Inf spelling, garbage, zero and negative steps, hex and suffix numbers",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchrodlength>Inf</launchrodlength>
  <launchrodangle>abc</launchrodangle>
  <launchroddirection> 45 </launchroddirection>
  <launchaltitude>0x1p4</launchaltitude>
  <launchlatitude>12.5d</launchlatitude>
  <launchlongitude>1e1f</launchlongitude>
  <timestep>0</timestep>
  <maxtime>-1</maxtime>
  <recoveryspeedwarning>0</recoveryspeedwarning>
  <launchintowind>TRUE</launchintowind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  rod=0.0 intoWind=true angle=0.0 dir=0.7853981633974483
  site=16.0,12.5,10.0 geo=FLAT
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: launchintowind garbage, big angle, out of range site",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchintowind> true </launchintowind>
  <launchrodangle>170</launchrodangle>
  <launchroddirection>-90</launchroddirection>
  <launchaltitude>200000</launchaltitude>
  <launchlatitude>95</launchlatitude>
  <launchlongitude>-200</launchlongitude>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  rod=0.0 intoWind=false angle=1.0471975511965976 dir=4.71238898038469
  site=11018.064362274883,90.0,-180.0 geo=FLAT
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: random seeds",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <randomseed> +42 </randomseed>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  seedFixed=true seed=42
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: random seed too large",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <randomseed>2147483648</randomseed>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal random seed defined, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: random seed negative",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <randomseed>-2147483648</randomseed>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  seedFixed=true seed=-2147483648
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: enums garbage",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <geodeticmethod>round</geodeticmethod>
  <simulationsteppermethod>euler</simulationsteppermethod>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown geodetic computation method 'round'
  W[Other,NORMAL] Unknown Simulation Stepper 'euler'
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: enums other values",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <geodeticmethod> wgs84 </geodeticmethod>
  <simulationsteppermethod>rk6</simulationsteppermethod>
  <windmodeltype>multilevel</windmodeltype>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
  site=0.0,0.0,0.0 geo=WGS84
  stepper=RK6 dt=0.0 tmax=1200.0 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: windmodeltype garbage",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <windmodeltype>Gusty</windmodeltype>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [No enum constant info.openrocket.core.models.wind.WindModelType for string value: Gusty]
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: windmodeltype with spaces",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <windmodeltype> Average </windmodeltype>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [No enum constant info.openrocket.core.models.wind.WindModelType for string value:  Average ]
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: unknown child element of conditions with child",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <bogus>12</bogus>
  <bogus2><inner>3</inner></bogus2>
  <launchrodlength>2.5</launchrodlength>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element inner, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  rod=2.5 intoWind=false angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: empty conditions",
     .xml      = R"xml(
<conditions/>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: every element as the saver writes it",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <launchrodlength>1.5</launchrodlength>
  <launchintowind>false</launchintowind>
  <launchrodangle>5.0</launchrodangle>
  <launchroddirection>90.0</launchroddirection>
  <windaverage>2.0</windaverage>
  <windturbulence>0.1</windturbulence>
  <winddirection>1.5707963267948966</winddirection>
  <wind model="average">
    <speed>2.0</speed>
    <direction>1.5707963267948966</direction>
    <standarddeviation>0.2</standarddeviation>
  </wind>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="0.0" speed="3.8" direction="3.0" standarddeviation="1.52"/>
  </wind>
  <windmodeltype>Average</windmodeltype>
  <launchaltitude>0.0</launchaltitude>
  <launchlatitude>32.0</launchlatitude>
  <launchlongitude>-106.0</launchlongitude>
  <geodeticmethod>spherical</geodeticmethod>
  <simulationsteppermethod>rk4</simulationsteppermethod>
  <randomseed>-77</randomseed>
  <atmosphere model="extendedisa">
    <basetemperature>293.15</basetemperature>
    <basepressure>100000.0</basepressure>
    <baserelativehumidity>0.25</baserelativehumidity>
  </atmosphere>
  <gravity model="constant">
    <value>9.5</value>
  </gravity>
  <timestep>0.02</timestep>
  <maxtime>600.0</maxtime>
  <recoveryspeedwarning>21.0</recoveryspeedwarning>
  <drogueLowspeedwarning>4.0</drogueLowspeedwarning>
  <recoverydroguemainhighspeedwarning>31.0</recoverydroguemainhighspeedwarning>
  <recoverydroguemainlowspeedwarning>16.0</recoverydroguemainlowspeedwarning>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
  site=0.0,32.0,-106.0 geo=SPHERICAL
  stepper=RK4 dt=0.02 tmax=600.0 maxAngle=0.05235987755982988
  seedFixed=true seed=-77
  atmosphere=isa:false T=293.15 p=100000.0 hum=0.25
  gravity=CONSTANT/9.5
  thresholds=21.0/4.0/31.0/16.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: configid that is no UUID",
     .xml      = R"xml(
<conditions><configid>garbage</configid></conditions>
)xml",
     .java     = R"out(
  fcid=00000000-0000-0000-ffff-fffff49c6835
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: configid padded",
     .xml      = R"xml(
<conditions><configid> 11111111-1111-1111-1111-111111111111 </configid></conditions>
)xml",
     .java     = R"out(
  fcid=00000000-0000-0000-0000-00003a273ac0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: configid with short groups",
     .xml      = R"xml(
<conditions><configid>1-2-3-4-5</configid></conditions>
)xml",
     .java     = R"out(
  fcid=00000001-0002-0003-0004-000000000005
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: configid is the literal error key",
     .xml      = R"xml(
<conditions><configid>ffffffff-f4f2-f1f0-0000-0000000009b9</configid></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: configid twice",
     .xml      = R"xml(
<conditions><configid>11111111-1111-1111-1111-111111111111</configid><configid>22222222-2222-2222-2222-222222222222</configid></conditions>
)xml",
     .java     = R"out(
  fcid=22222222-2222-2222-2222-222222222222
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind true",
     .xml      = R"xml(
<conditions><launchintowind>true</launchintowind></conditions>
)xml",
     .java     = R"out(
  rod=0.0 intoWind=true angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind mixed case",
     .xml      = R"xml(
<conditions><launchintowind>tRuE</launchintowind></conditions>
)xml",
     .java     = R"out(
  rod=0.0 intoWind=true angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind leading space",
     .xml      = R"xml(
<conditions><launchintowind> true</launchintowind></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind yes",
     .xml      = R"xml(
<conditions><launchintowind>yes</launchintowind></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind one",
     .xml      = R"xml(
<conditions><launchintowind>1</launchintowind></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launchintowind true then empty",
     .xml      = R"xml(
<conditions><launchintowind>true</launchintowind><launchintowind></launchintowind></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: rod angle in degrees and direction in degrees reduced",
     .xml      = R"xml(
<conditions>
  <launchrodangle>30</launchrodangle>
  <launchroddirection>450</launchroddirection>
</conditions>
)xml",
     .java     = R"out(
  rod=0.0 intoWind=false angle=0.5235987755982988 dir=1.5707963267948966
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: rod angle negative and beyond the clamp, direction negative",
     .xml      = R"xml(
<conditions>
  <launchrodangle>-75</launchrodangle>
  <launchroddirection>-45</launchroddirection>
</conditions>
)xml",
     .java     = R"out(
  rod=0.0 intoWind=false angle=-1.0471975511965976 dir=5.497787143782138
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: rod length zero and negative",
     .xml      = R"xml(
<conditions><launchrodlength>0</launchrodlength><launchrodlength>-2.5</launchrodlength></conditions>
)xml",
     .java     = R"out(
  rod=-2.5 intoWind=false angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: numbers with spaces, exponent, hexadecimal and suffix",
     .xml      = R"xml(
<conditions>
  <launchrodlength> 1e1 </launchrodlength>
  <launchrodangle>0x1p3</launchrodangle>
  <launchroddirection>1.5e1d</launchroddirection>
  <timestep>.025</timestep>
  <maxtime>5.</maxtime>
</conditions>
)xml",
     .java     = R"out(
  rod=10.0 intoWind=false angle=0.13962634015954636 dir=0.2617993877991494
  stepper=RK4 dt=0.025 tmax=5.0 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: numbers that are no numbers",
     .xml      = R"xml(
<conditions>
  <launchrodlength></launchrodlength>
  <launchrodangle>1,5</launchrodangle>
  <launchroddirection>1 5</launchroddirection>
  <windaverage>five</windaverage>
  <windturbulence>0.1%</windturbulence>
  <winddirection>north</winddirection>
  <launchaltitude>1e</launchaltitude>
  <launchlatitude>+</launchlatitude>
  <launchlongitude>--1</launchlongitude>
  <timestep>Inf</timestep>
  <maxtime>-Inf</maxtime>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
  W[Other,NORMAL] Illegal launch longitude.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind elements in the saver's order",
     .xml      = R"xml(
<conditions>
  <windaverage>5</windaverage>
  <windturbulence>0.1</windturbulence>
  <winddirection>1</winddirection>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(5.0,1.0,0.5) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind turbulence before the average",
     .xml      = R"xml(
<conditions>
  <windturbulence>0.1</windturbulence>
  <windaverage>5</windaverage>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(5.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind average negative",
     .xml      = R"xml(
<conditions>
  <winddirection>1</winddirection>
  <windaverage>-4</windaverage>
  <windturbulence>0.5</windturbulence>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(4.0,4.141592653589793,2.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind average changed keeps the intensity",
     .xml      = R"xml(
<conditions>
  <windaverage>4</windaverage>
  <windturbulence>0.25</windturbulence>
  <windaverage>8</windaverage>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(8.0,1.5707963267948966,2.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind direction beyond a turn and negative turbulence",
     .xml      = R"xml(
<conditions>
  <windaverage>4</windaverage>
  <winddirection>7</winddirection>
  <windturbulence>-0.25</windturbulence>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(4.0,0.7168146928204138,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind elements do not switch the wind model type",
     .xml      = R"xml(
<conditions>
  <windmodeltype>MultiLevel</windmodeltype>
  <windaverage>4</windaverage>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(4.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: windmodeltype spellings",
     .xml      = R"xml(
<conditions>
  <windmodeltype>MULTILEVEL</windmodeltype>
  <windmodeltype>average</windmodeltype>
  <windmodeltype>multiLEVEL</windmodeltype>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: windmodeltype with an underscore",
     .xml      = R"xml(
<conditions>
  <launchrodlength>2</launchrodlength>
  <windmodeltype>multi_level</windmodeltype>
  <launchrodlength>3</launchrodlength>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [No enum constant info.openrocket.core.models.wind.WindModelType for string value: multi_level]
  closed=(not closed)
  rod=2.0 intoWind=false angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: windmodeltype empty",
     .xml      = R"xml(
<conditions><windmodeltype></windmodeltype></conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [No enum constant info.openrocket.core.models.wind.WindModelType for string value: ]
  closed=(not closed)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launch site within range",
     .xml      = R"xml(
<conditions>
  <launchaltitude>1500.5</launchaltitude>
  <launchlatitude>-45.25</launchlatitude>
  <launchlongitude>179.5</launchlongitude>
</conditions>
)xml",
     .java     = R"out(
  site=1500.5,-45.25,179.5 geo=FLAT
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: launch altitude negative",
     .xml      = R"xml(
<conditions><launchaltitude>-400</launchaltitude></conditions>
)xml",
     .java     = R"out(
  site=-400.0,0.0,0.0 geo=FLAT
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: geodetic methods",
     .xml      = R"xml(
<conditions>
  <geodeticmethod>spherical</geodeticmethod>
  <geodeticmethod>WGS84</geodeticmethod>
  <geodeticmethod>Flat</geodeticmethod>
  <geodeticmethod>wgs_84</geodeticmethod>
  <geodeticmethod></geodeticmethod>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown geodetic computation method 'WGS84'
  W[Other,NORMAL] Unknown geodetic computation method 'Flat'
  W[Other,NORMAL] Unknown geodetic computation method 'wgs_84'
  W[Other,NORMAL] Unknown geodetic computation method ''
  site=0.0,0.0,0.0 geo=SPHERICAL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: geodetic method flat padded",
     .xml      = R"xml(
<conditions>
  <geodeticmethod>wgs84</geodeticmethod>
  <geodeticmethod>
    flat
  </geodeticmethod>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: stepper methods",
     .xml      = R"xml(
<conditions>
  <simulationsteppermethod> rk6 </simulationsteppermethod>
  <simulationsteppermethod>RK4</simulationsteppermethod>
  <simulationsteppermethod>rk5</simulationsteppermethod>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown Simulation Stepper 'RK4'
  W[Other,NORMAL] Unknown Simulation Stepper 'rk5'
  stepper=RK6 dt=0.0 tmax=1200.0 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: random seed forms",
     .xml      = R"xml(
<conditions>
  <randomseed>4 2</randomseed>
  <randomseed>0x10</randomseed>
  <randomseed>1.0</randomseed>
  <randomseed></randomseed>
  <randomseed>-</randomseed>
  <randomseed>2147483647</randomseed>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal random seed defined, ignoring.
  seedFixed=true seed=2147483647
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: random seed valid then invalid",
     .xml      = R"xml(
<conditions>
  <randomseed>7</randomseed>
  <randomseed>seven</randomseed>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal random seed defined, ignoring.
  seedFixed=true seed=7
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: random seed zero",
     .xml      = R"xml(
<conditions><randomseed>-0</randomseed></conditions>
)xml",
     .java     = R"out(
  seedFixed=true seed=0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: time step and maximum time small and large",
     .xml      = R"xml(
<conditions>
  <timestep>1e-300</timestep>
  <maxtime>1e300</maxtime>
</conditions>
)xml",
     .java     = R"out(
  stepper=RK4 dt=0.0 tmax=1.0E300 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: time step valid then invalid",
     .xml      = R"xml(
<conditions>
  <timestep>0.01</timestep>
  <timestep>-0.0</timestep>
  <maxtime>30</maxtime>
  <maxtime>none</maxtime>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal time step defined, ignoring.
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
  stepper=RK4 dt=0.01 tmax=30.0 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: finite numbers however large are applied",
     .xml      = R"xml(
<conditions>
  <launchrodlength>1e300</launchrodlength>
  <windaverage>1e300</windaverage>
  <launchaltitude>-1e300</launchaltitude>
  <timestep>1e300</timestep>
  <maxtime>1.7976931348623157e308</maxtime>
  <recoveryspeedwarning>1e300</recoveryspeedwarning>
</conditions>
)xml",
     .java     = R"out(
  rod=1.0E300 intoWind=false angle=0.0 dir=0.0
  wind=AVERAGE avg=(1.0E300,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
  site=-1.0E300,0.0,0.0 geo=FLAT
  stepper=RK4 dt=1.0E300 tmax=1.7976931348623157E308 maxAngle=0.05235987755982988
  thresholds=1.0E300/3.048/30.48/15.24
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: finite wind numbers whose product is not finite",
     .xml      = R"xml(
<conditions>
  <windaverage>1e200</windaverage>
  <windturbulence>1e200</windturbulence>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(1.0E200,1.5707963267948966,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  wind=AVERAGE avg=(1.0E200,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why =
         "decision L3: the standard deviation, the intensity times the speed, would be infinite"},
    {.name     = "s1: rod angle and direction beyond the range of a double in radians",
     .xml      = R"xml(
<conditions>
  <launchrodangle>1e308</launchrodangle>
  <launchroddirection>1e308</launchroddirection>
</conditions>
)xml",
     .java     = R"out(
  rod=0.0 intoWind=false angle=1.0471975511965976 dir=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
  rod=0.0 intoWind=false angle=1.0471975511965976 dir=0.0
)out",
     .why = "decision L3: the direction in radians is infinite, and OpenRocket stores the NaN that "
            "reducing it to one turn gives; the angle is clamped as in OpenRocket"},
    {.name     = "s1: rod direction just within the range of a double in radians",
     .xml      = R"xml(
<conditions>
  <launchroddirection>2.8e307</launchroddirection>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy wind average that takes an infinite deviation along",
     .xml      = R"xml(
<conditions>
  <windaverage>0.001</windaverage>
  <windturbulence>1e306</windturbulence>
  <windaverage>1e10</windaverage>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(1.0E10,1.5707963267948966,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
  wind=AVERAGE avg=(0.001,1.5707963267948966,1.0E303) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why      = "decision L3: the new speed times the turbulence intensity would be infinite"},
    {.name     = "s1: thresholds zero, negative and garbage are silent",
     .xml      = R"xml(
<conditions>
  <recoveryspeedwarning>25</recoveryspeedwarning>
  <recoveryspeedwarning>0</recoveryspeedwarning>
  <drogueLowspeedwarning>-1</drogueLowspeedwarning>
  <recoverydroguemainhighspeedwarning>fast</recoverydroguemainhighspeedwarning>
  <recoverydroguemainlowspeedwarning></recoverydroguemainlowspeedwarning>
</conditions>
)xml",
     .java     = R"out(
  thresholds=25.0/3.048/30.48/15.24
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: thresholds in lower case are other elements",
     .xml      = R"xml(
<conditions>
  <droguelowspeedwarning>9</droguelowspeedwarning>
  <RecoverySpeedWarning>9</RecoverySpeedWarning>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: thresholds negative infinity",
     .xml      = R"xml(
<conditions>
  <drogueLowspeedwarning>-Infinity</drogueLowspeedwarning>
  <recoverydroguemainhighspeedwarning>-Infinity</recoverydroguemainhighspeedwarning>
  <recoverydroguemainlowspeedwarning>-Infinity</recoverydroguemainlowspeedwarning>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: unknown elements with text and attributes are silent",
     .xml      = R"xml(
<conditions a="1">
  <bogus b="2">text</bogus>
  <launchrodlength unit="m">2.5</launchrodlength>
  <maxsteps>5</maxsteps>
</conditions>
)xml",
     .java     = R"out(
  closed=conditions {a=1} []
  rod=2.5 intoWind=false angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: text in the conditions element",
     .xml      = R"xml(
<conditions>stray<launchrodlength>2.5</launchrodlength>text</conditions>
)xml",
     .java     = R"out(
  closed=conditions {} [straytext]
  rod=2.5 intoWind=false angle=0.0 dir=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: known element with a child",
     .xml      = R"xml(
<conditions a="1">
  <launchrodlength>2<x y="3"/>5</launchrodlength>
  <maxtime>9</maxtime>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  closed=conditions {} [2]
  rod=5.0 intoWind=false angle=0.0 dir=0.0
  stepper=RK4 dt=0.0 tmax=9.0 maxAngle=0.05235987755982988
)out",
     .qtrocket = {},
     .why      = {}},
    // END GENERATED: conditions
});

class ConditionsElements : public ::testing::TestWithParam<ConditionsCase>
{ };

TEST_P(ConditionsElements, LoadAsInOpenRocketButWhereStated)
{
    expectConditionsCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, ConditionsElements, ::testing::ValuesIn(kConditionsCases),
                         conditionsCaseTestName);

// ---- numbers that are not finite ------------------------------------------------------------

/// One number of a <conditions> element, with what OpenRocket and QtRocket make of a NaN, of
/// positive infinity and of negative infinity in its place (the scout's non-finite table,
/// measured again with CondProbe; decision L3 for what QtRocket does).
struct NonFiniteRow
{
    /// The element or attribute.
    std::string_view name;
    /// The <conditions> element, with "{V}" where the number goes.
    std::string_view xml;
    /// What OpenRocket makes of "NaN", "Infinity" and "-Infinity", as describeOutcome() prints
    /// an outcome.
    std::array<std::string_view, 3> java;
    /// What QtRocket makes of each, where that is not what OpenRocket makes of it: the value is
    /// never applied, and the outcome is that of the element without the number plus the
    /// warning. Empty: the same as OpenRocket.
    std::array<std::string_view, 3> qtrocket;
};

constexpr std::array<std::string_view, 3> kNonFiniteValues{"NaN", "Infinity", "-Infinity"};

constexpr auto kNonFiniteRows = std::to_array<NonFiniteRow>({
    // BEGIN GENERATED: nonfinite
    {.name     = "launchrodlength",
     .xml      = R"xml(<conditions><launchrodlength>{V}</launchrodlength></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
)out",
                  R"out(
  rod=Infinity intoWind=false angle=0.0 dir=0.0
)out",
                  R"out(
  rod=-Infinity intoWind=false angle=0.0 dir=0.0
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
)out"}},
    {.name     = "launchrodangle",
     .xml      = R"xml(<conditions><launchrodangle>{V}</launchrodangle></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
)out",
                  R"out(
  rod=0.0 intoWind=false angle=1.0471975511965976 dir=0.0
)out",
                  R"out(
  rod=0.0 intoWind=false angle=-1.0471975511965976 dir=0.0
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch rod angle defined, ignoring.
)out"}},
    {.name     = "launchroddirection",
     .xml      = R"xml(<conditions><launchroddirection>{V}</launchroddirection></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
)out",
                  R"out(
  rod=0.0 intoWind=false angle=0.0 dir=NaN
)out",
                  R"out(
  rod=0.0 intoWind=false angle=0.0 dir=NaN
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
)out"}},
    {.name     = "windaverage",
     .xml      = R"xml(<conditions><windaverage>{V}</windaverage></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
)out",
                  R"out(
  wind=AVERAGE avg=(Infinity,1.5707963267948966,NaN) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(Infinity,4.71238898038469,NaN) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal average windspeed defined, ignoring.
)out"}},
    {.name = "windturbulence",
     .xml =
         R"xml(<conditions><windaverage>2</windaverage><windturbulence>{V}</windturbulence></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(2.0,1.5707963267948966,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  W[Other,NORMAL] Illegal wind turbulence intensity defined, ignoring.
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"}},
    {.name     = "winddirection",
     .xml      = R"xml(<conditions><winddirection>{V}</winddirection></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
)out",
                  R"out(
  wind=AVERAGE avg=(0.0,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(0.0,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal wind direction defined, ignoring.
)out"}},
    {.name     = "launchaltitude",
     .xml      = R"xml(<conditions><launchaltitude>{V}</launchaltitude></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
)out",
                  R"out(
  site=11018.064362274883,0.0,0.0 geo=FLAT
)out",
                  R"out(
  site=-Infinity,0.0,0.0 geo=FLAT
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch altitude defined, ignoring.
)out"}},
    {.name     = "launchlatitude",
     .xml      = R"xml(<conditions><launchlatitude>{V}</launchlatitude></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
)out",
                  R"out(
  site=0.0,90.0,0.0 geo=FLAT
)out",
                  R"out(
  site=0.0,-90.0,0.0 geo=FLAT
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch latitude defined, ignoring.
)out"}},
    {.name     = "launchlongitude",
     .xml      = R"xml(<conditions><launchlongitude>{V}</launchlongitude></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal launch longitude.
)out",
                  R"out(
  site=0.0,0.0,180.0 geo=FLAT
)out",
                  R"out(
  site=0.0,0.0,-180.0 geo=FLAT
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal launch longitude.
)out",
                  R"out(
  W[Other,NORMAL] Illegal launch longitude.
)out"}},
    {.name     = "timestep",
     .xml      = R"xml(<conditions><timestep>{V}</timestep></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal time step defined, ignoring.
)out",
                  R"out(
  stepper=RK4 dt=Infinity tmax=1200.0 maxAngle=0.05235987755982988
)out",
                  R"out(
  W[Other,NORMAL] Illegal time step defined, ignoring.
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal time step defined, ignoring.
)out",
                  ""}},
    {.name     = "maxtime",
     .xml      = R"xml(<conditions><maxtime>{V}</maxtime></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
)out",
                  R"out(
  stepper=RK4 dt=0.0 tmax=Infinity maxAngle=0.05235987755982988
)out",
                  R"out(
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal max simulation time defined, ignoring.
)out",
                  ""}},
    {.name = "recoveryspeedwarning",
     .xml  = R"xml(<conditions><recoveryspeedwarning>{V}</recoveryspeedwarning></conditions>)xml",
     .java = {R"out(
)out",
              R"out(
  thresholds=Infinity/3.048/30.48/15.24
)out",
              R"out(
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  ""}},
    {.name = "drogueLowspeedwarning",
     .xml  = R"xml(<conditions><drogueLowspeedwarning>{V}</drogueLowspeedwarning></conditions>)xml",
     .java = {R"out(
)out",
              R"out(
  thresholds=20.0/Infinity/30.48/15.24
)out",
              R"out(
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  ""}},
    {.name = "recoverydroguemainhighspeedwarning",
     .xml =
         R"xml(<conditions><recoverydroguemainhighspeedwarning>{V}</recoverydroguemainhighspeedwarning></conditions>)xml",
     .java     = {R"out(
)out",
                  R"out(
  thresholds=20.0/3.048/Infinity/15.24
)out",
                  R"out(
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  ""}},
    {.name = "recoverydroguemainlowspeedwarning",
     .xml =
         R"xml(<conditions><recoverydroguemainlowspeedwarning>{V}</recoverydroguemainlowspeedwarning></conditions>)xml",
     .java     = {R"out(
)out",
                  R"out(
  thresholds=20.0/3.048/30.48/Infinity
)out",
                  R"out(
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  ""}},
    {.name = "wind average speed",
     .xml  = R"xml(<conditions><wind model="average"><speed>{V}</speed></wind></conditions>)xml",
     .java = {R"out(
)out",
              R"out(
  wind=AVERAGE avg=(Infinity,1.5707963267948966,NaN) multi=MSL(0.0,0.0,0.0,0.0)
)out",
              R"out(
  wind=AVERAGE avg=(Infinity,4.71238898038469,NaN) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out"}},
    {.name = "wind average direction",
     .xml =
         R"xml(<conditions><wind model="average"><direction>{V}</direction></wind></conditions>)xml",
     .java     = {R"out(
)out",
                  R"out(
  wind=AVERAGE avg=(0.0,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(0.0,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
)out"}},
    {.name = "wind average standarddeviation",
     .xml =
         R"xml(<conditions><wind model="average"><speed>2</speed><standarddeviation>{V}</standarddeviation></wind></conditions>)xml",
     .java     = {R"out(
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(2.0,1.5707963267948966,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=AVERAGE avg=(2.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out"}},
    {.name = "windlevel altitude",
     .xml =
         R"xml(<conditions><wind model="multilevel"><windlevel altitude="{V}" speed="2" direction="1" standarddeviation="0.5"/></wind></conditions>)xml",
     .java     = {R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(NaN,2.0,1.0,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(Infinity,2.0,1.0,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(-Infinity,2.0,1.0,0.5)
)out"},
     .qtrocket = {R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out"}},
    {.name = "windlevel speed",
     .xml =
         R"xml(<conditions><wind model="multilevel"><windlevel altitude="10" speed="{V}" direction="1" standarddeviation="0.5"/></wind></conditions>)xml",
     .java     = {R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,NaN,1.0,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,Infinity,1.0,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,Infinity,4.141592653589793,0.5)
)out"},
     .qtrocket = {R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out"}},
    {.name = "windlevel direction",
     .xml =
         R"xml(<conditions><wind model="multilevel"><windlevel altitude="10" speed="2" direction="{V}" standarddeviation="0.5"/></wind></conditions>)xml",
     .java     = {R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,NaN,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,NaN,0.5)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,NaN,0.5)
)out"},
     .qtrocket = {R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out"}},
    {.name = "windlevel standarddeviation",
     .xml =
         R"xml(<conditions><wind model="multilevel"><windlevel altitude="10" speed="2" direction="1" standarddeviation="{V}"/></wind></conditions>)xml",
     .java     = {R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,1.0,NaN)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,1.0,Infinity)
)out",
                  R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,1.0,0.0)
)out"},
     .qtrocket = {R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
                  R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out"}},
    {.name = "basetemperature",
     .xml =
         R"xml(<conditions><atmosphere model="extendedisa"><basetemperature>{V}</basetemperature></atmosphere></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
)out",
                  R"out(
  atmosphere=isa:false T=Infinity p=101325.0 hum=0.0
)out",
                  R"out(
  atmosphere=isa:false T=-Infinity p=101325.0 hum=0.0
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
)out"}},
    {.name = "basepressure",
     .xml =
         R"xml(<conditions><atmosphere model="extendedisa"><basepressure>{V}</basepressure></atmosphere></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
)out",
                  R"out(
  atmosphere=isa:false T=288.15 p=Infinity hum=0.0
)out",
                  R"out(
  atmosphere=isa:false T=288.15 p=0.001 hum=0.0
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
)out",
                  R"out(
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
)out"}},
    {.name = "baserelativehumidity",
     .xml =
         R"xml(<conditions><atmosphere model="extendedisa"><baserelativehumidity>{V}</baserelativehumidity></atmosphere></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
)out",
                  R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
)out",
                  R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
)out"},
     .qtrocket = {"", "", ""}},
    {.name = "gravity value",
     .xml =
         R"xml(<conditions><gravity model="constant"><value>{V}</value></gravity></conditions>)xml",
     .java     = {R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  gravity=CONSTANT/9.807
)out",
                  R"out(
  gravity=CONSTANT/Infinity
)out",
                  R"out(
  gravity=CONSTANT/-Infinity
)out"},
     .qtrocket = {"",
                  R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  gravity=CONSTANT/9.807
)out",
                  R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  gravity=CONSTANT/9.807
)out"}},
    // END GENERATED: nonfinite
});

class NonFiniteConditions : public ::testing::TestWithParam<std::tuple<NonFiniteRow, std::size_t>>
{ };

/// What QtRocket makes of value @p value of @p row: its own outcome where the row has one, else
/// OpenRocket's.
[[nodiscard]] std::string expectedOutcome(const NonFiniteRow& row, std::size_t value)
{
    return std::string(row.qtrocket.at(value).empty() ? row.java.at(value)
                                                      : row.qtrocket.at(value));
}

TEST_P(NonFiniteConditions, AreNotApplied)
{
    const NonFiniteRow& row   = std::get<0>(GetParam());
    const std::size_t   value = std::get<1>(GetParam());
    EXPECT_EQ(runConditions(replaceAll(std::string(row.xml), "{V}", kNonFiniteValues.at(value))),
              expectedOutcome(row, value));
}

/// The test name of a row and a value: "LaunchrodlengthNaN", "WindlevelSpeedMinusInfinity".
[[nodiscard]] std::string nonFiniteTestName(
    const ::testing::TestParamInfo<std::tuple<NonFiniteRow, std::size_t>>& info)
{
    return conditionsTestName(std::format("{} {}", std::get<0>(info.param).name,
                                          kNonFiniteValues.at(std::get<1>(info.param))));
}

INSTANTIATE_TEST_SUITE_P(Table, NonFiniteConditions,
                         ::testing::Combine(::testing::ValuesIn(kNonFiniteRows),
                                            ::testing::Values(std::size_t{0}, std::size_t{1},
                                                              std::size_t{2})),
                         nonFiniteTestName);

/// The rows of the table in which an outcome expected of QtRocket shows a number that is not
/// finite.
[[nodiscard]] Texts rowsWithANonFiniteOutcome()
{
    Texts rows;
    for (const NonFiniteRow& row : kNonFiniteRows)
    {
        for (std::size_t value = 0; value < kNonFiniteValues.size(); value++)
        {
            const std::string expected = expectedOutcome(row, value);
            if (expected.contains("Infinity") || expected.contains("NaN"))
            {
                rows.emplace_back(row.name);
            }
        }
    }
    return rows;
}

// What the table says in one place: QtRocket takes no number that is not finite. In every row
// the outcome of a refused number is the outcome of the element without it, with a warning where
// OpenRocket would have stored something; so no outcome expected of QtRocket shows an infinity
// or a NaN.
TEST(NonFiniteConditionsTable, NoOutcomeOfQtRocketHoldsANumberThatIsNotFinite)
{
    EXPECT_EQ(rowsWithANonFiniteOutcome(), Texts{});
    // The table has every number of the conditions: 26 elements and attributes.
    EXPECT_EQ(kNonFiniteRows.size(), 26U);
}

// ---- where the options start ----------------------------------------------------------------

// CondProbe, "s1: empty conditions": the options under OpenRocket's test preferences.
TEST(SimulationConditionsHandler, StartsFromThePreferencesOfTheContext)
{
    ConditionsFixture                 fixture;
    const SimulationConditionsHandler handler(fixture.context());

    const Texts baseline(kBaselineOptions.begin(), kBaselineOptions.end());
    EXPECT_EQ(describeConditions(handler.getConditions()), baseline);
    EXPECT_EQ(handler.getIdToSet(), FlightConfigurationId::errorId());
}

// The options are SimulationOptions(preferences) with one change: the geodetic computation.
TEST(SimulationConditionsHandler, StartsWithTheFlatGeodeticComputationWhateverTheStoreSays)
{
    HandlerFixture fixture;
    storeEverySimulationKey(fixture.preferences());
    const SimulationConditionsHandler handler(fixture.context());

    SimulationOptions expected(fixture.preferences());
    EXPECT_EQ(expected.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    expected.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(describe(handler.getConditions()), describe(expected));
    EXPECT_EQ(handler.getConditions().getLaunchRodLength(), 2.5) << "a value of the store";
}

// Java asks the application's preferences, which are always there.
TEST(SimulationConditionsHandler, NeedsThePreferenceStoreOfTheContext)
{
    const DocumentLoadingContext context;
    ASSERT_EQ(context.getPreferences(), nullptr);
    EXPECT_THROW(SimulationConditionsHandler{context}, BugError);
}

// ---- what the notation of the cases does not show --------------------------------------------

/// The stepper method the preference store of a fixture holds after @p xml was loaded.
[[nodiscard]] std::string storedStepperAfter(std::string_view storedBefore, std::string_view xml)
{
    ConditionsFixture fixture;
    fixture.preferences().setSimulationStepperMethodName(storedBefore);
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run = runHandler(handler, xml);
    EXPECT_TRUE(run.result.has_value());
    return fixture.preferences().getSimulationStepperMethodName();
}

// Decision L8: SimulationOptions.setSimulationStepperMethodChoice() stores the choice in the
// application preferences (ApplicationPreferences.setSimulationStepperMethodChoice()), so
// OpenRocket's loader changes the stepper of new simulations when it opens a file. (The store of
// OpenRocket's tests drops what is put into it, so the probe cannot show this.)
TEST(SimulationConditionsHandler, AStepperMethodOfTheFileIsWrittenToThePreferenceStore)
{
    EXPECT_EQ(
        storedStepperAfter("RK4",
                           "<conditions><simulationsteppermethod>rk6</simulationsteppermethod>"
                           "</conditions>"),
        "RK6");
    EXPECT_EQ(
        storedStepperAfter("RK6",
                           "<conditions><simulationsteppermethod>rk4</simulationsteppermethod>"
                           "</conditions>"),
        "RK4");
    // An unknown method and a file without the element write nothing.
    EXPECT_EQ(
        storedStepperAfter("RK6",
                           "<conditions><simulationsteppermethod>RK4</simulationsteppermethod>"
                           "</conditions>"),
        "RK6");
    EXPECT_EQ(storedStepperAfter("RK6", "<conditions><timestep>0.01</timestep></conditions>"),
              "RK6");
}

/// The id a <conditions> element with an empty <configid> gives.
[[nodiscard]] FlightConfigurationId idOfEmptyConfigId()
{
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun run = runHandler(handler, "<conditions><configid></configid></conditions>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    return handler.getIdToSet();
}

// Decision L8: an empty <configid> is a new random id, as in Java (simplerocket.ork of the
// legacy files has four of them).
TEST(SimulationConditionsHandler, AnEmptyConfigIdIsANewRandomId)
{
    const FlightConfigurationId first  = idOfEmptyConfigId();
    const FlightConfigurationId second = idOfEmptyConfigId();
    EXPECT_TRUE(first.isValid());
    EXPECT_FALSE(first.isDefaultId());
    EXPECT_NE(first, second);
}

// The ISA atmosphere follows the launch altitude, whichever of the two the file names first
// (EdgeProbe's baseline document: T=287.5000511226052 p=100152.25761373011 at 100 m). The
// values come out of the atmospheric model's exponentials, so they are compared within the
// tolerance of a pinned value.
class IsaAtmosphereAndAltitude : public ::testing::TestWithParam<std::string_view>
{ };

TEST_P(IsaAtmosphereAndAltitude, TheAtmosphereFollowsTheLaunchAltitude)
{
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run = runHandler(handler, GetParam());
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    const SimulationOptions& options = handler.getConditions();
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchAltitude(), 100.0);
    EXPECT_TRUE(isJavaValue(287.5000511226052, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(100152.25761373011, options.getLaunchPressure()));
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.0);
}

INSTANTIATE_TEST_SUITE_P(
    EitherOrder, IsaAtmosphereAndAltitude,
    ::testing::Values(std::string_view("<conditions><atmosphere model='isa'/>"
                                       "<launchaltitude>100.0</launchaltitude></conditions>"),
                      std::string_view("<conditions><launchaltitude>100.0</launchaltitude>"
                                       "<atmosphere model='isa'/></conditions>")));

// The simulation's handler moves the options into the Simulation it makes; they arrive whole.
TEST(SimulationConditionsHandler, TheOptionsCanBeMovedOutOfTheHandler)
{
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run =
        runHandler(handler,
                   "<conditions><launchrodlength>2.5</launchrodlength><wind model='multilevel'>"
                   "<windlevel altitude='10' speed='2' direction='1' standarddeviation='0.5'/>"
                   "</wind><randomseed>7</randomseed></conditions>");
    ASSERT_TRUE(run.result.has_value());
    const std::string before = describe(handler.getConditions());

    SimulationOptions taken = std::move(handler.getConditions());

    EXPECT_EQ(describe(taken), before);
    EXPECT_EQ(taken.getRandomSeed(), 7);
    EXPECT_TRUE(taken.isRandomSeedFixed());
    // They are live options: a change of a wind model is still announced.
    const QtRocket::Test::ChangeCounter changes(taken.changed());
    taken.getMultiLevelWindModel().setAltitudeReference(
        QtRocket::WindModel::AltitudeReference::AGL);
    EXPECT_EQ(changes.count(), 1);
}

// A handler reads one <conditions> element; the next element gets a new handler and so starts
// from the preferences again (Java's SingleSimulationHandler makes one per element).
TEST(SimulationConditionsHandler, EveryHandlerStartsAfresh)
{
    ConditionsFixture fixture;
    {
        SimulationConditionsHandler first(fixture.context());
        EXPECT_TRUE(
            runHandler(first, "<conditions><configid>a</configid><maxtime>9</maxtime></conditions>")
                .result.has_value());
        EXPECT_EQ(first.getConditions().getMaxSimulationTime(), 9.0);
    }
    const SimulationConditionsHandler second(fixture.context());
    EXPECT_EQ(second.getConditions().getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(second.getIdToSet(), FlightConfigurationId::errorId());
}

// ---- whatever a file holds --------------------------------------------------------------------

/// The plain-text children of <conditions>, and one the handler does not know.
constexpr auto kTextElements = std::to_array<std::string_view>({
    "configid",
    "launchrodlength",
    "launchintowind",
    "launchrodangle",
    "launchroddirection",
    "windaverage",
    "windturbulence",
    "winddirection",
    "windmodeltype",
    "launchaltitude",
    "launchlatitude",
    "launchlongitude",
    "geodeticmethod",
    "simulationsteppermethod",
    "randomseed",
    "timestep",
    "maxtime",
    "recoveryspeedwarning",
    "drogueLowspeedwarning",
    "recoverydroguemainhighspeedwarning",
    "recoverydroguemainlowspeedwarning",
    "draglookupcsv",
    "stabilitylookupcsv",
    "bogus",
});

/// The other places of a <conditions> element that hold a value of the file, with "{V}" where
/// the value goes.
constexpr auto kValuePlaces = std::to_array<std::string_view>({
    R"(<wind model="{V}"><speed>3</speed><windlevel altitude="1" speed="2" direction="3" standarddeviation="4"/></wind>)",
    R"(<wind model="average"><speed>{V}</speed><direction>{V}</direction><standarddeviation>{V}</standarddeviation></wind>)",
    R"(<wind model="multilevel" altituderef="{V}"/>)",
    R"(<wind model="multilevel"><windlevel altitude="{V}" speed="2" direction="3" standarddeviation="4"/></wind>)",
    R"(<wind model="multilevel"><windlevel altitude="1" speed="{V}" direction="3" standarddeviation="4"/></wind>)",
    R"(<wind model="multilevel"><windlevel altitude="1" speed="2" direction="{V}" standarddeviation="4"/></wind>)",
    R"(<wind model="multilevel"><windlevel altitude="1" speed="2" direction="3" standarddeviation="{V}"/></wind>)",
    R"(<atmosphere model="{V}"><basetemperature>300</basetemperature></atmosphere>)",
    R"(<atmosphere model="extendedisa"><basetemperature>{V}</basetemperature></atmosphere>)",
    R"(<atmosphere model="extendedisa"><basepressure>{V}</basepressure></atmosphere>)",
    R"(<atmosphere model="extendedisa"><baserelativehumidity>{V}</baserelativehumidity></atmosphere>)",
    R"(<atmosphere model="isa"/><launchaltitude>{V}</launchaltitude>)",
    R"(<launchaltitude>{V}</launchaltitude><atmosphere model="isa"/>)",
    R"(<gravity model="{V}"><value>5</value></gravity>)",
    R"(<gravity model="constant"><value>{V}</value></gravity>)",
    R"(<draglookup file="{V}"/>)",
    R"(<stabilitylookup file="{V}"><row>{V}</row></stabilitylookup>)",
    R"(<draglookup><row>Mach,Cd</row><row>{V},{V}</row></draglookup>)",
    R"(<stabilitylookup><row>Mach;AoA;Cn;Cm;Cp</row><row>{V};{V};{V};{V};{V}</row></stabilitylookup>)",
});

/// Values a file may hold in any of those places: numbers at and beyond every limit, the
/// spellings of what is not a number, the words of other places, and names of files.
constexpr auto kAnyValues = std::to_array<std::string_view>({
    "",
    " ",
    "0",
    "-0.0",
    "1",
    "-1",
    "0.5",
    "1e308",
    "1.7976931348623157e308",
    "-1.7976931348623157e308",
    "-1e308",
    "1e999",
    "-1e999",
    "4.9e-324",
    "1e-999",
    "NaN",
    "Infinity",
    "-Infinity",
    "+Infinity",
    "Inf",
    "0x1p1023",
    "0x1p1024",
    "1.5d",
    "abc",
    "true",
    "average",
    "Average",
    "multilevel",
    "MultiLevel",
    "msl",
    "agl",
    "isa",
    "extendedisa",
    "wgs",
    "constant",
    "flat",
    "wgs84",
    "rk6",
    "2147483647",
    "2147483648",
    "-2147483649",
    "99999999999999999999",
    "Mach,Cd",
    "0,1",
    "/",
    ".",
    "..",
    "r\xC3\xA9sum\xC3\xA9",
});

/// Every <conditions> element made of one place and one value, the values including a number
/// of 4000 digits.
[[nodiscard]] Texts conditionsOfAnyValue()
{
    Texts places;
    for (const std::string_view element : kTextElements)
    {
        places.push_back(std::format("<{0}>{{V}}</{0}>", element));
    }
    places.insert(places.end(), kValuePlaces.begin(), kValuePlaces.end());
    Texts values(kAnyValues.begin(), kAnyValues.end());
    values.emplace_back(4000, '9');

    Texts conditions;
    for (const std::string& place : places)
    {
        for (const std::string& value : values)
        {
            conditions.push_back(
                std::format("<conditions>{}</conditions>", replaceAll(place, "{V}", value)));
        }
    }
    return conditions;
}

/// What is wrong with what the handler makes of @p xml, or nothing: it must not throw, it may
/// fail only as a load fails (ErrorCode::INVALID_ARGUMENT), and its options must hold no number
/// that is not finite.
[[nodiscard]] std::string faultOf(const std::string& xml)
{
    try
    {
        ConditionsFixture           fixture;
        SimulationConditionsHandler handler(fixture.context());
        const HandlerRun            run = runHandler(handler, xml);
        if (!run.result.has_value() && run.result.error().code != ErrorCode::INVALID_ARGUMENT)
        {
            return std::format("{} fails with {}", xml, run.result.error().toString());
        }
        const std::string options =
            QtRocket::Strings::join(" ", describeConditions(handler.getConditions()));
        if (options.contains("NaN") || options.contains("Infinity"))
        {
            return std::format("{} gives {}", xml, options);
        }
    }
    catch (const std::exception& thrown)
    {
        return std::format("{} throws {}", xml, thrown.what());
    }
    return {};
}

/// faultOf() of every element of conditionsOfAnyValue() that has a fault.
[[nodiscard]] Texts faultsOfAnyValue()
{
    Texts faults;
    for (const std::string& xml : conditionsOfAnyValue())
    {
        if (std::string fault = faultOf(xml); !fault.empty())
        {
            faults.push_back(std::move(fault));
        }
    }
    return faults;
}

// Decisions D9 and U3 over the whole element: whatever value stands wherever a file holds one,
// the handler neither throws (no BugError of a setter is reached) nor takes a number that is
// not finite, and what it fails with is the failure of a load.
TEST(SimulationConditionsHandler, NoValueOfAFileMakesItThrowOrTakeANumberThatIsNotFinite)
{
    EXPECT_EQ(conditionsOfAnyValue().size(), (24U + 19U) * 49U);
    EXPECT_EQ(faultsOfAnyValue(), Texts{});
}

}  // namespace
