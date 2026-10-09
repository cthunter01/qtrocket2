#include "QtRocket/file/openrocket/MotorConfigurationHandler.h"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// MotorConfigurationHandler: the handler of a <motorconfiguration> element of the rocket. What a
// file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::ErrorCode;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::MotorConfigurationHandler;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::WarningSet;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES MotorConfigurationHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 34> kJava{{
    {.name = "mc-basic", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>Cfg</name><stage number="0" active="true"/><stage number="1" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=2, nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload={0=true, 1=false} active=[true,true] motors=0)out"},
    {.name = "mc-empty", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-without-configid", .xml = R"xml(<motorconfiguration default="true"><name>Cfg</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=random
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='Cfg' preload=null active=[true] motors=0)out"},
    {.name = "mc-empty-configid", .xml = R"xml(<motorconfiguration configid=""><name>Cfg</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='Cfg' preload=null active=[true] motors=0)out"},
    {.name = "mc-text-configids", .xml = R"xml(<motorconfiguration configid="abc"><name>one</name></motorconfiguration><motorconfiguration configid="def" default="true"></motorconfiguration><motorconfiguration configid="zzzzzzzzzz"><name>negative hash</name></motorconfiguration><motorconfiguration configid=" "><name>blank</name></motorconfiguration><motorconfiguration configid="1-2-3-4-5"><name>short</name></motorconfiguration><motorconfiguration configid="&#228;&#8364;&#119070;"><name>not ASCII</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass,aero,tree=1, nonfunc=1, tree=7}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=00000000-0000-0000-0000-000000018405
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000017862 name='one' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000018405 name='[{motors}]' preload=null active=[true] motors=0
| config 00000000-0000-0000-ffff-ffffa1c42c40 name='negative hash' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000000020 name='blank' preload=null active=[true] motors=0
| config 00000001-0002-0003-0004-000000000005 name='short' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000fd55b2 name='not ASCII' preload=null active=[true] motors=0)out"},
    {.name = "mc-upper-case-configid", .xml = R"xml(<motorconfiguration configid="AAAAAAAA-2222-3333-4444-555555555555" default="true"><name>Cfg</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=aaaaaaaa-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true] motors=0
| config aaaaaaaa-2222-3333-4444-555555555555 name='Cfg' preload=null active=[true] motors=0)out"},
    {.name = "mc-two-names", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>one</name><name>two</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {configid=11111111-2222-3333-4444-555555555555, default=true} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='one' preload=null active=[true] motors=0)out"},
    {.name = "mc-blank-name", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><name>  </name></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666"><name/></motorconfiguration><motorconfiguration configid="33333333-4444-5555-6666-777777777777"><name> padded </name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=0
| config 33333333-4444-5555-6666-777777777777 name=' padded ' preload=null active=[true] motors=0)out"},
    {.name = "mc-name-with-child", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>A<b/>B</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element b, ignoring.
W Unknown text in element 'motorconfiguration', ignoring.
ROOT rocket {configid=11111111-2222-3333-4444-555555555555, default=true} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='B' preload=null active=[true] motors=0)out"},
    {.name = "mc-name-is-the-default-name", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><name>[{motors}]</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-no-stage-active", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="1" active="false"/><stage number="2" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload={0=true, 1=false, 2=false} active=[true,true] motors=0)out"},
    {.name = "mc-no-stage-active-and-stage-0-said-so", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="0" active="false"/><stage number="1" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload={0=true, 1=false} active=[true,true] motors=0)out"},
    {.name = "mc-a-stage-twice", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="0" active="true"/><stage number="0" active="false"/><stage number="1" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload={0=false, 1=false} active=[true,true] motors=0)out"},
    {.name = "mc-active-forms", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="0" active="TRUE"/><stage number="1" active="True"/><stage number="2" active=" true"/><stage number="3" active="true "/><stage number="4" active="yes"/><stage number="5" active="1"/><stage number="6" active=""/><stage number="7"/><stage number="8" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload={0=true, 1=true, 2=false, 3=false, 4=false, 5=false, 6=false, 7=false, 8=false} active=[true] motors=0)out"},
    {.name = "mc-stage-numbers", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="-1" active="true"/><stage number="+2" active="true"/><stage number="007" active="false"/><stage number="2147483647" active="true"/><stage number="-2147483648" active="false"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload={-2147483648=false, -1=true, 2=true, 7=false, 2147483647=true} active=[true] motors=0)out"},
    {.name = "mc-stage-number-no-number", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="0" active="true"/><stage number="x" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-number-blank", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number=" 1" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: " 1"
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-number-fraction", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="1.0" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "1.0"
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-number-empty", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: ""
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-number-missing", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Cannot parse null string
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-number-too-large", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="2147483648" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "2147483648"
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-with-child", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="0" active="true"><x/></stage></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Cannot parse null string
W Unknown element x, ignoring.
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "mc-stage-text-and-attributes", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="1" active="true" extra="1">text</stage><name a="1">Cfg</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload={1=true} active=[true,true] motors=0)out"},
    {.name = "mc-default-forms", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="TRUE"></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666" default="true "></motorconfiguration><motorconfiguration configid="33333333-4444-5555-6666-777777777777" default=""></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=0
| config 33333333-4444-5555-6666-777777777777 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-default-twice", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666" default="true"></motorconfiguration><motorconfiguration configid="33333333-4444-5555-6666-777777777777" default="false"></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=1, nonfunc=2, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=22222222-3333-4444-5555-666666666666
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=0
| config 33333333-4444-5555-6666-777777777777 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-default-again", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666"></motorconfiguration><motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-text-and-attributes", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" extra="x" default="true">text</motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'motorconfiguration', ignoring.
W Unknown attributes in element 'motorconfiguration', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mc-flightconfiguration-text-and-attributes", .xml = R"xml(<flightconfiguration configid="11111111-2222-3333-4444-555555555555" extra="x">text<name>F</name></flightconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'flightconfiguration', ignoring.
W Unknown attributes in element 'flightconfiguration', ignoring.
W Unknown parameter type 'flightconfiguration' for Rocket, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='F' preload=null active=[true] motors=0)out"},
    {.name = "mc-same-id-twice", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><name>one</name><stage number="0" active="true"/><stage number="1" active="true"/></motorconfiguration><motorconfiguration configid="11111111-2222-3333-4444-555555555555"><name>two</name><stage number="1" active="false"/><stage number="2" active="true"/></motorconfiguration><motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="3" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='two' preload={0=true, 1=false, 2=true, 3=true} active=[true,true] motors=0)out"},
    {.name = "mc-unknown-child", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true" extra="x"><name>Cfg</name><stage number="0" active="false"/><bogus/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {configid=11111111-2222-3333-4444-555555555555, default=true, extra=x} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='Cfg' preload={0=true} active=[true] motors=0)out"},
    {.name = "mc-unknown-child-first", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><bogus b="1">t</bogus><name>Cfg</name><stage number="1" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
W Unknown attributes in element 'motorconfiguration', ignoring.
ROOT rocket {configid=11111111-2222-3333-4444-555555555555, default=true} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config random name='Cfg' preload={1=true} active=[true,true] motors=0)out"},
    {.name = "mc-behind-the-stages", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents><motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>Cfg</name><stage number="0" active="false"/><stage number="1" active="true"/></motorconfiguration>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=2, nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload={0=false, 1=true} active=[true,true] motors=0)out"},
    {.name = "mc-default-key", .xml = R"xml(<motorconfiguration configid="ffffffff-f4f2-f1f0-0000-00000000162c" default="true"><name>Cfg</name><stage number="0" active="false"/><stage number="1" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='Cfg' preload={0=false, 1=true} active=[true,true] motors=0)out"},
    {.name = "mc-many", .xml = R"xml(<motorconfiguration><name>c0</name><stage number="0" active="true"/></motorconfiguration><motorconfiguration><name>c1</name><stage number="1" active="true"/></motorconfiguration><motorconfiguration><name>c2</name><stage number="2" active="true"/></motorconfiguration><motorconfiguration><name>c3</name><stage number="0" active="true"/></motorconfiguration><motorconfiguration><name>c4</name><stage number="1" active="true"/></motorconfiguration><motorconfiguration><name>c5</name><stage number="2" active="true"/></motorconfiguration><motorconfiguration><name>c6</name><stage number="0" active="true"/></motorconfiguration><motorconfiguration><name>c7</name><stage number="1" active="true"/></motorconfiguration><motorconfiguration><name>c8</name><stage number="2" active="true"/></motorconfiguration><motorconfiguration><name>c9</name><stage number="0" active="true"/></motorconfiguration><motorconfiguration><name>c10</name><stage number="1" active="true"/></motorconfiguration><motorconfiguration><name>c11</name><stage number="2" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 14 {mass,aero,tree=1, tree=13}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='c0' preload={0=true} active=[true] motors=0
| config random name='c1' preload={1=true} active=[true] motors=0
| config random name='c2' preload={2=true} active=[true] motors=0
| config random name='c3' preload={0=true} active=[true] motors=0
| config random name='c4' preload={1=true} active=[true] motors=0
| config random name='c5' preload={2=true} active=[true] motors=0
| config random name='c6' preload={0=true} active=[true] motors=0
| config random name='c7' preload={1=true} active=[true] motors=0
| config random name='c8' preload={2=true} active=[true] motors=0
| config random name='c9' preload={0=true} active=[true] motors=0
| config random name='c10' preload={1=true} active=[true] motors=0
| config random name='c11' preload={2=true} active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 3> kOwn{{
    // A configid that spells out the error id's key is the error id here: the configuration is refused with the handler's warning for an id that is not valid.
    // OpenRocket: W Unknown attributes in element 'motorconfiguration', ignoring.
    // OpenRocket: EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
    // OpenRocket: | selected=ffffffff-f4f2-f1f0-0000-0000000009b9
    // OpenRocket: | config ffffffff-f4f2-f1f0-0000-0000000009b9 name='Cfg' preload={0=true} active=[true] motors=0
    {.name = "mc-error-key", .xml = R"xml(<motorconfiguration configid="ffffffff-f4f2-f1f0-0000-0000000009b9" default="true" extra="x"><name>Cfg</name><stage number="0" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The instance budget is over all flight configurations: a rocket of twelve refuses a launch lug of 10000 instances (OpenRocket takes it).
    // OpenRocket: EVENTS 16 {mass,aero=1, mass,aero,tree=2, tree=13}
    // OpenRocket: |       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=10000 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4 ...
    {.name = "mc-fix-instances-beyond-the-budget-of-its-configurations", .xml = R"xml(<motorconfiguration configid="00000001-0000-0000-0000-000000000001"/><motorconfiguration configid="00000002-0000-0000-0000-000000000001"/><motorconfiguration configid="00000003-0000-0000-0000-000000000001"/><motorconfiguration configid="00000004-0000-0000-0000-000000000001"/><motorconfiguration configid="00000005-0000-0000-0000-000000000001"/><motorconfiguration configid="00000006-0000-0000-0000-000000000001"/><motorconfiguration configid="00000007-0000-0000-0000-000000000001"/><motorconfiguration configid="00000008-0000-0000-0000-000000000001"/><motorconfiguration configid="00000009-0000-0000-0000-000000000001"/><motorconfiguration configid="0000000a-0000-0000-0000-000000000001"/><motorconfiguration configid="0000000b-0000-0000-0000-000000000001"/><motorconfiguration configid="0000000c-0000-0000-0000-000000000001"/><subcomponents><stage><subcomponents><bodytube><subcomponents><launchlug><instancecount>10000</instancecount></launchlug></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 15 {mass,aero,tree=2, tree=13}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 00000001-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000002-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000003-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000004-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000005-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000006-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000007-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000008-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 00000009-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 0000000a-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 0000000b-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0
| config 0000000c-0000-0000-0000-000000000001 name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Uuid::javaFromString() reads ASCII digits only: a configid with a fullwidth digit is no UUID, so the id is made of the text's hash code and is another configuration than its ASCII spelling (one configuration in OpenRocket).
    // OpenRocket: EVENTS 3 {nonfunc=1, tree=2}
    {.name = "mc-fix-configid-with-other-digits", .xml = R"xml(<motorconfiguration configid="&#65297;-2-3-4-5"><name>n</name></motorconfiguration><motorconfiguration configid="1-2-3-4-5" default="true"><name>ascii</name></motorconfiguration><subcomponents><stage/></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=00000001-0002-0003-0004-000000000005
| config default name='[{motors}]' preload=null active=[false] motors=0
| config 00000000-0000-0000-ffff-ffffea57d42b name='n' preload=null active=[false] motors=0
| config 00000001-0002-0003-0004-000000000005 name='ascii' preload=null active=[false] motors=0)out"},
}};
// END GENERATED TABLES MotorConfigurationHandler
// clang-format on

TEST(MotorConfigurationHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(MotorConfigurationHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(MotorConfigurationHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(MotorConfigurationHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

TEST(MotorConfigurationHandler, TakesANameOnceAndAnyNumberOfStages)
{
    RocketLoadFixture         fixture;
    MotorConfigurationHandler handler(fixture.rocket(), fixture.context());
    WarningSet                warnings;

    EXPECT_EQ(handler.openElement("stage", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("name", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("stage", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_TRUE(warnings.empty());

    // A second name and anything else: ignored (null), each with the warning.
    EXPECT_EQ(handler.openElement("name", {}, warnings).value_or(&handler), nullptr);
    EXPECT_EQ(warningTexts(warnings), Texts{"Invalid parameter encountered, ignoring."});
    WarningSet others;
    EXPECT_EQ(handler.openElement("bogus", {}, others).value_or(&handler), nullptr);
    EXPECT_EQ(handler.openElement("Stage", {}, others).value_or(&handler), nullptr);
    EXPECT_EQ(warningTexts(others), Texts{"Invalid parameter encountered, ignoring."});
}

// A stage number that is no int ends the load, with the message of Java's
// NumberFormatException under the code the top-level loader turns into "Exception loading
// stream: ...".
TEST(MotorConfigurationHandler, FailsTheLoadForAStageNumberThatIsNone)
{
    RocketLoadFixture         fixture;
    MotorConfigurationHandler handler(fixture.rocket(), fixture.context());
    WarningSet                warnings;

    const Result<void> bad = handler.closeElement("stage", {{"number", "x"}}, "", warnings);
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(bad.error().message, "For input string: \"x\"");

    const Result<void> missing = handler.closeElement("stage", {{"active", "true"}}, "", warnings);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(missing.error().message, "Cannot parse null string");
    EXPECT_TRUE(warnings.empty());
    // Nothing was made: the configuration is made when the element closes.
    EXPECT_EQ(fixture.rocket().getFlightConfigurationCount(), 0);
}

constexpr std::string_view kTwoStages =
    R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><stage number="0" active="false"/><stage number="1" active="true"/></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666"><stage number="0" active="true"/><stage number="1" active="false"/><stage number="-1" active="true"/><stage number="2147483647" active="true"/></motorconfiguration><subcomponents><stage><subcomponents><bodytube/></subcomponents></stage><stage><subcomponents><bodytube/></subcomponents></stage></subcomponents>)xml";

/// "true,false" for the two stages of @p configuration.
[[nodiscard]] std::string activeStages(const FlightConfiguration& configuration)
{
    return std::string(configuration.isStageActive(0) ? "true" : "false") + "," +
           (configuration.isStageActive(1) ? "true" : "false");
}

// The handler only hands the stage entries to the configuration: the stages do not exist while
// the <motorconfiguration> elements are read. OpenRocketLoader applies them when the file has
// been read; this is that step, and a stage number no stage has does no harm in it.
TEST(MotorConfigurationHandler, LeavesTheStageEntriesForTheEndOfTheLoad)
{
    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(kTwoStages);
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    Rocket&                     rocket = fixture.rocket();
    const FlightConfigurationId first =
        FlightConfigurationId::fromString("11111111-2222-3333-4444-555555555555");
    const FlightConfigurationId second =
        FlightConfigurationId::fromString("22222222-3333-4444-5555-666666666666");
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), first);
    // As read: both stages are active in both configurations, as in any new one.
    EXPECT_EQ(activeStages(rocket.getFlightConfiguration(first)), "true,true");
    EXPECT_EQ(activeStages(rocket.getFlightConfiguration(second)), "true,true");
    ASSERT_TRUE(rocket.getFlightConfiguration(second).getPreloadedStageActiveness().has_value());

    rocket.getFlightConfiguration(first).applyPreloadedStageActiveness();
    rocket.getFlightConfiguration(second).applyPreloadedStageActiveness();
    EXPECT_EQ(activeStages(rocket.getFlightConfiguration(first)), "false,true");
    EXPECT_EQ(activeStages(rocket.getFlightConfiguration(second)), "true,false");
    EXPECT_FALSE(rocket.getFlightConfiguration(second).getPreloadedStageActiveness().has_value());
}

}  // namespace
