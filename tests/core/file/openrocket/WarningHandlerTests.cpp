#include "QtRocket/file/openrocket/WarningHandler.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <warning> elements, run through WarningHandler alone, and <flightdata> elements
// with warnings, run through FlightDataHandler as the loader runs them; their expectations are
// what OpenRocket makes of the same elements (the Java probe FdProbe of run 9b, part S2; see
// FlightDataTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::MessagePriority;
using QtRocket::MessageSource;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::WarningHandler;
using QtRocket::WarningSet;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::expectFlightDataCase;
using QtRocket::Test::FlightDataCase;
using QtRocket::Test::flightDataCaseTestName;
using QtRocket::Test::FlightDataFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kFlightChuteId;
using QtRocket::Test::runHandler;
using QtRocket::Test::uuidOf;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr auto kWarningCases = std::to_array<FlightDataCase>({
    // BEGIN GENERATED: warning
    {.name     = "fd: warnings",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA">
    <id>cccccccc-0000-0000-0000-000000000001</id>
    <description>Large angle of attack encountered (20.1&#176;).</description>
    <priority>LOW</priority>
    <parameter>0.35</parameter>
    Large angle of attack encountered (20.1&#176;).
  </warning>
  <warning type="LargeAOA">
    <parameter>0.5</parameter>
  </warning>
  <warning type="HighSpeedDeployment">
    <id>cccccccc-0000-0000-0000-000000000002</id>
    <description>Recovery device deployment at high speed (26.9 m/s)</description>
    <priority>NORMAL</priority>
    <source>aaaaaaaa-0000-0000-0000-000000000004</source>
    <parameter>26.9</parameter>
  </warning>
  <warning type="HighSpeedDeployment">
    <source>aaaaaaaa-0000-0000-0000-000000000004</source>
    <parameter>30</parameter>
  </warning>
  <warning type="HighSpeedDeployment">
    <source>aaaaaaaa-0000-0000-0000-000000000099</source>
  </warning>
  <warning type="RecoveryHighSpeedDeployment">
    <description>Recovery device deployment at high speed (26.9 m/s)</description>
    <priority>HIGH</priority>
    <parameter>26.9</parameter>
  </warning>
  <warning type="EventAfterLanding">
    <id>cccccccc-0000-0000-0000-000000000003</id>
    <priority>bogus</priority>
  </warning>
  <warning>
    only text content
  </warning>
  <warning type="Other">
    <description>  Described  </description>
    <priority>HIGH</priority>
    <bogus>x</bogus>
    trailing text
  </warning>
  <warning type="Other">
    <description>Described</description>
    <priority>HIGH</priority>
  </warning>
  <warning type="MissingMotor">
    <description>No motor with designation 'X' for manufacturer 'Y' found.</description>
  </warning>
  <warning type="Other"/>
  <databranch name="A" types="time,altitude">
    <datapoint>0,0</datapoint>
    <event time="1" type="groundhit" id="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000009"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003" eventid="bbbbbbbb-0000-0000-0000-000000000077"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  closed=flightdata {} []
  data: branches=1 maxAlt=0.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=0.0 tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,LOW] id=cccccccc-0000-0000-0000-000000000001 text='Large angle of attack encountered (20.1<U+00B0>)' desc='Large angle of attack encountered (20.1<U+00B0>)' sources= param=0.35
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000002 text='Recovery device deployment at high speed (26.9 m/s):  "P"' desc='Recovery device deployment at high speed (26.9 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004; param=26.9
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed:  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed' sources=<i>Component Removed From Rocket</i>(REMOVED); param=NaN
    fw[Other,HIGH] id=(random) text='Recovery device deployment at high speed (26.9 m/s)' desc='Recovery device deployment at high speed (26.9 m/s)' sources=
    fw[EventAfterLanding,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    fw[Other,NORMAL] id=(random) text='only text content' desc='only text content' sources=
    fw[Other,HIGH] id=(random) text='Described' desc='Described' sources=
    fw[Other,NORMAL] id=(random) text='No motor with designation 'X' for manufacturer 'Y' found.' desc='No motor with designation 'X' for manufacturer 'Y' found.' sources=
    fw[Other,NORMAL] id=(random) text='' desc='' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m}
      event GROUND_HIT t=1.0 id=bbbbbbbb-0000-0000-0000-000000000009 src=null data=[null]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      event SIM_WARN t=2.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      row: 0.0 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning parameter garbage",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA">
    <parameter>abc</parameter>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "abc"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning parameter NaN text and Inf",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA">
    <parameter>NaN</parameter>
  </warning>
  <warning type="HighSpeedDeployment">
    <parameter>Infinity</parameter>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered.' desc='Large angle of attack encountered.' sources= param=NaN
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed (<U+221E> m/s)' desc='Recovery device deployment at high speed (<U+221E> m/s)' sources= param=Infinity
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning parameter OpenRocket Inf spelling",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="HighSpeedDeployment">
    <parameter>Inf</parameter>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "Inf"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning id garbage",
     .setup    = "@uuid [not-a-uuid]\n",
     .xml      = R"xml(
<flightdata>
  <warning type="Other">
    <id>not-a-uuid</id>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd: warning source garbage",
     .setup    = "@uuid [not-a-uuid]\n",
     .xml      = R"xml(
<flightdata>
  <warning type="Other">
    <source>not-a-uuid</source>
  </warning>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "fd2: warnings only",
     .setup    = "",
     .xml      = R"xml(
<flightdata maxaltitude="5">
  <warning type="Other"><description>W1</description><priority>NORMAL </priority></warning>
  <warning type="Other"><description>W2</description><priority>high</priority></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {maxaltitude=5} []
  data: branches=0 maxAlt=5.0 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=(random) text='W1' desc='W1' sources=
    fw[Other,NORMAL] id=(random) text='W2' desc='W2' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: empty element",
     .setup    = "",
     .xml      = R"xml(
<warning/>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='' desc='' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: only text, no type",
     .setup    = "",
     .xml      = R"xml(
<warning>
  Large angle of attack encountered (20.1 deg).
</warning>
)xml",
     .java     = R"out(
  closed=warning {} [Large angle of attack encountered (20.1 deg).]
  set: 1
    fw[Other,NORMAL] id=(random) text='Large angle of attack encountered (20.1 deg).' desc='Large angle of attack encountered (20.1 deg).' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: text in two lines keeps what is between them",
     .setup    = "",
     .xml      = R"xml(
<warning type="Other">  first line
   second line  </warning>
)xml",
     .java     = R"out(
  closed=warning {type=Other} [first line<LF>   second line]
  set: 1
    fw[Other,NORMAL] id=(random) text='first line<LF>   second line' desc='first line<LF>   second line' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: text around the children is joined",
     .setup    = "",
     .xml      = R"xml(
<warning type="Other">before<priority>HIGH</priority>after</warning>
)xml",
     .java     = R"out(
  closed=warning {type=Other} [beforeafter]
  set: 1
    fw[Other,HIGH] id=(random) text='beforeafter' desc='beforeafter' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: every child as the saver writes it",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA">
  <id>cccccccc-0000-0000-0000-000000000001</id>
  <description>Large angle of attack encountered (20.1 deg)</description>
  <priority>LOW</priority>
  <parameter>0.35</parameter>
  Large angle of attack encountered (20.1 deg)
</warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} [Large angle of attack encountered (20.1 deg)]
  set: 1
    fw[LargeAOA,LOW] id=cccccccc-0000-0000-0000-000000000001 text='Large angle of attack encountered (20.1<U+00B0>)' desc='Large angle of attack encountered (20.1<U+00B0>)' sources= param=0.35
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: large aoa without priority is normal",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>0.5</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} []
  set: 1
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: large aoa without parameter",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><description>ignored text</description></warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} []
  set: 1
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered.' desc='Large angle of attack encountered.' sources= param=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: high speed deployment with its source",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedDeployment">
  <id>cccccccc-0000-0000-0000-000000000002</id>
  <description>Recovery device deployment at high speed (26.9 m/s)</description>
  <priority>NORMAL</priority>
  <source>aaaaaaaa-0000-0000-0000-000000000004</source>
  <parameter>26.943764412970157</parameter>
</warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedDeployment} []
  set: 1
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000002 text='Recovery device deployment at high speed (26.9 m/s):  "P"' desc='Recovery device deployment at high speed (26.9 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004; param=26.943764412970157
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: the type name this OpenRocket writes is an other",
     .setup    = "",
     .xml      = R"xml(
<warning type="RecoveryHighSpeedDeployment">
  <description>Recovery device deployment at high speed (26.9 m/s)</description>
  <priority>NORMAL</priority>
  <source>aaaaaaaa-0000-0000-0000-000000000004</source>
  <parameter>26.9</parameter>
</warning>
)xml",
     .java     = R"out(
  closed=warning {type=RecoveryHighSpeedDeployment} []
  set: 1
    fw[Other,NORMAL] id=(random) text='Recovery device deployment at high speed (26.9 m/s):  "P"' desc='Recovery device deployment at high speed (26.9 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004;
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: event after landing",
     .setup    = "",
     .xml      = R"xml(
<warning type="EventAfterLanding">
  <id>cccccccc-0000-0000-0000-000000000003</id>
  <description>Flight Event occurred after landing: Apogee</description>
  <priority>HIGH</priority>
</warning>
)xml",
     .java     = R"out(
  closed=warning {type=EventAfterLanding} []
  set: 1
    fw[EventAfterLanding,HIGH] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: event after landing without priority is normal",
     .setup    = "",
     .xml      = R"xml(
