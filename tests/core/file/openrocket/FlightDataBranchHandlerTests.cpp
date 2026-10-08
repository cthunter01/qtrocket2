#include "QtRocket/file/openrocket/FlightDataBranchHandler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <flightdata> elements with <databranch> elements, run through FlightDataHandler
// as the loader runs them; their expectations are what OpenRocket makes of the same elements
// (the Java probe FdProbe of run 9b, part S2; see FlightDataTestSupport.h), but where a case
// states that QtRocket differs.
//
// The tests share the process-wide registry of flight data types with every other test of
// their process (see FlightDataBranchHandler): the unknown names and the symbols of the custom
// expressions here start with "qtrFd", and nothing compares a type that is not built in by
// identity with one of another load.

namespace
{

using QtRocket::BugError;
using QtRocket::CustomExpression;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataBranchHandler;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeGroup;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::SimulationAbort;
using QtRocket::UnitGroupId;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::ascii;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::describeFlightDataType;
using QtRocket::Test::expectFlightDataCase;
using QtRocket::Test::FlightDataCase;
using QtRocket::Test::flightDataCaseTestName;
using QtRocket::Test::FlightDataFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kFlightTubeId;
using QtRocket::Test::runHandler;
using QtRocket::Test::uuidOf;

using Texts = std::vector<std::string>;

constexpr auto kBranchCases = std::to_array<FlightDataCase>({
    // BEGIN GENERATED: branch
    {.name     = "fd: duplicate type",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,Time">
    <datapoint>0,0</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Time already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: same type by key and legacy name",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="drag_coeff,Drag coefficient">
    <datapoint>0,0</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Drag coefficient (CD) already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: empty types attribute",
     .setup    = "@unknown Registered before\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="">
    <datapoint></datapoint>
    <datapoint>5</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Data point format error, ignoring point.
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Registered before{Unknown,Custom,-,<U+200B>}
      row: 5.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: type names of every generation, unknown names, padded names",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,Altitude,Drag coefficient,Drag coefficient (CD),Position upwind,Roll rate,thrust_correction,Custom thing, Mach number,Another thing">
    <datapoint>0,1,2,3,4,5,6,7,8,9</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Drag coefficient (CD) already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: datapoints",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude,velocity_total">
    <datapoint>0,0</datapoint>
    <datapoint>0,0,0,0</datapoint>
    <datapoint>1,abc,2</datapoint>
    <datapoint> 1 , 2 , 3 </datapoint>
    <datapoint>2,NaN,Inf</datapoint>
    <datapoint>3,nan,-inf</datapoint>
    <datapoint>4,Infinity,-Infinity</datapoint>
    <datapoint>5,1e400,0x10</datapoint>
    <datapoint>6,7,8,</datapoint>
    <datapoint>6,,8</datapoint>
    <bogus>1</bogus>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Data point did not contain correct amount of values, ignoring point.
  W[Other,NORMAL] Data point format error, ignoring point.
  W[Other,NORMAL] Unknown element 'bogus' encountered, ignoring.
  closed=flightdata {name=A, types=time,altitude,velocity_total} []
  data: branches=1 maxAlt=Infinity maxVel=Infinity maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=6.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=5 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
      row: 1.0 2.0 3.0
      row: 2.0 NaN Infinity
      row: 3.0 NaN -Infinity
      row: 4.0 Infinity -Infinity
      row: 6.0 7.0 8.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: events",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="Other">
    <id>cccccccc-0000-0000-0000-000000000001</id>
    <description>A stored warning</description>
    <priority>LOW</priority>
  </warning>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="0" type="launch"/>
    <event time="NaN" type="launch" id="bbbbbbbb-0000-0000-0000-000000000002"/>
    <event time="abc" type="launch"/>
    <event type="launch"/>
    <event time="1" type="bogus"/>
    <event time="1"/>
    <event time="Inf" type="apogee"/>
    <event time="-1" type="apogee"/>
    <event time="2" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="2" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="2" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="2" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="2" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="2" type="recoverydevicedeployment" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="2" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="3" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001"/>
    <event time="3" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000099"/>
    <event time="3" type="simwarn"/>
    <event time="3" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="3" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="4" type="simabort" cause="stageseparation"/>
    <event time="4" type="simabort"/>
    <event time="4" type="simabort" cause="bogus"/>
    <event time="4" type="apogee" cause="nomotorsdefined"/>
    <event time="4" type="ignition" cause="nomotorsdefined"/>
    <event time="4" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" cause="nomotorsdefined"/>
    <event time="5" type="altitude"/>
    <event time="5" type="exception"/>
    <event time="5" type="tumble"/>
    <event time="6" type="stageseparation" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="7" type=" groundhit "/>
    <event time="7" type="GroundHit"/>
    <event time="7" type="ground_hit"/>
    <event time="8" type="simulationend">text</event>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: LAUNCH event has a NaN time!
  W[Other,NORMAL] Illegal event time specification, ignoring: For input string: "abc"
  W[Other,NORMAL] Illegal event time specification, ignoring: null string
  W[Other,NORMAL] Illegal event specification, ignoring.
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITION events should have MotorMount type data payloads, instead of Parachute
  W[Other,NORMAL] Illegal parameters for FlightEvent: BURNOUT events should have MotorMount type data payloads, instead of Parachute
  W[Other,NORMAL] Illegal parameters for FlightEvent: EJECTION_CHARGE events should have AxialStage type data payloads, instead of BodyTube
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was B
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was <i>Component Removed From Rocket</i>
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_ABORT events require SimulationAbort objects
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITIONevents should have MotorClusterState type data payloads
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,LOW] id=cccccccc-0000-0000-0000-000000000001 text='A stored warning' desc='A stored warning' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=6.0 srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event LAUNCH t=0.0 id=(random) src=null data=[null]
      event APOGEE t=Infinity id=(random) src=null data=[null]
      event APOGEE t=-1.0 id=(random) src=null data=[null]
      event IGNITION t=2.0 id=(random) src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      event EJECTION_CHARGE t=2.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=2.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event IGNITION t=2.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event SIM_WARN t=3.0 id=(random) src=null data=[Other:A stored warning]
      event APOGEE t=4.0 id=(random) src=null data=[SimulationAbort:No motors defined in the simulation]
      event ALTITUDE t=5.0 id=(random) src=null data=[null]
      event EXCEPTION t=5.0 id=(random) src=null data=[null]
      event TUMBLE t=5.0 id=(random) src=null data=[null]
      event STAGE_SEPARATION t=6.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event GROUND_HIT t=7.0 id=(random) src=null data=[null]
      event SIMULATION_END t=8.0 id=(random) src=null data=[null]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: event with garbage id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="0" type="launch" id="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: event with garbage source",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="0" type="launch" source="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: event with garbage warnid",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="0" type="simwarn" warnid="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: event with lenient uuid",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="0" type="launch" id="1-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event LAUNCH t=0.0 id=00000001-0002-0003-0004-000000000005 src=null data=[null]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: type names of every generation, unknown names, padded names",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,Altitude,Drag coefficient,Position upwind,Roll rate,thrust_correction,Custom thing, Mach number,Another thing,TYPE_DRAG_COEFF,Pitch rate ,Normal force coefficient (CN),altitude above sea level">
    <datapoint>0,1,2,3,4,5,6,7,8,9,10,11,12</datapoint>
  </databranch>
  <databranch name="B" types="Custom thing,custom THING2">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=2 maxAlt=1.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Position North of launch{Py,Position and Motion,position_y,m} | Roll rate (Z){d<U+03A6>,Orientation,roll_rate,rad/s} | Thrust Pressure Correction{Fta,Thrust and Drag,thrust_correction,N} | Drag coefficient (CD){Cd,Thrust and Drag,drag_coeff,<U+200B>} | Normal force coefficient (CN){Cn,Coefficients,normal_force_coeff,<U+200B>} | Custom thing{Unknown,Custom,-,<U+200B>} |  Mach number{Unknown,Custom,-,<U+200B>} | Another thing{Unknown,Custom,-,<U+200B>} | TYPE_DRAG_COEFF{Unknown,Custom,-,<U+200B>} | Pitch rate {Unknown,Custom,-,<U+200B>} | altitude above sea level{Unknown,Custom,-,<U+200B>}
      row: 0.0 1.0 3.0 4.0 5.0 2.0 11.0 6.0 7.0 8.0 9.0 10.0 12.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Custom thing{Unknown,Custom,-,<U+200B>} | custom THING2{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: two unknown names equal ignoring case",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="B" types="Custom thing,custom THING">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type custom THING already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: event after landing warning with its event",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding">
    <id>cccccccc-0000-0000-0000-000000000003</id>
    <description>Flight Event occurred after landing: Apogee</description>
    <priority>HIGH</priority>
  </warning>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000009"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,HIGH] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Apogee' desc='Flight Event occurred after landing: Apogee' sources= event=Apogee@bbbbbbbb-0000-0000-0000-000000000009
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000009 src=null data=[null]
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: Apogee]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: event after landing whose event comes later, and garbage eventid",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding">
    <id>cccccccc-0000-0000-0000-000000000003</id>
  </warning>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="3" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000009"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      event APOGEE t=3.0 id=bbbbbbbb-0000-0000-0000-000000000009 src=null data=[null]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: garbage eventid",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding">
    <id>cccccccc-0000-0000-0000-000000000003</id>
  </warning>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="zzz"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: zzz]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: abort causes",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="4" type="simabort" cause="nomotorsdefined"/>
    <event time="5" type="simabort" cause=" tumbleunderthrust "/>
    <event time="6" type="simabort" cause="NoLiftoff"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_ABORT events require SimulationAbort objects
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event SIM_ABORT t=4.0 id=(random) src=null data=[SimulationAbort:No motors defined in the simulation]
      event SIM_ABORT t=5.0 id=(random) src=null data=[SimulationAbort:Stage began to tumble under thrust.]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: optimum delay from the last burnout",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time" timeToOptimumAltitude="4.5">
    <datapoint>0</datapoint>
    <event time="1.25" type="burnout"/>
    <event time="2.25" type="burnout"/>
    <event time="0.75" type="burnout"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=4.5 optDelay=3.75 sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event BURNOUT t=1.25 id=(random) src=null data=[null]
      event BURNOUT t=2.25 id=(random) src=null data=[null]
      event BURNOUT t=0.75 id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: types with a trailing comma",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude,,">
    <datapoint>1,2</datapoint>
    <datapoint>3,4,</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=4.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=3.0 tFlight=3.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=2 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 1.0 2.0
      row: 3.0 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: types that are only commas",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types=",,">
    <datapoint>1</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Must specify at least one data type.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: types with an empty name in front",
     .setup    = "@unknown Registered before\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types=",time">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Registered before{Unknown,Custom,-,<U+200B>}
      row: 2.0 1.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: empty types attribute takes the name of the registered unknown type",
     .setup    = "@unknown Registered before\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="">
    <datapoint>5</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Registered before{Unknown,Custom,-,<U+200B>}
      row: 5.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: type names are not trimmed",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time, altitude,altitude ,ALTITUDE2">
    <datapoint>1,2,3,4</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=1.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} |  altitude{Unknown,Custom,-,<U+200B>} | altitude {Unknown,Custom,-,<U+200B>} | ALTITUDE2{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0 3.0 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a save key in another case is the type again",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,TIME">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type TIME already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a save key and the name of its type",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="altitude,Altitude">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Altitude already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: the upwind name and the save key of its type",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Position upwind,position_y">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Position North of launch already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an unknown name twice",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Mystery,Mystery">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type Mystery already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two names that are equal ignoring case and hash apart",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd &#181;,qtrFd &#956;">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=qtrFd <U+00B5>{Unknown,Custom,-,<U+200B>} | qtrFd <U+03BC>{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0
)out",
     .qtrocket = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd <U+03BC> already exists.]
  closed=(not closed)
  data: null
)out",
     .why =
         "The micro sign and the Greek letter mu are equal ignoring case, so the two names are one "
         "type twice. OpenRocket asks a HashMap, and its hash of a name (the name in lower case) "
         "differs for the two, so there they are two columns (see FlightDataType::hashCode())."},
    {.name     = "s2: two names that are equal ignoring case, with a sharp s",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd stra&#223;e,qtrFd STRASSE,qtrFd STRA&#7838;E">
    <datapoint>1,2,3</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd STRA<U+1E9E>E already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two names with a dotted capital i",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd &#304;x,qtrFd ix,qtrFd Ix,qtrFd &#305;x">
    <datapoint>1,2,3,4</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd Ix already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd ix already exists.]
  closed=(not closed)
  data: null
)out",
     .why = "The name with the dotted capital I and the name with the small i are equal ignoring "
            "case, so the second name is the first type again. OpenRocket's hash of the first (the "
            "name in lower case, where that letter becomes an i and a combining dot) differs from "
            "the second's, so its HashMap takes both and refuses the third name (see "
            "FlightDataType::hashCode())."},
    {.name     = "s2: the ten legacy names",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Drag coefficient,Axial drag coefficient,Friction drag coefficient,Pressure drag coefficient,Base drag coefficient,Normal force coefficient,Pitch moment coefficient,Roll rate,Pitch rate,Yaw rate">
    <datapoint>1,2,3,4,5,6,7,8,9,10</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Roll rate (Z){d<U+03A6>,Orientation,roll_rate,rad/s} | Pitch rate (Y){d<U+03B8>,Orientation,pitch_rate,rad/s} | Yaw rate (X){d<U+03A8>,Orientation,yaw_rate,rad/s} | Drag coefficient (CD){Cd,Thrust and Drag,drag_coeff,<U+200B>} | Friction drag coefficient (CD_friction){Cdf,Thrust and Drag,friction_drag_coeff,<U+200B>} | Pressure drag coefficient (CD_pressure){Cdp,Thrust and Drag,pressure_drag_coeff,<U+200B>} | Base drag coefficient (CD_base){Cdb,Thrust and Drag,base_drag_coeff,<U+200B>} | Axial drag coefficient (CA){Cda,Thrust and Drag,axial_drag_coeff,<U+200B>} | Normal force coefficient (CN){Cn,Coefficients,normal_force_coeff,<U+200B>} | Pitch moment coefficient (Cm){C<U+03B8>,Coefficients,pitch_moment_coeff,<U+200B>}
      row: 8.0 9.0 10.0 1.0 3.0 4.0 5.0 2.0 6.0 7.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: custom expressions of the document",
     .setup    = "@expression Kinetic energy|ke|J|0.5*m*Vt^2\n"
                 "@expression Furlong speed|fs|furlongs/fortnight|Vt*1.2\n"
                 "@expression Plain number|pn||m\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,Kinetic energy,Furlong speed,Plain number">
    <datapoint>1,2,3,4</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=1.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Kinetic energy{ke,Custom,-,J} | Furlong speed{fs,Custom,-,furlongs/fortnight} | Plain number{pn,Custom,-,}
      row: 1.0 2.0 3.0 4.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: the name of a custom expression is compared exactly",
     .setup    = "@expression Kinetic energy|ke|J|0.5*m*Vt^2\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="kinetic energy">
    <datapoint>1</datapoint>
  </databranch>
  <databranch name="B" types="Kinetic energy ">
    <datapoint>2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=2 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=kinetic energy{Unknown,Custom,-,<U+200B>}
      row: 1.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Kinetic energy {Unknown,Custom,-,<U+200B>}
      row: 2.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a custom expression and an unknown name that equals it",
     .setup    = "@expression Kinetic energy|ke|J|0.5*m*Vt^2\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Kinetic energy,KINETIC ENERGY">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type KINETIC ENERGY already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a custom expression without a name",
     .setup    = "@unknown Registered before\n"
                 "@expression |qtrNoName|m|h\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="">
    <datapoint>1</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types={qtrNoName,Custom,-,m}
      row: 1.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a custom expression with the symbol of the unknown types",
     .setup    = "@expression Odd one|Unknown|m|h\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Odd one,Mystery">
    <datapoint>1,2</datapoint>
  </databranch>
  <databranch name="B" types="Odd one">
    <datapoint>3</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=2 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Odd one{Unknown,Custom,-,m} | Mystery{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Odd one{Unknown,Custom,-,m}
      row: 3.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a custom expression named as a built-in type is not asked",
     .setup    = "@expression Altitude|qtrAlt|m|h*2\n"
                 "@expression altitude|qtrAlt2|m|h*3\n"
                 "@expression Drag coefficient|qtrCd||Cd*2\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Altitude,Drag coefficient">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=1.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Altitude{h,Position and Motion,altitude,m} | Drag coefficient (CD){Cd,Thrust and Drag,drag_coeff,<U+200B>}
      row: 1.0 2.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: the first of two custom expressions of one name",
     .setup    = "@expression Twin|qtrTwinA|m/s|Vt\n"
                 "@expression Twin|qtrTwinB|m|h\n",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="Twin">
    <datapoint>1</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Twin{qtrTwinA,Custom,-,m/s}
      row: 1.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: datapoint numbers",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>1e2,+5</datapoint>
    <datapoint>1.5d,0x1p3</datapoint>
    <datapoint> 7 ,&#9;8&#10;</datapoint>
    <datapoint>inf,-INF</datapoint>
    <datapoint>NAN,nan</datapoint>
    <datapoint>Infinity,-Infinity</datapoint>
    <datapoint>-0,-0.0</datapoint>
    <datapoint>1e400,-1e400</datapoint>
    <datapoint>1e-310,1e-400</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=8.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=1.5 tFlight=1.0E-310 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=9 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 100.0 5.0
      row: 1.5 8.0
      row: 7.0 8.0
      row: Infinity -Infinity
      row: NaN NaN
      row: Infinity -Infinity
      row: -0.0 -0.0
      row: Infinity -Infinity
      row: 1.0E-310 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: datapoints that are no numbers",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <datapoint>1,+Inf</datapoint>
    <datapoint>1,NaN </datapoint>
    <datapoint>1, Inf</datapoint>
    <datapoint>1,1.2.3</datapoint>
    <datapoint>1,</datapoint>
    <datapoint>,1</datapoint>
    <datapoint>1;2</datapoint>
    <datapoint>,,</datapoint>
    <datapoint></datapoint>
    <datapoint/>
    <datapoint>9,9</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Data point format error, ignoring point.
  W[Other,NORMAL] Data point did not contain correct amount of values, ignoring point.
  closed=flightdata {} []
  data: branches=1 maxAlt=9.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=9.0 tFlight=9.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=2 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 1.0 NaN
      row: 9.0 9.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: datapoint with a child element",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">
  <databranch name="A" types="time,altitude">
    <datapoint>1,2<x a="1"/>3</datapoint>
    <datapoint>4,5</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  W[Other,NORMAL] Data point did not contain correct amount of values, ignoring point.
  closed=flightdata {name=A, types=time,altitude} []
  data: branches=1 maxAlt=5.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=4.0 tFlight=4.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      row: 4.0 5.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: datapoint with attributes",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint a="1">4</datapoint>
  </databranch>
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
    {.name     = "s2: event times",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time=" 1 " type="apogee"/>
    <event time="Inf" type="apogee"/>
    <event time="-inf" type="apogee"/>
    <event time="0x10" type="apogee"/>
    <event time="1e400" type="apogee"/>
    <event time="nan" type="apogee"/>
    <event time="" type="apogee"/>
    <event time="1.2.3" type="apogee"/>
    <event time="+Inf" type="apogee"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal event time specification, ignoring: For input string: "0x10"
  W[Other,NORMAL] Illegal parameters for FlightEvent: APOGEE event has a NaN time!
  W[Other,NORMAL] Illegal event time specification, ignoring: empty String
  W[Other,NORMAL] Illegal event time specification, ignoring: multiple points
  W[Other,NORMAL] Illegal event time specification, ignoring: For input string: "+Inf"
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=(random) src=null data=[null]
      event APOGEE t=Infinity id=(random) src=null data=[null]
      event APOGEE t=-Infinity id=(random) src=null data=[null]
      event APOGEE t=Infinity id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event types",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch"/>
    <event time="1" type="liftoff"/>
    <event time="1" type="launchrod"/>
    <event time="1" type="apogee"/>
    <event time="1" type="groundhit"/>
    <event time="1" type="simulationend"/>
    <event time="1" type="altitude"/>
    <event time="1" type="tumble"/>
    <event time="1" type="exception"/>
    <event time="1" type="stageseparation"/>
    <event time="1" type="recoverydevicedeployment"/>
    <event time="1" type=" tumble "/>
    <event time="1" type="TUMBLE"/>
    <event time="1" type="sim_warn"/>
    <event time="1" type=""/>
    <event time="1" type="launch rod"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal event specification, ignoring.
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=1.0 srcId=null
      types=Time{t,Time,time,s}
      event LAUNCH t=1.0 id=(random) src=null data=[null]
      event LIFTOFF t=1.0 id=(random) src=null data=[null]
      event LAUNCHROD t=1.0 id=(random) src=null data=[null]
      event APOGEE t=1.0 id=(random) src=null data=[null]
      event GROUND_HIT t=1.0 id=(random) src=null data=[null]
      event SIMULATION_END t=1.0 id=(random) src=null data=[null]
      event ALTITUDE t=1.0 id=(random) src=null data=[null]
      event TUMBLE t=1.0 id=(random) src=null data=[null]
      event EXCEPTION t=1.0 id=(random) src=null data=[null]
      event STAGE_SEPARATION t=1.0 id=(random) src=null data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=1.0 id=(random) src=null data=[null]
      event TUMBLE t=1.0 id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: every validation failure of an event",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="Other"><id>cccccccc-0000-0000-0000-000000000001</id><description>Stored</description></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="NaN" type="launch"/>
    <event time="NaN" type="simwarn" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="NaN" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="1" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="1" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="1" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="1" type="ignition" cause="nomotorsdefined"/>
    <event time="1" type="burnout" cause="nomotorsdefined"/>
    <event time="1" type="ejectioncharge" cause="nomotorsdefined"/>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000004" cause="nomotorsdefined"/>
    <event time="1" type="simwarn"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000099"/>
    <event time="1" type="simwarn" cause="nomotorsdefined"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" cause="nomotorsdefined"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="simwarn" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="1" type="simabort"/>
    <event time="1" type="simabort" cause="bogus"/>
    <event time="1" type="simabort" cause=""/>
    <event time="1" type="simabort" warnid="cccccccc-0000-0000-0000-000000000001"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: LAUNCH event has a NaN time!
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event has a NaN time!
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITION event has a NaN time!
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITION events should have MotorMount type data payloads, instead of Parachute
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITION events should have MotorMount type data payloads, instead of AxialStage
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITION events should have MotorMount type data payloads, instead of Rocket
  W[Other,NORMAL] Illegal parameters for FlightEvent: BURNOUT events should have MotorMount type data payloads, instead of Parachute
  W[Other,NORMAL] Illegal parameters for FlightEvent: BURNOUT events should have MotorMount type data payloads, instead of Rocket
  W[Other,NORMAL] Illegal parameters for FlightEvent: EJECTION_CHARGE events should have AxialStage type data payloads, instead of BodyTube
  W[Other,NORMAL] Illegal parameters for FlightEvent: EJECTION_CHARGE events should have AxialStage type data payloads, instead of Parachute
  W[Other,NORMAL] Illegal parameters for FlightEvent: EJECTION_CHARGE events should have AxialStage type data payloads, instead of Rocket
  W[Other,NORMAL] Illegal parameters for FlightEvent: IGNITIONevents should have MotorClusterState type data payloads
  W[Other,NORMAL] Illegal parameters for FlightEvent: BURNOUT events should have MotorClusterState type data payloads
  W[Other,NORMAL] Illegal parameters for FlightEvent: EJECTION_CHARGE events should have MotorClusterState type data payloads
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was R
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was P
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was <i>Component Removed From Rocket</i>
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was S
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_ABORT events require SimulationAbort objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Stored' desc='Stored' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: events that are valid with a source or a cause",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="Other"><id>cccccccc-0000-0000-0000-000000000001</id><description>Stored</description></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="1" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="1" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="1" type="ignition" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="burnout" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="ejectioncharge" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="launch" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="stageseparation" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="recoverydevicedeployment" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="1" type="apogee" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="simabort" cause="noactivestages"/>
    <event time="1" type="simabort" cause="nomotorsdefined" source="aaaaaaaa-0000-0000-0000-000000000003"/>
    <event time="1" type="simabort" cause="noconfiguredignition" source="aaaaaaaa-0000-0000-0000-000000000099"/>
    <event time="1" type="simabort" cause="nomotorsfired"/>
    <event time="1" type="simabort" cause="noliftoff"/>
    <event time="1" type="simabort" cause="nocp"/>
    <event time="1" type="simabort" cause="activelengthzero"/>
    <event time="1" type="simabort" cause="activemasszero"/>
    <event time="1" type="simabort" cause="tumbleunderthrust"/>
    <event time="1" type="simabort" cause="deployunderthrust"/>
    <event time="1" type="simabort" cause=" nocp "/>
    <event time="1" type="simabort" cause="NO_CP"/>
    <event time="1" type="launch" cause="nocp"/>
    <event time="1" type="altitude" cause="nocp"/>
    <event time="1" type="exception" cause="nocp"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001"/>
    <event time="1" type="launch" warnid="cccccccc-0000-0000-0000-000000000001"/>
    <event time="1" type="launch" warnid="not-a-uuid" eventid="not-a-uuid"/>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" eventid="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_ABORT events require SimulationAbort objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Stored' desc='Stored' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=1.0 srcId=null
      types=Time{t,Time,time,s}
      event IGNITION t=1.0 id=(random) src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      event BURNOUT t=1.0 id=(random) src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[null]
      event EJECTION_CHARGE t=1.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event IGNITION t=1.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event BURNOUT t=1.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event EJECTION_CHARGE t=1.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event LAUNCH t=1.0 id=(random) src=P@aaaaaaaa-0000-0000-0000-000000000004 data=[null]
      event STAGE_SEPARATION t=1.0 id=(random) src=P@aaaaaaaa-0000-0000-0000-000000000004 data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=1.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event APOGEE t=1.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[null]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:No active stages]
      event SIM_ABORT t=1.0 id=(random) src=B@aaaaaaaa-0000-0000-0000-000000000003 data=[SimulationAbort:No motors defined in the simulation]
      event SIM_ABORT t=1.0 id=(random) src=<i>Component Removed From Rocket</i>(REMOVED) data=[SimulationAbort:No motors configured to ignite at liftoff]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:No motors ignited]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:<html>Motor burnout without liftoff. <br>Use more (powerful) motors, or decrease the rocket mass.</html>]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Active airframe has length 0]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Total mass of active stages is 0]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Stage began to tumble under thrust.]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Recovery system deployed while still under thrust]
      event SIM_ABORT t=1.0 id=(random) src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event LAUNCH t=1.0 id=(random) src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event ALTITUDE t=1.0 id=(random) src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event EXCEPTION t=1.0 id=(random) src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event SIM_WARN t=1.0 id=(random) src=null data=[Other:Stored]
      event LAUNCH t=1.0 id=(random) src=null data=[null]
      event LAUNCH t=1.0 id=(random) src=null data=[null]
      event SIM_WARN t=1.0 id=(random) src=null data=[Other:Stored]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event ids",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="3" type="tumble" id="BBBBBBBB-0000-0000-0000-00000000000C"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event LAUNCH t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event APOGEE t=2.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event TUMBLE t=3.0 id=bbbbbbbb-0000-0000-0000-00000000000c src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with an empty id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" id=""/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: ]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with a padded id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" id=" bbbbbbbb-0000-0000-0000-000000000001"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [UUID string too large]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with an empty source",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" source=""/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: ]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with an empty warnid",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid=""/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: ]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a bad time or type hides a bad id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="x" type="launch" id="not-a-uuid" source="not-a-uuid"/>
    <event time="1" type="x" id="not-a-uuid" source="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal event time specification, ignoring: For input string: "x"
  W[Other,NORMAL] Illegal event specification, ignoring.
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a bad id fails before the event is checked",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="NaN" type="launch" id="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with text and attributes it does not know",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" bogus="1">text</event>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event LAUNCH t=1.0 id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event with a child element",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch"><x time="2" type="apogee"/></event>
    <event time="3" type="tumble"><x/></event>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  W[Other,NORMAL] Illegal event time specification, ignoring: null string
  closed=flightdata {time=1, type=launch} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=2.0 id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: events and datapoints in any order",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time,altitude">
    <event time="5" type="groundhit"/>
    <datapoint>0,0</datapoint>
    <event time="1" type="launch"/>
    <datapoint>1,3</datapoint>
    <event time="0.5" type="liftoff"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=3.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=1.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=2 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event GROUND_HIT t=5.0 id=(random) src=null data=[null]
      event LAUNCH t=1.0 id=(random) src=null data=[null]
      event LIFTOFF t=0.5 id=(random) src=null data=[null]
      row: 0.0 0.0
      row: 1.0 3.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two stage separations",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="2" type="stageseparation" source="aaaaaaaa-0000-0000-0000-000000000002"/>
    <event time="1" type="stageseparation"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=1.0 srcId=null
      types=Time{t,Time,time,s}
      event STAGE_SEPARATION t=2.0 id=(random) src=S@aaaaaaaa-0000-0000-0000-000000000002 data=[null]
      event STAGE_SEPARATION t=1.0 id=(random) src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: event after landing is hooked up to an earlier event",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding">
    <id>cccccccc-0000-0000-0000-000000000003</id>
    <priority>HIGH</priority>
  </warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="recoverydevicedeployment" id="bbbbbbbb-0000-0000-0000-000000000009" source="aaaaaaaa-0000-0000-0000-000000000004"/>
    <event time="1" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000009"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,HIGH] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Recovery device deployment' desc='Flight Event occurred after landing: Recovery device deployment' sources= event=Recovery device deployment@bbbbbbbb-0000-0000-0000-000000000009
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event RECOVERY_DEVICE_DEPLOYMENT t=1.0 id=bbbbbbbb-0000-0000-0000-000000000009 src=P@aaaaaaaa-0000-0000-0000-000000000004 data=[null]
      event SIM_WARN t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: Recovery device deployment]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: every event type in the text of an event after landing",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000001</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000002</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000004</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000005</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000006</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000007</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000008</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000009</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000a</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000b</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000c</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000d</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000e</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-00000000000f</id></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000010</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="1" type="ignition" id="bbbbbbbb-0000-0000-0000-000000000002"/>
    <event time="1" type="liftoff" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="1" type="launchrod" id="bbbbbbbb-0000-0000-0000-000000000004"/>
    <event time="1" type="burnout" id="bbbbbbbb-0000-0000-0000-000000000005"/>
    <event time="1" type="ejectioncharge" id="bbbbbbbb-0000-0000-0000-000000000006"/>
    <event time="1" type="stageseparation" id="bbbbbbbb-0000-0000-0000-000000000007"/>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000008"/>
    <event time="1" type="recoverydevicedeployment" id="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="1" type="groundhit" id="bbbbbbbb-0000-0000-0000-00000000000a"/>
    <event time="1" type="simulationend" id="bbbbbbbb-0000-0000-0000-00000000000b"/>
    <event time="1" type="altitude" id="bbbbbbbb-0000-0000-0000-00000000000c"/>
    <event time="1" type="tumble" id="bbbbbbbb-0000-0000-0000-00000000000d"/>
    <event time="1" type="simabort" id="bbbbbbbb-0000-0000-0000-00000000000f" cause="nocp"/>
    <event time="1" type="exception" id="bbbbbbbb-0000-0000-0000-000000000010"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000002" eventid="bbbbbbbb-0000-0000-0000-000000000002"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000004" eventid="bbbbbbbb-0000-0000-0000-000000000004"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000005" eventid="bbbbbbbb-0000-0000-0000-000000000005"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000006" eventid="bbbbbbbb-0000-0000-0000-000000000006"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000007" eventid="bbbbbbbb-0000-0000-0000-000000000007"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000008" eventid="bbbbbbbb-0000-0000-0000-000000000008"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000009" eventid="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-00000000000a" eventid="bbbbbbbb-0000-0000-0000-00000000000a"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-00000000000b" eventid="bbbbbbbb-0000-0000-0000-00000000000b"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-00000000000c" eventid="bbbbbbbb-0000-0000-0000-00000000000c"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-00000000000d" eventid="bbbbbbbb-0000-0000-0000-00000000000d"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000e" warnid="cccccccc-0000-0000-0000-00000000000e" eventid="bbbbbbbb-0000-0000-0000-00000000000e"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-00000000000f" eventid="bbbbbbbb-0000-0000-0000-00000000000f"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000010" eventid="bbbbbbbb-0000-0000-0000-000000000010"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Flight Event occurred after landing: Launch' desc='Flight Event occurred after landing: Launch' sources= event=Launch@bbbbbbbb-0000-0000-0000-000000000001
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000002 text='Flight Event occurred after landing: Motor ignition' desc='Flight Event occurred after landing: Motor ignition' sources= event=Motor ignition@bbbbbbbb-0000-0000-0000-000000000002
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Lift-off' desc='Flight Event occurred after landing: Lift-off' sources= event=Lift-off@bbbbbbbb-0000-0000-0000-000000000003
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000004 text='Flight Event occurred after landing: Launch rod clearance' desc='Flight Event occurred after landing: Launch rod clearance' sources= event=Launch rod clearance@bbbbbbbb-0000-0000-0000-000000000004
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000005 text='Flight Event occurred after landing: Motor burnout' desc='Flight Event occurred after landing: Motor burnout' sources= event=Motor burnout@bbbbbbbb-0000-0000-0000-000000000005
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000006 text='Flight Event occurred after landing: Ejection charge' desc='Flight Event occurred after landing: Ejection charge' sources= event=Ejection charge@bbbbbbbb-0000-0000-0000-000000000006
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000007 text='Flight Event occurred after landing: Stage separation' desc='Flight Event occurred after landing: Stage separation' sources= event=Stage separation@bbbbbbbb-0000-0000-0000-000000000007
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000008 text='Flight Event occurred after landing: Apogee' desc='Flight Event occurred after landing: Apogee' sources= event=Apogee@bbbbbbbb-0000-0000-0000-000000000008
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000009 text='Flight Event occurred after landing: Recovery device deployment' desc='Flight Event occurred after landing: Recovery device deployment' sources= event=Recovery device deployment@bbbbbbbb-0000-0000-0000-000000000009
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000a text='Flight Event occurred after landing: Ground hit' desc='Flight Event occurred after landing: Ground hit' sources= event=Ground hit@bbbbbbbb-0000-0000-0000-00000000000a
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000b text='Flight Event occurred after landing: Simulation end' desc='Flight Event occurred after landing: Simulation end' sources= event=Simulation end@bbbbbbbb-0000-0000-0000-00000000000b
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000c text='Flight Event occurred after landing: Altitude change' desc='Flight Event occurred after landing: Altitude change' sources= event=Altitude change@bbbbbbbb-0000-0000-0000-00000000000c
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000d text='Flight Event occurred after landing: Tumbling' desc='Flight Event occurred after landing: Tumbling' sources= event=Tumbling@bbbbbbbb-0000-0000-0000-00000000000d
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000e text='Flight Event occurred after landing: Warning' desc='Flight Event occurred after landing: Warning' sources= event=Warning@bbbbbbbb-0000-0000-0000-00000000000e
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-00000000000f text='Flight Event occurred after landing: Simulation abort' desc='Flight Event occurred after landing: Simulation abort' sources= event=Simulation abort@bbbbbbbb-0000-0000-0000-00000000000f
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000010 text='Flight Event occurred after landing: Exception' desc='Flight Event occurred after landing: Exception' sources= event=Exception@bbbbbbbb-0000-0000-0000-000000000010
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=1.0 srcId=null
      types=Time{t,Time,time,s}
      event LAUNCH t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event IGNITION t=1.0 id=bbbbbbbb-0000-0000-0000-000000000002 src=null data=[null]
      event LIFTOFF t=1.0 id=bbbbbbbb-0000-0000-0000-000000000003 src=null data=[null]
      event LAUNCHROD t=1.0 id=bbbbbbbb-0000-0000-0000-000000000004 src=null data=[null]
      event BURNOUT t=1.0 id=bbbbbbbb-0000-0000-0000-000000000005 src=null data=[null]
      event EJECTION_CHARGE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000006 src=null data=[null]
      event STAGE_SEPARATION t=1.0 id=bbbbbbbb-0000-0000-0000-000000000007 src=null data=[null]
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000008 src=null data=[null]
      event RECOVERY_DEVICE_DEPLOYMENT t=1.0 id=bbbbbbbb-0000-0000-0000-000000000009 src=null data=[null]
      event GROUND_HIT t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[null]
      event SIMULATION_END t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000b src=null data=[null]
      event ALTITUDE t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000c src=null data=[null]
      event TUMBLE t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000d src=null data=[null]
      event SIM_ABORT t=1.0 id=bbbbbbbb-0000-0000-0000-00000000000f src=null data=[SimulationAbort:Can't calculate Center of Pressure]
      event EXCEPTION t=1.0 id=bbbbbbbb-0000-0000-0000-000000000010 src=null data=[null]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Launch]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Motor ignition]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Lift-off]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Launch rod clearance]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Motor burnout]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Ejection charge]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Stage separation]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Apogee]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Recovery device deployment]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Ground hit]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Simulation end]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Altitude change]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Tumbling]
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000e src=null data=[EventAfterLanding:Flight Event occurred after landing: Warning]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Simulation abort]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Exception]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two events after landing share one warning",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="1" type="tumble" id="bbbbbbbb-0000-0000-0000-000000000002"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="3" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000b" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000002"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Tumbling' desc='Flight Event occurred after landing: Tumbling' sources= event=Tumbling@bbbbbbbb-0000-0000-0000-000000000002
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event TUMBLE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000002 src=null data=[null]
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: Tumbling]
      event SIM_WARN t=3.0 id=bbbbbbbb-0000-0000-0000-00000000000b src=null data=[EventAfterLanding:Flight Event occurred after landing: Tumbling]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an event after landing without eventid keeps the event set before",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="3" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000b" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Apogee' desc='Flight Event occurred after landing: Apogee' sources= event=Apogee@bbbbbbbb-0000-0000-0000-000000000001
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: Apogee]
      event SIM_WARN t=3.0 id=bbbbbbbb-0000-0000-0000-00000000000b src=null data=[EventAfterLanding:Flight Event occurred after landing: Apogee]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an eventid that names no event clears the event set before",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="3" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000b" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000077"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      event SIM_WARN t=3.0 id=bbbbbbbb-0000-0000-0000-00000000000b src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an eventid of an event of another branch",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
  </databranch>
  <databranch name="B" types="time">
    <datapoint>0</datapoint>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=2 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      row: 0.0
    branch[1] 'B' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=2.0 id=bbbbbbbb-0000-0000-0000-00000000000a src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an event after landing that is dropped still hooks its warning up",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="NaN" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000a" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" id="bbbbbbbb-0000-0000-0000-00000000000b" warnid="cccccccc-0000-0000-0000-000000000003" source="aaaaaaaa-0000-0000-0000-000000000004" eventid="bbbbbbbb-0000-0000-0000-000000000001"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event has a NaN time!
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN event requires null source component; was P
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Apogee' desc='Flight Event occurred after landing: Apogee' sources= event=Apogee@bbbbbbbb-0000-0000-0000-000000000001
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: an event after landing with a cause is no warning event",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" cause="nocp" eventid="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=null data=[null]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: eventid of a warning of another kind is not read",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000003</id><parameter>0.5</parameter></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="not-a-uuid"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=2.0 id=(random) src=null data=[LargeAOA:Large angle of attack encountered (28.6<U+00B0>)]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id with a group that is no number",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="g-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 0 in: "g"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id with an empty group",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="1--3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT []
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id with a group beyond a long",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="1-2-3-4-12345678901234567"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 16 in: "12345678901234567"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id with a group that is only a sign",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="+-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 1 in: "+"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id of 36 characters with a letter that is no digit",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeeg"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 11 in: "eeeeeeeeeeeg"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event id of 20 characters of two bytes",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: <U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9>]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event source with a group that is no number",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" source="1-2-3-z-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 0 in: "z"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event source with an empty group",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" source="-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT []
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event warnid with a group that is no number",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="q-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 0 in: "q"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: event warnid with an empty group",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="1-2-3--5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT []
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid that is no id without a warning is not read",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" eventid="1-2-3-4-"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid with an empty group",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="1-2-3-4-"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT []
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid with a group that is no number",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="1-2-3-4-5x"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 1 in: "5x"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid with a group beyond a long",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="12345678901234567-2-3-4-5"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Error at index 16 in: "12345678901234567"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid of 20 characters of two bytes",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;&#233;"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: <U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9><U+00E9>]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: eventid in the lenient form",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="apogee" id="bbbbbbbb-0000-0000-0000-000000000003"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0-0-0-3"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: Apogee' desc='Flight Event occurred after landing: Apogee' sources= event=Apogee@bbbbbbbb-0000-0000-0000-000000000003
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event APOGEE t=1.0 id=bbbbbbbb-0000-0000-0000-000000000003 src=null data=[null]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: Apogee]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "rb: two names with a long s",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd &#383;x,qtrFd sx">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=qtrFd <U+017F>x{Unknown,Custom,-,<U+200B>} | qtrFd sx{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0
)out",
     .qtrocket = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd sx already exists.]
  closed=(not closed)
  data: null
)out",
     .why = "The long s and the s are equal ignoring case, so the two names are one type twice and "
            "the load fails, where OpenRocket loads two columns: its HashMap hashes the names in "
            "lower case, which leaves the long s as it is (see FlightDataType::hashCode())."},
    {.name     = "rb: two names with the two small sigmas",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd &#963;x,qtrFd &#962;x">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=qtrFd <U+03C3>x{Unknown,Custom,-,<U+200B>} | qtrFd <U+03C2>x{Unknown,Custom,-,<U+200B>}
      row: 1.0 2.0
)out",
     .qtrocket = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd <U+03C2>x already exists.]
  closed=(not closed)
  data: null
)out",
     .why = "The sigma and the final sigma are equal ignoring case, so the two names are one type "
            "twice and the load fails, where OpenRocket loads two columns: both letters are lower "
            "case already and hash apart (see FlightDataType::hashCode())."},
    {.name     = "rb: two names with a Kelvin sign",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <databranch name="A" types="qtrFd &#8490;x,qtrFd kx">
    <datapoint>1,2</datapoint>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Value type qtrFd kx already exists.]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: branch
});

