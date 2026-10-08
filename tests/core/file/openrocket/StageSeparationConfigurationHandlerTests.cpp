#include "QtRocket/file/openrocket/StageSeparationConfigurationHandler.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// StageSeparationConfigurationHandler: the handler of a <separationconfiguration> element of a
// stage. What a file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::AxialStage;
using QtRocket::ElementHandler;
using QtRocket::StageSeparationConfiguration;
using QtRocket::StageSeparationConfigurationHandler;
using QtRocket::WarningSet;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::runHandler;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES StageSeparationConfigurationHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 21> kJava{{
    {.name = "sc-basic", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-scout", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><separationevent>upperignition</separationevent><separationdelay>2</separationdelay><separationaltitude>50</separationaltitude><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>upper_ignition</separationevent><separationdelay>3</separationdelay></separationconfiguration></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationaltitude>7</separationaltitude></separationconfiguration></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=UPPER_IGNITION:2.0:50.0 sep[11111111-2222-3333-4444-555555555555]=UPPER_IGNITION:3.0:50.0
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:0.0:7.0
| selected=default
| config default name='[{motors}]' preload=null active=[false,false] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[false,false] motors=0)out"},
    {.name = "sc-starts-from-the-default-of-the-moment", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>3</separationdelay></separationconfiguration><separationevent>never</separationevent><separationaltitude>50</separationaltitude><separationconfiguration configid="22222222-3333-4444-5555-666666666666"><separationdelay>4</separationdelay></separationconfiguration><separationdelay>9</separationdelay><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=NEVER:9.0:50.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:3.0:200.0 sep[22222222-3333-4444-5555-666666666666]=NEVER:4.0:50.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-same-id-twice-keeps-the-first", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>apogee</separationevent><separationdelay>3</separationdelay></separationconfiguration><separationevent>never</separationevent><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationaltitude>77</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=NEVER:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=APOGEE:3.0:77.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"></separationconfiguration><separationevent>apogee</separationevent><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=APOGEE:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-without-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><separationconfiguration><separationdelay>2</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[random]=BURNOUT:1.5:120.0 sep[random]=EJECTION:2.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-text-configids", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="abc"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><separationconfiguration configid=""><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><separationconfiguration configid="1-2-3-4-5"><separationdelay>2</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[00000000-0000-0000-0000-000000017862]=BURNOUT:1.5:120.0 sep[random]=BURNOUT:1.5:120.0 sep[00000001-0002-0003-0004-000000000005]=EJECTION:2.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-every-event", .xml = R"xml(<subcomponents><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>launch</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>ignition</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>ejection</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>upperignition</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>altitudeascending</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>apogee</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>altitudedescending</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>never</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 18 {mass,aero,tree=9, tree=9}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=LAUNCH:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=IGNITION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.4 x=0.4 len=0.2 stage=2 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.6000000000000001 x=0.6000000000000001 len=0.2 stage=3 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.8 x=0.8 len=0.2 stage=4 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=UPPER_IGNITION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:1.0 x=1.0 len=0.2 stage=5 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=ALTITUDE_ASCENDING:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:1.2 x=1.2 len=0.2 stage=6 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=APOGEE:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:1.4 x=1.4 len=0.2 stage=7 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=ALTITUDE_DESCENDING:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:1.5999999999999999 x=1.5999999999999999 len=0.2 stage=8 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=NEVER:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true,true,true,true,true,true,true,true] motors=0)out"},
    {.name = "sc-names-that-are-none", .xml = R"xml(<subcomponents><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>BURNOUT</separationevent><separationdelay>1</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>upper_ignition</separationevent><separationdelay>1</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>altitude</separationevent><separationdelay>1</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent></separationevent><separationdelay>1</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 8 {mass,aero,tree=4, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.4 x=0.4 len=0.2 stage=2 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.6000000000000001 x=0.6000000000000001 len=0.2 stage=3 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true,true,true] motors=0)out"},
    {.name = "sc-event-unknown-after-a-known-one", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>apogee</separationevent><separationevent>bogus</separationevent></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-delay-unreadable-after-a-number", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>3</separationdelay><separationdelay>x</separationdelay><separationaltitude>60</separationaltitude><separationaltitude/></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-last-value-counts", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>bogus</separationevent><separationevent>never</separationevent><separationdelay>x</separationdelay><separationdelay>3</separationdelay><separationaltitude>1</separationaltitude><separationaltitude>2</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=NEVER:3.0:2.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-trimmed", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>
 apogee </separationevent><separationdelay> 1.5 </separationdelay><separationaltitude> 120
</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-negative-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>-1.5</separationdelay><separationaltitude>-120</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:-1.5:-120.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555" x="1">text<separationevent a="1">apogee</separationevent><separationdelay b="2">3</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=APOGEE:3.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-unknown-children", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><bogus a="1">t</bogus><separationevent>apogee</separationevent><other/></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=APOGEE:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-child-in-a-value", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>apogee<x/></separationevent><separationdelay>3</separationdelay></separationconfiguration><name>S</name><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Invalid parameter encountered, ignoring.
W Unknown attributes in element 'stage', ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=2, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'S' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[random]=EJECTION:3.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-default-key", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="ffffffff-f4f2-f1f0-0000-00000000162c"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>9</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=BURNOUT:1.5:120.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:9.0:120.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-default-key-altitude-only", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="ffffffff-f4f2-f1f0-0000-00000000162c"><separationaltitude>33</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:33.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "sc-boosters", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parallelstage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration></parallelstage><boosterset><separationconfiguration configid="22222222-3333-4444-5555-666666666666"><separationevent>never</separationevent></separationconfiguration></boosterset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=2, mass,aero,tree=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=1.0 len=0.0 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=1.0 len=0.0 stage=2 sep=EJECTION:0.0:200.0 sep[22222222-3333-4444-5555-666666666666]=NEVER:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true,false,false] motors=0)out"},
    {.name = "sc-plain-elements-set-the-default", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationevent>altitudedescending</separationevent><separationaltitude>100</separationaltitude><separationdelay>1</separationdelay><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=ALTITUDE_DESCENDING:1.0:100.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 5> kOwn{{
    // Decision L3: an infinite separation delay is refused with a warning (OpenRocket stores it).
    // OpenRocket: |   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:Infinity:200.0
    {.name = "sc-infinite-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>Infinity</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    // Decision L3: a separation delay of NaN is refused with a warning (OpenRocket passes it over without one).
    {.name = "sc-nan-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationdelay>3</separationdelay><separationdelay>NaN</separationdelay></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    // Decision L3: an infinite separation altitude is refused with a warning (OpenRocket stores it).
    // OpenRocket: |   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:-Infinity
    {.name = "sc-infinite-altitude", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationaltitude>-Infinity</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    // Decision L3: a separation altitude of NaN is refused with a warning (OpenRocket passes it over without one).
    {.name = "sc-nan-altitude", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationaltitude>NaN</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    // A configid that spells out the error id's key is the error id here: the element is refused. OpenRocket stores a separation under it.
    // OpenRocket: |   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0 sep[ffffffff-f4f2-f1f0-0000-0000000009b9]=BURNOUT:1.5:120.0
    {.name = "sc-error-key", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage><stage><separationconfiguration configid="ffffffff-f4f2-f1f0-0000-0000000009b9"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.2 x=0.2 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
}};
// END GENERATED TABLES StageSeparationConfigurationHandler
// clang-format on

TEST(StageSeparationConfigurationHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(StageSeparationConfigurationHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(StageSeparationConfigurationHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(StageSeparationConfigurationHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// "<event>:<delay>:<altitude>" of @p config.
[[nodiscard]] std::string told(const StageSeparationConfiguration& config)
{
    return std::string(separationEventName(config.getSeparationEvent())) + ":" +
           QtRocket::Strings::javaDoubleToString(config.getSeparationDelay()) + ":" +
           QtRocket::Strings::javaDoubleToString(config.getSeparationAltitude());
}

/// What getConfiguration() makes of a separation of LAUNCH, 7 s and 70 m after the handler has
/// read @p xml.
[[nodiscard]] std::string configurationAfter(std::string_view xml)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    StageSeparationConfigurationHandler handler(stage, fixture.context());
    const HandlerRun                    run = runHandler(handler, xml);
    if (!run.result.has_value())
    {
        return "FAILED " + run.result.error().message;
    }
    StageSeparationConfiguration def;
    def.setSeparationEvent(StageSeparationConfiguration::SeparationEvent::LAUNCH);
    def.setSeparationDelay(7);
    def.setSeparationAltitude(70);
    return told(handler.getConfiguration(def));
}

// getConfiguration(): a copy of what it is given, with what was read put in.
TEST(StageSeparationConfigurationHandler, PutsWhatItReadIntoACopyOfAConfiguration)
{
    EXPECT_EQ(configurationAfter("<s/>"), "LAUNCH:7.0:70.0");
    EXPECT_EQ(configurationAfter("<s><separationevent>apogee</separationevent></s>"),
              "APOGEE:7.0:70.0");
    EXPECT_EQ(configurationAfter("<s><separationdelay>1.5</separationdelay></s>"),
              "LAUNCH:1.5:70.0");
    EXPECT_EQ(configurationAfter("<s><separationaltitude>120</separationaltitude></s>"),
              "LAUNCH:7.0:120.0");
    EXPECT_EQ(configurationAfter("<s><separationevent>upperignition</separationevent>"
                                 "<separationdelay>0</separationdelay>"
                                 "<separationaltitude>0</separationaltitude></s>"),
              "UPPER_IGNITION:0.0:0.0");
    // What could not be read, and what is not finite, is not put in; and it takes back what
    // was read before it.
    EXPECT_EQ(configurationAfter("<s><separationevent>apogee</separationevent>"
                                 "<separationevent>upper_ignition</separationevent>"
                                 "<separationdelay>1</separationdelay>"
                                 "<separationdelay>x</separationdelay>"
                                 "<separationaltitude>2</separationaltitude>"
                                 "<separationaltitude>-Infinity</separationaltitude></s>"),
              "LAUNCH:7.0:70.0");
    EXPECT_EQ(configurationAfter("<s><separationaltitude>NaN</separationaltitude></s>"),
              "LAUNCH:7.0:70.0");
}

// Where this handler differs from the one of a recovery device: the element starts from what
// its configuration has, not from the default.
TEST(StageSeparationConfigurationHandler, StartsFromTheSeparationItsConfigurationHas)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    WarningSet        warnings;
    const ElementHandler::Attributes id{{"configid", "11111111-2222-3333-4444-555555555555"}};

    StageSeparationConfigurationHandler first(stage, fixture.context());
    ASSERT_TRUE(first.closeElement("separationevent", {}, "apogee", warnings).has_value());
    ASSERT_TRUE(first.closeElement("separationdelay", {}, "3", warnings).has_value());
    // Nothing is touched before the element closes.
    EXPECT_EQ(stage.getSeparationConfigurations().size(), 0U);
    ASSERT_TRUE(first.endHandler("separationconfiguration", id, "", warnings).has_value());
    ASSERT_EQ(stage.getSeparationConfigurations().size(), 1U);
    EXPECT_EQ(told(stage.getSeparationConfigurations().get(0)), "APOGEE:3.0:200.0");

    // The default changes in between; the second element of the same id does not start from it.
    stage.getSeparationConfigurations().getDefault().setSeparationEvent(
        StageSeparationConfiguration::SeparationEvent::NEVER);
    StageSeparationConfigurationHandler second(stage, fixture.context());
    ASSERT_TRUE(second.closeElement("separationaltitude", {}, "55", warnings).has_value());
    ASSERT_TRUE(second.endHandler("separationconfiguration", id, "", warnings).has_value());
    ASSERT_EQ(stage.getSeparationConfigurations().size(), 1U);
    EXPECT_EQ(told(stage.getSeparationConfigurations().get(0)), "APOGEE:3.0:55.0");
    EXPECT_EQ(told(stage.getSeparationConfigurations().getDefault()), "NEVER:0.0:200.0");
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