<warning type="EventAfterLanding"/>
)xml",
     .java     = R"out(
  closed=warning {type=EventAfterLanding} []
  set: 1
    fw[EventAfterLanding,NORMAL] id=(random) text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: type names are compared exactly",
     .setup    = "",
     .xml      = R"xml(
<warning type="largeaoa"><description>d</description><parameter>0.5</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=largeaoa} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: type name padded",
     .setup    = "",
     .xml      = R"xml(
<warning type=" LargeAOA "><description>d</description><parameter>0.5</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type= LargeAOA } []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: other warning classes are others",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedMainDeployment"><description>Main deployment at high speed (30 m/s)</description><parameter>30</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedMainDeployment} []
  set: 1
    fw[Other,NORMAL] id=(random) text='Main deployment at high speed (30 m/s)' desc='Main deployment at high speed (30 m/s)' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: missing motor is an other",
     .setup    = "",
     .xml      = R"xml(
<warning type="MissingMotor"><description>No motor with designation 'X' found.</description><priority>HIGH</priority></warning>
)xml",
     .java     = R"out(
  closed=warning {type=MissingMotor} []
  set: 1
    fw[Other,HIGH] id=(random) text='No motor with designation 'X' found.' desc='No motor with designation 'X' found.' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: priority low",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><priority>LOW</priority></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,LOW] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: priority in lower case",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><priority>low</priority></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: priority padded",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><priority> HIGH </priority></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: priority empty",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><priority></priority></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: priority twice",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><priority>HIGH</priority><priority>LOW</priority></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,LOW] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: description twice",
     .setup    = "",
     .xml      = R"xml(
<warning><description>one</description><description>two</description></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='two' desc='two' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: empty description beats the text",
     .setup    = "",
     .xml      = R"xml(
<warning><description></description>the text</warning>
)xml",
     .java     = R"out(
  closed=warning {} [the text]
  set: 1
    fw[Other,NORMAL] id=(random) text='' desc='' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: description padded",
     .setup    = "",
     .xml      = R"xml(
<warning><description>   padded   text   </description></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='padded   text' desc='padded   text' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: description with a child element",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><description>a<b>x</b>c</description><parameter>0.5</parameter></warning>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element b, ignoring.
  closed=warning {} [a]
  set: 1
    fw[Other,NORMAL] id=(random) text='c' desc='c' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id lenient",
     .setup    = "",
     .xml      = R"xml(
<warning><id>1-2-3-4-5</id><description>d</description></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=00000001-0002-0003-0004-000000000005 text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id twice",
     .setup    = "",
     .xml      = R"xml(
<warning><id>cccccccc-0000-0000-0000-000000000001</id><id>cccccccc-0000-0000-0000-000000000002</id></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000002 text='' desc='' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id padded",
     .setup    = "@uuid [ cccccccc-0000-0000-0000-000000000001 ]\n",
     .xml      = R"xml(
<warning><id> cccccccc-0000-0000-0000-000000000001 </id></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [UUID string too large]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id empty",
     .setup    = "@uuid []\n",
     .xml      = R"xml(
<warning><id></id></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: ]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id garbage",
     .setup    = "@uuid [not-a-uuid]\n",
     .xml      = R"xml(
<warning><id>not-a-uuid</id></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: id with attributes",
     .setup    = "",
     .xml      = R"xml(
<warning><id foo="1">cccccccc-0000-0000-0000-000000000001</id></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='' desc='' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source of every component",
     .setup    = "",
     .xml      = R"xml(
<warning>
  <description>d</description>
  <source>aaaaaaaa-0000-0000-0000-000000000001</source>
  <source>aaaaaaaa-0000-0000-0000-000000000002</source>
  <source>aaaaaaaa-0000-0000-0000-000000000003</source>
  <source>aaaaaaaa-0000-0000-0000-000000000004</source>
</warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d:  "R", "S", "B", "P"' desc='d' sources=R@aaaaaaaa-0000-0000-0000-000000000001;S@aaaaaaaa-0000-0000-0000-000000000002;B@aaaaaaaa-0000-0000-0000-000000000003;P@aaaaaaaa-0000-0000-0000-000000000004;
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: the same source twice",
     .setup    = "",
     .xml      = R"xml(