/// A warning's text holds an angle or a speed in the default units.
class DataBranchElements : public ::testing::TestWithParam<FlightDataCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(DataBranchElements, LoadAsInOpenRocketButWhereStated)
{
    expectFlightDataCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, DataBranchElements, ::testing::ValuesIn(kBranchCases),
                         flightDataCaseTestName);

// ---- the column names ---------------------------------------------------------------------

/// A column name of a file and the type OpenRocket reads it as.
struct ColumnName
{
    /// The name, as it stands in a types attribute.
    std::string_view name;
    /// The save key of the built-in type it is; empty for a name that is no built-in type.
    std::string_view saveKey;
};

// The 145 column names of the scout of tier 9 (the save keys and names of every built-in type,
// the names of the 16 examples and of their history, the names that have since changed) with the
// type OpenRocket's FlightDataBranchHandler resolves each to (the scout's TypeProbe,
// column-types-java.tsv).
constexpr auto kColumnNames = std::to_array<ColumnName>({
    // BEGIN GENERATED: columns
    {.name = "acceleration_bodyx", .saveKey = "acceleration_bodyx"},
    {.name = "acceleration_bodyy", .saveKey = "acceleration_bodyy"},
    {.name = "acceleration_bodyz", .saveKey = "acceleration_bodyz"},
    {.name = "acceleration_total", .saveKey = "acceleration_total"},
    {.name = "acceleration_x", .saveKey = "acceleration_x"},
    {.name = "acceleration_xy", .saveKey = "acceleration_xy"},
    {.name = "acceleration_y", .saveKey = "acceleration_y"},
    {.name = "acceleration_z", .saveKey = "acceleration_z"},
    {.name = "air_density", .saveKey = "air_density"},
    {.name = "Air density", .saveKey = "air_density"},
    {.name = "air_pressure", .saveKey = "air_pressure"},
    {.name = "Air pressure", .saveKey = "air_pressure"},
    {.name = "air_temperature", .saveKey = "air_temperature"},
    {.name = "Air temperature", .saveKey = "air_temperature"},
    {.name = "altitude", .saveKey = "altitude"},
    {.name = "Altitude", .saveKey = "altitude"},
    {.name = "altitude_above_sea", .saveKey = "altitude_above_sea"},
    {.name = "Altitude above sea level", .saveKey = "altitude_above_sea"},
    {.name = "Angle of attack", .saveKey = "aoa"},
    {.name = "aoa", .saveKey = "aoa"},
    {.name = "axial_drag_coeff", .saveKey = "axial_drag_coeff"},
    {.name = "Axial drag coefficient", .saveKey = "axial_drag_coeff"},
    {.name = "Axial drag coefficient (CA)", .saveKey = "axial_drag_coeff"},
    {.name = "base_drag_coeff", .saveKey = "base_drag_coeff"},
    {.name = "Base drag coefficient", .saveKey = "base_drag_coeff"},
    {.name = "Base drag coefficient (CD_base)", .saveKey = "base_drag_coeff"},
    {.name = "cg_location", .saveKey = "cg_location"},
    {.name = "CG location", .saveKey = "cg_location"},
    {.name = "cna", .saveKey = "cna"},
    {.name = "computation_time", .saveKey = "computation_time"},
    {.name = "Computation time", .saveKey = "computation_time"},
    {.name = "Control fin cant", .saveKey = ""},
    {.name = "coriolis_acceleration", .saveKey = "coriolis_acceleration"},
    {.name = "Coriolis acceleration", .saveKey = "coriolis_acceleration"},
    {.name = "corrective_moment_coeff", .saveKey = "corrective_moment_coeff"},
    {.name = "Corrective moment coefficient", .saveKey = "corrective_moment_coeff"},
    {.name = "cp_location", .saveKey = "cp_location"},
    {.name = "CP location", .saveKey = "cp_location"},
    {.name = "damping_moment_coeff", .saveKey = "damping_moment_coeff"},
    {.name = "damping_moment_coeff_aerodynamic", .saveKey = "damping_moment_coeff_aerodynamic"},
    {.name = "Damping moment coefficient", .saveKey = "damping_moment_coeff"},
    {.name    = "Damping moment coefficient (aerodynamic)",
     .saveKey = "damping_moment_coeff_aerodynamic"},
    {.name    = "Damping moment coefficient (propulsive)",
     .saveKey = "damping_moment_coeff_propulsive"},
    {.name = "damping_moment_coeff_propulsive", .saveKey = "damping_moment_coeff_propulsive"},
    {.name = "damping_ratio", .saveKey = "damping_ratio"},
    {.name = "Damping ratio", .saveKey = "damping_ratio"},
    {.name = "drag_coeff", .saveKey = "drag_coeff"},
    {.name = "Drag coefficient", .saveKey = "drag_coeff"},
    {.name = "Drag coefficient (CD)", .saveKey = "drag_coeff"},
    {.name = "drag_force", .saveKey = "drag_force"},
    {.name = "Drag force", .saveKey = "drag_force"},
    {.name = "friction_drag_coeff", .saveKey = "friction_drag_coeff"},
    {.name = "Friction drag coefficient", .saveKey = "friction_drag_coeff"},
    {.name = "Friction drag coefficient (CD_friction)", .saveKey = "friction_drag_coeff"},
    {.name = "Gravitational acceleration", .saveKey = "gravity"},
    {.name = "gravity", .saveKey = "gravity"},
    {.name = "Lateral acceleration", .saveKey = "acceleration_xy"},
    {.name = "Lateral direction", .saveKey = "position_direction"},
    {.name = "Lateral distance", .saveKey = "position_xy"},
    {.name = "Lateral orientation (azimuth)", .saveKey = "orientation_phi"},
    {.name = "Lateral velocity", .saveKey = "velocity_xy"},
    {.name = "latitude", .saveKey = "latitude"},
    {.name = "Latitude", .saveKey = "latitude"},
    {.name = "longitude", .saveKey = "longitude"},
    {.name = "Longitude", .saveKey = "longitude"},
    {.name = "longitudinal_inertia", .saveKey = "longitudinal_inertia"},
    {.name = "Longitudinal moment of inertia", .saveKey = "longitudinal_inertia"},
    {.name = "mach_number", .saveKey = "mach_number"},
    {.name = "Mach number", .saveKey = "mach_number"},
    {.name = "mass", .saveKey = "mass"},
    {.name = "Mass", .saveKey = "mass"},
    {.name = "motor_mass", .saveKey = "motor_mass"},
    {.name = "Motor mass", .saveKey = "motor_mass"},
    {.name = "natural_frequency", .saveKey = "natural_frequency"},
    {.name = "Natural frequency", .saveKey = "natural_frequency"},
    {.name = "normal_force_coeff", .saveKey = "normal_force_coeff"},
    {.name = "Normal force coefficient", .saveKey = "normal_force_coeff"},
    {.name = "Normal force coefficient (CN)", .saveKey = "normal_force_coeff"},
    {.name = "Normal force coefficient derivative (CN\u03b1)", .saveKey = "cna"},
    {.name = "orientation_phi", .saveKey = "orientation_phi"},
    {.name = "orientation_theta", .saveKey = "orientation_theta"},
    {.name = "Pitch damping coefficient", .saveKey = "pitch_damping_moment_coeff"},
    {.name = "pitch_damping_moment_coeff", .saveKey = "pitch_damping_moment_coeff"},
    {.name = "pitch_moment_coeff", .saveKey = "pitch_moment_coeff"},
    {.name = "Pitch moment coefficient", .saveKey = "pitch_moment_coeff"},
    {.name = "Pitch moment coefficient (Cm)", .saveKey = "pitch_moment_coeff"},
    {.name = "pitch_rate", .saveKey = "pitch_rate"},
    {.name = "Pitch rate", .saveKey = "pitch_rate"},
    {.name = "position_direction", .saveKey = "position_direction"},
    {.name = "Position East of launch", .saveKey = "position_x"},
    {.name = "Position North of launch", .saveKey = "position_y"},
    {.name = "Position upwind", .saveKey = "position_y"},
    {.name = "position_x", .saveKey = "position_x"},
    {.name = "position_xy", .saveKey = "position_xy"},
    {.name = "position_y", .saveKey = "position_y"},
    {.name = "pressure_drag_coeff", .saveKey = "pressure_drag_coeff"},
    {.name = "Pressure drag coefficient", .saveKey = "pressure_drag_coeff"},
    {.name = "Pressure drag coefficient (CD_pressure)", .saveKey = "pressure_drag_coeff"},
    {.name = "reference_area", .saveKey = "reference_area"},
    {.name = "Reference area", .saveKey = "reference_area"},
    {.name = "reference_length", .saveKey = "reference_length"},
    {.name = "Reference length", .saveKey = "reference_length"},
    {.name = "reynolds_number", .saveKey = "reynolds_number"},
    {.name = "Reynolds number", .saveKey = "reynolds_number"},
    {.name = "roll_damping_coeff", .saveKey = "roll_damping_coeff"},
    {.name = "Roll damping coefficient", .saveKey = "roll_damping_coeff"},
    {.name = "roll_forcing_coeff", .saveKey = "roll_forcing_coeff"},
    {.name = "Roll forcing coefficient", .saveKey = "roll_forcing_coeff"},
    {.name = "roll_moment_coeff", .saveKey = "roll_moment_coeff"},
    {.name = "Roll moment coefficient", .saveKey = "roll_moment_coeff"},
    {.name = "roll_rate", .saveKey = "roll_rate"},
    {.name = "Roll rate", .saveKey = "roll_rate"},
    {.name = "rotational_inertia", .saveKey = "rotational_inertia"},
    {.name = "Rotational moment of inertia", .saveKey = "rotational_inertia"},
    {.name = "side_force_coeff", .saveKey = "side_force_coeff"},
    {.name = "Side force coefficient", .saveKey = "side_force_coeff"},
    {.name = "Simulation time step", .saveKey = "time_step"},
    {.name = "speed_of_sound", .saveKey = "speed_of_sound"},
    {.name = "Speed of sound", .saveKey = "speed_of_sound"},
    {.name = "stability", .saveKey = "stability"},
    {.name = "Stability margin calibers", .saveKey = "stability"},
    {.name = "Thrust", .saveKey = "thrust_force"},
    {.name = "thrust_correction", .saveKey = "thrust_correction"},
    {.name = "thrust_force", .saveKey = "thrust_force"},
    {.name = "Thrust-to-weight ratio", .saveKey = "thrust_weight_ratio"},
    {.name = "thrust_weight_ratio", .saveKey = "thrust_weight_ratio"},
    {.name = "time", .saveKey = "time"},
    {.name = "Time", .saveKey = "time"},
    {.name = "time_step", .saveKey = "time_step"},
    {.name = "Total acceleration", .saveKey = "acceleration_total"},
    {.name = "Total velocity", .saveKey = "velocity_total"},
    {.name = "velocity_total", .saveKey = "velocity_total"},
    {.name = "velocity_xy", .saveKey = "velocity_xy"},
    {.name = "velocity_z", .saveKey = "velocity_z"},
    {.name = "Vertical acceleration", .saveKey = "acceleration_z"},
    {.name = "Vertical orientation (zenith)", .saveKey = "orientation_theta"},
    {.name = "Vertical velocity", .saveKey = "velocity_z"},
    {.name = "wind_direction", .saveKey = "wind_direction"},
    {.name = "Wind direction", .saveKey = "wind_direction"},
    {.name = "wind_velocity", .saveKey = "wind_velocity"},
    {.name = "Wind velocity", .saveKey = "wind_velocity"},
    {.name = "yaw_moment_coeff", .saveKey = "yaw_moment_coeff"},
    {.name = "Yaw moment coefficient", .saveKey = "yaw_moment_coeff"},
    {.name = "yaw_rate", .saveKey = "yaw_rate"},
    {.name = "Yaw rate", .saveKey = "yaw_rate"},
    // END GENERATED: columns
});

