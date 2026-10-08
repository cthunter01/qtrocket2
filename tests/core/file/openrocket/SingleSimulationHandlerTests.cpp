#include "QtRocket/file/openrocket/SingleSimulationHandler.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/EntryTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "simulation/SimulationOptionsSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <simulations> elements with one simulation, or (with the directive "@element")
// one <simulation> element run through a SingleSimulationHandler; their expectations are what
// OpenRocket makes of the same elements (the Java probe SimProbe of run 9b, part S3; see
// SimulationTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::AirStart;
using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::ErrorCode;
using QtRocket::InMemoryPreferences;
using QtRocket::JavaCode;
using QtRocket::Result;
using QtRocket::ScriptingExtension;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::SimulationExtensionRegistry;
using QtRocket::SimulationStepperMethod;
using QtRocket::SingleSimulationHandler;
using QtRocket::UnknownSimulationExtension;
using QtRocket::WarningSet;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::describe;
using QtRocket::Test::expectSimulationCase;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationCase;
using QtRocket::Test::simulationCaseTestName;
using QtRocket::Test::SimulationFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr auto kSimulationCases = std::to_array<SimulationCase>({
    // BEGIN GENERATED: simulation
    {.name     = "sim: no configid in conditions",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <launchrodlength>1.5</launchrodlength>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: no conditions element",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation conditions not defined, using defaults.
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: empty configid",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid></configid>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 (random)
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=(random) simulated=equal
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: configid that is no UUID",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid>garbage</configid>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 00000000-0000-0000-ffff-fffff49c6835
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=00000000-0000-0000-ffff-fffff49c6835 simulated=equal
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: configid of a configuration the rocket lacks",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid>22222222-2222-2222-2222-222222222222</configid>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 22222222-2222-2222-2222-222222222222
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=22222222-2222-2222-2222-222222222222 simulated=equal
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: configid is the literal error key",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid>ffffffff-f4f2-f1f0-0000-0000000009b9</configid>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 ffffffff-f4f2-f1f0-0000-0000000009b9
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=ffffffff-f4f2-f1f0-0000-0000000009b9 simulated=equal
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = R"out(
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .why = "An id is the error id when its key is the error key (FlightConfigurationId compares "
            "by value), and a simulation cannot have the error id (decision L8). In OpenRocket "
            "only its one ERROR_FCID object is the error id, so an id read from this text is a "
            "configuration like any other."},
    {.name     = "sim: status attribute missing",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation>
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status garbage",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="bogus">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status outdated with data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="outdated">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status external with data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="external">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status notsimulated with data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="notsimulated">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status aborted with data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="aborted">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status cantrun with data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="cantrun">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status uptodate without flightdata",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: status outdated without flightdata",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="outdated">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: no name, unknown simulator and calculator, listener",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <simulator>Euler</simulator>
    <calculator> Foo </calculator>
    <listener>net.sf.openrocket.simulation.listeners.example.CSVSaveListener</listener>
    <listener>  </listener>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown simulator 'Euler' specified, ignoring.
  W[Other,NORMAL] Unknown calculator 'Foo' specified, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Simulation' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: net.sf.openrocket.simulation.listeners.example.CSVSaveListener' config={className = String net.sf.openrocket.simulation.listeners.example.CSVSaveListener; }
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim: extension unknown id, no id, legacy package",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <extension extensionid="com.example.Unknown">
      <entry key="a" type="string">x</entry>
    </extension>
    <extension>
      <entry key="a" type="string">x</entry>
    </extension>
    <extension extensionid="net.sf.openrocket.simulation.extension.example.AirStart">
      <entry key="launchAltitude" type="number">250</entry>
      <entry key="b" type="boolean">TRUE</entry>
      <entry key="s" type="string"> padded </entry>
      <entry key="i" type="number"> 42 </entry>
      <entry key="l" type="number">4294967296</entry>
      <entry key="big" type="number">123456789012345678901234567890</entry>
      <entry key="d" type="number">2.5</entry>
      <entry key="dec" type="number">0.1000000000000000055511151231257827</entry>
      <entry key="nan" type="number">NaN</entry>
      <entry key="bad" type="number">abc</entry>
      <entry key="unknowntype" type="date">x</entry>
      <entry key="notype">x</entry>
      <entry key="list" type="list">
        <entry type="number">1</entry>
        <entry type="string">two</entry>
        <entry type="list">
          <entry type="boolean">true</entry>
        </entry>
      </entry>
      <entry type="string">no key at top level</entry>
    </extension>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation extension with id 'com.example.Unknown' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (250 m, 50 m/s)' config={launchAltitude = Integer 250; b = Boolean true; s = String  padded ; i = Integer 42; l = Long 4294967296; big = BigDecimal 123456789012345678901234567890 unscaled=123456789012345678901234567890 scale=0; d = Double 2.5; dec = BigDecimal 0.1000000000000000055511151231257827 unscaled=1000000000000000055511151231257827 scale=34; }
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Simulation extension with id 'com.example.Unknown' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    ext UnknownSimulationExtension id=com.example.Unknown name='Unknown' config={a = String x; }
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (250 m, 50 m/s)' config={launchAltitude = Integer 250; b = Boolean true; s = String  padded ; i = Integer 42; l = Long 4294967296; big = BigDecimal 123456789012345678901234567890 unscaled=123456789012345678901234567890 scale=0; d = Double 2.5; dec = BigDecimal 0.1000000000000000055511151231257827 unscaled=1000000000000000055511151231257827 scale=34; list = List[Integer 1, String two, List[Boolean true, ], ]; }
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .why = "An extension whose id no provider knows is kept as an UnknownSimulationExtension with "
            "its id and entries, so that a save can write it back (decisions D11 and L10). "
            "OpenRocket gives the same warning and drops it. And an entry of type list is loaded "
            "with its values (decision D9; see ConfigHandler); OpenRocket never loads one."},
    {.name     = "fd2: two flightdata elements",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <flightdata maxaltitude="5">
    </flightdata>
    <flightdata maxaltitude="6">
      <warning type="Other"><description>W2</description></warning>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=0 maxAlt=6.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      fw[Other,NORMAL] id=(random) text='W2' desc='W2' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim2: two conditions elements, name twice",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>First</name>
    <name>Second</name>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <launchrodlength>2.5</launchrodlength>
    </conditions>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <timestep>0.02</timestep>
    </conditions>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Second' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    stepper=RK4 dt=0.02 tmax=1200.0 maxAngle=0.05235987755982988
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "sim2: simulations element with unknown child and text",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
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
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.ScriptingExtension">
      <entry key="script" type="string">var a = 1;
      // two lines</entry>
      <entry key="language" type="string">JavaScript</entry>
      <entry key="enabled" type="boolean">false</entry>
    </extension>
    <extension extensionid="">
      <entry key="a" type="string">x</entry>
    </extension>
    <extension extensionid="   ">
      <entry key="a" type="string">x</entry>
    </extension>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    ext ScriptingExtension id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name='JavaScript script' config={script = String var a = 1;<LF>      // two lines; language = String JavaScript; enabled = Boolean false; }
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: element: a simulation closes with its attributes and text",
     .setup    = "@element\n",
     .xml      = R"xml(
<simulation status="uptodate" extra="1">
  stray
  <name>Sim</name>
  <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  <flightdata maxaltitude="1.5"/>
</simulation>
)xml",
     .java     = R"out(
  closed=simulation {extra=1, status=uptodate} [stray]
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: element: an unknown child gives the simulation its attributes",
     .setup    = "@element\n",
     .xml      = R"xml(
<simulation status="uptodate">
  <name>Sim</name>
  <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  <bogus a="1">text</bogus>
  <flightdata maxaltitude="1.5"/>
</simulation>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulation {a=1} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: element: no children at all",
     .setup    = "@element\n",
     .xml      = R"xml(
<simulation status="uptodate"/>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation conditions not defined, using defaults.
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: names as they are written",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>  padded  </name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <name/>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <name>First</name>
    <name>Second &amp; last</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <name a="1">line one
line two&#9;tab</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 5
  sim[0] name='  padded  ' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[1] name='' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[2] name='Second & last' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[3] name='line one<LF>line two<TAB>tab' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[4] name='Simulation' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: simulator and calculator are compared trimmed and exactly",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator> RK4Simulator </simulator>
    <calculator>
      BarrowmanCalculator
    </calculator>
    <simulator>rk4simulator</simulator>
    <calculator>Barrowman</calculator>
    <simulator/>
    <calculator>  </calculator>
    <simulator>RK6Simulator</simulator>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown simulator 'rk4simulator' specified, ignoring.
  W[Other,NORMAL] Unknown calculator 'Barrowman' specified, ignoring.
  W[Other,NORMAL] Unknown simulator '' specified, ignoring.
  W[Other,NORMAL] Unknown calculator '' specified, ignoring.
  W[Other,NORMAL] Unknown simulator 'RK6Simulator' specified, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: listeners",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <listener>net.sf.openrocket.simulation.listeners.example.CSVSaveListener</listener>
    <listener>  padded.Name  </listener>
    <listener/>
    <listener>   </listener>
    <listener a="1">info.openrocket.core.simulation.listeners.example.AirStart</listener>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: net.sf.openrocket.simulation.listeners.example.CSVSaveListener' config={className = String net.sf.openrocket.simulation.listeners.example.CSVSaveListener; }
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: padded.Name' config={className = String padded.Name; }
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: info.openrocket.core.simulation.listeners.example.AirStart' config={className = String info.openrocket.core.simulation.listeners.example.AirStart; }
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: every status with flight data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate"><name>uptodate</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="loaded"><name>loaded</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="outdated"><name>outdated</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="external"><name>external</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="notsimulated"><name>notsimulated</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="cantrun"><name>cantrun</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="aborted"><name>aborted</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 7
  sim[0] name='uptodate' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[1] name='loaded' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[2] name='outdated' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[3] name='external' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[4] name='notsimulated' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[5] name='cantrun' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[6] name='aborted' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: every status without flight data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate"><name>uptodate</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="loaded"><name>loaded</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="outdated"><name>outdated</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="external"><name>external</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="notsimulated"><name>notsimulated</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="cantrun"><name>cantrun</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="aborted"><name>aborted</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation status="bogus"><name>bogus</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
  <simulation><name>none</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions></simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 9
  sim[0] name='uptodate' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[1] name='loaded' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[2] name='outdated' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[3] name='external' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[4] name='notsimulated' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[5] name='cantrun' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[6] name='aborted' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[7] name='bogus' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[8] name='none' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: the status is trimmed and compared exactly",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status=" uptodate "><name>padded</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="UPTODATE"><name>capitals</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="NOT_SIMULATED"><name>constant</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="not_simulated"><name>underscore</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status=""><name>empty</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
  <simulation status="Out of Date"><name>display</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions><flightdata/></simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 6
  sim[0] name='padded' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[1] name='capitals' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[2] name='constant' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[3] name='underscore' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[4] name='empty' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[5] name='display' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a second conditions element replaces the first",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions>
      <configid>22222222-2222-2222-2222-222222222222</configid>
      <launchrodlength>2.5</launchrodlength>
      <randomseed>7</randomseed>
    </conditions>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <maxtime>300</maxtime>
    </conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    stepper=RK4 dt=0.0 tmax=300.0 maxAngle=0.05235987755982988
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a second conditions element without configid",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <conditions><launchrodlength>2.5</launchrodlength></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: configid twice",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions>
      <configid>22222222-2222-2222-2222-222222222222</configid>
      <configid>11111111-1111-1111-1111-111111111111</configid>
    </conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: configid that spells out the default id",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>ffffffff-f4f2-f1f0-0000-00000000162c</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=ffffffff-f4f2-f1f0-0000-00000000162c simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: configid with white space around it",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid> 11111111-1111-1111-1111-111111111111 </configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 00000000-0000-0000-0000-00003a273ac0
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=00000000-0000-0000-0000-00003a273ac0 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: conditions that fail the load",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <windmodeltype>Gusty</windmodeltype>
    </conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [No enum constant info.openrocket.core.models.wind.WindModelType for string value: Gusty]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: flight data that fail the load",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="time,time"/>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Time already exists.]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: no conditions and a status that is unknown",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation>
    <name>Sim</name>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Simulation conditions not defined, using defaults.
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: conditions with numbers that are not finite",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <launchrodlength>Infinity</launchrodlength>
      <launchroddirection>Infinity</launchroddirection>
      <timestep>Infinity</timestep>
    </conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=differs
    rod=Infinity intoWind=false angle=0.0 dir=NaN
    stepper=RK4 dt=Infinity tmax=1200.0 maxAngle=0.05235987755982988
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal launch rod length defined, ignoring.
  W[Other,NORMAL] Illegal launch rod direction defined, ignoring.
  W[Other,NORMAL] Illegal time step defined, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why = "A number of the conditions that is not finite is not applied and gives the warning of "
            "its element (decision U3; see SimulationConditionsHandler). OpenRocket stores the "
            "infinite rod length and time step and makes a NaN of the infinite direction, after "
            "which the options no longer equal their copy and the simulation is OUTDATED."},
    {.name     = "s3: conditions with options of every kind",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <launchrodlength>1.5</launchrodlength>
      <launchintowind>true</launchintowind>
      <launchrodangle>5.0</launchrodangle>
      <launchroddirection>90.0</launchroddirection>
      <wind model="multilevel" altituderef="agl">
        <windlevel altitude="0.0" speed="3.8" direction="3.0" standarddeviation="1.52"/>
        <windlevel altitude="100.0" speed="5" direction="1.0" standarddeviation="0.5"/>
      </wind>
      <windmodeltype>MultiLevel</windmodeltype>
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>wgs84</geodeticmethod>
      <randomseed>42</randomseed>
      <atmosphere model="extendedisa">
        <basetemperature>290</basetemperature>
        <basepressure>100000</basepressure>
        <baserelativehumidity>0.5</baserelativehumidity>
      </atmosphere>
      <gravity model="constant"><value>9.5</value></gravity>
      <timestep>0.02</timestep>
      <maxtime>600.0</maxtime>
      <recoveryspeedwarning>25</recoveryspeedwarning>
      <draglookup>
        <row>Mach,Cd</row>
        <row>0,0.3</row>
        <row>1,0.5</row>
      </draglookup>
    </conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=true angle=0.08726646259971647 dir=1.5707963267948966
    wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=AGL(0.0,3.8,3.0,1.52)(100.0,5.0,1.0,0.5)
    site=100.0,32.0,-106.0 geo=WGS84
    stepper=RK4 dt=0.02 tmax=600.0 maxAngle=0.05235987755982988
    seedFixed=true seed=42
    atmosphere=isa:false T=290.0 p=100000.0 hum=0.5
    gravity=CONSTANT/9.5
    thresholds=25.0/3.048/30.48/15.24
    drag=path:null table:[aoa=false mach=0.0..1.0 cd=0.4] rows:[Mach,Cd|0,0.3|1,0.5]
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: extensions that the providers know",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.example.AirStart">
      <entry key="launchAltitude" type="number">250</entry>
      <entry key="launchVelocity" type="number">12.5</entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.example.RollControl">
      <entry key="controlFinName" type="string">CONTROL</entry>
      <entry key="startTime" type="number">0.5</entry>
      <entry key="setPoint" type="number">0.0</entry>
      <entry key="finRate" type="number">10.0</entry>
      <entry key="maxFinAngle" type="number">15.0</entry>
      <entry key="KP" type="number">0.007</entry>
      <entry key="KI" type="number">0.2</entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.ScriptingExtension">
      <entry key="script" type="string">var a = 1;
// two lines</entry>
      <entry key="language" type="string">JavaScript</entry>
      <entry key="enabled" type="boolean">false</entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.JavaCode">
      <entry key="className" type="string">some.Listener</entry>
    </extension>
    <extension extensionid="info.openrocket.core.simulation.extension.example.AirStart"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (250 m, 12.5 m/s)' config={launchAltitude = Integer 250; launchVelocity = Double 12.5; }
    ext RollControl id=info.openrocket.core.simulation.extension.example.RollControl name='Roll Control' config={controlFinName = String CONTROL; startTime = Double 0.5; setPoint = Double 0.0; finRate = Double 10.0; maxFinAngle = Double 15.0; KP = Double 0.007; KI = Double 0.2; }
    ext ScriptingExtension id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name='JavaScript script' config={script = String var a = 1;<LF>// two lines; language = String JavaScript; enabled = Boolean false; }
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: some.Listener' config={className = String some.Listener; }
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (100 m, 50 m/s)' config={}
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: an extension that no provider knows",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="com.example.Unknown">
      <entry key="a" type="string">x</entry>
      <entry key="n" type="number">3</entry>
    </extension>
    <extension extensionid="com.example.Empty"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation extension with id 'com.example.Unknown' not found.
  W[Other,NORMAL] Simulation extension with id 'com.example.Empty' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Simulation extension with id 'com.example.Unknown' not found.
  W[Other,NORMAL] Simulation extension with id 'com.example.Empty' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext UnknownSimulationExtension id=com.example.Unknown name='Unknown' config={a = String x; n = Integer 3; }
    ext UnknownSimulationExtension id=com.example.Empty name='Empty' config={}
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why = "An extension whose id no provider knows is kept as an UnknownSimulationExtension with "
            "its id and entries, so that a save can write it back (decisions D11 and L10). "
            "OpenRocket gives the same warning and drops it."},
    {.name     = "s3: extension ids of the old package",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="net.sf.openrocket.simulation.extension.example.AirStart">
      <entry key="launchAltitude" type="number">250</entry>
    </extension>
    <extension extensionid="net.sf.openrocket.simulation.extension.impl.JavaCode">
      <entry key="className" type="string">net.sf.openrocket.simulation.listeners.example.CSVSaveListener</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (250 m, 50 m/s)' config={launchAltitude = Integer 250; }
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: net.sf.openrocket.simulation.listeners.example.CSVSaveListener' config={className = String net.sf.openrocket.simulation.listeners.example.CSVSaveListener; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: old package names in an id that nobody knows",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="net.sf.openrocket.x.net.sf.openrocket.Twice"/>
    <extension extensionid="NET.SF.OPENROCKET.Capitals"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.x.info.openrocket.core.Twice' not found.
  W[Other,NORMAL] Simulation extension with id 'NET.SF.OPENROCKET.Capitals' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.x.info.openrocket.core.Twice' not found.
  W[Other,NORMAL] Simulation extension with id 'NET.SF.OPENROCKET.Capitals' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext UnknownSimulationExtension id=info.openrocket.core.x.info.openrocket.core.Twice name='Twice' config={}
    ext UnknownSimulationExtension id=NET.SF.OPENROCKET.Capitals name='Capitals' config={}
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why = "An extension whose id no provider knows is kept as an UnknownSimulationExtension with "
            "its id and entries, so that a save can write it back (decisions D11 and L10). "
            "OpenRocket gives the same warning and drops it."},
    {.name     = "s3: extensions without an id",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension>
      <entry key="a" type="string">x</entry>
    </extension>
    <extension extensionid="">
      <entry key="a" type="string">x</entry>
    </extension>
    <extension extensionid="   ">
      <entry key="a" type="string">x</entry>
    </extension>
    <extension id="info.openrocket.core.simulation.extension.example.AirStart"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: an extension id with white space around it",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid=" info.openrocket.core.simulation.extension.example.AirStart "/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Simulation extension with id ' info.openrocket.core.simulation.extension.example.AirStart ' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Simulation extension with id ' info.openrocket.core.simulation.extension.example.AirStart ' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext UnknownSimulationExtension id= info.openrocket.core.simulation.extension.example.AirStart  name='AirStart ' config={}
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why = "An extension whose id no provider knows is kept as an UnknownSimulationExtension with "
            "its id and entries, so that a save can write it back (decisions D11 and L10). "
            "OpenRocket gives the same warning and drops it."},
    {.name     = "s3: entries of every type in an extension",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.JavaCode">
      <entry key="b" type="boolean">TRUE</entry>
      <entry key="notb" type="boolean"> true </entry>
      <entry key="s" type="string"> padded </entry>
      <entry key="i" type="number"> 42 </entry>
      <entry key="l" type="number">4294967296</entry>
      <entry key="big" type="number">123456789012345678901234567890</entry>
      <entry key="d" type="number">2.5</entry>
      <entry key="dec" type="number">2.50</entry>
      <entry key="exp" type="number">1e1</entry>
      <entry key="nan" type="number">NaN</entry>
      <entry key="inf" type="number">Infinity</entry>
      <entry key="huge" type="number">1e999</entry>
      <entry key="bad" type="number">abc</entry>
      <entry key="unknowntype" type="date">x</entry>
      <entry key="notype">x</entry>
      <entry type="string">no key</entry>
      <entry key="d" type="string">a key twice</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: none' config={b = Boolean true; notb = Boolean false; s = String  padded ; i = Integer 42; l = Long 4294967296; big = BigDecimal 123456789012345678901234567890 unscaled=123456789012345678901234567890 scale=0; d = String a key twice; dec = BigDecimal 2.50 unscaled=250 scale=2; exp = BigDecimal 1E+1 unscaled=1 scale=-1; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: list entries in an extension",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.JavaCode">
      <entry key="before" type="number">1</entry>
      <entry key="list" type="list">
        <entry type="number">1</entry>
        <entry type="string">two</entry>
        <entry type="list">
          <entry type="boolean">true</entry>
        </entry>
      </entry>
      <entry key="empty" type="list"/>
      <entry key="after" type="number">2</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: none' config={before = Integer 1; after = Integer 2; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: none' config={before = Integer 1; list = List[Integer 1, String two, List[Boolean true, ], ]; empty = List[]; after = Integer 2; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why = "An entry of type list is loaded with its values (decision D9; see ConfigHandler). "
            "OpenRocket's saver writes such entries and its loader never loads one."},
    {.name     = "s3: an entry with a child element",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.JavaCode">
      <entry key="className" type="string">a<b/>c</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element b, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: an entry with a child that has an extension id",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="com.example.Outer">
      <entry key="k" type="string">v</entry>
      <entry key="className" type="string">a<b extensionid="info.openrocket.core.simulation.extension.impl.JavaCode"/>c</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element b, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: an extension element in an extension, text and other children",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.JavaCode" other="1">
      stray text
      <extension extensionid="info.openrocket.core.simulation.extension.example.AirStart">inner</extension>
      <other x="1">text</other>
      <entry key="className" type="string">kept.Name</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown text in element 'extension', ignoring.
  W[Other,NORMAL] Unknown attributes in element 'extension', ignoring.
  W[Other,NORMAL] Unknown text in element 'other', ignoring.
  W[Other,NORMAL] Unknown attributes in element 'other', ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: kept.Name' config={className = String kept.Name; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: listeners and extensions keep the order of the file",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <listener>first.Listener</listener>
    <extension extensionid="info.openrocket.core.simulation.extension.example.RollControl"/>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <listener>third.Listener</listener>
    <flightdata maxaltitude="1.5"/>
    <extension extensionid="info.openrocket.core.simulation.extension.example.AirStart"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: first.Listener' config={className = String first.Listener; }
    ext RollControl id=info.openrocket.core.simulation.extension.example.RollControl name='Roll Control' config={}
    ext JavaCode id=info.openrocket.core.simulation.extension.impl.JavaCode name='Java code: third.Listener' config={className = String third.Listener; }
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (100 m, 50 m/s)' config={}
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a script that is enabled stays as the file has it",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.impl.ScriptingExtension">
      <entry key="script" type="string">print("x");</entry>
      <entry key="language" type="string">JavaScript</entry>
      <entry key="enabled" type="boolean">true</entry>
    </extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext ScriptingExtension id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name='JavaScript script' config={script = String print("x");; language = String JavaScript; enabled = Boolean true; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: empty flight data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: flight data with an abort",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="time,altitude">
        <datapoint>0,0</datapoint>
        <event time="4" type="simabort" id="bbbbbbbb-0000-0000-0000-000000000002" cause="nomotorsdefined"/>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=ABORTED status=ABORTED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
        event SIM_ABORT t=4.0 id=bbbbbbbb-0000-0000-0000-000000000002 src=null data=[SimulationAbort:No motors defined in the simulation]
        row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: flight data with warnings, a branch and events",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="loaded">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="99">
      <warning type="HighSpeedDeployment">
        <id>cccccccc-0000-0000-0000-000000000001</id>
        <description>Recovery device deployment at high speed (26.9 m/s)</description>
        <priority>NORMAL</priority>
        <source>aaaaaaaa-0000-0000-0000-000000000004</source>
        <parameter>26.9</parameter>
      </warning>
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <event time="1" type="simwarn" id="bbbbbbbb-0000-0000-0000-000000000002" warnid="cccccccc-0000-0000-0000-000000000001"/>
        <event time="1.5" type="recoverydevicedeployment" id="bbbbbbbb-0000-0000-0000-000000000003" source="dddddddd-0000-0000-0000-000000000009"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Recovery device deployment at high speed (26.9 m/s):  "P"' desc='Recovery device deployment at high speed (26.9 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004; param=26.9
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        event SIM_WARN t=1.0 id=bbbbbbbb-0000-0000-0000-000000000002 src=null data=[RecoveryHighSpeedDeployment:Recovery device deployment at high speed (26.9 m/s):  "P"]
        event RECOVERY_DEVICE_DEPLOYMENT t=1.5 id=bbbbbbbb-0000-0000-0000-000000000003 src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a second flight data element replaces the first",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5">
      <warning>stored with the first</warning>
      <databranch name="A" types="time,altitude">
        <datapoint>0,0</datapoint>
        <datapoint>1,5</datapoint>
      </databranch>
    </flightdata>
    <flightdata maxaltitude="2.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=2.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: everything in one simulation",
     .setup    = "@digest\n",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Full</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <launchrodlength>1.5</launchrodlength>
      <timestep>0.05</timestep>
    </conditions>
    <landingdispersion runs="500" seed="12345">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
    </landingdispersion>
    <plotappearance>
      <series symbol="h" linestyle="dashed" red="255" green="0" blue="0" alpha="255"/>
    </plotappearance>
    <extension extensionid="info.openrocket.core.simulation.extension.example.AirStart">
      <entry key="launchAltitude" type="number">100.0</entry>
      <entry key="launchVelocity" type="number">50.0</entry>
    </extension>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Full' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.0 dir=0.0
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    ext AirStart id=info.openrocket.core.simulation.extension.example.AirStart name='Air-start (100 m, 50 m/s)' config={launchAltitude = Double 100.0; launchVelocity = Double 50.0; }
    plot 'h': color=255,0,0,255 style=DASHED
    dispersion: {runs=500, seed=12345} [{distribution=normal, parameter=windspeed, spread=0.5}]
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        col time: min=0.0 max=2.0 first=0.0 last=2.0 sum=3.0 nans=0
        col altitude: min=0.0 max=10.0 first=0.0 last=5.0 sum=15.0 nans=0
        col velocity_total: min=0.0 max=20.0 first=0.0 last=NaN sum=20.0 nans=1
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "r3: the extensions of OpenRocket that have no provider here",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <extension extensionid="info.openrocket.core.simulation.extension.example.CSVSave"/>
    <extension extensionid="info.openrocket.core.simulation.extension.example.DampingMoment"><entry key="a" type="number">5</entry></extension>
    <extension extensionid="info.openrocket.core.simulation.extension.example.PrintSimulation"/>
    <extension extensionid="net.sf.openrocket.simulation.extension.example.StopSimulation"><entry key="reportRate" type="number">50</entry><entry key="stopStep" type="number">2000</entry><entry key="stopTime" type="number">3.5</entry></extension>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext CSVSave id=info.openrocket.core.simulation.extension.example.CSVSave name='CSVSave' config={}
    ext DampingMoment id=info.openrocket.core.simulation.extension.example.DampingMoment name='Damping moment coefficient (Cdm) (built-in)' config={a = Integer 5; }
    ext PrintSimulation id=info.openrocket.core.simulation.extension.example.PrintSimulation name='Print Simulation Values' config={}
    ext StopSimulation id=info.openrocket.core.simulation.extension.example.StopSimulation name='Stop Simulation' config={reportRate = Integer 50; stopStep = Integer 2000; stopTime = Double 3.5; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.simulation.extension.example.CSVSave' not found.
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.simulation.extension.example.DampingMoment' not found.
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.simulation.extension.example.PrintSimulation' not found.
  W[Other,NORMAL] Simulation extension with id 'info.openrocket.core.simulation.extension.example.StopSimulation' not found.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    ext UnknownSimulationExtension id=info.openrocket.core.simulation.extension.example.CSVSave name='CSVSave' config={}
    ext UnknownSimulationExtension id=info.openrocket.core.simulation.extension.example.DampingMoment name='DampingMoment' config={a = Integer 5; }
    ext UnknownSimulationExtension id=info.openrocket.core.simulation.extension.example.PrintSimulation name='PrintSimulation' config={}
    ext UnknownSimulationExtension id=info.openrocket.core.simulation.extension.example.StopSimulation name='StopSimulation' config={reportRate = Integer 50; stopStep = Integer 2000; stopTime = Double 3.5; }
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .why =
         "QtRocket ships providers for four of OpenRocket's eight extensions: AirStart, "
         "RollControl, JavaCode and ScriptingExtension (decision U1; see "
         "SimulationExtensionRegistry::bundled()). CSVSave, DampingMoment, PrintSimulation and "
         "StopSimulation have none yet, so a file that uses one of them loads with the warning for "
         "an extension nobody knows and keeps it as an UnknownSimulationExtension with its id and "
         "entries (decision D11), where OpenRocket loads the extension without a warning."},
    // END GENERATED: simulation
});

/// A warning's text holds an angle or a speed in the default units, and so does the name of
/// an AirStart extension.
class SimulationElements : public ::testing::TestWithParam<SimulationCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(SimulationElements, LoadAsInOpenRocketButWhereStated)
{
    expectSimulationCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, SimulationElements, ::testing::ValuesIn(kSimulationCases),
                         simulationCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// A <simulation> of the configuration that has a motor, with @p children between its
/// conditions and its stored results.
[[nodiscard]] std::string simulationWith(std::string_view children)
{
    return std::string(
               "<simulation status='uptodate'><name>Sim</name>"
               "<conditions><configid>11111111-1111-1111-1111-111111111111</configid>"
               "</conditions>") +
           std::string(children) + "<flightdata maxaltitude='1.5'/></simulation>";
}

/// The simulation that a new handler in @p fixture makes of @p xml, which loads without a
/// failure; the warnings of the load go to @p warnings.
[[nodiscard]] std::shared_ptr<Simulation> load(SimulationFixture& fixture, std::string_view xml,
                                               Texts& warnings)
{
    SingleSimulationHandler handler(fixture.context());
    const HandlerRun        run = runHandler(handler, xml);
    EXPECT_TRUE(run.result.has_value());
    warnings = run.texts();
    return handler.getSimulation();
}

TEST(SingleSimulationHandler, TheContextNeedsADocumentAndAPreferenceStore)
{
    const DocumentLoadingContext empty;
    EXPECT_THROW({ const SingleSimulationHandler handler(empty); }, BugError);

    SimulationFixture      fixture;
    DocumentLoadingContext withoutPreferences = fixture.context();
    withoutPreferences.setPreferences(nullptr);
    EXPECT_THROW({ const SingleSimulationHandler handler(withoutPreferences); }, BugError);

    DocumentLoadingContext withoutDocument = fixture.context();
    withoutDocument.setOpenRocketDocument(nullptr);
    EXPECT_THROW({ const SingleSimulationHandler handler(withoutDocument); }, BugError);
}

TEST(SingleSimulationHandler, TheSimulationIsThereWhenTheElementHasEnded)
{
    SimulationFixture       fixture;
    SingleSimulationHandler handler(fixture.context());
    EXPECT_EQ(&handler.getDocument(), &fixture.document());
    EXPECT_EQ(handler.getSimulation(), nullptr);

    const HandlerRun run = runHandler(handler, simulationWith(""));
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    // What the parent of the element is told: the attributes of the simulation, for the
    // warnings SimulationsHandler decides on.
    EXPECT_EQ(run.element, "simulation");
    EXPECT_EQ(run.attributes.size(), 1U);

    const std::shared_ptr<Simulation> simulation = handler.getSimulation();
    ASSERT_NE(simulation, nullptr);
    ASSERT_EQ(fixture.document().getSimulationCount(), 1U);
    EXPECT_EQ(fixture.document().getSimulation(0), simulation);
    EXPECT_EQ(simulation->getDocument(), &fixture.document());
    EXPECT_EQ(&simulation->getRocket(), &fixture.rocket());
    EXPECT_EQ(simulation->getPreferences(), &fixture.preferences());
    EXPECT_EQ(simulation->getName(), "Sim");
    EXPECT_EQ(simulation->getStoredStatus(), Simulation::Status::LOADED);
    EXPECT_EQ(simulation->getId().toString(), "11111111-1111-1111-1111-111111111111");
    ASSERT_NE(simulation->getSimulatedData(), nullptr);
    EXPECT_EQ(simulation->getSimulatedData()->getMaxAltitude(), 1.5);
    EXPECT_FALSE(simulation->getLandingDispersionSettings().has_value());
    EXPECT_TRUE(simulation->getPlotAppearances().empty());
    EXPECT_TRUE(simulation->getSimulationExtensions().empty());
}

// The loaded options equal the simulated conditions the constructor copied from them, so that
// the loader's next step, syncModId(), leaves a loaded simulation LOADED; and they are still
// connected to the simulation, which tells of their changes and is OUTDATED after one.
TEST(SingleSimulationHandler, TheOptionsBelongToTheSimulation)
{
    SimulationFixture                 fixture;
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation =
        load(fixture,
             "<simulation status='uptodate'><conditions>"
             "<configid>11111111-1111-1111-1111-111111111111</configid>"
             "<launchrodlength>1.5</launchrodlength><timestep>0.02</timestep>"
             "<simulationsteppermethod>rk6</simulationsteppermethod>"
             "</conditions><flightdata/></simulation>",
             warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(warnings, Texts{});
    EXPECT_EQ(simulation->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_EQ(simulation->getOptions().getTimeStep(), 0.02);
    ASSERT_NE(simulation->getSimulatedConditions(), nullptr);
    EXPECT_TRUE(*simulation->getSimulatedConditions() == simulation->getOptions());
    // Loading a stepper method writes the choice into the context's preference store, as
    // OpenRocket's loader writes the application's preferences (decision L8).
    EXPECT_EQ(simulation->getOptions().getSimulationStepperMethodChoice(),
              SimulationStepperMethod::RK6);
    EXPECT_EQ(fixture.preferences().getSimulationStepperMethodName(), "RK6");

    simulation->syncModId();
    EXPECT_EQ(simulation->getStatus(), Simulation::Status::LOADED);
    EXPECT_TRUE(simulation->validateInputs().has_value());

    const ChangeCounter changes(simulation->changed());
    simulation->getOptions().setLaunchRodLength(2.0);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(simulation->getStatus(), Simulation::Status::OUTDATED);
}

// Until the loader has called syncModId(), a loaded simulation of another configuration than
// the rocket's selected one compares the modification id of the wrong configuration.
TEST(SingleSimulationHandler, ALoadedSimulationIsOutdatedUntilItsModIdIsSynchronised)
{
    SimulationFixture                 fixture;
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation = load(fixture, simulationWith(""), warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(simulation->getStoredStatus(), Simulation::Status::LOADED);
    EXPECT_EQ(simulation->clone()->getStatus(), Simulation::Status::OUTDATED);
    simulation->syncModId();
    EXPECT_EQ(simulation->getStatus(), Simulation::Status::LOADED);
}

// Without a registry in the context no extension id is known: every extension is kept as an
// unknown one, with OpenRocket's warning.
TEST(SingleSimulationHandler, WithoutARegistryEveryExtensionIsUnknown)
{
    SimulationFixture fixture;
    fixture.context().setSimulationExtensionRegistry(nullptr);
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation = load(
        fixture,
        simulationWith("<extension extensionid='info.openrocket.core.simulation.extension."
                       "example.AirStart'><entry key='launchAltitude' type='number'>250</entry>"
                       "</extension>"),
        warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(warnings, Texts{"Simulation extension with id "
                              "'info.openrocket.core.simulation.extension.example.AirStart' not "
                              "found."});
    ASSERT_EQ(simulation->getSimulationExtensions().size(), 1U);
    const SimulationExtension& extension = *simulation->getSimulationExtensions().at(0);
    EXPECT_NE(dynamic_cast<const UnknownSimulationExtension*>(&extension), nullptr);
    EXPECT_EQ(extension.getId(), AirStart::kId);
    EXPECT_EQ(describe(extension.getConfig()), "{launchAltitude = Integer 250; }");
}

// An empty registry is a registry: it knows no id either.
TEST(SingleSimulationHandler, WithAnEmptyRegistryEveryExtensionIsUnknown)
{
    SimulationFixture                 fixture;
    const SimulationExtensionRegistry empty;
    fixture.context().setSimulationExtensionRegistry(&empty);
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation =
        load(fixture,
             simulationWith("<extension extensionid='net.sf.openrocket.simulation.extension.impl."
                            "JavaCode'/>"),
             warnings);
    ASSERT_NE(simulation, nullptr);
    // The id in the warning and in the extension is the one of today's package.
    EXPECT_EQ(warnings,
              Texts{"Simulation extension with id "
                    "'info.openrocket.core.simulation.extension.impl.JavaCode' not found."});
    ASSERT_EQ(simulation->getSimulationExtensions().size(), 1U);
    EXPECT_EQ(simulation->getSimulationExtensions().at(0)->getId(), JavaCode::kId);
}

// A simulation with an extension that nobody provides says so when it is asked to run, and
// does not fly without the extension (decision D11).
TEST(SingleSimulationHandler, ASimulationWithAnUnknownExtensionDoesNotRun)
{
    SimulationFixture                 fixture;
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation =
        load(fixture,
             simulationWith("<extension extensionid='com.example.Wobble'>"
                            "<entry key='amount' type='number'>3</entry></extension>"),
             warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(warnings, Texts{"Simulation extension with id 'com.example.Wobble' not found."});

    const Result<void> ran = simulation->simulate();
    ASSERT_FALSE(ran.has_value());
    EXPECT_EQ(ran.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(ran.error().message, "Simulation extension with id 'com.example.Wobble' not found.");
}

// The extension of a known id is the provider's own object, with the entries as its
// configuration; the extension of a <listener> is a JavaCode with the class name.
TEST(SingleSimulationHandler, TheExtensionsAreTheProvidersObjects)
{
    const DefaultUnitsGuard           units;
    SimulationFixture                 fixture;
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation =
        load(fixture,
             simulationWith("<listener> some.Listener </listener>"
                            "<extension extensionid='info.openrocket.core.simulation.extension."
                            "example.AirStart'>"
                            "<entry key='launchAltitude' type='number'>250</entry>"
                            "<entry key='launchVelocity' type='number'>12.5</entry></extension>"),
             warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(warnings, Texts{});
    ASSERT_EQ(simulation->getSimulationExtensions().size(), 2U);

    const auto* const listener =
        dynamic_cast<const JavaCode*>(simulation->getSimulationExtensions().at(0).get());
    ASSERT_NE(listener, nullptr);
    EXPECT_EQ(listener->getClassName(), "some.Listener");

    const auto* const airStart =
        dynamic_cast<const AirStart*>(simulation->getSimulationExtensions().at(1).get());
    ASSERT_NE(airStart, nullptr);
    EXPECT_EQ(airStart->getLaunchAltitude(), 250.0);
    EXPECT_EQ(airStart->getLaunchVelocity(), 12.5);
    EXPECT_EQ(airStart->getName(), "Air-start (250 m, 12.5 m/s)");
}

// What the loader does with every extension after the whole document
// (SimulationExtension::documentLoaded()): a script that the file has enabled is disabled, with
// OpenRocket's warning. The handler itself leaves the script as the file has it.
TEST(SingleSimulationHandler, AnEnabledScriptIsDisabledByTheLoadersNextStep)
{
    SimulationFixture                 fixture;
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation = load(
        fixture,
        simulationWith("<extension extensionid='info.openrocket.core.simulation.extension.impl."
                       "ScriptingExtension'>"
                       "<entry key='script' type='string'>print(1);</entry>"
                       "<entry key='language' type='string'>JavaScript</entry>"
                       "<entry key='enabled' type='boolean'>true</entry></extension>"),
        warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(warnings, Texts{});
    ASSERT_EQ(simulation->getSimulationExtensions().size(), 1U);
    auto* const script =
        dynamic_cast<ScriptingExtension*>(simulation->getSimulationExtensions().at(0).get());
    ASSERT_NE(script, nullptr);
    EXPECT_TRUE(script->isEnabled());
    EXPECT_EQ(script->getScript(), "print(1);");

    WarningSet loadWarnings;
    script->documentLoaded(fixture.document(), *simulation, loadWarnings);
    EXPECT_FALSE(script->isEnabled());
    EXPECT_EQ(warningTexts(loadWarnings), Texts{std::string(ScriptingExtension::kDisabledWarning)});
    // Once is enough: the script is disabled now.
    script->documentLoaded(fixture.document(), *simulation, loadWarnings);
    EXPECT_EQ(loadWarnings.size(), 1U);
}

// The document owns the simulation; the handler only points at it.
TEST(SingleSimulationHandler, TheHandlerDoesNotKeepTheSimulationAlive)
{
    SimulationFixture       fixture;
    SingleSimulationHandler handler(fixture.context());
    const HandlerRun        run = runHandler(handler, simulationWith(""));
    EXPECT_TRUE(run.result.has_value());
    ASSERT_NE(handler.getSimulation(), nullptr);
    EXPECT_EQ(handler.getSimulation(), fixture.document().getSimulation(0));

    static_cast<void>(fixture.document().removeSimulation(0));
    fixture.document().clearUndo();
    EXPECT_EQ(handler.getSimulation(), nullptr);
}

// A failure leaves the document as it was and the handler without a simulation.
TEST(SingleSimulationHandler, AFailureLeavesNoSimulation)
{
    SimulationFixture       fixture;
    SingleSimulationHandler handler(fixture.context());
    const HandlerRun        run =
        runHandler(handler,
                   "<simulation status='uptodate'><name>Sim</name>"
                   "<conditions><launchrodlength>1</launchrodlength></conditions></simulation>");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(run.result.error().message,
              "Attempted to set the configuration to an error id. Not Allowed!");
    EXPECT_EQ(handler.getSimulation(), nullptr);
    EXPECT_EQ(fixture.document().getSimulationCount(), 0U);
    // No configuration was made in the rocket for it.
    EXPECT_EQ(fixture.rocket().getIds().size(), 1U);
}

// The store the options and the simulation keep is the context's, not a default one: a value
// the file does not give is the store's.
TEST(SingleSimulationHandler, WhatTheFileLeavesOutComesFromTheContextsPreferences)
{
    // Declared first: the simulation in the fixture's document refers to it.
    InMemoryPreferences other;
    other.putDouble("LaunchRodLength", 3.25);
    SimulationFixture fixture;
    fixture.context().setPreferences(&other);
    Texts                             warnings;
    const std::shared_ptr<Simulation> simulation = load(fixture, simulationWith(""), warnings);
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(simulation->getPreferences(), &other);
    EXPECT_EQ(simulation->getOptions().getLaunchRodLength(), 3.25);
}

}  // namespace