<warning>
  <description>d</description>
  <source>aaaaaaaa-0000-0000-0000-000000000004</source>
  <source>aaaaaaaa-0000-0000-0000-000000000004</source>
</warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d:  "P", "P"' desc='d' sources=P@aaaaaaaa-0000-0000-0000-000000000004;P@aaaaaaaa-0000-0000-0000-000000000004;
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source that is not in the rocket",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedDeployment">
  <source>aaaaaaaa-0000-0000-0000-000000000099</source>
  <parameter>30</parameter>
</warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedDeployment} []
  set: 1
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed (30 m/s):  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed (30 m/s)' sources=<i>Component Removed From Rocket</i>(REMOVED); param=30.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: sources in the rocket and not",
     .setup    = "",
     .xml      = R"xml(
<warning>
  <description>d</description>
  <source>aaaaaaaa-0000-0000-0000-000000000099</source>
  <source>aaaaaaaa-0000-0000-0000-000000000003</source>
  <source>aaaaaaaa-0000-0000-0000-000000000098</source>
</warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d:  "<i>Component Removed From Rocket</i>", "B", "<i>Component Removed From Rocket</i>"' desc='d' sources=<i>Component Removed From Rocket</i>(REMOVED);B@aaaaaaaa-0000-0000-0000-000000000003;<i>Component Removed From Rocket</i>(REMOVED);
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source lenient",
     .setup    = "",
     .xml      = R"xml(
<warning><description>d</description><source>aaaaaaaa-0-0-0-4</source></warning>
)xml",
     .java     = R"out(
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d:  "P"' desc='d' sources=P@aaaaaaaa-0000-0000-0000-000000000004;
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source empty",
     .setup    = "@uuid []\n",
     .xml      = R"xml(
<warning><source></source></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: ]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source padded",
     .setup    = "@uuid [ aaaaaaaa-0000-0000-0000-000000000004]\n",
     .xml      = R"xml(
<warning><source> aaaaaaaa-0000-0000-0000-000000000004</source></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [UUID string too large]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: source garbage",
     .setup    = "@uuid [not-a-uuid]\n",
     .xml      = R"xml(
<warning><source>not-a-uuid</source></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Invalid UUID string: not-a-uuid]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter padded",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>  0.25  </parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} []
  set: 1
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (14.3<U+00B0>)' desc='Large angle of attack encountered (14.3<U+00B0>)' sources= param=0.25
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter twice",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>0.25</parameter><parameter>0.5</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} []
  set: 1
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter NaN",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedDeployment"><parameter>NaN</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedDeployment} []
  set: 1
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed' desc='Recovery device deployment at high speed' sources= param=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter Infinity",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>-Infinity</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=LargeAOA} []
  set: 1
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (-<U+221E><U+00B0>)' desc='Large angle of attack encountered (-<U+221E><U+00B0>)' sources= param=-Infinity
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter hexadecimal and suffix",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedDeployment"><parameter>0x1p3d</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedDeployment} []
  set: 1
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed (8 m/s)' desc='Recovery device deployment at high speed (8 m/s)' sources= param=8.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter beyond the range",
     .setup    = "",
     .xml      = R"xml(
<warning type="HighSpeedDeployment"><parameter>1e400</parameter></warning>
)xml",
     .java     = R"out(
  closed=warning {type=HighSpeedDeployment} []
  set: 1
    fw[RecoveryHighSpeedDeployment,NORMAL] id=(random) text='Recovery device deployment at high speed (<U+221E> m/s)' desc='Recovery device deployment at high speed (<U+221E> m/s)' sources= param=Infinity
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter garbage",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>abc</parameter></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "abc"]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter garbage for an other",
     .setup    = "",
     .xml      = R"xml(
<warning type="Other"><description>d</description><parameter>abc</parameter></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "abc"]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter empty",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter></parameter></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [empty String]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter with two points",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>1.2.3</parameter></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [multiple points]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: parameter Inf",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><parameter>Inf</parameter></warning>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "Inf"]
  closed=(not closed)
  set: 0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: unknown child with text and attributes",
     .setup    = "",
     .xml      = R"xml(
<warning type="Other"><description>d</description><bogus a="1">text</bogus></warning>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  closed=warning {type=Other} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "w: unknown child with a child",
     .setup    = "",
     .xml      = R"xml(