/// What @p name resolves to in a document without custom expressions: the save key of a
/// built-in type (which is then that very type), else the description of the type in ASCII.
[[nodiscard]] std::string resolvedColumn(std::string_view name, FlightDataFixture& fixture)
{
    const FlightDataType& type =
        FlightDataBranchHandler::findFlightDataType(name, fixture.document());
    if (FlightDataType::getTypeBySaveKey(type.getSaveKey()) == &type)
    {
        return type.getSaveKey();
    }
    return ascii(describeFlightDataType(type));
}

/// "<name> -> <resolvedColumn()>" for every name of kColumnNames.
[[nodiscard]] Texts resolvedColumns()
{
    FlightDataFixture fixture;
    Texts             lines;
    for (const ColumnName& column : kColumnNames)
    {
        lines.push_back(std::format("{} -> {}", column.name, resolvedColumn(column.name, fixture)));
    }
    return lines;
}

/// "<name> -> <save key>" for every name of kColumnNames, as OpenRocket resolves it: a name
/// that is no built-in type is a type of that name with the symbol "Unknown" and no unit (whose
/// unit prints as a zero-width space).
[[nodiscard]] Texts expectedColumns()
{
    Texts lines;
    for (const ColumnName& column : kColumnNames)
    {
        lines.push_back(std::format(
            "{} -> {}", column.name,
            column.saveKey.empty() ? std::format("{}{{Unknown,Custom,-,<U+200B>}}", column.name)
                                   : std::string(column.saveKey)));
    }
    return lines;
}

