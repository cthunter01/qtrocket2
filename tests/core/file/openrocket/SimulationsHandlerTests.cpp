#include "QtRocket/file/openrocket/SimulationsHandler.h"

#include <array>
#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "document/DocumentTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <simulations> elements, run through SimulationsHandler as the loader runs
// them; their expectations are what OpenRocket makes of the same elements (the Java probe
// SimProbe of run 9b, part S3; see SimulationTestSupport.h), but where a case states that
// QtRocket differs. The cases here are about the <simulations> element itself and about what an
// ignored element does to the elements around it; the cases about what a <simulation> holds
// are in the tests of SingleSimulationHandler.

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::FlightDataType;
using QtRocket::Simulation;
using QtRocket::SimulationsHandler;
using QtRocket::WarningSet;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::DocumentRecorder;
using QtRocket::Test::expectSimulationCase;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationCase;
using QtRocket::Test::simulationCaseTestName;
using QtRocket::Test::SimulationFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr auto kSimulationsCases = std::to_array<SimulationCase>({
    // BEGIN GENERATED: simulations
    {.name     = "sim: unknown element (attribute slip)",
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
    <bogus a="1">text</bogus>
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
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
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
    {.name     = "sim: unknown element after flightdata",
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
    <flightdata maxaltitude="10.5">
    </flightdata>
    <bogus a="1"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    data: branches=0 maxAlt=10.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: empty simulations element",
     .setup    = "",
     .xml      = R"xml(
<simulations/>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: simulations element with attributes and text",
     .setup    = "",
     .xml      = R"xml(
<simulations a="1" status="uptodate">
  stray text
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {a=1, status=uptodate} [stray text]
  configs: 11111111-1111-1111-1111-111111111111
  sims: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: two simulations in the order of the file",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>First</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
  <simulation status="notsimulated">
    <name>Second</name>
    <conditions><configid>22222222-2222-2222-2222-222222222222</configid></conditions>
  </simulation>
  <simulation status="outdated">
    <name>Third</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid><timestep>0.02</timestep></conditions>
    <flightdata maxaltitude="3.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 22222222-2222-2222-2222-222222222222
  sims: 3
  sim[0] name='First' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[1] name='Second' stored=NOT_SIMULATED presync=CANT_RUN status=CANT_RUN fcid=22222222-2222-2222-2222-222222222222 simulated=equal
    data: null
  sim[2] name='Third' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    stepper=RK4 dt=0.02 tmax=1200.0 maxAngle=0.05235987755982988
    data: branches=0 maxAlt=3.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: unknown child of simulations before a simulation",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <bogus x="1">text<inner y="2"/></bogus>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  closed=simulations {x=1} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: text and more attributes in a simulation",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate" extra="1">
    stray
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown text in element 'simulation', ignoring.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: two unknown children",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <bogus a="1"/>
    <other b="2"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  W[Other,NORMAL] Unknown element 'other', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {a=1} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: unknown child with a status attribute of its own",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="outdated">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <bogus status="uptodate"/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  closed=simulations {status=outdated} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: name with a child element",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>before<b x="1">bold</b>after</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element b, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown text in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='after' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: two simulations with empty configids",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>One</name>
    <conditions><configid></configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <name>Two</name>
    <conditions><configid/></conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 (random) (random)
  sims: 2
  sim[0] name='One' stored=NOT_SIMULATED presync=CANT_RUN status=CANT_RUN fcid=(random) simulated=equal
    data: null
  sim[1] name='Two' stored=NOT_SIMULATED presync=CANT_RUN status=CANT_RUN fcid=(random) simulated=equal
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: two simulations of one new configuration",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>One</name>
    <conditions><configid>33333333-3333-3333-3333-333333333333</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
  <simulation status="uptodate">
    <name>Two</name>
    <conditions><configid>33333333-3333-3333-3333-333333333333</configid></conditions>
    <flightdata maxaltitude="2.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111 33333333-3333-3333-3333-333333333333
  sims: 2
  sim[0] name='One' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=33333333-3333-3333-3333-333333333333 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
  sim[1] name='Two' stored=LOADED presync=CANT_RUN status=CANT_RUN fcid=33333333-3333-3333-3333-333333333333 simulated=equal
    data: branches=0 maxAlt=2.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a failure in the second simulation keeps the first",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>One</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
  <simulation status="uptodate">
    <name>Two</name>
    <conditions><launchrodlength>1</launchrodlength></conditions>
  </simulation>
  <simulation status="uptodate">
    <name>Three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Attempted to set the configuration to an error id. Not Allowed!]
  closed=(not closed)
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='One' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: unknown child in flight data",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5">
      <bogus maxaltitude="7.5"/>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus' encountered, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=7.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "r3: an expression with the symbol Unknown names the columns without a name",
     .setup    = "@expression Odd one|Unknown|m|h\n",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>one</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="Mystery"><datapoint>1</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>two</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="B" types=""><datapoint>2</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="C" types="Other"><datapoint>3</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>four</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="D" types=""><datapoint>4</datapoint></databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 4
  sim[0] name='one' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Mystery{Unknown,Custom,-,<U+200B>}
        row: 1.0
  sim[1] name='two' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Odd one{Unknown,Custom,-,<U+200B>}
        row: 2.0
  sim[2] name='three' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'C' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Other{Unknown,Custom,-,<U+200B>}
        row: 3.0
  sim[3] name='four' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'D' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Odd one{Unknown,Custom,-,<U+200B>}
        row: 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "r3: columns without a name after unknown columns",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>one</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="Mystery"><datapoint>1</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>two</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="B" types=""><datapoint>2</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="C" types="Other"><datapoint>3</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>four</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="D" types=""><datapoint>4</datapoint></databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 4
  sim[0] name='one' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Mystery{Unknown,Custom,-,<U+200B>}
        row: 1.0
  sim[1] name='two' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Mystery{Unknown,Custom,-,<U+200B>}
        row: 2.0
  sim[2] name='three' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'C' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Other{Unknown,Custom,-,<U+200B>}
        row: 3.0
  sim[3] name='four' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'D' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Other{Unknown,Custom,-,<U+200B>}
        row: 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "r3: two expressions of one symbol in two simulations",
     .setup    = "@expression First|qtrSame|m|h\n"
                 "@expression Second|qtrSame|m/s|Vt\n",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>one</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="First,Second"><datapoint>1,2</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>two</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="B" types="Second,First"><datapoint>2,1</datapoint></databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 2
  sim[0] name='one' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=First{qtrSame,Custom,-,m} | Second{qtrSame,Custom,-,m/s}
        row: 1.0 2.0
  sim[1] name='two' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Second{qtrSame,Custom,-,m/s} | First{qtrSame,Custom,-,m}
        row: 2.0 1.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "r3: an expression without a name and the symbol Unknown",
     .setup    = "@expression |Unknown|m|h\n",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>one</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="A" types="Mystery"><datapoint>1</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>two</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="B" types=""><datapoint>2</datapoint></databranch>
    </flightdata>
  </simulation>
  <simulation status="uptodate">
    <name>three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata>
      <databranch name="C" types="Later"><datapoint>3</datapoint></databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 3
  sim[0] name='one' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Mystery{Unknown,Custom,-,<U+200B>}
        row: 1.0
  sim[1] name='two' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Mystery{Unknown,Custom,-,m}
        row: 2.0
  sim[2] name='three' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'C' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
        types=Later{Unknown,Custom,-,<U+200B>}
        row: 3.0
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: simulations
});

/// A warning's text holds an angle or a speed in the default units.
class SimulationsElements : public ::testing::TestWithParam<SimulationCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(SimulationsElements, LoadAsInOpenRocketButWhereStated)
{
    expectSimulationCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, SimulationsElements, ::testing::ValuesIn(kSimulationsCases),
                         simulationCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// Two simulations of the configuration that has a motor, the second without stored results.
constexpr std::string_view kTwoSimulations = R"(
<simulations>
  <simulation status="uptodate">
    <name>First</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <flightdata maxaltitude="1.5"/>
  </simulation>
  <simulation status="notsimulated">
    <name>Second</name>
    <conditions><configid>22222222-2222-2222-2222-222222222222</configid></conditions>
  </simulation>
</simulations>
)";

// Whoever makes the context gives it the document and the preference store: without one of them
// the handler cannot be made, whatever the file holds.
TEST(SimulationsHandler, TheContextNeedsADocumentAndAPreferenceStore)
{
    const DocumentLoadingContext empty;
    EXPECT_THROW({ const SimulationsHandler handler(empty); }, BugError);

    SimulationFixture      fixture;
    DocumentLoadingContext withoutPreferences = fixture.context();
    withoutPreferences.setPreferences(nullptr);
    EXPECT_THROW({ const SimulationsHandler handler(withoutPreferences); }, BugError);

    DocumentLoadingContext withoutDocument = fixture.context();
    withoutDocument.setOpenRocketDocument(nullptr);
    EXPECT_THROW({ const SimulationsHandler handler(withoutDocument); }, BugError);

    EXPECT_NO_THROW({ const SimulationsHandler handler(fixture.context()); });
}

// The simulations of the file are the document's, in the order of the file, each made for the
// document, its rocket and the context's preference store.
TEST(SimulationsHandler, TheSimulationsAreAddedToTheDocument)
{
    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, kTwoSimulations);
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    ASSERT_EQ(fixture.document().getSimulationCount(), 2U);
    const std::shared_ptr<Simulation> first  = fixture.document().getSimulation(0);
    const std::shared_ptr<Simulation> second = fixture.document().getSimulation(1);
    EXPECT_EQ(first->getName(), "First");
    EXPECT_EQ(second->getName(), "Second");
    EXPECT_EQ(first->getDocument(), &fixture.document());
    EXPECT_EQ(&first->getRocket(), &fixture.rocket());
    EXPECT_EQ(first->getPreferences(), &fixture.preferences());
    EXPECT_EQ(second->getPreferences(), &fixture.preferences());
    EXPECT_EQ(first->getStoredStatus(), Simulation::Status::LOADED);
    EXPECT_EQ(second->getStoredStatus(), Simulation::Status::NOT_SIMULATED);
    // The configuration the second names was made in the rocket.
    EXPECT_TRUE(fixture.rocket().containsFlightConfigurationId(second->getId()));
    EXPECT_EQ(fixture.rocket().getIds().size(), 2U);
}