<warning type="LargeAOA"><description>d</description><bogus><inner/></bogus></warning>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element inner, ignoring.
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  closed=warning {} []
  set: 1
    fw[Other,NORMAL] id=(random) text='d' desc='d' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two large aoa warnings, the larger second",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000001</id><parameter>0.25</parameter></warning>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000002</id><parameter>0.5</parameter></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000002"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[LargeAOA:Large angle of attack encountered (28.6<U+00B0>)]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two large aoa warnings, the smaller second",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000001</id><parameter>0.5</parameter></warning>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000002</id><parameter>0.25</parameter></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a large aoa without angle is replaced by one with",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000001</id></warning>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000002</id><parameter>0.25</parameter></warning>
  <warning type="LargeAOA"><id>cccccccc-0000-0000-0000-000000000003</id></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Large angle of attack encountered (14.3<U+00B0>)' desc='Large angle of attack encountered (14.3<U+00B0>)' sources= param=0.25
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: large aoa warnings of two priorities",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA"><priority>LOW</priority><parameter>0.25</parameter></warning>
  <warning type="LargeAOA"><priority>NORMAL</priority><parameter>0.5</parameter></warning>
  <warning type="LargeAOA"><priority>LOW</priority><parameter>0.75</parameter></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[LargeAOA,LOW] id=(random) text='Large angle of attack encountered (43<U+00B0>)' desc='Large angle of attack encountered (43<U+00B0>)' sources= param=0.75
    fw[LargeAOA,NORMAL] id=(random) text='Large angle of attack encountered (28.6<U+00B0>)' desc='Large angle of attack encountered (28.6<U+00B0>)' sources= param=0.5
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: others that differ in text, priority or sources",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning><description>same</description></warning>
  <warning><description>same</description></warning>
  <warning><description>same</description><priority>HIGH</priority></warning>
  <warning><description>same</description><source>aaaaaaaa-0000-0000-0000-000000000004</source></warning>
  <warning><description>same</description><source>aaaaaaaa-0000-0000-0000-000000000004</source></warning>
  <warning><description>same</description><source>aaaaaaaa-0000-0000-0000-000000000003</source></warning>
  <warning><description>Same</description></warning>
  <warning type="Whatever"><description>same</description></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=(random) text='same' desc='same' sources=
    fw[Other,HIGH] id=(random) text='same' desc='same' sources=
    fw[Other,NORMAL] id=(random) text='same:  "P"' desc='same' sources=P@aaaaaaaa-0000-0000-0000-000000000004;
    fw[Other,NORMAL] id=(random) text='same:  "B"' desc='same' sources=B@aaaaaaaa-0000-0000-0000-000000000003;
    fw[Other,NORMAL] id=(random) text='Same' desc='Same' sources=
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: high speed deployments are one per source list",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000001</id><source>aaaaaaaa-0000-0000-0000-000000000004</source><parameter>20</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000002</id><source>aaaaaaaa-0000-0000-0000-000000000004</source><parameter>30</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000003</id><parameter>40</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000004</id><source>aaaaaaaa-0000-0000-0000-000000000004</source><source>aaaaaaaa-0000-0000-0000-000000000003</source><parameter>50</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000005</id><source>aaaaaaaa-0000-0000-0000-000000000003</source><source>aaaaaaaa-0000-0000-0000-000000000004</source><parameter>60</parameter></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000001"/>
    <event time="2" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000002"/>
    <event time="3" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal parameters for FlightEvent: SIM_WARN events require Warning objects
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Recovery device deployment at high speed (20 m/s):  "P"' desc='Recovery device deployment at high speed (20 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004; param=20.0
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='Recovery device deployment at high speed (40 m/s)' desc='Recovery device deployment at high speed (40 m/s)' sources= param=40.0
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000004 text='Recovery device deployment at high speed (50 m/s):  "P", "B"' desc='Recovery device deployment at high speed (50 m/s)' sources=P@aaaaaaaa-0000-0000-0000-000000000004;B@aaaaaaaa-0000-0000-0000-000000000003; param=50.0
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000005 text='Recovery device deployment at high speed (60 m/s):  "B", "P"' desc='Recovery device deployment at high speed (60 m/s)' sources=B@aaaaaaaa-0000-0000-0000-000000000003;P@aaaaaaaa-0000-0000-0000-000000000004; param=60.0
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[RecoveryHighSpeedDeployment:Recovery device deployment at high speed (20 m/s):  "P"]
      event SIM_WARN t=3.0 id=(random) src=null data=[RecoveryHighSpeedDeployment:Recovery device deployment at high speed (40 m/s)]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two warnings with one source that is not in the rocket",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000001</id><source>aaaaaaaa-0000-0000-0000-000000000099</source><parameter>20</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000002</id><source>aaaaaaaa-0000-0000-0000-000000000099</source><parameter>30</parameter></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Recovery device deployment at high speed (20 m/s):  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed (20 m/s)' sources=<i>Component Removed From Rocket</i>(REMOVED); param=20.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two warnings with two sources that are not in the rocket",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000001</id><source>aaaaaaaa-0000-0000-0000-000000000098</source><parameter>20</parameter></warning>
  <warning type="HighSpeedDeployment"><id>cccccccc-0000-0000-0000-000000000002</id><source>aaaaaaaa-0000-0000-0000-000000000099</source><parameter>30</parameter></warning>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Recovery device deployment at high speed (20 m/s):  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed (20 m/s)' sources=<i>Component Removed From Rocket</i>(REMOVED); param=20.0
)out",
     .qtrocket = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000001 text='Recovery device deployment at high speed (20 m/s):  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed (20 m/s)' sources=<i>Component Removed From Rocket</i>(REMOVED); param=20.0
    fw[RecoveryHighSpeedDeployment,NORMAL] id=cccccccc-0000-0000-0000-000000000002 text='Recovery device deployment at high speed (30 m/s):  "<i>Component Removed From Rocket</i>"' desc='Recovery device deployment at high speed (30 m/s)' sources=<i>Component Removed From Rocket</i>(REMOVED); param=30.0
)out",
     .why = "A source id that names no component is kept (decision L9), so the two warnings have "
            "two sources and both stay. In OpenRocket both have its one removed component as their "
            "source and are one warning."},
    {.name     = "s2: two events after landing with one id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id><priority>HIGH</priority></warning>
  <warning type="EventAfterLanding"><id>cccccccc-0000-0000-0000-000000000003</id><priority>LOW</priority></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,HIGH] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    fw[EventAfterLanding,LOW] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      row: 0.0
)out",
     .qtrocket = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,HIGH] id=cccccccc-0000-0000-0000-000000000003 text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[EventAfterLanding:Flight Event occurred after landing: ]
      row: 0.0
)out",
     .why = "Two EventAfterLanding warnings with equal ids are equal here, so the second is not "
            "added. OpenRocket compares the two UUID objects by reference, which are never the "
            "same for two elements of a file (see Warning::EventAfterLanding)."},
    {.name     = "s2: events after landing without ids",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="EventAfterLanding"/>
  <warning type="EventAfterLanding"/>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=0 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[EventAfterLanding,NORMAL] id=(random) text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
    fw[EventAfterLanding,NORMAL] id=(random) text='Flight Event occurred after landing: ' desc='Flight Event occurred after landing: ' sources= event=null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: two warnings of other kinds with one id",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="Other"><id>cccccccc-0000-0000-0000-000000000003</id><description>first</description></warning>
  <warning type="Other"><id>cccccccc-0000-0000-0000-000000000003</id><description>second</description></warning>
  <databranch name="A" types="time">
    <datapoint>0</datapoint>
    <event time="1" type="simwarn" warnid="cccccccc-0000-0000-0000-000000000003"/>
  </databranch>
</flightdata>
)xml",
     .java     = R"out(
  closed=flightdata {} []
  data: branches=1 maxAlt=NaN maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=0.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='first' desc='first' sources=
    fw[Other,NORMAL] id=cccccccc-0000-0000-0000-000000000003 text='second' desc='second' sources=
    branch[0] 'A' rows=1 optAlt=NaN tOptAlt=NaN optDelay=NaN sepTime=NaN srcId=null
      types=Time{t,Time,time,s}
      event SIM_WARN t=1.0 id=(random) src=null data=[Other:first]
      row: 0.0
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s2: a warning with attributes and a child that fails",
     .setup    = "",
     .xml      = R"xml(
