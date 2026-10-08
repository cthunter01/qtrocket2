#include "QtRocket/file/openrocket/FlightDataHandler.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/ExampleFlightData.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <flightdata> elements, run through FlightDataHandler as the loader runs them;
// their expectations are what OpenRocket makes of the same elements (the Java probe FdProbe of
// run 9b, part S2; see FlightDataTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataHandler;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::expectFlightDataCase;
using QtRocket::Test::FlightDataCase;
using QtRocket::Test::flightDataCaseTestName;
using QtRocket::Test::FlightDataFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kExampleFlightDataOutcome;
using QtRocket::Test::kExampleFlightDataSetup;
using QtRocket::Test::kExampleFlightDataXml;
using QtRocket::Test::runFlightDataCase;
using QtRocket::Test::runHandler;
using QtRocket::Test::uuidOf;

using Texts = std::vector<std::string>;

constexpr auto kFlightDataCases = std::to_array<FlightDataCase>({
    // BEGIN GENERATED: flightdata
    {.name     = "fd: summary only",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="10.5" maxvelocity="NaN" maxacceleration="Inf" maxmach="-inf" timetoapogee="abc" flighttime="1e2" groundhitvelocity=" 5 " launchrodvelocity="Infinity" bogus="1">
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {bogus=1, flighttime=1e2, groundhitvelocity= 5 , launchrodvelocity=Infinity, maxacceleration=Inf, maxaltitude=10.5, maxmach=-inf, maxvelocity=NaN, timetoapogee=abc} []
  data: branches=0 maxAlt=10.5 maxVel=NaN maxAcc=Infinity maxMach=-Infinity tApogee=NaN tFlight=100.0 vGround=5.0 vRod=Infinity vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: empty flightdata",
     .setup    = "",
     .xml      = R"xml(
<flightdata/>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: databranch without name, without types",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="10.5">
  <databranch types="time">
    <datapoint>0</datapoint>
  </databranch>
  <databranch name="x">
    <datapoint>0</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal flight data definition, ignoring.
  closed=flightdata {name=x} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: branch without datapoints is dropped, summary from attributes",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="10.5">
  <databranch name="A" types="time,altitude">
    <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {maxaltitude=10.5} []
  data: branches=0 maxAlt=10.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: optimum altitude attributes",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude" optimumAltitude="abc" timeToOptimumAltitude="Infinity">
    <datapoint>0,0</datapoint>
  </databranch>
  <databranch name="B" types="time,altitude" optimumAltitude="NaN">
    <datapoint>0,0</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=2 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=Infinity optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 0.0 0.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning after the branch that refers to it",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001"/>
  </databranch>
  <warning type="Other">
    <id>cccccccc-0000-0000-0000-000000000001</id>
    <description>Late</description>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Late' desc='Late' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: unknown element in flightdata",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="10.5">
  <bogus a="1"/>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus' encountered, ignoring.
  closed=flightdata {a=1} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: two branches, second with other types",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="999">
  <databranch name="A" types="time,altitude,velocity_total,acceleration_total">
    <datapoint>0,0,0,5</datapoint>
    <datapoint>1,10,20,50</datapoint>
    <datapoint>2,10,3,1</datapoint>
    <datapoint>3,0,4,2</datapoint>
    <event time="0.5" type="launchrod"/>
    <event time="1.5" type="recoverydevicedeployment" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="3" type="groundhit"/>
    <event time="0.2" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000003"/>
  </databranch>
  <databranch name="B" types="time" timeToOptimumAltitude="2.5">
    <datapoint>0</datapoint>
    <event time="6" type="stageseparation" source="aaaaaaaa-0000-0000-0000-000000000002"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {maxaltitude=999} []
  data: branches=2 maxAlt=10.0 maxVel=20.0 maxAcc=50.0 maxMach=NaN tApogee=1.0 tFlight=3.0 vGround=4.0 vRod=10.0 vDeploy=11.5 optDelay=NaN
    branch[0] 'A' rows=4 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s} | Total acceleration{At,Position and Motion,acceleration_total,m/s<U+00B2>}
      event LAUNCHROD t=0.5 id=(random) src=null data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=1.5 id=(random) src=P@aaaaaaaa-0000-0000-0000-000000000004 data=[null]
      event GROUND_HIT t=3.0 id=(random) src=null data=[null]
      event BURNOUT t=0.2 id=(random) src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      row: 0.0 0.0 0.0 5.0
      row: 1.0 10.0 20.0 50.0
      row: 2.0 10.0 3.0 1.0
      row: 3.0 0.0 4.0 2.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=2.5 optDelay=NaN sepTime=6.0 srcId=null
      types=Time{t,Time,time,s}
      event STAGE_SEPARATION t=6.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: summary attributes as numbers go",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="+5" maxvelocity="1e400" maxacceleration="0x10" maxmach="1.5d" timetoapogee=" 7 " flighttime="" groundhitvelocity="INF" launchrodvelocity="+Inf" deploymentvelocity="-INF" optimumdelay="nan"/>
)xml",
     .java     = R"out(
  closed=flightdata {deploymentvelocity=-INF, flighttime=, groundhitvelocity=INF, launchrodvelocity=+Inf, maxacceleration=0x10, maxaltitude=+5, maxmach=1.5d, maxvelocity=1e400, optimumdelay=nan, timetoapogee= 7 } []
  data: branches=0 maxAlt=5.0 maxVel=Infinity maxAcc=NaN maxMach=1.5 tApogee=7.0 tFlight=NaN vGround=Infinity vRod=NaN vDeploy=-Infinity optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: summary attribute names are compared exactly",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxAltitude="5" MAXVELOCITY="6" optimumDelay="7"/>
)xml",
     .java     = R"out(
  closed=flightdata {MAXVELOCITY=6, maxAltitude=5, optimumDelay=7} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: text in flightdata",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">some text</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {maxaltitude=5} [some text]
  data: branches=0 maxAlt=5.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: summary attributes are ignored with a branch",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="999" maxvelocity="999" maxacceleration="999" maxmach="999" timetoapogee="999" flighttime="999" groundhitvelocity="999" launchrodvelocity="999" deploymentvelocity="999" optimumdelay="999">
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <datapoint>1,5</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {deploymentvelocity=999, flighttime=999, groundhitvelocity=999, launchrodvelocity=999, maxacceleration=999, maxaltitude=999, maxmach=999, maxvelocity=999, optimumdelay=999, timetoapogee=999} []
  data: branches=1 maxAlt=5.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=1.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=2 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 0.0 0.0
      row: 1.0 5.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a whole flight in one branch",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="Sustainer" optimumAltitude="33.5" timeToOptimumAltitude="2.75" types="time,altitude,velocity_total,acceleration_total,mach_number">
    <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="0" type="ignition" id="bbbbbbbb-0000-0000-0000-000000000002" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="0.25" type="liftoff" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="0.5" type="launchrod" id="bbbbbbbb-0000-0000-0000-000000000004"/>
    <event time="1.25" type="burnout" id="bbbbbbbb-0000-0000-0000-000000000005" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="3" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000006" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="3.25" type="ejectioncharge" id="bbbbbbbb-0000-0000-0000-000000000007" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="3.5" type="recoverydevicedeployment" id="bbbbbbbb-0000-0000-0000-000000000008" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="5.5" type="groundhit" id="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="5.5" type="simulationend" id="bbbbbbbb-0000-0000-0000-00000000000a"/>
    <datapoint>0,0,0,0,0</datapoint>
    <datapoint>0.5,1,8,40,0.02</datapoint>
    <datapoint>1,8,20,60,0.06</datapoint>
    <datapoint>2,25,12,9.8,0.03</datapoint>
    <datapoint>3,32,1,9.8,0.003</datapoint>
    <datapoint>4,20,6,100,0.02</datapoint>
    <datapoint>5,6,5,2,0.015</datapoint>
    <datapoint>6,0,4,1,0.012</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=32.0 maxVel=20.0 maxAcc=60.0 maxMach=0.06 tApogee=3.0 tFlight=6.0 vGround=4.5 vRod=8.0 vDeploy=3.5 optDelay=1.5
    branch[0] 'Sustainer' rows=8 optAlt=33.5 tOptAlt=2.75 optDelay=1.5 sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s} | Total acceleration{At,Position and Motion,acceleration_total,m/s<U+00B2>} | Mach number{M,Characteristic Numbers,mach_number,<U+200B>}
      event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
      event IGNITION t=0.0 id=bbbbbbbb-0000-0000-0000-000000000002 src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      event LIFTOFF t=0.25 id=bbbbbbbb-0000-0000-0000-000000000003 src=null data=[null]
      event LAUNCHROD t=0.5 id=bbbbbbbb-0000-0000-0000-000000000004 src=null data=[null]
      event BURNOUT t=1.25 id=bbbbbbbb-0000-0000-0000-000000000005 src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      event APOGEE t=3.0 id=bbbbbbbb-0000-0000-0000-000000000006 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
      event EJECTION_CHARGE t=3.25 id=bbbbbbbb-0000-0000-0000-000000000007 src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=3.5 id=bbbbbbbb-0000-0000-0000-000000000008 src=P@aaaaaaaa-0000-0000-0000-000000000004 data=[null]
      event GROUND_HIT t=5.5 id=bbbbbbbb-0000-0000-0000-000000000009 src=null data=[null]
      event SIMULATION_END t=5.5 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[null]
      row: 0.0 0.0 0.0 0.0 0.0
      row: 0.5 1.0 8.0 40.0 0.02
      row: 1.0 8.0 20.0 60.0 0.06
      row: 2.0 25.0 12.0 9.8 0.03
      row: 3.0 32.0 1.0 9.8 0.003
      row: 4.0 20.0 6.0 100.0 0.02
      row: 5.0 6.0 5.0 2.0 0.015
      row: 6.0 0.0 4.0 1.0 0.012
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: optimum altitude attributes as numbers go",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time" optimumAltitude=" 5 " timeToOptimumAltitude="1e400"><datapoint>0</datapoint></databranch>
  <databranch name="B" types="time" optimumAltitude="Inf" timeToOptimumAltitude="-Infinity"><datapoint>0</datapoint></databranch>
  <databranch name="C" types="time" optimumAltitude="" timeToOptimumAltitude="0x10"><datapoint>0</datapoint></databranch>
  <databranch name="" types="time" optimumaltitude="5" timetooptimumaltitude="6"><datapoint>0</datapoint></databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=4 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=5.0 tOptAlt=Infinity optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=-Infinity optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[2] 'C' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[3] '' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: the optimum altitude attributes do not take the spellings of the flight data",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time" optimumAltitude="-Inf" timeToOptimumAltitude="Inf"><datapoint>0</datapoint></databranch>
  <databranch name="B" types="time" optimumAltitude="inf" timeToOptimumAltitude="-inf"><datapoint>0</datapoint></databranch>
  <databranch name="C" types="time" optimumAltitude="-Infinity" timeToOptimumAltitude="Infinity"><datapoint>0</datapoint></databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=3 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[2] 'C' rows=1 optAlt=-Infinity tOptAlt=Infinity optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: text in databranch",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">text<datapoint>4</datapoint>more</databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=4.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: unknown element in databranch with a child",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">
  <databranch name="A" types="time">
    <datapoint>4</datapoint>
    <bogus b="2"><datapoint>5</datapoint></bogus>
    <datapoint>6</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus' encountered, ignoring.
  closed=flightdata {name=A, types=time} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=6.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=2 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 4.0
      row: 6.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: unknown element in flightdata with children",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">
  <bogus b="2"><databranch name="A" types="time"><datapoint>5</datapoint></databranch></bogus>
  <databranch name="B" types="time"><datapoint>6</datapoint></databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus' encountered, ignoring.
  closed=flightdata {b=2} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=6.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 6.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: branches with and without rows",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">
  <databranch name="A" types="time"/>
  <databranch name="B" types="time"><datapoint>6</datapoint></databranch>
  <databranch name="C" types="time"><datapoint>x</datapoint></databranch>
  <databranch name="D" types="altitude"><datapoint>7</datapoint></databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Data point format error, ignoring point.
  closed=flightdata {maxaltitude=5} []
  data: branches=2 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=6.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 6.0
    branch[1] 'D' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Altitude{h,Position and Motion,altitude,m}
      row: 7.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a warning between two branches",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
  <warning type="Other"><id>cccccccc-0000-0000-0000-000000000003</id><description>between</description></warning>
  <databranch name="B" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=2 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='between' desc='between' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[Other:between]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a warning element inside a branch",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <warning type="Other"><id>cccccccc-0000-0000-0000-000000000003</id><description>inside</description></warning>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'warning' encountered, ignoring.
  closed=flightdata {name=A, types=time} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: databranch attributes that are no numbers do not fail",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time" optimumAltitude="x" timeToOptimumAltitude="y" bogus="z"><datapoint>1</datapoint></databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=1.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 1.0
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: flightdata
});

