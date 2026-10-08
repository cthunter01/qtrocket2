#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "TestPaths.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"

// The rocket element as a whole: the scout's cases, a design for each form of the old file
// formats, a design of nested stages, pods and boosters, and the rocket elements of the design
// files of tests/data/ork and data/examples, each read by the handlers as a file is and compared
// with what OpenRocket makes of the same text.

namespace
{

using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::DesignFileCase;
using QtRocket::Test::failedDesignFiles;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::printedDesignFiles;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::rocketElementOfDesignFile;
using QtRocket::Test::RocketLoadFixture;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES rocket_element
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 48> kJava{{
    {.name = "scout-flightconfiguration-element", .xml = R"xml(<flightconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>Cfg</name></flightconfiguration>
<subcomponents><stage><name>S</name></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'flightconfiguration' for Rocket, ignoring.
ROOT rocket {} []
EVENTS 4 {nonfunc=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[false] motors=0
| config 11111111-2222-3333-4444-555555555555 name='Cfg' preload=null active=[false] motors=0)out"},
    {.name = "scout-motorconfiguration-element", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true" extra="x"><name>Cfg</name><stage number="0" active="false"/><bogus/></motorconfiguration>
<subcomponents><stage><name>S</name></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {configid=11111111-2222-3333-4444-555555555555, default=true, extra=x} []
EVENTS 3 {tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0
| config random name='Cfg' preload={0=true} active=[false] motors=0)out"},
    {.name = "scout-motorconfiguration-noid", .xml = R"xml(<motorconfiguration><name>Cfg</name></motorconfiguration>
<subcomponents><stage><name>S</name></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'S' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0
| config random name='Cfg' preload=null active=[false] motors=0)out"},
    {.name = "scout-motorconfiguration-badstage", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"><stage number="x" active="true"/></motorconfiguration>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 0 {}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "scout-position-on-bodytube", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><position type="top">0.1</position><axialoffset method="bogus">0.1</axialoffset><axialoffset method="top">abc</axialoffset></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Warning: info.openrocket.core.file.openrocket.importt.AxialPositionSetter is not valid for class: info.openrocket.core.rocketcomponent.BodyTube
W Invalid parameter encountered, ignoring.
W Warning: invalid value radius position. value=abc    class: info.openrocket.core.rocketcomponent.BodyTube
ROOT rocket {} []
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-radius-angle-on-wrong", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><trapezoidfinset><radiusoffset method="bogus">abc</radiusoffset><angleoffset method="mirror_xy">xx</angleoffset><angleoffset method="mirror_xy">90</angleoffset><radiusoffset>0.1</radiusoffset></trapezoidfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Warning: invalid value radius position. value=abc    class: info.openrocket.core.rocketcomponent.TrapezoidFinSet
W Warning: invalid angle position. value=xx  (degrees)  class: info.openrocket.core.rocketcomponent.TrapezoidFinSet
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:1.5707963267948966 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-unknown-component", .xml = R"xml(<subcomponents><stage><subcomponents><sleeve><name>x</name><subcomponents><bodytube/></subcomponents></sleeve></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element sleeve, ignoring.
ROOT rocket {} []
EVENTS 1 {tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "scout-incompatible-child", .xml = R"xml(<subcomponents><stage><subcomponents><trapezoidfinset><name>F</name></trapezoidfinset><bodytube><subcomponents><bodytube><name>inner</name></bodytube><stage/></subcomponents></bodytube></subcomponents></stage><bodytube/></subcomponents>)xml", .expected = R"out(RESULT ok
W Trapezoidal Fin Set cannot be attached to Stage; ignoring this component and its subcomponents.
W Body Tube cannot be attached to Body Tube; ignoring this component and its subcomponents.
W Stage cannot be attached to Body Tube; ignoring this component and its subcomponents.
W Body Tube cannot be attached to Rocket; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-nosecone-disallowed", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><foreradius>0.1</foreradius><foreshouldercapped>true</foreshouldercapped><bogus>1</bogus><length a="1">0.2</length></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'foreradius' for Nose Cone, ignoring.
W Unknown parameter type 'foreshouldercapped' for Nose Cone, ignoring.
W Unknown parameter type 'bogus' for Nose Cone, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero=1, mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-double-setter", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><radius>auto 0.0125</radius><length>abc</length><thickness>NaN</thickness></bodytube><bodytube><radius>AUTO</radius><length>Infinity</length><thickness>FILLED</thickness></bodytube><bodytube><radius> 0.02 </radius><length>1e-1d</length><thickness>filled 0.001</thickness></bodytube><bodytube><radius>auto  0.03</radius><length>0x1p-3</length></bodytube><bodytube><radius>0.01 auto</radius></bodytube><bodytube><radius>auto abc</radius></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring. data: 'abc' - Body Tube
W Invalid parameter encountered, ignoring. data: 'NaN' - Body Tube
W Invalid parameter encountered, ignoring. data: 'Infinity' - Body Tube
W Invalid parameter encountered, ignoring. data: 'filled 0.001' - Body Tube
W Invalid parameter encountered, ignoring. data: 'auto' - Body Tube
ROOT rocket {} []
EVENTS 15 {mass=1, mass,aero=7, mass,aero,tree=6, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.025 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:true thick=0.02:true mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.1 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.5 len=0.125 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.625 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.825 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-bad-id", .xml = R"xml(<subcomponents><stage><id>not-a-uuid</id></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Invalid UUID string: not-a-uuid
EVENTS 1 {tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "scout-short-id", .xml = R"xml(<subcomponents><stage><id>1-2-3-4-5</id><name> padded name </name></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 2 {tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage ' padded name ' id=00000001-0002-0003-0004-000000000005 axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "scout-bool-int-setters", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><isflipped>TRUE</isflipped><shapeclipped> false </shapeclipped><aftshouldercapped>yes</aftshouldercapped></nosecone><bodytube><subcomponents><trapezoidfinset><fincount> 4</fincount><instancecount>+5</instancecount></trapezoidfinset><trapezoidfinset><fincount>3.0</fincount></trapezoidfinset><launchlug><instancecount>0</instancecount></launchlug></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 8 {mass,aero=2, mass,aero,tree=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.35000000000000003 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.025:false aft=0.0:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.0:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=5 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=5 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-enum-setters", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><shape>Ogive</shape><finish> normal </finish><linestyle>DASHED</linestyle></nosecone><nosecone><shape>haack</shape><finish>finish_polished</finish><linestyle>dashdot</linestyle></nosecone></subcomponents></stage></subcomponents>
<referencetype>custom</referencetype><customreference>0.5</customreference><designtype>commercialkit</designtype><kitname>K</kitname>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 9 {mass,aero=1, mass,aero,tree=2, nonfunc=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=CUSTOM customref=0.5 design=COMMERCIAL_KIT kit='K'
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.30000000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.15000000000000002 len=0.15000000000000002 linestyle=DASHDOT finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=HAACK:0.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-color-setter", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><color/></nosecone><nosecone><color red="1" green="2" blue="3">x</color></nosecone><nosecone><color red="1" green="2" blue="300"/></nosecone><nosecone><color red=" 1" green="2" blue="3" alpha="4"/></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=4, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.6000000000000001 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.15000000000000002 len=0.15000000000000002 color=1,2,3,255 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.30000000000000004 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.45000000000000007 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-tabposition", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><trapezoidfinset><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition>0.01</tabposition></trapezoidfinset><trapezoidfinset><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="front">0.01</tabposition></trapezoidfinset><trapezoidfinset><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="center">0.0</tabposition></trapezoidfinset><trapezoidfinset><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="end">-0.01</tabposition></trapezoidfinset><trapezoidfinset><tabposition relativeto="weird">0.01</tabposition></trapezoidfinset><trapezoidfinset><tabposition relativeto="absolute">0.01</tabposition></trapezoidfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Required attribute 'relativeto' not found for fin tab position.
W Illegal attribute value 'weird' encountered.
ROOT rocket {} []
EVENTS 26 {mass=12, mass,aero=2, mass,aero,tree=7, nonfunc=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.04 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:TOP:0.01:0.01 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.015000000000000001 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999998:0.020000000000000004 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:ABSOLUTE:0.01:0.01 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-material-setter", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><material type="bulk" density="abc">Foo</material></bodytube><bodytube><material type="line" density="100">Foo</material></bodytube><bodytube><material density="100"> </material></bodytube><bodytube><material>Foo</material></bodytube><bodytube><material type="bulk" density="680.0" group="Bogus">Cardboard</material></bodytube><bodytube><material type="bulk" density="680.0" shearModulus="xx">Cardboard</material></bodytube><bodytube><material type="bulk" density="123.0">MyMat</material></bodytube><bodytube><material type="bulk" density="680.0" group="PaperProducts" extra="1">Cardboard</material></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal material specification, ignoring.
W Illegal material type specified, ignoring.
W Illegal material group specified, ignoring.
W Illegal shear modulus value, using 0.0.
ROOT rocket {} []
EVENTS 11 {mass=2, mass,aero,tree=8, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.5999999999999999 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.8 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|0.0|Custom] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.2 len=0.2 finish=NORMAL mat=[BULK|MyMat|123.0|0.0|Custom] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-preset-setter", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><name>BT</name><preset/></bodytube><bodytube><preset manufacturer="Estes"/></bodytube><bodytube><preset manufacturer="Estes" partno="BT-50"/></bodytube><bodytube><preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50" digest="x"/></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid ComponentPreset for component BT, no manufacturer specified.  Ignored
W Invalid ComponentPreset for component Body Tube, no partno specified.  Ignored
W Invalid ComponentPreset for component Body Tube, no digest specified.
W Invalid ComponentPreset for component Body Tube, no type specified.
W No matching ComponentPreset for component Body Tube found matching Estes BT-50
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=4, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.8 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'BT' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.2 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.4 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-cluster-setter", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><clusterconfiguration>3-ring</clusterconfiguration><subcomponents><innertube><clusterconfiguration>bogus</clusterconfiguration></innertube><innertube><clusterconfiguration> 3-ring </clusterconfiguration></innertube><innertube><clusterconfiguration>4-ring</clusterconfiguration><clusterscale>2</clusterscale><clusterrotation>45</clusterrotation></innertube></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'clusterconfiguration' for Body Tube, ignoring.
W Illegal cluster configuration specified.
ROOT rocket {} []
EVENTS 8 {mass=3, mass,aero,tree=1, mass,tree=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=4-ring:2.0:0.7853981633974483 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-motormount-illegal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><motormount><overhang>1</overhang></motormount><finpoints/><deploymentconfiguration/><separationconfiguration/></nosecone><bodytube><motorconfiguration/><flightconfiguration/></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal component defined as motor mount.
W Illegal component defined for fin points.
W Illegal component defined as recovery device.
W Illegal component defined as stage.
W Illegal component defined for motor configuration.
W Illegal component defined for flight configuration.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.35000000000000003 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-motormount-defaults", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent>ejectioncharge</ignitionevent><ignitiondelay>1.5</ignitiondelay><overhang>0.01</overhang><bogus/></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element 'bogus' encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.01 ign=EJECTION_CHARGE:1.5:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-motormount-bad-values", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent> ejectioncharge </ignitionevent><ignitiondelay>x</ignitiondelay><overhang>y</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
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
    {.name = "scout-motormount-ignitionconfig-full", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"/>
<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent> burnout </ignitionevent><ignitiondelay> 2 </ignitiondelay><bogus>t</bogus></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-motor-basic", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><type>single</type><manufacturer>Estes</manufacturer><digest>xyz</digest><designation>C6</designation><diameter>0.018</diameter><length>0.07</length><delay>5.0</delay></motor><motor><designation>B6</designation><delay>none</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, motor=1, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false motor[random]=F12X:Infinity:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=1
| config random name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "scout-deployment", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"/>
<subcomponents><stage><subcomponents><bodytube><subcomponents><parachute><deployevent>altitude</deployevent><deployaltitude>100</deployaltitude><deploydelay>1</deploydelay><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent> apogee </deployevent><deploydelay>x</deploydelay><bogus a="1">t</bogus></deploymentconfiguration><deploymentconfiguration><deployevent>bogus</deployevent></deploymentconfiguration><cd>AUTO</cd><linelength>auto</linelength><isdrogue>true</isdrogue></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=true mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=ALTITUDE:1.0:100.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.0:100.0 deploy[random]=ALTITUDE:1.0:100.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-separation", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"/>
<subcomponents><stage><separationevent>upperignition</separationevent><separationdelay>2</separationdelay><separationaltitude>50</separationaltitude><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>upper_ignition</separationevent><separationdelay>3</separationdelay></separationconfiguration></stage><stage><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationaltitude>7</separationaltitude></separationconfiguration></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=UPPER_IGNITION:2.0:50.0 sep[11111111-2222-3333-4444-555555555555]=UPPER_IGNITION:3.0:50.0
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=1 sep=EJECTION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:0.0:7.0
| selected=default
| config default name='[{motors}]' preload=null active=[false,false] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[false,false] motors=0)out"},
    {.name = "scout-finpoints-a", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="end">-0.01</tabposition><finpoints a="1">txt<point x="0" y="0"/><point x="0.1" y="0.05" z="1">t</point><bogus x="0.2" y="0.05"/><point x="0.2"/><point x="a" y="0"/><point x="0.3" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'point', ignoring.
W Unknown attributes in element 'point', ignoring.
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 9 {mass=3, mass,aero=2, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.7 len=0.3 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.010000000000000009:0.26999999999999996 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.1,0.05;0.2,0.05;0.30000000000000004,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-finpoints-two", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-finpoints-one", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><finpoints><point x="0" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=1.0 len=0.0 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.0:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-finpoints-selfintersect", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.1" y="0.1"/><point x="0.0" y="0.1"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-rocket-params", .xml = R"xml(<name>My Rocket</name><id>2b0e36d2-9a3f-4d2c-8f10-000000000001</id><designer>Me</designer><revision>rev
2</revision><comment>c</comment><referencetype>nosecone</referencetype><customreference>abc</customreference><designtype>kit_bash</designtype><designtype>kitbash</designtype><optimizationflight>true</optimizationflight><color red="1" green="2" blue="3"/><linestyle>solid</linestyle><finish>normal</finish><instancecount>3</instancecount><preset/>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring. data: 'abc' - My Rocket
W Invalid parameter encountered, ignoring.
W Unknown parameter type 'optimizationflight' for Rocket, ignoring.
W Unknown parameter type 'finish' for Rocket, ignoring.
W Unknown parameter type 'instancecount' for Rocket, ignoring.
W Invalid ComponentPreset for component My Rocket, no manufacturer specified.  Ignored
ROOT rocket {} []
EVENTS 8 {nonfunc=8}
| Rocket 'My Rocket' id=2b0e36d2-9a3f-4d2c-8f10-000000000001 axial=ABSOLUTE:0.0 x=0.0 comment='c' color=1,2,3,255 linestyle=SOLID designer='Me' revision='rev\n2' ref=NOSECONE customref=0.01 design=KIT_BASH
| selected=default
| config default name='[{motors}]' preload=null active=[] motors=0)out"},
    {.name = "scout-slip-motor", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><manufacturer>Estes</manufacturer><delay>3</delay><foo><bar/></foo></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, motor=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[random]=F12X:3.0:0.0:AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config random name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "scout-slip-param", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>0.5<x/></length><radius>0.03</radius><name>N<b/>M</name></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
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
    {.name = "scout-slip-component", .xml = R"xml(<subcomponents><stage><subcomponents><sleeve/><bodytube><name>after</name></bodytube></subcomponents><name>StageName</name></stage></subcomponents><name>RocketName</name>)xml", .expected = R"out(RESULT ok
W Unknown element sleeve, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=2, tree=2}
| Rocket 'RocketName' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'StageName' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'after' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-after-axialoffset", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><bulkhead><axialoffset method="after">0.1</axialoffset></bulkhead><bulkhead><axialoffset method="absolute">0.1</axialoffset></bulkhead></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 8 {mass,aero=2, mass,aero,tree=1, mass,tree=2, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Bulkhead 'Bulkhead' axial=AFTER:0.0 x=0.0 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
|       Bulkhead 'Bulkhead' axial=ABSOLUTE:0.1 x=0.1 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-podset-after", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><podset><axialoffset method="after">0.1</axialoffset></podset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=1, mass,aero,tree=1, nonfunc=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       PodSet 'Pod Set' axial=TOP:0.1 x=0.1 len=0.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-parallelstage-positions", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><parallelstage><instancecount>3</instancecount><radiusoffset method="surface">0.5</radiusoffset><angleoffset method="fixed">30</angleoffset><axialoffset method="bottom">0.01</axialoffset></parallelstage><boosterset><name>legacy</name></boosterset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 13 {mass,aero=6, mass,aero,tree=1, nonfunc=2, tree=4}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'Booster Set' axial=BOTTOM:0.010000000000000009 x=0.21000000000000002 len=0.0 stage=1 sep=EJECTION:0.0:200.0 inst=3 radius=SURFACE:0.0 angle=FIXED:0.5235987755982988
|       ParallelStage 'legacy' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=2 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true,false,false] motors=0)out"},
    {.name = "scout-stage-in-stage", .xml = R"xml(<subcomponents><stage><name>outer</name><subcomponents><stage><name>inner</name></stage></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Stage cannot be attached to Stage; ignoring this component and its subcomponents.
ROOT rocket {} []
EVENTS 2 {tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'outer' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "scout-rocket-in-rocket", .xml = R"xml(<subcomponents><rocket><name>inner</name></rocket><stage/></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element rocket, ignoring.
ROOT rocket {} []
EVENTS 1 {tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "scout-legacy-v10", .xml = R"xml(<name>Old</name>
<motorconfiguration configid="abc"><name>one</name></motorconfiguration>
<motorconfiguration configid="def" default="true"/>
<referencetype>maximum</referencetype>
<subcomponents><stage><name>Sustainer</name><subcomponents>
<nosecone><name>Nose</name><finish>normal</finish><material type="bulk" density="1250.0">PVC</material><length>0.1</length><thickness>0.002</thickness><shape>ogive</shape><shapeparameter>1.0</shapeparameter><aftradius>auto</aftradius><aftshoulderradius>0.0</aftshoulderradius><aftshoulderlength>0.0</aftshoulderlength><aftshoulderthickness>0.0</aftshoulderthickness><aftshouldercapped>false</aftshouldercapped></nosecone>
<bodytube><name>Body</name><finish>normal</finish><material type="bulk" density="680.0">Cardboard</material><length>0.3</length><thickness>0.002</thickness><radius>0.0125</radius>
<subcomponents>
<trapezoidfinset><name>Fins</name><position type="bottom">0.0</position><finish>normal</finish><material type="bulk" density="680.0">Cardboard</material><fincount>3</fincount><rotation>10.0</rotation><thickness>0.003</thickness><crosssection>square</crosssection><cant>0.0</cant><rootchord>0.05</rootchord><tipchord>0.05</tipchord><sweeplength>0.025</sweeplength><height>0.03</height></trapezoidfinset>
<launchlug><name>Lug</name><position type="top">0.05</position><finish>normal</finish><material type="bulk" density="680.0">Cardboard</material><radius>0.003</radius><length>0.03</length><thickness>0.001</thickness><radialdirection>45.0</radialdirection></launchlug>
<innertube><name>Mount</name><position type="bottom">0.0</position><material type="bulk" density="680.0">Cardboard</material><length>0.07</length><radialposition>0.0</radialposition><radialdirection>0.0</radialdirection><outerradius>0.009</outerradius><thickness>0.0005</thickness><clusterconfiguration>single</clusterconfiguration><clusterscale>1.0</clusterscale><clusterrotation>0.0</clusterrotation>
<motormount><ignitionevent>automatic</ignitionevent><ignitiondelay>0.0</ignitiondelay><overhang>0.003</overhang><motor configid="abc"><type>single</type><manufacturer>Estes</manufacturer><digest>oldformat</digest><designation>C6</designation><diameter>0.018</diameter><length>0.07</length><delay>5.0</delay></motor><motor configid="def"><designation>B4</designation><delay>none</delay></motor></motormount></innertube>
<parachute><name>Chute</name><position type="top">0.02</position><overridemass>0.01</overridemass><overridesubcomponents>false</overridesubcomponents><packedlength>0.025</packedlength><packedradius>0.0125</packedradius><radialposition>0.0</radialposition><radialdirection>0.0</radialdirection><cd>auto</cd><material type="surface" density="0.067">Ripstop nylon</material><deployevent>ejection</deployevent><deployaltitude>200.0</deployaltitude><deploydelay>0.0</deploydelay><diameter>0.3</diameter><linecount>6</linecount><linelength>0.3</linelength><linematerial type="line" density="0.0018">Elastic cord (round 2mm, 1/16 in)</linematerial></parachute>
</subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 38 {mass=5, mass,aero=12, mass,aero,tree=4, mass,tree=2, motor=1, nonfunc=10, tree=4}
| Rocket 'Old' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Sustainer' axial=AFTER:0.0 x=0.0 len=0.4 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose' axial=AFTER:0.0 x=0.0 len=0.1 finish=NORMAL mat=[BULK|PVC|1250.0|0.0|Custom] shape=OGIVE:1.0:false fore=0.0:false aft=0.0125:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Body' axial=AFTER:0.0 x=0.1 len=0.3 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.0125:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Fins' axial=BOTTOM:0.0 x=0.25 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.17453292519943295 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       LaunchLug 'Lug' axial=TOP:0.05 x=0.05 len=0.03 inst=1 angle=RELATIVE:0.7853981633974483 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.003 thick=0.001 spacing=0.06
|       InnerTube 'Mount' axial=BOTTOM:0.0 x=0.22999999999999998 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.009:false inner=0.008499999999999999:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[00000000-0000-0000-0000-000000017862]=F12X:5.0:0.0:AUTOMATIC:0.0:true motor[00000000-0000-0000-0000-000000018405]=F12X:Infinity:0.0:AUTOMATIC:0.0:true
|       Parachute 'Chute' axial=TOP:0.02 x=0.02 len=0.025 massovr=0.01 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.3:false linemat=[LINE|Elastic cord (round 2mm, 1/16 in)|0.0018|0.0|Custom]
| selected=00000000-0000-0000-0000-000000018405
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 00000000-0000-0000-0000-000000017862 name='one' preload=null active=[true] motors=1
| config 00000000-0000-0000-0000-000000018405 name='[{motors}]' preload=null active=[true] motors=1)out"},
    {.name = "legacy-position-with-type", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><innertube><name>top</name><position type="top">0.1</position></innertube><innertube><name>middle</name><position type="middle">0.1</position></innertube><innertube><name>bottom</name><position type="bottom">-0.1</position></innertube><innertube><name>absolute</name><position type="absolute">0.4</position></innertube><innertube><name>after</name><position type="after">0.1</position></innertube><trapezoidfinset><name>fins</name><position type="bottom">0.0</position></trapezoidfinset><launchlug><name>lug</name><position type="top">0.05</position></launchlug><parachute><name>chute</name><position type="top">0.02</position></parachute><masscomponent><name>both</name><axialoffset method="top">0.3</axialoffset><position type="top">0.3</position></masscomponent><masscomponent><name>both, differing</name><axialoffset method="top">0.3</axialoffset><position type="bottom">-0.2</position></masscomponent><masscomponent><name>method and type</name><position method="middle" type="top">0.1</position></masscomponent><masscomponent><name>no type</name><position>0.1</position></masscomponent><masscomponent><name>unknown type</name><position type="front">0.1</position></masscomponent></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 54 {mass,aero=15, mass,aero,tree=3, mass,tree=11, nonfunc=24, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'top' axial=TOP:0.1 x=0.1 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'middle' axial=MIDDLE:0.1 x=0.565 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'bottom' axial=BOTTOM:-0.1 x=0.83 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'absolute' axial=ABSOLUTE:0.4 x=0.4 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       InnerTube 'after' axial=AFTER:0.1 x=1.1 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'fins' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       LaunchLug 'lug' axial=TOP:0.05 x=0.05 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       Parachute 'chute' axial=TOP:0.02 x=0.02 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       MassComponent 'both' axial=TOP:0.3 x=0.3 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       MassComponent 'both, differing' axial=BOTTOM:-0.2 x=0.7749999999999999 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       MassComponent 'method and type' axial=MIDDLE:0.1 x=0.5875 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       MassComponent 'no type' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       MassComponent 'unknown type' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-fincount", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><trapezoidfinset><name>four</name><fincount>4</fincount></trapezoidfinset><ellipticalfinset><name>two</name><fincount>2</fincount></ellipticalfinset><freeformfinset><name>five</name><fincount>5</fincount></freeformfinset><tubefinset><name>six</name><fincount>6</fincount></tubefinset><trapezoidfinset><name>both</name><instancecount>3</instancecount><fincount>3</fincount></trapezoidfinset><trapezoidfinset><name>both, differing</name><instancecount>3</instancecount><fincount>5</fincount></trapezoidfinset><trapezoidfinset><name>too many</name><fincount>9</fincount></trapezoidfinset><trapezoidfinset><name>none</name><fincount>0</fincount></trapezoidfinset><trapezoidfinset><name>no number</name><fincount>three</fincount></trapezoidfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 29 {mass,aero=9, mass,aero,tree=10, nonfunc=9, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'four' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=4 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=4 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       EllipticalFinSet 'two' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=2 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=2 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] height=0.05
|       FreeformFinSet 'five' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=5 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=5 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
|       TubeFinSet 'six' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=6 autoradius=true thick=0.002
|       TrapezoidFinSet 'both' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'both, differing' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=5 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=5 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'too many' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=8 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=8 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'none' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=1 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=1 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'no number' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><trapezoidfinset><name>ten</name><rotation>10.0</rotation></trapezoidfinset><ellipticalfinset><name>ninety</name><rotation>90</rotation></ellipticalfinset><freeformfinset><name>minus</name><rotation>-45</rotation></freeformfinset><tubefinset><name>tube fins</name><rotation>30</rotation></tubefinset><trapezoidfinset><name>both</name><angleoffset method="relative">20.0</angleoffset><rotation>20.0</rotation></trapezoidfinset><trapezoidfinset><name>a turn and more</name><rotation>400</rotation></trapezoidfinset><trapezoidfinset><name>no number</name><rotation>x</rotation></trapezoidfinset><launchlug><name>no rotation for a lug</name><rotation>10</rotation></launchlug></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring. data: 'x' - no number
W Unknown parameter type 'rotation' for Launch Lug, ignoring.
ROOT rocket {} []
EVENTS 28 {mass,aero=10, mass,aero,tree=9, nonfunc=8, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'ten' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.17453292519943295 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       EllipticalFinSet 'ninety' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:1.5707963267948966 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] height=0.05
|       FreeformFinSet 'minus' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:-0.7853981633974483 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
|       TubeFinSet 'tube fins' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.5235987755982988 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=6 autoradius=true thick=0.002
|       TrapezoidFinSet 'both' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.3490658503988659 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'a turn and more' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.6981317007977319 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'no number' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       LaunchLug 'no rotation for a lug' axial=MIDDLE:0.0 x=0.485 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-relativeto", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><trapezoidfinset><name>front</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="front">0.01</tabposition></trapezoidfinset><trapezoidfinset><name>center</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="center">0.0</tabposition></trapezoidfinset><trapezoidfinset><name>end</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="end">-0.01</tabposition></trapezoidfinset><trapezoidfinset><name>top</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="top">0.01</tabposition></trapezoidfinset><trapezoidfinset><name>middle</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="middle">0.0</tabposition></trapezoidfinset><trapezoidfinset><name>bottom</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="bottom">-0.01</tabposition></trapezoidfinset><trapezoidfinset><name>twice</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition relativeto="end">-0.01</tabposition><tabposition relativeto="bottom">-0.01</tabposition></trapezoidfinset><trapezoidfinset><name>none</name><rootchord>0.1</rootchord><tablength>0.02</tablength><tabheight>0.01</tabheight><tabposition>0.01</tabposition></trapezoidfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Required attribute 'relativeto' not found for fin tab position.
ROOT rocket {} []
EVENTS 60 {mass=24, mass,aero=10, mass,aero,tree=9, nonfunc=16, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'front' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:TOP:0.01:0.01 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'center' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.04 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'end' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999995:0.07 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'top' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:TOP:0.01:0.01 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'middle' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.04 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'bottom' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999995:0.07 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'twice' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999995:0.07 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
|       TrapezoidFinSet 'none' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.04 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.1:0.05 sweep=0.025 height=0.03
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-overridesubcomponents", .xml = R"xml(<subcomponents><stage><overridemass>1</overridemass><overridesubcomponents>true</overridesubcomponents><subcomponents><bodytube><name>all</name><overridemass>0.1</overridemass><overridecg>0.2</overridecg><overridecd>0.3</overridecd><overridesubcomponents>true</overridesubcomponents><subcomponents><masscomponent><name>inside</name><overridemass>0.01</overridemass><overridesubcomponents>false</overridesubcomponents></masscomponent></subcomponents></bodytube><bodytube><name>mass only</name><overridemass>0.1</overridemass><overridesubcomponents>true</overridesubcomponents></bodytube><bodytube><name>flag only</name><overridesubcomponents>true</overridesubcomponents></bodytube><bodytube><name>new flags</name><overridemass>0.1</overridemass><overridesubcomponentsmass>true</overridesubcomponentsmass><overridecg>0.2</overridecg><overridesubcomponentscg>false</overridesubcomponentscg><overridecd>0.3</overridecd><overridesubcomponentscd>true</overridesubcomponentscd></bodytube><bodytube><name>old then new</name><overridesubcomponents>true</overridesubcomponents><overridesubcomponentscg>false</overridesubcomponentscg></bodytube><bodytube><name>no flag</name><overridesubcomponents>yes</overridesubcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 46 {aero=2, aero,treechild=6, mass=7, mass,aero,tree=6, mass,tree=1, mass,treechild=12, nonfunc=11, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.2 massovr=1.0 subovr=true,true,true stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'all' axial=AFTER:0.0 x=0.0 len=0.2 massovr=0.1 cgovr=0.2 cdovr=0.3 subovr=true,true,true finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       MassComponent 'inside' axial=TOP:0.0 x=0.0 len=0.025 massovr=0.01 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|     BodyTube 'mass only' axial=AFTER:0.0 x=0.2 len=0.2 massovr=0.1 subovr=true,true,true finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'flag only' axial=AFTER:0.0 x=0.4 len=0.2 subovr=true,true,true finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'new flags' axial=AFTER:0.0 x=0.6000000000000001 len=0.2 massovr=0.1 cgovr=0.2 cdovr=0.3 subovr=true,false,true finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'old then new' axial=AFTER:0.0 x=0.8 len=0.2 subovr=true,false,true finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|     BodyTube 'no flag' axial=AFTER:0.0 x=1.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-radialdirection", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><launchlug><name>lug</name><radialdirection>45.0</radialdirection></launchlug><launchlug><name>both</name><angleoffset method="relative">30</angleoffset><radialdirection>30.0</radialdirection></launchlug><innertube><name>tube</name><radialposition>0.01</radialposition><radialdirection>90.0</radialdirection></innertube><masscomponent><name>mass</name><radialposition>0.02</radialposition><radialdirection>180.0</radialdirection></masscomponent><parachute><name>chute</name><radialposition>0.0</radialposition><radialdirection>0.0</radialdirection></parachute><railbutton><name>no radial direction for a button</name><radialdirection>45</radialdirection></railbutton></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown parameter type 'radialdirection' for Rail Button, ignoring.
ROOT rocket {} []
EVENTS 22 {mass=4, mass,aero=4, mass,aero,tree=4, mass,tree=3, nonfunc=6, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       LaunchLug 'lug' axial=MIDDLE:0.0 x=0.485 len=0.03 inst=1 angle=RELATIVE:0.7853981633974483 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       LaunchLug 'both' axial=MIDDLE:0.0 x=0.485 len=0.03 inst=1 angle=RELATIVE:0.5235987755982988 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06
|       InnerTube 'tube' axial=BOTTOM:0.0 x=0.9299999999999999 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.01:1.5707963267948966 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       MassComponent 'mass' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.02:3.141592653589793 mass=0.0 type=MASSCOMPONENT
|       Parachute 'chute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       RailButton 'no radial direction for a button' axial=MIDDLE:0.0 x=0.5 len=0.0 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Delrin|1420.0|9.46E8|Plastics] diameter=0.0097:0.008 height=0.0097:0.002:0.002:0.0 spacing=0.0582
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "legacy-boosterset", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"></motorconfiguration><subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><boosterset><name>Boosters</name><instancecount>2</instancecount><radiusoffset method="surface">0.0</radiusoffset><angleoffset method="relative">90.0</angleoffset><axialoffset method="bottom">0.0</axialoffset><position type="bottom">0.0</position><separationevent>burnout</separationevent><separationdelay>1.0</separationdelay><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>ejection</separationevent><separationdelay>0.5</separationdelay></separationconfiguration><subcomponents><nosecone><name>Booster nose</name><length>0.1</length><aftradius>0.02</aftradius></nosecone><bodytube><name>Booster tube</name><length>0.4</length><radius>0.02</radius><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></boosterset><parallelstage><name>New name</name><instancecount>3</instancecount></parallelstage></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 29 {mass,aero=13, mass,aero,tree=3, motor=1, nonfunc=6, tree=6}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'Boosters' axial=BOTTOM:0.0 x=0.5 len=0.5 stage=1 sep=BURNOUT:1.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.5:200.0 inst=2 radius=SURFACE:0.0 angle=RELATIVE:1.5707963267948966
|         NoseCone 'Booster nose' axial=AFTER:0.0 x=0.0 len=0.1 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.02:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|         BodyTube 'Booster tube' axial=AFTER:0.0 x=0.1 len=0.4 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:false thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
|       ParallelStage 'New name' axial=BOTTOM:0.0 x=1.0 len=0.0 stage=2 sep=EJECTION:0.0:200.0 inst=3 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true,true,false] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true,true,false] motors=1)out"},
    {.name = "nested-stages-pods-and-boosters", .xml = R"xml(<name>Nested</name><motorconfiguration configid="11111111-2222-3333-4444-555555555555" default="true"><name>all</name><stage number="0" active="true"/><stage number="1" active="true"/><stage number="2" active="true"/><stage number="3" active="false"/></motorconfiguration><motorconfiguration configid="22222222-3333-4444-5555-666666666666"><stage number="0" active="true"/><stage number="1" active="false"/><stage number="2" active="false"/><stage number="3" active="false"/></motorconfiguration><subcomponents><stage><name>Sustainer</name><subcomponents><nosecone><name>Nose</name><length>0.15</length><aftradius>0.03</aftradius></nosecone><bodytube><name>Sustainer tube</name><length>0.5</length><radius>0.03</radius><motormount><ignitionevent>ejectioncharge</ignitionevent><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount><subcomponents><podset><name>Pods</name><instancecount>3</instancecount><radiusoffset method="surface">0.0</radiusoffset><angleoffset method="relative">60.0</angleoffset><axialoffset method="top">0.1</axialoffset><subcomponents><nosecone><name>Pod nose</name><length>0.05</length><aftradius>0.01</aftradius></nosecone><bodytube><name>Pod tube</name><length>0.2</length><radius>0.01</radius><subcomponents><parachute><name>Pod chute</name><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deployevent>apogee</deployevent><deploydelay>1.5</deploydelay><deployaltitude>120</deployaltitude></deploymentconfiguration></parachute><podset><name>Pods on a pod</name><instancecount>2</instancecount><subcomponents><bodytube><name>Pod pod tube</name><length>0.05</length><radius>0.004</radius></bodytube></subcomponents></podset></subcomponents></bodytube></subcomponents></podset><parallelstage><name>Boosters</name><instancecount>2</instancecount><radiusoffset method="surface">0.0</radiusoffset><angleoffset method="relative">0.0</angleoffset><axialoffset method="bottom">0.0</axialoffset><separationevent>burnout</separationevent><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>burnout</separationevent><separationdelay>1.5</separationdelay><separationaltitude>120</separationaltitude></separationconfiguration><subcomponents><nosecone><name>Booster nose</name><length>0.1</length><aftradius>0.02</aftradius></nosecone><bodytube><name>Booster tube</name><length>0.4</length><radius>0.02</radius><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount><subcomponents><trapezoidfinset><name>Booster fins</name><instancecount>2</instancecount></trapezoidfinset><parallelstage><name>Boosters on a booster</name><instancecount>2</instancecount><subcomponents><bodytube><name>Small booster tube</name><length>0.1</length><radius>0.008</radius><motormount><motor configid="22222222-3333-4444-5555-666666666666"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></parallelstage></subcomponents></bodytube></subcomponents></parallelstage><trapezoidfinset><name>Fins</name><instancecount>3</instancecount></trapezoidfinset></subcomponents></bodytube></subcomponents></stage><stage><name>Lower stage</name><separationevent>upperignition</separationevent><separationconfiguration configid="11111111-2222-3333-4444-555555555555"><separationevent>ejection</separationevent></separationconfiguration><separationconfiguration configid="22222222-3333-4444-5555-666666666666"><separationdelay>2</separationdelay></separationconfiguration><subcomponents><bodytube><name>Lower tube</name><length>0.3</length><radius>0.03</radius><motormount><motor configid="11111111-2222-3333-4444-555555555555"><designation>C6</designation><delay>5.0</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 77 {mass,aero=29, mass,aero,tree=11, mass,tree=1, motor=4, nonfunc=20, tree=12}
| Rocket 'Nested' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Sustainer' axial=AFTER:0.0 x=0.0 len=0.65 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose' axial=AFTER:0.0 x=0.0 len=0.15 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.03:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|     BodyTube 'Sustainer tube' axial=AFTER:0.0 x=0.15 len=0.5 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.03:false thick=0.002:false mount=true overhang=0.0 ign=EJECTION_CHARGE:0.0:true motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:BURNOUT:2.0:true motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:EJECTION_CHARGE:0.0:true
|       PodSet 'Pods' axial=TOP:0.1 x=0.1 len=0.25 inst=3 radius=SURFACE:0.0 angle=RELATIVE:1.0471975511965976
|         NoseCone 'Pod nose' axial=AFTER:0.0 x=0.0 len=0.05 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.01:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|         BodyTube 'Pod tube' axial=AFTER:0.0 x=0.05 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.01:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|           Parachute 'Pod chute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=APOGEE:1.5:120.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|           PodSet 'Pods on a pod' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|             BodyTube 'Pod pod tube' axial=AFTER:0.0 x=0.0 len=0.05 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.004:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       ParallelStage 'Boosters' axial=BOTTOM:0.0 x=0.0 len=0.5 stage=1 sep=BURNOUT:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=BURNOUT:1.5:120.0 inst=2 radius=SURFACE:0.0 angle=RELATIVE:0.0
|         NoseCone 'Booster nose' axial=AFTER:0.0 x=0.0 len=0.1 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.02:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|         BodyTube 'Booster tube' axial=AFTER:0.0 x=0.1 len=0.4 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.02:false thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
|           TrapezoidFinSet 'Booster fins' axial=BOTTOM:0.0 x=0.35000000000000003 len=0.05 inst=2 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=2 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|           ParallelStage 'Boosters on a booster' axial=BOTTOM:0.0 x=0.30000000000000004 len=0.1 stage=2 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|             BodyTube 'Small booster tube' axial=AFTER:0.0 x=0.0 len=0.1 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.008:false thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[22222222-3333-4444-5555-666666666666]=F12X:5.0:0.0:AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Fins' axial=BOTTOM:0.0 x=0.45 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|   AxialStage 'Lower stage' axial=AFTER:0.65 x=0.65 len=0.3 stage=3 sep=UPPER_IGNITION:0.0:200.0 sep[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 sep[22222222-3333-4444-5555-666666666666]=UPPER_IGNITION:2.0:200.0
|     BodyTube 'Lower tube' axial=AFTER:0.0 x=0.0 len=0.3 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.03:false thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false motor[11111111-2222-3333-4444-555555555555]=F12X:5.0:0.0:AUTOMATIC:0.0:false
| selected=11111111-2222-3333-4444-555555555555
| config default name='[{motors}]' preload=null active=[true,true,true,true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='all' preload={0=true, 1=true, 2=true, 3=false} active=[true,true,true,true] motors=3
| config 22222222-3333-4444-5555-666666666666 name='[{motors}]' preload={0=true, 1=false, 2=false, 3=false} active=[true,true,true,true] motors=2)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 11> kOwn{{
    // Decision L3 in OverrideSetter (part R1): an override mass of NaN is refused (OpenRocket stores it).
    // OpenRocket: EVENTS 7 {aero,treechild=1, mass=2, mass,treechild=2, nonfunc=1, tree=1}
    // OpenRocket: |   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 massovr=NaN cgovr=0.5 subovr=true,true,true stage=0 sep=EJECTION:0.0:200.0
    {.name = "scout-override-setters", .xml = R"xml(<subcomponents><stage><overridemass>NaN</overridemass><overridecg> 0.5 </overridecg><overridecd>abc</overridecd><overridesubcomponents>true</overridesubcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 6 {aero,treechild=1, mass=1, mass,treechild=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 cgovr=0.5 subovr=true,true,true stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    // Decision L4: without a delay OpenRocket dies of a NullPointerException; here the event is applied (to the default, the id having no motor) and a warning added.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "java.lang.Double.doubleValue()" because "this.ignitionConfigHandler.ignitionDelay" is nul ...
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "scout-motormount-ignitionconfig-partial", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitionevent>burnout</ignitionevent></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=BURNOUT:0.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Without an event OpenRocket stores null as the ignition event (of the default: the id has no motor); here the event stays.
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "scout-motormount-ignitionconfig-noevent", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="11111111-2222-3333-4444-555555555555"><ignitiondelay>2</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:2.0:true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: the third fin set has no point, of which OpenRocket dies; here its outline stays (the warning is the one the first fin set's points gave already).
    // OpenRocket: RESULT THROWN java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
    {.name = "scout-finpoints", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="end">-0.01</tabposition><finpoints><point x="0" y="0"/><point x="0.1" y="0.05" z="1">t</point><bogus x="0.2" y="0.05"/><point x="0.2"/><point x="a" y="0"/><point x="0.3" y="0"/></finpoints></freeformfinset><freeformfinset><finpoints><point x="0" y="0"/><point x="0.1" y="0"/></finpoints></freeformfinset><freeformfinset><finpoints/></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'point', ignoring.
W Unknown attributes in element 'point', ignoring.
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 12 {mass=3, mass,aero=3, mass,aero,tree=4, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.7 len=0.3 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.010000000000000009:0.26999999999999996 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.1,0.05;0.2,0.05;0.30000000000000004,0.0
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.09999999999999998,0.0
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: without a point OpenRocket dies of an IndexOutOfBoundsException; here the outline stays, with a warning.
    // OpenRocket: RESULT THROWN java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
    {.name = "scout-finpoints-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><finpoints/></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero=1, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a point with a NaN is passed over (OpenRocket stores it).
    // OpenRocket: |       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboar ...
    {.name = "scout-finpoints-nan", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="NaN" y="0.1"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decisions L3 and L4 in AxialPositionSetter (part R2): an offset of NaN is refused; OpenRocket sets the method and dies of a BugException.
    // OpenRocket: RESULT THROWN info.openrocket.core.util.BugException: BUG: setAxialOffset is broken -- attempted to update as NaN:  >> Dumping Detailed Information fr ...
    // OpenRocket: EVENTS 4 {mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=1}
    // OpenRocket: |       Bulkhead 'Bulkhead' axial=TOP:0.198 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false r ...
    {.name = "scout-nan-axialoffset", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><bulkhead><axialoffset method="top">NaN</axialoffset></bulkhead></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Warning: invalid value radius position. value=NaN    class: info.openrocket.core.rocketcomponent.Bulkhead
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3 in AxialPositionSetter (part R2): an infinite offset is refused (OpenRocket stores it).
    // OpenRocket: EVENTS 5 {mass,aero=1, mass,aero,tree=1, mass,tree=1, nonfunc=1, tree=1}
    // OpenRocket: |       Bulkhead 'Bulkhead' axial=TOP:Infinity x=Infinity len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:f ...
    {.name = "scout-inf-axialoffset", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><bulkhead><axialoffset method="top">Infinity</axialoffset></bulkhead></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Warning: invalid value radius position. value=Infinity    class: info.openrocket.core.rocketcomponent.Bulkhead
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, mass,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3 in the radius and angle setters (part R2): offsets that are not finite are refused (OpenRocket stores them).
    // OpenRocket: EVENTS 9 {mass,aero=5, mass,aero,tree=2, tree=2}
    // OpenRocket: |       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:NaN finish=NORM ...
    // OpenRocket: |       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.2 len=0.0 inst=2 radius=FREE:NaN angle=RELATIVE:Infinity
    {.name = "scout-nan-radiusoffset", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><subcomponents><trapezoidfinset><radiusoffset method="relative">NaN</radiusoffset><angleoffset method="relative">NaN</angleoffset></trapezoidfinset><podset><radiusoffset method="free">NaN</radiusoffset><angleoffset>Infinity</angleoffset></podset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Warning: invalid value radius position. value=NaN    class: info.openrocket.core.rocketcomponent.TrapezoidFinSet
W Warning: invalid angle position. value=NaN  (degrees)  class: info.openrocket.core.rocketcomponent.TrapezoidFinSet
W Warning: invalid value radius position. value=NaN    class: info.openrocket.core.rocketcomponent.PodSet
W Warning: invalid angle position. value=Infinity  (degrees)  class: info.openrocket.core.rocketcomponent.PodSet
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=2, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.2 len=0.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an ignition delay of NaN and an infinite overhang are refused (OpenRocket stores them).
    // OpenRocket: EVENTS 4 {mass,aero=1, mass,aero,tree=1, motor=1, tree=1}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "scout-nan-mount", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>NaN</ignitiondelay><overhang>Infinity</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal ignition delay specified, ignoring.
W Illegal overhang specified, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, motor=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=true overhang=0.0 ign=AUTOMATIC:0.0:false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: an infinite deploy delay is refused; the warning is also the one of the NaN, of which OpenRocket gives none.
    // OpenRocket: |       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nyl ...
    {.name = "scout-nan-deploy", .xml = R"xml(<motorconfiguration configid="11111111-2222-3333-4444-555555555555"/>
<subcomponents><stage><subcomponents><bodytube><subcomponents><parachute><deploydelay>NaN</deploydelay><deploymentconfiguration configid="11111111-2222-3333-4444-555555555555"><deploydelay>Infinity</deploydelay><deployaltitude>NaN</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring. data: 'NaN' - Parachute
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, mass,tree=1, tree=2}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 deploy[11111111-2222-3333-4444-555555555555]=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0
| config 11111111-2222-3333-4444-555555555555 name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES rocket_element
// BEGIN GENERATED TABLES legacy_files
constexpr std::array<DesignFileCase, 18> kLegacyFiles{{
    {.file = "simplerocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 80 {mass=21, mass,aero=19, mass,aero,tree=4, mass,tree=8, motor=1, nonfunc=24, tree=3}
COMPONENTS 14
STATE 9c68f2cfd96b7652ab4a226382d10121ebc2e59fa8abb6bb9486c8f8db01be3a)out"},
    {.file = "v1.0-roll-stabilized.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 90 {aero=1, mass=21, mass,aero=25, mass,aero,tree=5, mass,tree=7, motor=1, nonfunc=26, tree=4}
COMPONENTS 14
STATE 6106b63d4a201cc2ebc8acaf504edfef422ece4027f8a4cd6293b60b62196e60)out"},
    {.file = "v1.4-roll-stabilized.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 90 {aero=1, mass=21, mass,aero=25, mass,aero,tree=5, mass,tree=7, motor=1, nonfunc=26, tree=4}
COMPONENTS 14
STATE 3aa2b4a1e490f85532eb574c6a5fae1ffe4433d99b52063182880d1b7a9a09c4)out"},
    {.file = "v1.5-preset-usage.ork", .expected = R"out(RESULT ok
W No matching ComponentPreset for component Nose cone found matching SEMROC Astronautics BNC-55F
W No matching ComponentPreset for component Body tube found matching SEMROC Astronautics BT-55
W No matching ComponentPreset for component Centering ring found matching SEMROC Astronautics RA-5055
W No matching ComponentPreset for component Inner Tube found matching SEMROC Astronautics BT-50J
W No matching ComponentPreset for component Launch lug found matching SEMROC Astronautics LL-117
W No matching ComponentPreset for component Parachute found matching SEMROC Astronautics PN-18
ROOT rocket {} []
EVENTS 68 {mass=27, mass,aero=17, mass,aero,tree=4, mass,tree=4, motor=1, nonfunc=12, tree=3}
COMPONENTS 10
STATE 854c013e3e888b20c90c16357459a72d5ceca1dbf01ae75101018969fc3356a0)out"},
    {.file = "v1.6-a-simple-model-rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 78 {mass=19, mass,aero=18, mass,aero,tree=4, mass,tree=7, motor=1, nonfunc=22, tree=7}
COMPONENTS 13
STATE 2573bc9684cdb8a7575dd91bb6d07ead82ce554105fe6e82a882a8db27ee5e0d)out"},
    {.file = "v1.6-apocd.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 301 {aero=22, aero,treechild=1, mass=99, mass,aero=103, mass,aero,tree=20, mass,tree=9, mass,treechild=2, motor=1, nonfunc=43, tree=1}
COMPONENTS 31
STATE 7eb8676a8af4ce5bef6c95b6ea0fb265aff70f17e365b37382fe33f69ab20e54)out"},
    {.file = "v1.6-boosted-dart.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 201 {aero,treechild=1, mass=45, mass,aero=83, mass,aero,tree=18, mass,tree=5, mass,treechild=2, motor=1, nonfunc=40, tree=6}
COMPONENTS 26
STATE 491763fb1d1763e154d8a3477dc2adce3be9c79e3db5258806eb0b2dbca45dfa)out"},
    {.file = "v1.6-high-power-airstart.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 127 {mass=48, mass,aero=26, mass,aero,tree=5, mass,tree=11, motor=2, nonfunc=28, tree=7}
COMPONENTS 18
STATE c02e294ec4369bd6ec3c042b867281380d89b29466e606d7a84135055fbc8c57)out"},
    {.file = "v1.6-preset-usage-decals-first.ork", .expected = R"out(RESULT ok
W No matching ComponentPreset for component Nose cone found matching SEMROC Astronautics BNC-55F
W No matching ComponentPreset for component Body tube found matching SEMROC Astronautics BT-55
W No matching ComponentPreset for component Centering ring found matching SEMROC Astronautics RA-5055
W No matching ComponentPreset for component Inner Tube found matching SEMROC Astronautics BT-50J
W No matching ComponentPreset for component Launch lug found matching SEMROC Astronautics LL-117
W No matching ComponentPreset for component Parachute found matching SEMROC Astronautics PN-18
ROOT rocket {} []
EVENTS 68 {mass=27, mass,aero=17, mass,aero,tree=4, mass,tree=4, motor=1, nonfunc=12, tree=3}
COMPONENTS 10
STATE 6ab8aba018ec84f09ab9ff163c627b76f6073840afcbfc927c54bb5bd7763056)out"},
    {.file = "v1.6-simulation-listeners.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 194 {aero=3, mass=71, mass,aero=56, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=38, tree=3}
COMPONENTS 24
STATE 796b2932e75ea95b5279970494ba048bb562039436e717eaeabf2cba9a9e945e)out"},
    {.file = "v1.6-tarc-payloader.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 103 {mass=28, mass,aero=33, mass,aero,tree=6, mass,tree=8, motor=1, nonfunc=22, tree=5}
COMPONENTS 17
STATE 8aa963dba25edd93dbff5ad5d1f4f1ccc3ef567f800635a2d2e04af1bd605726)out"},
    {.file = "v1.6-three-stage-rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 143 {mass=23, mass,aero=42, mass,aero,tree=9, mass,tree=15, motor=3, nonfunc=42, tree=9}
COMPONENTS 28
STATE 3ce5dc3a7b414ade470080b077492c7db4ff2bd2a2532ec1bf3a78850c544083)out"},
    {.file = "v1.7-simulation-extensions-and-scripting.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 194 {aero=3, mass=71, mass,aero=56, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=38, tree=3}
COMPONENTS 24
STATE 8c99230c4dce305df83400008c1314a525f575ca6e14c4deef61dc476d92d738)out"},
    {.file = "v1.7-tube-fin.ork", .expected = R"out(RESULT ok
W No matching ComponentPreset for component Body tube found matching FlisKits BT-50-18
ROOT rocket {} []
EVENTS 24 {mass=2, mass,aero=8, mass,aero,tree=3, mass,tree=1, motor=1, nonfunc=6, tree=3}
COMPONENTS 6
STATE 9c4830bc98bd4219014db81b025b867413474ef59f5229bb334bd4086a54a75b)out"},
    {.file = "v1.8-logo-rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 43 {mass=1, mass,aero=28, mass,aero,tree=6, nonfunc=5, tree=3}
COMPONENTS 9
STATE 4ea1ecbdb0e6cd1b1503cff276d31b6fed1e3eeac8fd6326699233fa446bff14)out"},
    {.file = "v1.8-parallel-staging-example.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 101 {mass=28, mass,aero=31, mass,aero,tree=8, mass,tree=4, motor=2, nonfunc=21, tree=7}
COMPONENTS 16
STATE efafd3d49ec1a0ec5302bfc1f6f0833351803a142725dbfbc9198cd1d8467f8b)out"},
    {.file = "v1.8-pods-example.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 112 {mass=32, mass,aero=34, mass,aero,tree=8, mass,tree=5, motor=2, nonfunc=25, tree=6}
COMPONENTS 17
STATE 767340ee47c77cfe17661f9231b4b22aa507eafd375a6c44e9f17d5850ee9b49)out"},
    {.file = "v1.9-chute-release.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 149 {aero=3, mass=44, mass,aero=46, mass,aero,tree=5, mass,tree=8, motor=1, nonfunc=38, tree=4}
COMPONENTS 15
STATE 9b7c85357879f7c635dc2288e7906a5a9e7c12b81f3b5a568612eaf9b562a805)out"},
}};
// END GENERATED TABLES legacy_files
// BEGIN GENERATED TABLES example_files
constexpr std::array<DesignFileCase, 16> kExampleFiles{{
    {.file = "3D printable nose cone and fins.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 150 {aero=6, mass=42, mass,aero=40, mass,aero,tree=5, mass,tree=9, motor=1, nonfunc=40, tree=7}
COMPONENTS 16
STATE 5fb1a9241ea257deffbb6111e25c1d31560c4cede8107a49396ca2b86d564fe3)out"},
    {.file = "A simple model rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 102 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4, mass,tree=7, motor=1, nonfunc=30, tree=7}
COMPONENTS 13
STATE 3d0d76a1adaa16691d3dd3327cfbf7f03f3c4bebaaf6d42e0127157a39d5499f)out"},
    {.file = "ARC payload rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 145 {aero=2, mass=40, mass,aero=50, mass,aero,tree=6, mass,tree=8, motor=1, nonfunc=33, tree=5}
COMPONENTS 17
STATE fd79e66e05e7fc0f39a9f64fe49b9c758b2ce99915a6f20f37a617bc9d97b2f6)out"},
    {.file = "Airstart timing.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 170 {aero=2, mass=59, mass,aero=41, mass,aero,tree=5, mass,tree=11, motor=2, nonfunc=43, tree=7}
COMPONENTS 18
STATE b4dd530eccf0594d3ad6a5e478d2087dc044f64cee3646f2e87548f68a97491b)out"},
    {.file = "Chute release.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 150 {aero=3, mass=44, mass,aero=46, mass,aero,tree=5, mass,tree=8, motor=1, nonfunc=39, tree=4}
COMPONENTS 15
STATE 9b7c85357879f7c635dc2288e7906a5a9e7c12b81f3b5a568612eaf9b562a805)out"},
    {.file = "Clustered motors.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 134 {aero=2, mass=23, mass,aero=43, mass,aero,tree=5, mass,tree=10, motor=1, nonfunc=43, tree=7}
COMPONENTS 17
STATE 3f6cbad94ba24643d11fad249290aee5550c11c4801301d3d9ef90568e600687)out"},
    {.file = "Deployable payload.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 141 {aero=1, mass=23, mass,aero=46, mass,aero,tree=5, mass,tree=11, motor=1, nonfunc=45, tree=9}
COMPONENTS 19
STATE 28f980e102ec48dea1b48fe4e2c8609a54ea10cda8a44d39af1aff214ab74e68)out"},
    {.file = "Dual parachute deployment.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 182 {aero=3, mass=48, mass,aero=59, mass,aero,tree=7, mass,tree=11, mass,treechild=1, motor=1, nonfunc=44, tree=8}
COMPONENTS 20
STATE 6e40ee345f45b5abd97fffd727a4d68663c8bd1bebca6fc456de5df8ef3f8c12)out"},
    {.file = "Parallel booster staging.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 162 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8, mass,tree=7, motor=2, nonfunc=27, tree=5}
COMPONENTS 18
STATE ffdcc7318586a97c920863948d00ef64a97cdb498e664029869114c7546dbed8)out"},
    {.file = "Pods--airframes and winglets.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 235 {aero=1, mass=62, mass,aero=98, mass,aero,tree=13, mass,tree=7, motor=1, nonfunc=44, tree=9}
COMPONENTS 24
STATE 51a1013ca7a70c28d780c8251e2815b2cccba19f892e9d6a0feea0d588972bfd)out"},
    {.file = "Pods--powered with recovery deployment.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 104 {mass=17, mass,aero=46, mass,aero,tree=7, mass,tree=5, motor=2, nonfunc=23, tree=4}
COMPONENTS 15
STATE a795dad9d78ac894b452f7630ead292d4730bbe49e030b42b8ed419ed04067c8)out"},
    {.file = "Simulation extensions.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 242 {aero=6, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=54, tree=3}
COMPONENTS 24
STATE c87763ed91dcd641c34171b0b663aeae060be1f1e7f2b8167809071c5dd4772a)out"},
    {.file = "Simulation scripting.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 240 {aero=5, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=53, tree=3}
COMPONENTS 24
STATE 6f89b02264da5a93862dcaff750d9b26d84da446a9a0243698217099e8810b42)out"},
    {.file = "Three stage low power rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 190 {aero=2, mass=23, mass,aero=71, mass,aero,tree=9, mass,tree=15, motor=3, nonfunc=58, tree=9}
COMPONENTS 28
STATE aa1fba1092e554f0f4f2b66e1b508856a957577fa7c8fc443c379390b2c70fd5)out"},
    {.file = "Tube fin rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 75 {aero=1, mass=23, mass,aero=25, mass,aero,tree=4, mass,tree=3, motor=1, nonfunc=15, tree=3}
COMPONENTS 9
STATE 3ae2e9470e917e3151ed54ab2c3fbac92cc7b47198f62b29a6e875449ac0972c)out"},
    {.file = "Two stage high power rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 433 {aero=2, mass=159, mass,aero=124, mass,aero,tree=13, mass,tree=34, motor=2, nonfunc=93, tree=6}
COMPONENTS 50
STATE 1c1140eb42ccab8b91451f870b77455e18b8e5666a626c2872c8da29f90ce528)out"},
}};
// END GENERATED TABLES example_files
// clang-format on

TEST(RocketElement, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(RocketElement, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(RocketElement, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(RocketElement, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// The line "EVENTS ..." of what reading the rocket element of the example @p file gives.
[[nodiscard]] std::string eventsOfExample(std::string_view file)
{
    RocketLoadFixture fixture(true);
    const std::string told = fixture.loadAndSummarize(
        rocketElementOfDesignFile(QtRocket::Test::dataDir() / "examples" / file));
    const std::size_t begin = told.find("EVENTS ");
    return told.substr(begin, told.find('\n', begin) - begin);
}

// The rocket's events are on while a file loads (decision D3), and every setter fires its
// event as in OpenRocket. The scout counted the events of OpenRocket's whole load of two
// examples (tier9-scout-loader-components/out/events.out):
//   A simple model rocket.ork: 107 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4,
//     mass,tree=7, motor=1, nonfunc=35, tree=7}
//   Parallel booster staging.ork: 184 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8,
//     mass,tree=7, motor=2, nonfunc=49, tree=5}
// The rocket elements give exactly these but for one NONFUNCTIONAL_CHANGE per appearance
// element, which are taken out here (HOOK(R4)): the first design has 5 <appearance>, the second
// 15 and 7 <insideappearance>. With part R4's handlers the counts are the scout's.
TEST(RocketElement, FiresTheEventsOfOpenRocketsLoad)
{
    EXPECT_EQ(eventsOfExample("A simple model rocket.ork"),
              "EVENTS 102 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4, mass,tree=7, motor=1, "
              "nonfunc=30, tree=7}");
    EXPECT_EQ(eventsOfExample("Parallel booster staging.ork"),
              "EVENTS 162 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8, mass,tree=7, motor=2, "
              "nonfunc=27, tree=5}");
}

// The rocket elements of the 18 designs of tests/data/ork, which OpenRocket 0.9.3 to 23.09 wrote
// in the formats 1.0 to 1.9, read without their appearance elements (HOOK(R4)) and without a
// preset database: the warnings, the rocket's change events by kind, the number of components
// and the digest of the state are OpenRocket's.
TEST(RocketElement, ReadsTheLegacyDesignsAsOpenRocket)
{
    EXPECT_EQ(failedDesignFiles(QtRocket::Test::testDataDir() / "ork", kLegacyFiles, false),
              Texts{});
}

// The same for the 16 example designs of data/examples (formats 1.10 and 1.11), with the six
// presets they refer to: no warning, as in OpenRocket with its preset database.
TEST(RocketElement, ReadsTheExampleDesignsAsOpenRocket)
{
    EXPECT_EQ(failedDesignFiles(QtRocket::Test::dataDir() / "examples", kExampleFiles, true),
              Texts{});
}

// Not tests: they print what QtRocket makes of the design files, for scripts/make_tables.py,
// and the states behind the digests, to compare with the probe's dump files.
TEST(RocketElement, DISABLED_PrintsTheLegacyDesigns)
{
    std::cout << printedDesignFiles(QtRocket::Test::testDataDir() / "ork", kLegacyFiles, false,
                                    false);
}

TEST(RocketElement, DISABLED_PrintsTheExampleDesigns)
{
    std::cout << printedDesignFiles(QtRocket::Test::dataDir() / "examples", kExampleFiles, true,
                                    false);
}

TEST(RocketElement, DISABLED_PrintsTheStatesOfTheLegacyDesigns)
{
    std::cout << printedDesignFiles(QtRocket::Test::testDataDir() / "ork", kLegacyFiles, false,
                                    true);
}

TEST(RocketElement, DISABLED_PrintsTheStatesOfTheExampleDesigns)
{
    std::cout << printedDesignFiles(QtRocket::Test::dataDir() / "examples", kExampleFiles, true,
                                    true);
}

}  // namespace
