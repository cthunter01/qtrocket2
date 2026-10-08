#include "QtRocket/file/openrocket/DeploymentConfigurationHandler.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// DeploymentConfigurationHandler: the handler of a <deploymentconfiguration> element of a
// parachute or streamer. What a file's text gives is compared with what OpenRocket makes of the
// same text.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::DeploymentConfiguration;
using QtRocket::DeploymentConfigurationHandler;
using QtRocket::Parachute;
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
// BEGIN GENERATED TABLES DeploymentConfigurationHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 21> kJava{{
    {.name = "dc-basic", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-streamer", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><streamer><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></streamer></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.08937142857142857:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 strip=0.5:0.05
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-scout", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deployevent>altitude</deployevent><deployaltitude>100</deployaltitude><deploydelay>1</deploydelay><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent> apogee </deployevent><deploydelay>x</deploydelay><bogus a="1">t</bogus></deploymentconfiguration><deploymentconfiguration><deployevent>bogus</deployevent></deploymentconfiguration><cd>AUTO</cd><linelength>auto</linelength><isdrogue>true</isdrogue></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 7 {mass,aero=2, mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=true mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=ALTITUDE:1.0:100.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.0:100.0 deploy[random]=ALTITUDE:1.0:100.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-starts-from-the-default-of-the-moment", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>3</deploydelay></deploymentconfiguration><deployevent>never</deployevent><deployaltitude>50</deployaltitude><deploymentconfiguration configid="22222222-3333-4444-5555-666666666666"><deploydelay>4</deploydelay></deploymentconfiguration><deploydelay>9</deploydelay></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=NEVER:9.0:50.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:3.0:200.0 deploy[22222222-3333-4444-5555-666666666666]=NEVER:4.0:50.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-same-id-twice", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>3</deploydelay></deploymentconfiguration><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployaltitude>77</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:77.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"></deploymentconfiguration><deployevent>apogee</deployevent></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=APOGEE:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-without-configid", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration><deploymentconfiguration><deploydelay>2</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[random]=APOGEE:1.5:120.0 deploy[random]=EJECTION:2.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-text-configids", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="abc"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration><deploymentconfiguration configid=""><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration><deploymentconfiguration configid="1-2-3-4-5"><deploydelay>2</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[00000000-0000-0000-0000-000000017862]=APOGEE:1.5:120.0 deploy[random]=APOGEE:1.5:120.0 deploy[00000001-0002-0003-0004-000000000005]=EJECTION:2.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-every-event", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>launch</deployevent></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>ejection</deployevent></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>altitude</deployevent></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>lowerstageseparation</deployevent></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>never</deployevent></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {mass,aero=2, mass,aero,tree=1, mass,tree=6, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=LAUNCH:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=ALTITUDE:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=LOWER_STAGE_SEPARATION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=NEVER:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-names-that-are-none", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>APOGEE</deployevent><deploydelay>1</deploydelay></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>lower_stage_separation</deployevent><deploydelay>1</deploydelay></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>ejectioncharge</deployevent><deploydelay>1</deploydelay></deploymentconfiguration></parachute><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent></deployevent><deploydelay>1</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 8 {mass,aero=2, mass,aero,tree=1, mass,tree=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:1.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-event-unknown-after-a-known-one", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deployevent>bogus</deployevent></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-delay-unreadable-after-a-number", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>3</deploydelay><deploydelay>x</deploydelay><deployaltitude>60</deployaltitude><deployaltitude/></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-last-value-counts", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>bogus</deployevent><deployevent>never</deployevent><deploydelay>x</deploydelay><deploydelay>3</deploydelay><deployaltitude>1</deployaltitude><deployaltitude>2</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=NEVER:3.0:2.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-trimmed", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>
 apogee </deployevent><deploydelay> 1.5 </deploydelay><deployaltitude> 120
</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-number-forms", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>0x1p1</deploydelay><deployaltitude>1e2d</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:2.0:100.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-negative-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>-1.5</deploydelay><deployaltitude>-120</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:-1.5:-120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555" x="1">text<deployevent a="1">apogee</deployevent><deploydelay b="2">3</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:3.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-unknown-children", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><bogus a="1">t</bogus><deployevent>apogee</deployevent><other/></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-child-in-a-value", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee<x/></deployevent><deploydelay>3</deploydelay></deploymentconfiguration><name>P</name></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Invalid parameter encountered, ignoring.
W Unknown attributes in element 'parachute', ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=2, mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'P' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[random]=EJECTION:3.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-default-key", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="ffffffff-f4f2-f1f0-0000-00000000162c"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>9</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=APOGEE:1.5:120.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:9.0:120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "dc-plain-elements-set-the-default", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deployevent>altitude</deployevent><deployaltitude>100</deployaltitude><deploydelay>1</deploydelay></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=ALTITUDE:1.0:100.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 6> kOwn{{
    // Decision L3: an infinite deploy delay is refused with a warning (OpenRocket stores it).
    // OpenRocket: |       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nyl ...
    {.name = "dc-infinite-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>Infinity</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a deploy delay of NaN is refused with a warning (OpenRocket passes it over without one).
    {.name = "dc-nan-delay", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>3</deploydelay><deploydelay>NaN</deploydelay></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an infinite deploy altitude is refused with a warning (OpenRocket stores it).
    // OpenRocket: |       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nyl ...
    {.name = "dc-infinite-altitude", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployaltitude>-Infinity</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a deploy altitude of NaN is refused with a warning (OpenRocket passes it over without one).
    {.name = "dc-nan-altitude", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployaltitude>NaN</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: numbers too large for a double are infinities and refused (OpenRocket stores them).
    // OpenRocket: |       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nyl ...
    {.name = "dc-overflowing-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>1e400</deploydelay><deployaltitude>-1e400</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // A configid that spells out the error id's key is the error id here: the element is refused. OpenRocket stores a deployment under it.
    // OpenRocket: |       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nyl ...
    {.name = "dc-error-key", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><parachute><deploymentconfiguration configid="ffffffff-f4f2-f1f0-0000-0000000009b9"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES DeploymentConfigurationHandler
// clang-format on

TEST(DeploymentConfigurationHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(DeploymentConfigurationHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(DeploymentConfigurationHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(DeploymentConfigurationHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// "<event>:<delay>:<altitude>" of @p config.
[[nodiscard]] std::string told(const DeploymentConfiguration& config)
{
    return std::string(deployEventName(config.getDeployEvent())) + ":" +
           QtRocket::Strings::javaDoubleToString(config.getDeployDelay()) + ":" +
           QtRocket::Strings::javaDoubleToString(config.getDeployAltitude());
}

/// What getConfiguration() makes of a deployment of LAUNCH, 7 s and 70 m after the handler has
/// read @p xml.
[[nodiscard]] std::string configurationAfter(std::string_view xml)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>());
    Parachute&        chute = tube.addChild(std::make_unique<Parachute>());
    DeploymentConfigurationHandler handler(chute, fixture.context());
    const HandlerRun               run = runHandler(handler, xml);
    if (!run.result.has_value())
    {
        return "FAILED " + run.result.error().message;
    }
    DeploymentConfiguration def;
    def.setDeployEvent(DeploymentConfiguration::DeployEvent::LAUNCH);
    def.setDeployDelay(7);
    def.setDeployAltitude(70);
    return told(handler.getConfiguration(def));
}

// getConfiguration(): a copy of what it is given, with what was read put in.
TEST(DeploymentConfigurationHandler, PutsWhatItReadIntoACopyOfAConfiguration)
{
    EXPECT_EQ(configurationAfter("<d/>"), "LAUNCH:7.0:70.0");
    EXPECT_EQ(configurationAfter("<d><deployevent>apogee</deployevent></d>"), "APOGEE:7.0:70.0");
    EXPECT_EQ(configurationAfter("<d><deploydelay>1.5</deploydelay></d>"), "LAUNCH:1.5:70.0");
    EXPECT_EQ(configurationAfter("<d><deployaltitude>120</deployaltitude></d>"),
              "LAUNCH:7.0:120.0");
    EXPECT_EQ(configurationAfter("<d><deployevent>never</deployevent><deploydelay>0</deploydelay>"
                                 "<deployaltitude>0</deployaltitude></d>"),
              "NEVER:0.0:0.0");
    // What could not be read, and what is not finite, is not put in; and it takes back what
    // was read before it.
    EXPECT_EQ(configurationAfter("<d><deployevent>apogee</deployevent><deployevent>x</deployevent>"
                                 "<deploydelay>1</deploydelay><deploydelay>x</deploydelay>"
                                 "<deployaltitude>2</deployaltitude>"
                                 "<deployaltitude>Infinity</deployaltitude></d>"),
              "LAUNCH:7.0:70.0");
    EXPECT_EQ(configurationAfter("<d><deploydelay>NaN</deploydelay></d>"), "LAUNCH:7.0:70.0");
}

// The handler touches the device only when its element closes.
TEST(DeploymentConfigurationHandler, ChangesNothingBeforeItsElementCloses)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>());
    Parachute&        chute = tube.addChild(std::make_unique<Parachute>());
    DeploymentConfigurationHandler handler(chute, fixture.context());
    WarningSet                     warnings;

    ASSERT_TRUE(handler.closeElement("deployevent", {}, "apogee", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("deploydelay", {}, "3", warnings).has_value());
    EXPECT_EQ(chute.getDeploymentConfigurations().size(), 0U);
    EXPECT_EQ(told(chute.getDeploymentConfigurations().getDefault()), "EJECTION:0.0:200.0");

    ASSERT_TRUE(handler
                    .endHandler("deploymentconfiguration",
                                {{"configid", "11111111-2222-3333-4444-555555555555"}}, "",
                                warnings)
                    .has_value());
    ASSERT_EQ(chute.getDeploymentConfigurations().size(), 1U);
    EXPECT_EQ(told(chute.getDeploymentConfigurations().get(0)), "APOGEE:3.0:200.0");
    EXPECT_EQ(told(chute.getDeploymentConfigurations().getDefault()), "EJECTION:0.0:200.0");
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