/// A warning's text holds an angle or a speed in the default units.
class FlightDataElements : public ::testing::TestWithParam<FlightDataCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(FlightDataElements, LoadAsInOpenRocketButWhereStated)
{
    expectFlightDataCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, FlightDataElements, ::testing::ValuesIn(kFlightDataCases),
                         flightDataCaseTestName);

// ---- the stored data of an example ----------------------------------------------------------

/// @p lines joined, each ended with a line end.
[[nodiscard]] std::string joined(std::span<const std::string_view> lines)
{
    std::string text;
    for (const std::string_view line : lines)
    {
        text += line;
        text += '\n';
    }
    return text;
}

/// The element of ExampleFlightData.h, read by a new handler in @p fixture, whose rocket has
/// the components the element names.
[[nodiscard]] std::shared_ptr<FlightData> loadExample(FlightDataFixture& fixture)
{
    fixture.apply(joined(kExampleFlightDataSetup));
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run = runHandler(handler, joined(kExampleFlightDataXml));
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    return handler.getFlightData();
}

// The stored results of "Simulation 3 - too short delay" of the example "Clustered motors.ork":
// every summary value, the warning, the events and a digest of each of the 58 columns over the
// 337 rows, as OpenRocket loads them.
TEST(FlightDataHandlerExample, TheStoredDataOfAnExampleLoadAsInOpenRocket)
{
    const DefaultUnitsGuard units;
    EXPECT_EQ(runFlightDataCase(joined(kExampleFlightDataSetup), joined(kExampleFlightDataXml)),
              "\n" + joined(kExampleFlightDataOutcome));
}