// The document tells its listeners of every simulation that is added (as of any simulation
// added to it: a simulation event), and a simulation that was added tells the document of its
// changes (document events with the simulation as their source).
TEST(SimulationsHandler, TheDocumentTellsOfEverySimulationAndHearsItAfterwards)
{
    SimulationFixture fixture;
    DocumentRecorder  recorder(fixture.document());

    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, kTwoSimulations);
    EXPECT_TRUE(run.result.has_value());
    // The first simulation's configuration is in the rocket. The second's is made in it when
    // the simulation is given its id, which is a change of the rocket (the document's two
    // events for it) before the simulation is added.
    EXPECT_EQ(recorder.take(), "U S(Simulation) U D(Rocket) D(Rocket) U S(Simulation)");

    ASSERT_EQ(fixture.document().getSimulationCount(), 2U);
    fixture.document().getSimulation(0)->setName("Renamed");
    EXPECT_EQ(recorder.take(), "U D(Simulation)");
    // The options came out of the conditions handler with their connection to the simulation.
    fixture.document().getSimulation(1)->getOptions().setLaunchRodLength(2.5);
    EXPECT_EQ(recorder.take(), "U D(Simulation)");
}

// Java removes the status attribute from the map it is handed; here the caller's map stays.
TEST(SimulationsHandler, TheAttributesOfTheCallerStayAsTheyAre)
{
    SimulationFixture                fixture;
    SimulationsHandler               handler(fixture.context());
    const ElementHandler::Attributes attributes{{"status", "uptodate"}, {"extra", "1"}};
    WarningSet                       warnings;
    EXPECT_TRUE(handler.closeElement("simulation", attributes, "", warnings).has_value());
    EXPECT_EQ(attributes.size(), 2U);
    EXPECT_EQ(warningTexts(warnings),
              Texts{"Unknown attributes in element 'simulation', ignoring."});

    // The status alone is nothing to warn of.
    const ElementHandler::Attributes statusOnly{{"status", "uptodate"}};
    WarningSet                       none;
    EXPECT_TRUE(handler.closeElement("simulation", statusOnly, " \n ", none).has_value());
    EXPECT_EQ(warningTexts(none), Texts{});
}

