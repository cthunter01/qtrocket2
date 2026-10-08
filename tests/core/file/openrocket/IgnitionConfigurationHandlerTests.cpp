#include "QtRocket/file/openrocket/IgnitionConfigurationHandler.h"

#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// IgnitionConfigurationHandler: the reader of an <ignitionconfiguration> element. The tables
// read it as a file has it, in a motor mount, and compare what the mount holds afterwards with
// what OpenRocket makes of the same text; the tests behind them ask the handler itself.

namespace
{

using QtRocket::IgnitionConfigurationHandler;
using QtRocket::IgnitionEvent;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::runHandler;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES IgnitionConfigurationHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 13> kJava{{
    {.name = "ic-both", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-delay-first", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>2</ignitiondelay><ignitionevent>never</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:NEVER:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:NEVER:2.0:true] flying=[#2:F12X:NEVER:2.0:true])out"},
    {.name = "ic-trimmed", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent> ejectioncharge
</ignitionevent><ignitiondelay> 2 </ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:EJECTION_CHARGE:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:EJECTION_CHARGE:2.0:true] flying=[#2:F12X:EJECTION_CHARGE:2.0:true])out"},
    {.name = "ic-every-event", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>automatic</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>launch</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>ejectioncharge</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>never</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 12 {mass,aero,tree=5, motor=5, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:LAUNCH:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:EJECTION_CHARGE:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.8 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:NEVER:1.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=5 all=[#2:F12X:AUTOMATIC:1.0:true,#3:F12X:LAUNCH:1.0:true,#4:F12X:EJECTION_CHARGE:1.0:true,#5:F12X:BURNOUT:1.0:true,#6:F12X:NEVER:1.0:true] flying=[#2:F12X:AUTOMATIC:1.0:true,#3:F12X:LAUNCH:1.0:true,#4:F12X:EJECTION_CHARGE:1.0:true,#5:F12X:BURNOUT:1.0:true])out"},
    {.name = "ic-second-event-unknown", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitionevent>bogus</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-first-event-unknown", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>bogus</ignitionevent><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type 'bogus', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-two-events", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitionevent>never</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:NEVER:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:NEVER:2.0:true] flying=[#2:F12X:NEVER:2.0:true])out"},
    {.name = "ic-two-delays", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>2</ignitiondelay><ignitiondelay>3</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:3.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:3.0:true] flying=[#2:F12X:BURNOUT:3.0:true])out"},
    {.name = "ic-second-delay-unreadable", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>2</ignitiondelay><ignitiondelay>x</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-delay-forms", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>0x1p1</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-negative-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>-2.5</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:-2.5:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:-2.5:true] flying=[#2:F12X:BURNOUT:-2.5:true])out"},
    {.name = "ic-unknown-children", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><bogus a="1">t</bogus><ignitionevent>burnout</ignitionevent><other/><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
    {.name = "ic-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555" a="1">text<ignitionevent b="2">burnout</ignitionevent><ignitiondelay c="3">2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:2.0:true] flying=[#2:F12X:BURNOUT:2.0:true])out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 4> kOwn{{
    // An event that is none leaves OpenRocket's handler without one, and OpenRocket stores null as the configuration's ignition event; here the event stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thic ...
    // OpenRocket: ... (2 more lines differ)
    {.name = "ic-names-that-are-none", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>BURNOUT</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>ejection_charge</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>Launch</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burn out</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent></ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown ignition event type 'BURNOUT', ignoring.
W Unknown ignition event type 'ejection_charge', ignoring.
W Unknown ignition event type 'Launch', ignoring.
W Unknown ignition event type 'burn out', ignoring.
W Unknown ignition event type '', ignoring.
ROOT rocket {} []
EVENTS 12 {mass,aero,tree=5, motor=5, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.8 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:1.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=5 all=[#2:F12X:AUTOMATIC:1.0:true,#3:F12X:AUTOMATIC:1.0:true,#4:F12X:AUTOMATIC:1.0:true,#5:F12X:AUTOMATIC:1.0:true,#6:F12X:AUTOMATIC:1.0:true] flying=[#2:F12X:AUTOMATIC:1.0:true,#3:F12X:AUTOMATIC:1.0:true,#4:F12X:AUTOMATIC:1.0:true,#5:F12X:AUTOMATIC:1.0:true])out"},
    // Decision L3: a delay of NaN is refused (OpenRocket stores it); the element then has no delay, which is the second warning, and its event is applied.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: | config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:NaN:true] flying=[#2:F12X:BUR ...
    {.name = "ic-nan-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>NaN</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:0.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:0.0:true] flying=[#2:F12X:BURNOUT:0.0:true])out"},
    // Decision L3: an infinite delay is refused (OpenRocket stores it) and the delay read before it stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    // OpenRocket: | config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:Infinity:true] flying=[#2:F12 ...
    {.name = "ic-infinite-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>3</ignitiondelay><ignitiondelay>Infinity</ignitiondelay><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:3.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:BURNOUT:3.0:true] flying=[#2:F12X:BURNOUT:3.0:true])out"},
    // The child takes the event's text away (DelegatorHandler's slip), so the element has no event: OpenRocket stores null as the ignition event (of the default: the slip took the configid too); here the event stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "ic-child-in-a-value", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout<x/></ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Unknown ignition event type '', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:2.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1 all=[#2:F12X:AUTOMATIC:0.0:false] flying=[#2:F12X:AUTOMATIC:0.0:false])out"},
}};
// END GENERATED TABLES IgnitionConfigurationHandler
// clang-format on

TEST(IgnitionConfigurationHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(IgnitionConfigurationHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(IgnitionConfigurationHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(IgnitionConfigurationHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// What the handler has read from @p xml: "<delay or none>, <event or none>" and the warnings
/// in brackets.
[[nodiscard]] std::string readBy(std::string_view xml)
{
    HandlerFixture               fixture;
    IgnitionConfigurationHandler handler(fixture.context());
    const HandlerRun             run = runHandler(handler, xml);
    if (!run.result.has_value())
    {
        return "FAILED " + run.result.error().message;
    }
    const std::optional<double>        delay = handler.getIgnitionDelay();
    const std::optional<IgnitionEvent> event = handler.getIgnitionEvent();
    std::string                        told =
        (delay.has_value() ? QtRocket::Strings::javaDoubleToString(*delay) : std::string("none")) +
        ", " + (event.has_value() ? std::string(name(*event)) : std::string("none"));
    for (const std::string& warning : run.texts())
    {
        told += " [" + warning + "]";
    }
    return told;
}

TEST(IgnitionConfigurationHandler, HasReadNothingUntilItsChildrenGiveIt)
{
    EXPECT_EQ(readBy("<ignitionconfiguration/>"), "none, none");
    EXPECT_EQ(readBy("<ignitionconfiguration configid='x'>text</ignitionconfiguration>"),
              "none, none");
}

TEST(IgnitionConfigurationHandler, ReadsTheEventAndTheDelay)
{
    EXPECT_EQ(
        readBy("<i><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></i>"),
        "2.0, BURNOUT");
    // Trimmed, both of them.
    EXPECT_EQ(readBy("<i><ignitionevent> never </ignitionevent><ignitiondelay> 0.5 </ignitiondelay>"
                     "</i>"),
              "0.5, NEVER");
    EXPECT_EQ(readBy("<i><ignitiondelay>-1.5</ignitiondelay></i>"), "-1.5, none");
    EXPECT_EQ(readBy("<i><ignitionevent>ejectioncharge</ignitionevent></i>"),
              "none, EJECTION_CHARGE");
}

TEST(IgnitionConfigurationHandler, WarnsOfAnEventThatIsNoneOnlyWhileItHasNoEvent)
{
    EXPECT_EQ(readBy("<i><ignitionevent>BURNOUT</ignitionevent></i>"),
              "none, none [Unknown ignition event type 'BURNOUT', ignoring.]");
    // The text of the warning is the trimmed one.
    EXPECT_EQ(readBy("<i><ignitionevent> bogus </ignitionevent></i>"),
              "none, none [Unknown ignition event type 'bogus', ignoring.]");
    // After an event that was read, one that is none is passed over without a word.
    EXPECT_EQ(readBy("<i><ignitionevent>launch</ignitionevent><ignitionevent>bogus</ignitionevent>"
                     "</i>"),
              "none, LAUNCH");
    EXPECT_EQ(readBy("<i><ignitionevent>bogus</ignitionevent><ignitionevent>launch</ignitionevent>"
                     "</i>"),
              "none, LAUNCH [Unknown ignition event type 'bogus', ignoring.]");
}

TEST(IgnitionConfigurationHandler, KeepsTheDelayItHadWhenTheNextOneCannotBeRead)
{
    EXPECT_EQ(readBy("<i><ignitiondelay>x</ignitiondelay></i>"),
              "none, none [Illegal ignition delay specified, ignoring.]");
    EXPECT_EQ(readBy("<i><ignitiondelay>2</ignitiondelay><ignitiondelay/></i>"),
              "2.0, none [Illegal ignition delay specified, ignoring.]");
    // Decision L3: a delay that is not finite is none either (OpenRocket takes it).
    EXPECT_EQ(readBy("<i><ignitiondelay>2</ignitiondelay><ignitiondelay>NaN</ignitiondelay></i>"),
              "2.0, none [Illegal ignition delay specified, ignoring.]");
    EXPECT_EQ(readBy("<i><ignitiondelay>Infinity</ignitiondelay></i>"),
              "none, none [Illegal ignition delay specified, ignoring.]");
    EXPECT_EQ(readBy("<i><ignitiondelay>-1e999</ignitiondelay></i>"),
              "none, none [Illegal ignition delay specified, ignoring.]");
}

TEST(IgnitionConfigurationHandler, WarnsOfOtherChildren)
{
    EXPECT_EQ(readBy("<i><bogus a='1'>t</bogus><other/><ignitiondelay b='2'>1</ignitiondelay></i>"),
              "1.0, none [Unknown text in element 'bogus', ignoring.] [Unknown attributes in "
              "element 'bogus', ignoring.]");
}

}  // namespace