// The same numbers as the scout of tier 9 printed them for the simulation when OpenRocket loaded
// the whole file (ExamplesProbe.out, "Clustered motors.ork", sim[2]). The file's own summary
// attributes are rounded to three places and differ in the two that are interpolated or
// computed: deploymentvelocity="27.549" and optimumdelay="5.573".
TEST(FlightDataHandlerExample, TheSummaryIsComputedFromTheStoredBranch)
{
    const DefaultUnitsGuard           units;
    FlightDataFixture                 fixture;
    const std::shared_ptr<FlightData> data = loadExample(fixture);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getMaxAltitude(), 279.136);
    EXPECT_EQ(data->getMaxVelocity(), 98.494);
    EXPECT_EQ(data->getMaxAcceleration(), 208.827);
    EXPECT_EQ(data->getMaxMachNumber(), 0.29);
    EXPECT_EQ(data->getTimeToApogee(), 5.637);
    EXPECT_EQ(data->getFlightTime(), 52.959);
    EXPECT_EQ(data->getGroundHitVelocity(), 6.207);
    EXPECT_EQ(data->getLaunchRodVelocity(), 18.601);
    EXPECT_EQ(data->getDeploymentVelocity(), 27.55133333333334);
    EXPECT_EQ(data->getOptimumDelay(), 5.573144531249992);

    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightDataBranch& branch = data->getBranch(0);
    EXPECT_EQ(branch.getName(), "Sustainer");
    EXPECT_EQ(branch.getLength(), 337U);
    EXPECT_EQ(branch.getTypes().size(), 58U);
    EXPECT_EQ(branch.getOptimumAltitude(), 307.48948488047273);
    EXPECT_EQ(branch.getTimeToOptimumAltitude(), 7.433144531249992);
    EXPECT_EQ(branch.getOptimumDelay(), 5.573144531249992);
    EXPECT_TRUE(std::isnan(branch.getSeparationTime()));
    EXPECT_FALSE(branch.getSourceComponentId().has_value());
    EXPECT_EQ(branch.getEvents().size(), 11U);

    ASSERT_EQ(data->getWarningSet().size(), 1U);
    const Warning& warning = *data->getWarningSet().begin();
    EXPECT_EQ(warning.typeName(), "RecoveryHighSpeedDeployment");
    EXPECT_EQ(warning.id().toString(), "76a35ffa-fd95-40a7-9c8e-bcc900bbdb48");
    EXPECT_EQ(warning.toString(),
              "Recovery device deployment at high speed (27.5 m/s):  \"Parachute\"");
    EXPECT_EQ(warning.messageDescription(), "Recovery device deployment at high speed (27.5 m/s)");
}

