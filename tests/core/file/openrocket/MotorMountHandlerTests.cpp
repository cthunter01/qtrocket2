#include "QtRocket/file/openrocket/MotorMountHandler.h"

#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "document/DocumentTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "motor/TestMotorDatabase.h"

// MotorMountHandler: the handler of a <motormount> element. What a file's text gives is
// compared with what OpenRocket makes of the same text, read through the handler of the rocket
// element as a file is (RocketLoadFixture).

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::DatabaseMotorFinder;
using QtRocket::ElementHandler;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMountHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::RocketEventRecorder;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES MotorMountHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 40> kJava{{
    {.name = "mm-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-inner-tube", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><innertube><motormount><overhang>0.004</overhang><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></innertube></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 8 {mass,aero=3, mass,aero,tree=1, mass,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.9299999999999999 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.004 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-defaults", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent>ejectioncharge</ignitionevent><ignitiondelay>1.5</ignitiondelay><overhang>0.01</overhang><bogus/></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element 'bogus' encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=EJECTION_CHARGE:1.5:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-bad-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent> ejectioncharge </ignitionevent><ignitiondelay>x</ignitiondelay><overhang>y</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type ' ejectioncharge ', ignoring.
W Illegal ignition delay specified, ignoring.
W Illegal overhang specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-number-forms", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay> 1e-1d </ignitiondelay><overhang>0x1p-3</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.125 ign=AUTOMATIC:0.1:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-empty-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent/><ignitiondelay/><overhang/></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type '', ignoring.
W Illegal ignition delay specified, ignoring.
W Illegal overhang specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-ignition-events", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent>automatic</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>launch</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>ejectioncharge</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>burnout</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>never</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>BURNOUT</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>ejection_charge</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>EJECTION_CHARGE</ignitionevent></motormount></bodytube><bodytube><motormount><ignitionevent>burnout </ignitionevent></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type 'BURNOUT', ignoring.
W Unknown ignition event type 'ejection_charge', ignoring.
W Unknown ignition event type 'EJECTION_CHARGE', ignoring.
W Unknown ignition event type 'burnout ', ignoring.
ROOT rocket {} []
EVENTS 19 {mass,aero,tree=9, motor=9, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.7999999999999998 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=LAUNCH:0.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=EJECTION_CHARGE:0.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:0.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.8 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=NEVER:0.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.5999999999999999 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-negative-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>-2</ignitiondelay><overhang>-0.01</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=-0.01 ign=AUTOMATIC:-2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-without-designation", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><manufacturer>Estes</manufacturer><delay>3</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=none:3.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-without-anything", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Motor delay not specified, assuming no ejection charge.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=none:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-without-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[random]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-empty-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid=""><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[random]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-text-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="abc"><designation>C6</designation><delay>5.0</delay></motor><motor configid="zzzzzzzzzz"><designation>C6</designation><delay>5.0</delay></motor><motor configid=" "><designation>C6</designation><delay>5.0</delay></motor><motor configid="1-2-3-4-5"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=1, motor=1, tree=5}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[00000000-0000-0000-0000-000000017862]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[00000000-0000-0000-ffff-ffffa1c42c40]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[00000000-0000-0000-0000-000000000020]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[00000001-0002-0003-0004-000000000005]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000017862 name='[{motors}]' preload=null active=[true] motors=1
| config 00000000-0000-0000-ffff-ffffa1c42c40 name='[{motors}]' preload=null active=[true] motors=1
| config 00000000-0000-0000-0000-000000000020 name='[{motors}]' preload=null active=[true] motors=1
| config 00000001-0002-0003-0004-000000000005 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-non-ascii-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="&#228;&#8364;&#119070;"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[00000000-0000-0000-0000-000000fd55b2]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000fd55b2 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-upper-case-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="AAAAAAAA-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[aaaaaaaa-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config aaaaaaaa-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-twice", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><motor configid="11111111-2222-3333-4444-555555555555"><designation>D12</designation><delay>7.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:7.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motor-then-none", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><motor configid="11111111-2222-3333-4444-555555555555"><delay>7.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=none:7.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-two-motors", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor><motor configid="11111111-2222-3333-4444-555555555555"><designation>D12</designation><delay>none</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-default-ignition-is-inherited", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionevent>never</ignitionevent><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=NEVER:2.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:NEVER:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-ignition-of-a-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-ignition-without-a-motor-changes-the-default", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:2.0:true motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:BURNOUT:2.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-ignition-in-another-mount", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube><bodytube><motormount><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=2, motor=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.4 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:2.0:true motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-ignition-without-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:2.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-nozzle", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5</delay><nozzleexitdiameter>0.01</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.01:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-nozzle-too-large", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5</delay><nozzleexitdiameter>0.03</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid nozzle exit diameter, assuming unknown: Nozzle exit diameter must not exceed the motor diameter
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-nozzle-without-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><delay>5</delay><nozzleexitdiameter>0.03</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=none:5.0:0.03:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-nozzle-negative", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5</delay><nozzleexitdiameter>-0.01</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal nozzle exit diameter specified, assuming unknown.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-warnings-in-order", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>W1</designation><nozzleexitdiameter>0.03</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W finder: W1
W Motor delay not specified, assuming no ejection charge.
W Invalid nozzle exit diameter, assuming unknown: Nozzle exit diameter must not exceed the motor diameter
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-delay-forms", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>none</delay></motor><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>Infinity</delay></motor><motor configid="33333333-4444-5555-6666-777777777777"><designation>C6</designation><delay> 4 </delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=1, motor=1, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:Infinity:0.0:AUTOMATIC:0.0:false motor[22222222-3333-4444-5555-666666666666]=F12X:Infinity:0.0:AUTOMATIC:0.0:false motor[33333333-4444-5555-6666-777777777777]=F12X:4.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1
| config 33333333-4444-5555-6666-777777777777 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-delay-unreadable", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>x</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal motor delay specified, ignoring.
W Motor delay not specified, assuming no ejection charge.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-unknown-elements", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount m="1"><bogus a="1">t<x/></bogus><Motor/><overhang>0.01</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element 'bogus' encountered, ignoring.
W Unknown element 'Motor' encountered, ignoring.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount a="1">text<overhang b="2">0.01</overhang><ignitionevent c="3">never</ignitionevent><motor d="4" configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=1, mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=NEVER:0.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:NEVER:0.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-child-in-a-value", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>0.01<x/>0.02</overhang><ignitiondelay>3<y/></ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Unknown element y, ignoring.
W Illegal ignition delay specified, ignoring.
W Unknown text in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.02 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "mm-child-in-a-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><manufacturer>Estes</manufacturer><delay>3</delay><foo><bar/></foo></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[random]=F12X:3.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-default-key-ignition", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="ffffffff-f4f2-f1f0-0000-00000000162c"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:2.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-two-mounts-one-configuration", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount><subcomponents><innertube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></innertube></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=1, mass,tree=1, motor=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=2
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-motormount-twice", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>0.01</overhang><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount><motormount><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=1, mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "mm-configuration-made-before", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><name>Cfg</name></motorconfiguration><subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload=null active=[true] motors=1)out"},
    {.name = "mm-huge-overhang", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>1e300</overhang><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=1, mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=1.0E300 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 14> kOwn{{
    // Decision L3: an ignition delay of NaN is refused (OpenRocket stores it) and the delay stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-nan-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>2</ignitiondelay><ignitiondelay>NaN</ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an infinite ignition delay is refused (OpenRocket stores it) and the delay stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-infinite-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>2</ignitiondelay><ignitiondelay>Infinity</ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an infinite ignition delay is refused (OpenRocket stores it).
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-negative-infinite-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>-Infinity</ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a number too large for a double is an infinity and refused (OpenRocket stores it).
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-overflowing-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>1e400</ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an overhang of NaN is refused (OpenRocket stores it) and the overhang stays.
    // OpenRocket: EVENTS 5 {mass,aero=2, mass,aero,tree=1, motor=1, tree=1}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-nan-overhang", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>0.01</overhang><overhang>NaN</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal overhang specified, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an infinite overhang is refused (OpenRocket stores it) and the overhang stays.
    // OpenRocket: EVENTS 5 {mass,aero=2, mass,aero,tree=1, motor=1, tree=1}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-infinite-overhang", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>0.01</overhang><overhang>-Infinity</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal overhang specified, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: without a delay OpenRocket dies of a NullPointerException; here the event is applied and a warning added.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "java.lang.Double.doubleValue()" because "this.ignitionConfigHandler.ignitionDelay" is nul ...
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-ignition-event-only", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:0.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    // Without an event OpenRocket stores null as the configuration's ignition event; here the event stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-ignition-delay-only", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    // Decision L4: without a delay OpenRocket dies of a NullPointerException; here a warning is added.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "java.lang.Double.doubleValue()" because "this.ignitionConfigHandler.ignitionDelay" is nul ...
    {.name = "mm-ignition-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    // Decision L4: without a delay it could read OpenRocket dies of a NullPointerException; here a third warning is added.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "java.lang.Double.doubleValue()" because "this.ignitionConfigHandler.ignitionDelay" is nul ...
    {.name = "mm-ignition-unreadable", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>bogus</ignitionevent><ignitiondelay>x</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type 'bogus', ignoring.
W Illegal ignition delay specified, ignoring.
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    // MotorHandler (run 9a, decision U3): an ejection delay of negative infinity is refused and the motor is plugged; OpenRocket stores it.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-delay-negative-infinity", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>-Infinity</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal motor delay specified, ignoring.
W Motor delay not specified, assuming no ejection charge.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
    // A configid that spells out the error id's key is the error id here: the motor is refused. In OpenRocket no id made from a text is the error id.
    // OpenRocket: EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: | config ffffffff-f4f2-f1f0-0000-0000000009b9 name='[{motors}]' preload=null active=[true] motors=1
    {.name = "mm-error-key-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="ffffffff-f4f2-f1f0-0000-0000000009b9"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal motor specification, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // A configid that spells out the error id's key is the error id here: the element is refused. In OpenRocket no id made from a text is the error id.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "mm-error-key-ignition", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="ffffffff-f4f2-f1f0-0000-0000000009b9"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal motor specification, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // A configid that spells out the default id's key: OpenRocket puts the motor in the place of the mount's default configuration, which here is always "no motor"; the motor is refused.
    // OpenRocket: | config default name='[{motors}]' preload=null active=[true] motors=1
    {.name = "mm-default-key-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="ffffffff-f4f2-f1f0-0000-00000000162c"><designation>C6</designation><delay>5.0</delay></motor><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal motor specification, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1)out"},
}};
// END GENERATED TABLES MotorMountHandler
// clang-format on

TEST(MotorMountHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(MotorMountHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(MotorMountHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(MotorMountHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

// The component acts as a motor mount from the moment its <motormount> opens, whatever follows.
TEST(MotorMountHandler, MakesTheComponentAMotorMountAtOnce)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>());
    InnerTube&        inner = tube.addChild(std::make_unique<InnerTube>());
    ASSERT_FALSE(tube.isMotorMount());
    RocketEventRecorder events(fixture.rocket());

    const MotorMountHandler first(tube, fixture.context());
    EXPECT_TRUE(tube.isMotorMount());
    // One event of the body tube (a MOTOR_CHANGE, which is neither of the two kinds the
    // recorder names).
    EXPECT_EQ(events.take(), "C[BodyTube]");
    // A second <motormount> of the same component changes nothing and fires nothing.
    const MotorMountHandler second(tube, fixture.context());
    EXPECT_EQ(events.take(), "");

    const MotorMountHandler third(inner, fixture.context());
    EXPECT_TRUE(inner.isMotorMount());
    EXPECT_EQ(events.take(), "C[InnerTube]");
}

TEST(MotorMountHandler, IgnoresAChildItDoesNotKnow)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>());
    MotorMountHandler handler(tube, fixture.context());
    WarningSet        warnings;

    // Null: the element is ignored with what it holds.
    const Result<ElementHandler*> unknown = handler.openElement("bogus", {}, warnings);
    ASSERT_TRUE(unknown.has_value());
    EXPECT_EQ(unknown.value_or(&handler), nullptr);
    const Result<ElementHandler*> otherCase = handler.openElement("Motor", {}, warnings);
    EXPECT_EQ(otherCase.value_or(&handler), nullptr);
    EXPECT_EQ(warningTexts(warnings), (Texts{"Unknown element 'bogus' encountered, ignoring.",
                                             "Unknown element 'Motor' encountered, ignoring."}));
}

// The motors come from the context's finder through MotorHandler. With the motor database
// OpenRocket ships, the element of the scout's probe "motor-basic" gives the motors OpenRocket
// finds for it (tier9-scout-loader-components/out/cases2.out: its re-saved file names their
// digests), one of them with a warning of the database's finder.
TEST(MotorMountHandler, TakesItsMotorsFromTheFinderOfTheContext)
{
    constexpr std::string_view kXml =
        R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><type>single</type><manufacturer>Estes</manufacturer><digest>xyz</digest><designation>C6</designation><diameter>0.018</diameter><length>0.07</length><delay>5.0</delay></motor><motor><designation>B6</designation><delay>none</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml";

    RocketLoadFixture         fixture;
    const DatabaseMotorFinder finder(QtRocket::Test::bundledMotorDatabase());
    fixture.context().setMotorFinder(&finder);
    const HandlerRun run = fixture.load(kXml);
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(),
              Texts{"Multiple motors with designation 'B6' found, one chosen arbitrarily."});

    const auto* const tube =
        dynamic_cast<const BodyTube*>(&fixture.rocket().getChild(0).getChild(0));
    ASSERT_NE(tube, nullptr);
    const std::vector<FlightConfigurationId> ids = tube->getMotorConfigurationSet().getIds();
    ASSERT_EQ(ids.size(), 2U);
    EXPECT_EQ(ids.at(0).toString(), "11111111-2222-3333-4444-555555555555");

    const MotorConfiguration& first = tube->getMotorConfig(ids.at(0));
    ASSERT_NE(first.getMotor(), nullptr);
    EXPECT_EQ(first.getMotor()->getDesignation(), "C6");
    EXPECT_EQ(first.getMotor()->getDigest(), "2967cd7a160b396ef96f09695429d8e9");
    EXPECT_EQ(first.getEjectionDelay(), 5.0);

    // The motor without a configid belongs to a configuration of its own, which the rocket has
    // got too.
    const MotorConfiguration& second = tube->getMotorConfig(ids.at(1));
    ASSERT_NE(second.getMotor(), nullptr);
    EXPECT_EQ(second.getMotor()->getDesignation(), "B6");
    EXPECT_EQ(second.getMotor()->getDigest(), "74472299ac7ffc451f7b434d6c81b89c");
    EXPECT_EQ(second.getEjectionDelay(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(second.getEjectionDelay(), Motor::kPluggedDelay);
    EXPECT_EQ(fixture.rocket().getIds(), ids);
}

}  // namespace