// A failure of one simulation ends the load with Java's message under the error code a loader
// maps to "Exception loading stream: ...", and the simulations read before it stay.
TEST(SimulationsHandler, AFailureOfASimulationEndsTheLoad)
{
    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, R"(
<simulations>
  <simulation status="uptodate">
    <name>One</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
  <simulation status="uptodate">
    <name>Two</name>
  </simulation>
  <simulation status="uptodate">
    <name>Three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
  </simulation>
</simulations>
)");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(run.result.error().message,
              "Attempted to set the configuration to an error id. Not Allowed!");
    EXPECT_EQ(run.texts(), Texts{"Simulation conditions not defined, using defaults."});
    ASSERT_EQ(fixture.document().getSimulationCount(), 1U);
    EXPECT_EQ(fixture.document().getSimulation(0)->getName(), "One");
}

/// A <simulations> element with @p count simulations of the configuration that has a motor,
/// each with a name, a rod length and summary flight data.
[[nodiscard]] std::string manySimulations(int count)
{
    std::string document = "<simulations>";
    for (int i = 0; i < count; i++)
    {
        document += std::format(
            "<simulation status='uptodate'><name>Simulation {}</name><conditions>"
            "<configid>11111111-1111-1111-1111-111111111111</configid>"
            "<launchrodlength>1.5</launchrodlength></conditions>"
            "<flightdata maxaltitude='{}'/></simulation>",
            i + 1, i);
    }
    return document + "</simulations>";
}