/// The number of types of @p branch that are built-in types.
[[nodiscard]] std::size_t builtinTypeCount(const FlightDataBranch& branch)
{
    std::size_t count = 0;
    for (const FlightDataType* type : branch.getTypes())
    {
        count += type->isBuiltin() ? 1U : 0U;
    }
    return count;
}

/// The number of events of @p branch that have a source, and of those the number that point
/// to a component.
struct SourceCounts
{
    std::size_t withSource{0};
    std::size_t withPointer{0};
};

[[nodiscard]] SourceCounts countSources(const FlightDataBranch& branch)
{
    SourceCounts counts;
    for (const FlightEvent& event : branch.getEvents())
    {
        counts.withSource += event.hasSource() ? 1U : 0U;
        counts.withPointer += event.getSource() != nullptr ? 1U : 0U;
    }
    return counts;
}

// The file names its columns by the display names of OpenRocket 24.12, eleven of which have
// since changed ("Drag coefficient", "Roll rate", ...): all 58 are built-in types.
TEST(FlightDataHandlerExample, TheColumnsAreBuiltInTypesAndTheEventsNameComponentsById)
{
    const DefaultUnitsGuard           units;
    FlightDataFixture                 fixture;
    const std::shared_ptr<FlightData> data = loadExample(fixture);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightDataBranch& branch = data->getBranch(0);
    EXPECT_EQ(builtinTypeCount(branch), 58U);
    EXPECT_TRUE(branch.hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_DRAG_COEFF)));
    EXPECT_TRUE(branch.hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_ROLL_RATE)));
    EXPECT_FALSE(branch.hasType(FlightDataType::builtin(FlightDataTypeId::TYPE_CNA)));

    // Six of the eleven events have a source; none points into the document's rocket, and the
    // flight data co-own no rocket.
    const SourceCounts sources = countSources(branch);
    EXPECT_EQ(sources.withSource, 6U);
    EXPECT_EQ(sources.withPointer, 0U);
    EXPECT_EQ(data->getSimulatedRocket(), nullptr);

    // The SIM_WARN event finds its warning.
    const FlightEvent* const warn = branch.getFirstEvent(FlightEvent::Type::SIM_WARN);
    ASSERT_NE(warn, nullptr);
    EXPECT_EQ(warn->getTime(), 4.863);
    const Warning* const warning = data->findWarning(*warn);
    ASSERT_NE(warning, nullptr);
    EXPECT_EQ(warning->id().toString(), "76a35ffa-fd95-40a7-9c8e-bcc900bbdb48");
}