<flightdata>
  <warning type="LargeAOA" priority="HIGH"><parameter>x</parameter></warning>
</flightdata>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "x"]
  closed=(not closed)
  data: null
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: warning
});

/// A warning's text holds an angle or a speed in the default units.
class WarningElements : public ::testing::TestWithParam<FlightDataCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(WarningElements, LoadAsInOpenRocketButWhereStated)
{
    expectFlightDataCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, WarningElements, ::testing::ValuesIn(kWarningCases),
                         flightDataCaseTestName);

/// The one warning of @p set, or null when the set does not hold exactly one.
[[nodiscard]] const Warning* onlyWarning(const WarningSet& set)
{
    return set.size() == 1 ? &*set.begin() : nullptr;
}

/// Runs @p xml, a <warning> element, through a new WarningHandler that adds to @p set.
[[nodiscard]] HandlerRun runWarning(FlightDataFixture& fixture, WarningSet& set,
                                    std::string_view xml)
{
    WarningHandler handler(fixture.rocket(), set);
    return runHandler(handler, xml);
}

TEST(WarningHandler, EveryChildIsPlainText)
{
    FlightDataFixture     fixture;
    WarningSet            set;
    WarningSet            warnings;
    WarningHandler        handler(fixture.rocket(), set);
    ElementHandler* const plainText = &PlainTextHandler::instance();
    EXPECT_EQ(handler.openElement("id", {}, warnings).value_or(nullptr), plainText);
    EXPECT_EQ(handler.openElement("bogus", {{"a", "1"}}, warnings).value_or(nullptr), plainText);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(set.empty());
}