TEST(FlightDataBranchHandlerColumns, EveryColumnNameResolvesAsInOpenRocket)
{
    ASSERT_EQ(kColumnNames.size(), 145U);
    EXPECT_EQ(resolvedColumns(), expectedColumns());
}

// ---- the handler by itself ------------------------------------------------------------------

/// A handler for the types @p typeList in the document of @p fixture, with @p set as the
/// warnings of the flight data; null when create() fails.
[[nodiscard]] std::unique_ptr<FlightDataBranchHandler> makeHandler(FlightDataFixture& fixture,
                                                                   WarningSet&        set,
                                                                   std::string_view   typeList)
{
    Result<std::unique_ptr<FlightDataBranchHandler>> handler =
        FlightDataBranchHandler::create("Branch", typeList, set, fixture.context());
    return handler.has_value() ? std::move(*handler) : nullptr;
}

/// What create() fails with for @p typeList, as "<code>: <message>"; "created" when it does not
/// fail.
[[nodiscard]] std::string createFailure(std::string_view typeList)
{
    FlightDataFixture                                      fixture;
    WarningSet                                             set;
    const Result<std::unique_ptr<FlightDataBranchHandler>> handler =
        FlightDataBranchHandler::create("Branch", typeList, set, fixture.context());
    if (handler.has_value())
    {
        return "created";
    }
    return std::format("{}: {}", toString(handler.error().code), handler.error().message);
}