// An optional measurement: the seconds one handler after the other takes to read the element of
// the example a hundred times, 33,700 rows of 58 numbers (the 16 examples hold 36,962 rows).
TEST(FlightDataHandlerExample, DISABLED_PrintsTheTimeOfReadingTheExampleAHundredTimes)
{
    const std::string xml = joined(kExampleFlightDataXml);
    FlightDataFixture fixture;
    fixture.apply(joined(kExampleFlightDataSetup));
    std::size_t rows  = 0;
    const auto  start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++)
    {
        FlightDataHandler handler(fixture.context());
        const HandlerRun  run = runHandler(handler, xml);
        rows += run.result.has_value() ? handler.getFlightData()->getBranch(0).getLength() : 0U;
    }
    const std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;
    RecordProperty("seconds_for_33700_rows", std::format("{}", taken.count()));
    EXPECT_EQ(rows, 33700U);
}

// ---- the handler by itself ------------------------------------------------------------------

TEST(FlightDataHandler, TheContextNeedsADocument)
{
    const DocumentLoadingContext context;
    EXPECT_THROW({ const FlightDataHandler handler(context); }, BugError);
}

TEST(FlightDataHandler, TheFlightDataAreThereWhenTheElementHasClosed)
{
    FlightDataFixture fixture;
    FlightDataHandler handler(fixture.context());
    EXPECT_EQ(handler.getFlightData(), nullptr);
    EXPECT_TRUE(handler.getWarningSet().empty());

    const HandlerRun run = runHandler(
        handler, "<flightdata maxaltitude='10.5'><warning>stored</warning></flightdata>");
    EXPECT_TRUE(run.result.has_value());
    const std::shared_ptr<FlightData>& data = handler.getFlightData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getMaxAltitude(), 10.5);
    EXPECT_EQ(data->getBranchCount(), 0U);
    EXPECT_EQ(QtRocket::Test::warningTexts(data->getWarningSet()), Texts{"stored"});
    EXPECT_EQ(QtRocket::Test::warningTexts(handler.getWarningSet()), Texts{"stored"});
    // The same object every time.
    EXPECT_EQ(handler.getFlightData(), data);
}