/// The seconds a load of @p document takes; -1 when it fails or does not give @p simulations
/// simulations.
[[nodiscard]] double secondsToLoad(const std::string& document, int simulations)
{
    SimulationFixture                   fixture;
    SimulationsHandler                  handler(fixture.context());
    const auto                          start = std::chrono::steady_clock::now();
    const HandlerRun                    run   = runHandler(handler, document);
    const std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;
    const bool loaded = run.result.has_value() &&
                        std::cmp_equal(fixture.document().getSimulationCount(), simulations);
    return loaded ? taken.count() : -1.0;
}

/// The seconds a load of manySimulations(@p count) takes; -1 when it fails.
[[nodiscard]] double secondsToLoad(int count)
{
    return secondsToLoad(manySimulations(count), count);
}

// A measurement, not a test: how the time of a load grows with the number of simulations of
// the file when all of them are of a flight configuration the rocket has (every simulation
// that is added is an event of the document). Linear: 2000 simulations take 0.02 s in the
// release build, in OpenRocket 0.06 s. Run it by name.
TEST(SimulationsHandler, DISABLED_PrintsTheTimeOfLoadingManySimulations)
{
    RecordProperty("seconds_for_1000", std::format("{}", secondsToLoad(1000)));
    RecordProperty("seconds_for_2000", std::format("{}", secondsToLoad(2000)));
    RecordProperty("seconds_for_4000", std::format("{}", secondsToLoad(4000)));
    SUCCEED();
}

/// A <simulations> element with @p count simulations, each of a flight configuration of its
/// own that the rocket does not have: named by an id of its own, or with @p emptyIds by an
/// empty <configid>, which is a new random id.
[[nodiscard]] std::string simulationsOfNewConfigurations(int count, bool emptyIds)
{
    std::string document = "<simulations>";
    for (int i = 0; i < count; i++)
    {
        const std::string id = emptyIds ? "" : std::format("eeeeeeee-0000-0000-0000-{:012x}", i);
        document += std::format(
            "<simulation status='uptodate'><name>s</name><conditions>"
            "<configid>{}</configid></conditions>"
            "<flightdata maxaltitude='1'/></simulation>",
            id);
    }
    return document + "</simulations>";
}

// A measurement, not a test: the time of a load in which every simulation makes a flight
// configuration (Simulation::setFlightConfigurationId() for an id the rocket lacks). Making a
// configuration costs time in proportion to the configurations the rocket has (the tree change
// it fires updates every one of them), so the load is quadratic in the number of simulations:
// 500, 1000 and 2000 simulations take 0.35, 1.5 and 6 s in the release build (the review of
// run 9b measured 8000 in 140 s). OpenRocket does the same and takes 0.7, 1.8 and 6.5 s
// (Simulation.setFlightConfigurationID(), Rocket.createFlightConfiguration() and
// updateConfigurations()). Neither has a bound on the number of configurations the simulations
// of a file may make: an open question. Run it by name; it takes minutes in a debug build.
TEST(SimulationsHandler, DISABLED_PrintsTheTimeOfLoadingSimulationsOfNewConfigurations)
{
    for (const int count : {500, 1000, 2000})
    {
        RecordProperty(
            std::format("seconds_for_{}_distinct_ids", count),
            std::format("{}", secondsToLoad(simulationsOfNewConfigurations(count, false), count)));
        RecordProperty(
            std::format("seconds_for_{}_empty_ids", count),
            std::format("{}", secondsToLoad(simulationsOfNewConfigurations(count, true), count)));
    }
    SUCCEED();
}