/// The names of the types of @p handler, in the order of its types attribute.
[[nodiscard]] Texts typeNames(const FlightDataBranchHandler& handler)
{
    Texts names;
    for (const FlightDataType* type : handler.getTypes())
    {
        names.push_back(type->getName());
    }
    return names;
}

TEST(FlightDataBranchHandler, TheColumnsAreInTheOrderOfTheTypesAttribute)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    const std::unique_ptr<FlightDataBranchHandler> handler =
        makeHandler(fixture, set, "velocity_total,Time,qtrFd order,altitude");
    ASSERT_NE(handler, nullptr);
    EXPECT_EQ(typeNames(*handler), (Texts{"Total velocity", "Time", "qtrFd order", "Altitude"}));
    // A built-in type is the process-wide object.
    ASSERT_EQ(handler->getTypes().size(), 4U);
    EXPECT_EQ(handler->getTypes()[0],
              &FlightDataType::builtin(FlightDataTypeId::TYPE_VELOCITY_TOTAL));
    EXPECT_EQ(handler->getTypes()[1], &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
    EXPECT_EQ(handler->getTypes()[3], &FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
    // The branch sorts them (FlightDataBranch::getTypes()).
    const std::shared_ptr<FlightDataBranch> branch = handler->getBranch();
    EXPECT_EQ(branch->getName(), "Branch");
    EXPECT_EQ(branch->getTypes().size(), 4U);
    EXPECT_EQ(branch->getTypes().front(), &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
}

TEST(FlightDataBranchHandler, AListWithoutATypeOrWithATypeTwiceFails)
{
    EXPECT_EQ(createFailure(","), "INVALID_ARGUMENT: Must specify at least one data type.");
    EXPECT_EQ(createFailure(",,,"), "INVALID_ARGUMENT: Must specify at least one data type.");
    EXPECT_EQ(createFailure("time,time"), "INVALID_ARGUMENT: Value type Time already exists.");
    // The later of the two is named, and the first pair in the order of the list.
    EXPECT_EQ(createFailure("qtrFd Twice,altitude,QTRFD TWICE,altitude"),
              "INVALID_ARGUMENT: Value type QTRFD TWICE already exists.");
    EXPECT_EQ(createFailure("altitude,qtrFd Twice,Altitude,QTRFD TWICE"),
              "INVALID_ARGUMENT: Value type Altitude already exists.");
    EXPECT_EQ(createFailure("time"), "created");
    EXPECT_EQ(createFailure("time,"), "created");
}

// Every unknown name replaces the type that is registered under the symbol "Unknown"; the types
// made before stay what they were.
TEST(FlightDataBranchHandler, AnUnknownNameIsRegisteredUnderTheSymbolUnknown)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    const std::unique_ptr<FlightDataBranchHandler> handler =
        makeHandler(fixture, set, "qtrFd first,qtrFd second");
    ASSERT_NE(handler, nullptr);
    ASSERT_EQ(handler->getTypes().size(), 2U);
    const FlightDataType* const first  = handler->getTypes()[0];
    const FlightDataType* const second = handler->getTypes()[1];
    EXPECT_NE(first, second);
    EXPECT_EQ(first->getName(), "qtrFd first");
    EXPECT_EQ(first->getSymbol(), "Unknown");
    EXPECT_EQ(first->getGroup(), FlightDataTypeGroup::CUSTOM);
    EXPECT_EQ(first->getUnitGroupId(), UnitGroupId::NONE);
    EXPECT_FALSE(first->isBuiltin());
    EXPECT_EQ(first->getSaveKey(), "qtrFd first");
    EXPECT_EQ(second->getName(), "qtrFd second");
    EXPECT_EQ(FlightDataType::findBySymbol("Unknown"), second);

    // The same name again, while it is the registered one, is the same type.
    const std::unique_ptr<FlightDataBranchHandler> again =
        makeHandler(fixture, set, "qtrFd second");
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again->getTypes().front(), second);

    // An empty name takes the name of the registered type.
    const std::unique_ptr<FlightDataBranchHandler> empty = makeHandler(fixture, set, "");
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(typeNames(*empty), Texts{"qtrFd second"});
}

