#include "QtRocket/file/openrocket/ComponentHandler.h"

#include <array>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// ComponentHandler: the handler of a <subcomponents> element. Most of what is checked here is
// read through the handler of the rocket element, as a file is (RocketLoadFixture), and compared
// with what OpenRocket makes of the same text.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentHandler;
using QtRocket::ComponentKind;
using QtRocket::DocumentConfig;
using QtRocket::ElementHandler;
using QtRocket::Material;
using QtRocket::MaterialStorage;
using QtRocket::PodSet;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::WarningSet;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerFixture;
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
// BEGIN GENERATED TABLES ComponentHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 17> kJava{{
    {.name = "ch-every-element", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone/><bodytube><subcomponents><trapezoidfinset/><ellipticalfinset/><freeformfinset/><tubefinset/><launchlug/><railbutton/><engineblock/><innertube/><tubecoupler/><bulkhead/><centeringring/><masscomponent/><shockcord/><parachute/><streamer/><podset/><parallelstage/><boosterset/></subcomponents></bodytube><transition/></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 23 {mass,aero=1, mass,aero,tree=9, mass,tree=9, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.42500000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       EllipticalFinSet 'Elliptical Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] height=0.05
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.04999999999999999,0.0
|       TubeFinSet 'Tube Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=6 autoradius=true thick=0.002
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       RailButton 'Rail Button' axial=MIDDLE:0.0 x=0.1 len=0.0 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Delrin|1420.0|9.46E8|Plastics] diameter=0.0097:0.008 height=0.0097:0.002:0.002:0.0 spacing=0.0582
|       EngineBlock 'Engine Block' axial=BOTTOM:0.0 x=0.195 len=0.005 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TubeCoupler 'Tube Coupler' axial=BOTTOM:0.0 x=0.14 len=0.06 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0095:true radial=0.0:0.0 spacing=0.0
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       ShockCord 'Shock Cord' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cord=1.2750000000000001:true mat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.08937142857142857:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 strip=0.5:0.05
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.2 len=0.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=1 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=2 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|     Transition 'Transition' axial=AFTER:0.0 x=0.35000000000000003 len=0.07500000000000001 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.025:true aft=0.025:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,false,false] motors=0)out"},
    {.name = "ch-unknown-elements", .xml = R"xml(<subcomponents><rocket><name>inner</name></rocket><sleeve/><Stage/><stage><name>ok</name></stage><BODYTUBE/><component/></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element rocket, ignoring.
W Unknown element sleeve, ignoring.
W Unknown element Stage, ignoring.
W Unknown element BODYTUBE, ignoring.
W Unknown element component, ignoring.
ROOT rocket {} []
EVENTS 2 {tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'ok' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "ch-text-and-attributes", .xml = R"xml(<subcomponents>before<stage a="1">text<name>S</name><subcomponents x="y">t2<bodytube b="2">t3</bodytube></subcomponents></stage>after</subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bodytube', ignoring.
W Unknown attributes in element 'bodytube', ignoring.
W Unknown text in element 'stage', ignoring.
W Unknown attributes in element 'stage', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-junit-pod-below-stage", .xml = R"xml(<name>Invalid component hierarchy</name><subcomponents><stage><name>Sustainer</name><subcomponents><podset><name>Invalid pod set</name><subcomponents><bodytube><name>Ignored pod body</name></bodytube></subcomponents></podset><bodytube><name>Valid body tube</name><length>0.3</length><thickness>0.001</thickness><radius>0.02</radius></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Pod Set cannot be attached to Stage; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 8 {mass=1, mass,aero=2, mass,aero,tree=1, nonfunc=2, tree=2}
| Rocket 'Invalid component hierarchy' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Sustainer' axial=AFTER:0.0 x=0.0 len=0.3 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Valid body tube' axial=AFTER:0.0 x=0.0 len=0.3 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:false thick=0.001:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-rocket", .xml = R"xml(<subcomponents><bodytube/><nosecone/><podset/><parallelstage/><boosterset/><masscomponent/><trapezoidfinset/><stage><name>the stage</name></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Body Tube cannot be attached to Rocket; ignoring this component and its subcomponents.
W Nose Cone cannot be attached to Rocket; ignoring this component and its subcomponents.
W Pod Set cannot be attached to Rocket; ignoring this component and its subcomponents.
W Booster Set cannot be attached to Rocket; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Rocket; ignoring this component and its subcomponents.
W Trapezoidal Fin Set cannot be attached to Rocket; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 2 {tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'the stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "ch-below-stage", .xml = R"xml(<subcomponents><stage><subcomponents><trapezoidfinset/><stage/><innertube/><masscomponent/><parachute/><launchlug/><parallelstage/><boosterset/><bulkhead/><nosecone><name>fits</name></nosecone><transition/><bodytube/></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Trapezoidal Fin Set cannot be attached to Stage; ignoring this component and its subcomponents.
W Stage cannot be attached to Stage; ignoring this component and its subcomponents.
W Inner Tube cannot be attached to Stage; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Stage; ignoring this component and its subcomponents.
W Parachute cannot be attached to Stage; ignoring this component and its subcomponents.
W Launch Lug cannot be attached to Stage; ignoring this component and its subcomponents.
W Booster Set cannot be attached to Stage; ignoring this component and its subcomponents.
W Bulkhead cannot be attached to Stage; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=3, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.42500000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'fits' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     Transition 'Transition' axial=AFTER:0.0 x=0.15000000000000002 len=0.07500000000000001 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.025:true aft=-1.0:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.22500000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-bodytube", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><bodytube/><nosecone/><transition/><stage/><innertube><name>fits</name></innertube></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Body Tube cannot be attached to Body Tube; ignoring this component and its subcomponents.
W Nose Cone cannot be attached to Body Tube; ignoring this component and its subcomponents.
W Transition cannot be attached to Body Tube; ignoring this component and its subcomponents.
W Stage cannot be attached to Body Tube; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 6 {mass,aero=2, mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'fits' axial=BOTTOM:0.0 x=0.9299999999999999 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-nosecone", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><subcomponents><bodytube/><trapezoidfinset/><freeformfinset><name>fits</name></freeformfinset><launchlug/><railbutton/><podset/><parallelstage/><masscomponent><name>fits too</name></masscomponent><innertube/><tubefinset/></subcomponents></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Body Tube cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Trapezoidal Fin Set cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Launch Lug cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Rail Button cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Pod Set cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Booster Set cannot be attached to Nose Cone; ignoring this component and its subcomponents.
W Tube Fin Set cannot be attached to Nose Cone; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=2, mass,tree=2, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|       FreeformFinSet 'fits' axial=BOTTOM:0.0 x=0.10000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.05,0.0027106460562619575
|       MassComponent 'fits too' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.08000000000000002 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-transition", .xml = R"xml(<subcomponents><stage><subcomponents><transition><subcomponents><trapezoidfinset/><ellipticalfinset/><freeformfinset><name>fits</name></freeformfinset><parachute><name>fits too</name></parachute><stage/></subcomponents></transition></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Trapezoidal Fin Set cannot be attached to Transition; ignoring this component and its subcomponents.
W Elliptical Fin Set cannot be attached to Transition; ignoring this component and its subcomponents.
W Stage cannot be attached to Transition; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=2, mass,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.07500000000000001 stage=0 sep=EJECTION:0.0:200.0
|     Transition 'Transition' axial=AFTER:0.0 x=0.0 len=0.07500000000000001 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.025:true aft=0.025:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false
|       FreeformFinSet 'fits' axial=BOTTOM:0.0 x=0.02500000000000001 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.05,0.0
|       Parachute 'fits too' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-innertube", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><innertube><subcomponents><innertube><name>fits</name></innertube><bodytube/><trapezoidfinset/><launchlug/><engineblock><name>fits too</name></engineblock><podset/></subcomponents></innertube></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Body Tube cannot be attached to Inner Tube; ignoring this component and its subcomponents.
W Trapezoidal Fin Set cannot be attached to Inner Tube; ignoring this component and its subcomponents.
W Launch Lug cannot be attached to Inner Tube; ignoring this component and its subcomponents.
W Pod Set cannot be attached to Inner Tube; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 9 {mass,aero=2, mass,aero,tree=1, mass,tree=3, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.9299999999999999 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|         InnerTube 'fits' axial=BOTTOM:0.0 x=0.0 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|         EngineBlock 'fits too' axial=BOTTOM:0.0 x=0.065 len=0.005 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.009:true inner=0.009:false radial=0.0:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-leaves", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><trapezoidfinset><subcomponents><masscomponent/></subcomponents></trapezoidfinset><launchlug><subcomponents><masscomponent/></subcomponents></launchlug><railbutton><subcomponents><masscomponent/></subcomponents></railbutton><masscomponent><subcomponents><masscomponent/></subcomponents></masscomponent><parachute><subcomponents><shockcord/></subcomponents></parachute><bulkhead><subcomponents><masscomponent/></subcomponents></bulkhead><centeringring><subcomponents><masscomponent/></subcomponents></centeringring><tubecoupler><subcomponents><masscomponent><name>fits</name></masscomponent></subcomponents></tubecoupler><engineblock><subcomponents><masscomponent/></subcomponents></engineblock><tubefinset><subcomponents><masscomponent/></subcomponents></tubefinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Mass Component cannot be attached to Trapezoidal Fin Set; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Launch Lug; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Rail Button; ignoring this component and its subcomponents.
W Shock Cord cannot be attached to Parachute; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Bulkhead; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Centering Ring; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Engine Block; ignoring this component and its subcomponents.
W Mass Component cannot be attached to Tube Fin Set; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 18 {mass,aero=3, mass,aero,tree=5, mass,tree=8, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.485 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       RailButton 'Rail Button' axial=MIDDLE:0.0 x=0.5 len=0.0 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Delrin|1420.0|9.46E8|Plastics] diameter=0.0097:0.008 height=0.0097:0.002:0.002:0.0 spacing=0.0582
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|         MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.998 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.048:true inner=0.0:false radial=0.0:0.0 spacing=0.0
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.998 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.048:true inner=0.0:true radial=0.0:0.0 spacing=0.0
|       TubeCoupler 'Tube Coupler' axial=BOTTOM:0.0 x=0.94 len=0.06 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.048:true inner=0.048:false radial=0.0:0.0
|         MassComponent 'fits' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       EngineBlock 'Engine Block' axial=BOTTOM:0.0 x=0.995 len=0.005 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.048:true inner=0.048:false radial=0.0:0.0
|       TubeFinSet 'Tube Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=6 autoradius=true thick=0.002
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-below-pods-and-boosters", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><podset><subcomponents><nosecone><name>pod nose</name></nosecone><bodytube><name>pod tube</name></bodytube><podset/><stage/><trapezoidfinset/><parallelstage/></subcomponents></podset><parallelstage><subcomponents><nosecone><name>booster nose</name></nosecone><bodytube><name>booster tube</name></bodytube><parallelstage/><stage/><podset/></subcomponents></parallelstage></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Pod Set cannot be attached to Pod Set; ignoring this component and its subcomponents.
W Stage cannot be attached to Pod Set; ignoring this component and its subcomponents.
W Trapezoidal Fin Set cannot be attached to Pod Set; ignoring this component and its subcomponents.
W Booster Set cannot be attached to Pod Set; ignoring this component and its subcomponents.
W Booster Set cannot be attached to Booster Set; ignoring this component and its subcomponents.
W Stage cannot be attached to Booster Set; ignoring this component and its subcomponents.
W Pod Set cannot be attached to Booster Set; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 14 {mass,aero=2, mass,aero,tree=5, nonfunc=4, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.6499999999999999 len=0.35000000000000003 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|         NoseCone 'pod nose' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|         BodyTube 'pod tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.6499999999999999 len=0.35000000000000003 stage=1 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|         NoseCone 'booster nose' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|         BodyTube 'booster tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "ch-ignored-subtree-is-not-read", .xml = R"xml(<subcomponents><stage><subcomponents><trapezoidfinset><name>F</name><id>not-a-uuid</id><bogus/><subcomponents><sleeve/></subcomponents></trapezoidfinset><bodytube><name>read</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Trapezoidal Fin Set cannot be attached to Stage; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'read' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-attached-before-read", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><radius>0.03</radius></bodytube><bodytube><radius>auto</radius><thickness>0.05</thickness></bodytube><bodytube><radius>0.01</radius></bodytube></subcomponents></stage><stage><subcomponents><bodytube><radius>auto</radius></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass=1, mass,aero=2, mass,aero,tree=4, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.6000000000000001 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.03:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.03:true thick=0.05:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.01:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|   AxialStage 'Stage' axial=AFTER:0.6000000000000001 x=0.6000000000000001 len=0.2 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.01:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,true] motors=0)out"},
    {.name = "ch-stale-shoulder", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><length>0.1</length><aftradius>auto</aftradius><aftshoulderradius>0.04</aftshoulderradius><aftshoulderlength>0.02</aftshoulderlength><aftshoulderthickness>0.03</aftshoulderthickness></nosecone><bodytube><radius>0.05</radius></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 11 {mass=5, mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.30000000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.1 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.05:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.04:0.02:0.03:true flipped=false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.1 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ch-subcomponents-twice", .xml = R"xml(<subcomponents><stage><name>one</name></stage></subcomponents><subcomponents><stage><name>two</name><subcomponents><bodytube/></subcomponents><subcomponents><bodytube/></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=2, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'one' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
|   AxialStage 'two' axial=AFTER:0.0 x=0.0 len=0.4 stage=1 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[false,true] motors=0)out"},
    {.name = "ch-empty-subcomponents", .xml = R"xml(<subcomponents/><subcomponents> </subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 2> kOwn{{
    // The instance budget: 400 launch lugs in each of 400 pods are 160000 instances, so the count is refused and the lug stays one (OpenRocket takes it); the 2 of the second lug fit.
    // OpenRocket: EVENTS 9 {mass,aero=3, mass,aero,tree=4, tree=2}
    // OpenRocket: |           LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=400 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0 ...
    {.name = "ch-fix-instances-pod-and-lug", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><podset><instancecount>400</instancecount><subcomponents><bodytube><subcomponents><launchlug><instancecount>400</instancecount></launchlug><launchlug><instancecount>2</instancecount></launchlug></subcomponents></bodytube></subcomponents></podset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 8 {mass,aero=2, mass,aero,tree=4, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.0 len=0.2 inst=400 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|         BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|           LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|           LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=2 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The instance budget: 300 pods would each hold the 400 launch lugs, so the count of the pod set is refused and stays 2 (OpenRocket takes it); 3 pods fit.
    // OpenRocket: EVENTS 8 {mass,aero=3, mass,aero,tree=3, tree=2}
    {.name = "ch-fix-instances-count-of-the-parent", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><podset><subcomponents><bodytube><subcomponents><launchlug><instancecount>400</instancecount></launchlug></subcomponents></bodytube></subcomponents><instancecount>300</instancecount><instancecount>3</instancecount></podset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 7 {mass,aero=2, mass,aero,tree=3, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.0 len=0.2 inst=3 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|         BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|           LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=400 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES ComponentHandler
// clang-format on

TEST(ComponentHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(ComponentHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(ComponentHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(ComponentHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

// ComponentHandlerTest.testIncompatibleComponentIsIgnoredWithWarning. The Java test loads the
// document with GeneralRocketLoader; here its rocket element is read by the handlers, which is
// all of the document that matters to it (GeneralRocketLoaderTests.cpp has the whole loader).
TEST(ComponentHandler, IncompatibleComponentIsIgnoredWithWarning)
{
    constexpr std::string_view kXml = R"xml(<rocket>
    <name>Invalid component hierarchy</name>
    <subcomponents>
      <stage>
        <name>Sustainer</name>
        <subcomponents>
          <podset>
            <name>Invalid pod set</name>
            <subcomponents>
              <bodytube>
                <name>Ignored pod body</name>
              </bodytube>
            </subcomponents>
          </podset>
          <bodytube>
            <name>Valid body tube</name>
            <length>0.3</length>
            <thickness>0.001</thickness>
            <radius>0.02</radius>
          </bodytube>
        </subcomponents>
      </stage>
    </subcomponents>
  </rocket>)xml";

    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(kXml);
    ASSERT_TRUE(run.result.has_value());

    const Rocket&           rocket = fixture.rocket();
    const AxialStage* const stage  = rocket.getStage(0);
    ASSERT_NE(stage, nullptr);
    ASSERT_EQ(stage->getChildCount(), 1U);
    const auto* const bodyTube = dynamic_cast<const BodyTube*>(&stage->getChild(0));
    ASSERT_NE(bodyTube, nullptr);
    EXPECT_EQ(bodyTube->getName(), "Valid body tube");
    const std::string expectedWarning = PodSet().getComponentName() + " cannot be attached to " +
                                        stage->getComponentName() +
                                        "; ignoring this component and its subcomponents.";
    EXPECT_EQ(run.texts(), Texts{expectedWarning});
    // What the Java test does not look at: the text of the warning, and the tube's values.
    EXPECT_EQ(
        expectedWarning,
        "Pod Set cannot be attached to Stage; ignoring this component and its subcomponents.");
    EXPECT_EQ(bodyTube->getLength(), 0.3);
    EXPECT_EQ(bodyTube->getOuterRadius(), 0.02);
}

TEST(ComponentHandler, AttachesAComponentBeforeAnythingOfItIsRead)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    ComponentHandler  handler(stage, fixture.context());
    WarningSet        warnings;

    const Result<ElementHandler*> opened = handler.openElement("bodytube", {}, warnings);
    ASSERT_TRUE(opened.has_value());
    // The handler of the component's own element, and the component is in the tree already,
    // with the values of a new one.
    EXPECT_NE(opened.value_or(nullptr), nullptr);
    ASSERT_EQ(stage.getChildCount(), 1U);
    EXPECT_EQ(stage.getChild(0).kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(stage.getChild(0).getName(), "Body Tube");
    EXPECT_EQ(&stage.getChild(0).getRocket(), &fixture.rocket());
    EXPECT_TRUE(warnings.empty());

    // The next component is the last child, behind the first.
    const Result<ElementHandler*> second = handler.openElement("nosecone", {}, warnings);
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(stage.getChildCount(), 2U);
    EXPECT_EQ(stage.getChild(1).kind(), ComponentKind::NOSE_CONE);
}

TEST(ComponentHandler, IgnoresWhatItCannotAttach)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    ComponentHandler  handler(stage, fixture.context());
    WarningSet        warnings;

    // Null: the element and everything in it is ignored. Nothing was attached.
    const Result<ElementHandler*> unknown = handler.openElement("sleeve", {}, warnings);
    ASSERT_TRUE(unknown.has_value());
    EXPECT_EQ(unknown.value_or(&handler), nullptr);
    const Result<ElementHandler*> rocket = handler.openElement("rocket", {}, warnings);
    EXPECT_EQ(rocket.value_or(&handler), nullptr);
    const Result<ElementHandler*> misfit = handler.openElement("trapezoidfinset", {}, warnings);
    EXPECT_EQ(misfit.value_or(&handler), nullptr);
    EXPECT_EQ(stage.getChildCount(), 0U);
    EXPECT_EQ(warningTexts(warnings),
              (Texts{"Unknown element sleeve, ignoring.", "Unknown element rocket, ignoring.",
                     "Trapezoidal Fin Set cannot be attached to Stage; ignoring this component "
                     "and its subcomponents."}));
}

/// A chain of @p count inner tubes, each in the one before, as the content of a body tube.
[[nodiscard]] std::string nestedInnerTubes(int count)
{
    std::string xml;
    for (int i = 0; i < count; ++i)
    {
        xml += std::format("<subcomponents><innertube><name>level {}</name>", i + 3);
    }
    for (int i = 0; i < count; ++i)
    {
        xml += "</innertube></subcomponents>";
    }
    return xml;
}

/// How many components stand below @p component in a chain of only children.
[[nodiscard]] int chainBelow(const RocketComponent& component)
{
    int                    depth = 0;
    const RocketComponent* last  = &component;
    while (last->getChildCount() == 1)
    {
        last = &last->getChild(0);
        ++depth;
    }
    return depth;
}

// QtRocket's own rule (OpenRocket has none): components are not nested deeper than kMaxDepth.
TEST(ComponentHandler, DoesNotNestComponentsWithoutBound)
{
    // The stage is at level 1, the body tube at 2, the inner tubes from 3 on.
    const int         tubes = ComponentHandler::kMaxDepth + 3;
    const std::string xml   = std::format(
        "<subcomponents><stage><subcomponents><bodytube>{}</bodytube><bodytube>"
        "<name>behind</name></bodytube></subcomponents></stage></subcomponents>",
        nestedInnerTubes(tubes));

    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(xml);
    ASSERT_TRUE(run.result.has_value());
    // The deepest component stands kMaxDepth levels below the rocket; the tube that would be
    // one level deeper is ignored with what it holds, once, and with one warning.
    EXPECT_EQ(chainBelow(fixture.rocket().getChild(0).getChild(0)),
              ComponentHandler::kMaxDepth - 2);
    EXPECT_EQ(run.texts(), Texts{"Inner Tube is nested too deeply; ignoring this component and "
                                 "its subcomponents."});
    // What follows the chain is read as usual.
    ASSERT_EQ(fixture.rocket().getChild(0).getChildCount(), 2U);
    EXPECT_EQ(fixture.rocket().getChild(0).getChild(1).getName(), "behind");
    EXPECT_EQ(QtRocket::Test::whatUsingTheRocketThrows(fixture), "");
}

TEST(ComponentHandler, TakesAChainOfExactlyTheBound)
{
    const int         tubes = ComponentHandler::kMaxDepth - 2;
    RocketLoadFixture fixture;
    const HandlerRun  run = fixture.load(
        std::format("<subcomponents><stage><subcomponents><bodytube>{}</bodytube></subcomponents>"
                    "</stage></subcomponents>",
                    nestedInnerTubes(tubes)));
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(chainBelow(fixture.rocket().getChild(0).getChild(0)), tubes);
    EXPECT_EQ(run.texts(), Texts{});
}

/// The kind of the component that @p element makes below a parent it fits, or the reason it
/// made none.
[[nodiscard]] std::string kindMadeBy(std::string_view element)
{
    RocketLoadFixture fixture;
    AxialStage&       stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&         tube  = stage.addChild(std::make_unique<BodyTube>());
    // A nose cone and a transition go below the stage, everything else into the tube; a stage
    // goes below the rocket.
    RocketComponent* parent = &tube;
    if (element == "nosecone" || element == "transition" || element == "bodytube")
    {
        parent = &stage;
    }
    else if (element == "stage")
    {
        parent = &fixture.rocket();
    }
    const std::size_t             before = parent->getChildCount();
    ComponentHandler              handler(*parent, fixture.context());
    WarningSet                    warnings;
    const Result<ElementHandler*> opened = handler.openElement(element, {}, warnings);
    if (!opened.has_value() || opened.value_or(nullptr) == nullptr ||
        parent->getChildCount() != before + 1)
    {
        return std::format("nothing made of <{}>", element);
    }
    return std::string(componentKindName(parent->getChild(before).kind()));
}

/// kindMadeBy() of every name the component table has, as "<name>: <kind>".
[[nodiscard]] Texts kindsOfTheTable()
{
    Texts kinds;
    for (const std::string_view element : DocumentConfig::componentElements())
    {
        kinds.push_back(std::format("{}: {}", element, kindMadeBy(element)));
    }
    return kinds;
}

TEST(ComponentHandler, MakesTheComponentOfEveryNameOfTheTable)
{
    EXPECT_EQ(kindsOfTheTable(), (Texts{"bodytube: BODY_TUBE",
                                        "boosterset: PARALLEL_STAGE",
                                        "bulkhead: BULKHEAD",
                                        "centeringring: CENTERING_RING",
                                        "ellipticalfinset: ELLIPTICAL_FIN_SET",
                                        "engineblock: ENGINE_BLOCK",
                                        "freeformfinset: FREEFORM_FIN_SET",
                                        "innertube: INNER_TUBE",
                                        "launchlug: LAUNCH_LUG",
                                        "masscomponent: MASS_COMPONENT",
                                        "nosecone: NOSE_CONE",
                                        "parachute: PARACHUTE",
                                        "parallelstage: PARALLEL_STAGE",
                                        "podset: POD_SET",
                                        "railbutton: RAIL_BUTTON",
                                        "shockcord: SHOCK_CORD",
                                        "stage: AXIAL_STAGE",
                                        "streamer: STREAMER",
                                        "transition: TRANSITION",
                                        "trapezoidfinset: TRAPEZOID_FIN_SET",
                                        "tubecoupler: TUBE_COUPLER",
                                        "tubefinset: TUBE_FIN_SET"}));
}

// ------------------------------------------------------- the default materials of a class
//
// The review's finding: a component the loader made kept the built-in default materials, where
// OpenRocket's constructors ask the application's preferences for the default material of the
// component's class. It shows in a component whose element names no material. The handler now
// completes a new component from the preferences of the loading context, before it attaches
// it. Pinned with OpenRocket: HandlerProbe.java read the two documents below with preferences
// that name twelve defaults (DefaultMaterials.java and out/DefaultMaterials.java.out of the
// probes of tier 9b's fixer of the rocket side).

/// One default material of the preferences: the Java class it is stored for, and a built-in
/// material by its type and name.
struct DefaultMaterial
{
    std::string_view className;
    Material::Type   type;
    std::string_view material;
};

/// The defaults DefaultMaterials.java stores.
constexpr auto kDefaultMaterials = std::to_array<DefaultMaterial>({
    {.className = "BodyTube", .type = Material::Type::BULK, .material = "Fiberglass"},
    {.className = "FinSet", .type = Material::Type::BULK, .material = "Balsa"},
    {.className = "NoseCone", .type = Material::Type::BULK, .material = "Polystyrene"},
    {.className = "RingComponent", .type = Material::Type::BULK, .material = "Plywood (birch)"},
    {.className = "CenteringRing", .type = Material::Type::BULK, .material = "Aluminum"},
    {.className = "ExternalComponent", .type = Material::Type::BULK, .material = "Carbon fiber"},
    // A material of another type than the class takes: it is passed over, and nothing further
    // up the classes is looked at.
    {.className = "LaunchLug", .type = Material::Type::SURFACE, .material = "Silk"},
    // A rail button is made of Delrin whatever the preferences say.
    {.className = "RailButton", .type = Material::Type::BULK, .material = "Brass"},
    {.className = "RecoveryDevice", .type = Material::Type::SURFACE, .material = "Mylar"},
    // The canopy's default is asked for RecoveryDevice, so this one is not seen.
    {.className = "Streamer", .type = Material::Type::SURFACE, .material = "Silk"},
    {.className = "Parachute",
     .type      = Material::Type::LINE,
     .material  = "Braided nylon (2 mm, 1/16 in)"},
    {.className = "ShockCord",
     .type      = Material::Type::LINE,
     .material  = "Tubular nylon (11 mm, 7/16 in)"},
});

/// Stores kDefaultMaterials in @p preferences, each material taken from @p materials, and
/// returns what it stored: a line "<class> = <the material's storable string>" each.
std::string nameDefaultMaterials(Preferences& preferences, const MaterialStorage& materials)
{
    std::string stored;
    for (const DefaultMaterial& one : kDefaultMaterials)
    {
        const std::optional<Material> material = materials.findMaterial(one.type, one.material);
        QtRocket::setDefaultComponentMaterial(preferences, one.className, material);
        stored += std::format("{} = {}\n", one.className,
                              material.has_value() ? material->toStorableString() : "none");
    }
    return stored;
}

// clang-format off
/// What DefaultMaterials.java stored in OpenRocket's preferences, as its preferences print it.
constexpr std::string_view kStoredDefaults =
    "BodyTube = BULK|Fiberglass|1850.0|4.14E9|Composites\n"
    "FinSet = BULK|Balsa|170.0|2.3E8|Woods\n"
    "NoseCone = BULK|Polystyrene|1050.0|1.23E9|Plastics\n"
    "RingComponent = BULK|Plywood (birch)|630.0|6.13E8|Woods\n"
    "CenteringRing = BULK|Aluminum|2700.0|2.6E10|Metals\n"
    "ExternalComponent = BULK|Carbon fiber|1780.0|4.14E9|Composites\n"
    "LaunchLug = SURFACE|Silk|0.06|0.0|Fabrics\n"
    "RailButton = BULK|Brass|8600.0|3.89E10|Metals\n"
    "RecoveryDevice = SURFACE|Mylar|0.021|0.0|Plastics\n"
    "Streamer = SURFACE|Silk|0.06|0.0|Fabrics\n"
    "Parachute = LINE|Braided nylon (2 mm, 1/16 in)|0.001|0.0|Nylons\n"
    "ShockCord = LINE|Tubular nylon (11 mm, 7/16 in)|0.013|0.0|Nylons\n";

constexpr std::string_view kEveryElement = R"xml(<subcomponents><stage><subcomponents><nosecone/><bodytube><subcomponents><trapezoidfinset/><ellipticalfinset/><freeformfinset/><tubefinset/><launchlug/><railbutton/><engineblock/><innertube/><tubecoupler/><bulkhead/><centeringring/><masscomponent/><shockcord/><parachute/><streamer/><podset/><parallelstage/></subcomponents></bodytube><transition/></subcomponents></stage></subcomponents>)xml";
/// OpenRocket's answer to it with those preferences (the case dm-every-element of out/DefaultMaterials.java.out).
constexpr std::string_view kEveryElementInOpenRocket = R"out(RESULT ok
ROOT rocket {} []
EVENTS 22 {mass,aero=1, mass,aero,tree=9, mass,tree=9, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.42500000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Polystyrene|1050.0|1.23E9|Plastics] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Fiberglass|1850.0|4.14E9|Composites] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Balsa|170.0|2.3E8|Woods] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Balsa|170.0|2.3E8|Woods] chord=0.05:0.05 sweep=0.025 height=0.03
|       EllipticalFinSet 'Elliptical Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Balsa|170.0|2.3E8|Woods] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Balsa|170.0|2.3E8|Woods] height=0.05
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Balsa|170.0|2.3E8|Woods] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Balsa|170.0|2.3E8|Woods] points=0.0,0.0;0.025,0.05;0.075,0.05;0.04999999999999999,0.0
|       TubeFinSet 'Tube Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.0 finish=NORMAL mat=[BULK|Carbon fiber|1780.0|4.14E9|Composites] fins=6 autoradius=true thick=0.002
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       RailButton 'Rail Button' axial=MIDDLE:0.0 x=0.1 len=0.0 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Delrin|1420.0|9.46E8|Plastics] diameter=0.0097:0.008 height=0.0097:0.002:0.002:0.0 spacing=0.0582
|       EngineBlock 'Engine Block' axial=BOTTOM:0.0 x=0.195 len=0.005 mat=[BULK|Plywood (birch)|630.0|6.13E8|Woods] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Plywood (birch)|630.0|6.13E8|Woods] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TubeCoupler 'Tube Coupler' axial=BOTTOM:0.0 x=0.14 len=0.06 mat=[BULK|Plywood (birch)|630.0|6.13E8|Woods] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Plywood (birch)|630.0|6.13E8|Woods] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Aluminum|2700.0|2.6E10|Metals] outer=0.023:true inner=0.0095:true radial=0.0:0.0 spacing=0.0
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       ShockCord 'Shock Cord' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cord=1.2750000000000001:true mat=[LINE|Tubular nylon (11 mm, 7/16 in)|0.013|0.0|Nylons]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Mylar|0.021|0.0|Plastics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Braided nylon (2 mm, 1/16 in)|0.001|0.0|Nylons]
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.04468571428571429:true drogue=false mat=[SURFACE|Mylar|0.021|0.0|Plastics] deploy=EJECTION:0.0:200.0 strip=0.5:0.05
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.2 len=0.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=1 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|     Transition 'Transition' axial=AFTER:0.0 x=0.35000000000000003 len=0.07500000000000001 finish=NORMAL mat=[BULK|Carbon fiber|1780.0|4.14E9|Composites] shape=CONICAL:0.0:false fore=0.025:true aft=0.025:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true,false] motors=0)out";

constexpr std::string_view kMaterialsOfTheFile = R"xml(<subcomponents><stage><subcomponents><bodytube><material type="bulk" density="680.0" group="PaperProducts">Cardboard</material><subcomponents><trapezoidfinset><material type="bulk" density="1250.0" group="Plastics">PLA - 100% infill</material><filletmaterial type="bulk" density="680.0" group="PaperProducts">Cardboard</filletmaterial></trapezoidfinset><centeringring><material type="bulk" density="630.0" group="Woods">Plywood (birch)</material></centeringring><parachute><material type="surface" density="0.067" group="Fabrics">Ripstop nylon</material><linematerial type="line" density="0.0018" group="Elastics">Elastic cord (round 2 mm, 1/16 in)</linematerial></parachute><shockcord><material type="line" density="0.0018" group="Elastics">Elastic cord (round 2 mm, 1/16 in)</material></shockcord><streamer/></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml";
/// OpenRocket's answer to it with those preferences (the case dm-the-materials-of-the-file of out/DefaultMaterials.java.out).
constexpr std::string_view kMaterialsOfTheFileInOpenRocket = R"out(RESULT ok
ROOT rocket {} []
EVENTS 14 {mass=7, mass,aero,tree=2, mass,tree=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|PLA - 100% infill|1250.0|2.4E9|Plastics] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Plywood (birch)|630.0|6.13E8|Woods] outer=0.023:true inner=0.0:true radial=0.0:0.0 spacing=0.0
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       ShockCord 'Shock Cord' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cord=0.675:true mat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.04468571428571429:true drogue=false mat=[SURFACE|Mylar|0.021|0.0|Plastics] deploy=EJECTION:0.0:200.0 strip=0.5:0.05
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out";

// clang-format on

/// What reading @p xml gives in a fixture whose preferences name kDefaultMaterials, in the
/// probe's notation; "other defaults" when the preferences do not hold what the probe's held.
[[nodiscard]] std::string readWithDefaultMaterials(std::string_view xml)
{
    RocketLoadFixture fixture;
    if (nameDefaultMaterials(fixture.fixture().preferences(), fixture.fixture().materials()) !=
        kStoredDefaults)
    {
        return "other defaults";
    }
    return fixture.loadAndDescribe(xml);
}

TEST(ComponentHandler, GivesANewComponentTheDefaultMaterialOfItsClassAsOpenRocket)
{
    RocketLoadFixture fixture;
    EXPECT_EQ(nameDefaultMaterials(fixture.fixture().preferences(), fixture.fixture().materials()),
              kStoredDefaults);
    // Fiberglass for the tube, Balsa for the fins and their fillets, Polystyrene for the nose,
    // Carbon fiber for what has no default nearer than ExternalComponent's (the transition, the
    // tube fins), Cardboard for the launch lug, Delrin for the rail button, Aluminum for the
    // centering ring and Plywood for the other rings, Mylar for both canopies; and no event
    // more than without the defaults (22: the case ch-every-element has two components less).
    EXPECT_EQ(readWithDefaultMaterials(kEveryElement), kEveryElementInOpenRocket);
}

TEST(ComponentHandler, TheMaterialsOfAFileReplaceTheDefaultsAsInOpenRocket)
{
    // The streamer, whose element names no material, has the default; the others the file's.
    EXPECT_EQ(readWithDefaultMaterials(kMaterialsOfTheFile), kMaterialsOfTheFileInOpenRocket);
}

/// The material of the body tube a ComponentHandler makes below a new stage of the rocket of
/// @p fixture, as its storable string; "none made" when it makes none.
[[nodiscard]] std::string materialOfANewTube(HandlerFixture& fixture)
{
    AxialStage&                   stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    ComponentHandler              handler(stage, fixture.context());
    WarningSet                    warnings;
    const Result<ElementHandler*> opened = handler.openElement("bodytube", {}, warnings);
    if (!opened.has_value() || stage.getChildCount() != 1 || !warnings.empty())
    {
        return "none made";
    }
    const auto* const tube = dynamic_cast<const BodyTube*>(&stage.getChild(0));
    return tube == nullptr ? "none made" : tube->getMaterial().toStorableString();
}

/// What a loading context may be without.
enum class Lacking
{
    NOTHING,
    PREFERENCES,
    MATERIALS,
};

/// materialOfANewTube() in a fixture whose preferences name kDefaultMaterials and whose context
/// was then left without @p lacking.
[[nodiscard]] std::string materialOfANewTubeLacking(Lacking lacking)
{
    RocketLoadFixture fixture;
    static_cast<void>(
        nameDefaultMaterials(fixture.fixture().preferences(), fixture.fixture().materials()));
    if (lacking == Lacking::PREFERENCES)
    {
        fixture.context().setPreferences(nullptr);
    }
    if (lacking == Lacking::MATERIALS)
    {
        fixture.context().setApplicationMaterials(nullptr);
    }
    return materialOfANewTube(fixture.fixture());
}

/// materialOfANewTube() with application materials that do not hold the built-in ones: the
/// storage of a HandlerFixture as it is made, which holds none. The preferences name
/// kDefaultMaterials (taken from another storage).
[[nodiscard]] std::string materialOfANewTubeWithoutTheBuiltInMaterials()
{
    MaterialStorage builtIn;
    QtRocket::addBuiltinMaterials(builtIn);
    HandlerFixture bare;
    static_cast<void>(nameDefaultMaterials(bare.preferences(), builtIn));
    return materialOfANewTube(bare);
}

// The defaults are given only when the loading context has preferences and application
// materials with the built-in materials the lookup falls back on. Without them a new component
// keeps the built-in default, and nothing is thrown (getDefaultComponentMaterial() has a
// BugError for a storage without the fallback).
TEST(ComponentHandler, KeepsTheBuiltInMaterialWithoutPreferencesOrMaterials)
{
    constexpr std::string_view kFiberglass = "BULK|Fiberglass|1850.0|4.14E9|Composites";
    constexpr std::string_view kCardboard  = "BULK|Cardboard|680.0|4.0E8|PaperProducts";

    EXPECT_EQ(materialOfANewTubeLacking(Lacking::NOTHING), kFiberglass);
    EXPECT_EQ(materialOfANewTubeLacking(Lacking::PREFERENCES), kCardboard);
    EXPECT_EQ(materialOfANewTubeLacking(Lacking::MATERIALS), kCardboard);
    EXPECT_EQ(materialOfANewTubeWithoutTheBuiltInMaterials(), kCardboard);

    // Preferences that name nothing give what the built-in default is.
    RocketLoadFixture unnamed;
    EXPECT_EQ(materialOfANewTube(unnamed.fixture()), kCardboard);
}

}  // namespace