// The types a file can name, by the classes they become.
TEST(WarningHandler, TheTypeDecidesTheClass)
{
    const DefaultUnitsGuard units;
    FlightDataFixture       fixture;
    WarningSet              set;

    EXPECT_TRUE(
        runWarning(fixture, set, "<warning type='LargeAOA'><parameter>0.25</parameter></warning>")
            .result.has_value());
    const auto* aoa = dynamic_cast<const Warning::LargeAOA*>(onlyWarning(set));
    ASSERT_NE(aoa, nullptr);
    EXPECT_EQ(aoa->aoa(), 0.25);

    set.clear();
    EXPECT_TRUE(
        runWarning(fixture, set,
                   "<warning type='HighSpeedDeployment'><parameter>26.5</parameter></warning>")
            .result.has_value());
    const auto* speed = dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(onlyWarning(set));
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->speed(), 26.5);

    set.clear();
    EXPECT_TRUE(runWarning(fixture, set, "<warning type='EventAfterLanding'>text</warning>")
                    .result.has_value());
    const auto* landing = dynamic_cast<const Warning::EventAfterLanding*>(onlyWarning(set));
    ASSERT_NE(landing, nullptr);
    EXPECT_FALSE(landing->eventType().has_value());
    EXPECT_FALSE(landing->eventId().has_value());

    set.clear();
    EXPECT_TRUE(runWarning(fixture, set,
                           "<warning type='RecoveryHighSpeedDeployment'>"
                           "<description>stored text</description><parameter>26.5</parameter>"
                           "</warning>")
                    .result.has_value());
    const auto* other = dynamic_cast<const Warning::Other*>(onlyWarning(set));
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(other->description(), "stored text");
}

TEST(WarningHandler, AWarningWithoutParameterHasNoNumber)
{
    FlightDataFixture fixture;
    WarningSet        set;
    EXPECT_TRUE(runWarning(fixture, set, "<warning type='LargeAOA'/>").result.has_value());
    const auto* aoa = dynamic_cast<const Warning::LargeAOA*>(onlyWarning(set));
    ASSERT_NE(aoa, nullptr);
    EXPECT_TRUE(std::isnan(aoa->aoa()));
}

TEST(WarningHandler, TheIdIsTheOneOfTheFileOrARandomOne)
{
    FlightDataFixture fixture;
    WarningSet        set;
    EXPECT_TRUE(
        runWarning(fixture, set, "<warning><id>1-2-3-4-5</id>first</warning>").result.has_value());
    EXPECT_TRUE(runWarning(fixture, set, "<warning>second</warning>").result.has_value());
    EXPECT_TRUE(runWarning(fixture, set, "<warning>third</warning>").result.has_value());
    ASSERT_EQ(set.size(), 3U);

    std::vector<Uuid> ids;
    for (const Warning& warning : set)
    {
        ids.push_back(warning.id());
    }
    // java.util.UUID.fromString() takes the short groups.
    EXPECT_EQ(ids.at(0).toString(), "00000001-0002-0003-0004-000000000005");
    // Without an <id> every warning has an id of its own, as UUID.randomUUID() makes them.
    EXPECT_EQ(ids.at(1).version(), 4);
    EXPECT_EQ(ids.at(2).version(), 4);
    EXPECT_NE(ids.at(1), ids.at(2));
}

// Decision L9 of the loader: OpenRocket's source is its one removed component, whose id is not
// the one of the file.
TEST(WarningHandler, ASourceThatIsNotInTheRocketKeepsItsId)
{
    FlightDataFixture fixture;
    WarningSet        set;
    EXPECT_TRUE(runWarning(fixture, set,
                           "<warning><description>d</description>"
                           "<source>aaaaaaaa-0000-0000-0000-000000000099</source>"
                           "<source>aaaaaaaa-0000-0000-0000-000000000004</source></warning>")
                    .result.has_value());
    const Warning* warning = onlyWarning(set);
    ASSERT_NE(warning, nullptr);
    ASSERT_EQ(warning->sources().size(), 2U);
    EXPECT_EQ(warning->sources().at(0).id.toString(), "aaaaaaaa-0000-0000-0000-000000000099");
    EXPECT_EQ(warning->sources().at(0).name, MessageSource::kRemovedComponentName);
    EXPECT_EQ(warning->sources().at(0).name, "<i>Component Removed From Rocket</i>");
    EXPECT_EQ(warning->sources().at(1).id, uuidOf(kFlightChuteId));
    EXPECT_EQ(warning->sources().at(1).name, "P");
    EXPECT_EQ(warning->toString(), "d:  \"<i>Component Removed From Rocket</i>\", \"P\"");
}