// Java resolves every name before its branch refuses the list.
TEST(FlightDataBranchHandler, TheNamesOfAListThatFailsAreRegisteredAllTheSame)
{
    EXPECT_EQ(createFailure("time,time,qtrFd of a failed list"),
              "INVALID_ARGUMENT: Value type Time already exists.");
    const FlightDataType* const registered = FlightDataType::findBySymbol("Unknown");
    ASSERT_NE(registered, nullptr);
    EXPECT_EQ(registered->getName(), "qtrFd of a failed list");
}

TEST(FlightDataBranchHandler, ACustomExpressionOfTheDocumentIsFoundByItsName)
{
    FlightDataFixture fixture;
    fixture.document().addCustomExpression(
        CustomExpression("qtrFd energy", "qtrFdKe", "J", "0.5*m*Vt^2"));
    const FlightDataType& type =
        FlightDataBranchHandler::findFlightDataType("qtrFd energy", fixture.document());
    EXPECT_EQ(type.getName(), "qtrFd energy");
    EXPECT_EQ(type.getSymbol(), "qtrFdKe");
    EXPECT_EQ(type.getUnitGroupId(), UnitGroupId::ENERGY);
    EXPECT_EQ(FlightDataType::findBySymbol("qtrFdKe"), &type);
    // Another document does not know the expression.
    FlightDataFixture     other;
    const FlightDataType& unknown =
        FlightDataBranchHandler::findFlightDataType("qtrFd energy", other.document());
    EXPECT_EQ(unknown.getSymbol(), "Unknown");
}