TEST(FlightDataHandler, TheFlightDataAndTheirBranchesAreImmutable)
{
    FlightDataFixture fixture;
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run = runHandler(
        handler,
        "<flightdata><warning>stored</warning>"
        "<databranch name='A' types='time'><datapoint>1</datapoint></databranch></flightdata>");
    EXPECT_TRUE(run.result.has_value());
    const std::shared_ptr<FlightData>& data = handler.getFlightData();
    ASSERT_NE(data, nullptr);
    EXPECT_FALSE(data->isMutable());
    EXPECT_FALSE(data->getWarningSet().isMutable());
    ASSERT_EQ(data->getBranchCount(), 1U);
    EXPECT_FALSE(data->getBranch(0).isMutable());
    EXPECT_THROW(data->getWarningSet().add("another"), BugError);
    EXPECT_THROW(data->getBranch(0).addPoint(), BugError);
}

// The warnings of the flight data are copies of the ones the handler collected, with their ids.
TEST(FlightDataHandler, TheWarningsOfTheFlightDataKeepTheirIds)
{
    FlightDataFixture fixture;
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run =
        runHandler(handler,
                   "<flightdata>"
                   "<warning><id>cccccccc-0000-0000-0000-000000000001</id>first</warning>"
                   "<warning>second</warning></flightdata>");
    EXPECT_TRUE(run.result.has_value());
    const std::shared_ptr<FlightData>& data = handler.getFlightData();
    ASSERT_NE(data, nullptr);
    const Uuid           id     = uuidOf("cccccccc-0000-0000-0000-000000000001");
    const Warning* const loaded = data->getWarningSet().findById(id);
    const Warning* const read   = handler.getWarningSet().findById(id);
    ASSERT_NE(loaded, nullptr);
    ASSERT_NE(read, nullptr);
    EXPECT_NE(loaded, read);
    EXPECT_EQ(loaded->toString(), "first");
    EXPECT_TRUE(data->getWarningSet() == handler.getWarningSet());
}

// A failure of a child ends the reading, and the handler has made no flight data.
TEST(FlightDataHandler, AFailedBranchLeavesNoFlightData)
{
    FlightDataFixture fixture;
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run = runHandler(
        handler,
        "<flightdata maxaltitude='5'><databranch name='A' types='time,time'/></flightdata>");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().message, "Value type Time already exists.");
    EXPECT_EQ(handler.getFlightData(), nullptr);
}

// What the simulation's handler does with the flight data: they go into the Simulation as they
// are.
TEST(FlightDataHandler, ASimulationTakesTheFlightData)
{
    FlightDataFixture fixture;
    FlightDataHandler handler(fixture.context());
    const HandlerRun  run = runHandler(
        handler,
        "<flightdata><warning>stored</warning>"
        "<databranch name='A' types='time,altitude'>"
        "<datapoint>0,0</datapoint><datapoint>1,5</datapoint></databranch></flightdata>");
    EXPECT_TRUE(run.result.has_value());
    ASSERT_NE(handler.getFlightData(), nullptr);

    const Simulation simulation(&fixture.document(), fixture.rocket(), Simulation::Status::LOADED,
                                "Loaded", SimulationOptions(fixture.preferences()), {},
                                handler.getFlightData(), {}, &fixture.preferences());
    EXPECT_EQ(simulation.getSimulatedData(), handler.getFlightData());
    EXPECT_TRUE(simulation.hasSimulationData());
    ASSERT_NE(simulation.getSimulatedWarnings(), nullptr);
    EXPECT_EQ(QtRocket::Test::warningTexts(*simulation.getSimulatedWarnings()), Texts{"stored"});
}

}  // namespace