// The name of a source is the component's when the file is read.
TEST(WarningHandler, ASourceHasTheNameOfItsComponent)
{
    FlightDataFixture fixture;
    fixture.chute().setName("Main chute");
    WarningSet set;
    EXPECT_TRUE(runWarning(fixture, set,
                           "<warning type='HighSpeedDeployment'>"
                           "<source>aaaaaaaa-0000-0000-0000-000000000004</source></warning>")
                    .result.has_value());
    const Warning* warning = onlyWarning(set);
    ASSERT_NE(warning, nullptr);
    EXPECT_EQ(warning->toString(), "Recovery device deployment at high speed:  \"Main chute\"");
}

TEST(WarningHandler, AParameterThatIsNoNumberFailsAndAddsNothing)
{
    FlightDataFixture fixture;
    WarningSet        set;
    const HandlerRun  run = runWarning(
        fixture, set,
        "<warning type='Other'><description>d</description><parameter> 1,5 </parameter></warning>");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(run.result.error().message, "For input string: \"1,5\"");
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(run.texts(), Texts{});
}

// The failure is the one of Uuid::javaFromString(), under the code of an argument a load fails
// for; the cases compare its text.
TEST(WarningHandler, AnIdOrASourceThatIsNoUuidFailsAndAddsNothing)
{
    FlightDataFixture fixture;
    WarningSet        set;
    const HandlerRun  id =
        runWarning(fixture, set, "<warning><id>no-uuid</id><description>d</description></warning>");
    ASSERT_FALSE(id.result.has_value());
    EXPECT_EQ(id.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_TRUE(id.result.error().message.starts_with("Invalid UUID string: "));
    EXPECT_TRUE(id.result.error().message.contains("no-uuid"));

    const HandlerRun source = runWarning(
        fixture, set, "<warning><description>d</description><source>no-uuid</source></warning>");
    ASSERT_FALSE(source.result.has_value());
    EXPECT_EQ(source.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(source.result.error().message, id.result.error().message);
    EXPECT_TRUE(set.empty());
}

// The set of the flight data handler holds one warning of a kind: a handler adds to what the
// handlers before it left there.
TEST(WarningHandler, AWarningEqualToOneOfTheSetIsNotAdded)
{
    const DefaultUnitsGuard units;
    FlightDataFixture       fixture;
    WarningSet              set;
    const std::string_view  first =
        "<warning type='LargeAOA'><id>cccccccc-0000-0000-0000-000000000001</id>"
        "<parameter>0.25</parameter></warning>";
    const std::string_view larger =
        "<warning type='LargeAOA'><id>cccccccc-0000-0000-0000-000000000002</id>"
        "<parameter>0.5</parameter></warning>";
    EXPECT_TRUE(runWarning(fixture, set, first).result.has_value());
    EXPECT_TRUE(runWarning(fixture, set, larger).result.has_value());
    EXPECT_TRUE(runWarning(fixture, set, first).result.has_value());

    // The first stays, with its id, and has taken the larger angle.
    const auto* aoa = dynamic_cast<const Warning::LargeAOA*>(onlyWarning(set));
    ASSERT_NE(aoa, nullptr);
    EXPECT_EQ(aoa->id().toString(), "cccccccc-0000-0000-0000-000000000001");
    EXPECT_EQ(aoa->aoa(), 0.5);
    EXPECT_EQ(aoa->priority(), MessagePriority::NORMAL);
    EXPECT_EQ(set.findById(uuidOf("cccccccc-0000-0000-0000-000000000002")), nullptr);
}

// The warnings of the load are not the warnings of the flight: an unknown child is reported to
// the first and changes nothing in the second.
TEST(WarningHandler, TheWarningsOfTheLoadAreApartFromTheSet)
{
    FlightDataFixture fixture;
    WarningSet        set;
    const HandlerRun  run =
        runWarning(fixture, set, "<warning><description>d</description><bogus/></warning>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{"Unknown element 'bogus', ignoring."});
    EXPECT_EQ(warningTexts(set), Texts{"d"});
}

}  // namespace