TEST(FlightDataBranchHandler, TheContextNeedsADocument)
{
    const DocumentLoadingContext context;
    WarningSet                   set;
    EXPECT_THROW(
        {
            [[maybe_unused]] const auto handler =
                FlightDataBranchHandler::create("Branch", "time", set, context);
        },
        BugError);
}

TEST(FlightDataBranchHandler, DataPointsAndEventsArePlainTextAndTheRestIsIgnored)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    WarningSet                                     warnings;
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    ElementHandler* const plainText = &PlainTextHandler::instance();
    ElementHandler* const unset     = handler.get();
    EXPECT_EQ(handler->openElement("datapoint", {}, warnings).value_or(unset), plainText);
    EXPECT_EQ(handler->openElement("event", {}, warnings).value_or(unset), plainText);
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(handler->openElement("Datapoint", {}, warnings).value_or(unset), nullptr);
    EXPECT_EQ(QtRocket::Test::warningTexts(warnings),
              Texts{"Unknown element 'Datapoint' encountered, ignoring."});
}

TEST(FlightDataBranchHandler, TheBranchIsImmutableOnceItIsAskedFor)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    const std::unique_ptr<FlightDataBranchHandler> handler =
        makeHandler(fixture, set, "time,altitude");
    ASSERT_NE(handler, nullptr);
    handler->setOptimumAltitude(12.5);
    handler->setTimeToOptimumAltitude(1.5);
    const HandlerRun run = runHandler(
        *handler, "<databranch><datapoint>1,2</datapoint><datapoint>3,4</datapoint></databranch>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const std::shared_ptr<FlightDataBranch> branch = handler->getBranch();
    ASSERT_NE(branch, nullptr);
    EXPECT_FALSE(branch->isMutable());
    EXPECT_EQ(branch->getLength(), 2U);
    EXPECT_EQ(branch->getOptimumAltitude(), 12.5);
    EXPECT_EQ(branch->getTimeToOptimumAltitude(), 1.5);
    EXPECT_EQ(branch->getMaximum(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE)), 4.0);
    EXPECT_FALSE(branch->getSourceComponentId().has_value());
    EXPECT_TRUE(std::isnan(branch->getSeparationTime()));
    // The same branch every time.
    EXPECT_EQ(handler->getBranch(), branch);
    EXPECT_THROW(branch->addPoint(), BugError);
}

// Flight data read from a file keep no pointer into the document's rocket, which an undo
// replaces: an event knows its source by its id.
TEST(FlightDataBranchHandler, AnEventKeepsTheIdOfItsSourceAndNoPointer)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    const HandlerRun run = runHandler(*handler,
                                      "<databranch>"
                                      "<event time='1' type='ignition' id='1-2-3-4-5' "
                                      "source='aaaaaaaa-0000-0000-0000-000000000003'/>"
                                      "<event time='2' type='recoverydevicedeployment' "
                                      "source='aaaaaaaa-0000-0000-0000-000000000099'/>"
                                      "<event time='3' type='apogee'/>"
                                      "</databranch>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const std::vector<FlightEvent>& events = handler->getBranch()->getEvents();
    ASSERT_EQ(events.size(), 3U);
    EXPECT_EQ(events[0].getType(), FlightEvent::Type::IGNITION);
    EXPECT_EQ(events[0].getTime(), 1.0);
    // java.util.UUID.fromString() takes the short groups.
    EXPECT_EQ(events[0].getId().toString(), "00000001-0002-0003-0004-000000000005");
    EXPECT_EQ(events[0].getSource(), nullptr);
    EXPECT_EQ(events[0].getSourceId(), std::optional<Uuid>(uuidOf(kFlightTubeId)));
    EXPECT_FALSE(events[0].hasData());
    // An id that names no component is kept (decision L9 of the loader).
    EXPECT_EQ(events[1].getSource(), nullptr);
    EXPECT_EQ(events[1].getSourceId(),
              std::optional<Uuid>(uuidOf("aaaaaaaa-0000-0000-0000-000000000099")));
    EXPECT_EQ(events[1].getId().version(), 4);
    EXPECT_FALSE(events[2].hasSource());
    EXPECT_NE(events[1].getId(), events[2].getId());
}

