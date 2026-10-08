#include "QtRocket/file/openrocket/ComponentParameterHandler.h"

#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// ComponentParameterHandler: the handler of a component's element, the rocket's included. What
// a file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Color;
using QtRocket::ComponentParameterHandler;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
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
// BEGIN GENERATED TABLES ComponentParameterHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 17> kJava{{
    {.name = "cp-document-order", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>first</name><length>0.1</length><name>second</name><length>0.3</length><radius>0.02</radius><thickness>0.03</thickness><radius>0.05</radius></bodytube></subcomponents></stage></subcomponents><name>R1</name><name>R2</name>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 11 {mass=1, mass,aero=4, mass,aero,tree=1, nonfunc=4, tree=1}
| Rocket 'R2' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.3 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'second' axial=AFTER:0.0 x=0.0 len=0.3 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.03:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-wrong-component", .xml = R"xml(<subcomponents><stage><motormount/><finpoints/><deploymentconfiguration/><motorconfiguration/><flightconfiguration/><subcomponents><nosecone><motormount><overhang>1</overhang></motormount><finpoints/><deploymentconfiguration/><separationconfiguration/><motorconfiguration/><flightconfiguration/><name>N</name></nosecone><bodytube><motorconfiguration/><flightconfiguration/><finpoints/><deploymentconfiguration/><separationconfiguration/><subcomponents><trapezoidfinset><finpoints><point x="0" y="0"/></finpoints><motormount/></trapezoidfinset><shockcord><deploymentconfiguration/></shockcord><masscomponent><deploymentconfiguration/></masscomponent></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal component defined as motor mount.
W Illegal component defined for fin points.
W Illegal component defined as recovery device.
W Illegal component defined for motor configuration.
W Illegal component defined for flight configuration.
W Illegal component defined as stage.
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=3, mass,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.35000000000000003 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'N' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       ShockCord 'Shock Cord' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cord=1.1250000000000002:true mat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-right-component", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"/><flightconfiguration configid="22222222-3333-4444-5555-666666666666"/><subcomponents><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><subcomponents><bodytube><motormount/><subcomponents><innertube><motormount/></innertube><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset><parachute><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></parachute><streamer><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></streamer><parallelstage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration></parallelstage><boosterset><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration></boosterset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'flightconfiguration' for Rocket, ignoring.
ROOT rocket {} []
EVENTS 13 {mass,aero=1, mass,aero,tree=2, mass,tree=3, motor=2, tree=5}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.1,0.0
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.08937142857142857:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 strip=0.5:0.05
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=2 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true,false,false] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true,false,false] motors=0
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload=null active=[true,false,false] motors=0)out"},
    {.name = "cp-wrong-component-with-content", .xml = R"xml(<subcomponents><stage><name>S</name><subcomponents><nosecone a="1">t0<motormount x="1">t1<motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation></motor></motormount>t2<name>N</name>t3</nosecone><bodytube><name>T</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal component defined as motor mount.
W Unknown text in element 'nosecone', ignoring.
W Unknown attributes in element 'nosecone', ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=2, nonfunc=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.35000000000000003 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'N' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'T' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-unknown-parameters", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><bogus>1</bogus><Length>2</Length><radialposition>0.1</radialposition><fincount>3</fincount><deployevent>apogee</deployevent><name>T</name><stage/><bodytube/><rocket/></bodytube></subcomponents></stage></subcomponents><bogus/><length>1</length>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'bogus' for Body Tube, ignoring.
W Unknown parameter type 'Length' for Body Tube, ignoring.
W Unknown parameter type 'radialposition' for Body Tube, ignoring.
W Unknown parameter type 'fincount' for Body Tube, ignoring.
W Unknown parameter type 'deployevent' for Body Tube, ignoring.
W Unknown parameter type 'stage' for Body Tube, ignoring.
W Unknown parameter type 'bodytube' for Body Tube, ignoring.
W Unknown parameter type 'rocket' for Body Tube, ignoring.
W Unknown parameter type 'bogus' for Rocket, ignoring.
W Unknown parameter type 'length' for Rocket, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'T' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-parameter-with-child", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>0.5<x/></length><radius>0.03</radius><name>N<b/>M</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Invalid parameter encountered, ignoring. data: '' - Body Tube
W Unknown element b, ignoring.
W Unknown text in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'M' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.03:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-unknown-parameter-with-children", .xml = R"xml(<subcomponents><stage><name>S</name><subcomponents><bodytube><bogus a="1">t<x><y/></x>u</bogus><name>N</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Unknown parameter type 'bogus' for Body Tube, ignoring.
W Unknown text in element 'bodytube', ignoring.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'N' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-flightconfiguration", .xml = R"xml(<flightconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>Cfg</name></flightconfiguration><subcomponents><stage><subcomponents><bodytube></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'flightconfiguration' for Rocket, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload=null active=[true] motors=0)out"},
    {.name = "cp-flightconfiguration-on-tube", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><flightconfiguration configid="11111111-2222-3333-4444-555555555555"/><name>T</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal component defined for flight configuration.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'T' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-refused-for-a-nose-cone", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><foreradius>0.1</foreradius><foreshoulderradius>0.01</foreshoulderradius><foreshoulderlength>0.01</foreshoulderlength><foreshoulderthickness>0.01</foreshoulderthickness><foreshouldercapped>true</foreshouldercapped><aftradius>0.03</aftradius></nosecone><transition><foreradius>0.03</foreradius><foreshoulderradius>0.01</foreshoulderradius><foreshoulderlength>0.01</foreshoulderlength><foreshoulderthickness>0.01</foreshoulderthickness><foreshouldercapped>true</foreshouldercapped></transition></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'foreradius' for Nose Cone, ignoring.
W Unknown parameter type 'foreshoulderradius' for Nose Cone, ignoring.
W Unknown parameter type 'foreshoulderlength' for Nose Cone, ignoring.
W Unknown parameter type 'foreshoulderthickness' for Nose Cone, ignoring.
W Unknown parameter type 'foreshouldercapped' for Nose Cone, ignoring.
ROOT rocket {} []
EVENTS 10 {mass=5, mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.22500000000000003 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.03:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     Transition 'Transition' axial=AFTER:0.0 x=0.15000000000000002 len=0.07500000000000001 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.03:false aft=0.025:true thick=0.002:false foresh=0.01:0.01:0.01:true aftsh=0.0:0.0:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-walk-up-the-classes", .xml = R"xml(<subcomponents><stage><separationevent>never</separationevent><subcomponents><bodytube><subcomponents><parallelstage><separationevent>burnout</separationevent><separationdelay>2</separationdelay><instancecount>3</instancecount><name>B</name></parallelstage><parachute><packedlength>0.05</packedlength><cd>0.9</cd><diameter>0.4</diameter><comment>c</comment></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {aero=1, mass=1, mass,aero=2, mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=NEVER:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'B' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=1 sep=BURNOUT:2.0:200.0 inst=3 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.05 comment='c' packed=0.05:0.0125:false radial=0.0:0.0 cd=0.9:false drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.4 lines=6:0.6000000000000001:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true,false] motors=0)out"},
    {.name = "cp-id-fails-the-load", .xml = R"xml(<subcomponents><stage><name>S</name><id>not-a-uuid</id><name>never set</name><subcomponents><bodytube><name>before</name></bodytube></subcomponents></stage></subcomponents><name>never set either</name>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Invalid UUID string: not-a-uuid
EVENTS 2 {tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "cp-id", .xml = R"xml(<subcomponents><stage><id>22222222-3333-4444-5555-666666666666</id><subcomponents><bodytube><id>1-2-3-4-5</id></bodytube></subcomponents></stage></subcomponents><id>33333333-4444-5555-6666-777777777777</id>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' id=33333333-4444-5555-6666-777777777777 axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' id=22222222-3333-4444-5555-666666666666 axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' id=00000001-0002-0003-0004-000000000005 axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-rocket-parameters", .xml = R"xml(<name>My Rocket</name><designer>Me</designer><revision>rev
2</revision><comment>c</comment><referencetype>nosecone</referencetype><customreference>0.5</customreference><designtype>kitbash</designtype><kitname>K</kitname><color red="1" green="2" blue="3"/><linestyle>solid</linestyle><finish>normal</finish><instancecount>3</instancecount><preset/><axialoffset method="top">1</axialoffset><overridemass>2</overridemass>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'finish' for Rocket, ignoring.
W Unknown parameter type 'instancecount' for Rocket, ignoring.
W Invalid ComponentPreset for component My Rocket, no manufacturer specified.  Ignored
ROOT rocket {} []
EVENTS 10 {mass=1, nonfunc=9}
| Rocket 'My Rocket' axial=ABSOLUTE:0.0 x=0.0 comment='c' color=1,2,3,255 linestyle=SOLID massovr=2.0 designer='Me' revision='rev\n2' ref=NOSECONE customref=0.5 design=KIT_BASH kit='K'
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "cp-rocket-element-with-text-and-attributes", .xml = R"xml(<rocket a="1" b="2">text<name>R</name>more</rocket>)xml", .expected = R"out(RESULT ok
ROOT rocket {a=1, b=2} [textmore]
EVENTS 1 {nonfunc=1}
| Rocket 'R' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "cp-appearance-elements", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20" blue="30"/><shine>0.5</shine></appearance><insideappearance><edgessameasinside>true</edgessameasinside><insidesameasoutside>true</insidesameasoutside><paint red="1" green="2" blue="3"/><shine>0.1</shine></insideappearance><name>N</name></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=1, nonfunc=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'N' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=10,20,30,255 shine=0.5 opacity=false] inside=[paint=1,2,3,255 shine=0.1 opacity=false] insideflags=true,true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "cp-preset-without-database", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/><length>0.3</length></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W No matching ComponentPreset for component BT found matching Estes BT-50, 30352
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.3 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'BT' axial=AFTER:0.0 x=0.0 len=0.3 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 0> kOwn{};
// END GENERATED TABLES ComponentParameterHandler
// BEGIN GENERATED TABLES ComponentParameterHandler.presets
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 4> kJavaWithPresets{{
    {.name = "pp-preset-alone", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=1, mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.4572 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'BT' axial=AFTER:0.0 x=0.0 len=0.4572 preset=BT-50, 30352 finish=NORMAL mat=[BULK|Paper, spiral kraft glassine, Estes avg, bulk|894.4|0.0|Other] r=0.012395199999999999:false thick=3.3019999999999924E-4:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "pp-values-behind-a-preset-keep-it", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/><finish>polished</finish><material type="bulk" density="900.0">Mine</material><length>0.3</length><thickness>0.001</thickness><radius>0.02</radius></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {aero=1, mass=2, mass,aero=3, mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.3 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'BT' axial=AFTER:0.0 x=0.0 len=0.3 preset=BT-50, 30352 finish=POLISHED mat=[BULK|Mine|900.0|0.0|Custom] r=0.02:false thick=0.001:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "pp-the-bracket-ends-with-the-element", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><name>NC</name><preset type="NOSE_CONE" manufacturer="Estes" partno="PNC-50YR, 72604" digest="18363dec5642206a109389ef48efcb32"/><length>0.2</length><subcomponents><masscomponent><mass>0.01</mass></masscomponent></subcomponents><aftradius>0.03</aftradius></nosecone><bodytube><name>BT</name><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/><subcomponents><parachute><name>P</name><preset type="PARACHUTE" manufacturer="Estes" partno="PK-10, 2262" digest="b595c8a31baaf325291d5c957f3940f8"/><diameter>0.5</diameter><linecount>4</linecount></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 20 {mass=2, mass,aero=6, mass,aero,tree=2, mass,tree=2, nonfunc=7, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.6572 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'NC' axial=AFTER:0.0 x=0.0 len=0.2 preset=PNC-50YR, 72604 finish=NORMAL mat=[BULK|Polystyrene, cast, bulk|756.2852368449444|0.0|Custom] shape=OGIVE:1.0:false fore=0.0:false aft=0.03:false thick=0.0015748:false foresh=0.0:0.0:0.0015748:false aftsh=0.012065:0.019049999999999997:0.0015748:true flipped=false
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.01 type=MASSCOMPONENT
|     BodyTube 'BT' axial=AFTER:0.0 x=0.2 len=0.4572 preset=BT-50, 30352 finish=NORMAL mat=[BULK|Paper, spiral kraft glassine, Estes avg, bulk|894.4|0.0|Other] r=0.012395199999999999:false thick=3.3019999999999924E-4:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'P' axial=TOP:0.0 x=0.0 len=0.025 preset=PK-10, 2262 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Polyethylene film, HDPE, 1.0 mil, bare|0.0235|0.0|Other] deploy=EJECTION:0.0:200.0 diameter=0.5 lines=4:0.254:false linemat=[LINE|Carpet Thread|3.3E-4|0.0|Other]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "pp-preset-behind-the-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><length>0.3</length><radius>0.02</radius><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero=3, mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.4572 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'BT' axial=AFTER:0.0 x=0.0 len=0.4572 preset=BT-50, 30352 finish=NORMAL mat=[BULK|Paper, spiral kraft glassine, Estes avg, bulk|894.4|0.0|Other] r=0.012395199999999999:false thick=3.3019999999999924E-4:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 0> kOwnWithPresets{};
// END GENERATED TABLES ComponentParameterHandler.presets
// clang-format on

TEST(ComponentParameterHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(ComponentParameterHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(ComponentParameterHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(ComponentParameterHandler, ReadsItsCasesWithPresetsAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJavaWithPresets, true), Texts{});
}

TEST(ComponentParameterHandler, ReadsItsOwnCasesWithPresetsAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwnWithPresets, true), Texts{});
}

// Not a test: prints what QtRocket makes of the cases with the example presets, for
// scripts/make_tables.py. Run with --gtest_also_run_disabled_tests.
TEST(ComponentParameterHandler, DISABLED_PrintsWithPresetsCases)
{
    std::cout << printedRocketCases(kJavaWithPresets, true)
              << printedRocketCases(kOwnWithPresets, true);
}

TEST(ComponentParameterHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kJavaWithPresets, true), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwnWithPresets, true), Texts{});
}

constexpr std::string_view kPresetTube =
    R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50, 30352" digest="a59dec8e4034a2fee5955dbf4ff07f1c"/><radius>0.02</radius></bodytube></subcomponents></stage></subcomponents>)xml";

// The bracket of the handler: while the element of a component is read, a value that differs
// from the preset's does not take the preset away; once the element has closed it does again.
TEST(ComponentParameterHandler, KeepsThePresetWhileItsElementIsReadAndNoLonger)
{
    RocketLoadFixture fixture(true);
    const HandlerRun  run = fixture.load(kPresetTube);
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    auto* const tube = dynamic_cast<BodyTube*>(&fixture.rocket().getChild(0).getChild(0));
    ASSERT_NE(tube, nullptr);
    // The radius is the file's, not the preset's, the length is the preset's 0.4572 m, and the
    // preset is still there.
    EXPECT_EQ(tube->getOuterRadius(), 0.02);
    EXPECT_EQ(tube->getLength(), 0.4572);
    ASSERT_NE(tube->getPresetComponent(), nullptr);
    EXPECT_EQ(tube->getPresetComponent()->getPartNo(), "BT-50, 30352");

    // The element is closed: the next change of the radius clears the preset, as for any
    // component.
    tube->setOuterRadius(0.021);
    EXPECT_EQ(tube->getPresetComponent(), nullptr);
}

// The bracket opens when the handler is made and closes in endHandler(), not when the handler
// goes: a handler that is made and never ended leaves the component ignoring preset clearing.
TEST(ComponentParameterHandler, TheBracketIsWhatKeepsThePreset)
{
    RocketLoadFixture fixture(true);
    const HandlerRun  run = fixture.load(kPresetTube);
    ASSERT_TRUE(run.result.has_value());
    auto* const tube = dynamic_cast<BodyTube*>(&fixture.rocket().getChild(0).getChild(0));
    ASSERT_NE(tube, nullptr);
    ASSERT_NE(tube->getPresetComponent(), nullptr);

    {
        const ComponentParameterHandler handler(*tube, fixture.context());
        tube->setOuterRadius(0.03);
        EXPECT_NE(tube->getPresetComponent(), nullptr);
    }
    tube->setOuterRadius(0.031);
    EXPECT_NE(tube->getPresetComponent(), nullptr);

    // endHandler() closes it.
    ComponentParameterHandler handler(*tube, fixture.context());
    WarningSet                warnings;
    ASSERT_TRUE(handler.endHandler("bodytube", {}, "", warnings).has_value());
    tube->setOuterRadius(0.032);
    EXPECT_EQ(tube->getPresetComponent(), nullptr);
}

TEST(ComponentParameterHandler, HandsAParameterToPlainText)
{
    RocketLoadFixture         fixture;
    ComponentParameterHandler handler(fixture.rocket(), fixture.context());
    WarningSet                warnings;
    // Whatever the name: only when the element closes is it looked up.
    for (const std::string_view element : {"name", "bogus", "length", "stage"})
    {
        const Result<ElementHandler*> opened = handler.openElement(element, {}, warnings);
        EXPECT_EQ(opened.value_or(nullptr), &PlainTextHandler::instance()) << element;
    }
    EXPECT_TRUE(warnings.empty());
}

TEST(ComponentParameterHandler, AppliesAParameterWhenItsElementCloses)
{
    RocketLoadFixture         fixture;
    AxialStage&               stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&                 tube  = stage.addChild(std::make_unique<BodyTube>());
    ComponentParameterHandler handler(tube, fixture.context());
    WarningSet                warnings;

    ASSERT_TRUE(handler.closeElement("length", {}, " 0.75 ", warnings).has_value());
    EXPECT_EQ(tube.getLength(), 0.75);
    // The attributes go to the setter with the text.
    ASSERT_TRUE(
        handler.closeElement("color", {{"red", "1"}, {"green", "2"}, {"blue", "3"}}, "", warnings)
            .has_value());
    const std::optional<Color>& color = tube.getColor();
    EXPECT_EQ(color.has_value() ? color->blue() : -1, 3);
    EXPECT_TRUE(warnings.empty());

    // An element no class of a body tube knows, and one that only another class knows.
    ASSERT_TRUE(handler.closeElement("bogus", {}, "1", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("fincount", {}, "3", warnings).has_value());
    EXPECT_EQ(warningTexts(warnings),
              (Texts{"Unknown parameter type 'bogus' for Body Tube, ignoring.",
                     "Unknown parameter type 'fincount' for Body Tube, ignoring."}));
}

TEST(ComponentParameterHandler, PassesTheFailureOfASetterOn)
{
    RocketLoadFixture         fixture;
    ComponentParameterHandler handler(fixture.rocket(), fixture.context());
    WarningSet                warnings;
    const Result<void>        closed = handler.closeElement("id", {}, "not-a-uuid", warnings);
    ASSERT_FALSE(closed.has_value());
    // What the top-level loader turns into "Exception loading stream: Invalid UUID string: ...".
    EXPECT_EQ(closed.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(closed.error().message, "Invalid UUID string: not-a-uuid");
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