/// A <simulations> element with one simulation that has @p count elements, each of which
/// gives a warning of its own: `warning` for stored warnings of distinct texts in its flight
/// data, `child` for children the handler does not know, `extension` for extensions whose ids
/// no provider knows.
[[nodiscard]] std::string simulationOfManyWarnings(std::string_view kind, int count)
{
    std::string elements;
    for (int i = 0; i < count; i++)
    {
        if (kind == "warning")
        {
            elements += std::format("<warning>w{}</warning>", i);
        }
        else if (kind == "child")
        {
            elements += std::format("<u{}/>", i);
        }
        else
        {
            elements += std::format("<extension extensionid='x.E{}'/>", i);
        }
    }
    const std::string_view conditions =
        "<conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>";
    if (kind == "warning")
    {
        return std::format(
            "<simulations><simulation status='uptodate'><name>s</name>{}"
            "<flightdata>{}</flightdata></simulation></simulations>",
            conditions, elements);
    }
    return std::format(
        "<simulations><simulation status='uptodate'><name>s</name>{}{}</simulation></simulations>",
        conditions, elements);
}

// A measurement, not a test: the time of a load of one simulation whose elements give one
// warning each. A warning set looks for an equal warning in everything it holds before it adds
// one (MessageSet::add(); Java: MessageSet.add() with messages.indexOf()), so a load is
// quadratic in the number of distinct warnings, whichever handler adds them. In the release
// build 10,000, 20,000 and 40,000 elements take:
//   stored <warning> elements of distinct texts   0.9, 3.1 and 13 s    (OpenRocket: 0.9, 2.6, 16 s)
//   unknown children of the <simulation>          0.4, 1.5 and 6.3 s   (OpenRocket: 0.6, 1.6, 5.3
//   s) extensions with ids no provider knows         0.5, 1.6 and 6.8 s   (OpenRocket:
//   0.4, 1.3, 3.6 s)
// Neither has a bound on the warnings of a load: an open question for the whole loader, since
// every handler adds its warnings to the same set. Run it by name; it takes many minutes in a
// debug build.
TEST(SimulationsHandler, DISABLED_PrintsTheTimeOfLoadingManyWarnings)
{
    for (const std::string_view kind : {"warning", "child", "extension"})
    {
        for (const int count : {10000, 20000, 40000})
        {
            RecordProperty(
                std::format("seconds_for_{}_{}_elements", count, kind),
                std::format("{}", secondsToLoad(simulationOfManyWarnings(kind, count), 1)));
        }
    }
    SUCCEED();
}

// Java's rebuild of the custom expressions when a <simulation> closes dies of a
// PatternSyntaxException when a symbol of another expression of the document is no regular
// expression (the symbols are joined into one): measured in OpenRocket, the load of this
// document fails with "Unclosed group near index ...". Here the types are registered and
// nothing is parsed, so the document loads (see SimulationsHandler and CustomExpression).
TEST(SimulationsHandler, ASymbolThatIsNoRegularExpressionDoesNotFailTheLoad)
{
    SimulationFixture fixture;
    fixture.apply("@expression First|qtrFirst|m|h\n@expression Second|(|m|h\n");
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, manySimulations(2));
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_EQ(fixture.document().getSimulationCount(), 2U);
}

// The types are registered when a <simulation> closes, and at no other element: an unknown
// child of <simulations> is ignored without a call of closeElement().
TEST(SimulationsHandler, ClosingASimulationRegistersTheTypesOfTheCustomExpressions)
{
    SimulationFixture fixture;
    fixture.apply("@expression qtrClose name|qtrCloseSymbol|m|h\n");
    SimulationsHandler handler(fixture.context());
    // Another type takes the symbol, as a load of another document may leave it.
    const FlightDataType& other =
        FlightDataType::getType("qtrClose other", "qtrCloseSymbol", QtRocket::UnitGroupId::NONE);
    ASSERT_EQ(FlightDataType::findBySymbol("qtrCloseSymbol"), &other);

    EXPECT_TRUE(runHandler(handler, "<simulations><unknown/></simulations>").result.has_value());
    EXPECT_EQ(FlightDataType::findBySymbol("qtrCloseSymbol"), &other);

    WarningSet warnings;
    EXPECT_TRUE(
        handler.closeElement("simulation", {{"status", "uptodate"}}, "", warnings).has_value());
    const FlightDataType* const registered = FlightDataType::findBySymbol("qtrCloseSymbol");
    ASSERT_NE(registered, nullptr);
    EXPECT_NE(registered, &other);
    EXPECT_EQ(registered->getName(), "qtrClose name");
    EXPECT_EQ(registered->getUnitGroupId(), QtRocket::UnitGroupId::ALL_LENGTHS);

    // A second closing finds the type as it is and makes no new one.
    EXPECT_TRUE(
        handler.closeElement("simulation", {{"status", "uptodate"}}, "", warnings).has_value());
    EXPECT_EQ(FlightDataType::findBySymbol("qtrCloseSymbol"), registered);
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