TEST(FlightDataBranchHandler, AnEventCarriesItsWarningOrItsAbort)
{
    const DefaultUnitsGuard units;
    FlightDataFixture       fixture;
    WarningSet              set;
    Warning::LargeAOA       stored(0.25);
    stored.setId(uuidOf("cccccccc-0000-0000-0000-000000000001"));
    set.add(stored);
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    const HandlerRun run =
        runHandler(*handler,
                   "<databranch>"
                   "<event time='1' type='simwarn' warnid='cccccccc-0000-0000-0000-000000000001'/>"
                   "<event time='2' type='simabort' cause='tumbleunderthrust'/>"
                   "</databranch>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const std::vector<FlightEvent>& events = handler->getBranch()->getEvents();
    ASSERT_EQ(events.size(), 2U);
    const std::shared_ptr<const Warning> warning = events[0].getWarning();
    ASSERT_NE(warning, nullptr);
    // A copy with the id of the warning of the set.
    EXPECT_EQ(warning->id(), stored.id());
    EXPECT_NE(warning.get(), set.findById(stored.id()));
    EXPECT_EQ(warning->typeName(), "LargeAOA");
    const SimulationAbort* const abort = events[1].getAbort();
    ASSERT_NE(abort, nullptr);
    EXPECT_EQ(abort->cause(), SimulationAbort::Cause::TUMBLE_UNDER_THRUST);
}

/// The event of an EventAfterLanding warning as "<event type>@<event id>", "none" without an
/// event and "?" for a warning of another class or no warning.
[[nodiscard]] std::string eventOf(const Warning* warning)
{
    const auto* landing = dynamic_cast<const Warning::EventAfterLanding*>(warning);
    if (landing == nullptr)
    {
        return "?";
    }
    if (!landing->eventType().has_value() && !landing->eventId().has_value())
    {
        return "none";
    }
    return std::format("{}@{}", landing->eventType().value_or("-"),
                       landing->eventId().value_or(Uuid::nil()).toString());
}

// Java's events share the warning object with the warning set, so every event shows the event
// the warning was given last. Here an event holds the warning as it was when the event was
// read; the set has it as it is now.
TEST(FlightDataBranchHandler, AnEventAfterLandingGivesItsWarningTheEvent)
{
    FlightDataFixture          fixture;
    WarningSet                 set;
    Warning::EventAfterLanding stored;
    stored.setId(uuidOf("cccccccc-0000-0000-0000-000000000003"));
    set.add(stored);
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    const HandlerRun run =
        runHandler(*handler,
                   "<databranch>"
                   "<event time='1' type='apogee' id='bbbbbbbb-0000-0000-0000-000000000001'/>"
                   "<event time='1' type='tumble' id='bbbbbbbb-0000-0000-0000-000000000002'/>"
                   "<event time='2' type='simwarn' warnid='cccccccc-0000-0000-0000-000000000003' "
                   "eventid='bbbbbbbb-0000-0000-0000-000000000001'/>"
                   "<event time='3' type='simwarn' warnid='cccccccc-0000-0000-0000-000000000003' "
                   "eventid='bbbbbbbb-0000-0000-0000-000000000002'/>"
                   "<event time='4' type='simwarn' warnid='cccccccc-0000-0000-0000-000000000003'/>"
                   "</databranch>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const std::vector<FlightEvent>& events = handler->getBranch()->getEvents();
    ASSERT_EQ(events.size(), 5U);
    EXPECT_EQ(eventOf(events[2].getWarning().get()), "Apogee@bbbbbbbb-0000-0000-0000-000000000001");
    EXPECT_EQ(eventOf(events[3].getWarning().get()),
              "Tumbling@bbbbbbbb-0000-0000-0000-000000000002");
    // Without an eventid the warning keeps the event it has.
    EXPECT_EQ(eventOf(events[4].getWarning().get()),
              "Tumbling@bbbbbbbb-0000-0000-0000-000000000002");
    EXPECT_EQ(eventOf(set.findById(stored.id())), "Tumbling@bbbbbbbb-0000-0000-0000-000000000002");
    EXPECT_EQ(set.size(), 1U);
}

// The warning's event is looked up when the SIM_WARN event is already among the branch's
// events, so an eventid can name the SIM_WARN event itself.
TEST(FlightDataBranchHandler, AnEventAfterLandingCanNameItself)
{
    FlightDataFixture          fixture;
    WarningSet                 set;
    Warning::EventAfterLanding stored;
    stored.setId(uuidOf("cccccccc-0000-0000-0000-000000000003"));
    set.add(stored);
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    const HandlerRun run =
        runHandler(*handler,
                   "<databranch>"
                   "<event time='2' type='simwarn' id='bbbbbbbb-0000-0000-0000-00000000000e' "
                   "warnid='cccccccc-0000-0000-0000-000000000003' "
                   "eventid='bbbbbbbb-0000-0000-0000-00000000000e'/>"
                   "</databranch>");
    EXPECT_TRUE(run.result.has_value());

    const std::vector<FlightEvent>& events = handler->getBranch()->getEvents();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].getId().toString(), "bbbbbbbb-0000-0000-0000-00000000000e");
    EXPECT_EQ(eventOf(events[0].getWarning().get()),
              "Warning@bbbbbbbb-0000-0000-0000-00000000000e");
    EXPECT_EQ(eventOf(set.findById(stored.id())), "Warning@bbbbbbbb-0000-0000-0000-00000000000e");
}

TEST(FlightDataBranchHandler, AnIdThatIsNoUuidFailsTheLoad)
{
    FlightDataFixture                              fixture;
    WarningSet                                     set;
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    ASSERT_NE(handler, nullptr);
    const HandlerRun run = runHandler(
        *handler, "<databranch><event time='1' type='launch' source='no-uuid'/></databranch>");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    // The message of Java's UUID.fromString(); the cases have the other kinds of text.
    EXPECT_EQ(run.result.error().message, "Invalid UUID string: no-uuid");
    EXPECT_TRUE(handler->getBranch()->getEvents().empty());
}

// ---- every combination ----------------------------------------------------------------------

/// The values a test gives one attribute of an <event>; an empty text stands for no attribute.
struct AttributeValues
{
    std::string_view                name;
    std::array<std::string_view, 6> values;
    std::size_t                     count;
};

constexpr std::array<AttributeValues, 6> kEventAttributes{{
    {.name = "time", .values = {"", "1", "NaN", "Inf", "x", ""}, .count = 5},
    {.name   = "id",
     .values = {"", "bbbbbbbb-0000-0000-0000-000000000001", "", "", "", ""},
     .count  = 2},
    {.name   = "source",
     .values = {"", "aaaaaaaa-0000-0000-0000-000000000001", "aaaaaaaa-0000-0000-0000-000000000002",
                "aaaaaaaa-0000-0000-0000-000000000003", "aaaaaaaa-0000-0000-0000-000000000004",
                "aaaaaaaa-0000-0000-0000-000000000099"},
     .count  = 6},
    {.name   = "warnid",
     .values = {"", "cccccccc-0000-0000-0000-000000000001", "cccccccc-0000-0000-0000-000000000003",
                "cccccccc-0000-0000-0000-000000000099", "", ""},
     .count  = 4},
    {.name = "cause", .values = {"", "nocp", "bogus", "", "", ""}, .count = 3},
    {.name   = "eventid",
     .values = {"", "bbbbbbbb-0000-0000-0000-000000000001", "bbbbbbbb-0000-0000-0000-000000000077",
                "", "", ""},
     .count  = 3},
}};

/// An <event> element of the type @p type for every combination of the values of
/// kEventAttributes, inside a <databranch> element.
[[nodiscard]] std::string everyEventOfType(std::string_view type)
{
    std::string                                      xml = "<databranch>";
    std::array<std::size_t, kEventAttributes.size()> choice{};
    while (true)
    {
        xml += std::format("<event type='{}'", type);
        for (std::size_t i = 0; i < kEventAttributes.size(); i++)
        {
            const std::string_view value = kEventAttributes.at(i).values.at(choice.at(i));
            if (!value.empty())
            {
                xml += std::format(" {}='{}'", kEventAttributes.at(i).name, value);
            }
        }
        xml += "/>";

        std::size_t digit = 0;
        while (digit < choice.size() && ++choice.at(digit) == kEventAttributes.at(digit).count)
        {
            choice.at(digit) = 0;
            digit++;
        }
        if (digit == choice.size())
        {
            break;
        }
    }
    return xml + "</databranch>";
}

/// Whether every event of @p branch is what its type asks for: a SIM_WARN event has a warning
/// and no source, a SIM_ABORT event an abort, and no event a NaN time.
[[nodiscard]] bool everyEventIsValid(const FlightDataBranch& branch)
{
    return std::ranges::all_of(branch.getEvents(), [](const FlightEvent& event) {
        const bool warningOk = event.getType() != FlightEvent::Type::SIM_WARN ||
                               (event.getWarning() != nullptr && !event.hasSource());
        const bool abortOk =
            event.getType() != FlightEvent::Type::SIM_ABORT || event.getAbort() != nullptr;
        return warningOk && abortOk && !std::isnan(event.getTime());
    });
}

/// Reads every event of everyEventOfType(@p type) and says what came of it: "<number of events
/// kept> valid" (or "invalid" when one of them is not what its type asks for), or the failure.
/// Nothing may throw.
[[nodiscard]] std::string readEveryEventOfType(std::string_view type)
{
    FlightDataFixture          fixture;
    WarningSet                 set;
    Warning::Other             other("stored");
    Warning::EventAfterLanding landing;
    other.setId(uuidOf("cccccccc-0000-0000-0000-000000000001"));
    landing.setId(uuidOf("cccccccc-0000-0000-0000-000000000003"));
    set.add(other);
    set.add(landing);
    const std::unique_ptr<FlightDataBranchHandler> handler = makeHandler(fixture, set, "time");
    if (handler == nullptr)
    {
        return "no handler";
    }
    const HandlerRun run = runHandler(*handler, everyEventOfType(type));
    if (!run.result.has_value())
    {
        return "failed: " + run.result.error().message;
    }
    const std::shared_ptr<FlightDataBranch> branch = handler->getBranch();
    return std::format("{} {}", branch->getEvents().size(),
                       everyEventIsValid(*branch) ? "valid" : "invalid");
}

/// readEveryEventOfType() for every type of event, as "<type>: <outcome>".
[[nodiscard]] Texts readEveryEvent()
{
    Texts lines;
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        lines.push_back(std::format("{}: {}", name(type), readEveryEventOfType(orkName(type))));
    }
    lines.push_back("bogus: " + readEveryEventOfType("bogus"));
    return lines;
}

// Decision D9 of the loader: nothing a file can hold may end in a BugError. 2160 events of every
// type: each combination of 5 times, 2 ids, 6 sources, 4 warnings, 3 causes and 3 eventids.
// The numbers of events kept follow from the rules of the class comment. 2 of the times are
// kept ("1" and "Inf"; a NaN is refused by the check of the event, the other two are not read),
// so a type that takes any source and any data keeps 2 * 2 * 6 * 4 * 3 * 3 = 864. An IGNITION
// and a BURNOUT take 3 of the sources (none, the tube, the one that is not in the rocket) and no
// abort (2 of the causes): 288, and so does an EJECTION_CHARGE with the stage for the tube. A
// SIM_ABORT needs the one cause: 288. A SIM_WARN takes no source, needs one of the 2 warnings
// of the set and no abort: 2 * 2 * 1 * 2 * 2 * 3 = 48.
TEST(FlightDataBranchHandlerEveryEvent, EveryCombinationOfAttributesIsReadOrRefused)
{
    const Texts expected{
        "LAUNCH: 864 valid",
        "IGNITION: 288 valid",
        "LIFTOFF: 864 valid",
        "LAUNCHROD: 864 valid",
        "BURNOUT: 288 valid",
        "EJECTION_CHARGE: 288 valid",
        "STAGE_SEPARATION: 864 valid",
        "APOGEE: 864 valid",
        "RECOVERY_DEVICE_DEPLOYMENT: 864 valid",
        "GROUND_HIT: 864 valid",
        "SIMULATION_END: 864 valid",
        "ALTITUDE: 864 valid",
        "TUMBLE: 864 valid",
        "SIM_WARN: 48 valid",
        "SIM_ABORT: 288 valid",
        "EXCEPTION: 864 valid",
        "bogus: 0 valid",
    };
    EXPECT_EQ(readEveryEvent(), expected);
}

}  // namespace
