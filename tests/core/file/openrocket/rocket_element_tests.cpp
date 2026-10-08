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
constexpr std::array<RocketCase, 56> kJava{{
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
    {.name = "scout-appearance-basic", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20" blue="30" alpha="40"/><shine>0.5</shine><opacityaffectstexture>TRUE</opacityaffectstexture><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance><insideappearance><edgessameasinside>true</edgessameasinside><insidesameasoutside>true</insidesameasoutside><paint red="1" green="2" blue="3"/><shine>0.1</shine></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=1, nonfunc=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=10,20,30,40 shine=0.5 opacity=true decal='decals/a.png':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0] inside=[paint=1,2,3,255 shine=0.1 opacity=false] insideflags=true,true
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-bad-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="x" edgemode="STICKER"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-bad-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="sticker"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: No enum constant info.openrocket.core.appearance.Decal.EdgeMode.sticker
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-bad-shine", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>abc</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "abc"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-bad-paint", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20"/><shine> 0.7 </shine><opacityaffectstexture> true </opacityaffectstexture><bogus x="1">text</bogus><center x="1" y="2"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
W Unknown attributes in element 'center', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.7 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-decal-extras2", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="REPEAT" extra="1">text<bogus/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'decal', ignoring.
W Unknown attributes in element 'decal', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-appearance-two-decals", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="REPEAT"/><decal name="decals/b.png" rotation="1" edgemode="CLAMP"/></appearance><appearance><shine>0.9</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.9 opacity=false]
| decals='decals/a.png','decals/b.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "scout-insideappearance-on-stage", .xml = R"xml(<subcomponents><stage><insideappearance><edgessameasinside>true</edgessameasinside><paint red="1" green="2" blue="3"/></insideappearance><appearance><shine>0.9</shine></appearance><subcomponents><bodytube><inside-appearance><edgesSameAsInside>true</edgesSameAsInside><insideSameAsOutside>false</insideSameAsOutside><shine>0.2</shine></inside-appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0 app=[paint=187,187,187,255 shine=0.9 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.2 opacity=false] insideflags=true,false
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
constexpr std::array<RocketCase, 15> kOwn{{
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
    // Decision L4: a decal without rotation, of which OpenRocket dies (the image registered by then); here the decal is ignored with a warning and registers nothing.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/a.png'
    {.name = "scout-appearance-no-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" edgemode="STICKER"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a decal without edgemode, of which OpenRocket dies (the image registered by then); here the decal is ignored with a warning and registers nothing.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Name is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/a.png'
    {.name = "scout-appearance-no-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a decal without name, of which OpenRocket dies; here the decal is ignored with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "scout-appearance-no-name", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal rotation="0" edgemode="REPEAT"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a center without y, of which OpenRocket dies; here it is passed over with a warning, and the decal's text and attribute are warned of.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "scout-appearance-decal-extras", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="REPEAT" extra="1">text<center x="1"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
W Unknown text in element 'decal', ignoring.
W Unknown attributes in element 'decal', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
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
EVENTS 83 {mass=19, mass,aero=18, mass,aero,tree=4, mass,tree=7, motor=1, nonfunc=27, tree=7}
COMPONENTS 13
STATE 77763b6a0a2ab2d9213d5d8fb4f16458222baaf5c11547dce643ad382b68a5f1)out"},
    {.file = "v1.6-apocd.ork", .expected = R"out(RESULT ok
W Unknown attributes in element 'ambient', ignoring.
W Unknown attributes in element 'diffuse', ignoring.
W Unknown attributes in element 'specular', ignoring.
ROOT rocket {} []
EVENTS 321 {aero=22, aero,treechild=1, mass=99, mass,aero=103, mass,aero,tree=20, mass,tree=9, mass,treechild=2, motor=1, nonfunc=63, tree=1}
COMPONENTS 31
STATE b1e453db2e2ba8313597be32b21054edf3992870bc232c564772ea72198345cc)out"},
    {.file = "v1.6-boosted-dart.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 218 {aero,treechild=1, mass=45, mass,aero=83, mass,aero,tree=18, mass,tree=5, mass,treechild=2, motor=1, nonfunc=57, tree=6}
COMPONENTS 26
STATE 056b18cfbb1b1f41b1a870ee93d7d5e5c04b52c51ea6523242aa447936061b7f)out"},
    {.file = "v1.6-high-power-airstart.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 135 {mass=48, mass,aero=26, mass,aero,tree=5, mass,tree=11, motor=2, nonfunc=36, tree=7}
COMPONENTS 18
STATE 96dd1574020de05c59e272c9125f8848239dd8fae37205c2e7acaa7585ea4d17)out"},
    {.file = "v1.6-preset-usage-decals-first.ork", .expected = R"out(RESULT ok
W No matching ComponentPreset for component Nose cone found matching SEMROC Astronautics BNC-55F
W No matching ComponentPreset for component Body tube found matching SEMROC Astronautics BT-55
W No matching ComponentPreset for component Centering ring found matching SEMROC Astronautics RA-5055
W No matching ComponentPreset for component Inner Tube found matching SEMROC Astronautics BT-50J
W No matching ComponentPreset for component Launch lug found matching SEMROC Astronautics LL-117
W No matching ComponentPreset for component Parachute found matching SEMROC Astronautics PN-18
ROOT rocket {} []
EVENTS 73 {mass=27, mass,aero=17, mass,aero,tree=4, mass,tree=4, motor=1, nonfunc=17, tree=3}
COMPONENTS 10
STATE c302efe611b2a3afebea23dffa4d4e2b56db56839908acea4e59d33c1c3c7d86)out"},
    {.file = "v1.6-simulation-listeners.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 203 {aero=3, mass=71, mass,aero=56, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=47, tree=3}
COMPONENTS 24
STATE 1a6f74d83b63a096a0b421389a0fe56e0d16a754a2b759e71ce88ec735be7bd9)out"},
    {.file = "v1.6-tarc-payloader.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 111 {mass=28, mass,aero=33, mass,aero,tree=6, mass,tree=8, motor=1, nonfunc=30, tree=5}
COMPONENTS 17
STATE bc284d6a991574fc3e4cc0d6db95d81d8f7c9793533c6d92b49de7cba52dae0e)out"},
    {.file = "v1.6-three-stage-rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 153 {mass=23, mass,aero=42, mass,aero,tree=9, mass,tree=15, motor=3, nonfunc=52, tree=9}
COMPONENTS 28
STATE 9fce0e4d65b082dc4776e1dc83f927a4bf058aaa7ce9a1e654ee4a683bb6eb72)out"},
    {.file = "v1.7-simulation-extensions-and-scripting.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 203 {aero=3, mass=71, mass,aero=56, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=47, tree=3}
COMPONENTS 24
STATE 7c6ec0df632a1778988289966a006ddb4a688dda0dbf6a56f7121302b7516bc2)out"},
    {.file = "v1.7-tube-fin.ork", .expected = R"out(RESULT ok
W No matching ComponentPreset for component Body tube found matching FlisKits BT-50-18
ROOT rocket {} []
EVENTS 24 {mass=2, mass,aero=8, mass,aero,tree=3, mass,tree=1, motor=1, nonfunc=6, tree=3}
COMPONENTS 6
STATE 9c4830bc98bd4219014db81b025b867413474ef59f5229bb334bd4086a54a75b)out"},
    {.file = "v1.8-logo-rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 49 {mass=1, mass,aero=28, mass,aero,tree=6, nonfunc=11, tree=3}
COMPONENTS 9
STATE c61b5e0a2ff9bb0d2df58098ed3d77e71e7fa67202e5501a35c2f516254f65b2)out"},
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
EVENTS 155 {aero=3, mass=44, mass,aero=46, mass,aero,tree=5, mass,tree=8, motor=1, nonfunc=44, tree=4}
COMPONENTS 15
STATE 95120346d8f1c120ba5fd76b2102a2e1396b705a4fa59331520d92afe38155b3)out"},
}};
// END GENERATED TABLES legacy_files
// BEGIN GENERATED TABLES example_files
constexpr std::array<DesignFileCase, 16> kExampleFiles{{
    {.file = "3D printable nose cone and fins.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 161 {aero=6, mass=42, mass,aero=40, mass,aero,tree=5, mass,tree=9, motor=1, nonfunc=51, tree=7}
COMPONENTS 16
STATE 82b3bdf8b73a0a5350928524806203f86d4aab55baff78fcb93b647f80e7db92)out"},
    {.file = "A simple model rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 107 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4, mass,tree=7, motor=1, nonfunc=35, tree=7}
COMPONENTS 13
STATE b0a39754488aad4ed30ed2fed2b85e63d84ac84c3f2c20d84a0e36b2f0ce0c1b)out"},
    {.file = "ARC payload rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 153 {aero=2, mass=40, mass,aero=50, mass,aero,tree=6, mass,tree=8, motor=1, nonfunc=41, tree=5}
COMPONENTS 17
STATE c14300573110a705a6ac764f329f51740ca8cb34f572ce40cd880d0afb523536)out"},
    {.file = "Airstart timing.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 178 {aero=2, mass=59, mass,aero=41, mass,aero,tree=5, mass,tree=11, motor=2, nonfunc=51, tree=7}
COMPONENTS 18
STATE 8d115736d939d4bec9918d2ee3f5b23df64e302dde8c7808b4407fa14e71e43c)out"},
    {.file = "Chute release.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 156 {aero=3, mass=44, mass,aero=46, mass,aero,tree=5, mass,tree=8, motor=1, nonfunc=45, tree=4}
COMPONENTS 15
STATE 95120346d8f1c120ba5fd76b2102a2e1396b705a4fa59331520d92afe38155b3)out"},
    {.file = "Clustered motors.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 141 {aero=2, mass=23, mass,aero=43, mass,aero,tree=5, mass,tree=10, motor=1, nonfunc=50, tree=7}
COMPONENTS 17
STATE a26759fd7ff9f165380cbe91f9bd27ec02dcc52f898527eca5dc36b4dd20d3f4)out"},
    {.file = "Deployable payload.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 147 {aero=1, mass=23, mass,aero=46, mass,aero,tree=5, mass,tree=11, motor=1, nonfunc=51, tree=9}
COMPONENTS 19
STATE d43774b51cd0e298d61559d92148af3a961c6c3571dc388b67264723846183b5)out"},
    {.file = "Dual parachute deployment.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 182 {aero=3, mass=48, mass,aero=59, mass,aero,tree=7, mass,tree=11, mass,treechild=1, motor=1, nonfunc=44, tree=8}
COMPONENTS 20
STATE 6e40ee345f45b5abd97fffd727a4d68663c8bd1bebca6fc456de5df8ef3f8c12)out"},
    {.file = "Parallel booster staging.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 184 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8, mass,tree=7, motor=2, nonfunc=49, tree=5}
COMPONENTS 18
STATE 4d5e2cfb47fe69284ea3c55f7b51e71581290141fbc3831b2dd46b51beb5dcda)out"},
    {.file = "Pods--airframes and winglets.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 247 {aero=1, mass=62, mass,aero=98, mass,aero,tree=13, mass,tree=7, motor=1, nonfunc=56, tree=9}
COMPONENTS 24
STATE 87d27ce2507175b77334448bd3674b72031877b4496fcd896e49a3e95cb2fdb2)out"},
    {.file = "Pods--powered with recovery deployment.ork", .expected = R"out(RESULT ok
W Embedded motor attachment 'thrustcurves/e5b53def203dd437ebf0d67846f6cd3b.rse' contains no motor matching digest 'e5b53def203dd437ebf0d67846f6cd3b'.
ROOT rocket {} []
EVENTS 111 {mass=17, mass,aero=46, mass,aero,tree=7, mass,tree=5, motor=2, nonfunc=30, tree=4}
COMPONENTS 15
STATE 7752c1472ef360b96f5031300822a5e936d9feb9f029a771fb19efe7daa4652f)out"},
    {.file = "Simulation extensions.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 251 {aero=6, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=63, tree=3}
COMPONENTS 24
STATE 2388e0fb0a0c50eb34ef5322e92ace0584b3bfef7459f1d060f90fead65381c8)out"},
    {.file = "Simulation scripting.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 249 {aero=5, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=62, tree=3}
COMPONENTS 24
STATE dc31be59e5ffd036a2e9474543fbc7ec9812cc8768a3529b453e2c6a202d578b)out"},
    {.file = "Three stage low power rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 200 {aero=2, mass=23, mass,aero=71, mass,aero,tree=9, mass,tree=15, motor=3, nonfunc=68, tree=9}
COMPONENTS 28
STATE 8cd3405be368311d076e4fe4eda1d68fe5fb0d8c5b2ef5165522c37d523bbcc2)out"},
    {.file = "Tube fin rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 81 {aero=1, mass=23, mass,aero=25, mass,aero,tree=4, mass,tree=3, motor=1, nonfunc=21, tree=3}
COMPONENTS 9
STATE cf4b3e1636823d88cb05ecc496718960a5a35cdf13df29ed84961ef407e68bf4)out"},
    {.file = "Two stage high power rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 444 {aero=2, mass=159, mass,aero=124, mass,aero,tree=13, mass,tree=34, motor=2, nonfunc=104, tree=6}
COMPONENTS 50
STATE 4406d223493f92882918335dcbd49fb6c21988cea75e2b7689c7caa88903098f)out"},
}};
// END GENERATED TABLES example_files
// BEGIN GENERATED TABLES resave_files
constexpr std::array<DesignFileCase, 29> kResaveFiles{{
    {.file = "example-3d-printable-nose-cone-and-fins/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 161 {aero=6, mass=42, mass,aero=40, mass,aero,tree=5, mass,tree=9, motor=1, nonfunc=51, tree=7}
COMPONENTS 16
STATE 82b3bdf8b73a0a5350928524806203f86d4aab55baff78fcb93b647f80e7db92)out"},
    {.file = "example-a-simple-model-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 107 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4, mass,tree=7, motor=1, nonfunc=35, tree=7}
COMPONENTS 13
STATE b0a39754488aad4ed30ed2fed2b85e63d84ac84c3f2c20d84a0e36b2f0ce0c1b)out"},
    {.file = "example-airstart-timing/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 178 {aero=2, mass=59, mass,aero=41, mass,aero,tree=5, mass,tree=11, motor=2, nonfunc=51, tree=7}
COMPONENTS 18
STATE 8d115736d939d4bec9918d2ee3f5b23df64e302dde8c7808b4407fa14e71e43c)out"},
    {.file = "example-arc-payload-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 153 {aero=2, mass=40, mass,aero=50, mass,aero,tree=6, mass,tree=8, motor=1, nonfunc=41, tree=5}
COMPONENTS 17
STATE c14300573110a705a6ac764f329f51740ca8cb34f572ce40cd880d0afb523536)out"},
    {.file = "example-chute-release/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 156 {aero=3, mass=44, mass,aero=46, mass,aero,tree=5, mass,tree=8, motor=1, nonfunc=45, tree=4}
COMPONENTS 15
STATE 95120346d8f1c120ba5fd76b2102a2e1396b705a4fa59331520d92afe38155b3)out"},
    {.file = "example-clustered-motors/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 141 {aero=2, mass=23, mass,aero=43, mass,aero,tree=5, mass,tree=10, motor=1, nonfunc=50, tree=7}
COMPONENTS 17
STATE a26759fd7ff9f165380cbe91f9bd27ec02dcc52f898527eca5dc36b4dd20d3f4)out"},
    {.file = "example-deployable-payload/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 147 {aero=1, mass=23, mass,aero=46, mass,aero,tree=5, mass,tree=11, motor=1, nonfunc=51, tree=9}
COMPONENTS 19
STATE d43774b51cd0e298d61559d92148af3a961c6c3571dc388b67264723846183b5)out"},
    {.file = "example-dual-parachute-deployment/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 182 {aero=3, mass=48, mass,aero=59, mass,aero,tree=7, mass,tree=11, mass,treechild=1, motor=1, nonfunc=44, tree=8}
COMPONENTS 20
STATE 6e40ee345f45b5abd97fffd727a4d68663c8bd1bebca6fc456de5df8ef3f8c12)out"},
    {.file = "example-parallel-booster-staging/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 184 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8, mass,tree=7, motor=2, nonfunc=49, tree=5}
COMPONENTS 18
STATE c86943bd165f0d44961991cbbc99ace81e1fb040115255103272df4769b6a6c2)out"},
    {.file = "example-pods-airframes-and-winglets/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 247 {aero=1, mass=62, mass,aero=98, mass,aero,tree=13, mass,tree=7, motor=1, nonfunc=56, tree=9}
COMPONENTS 24
STATE 87d27ce2507175b77334448bd3674b72031877b4496fcd896e49a3e95cb2fdb2)out"},
    {.file = "example-pods-powered-with-recovery-deployment/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 111 {mass=17, mass,aero=46, mass,aero,tree=7, mass,tree=5, motor=2, nonfunc=30, tree=4}
COMPONENTS 15
STATE 7f950eaf661a84fe89ca58f2c7942c3969dd9735c4bc0bf173877c9bf55914f5)out"},
    {.file = "example-simulation-extensions/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 251 {aero=6, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=63, tree=3}
COMPONENTS 24
STATE 2388e0fb0a0c50eb34ef5322e92ace0584b3bfef7459f1d060f90fead65381c8)out"},
    {.file = "example-simulation-scripting/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 249 {aero=5, mass=75, mass,aero=81, mass,aero,tree=8, mass,tree=14, motor=1, nonfunc=62, tree=3}
COMPONENTS 24
STATE dc31be59e5ffd036a2e9474543fbc7ec9812cc8768a3529b453e2c6a202d578b)out"},
    {.file = "example-three-stage-low-power-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 200 {aero=2, mass=23, mass,aero=71, mass,aero,tree=9, mass,tree=15, motor=3, nonfunc=68, tree=9}
COMPONENTS 28
STATE 8cd3405be368311d076e4fe4eda1d68fe5fb0d8c5b2ef5165522c37d523bbcc2)out"},
    {.file = "example-tube-fin-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 81 {aero=1, mass=23, mass,aero=25, mass,aero,tree=4, mass,tree=3, motor=1, nonfunc=21, tree=3}
COMPONENTS 9
STATE cf4b3e1636823d88cb05ecc496718960a5a35cdf13df29ed84961ef407e68bf4)out"},
    {.file = "example-two-stage-high-power-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 444 {aero=2, mass=159, mass,aero=124, mass,aero,tree=13, mass,tree=34, motor=2, nonfunc=104, tree=6}
COMPONENTS 50
STATE 7947f7bec6254cfed98a839c8a1a5d80f43ae62c2e0510b4c1c73cd558f0871a)out"},
    {.file = "testrocket-beta/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 110 {mass=15, mass,aero=44, mass,aero,tree=8, mass,tree=6, motor=2, nonfunc=26, tree=9}
COMPONENTS 17
STATE 14f0ab6378aac423ee6e538764c0d0b2cda194837b5883aa300960d58c136c97)out"},
    {.file = "testrocket-big-blue/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 26 {mass=5, mass,aero=12, mass,aero,tree=3, mass,tree=1, nonfunc=3, tree=2}
COMPONENTS 6
STATE e353538fd25357f9dcf5e344ab66147ead9aca0f368d163cd4b16fb3dc629647)out"},
    {.file = "testrocket-cluster-pods/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 37 {mass=2, mass,aero=13, mass,aero,tree=2, mass,tree=2, motor=2, nonfunc=12, tree=4}
COMPONENTS 7
STATE 149aece86538e6a704074ed773752a9ef9f67c9e90bc7a884b994fcd96c2faea)out"},
    {.file = "testrocket-end-plate-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 43 {mass=1, mass,aero=24, mass,aero,tree=5, nonfunc=10, tree=3}
COMPONENTS 8
STATE da185645c68ed7849afabb7b3f33ddcd776177ffbb76024da953d3bae1e73a09)out"},
    {.file = "testrocket-estes-alpha-iii-with-inline-pod/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 87 {mass=14, mass,aero=34, mass,aero,tree=6, mass,tree=4, motor=1, nonfunc=21, tree=7}
COMPONENTS 13
STATE 159fa34d834be17496add9305adc767a145781ddd26b37d9812cc352875bdf10)out"},
    {.file = "testrocket-estes-alpha-iii-with-motor-pods/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 90 {mass=16, mass,aero=32, mass,aero,tree=5, mass,tree=5, motor=2, nonfunc=23, tree=7}
COMPONENTS 13
STATE 1b1daa6b902de0ec36a9bacea778eb31a753981fe9517e3c574831a7cd174a12)out"},
    {.file = "testrocket-estes-alpha-iii-with-pods/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 79 {mass=12, mass,aero=31, mass,aero,tree=5, mass,tree=4, motor=1, nonfunc=19, tree=7}
COMPONENTS 12
STATE 9b71cc5456ef745a2261dbb6c2d2eb8ec00ad23705b62bf2ab5cee22c2363351)out"},
    {.file = "testrocket-estes-alpha-iii-with-second-motor/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 75 {mass=15, mass,aero=26, mass,aero,tree=4, mass,tree=5, motor=2, nonfunc=17, tree=6}
COMPONENTS 11
STATE 21cc7ad18ee5117df61b07acf3e041f5719fbe249e83a6b29b6a04bf8d34a3f0)out"},
    {.file = "testrocket-estes-alpha-iii/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 65 {mass=12, mass,aero=24, mass,aero,tree=4, mass,tree=4, motor=1, nonfunc=14, tree=6}
COMPONENTS 10
STATE b0bcdccc25bd9052fa74794e94409a46f4981d40fcafe00e31a89c49bdcf46f9)out"},
    {.file = "testrocket-falcon-9-heavy/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 109 {mass=22, mass,aero=43, mass,aero,tree=9, mass,tree=3, motor=2, nonfunc=23, tree=7}
COMPONENTS 16
STATE a2bc12bfa5f00ced82ef6fbaa3480d3dd9d76788f265dcce33f82b4daf7e0ac8)out"},
    {.file = "testrocket-iso-haisu/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 137 {mass=38, mass,aero=52, mass,aero,tree=7, mass,tree=12, nonfunc=26, tree=2}
COMPONENTS 21
STATE 435adb82db99cb95dcee74a6d59ff26588c35d26716ad73ae4796413ecfd3dda)out"},
    {.file = "testrocket-multi-stage-event-test-rocket/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 61 {mass,aero=25, mass,aero,tree=6, mass,tree=2, motor=3, nonfunc=18, tree=7}
COMPONENTS 12
STATE 54b89827c2674fff50c17e5eb5fb49f0c1bebe790617e48a624d843636eb4842)out"},
    {.file = "testrocket-simple-2-stage/resave/rocket.ork", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 18 {mass=2, mass,aero=4, mass,aero,tree=2, nonfunc=5, tree=5}
COMPONENTS 5
STATE 2109e4c36013f35af67c9f322a5136cee3bbad13cbd314732e487b614d6e55b5)out"},
}};
// END GENERATED TABLES resave_files
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
// The rocket elements give exactly these: every event of OpenRocket's load of a design comes
// from its rocket element. (One NONFUNCTIONAL_CHANGE is of each appearance element: the first
// design has 5 <appearance>, the second 15 and 7 <insideappearance>.)
TEST(RocketElement, FiresTheEventsOfOpenRocketsLoad)
{
    EXPECT_EQ(eventsOfExample("A simple model rocket.ork"),
              "EVENTS 107 {aero=1, mass=19, mass,aero=33, mass,aero,tree=4, mass,tree=7, motor=1, "
              "nonfunc=35, tree=7}");
    EXPECT_EQ(eventsOfExample("Parallel booster staging.ork"),
              "EVENTS 184 {aero=2, mass=45, mass,aero=66, mass,aero,tree=8, mass,tree=7, motor=2, "
              "nonfunc=49, tree=5}");
}

// The rocket elements of the 18 designs of tests/data/ork, which OpenRocket 0.9.3 to 23.09 wrote
// in the formats 1.0 to 1.9, read without a preset database and with the attachments of each
// file (the entries of an archive, the files beside any other design): the warnings, the
// rocket's change events by kind, the number of components and the digest of the state, the
// appearances and the names of the decal images included, are OpenRocket's.
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

// The same for the 29 designs of the golden data as OpenRocket saved them again
// (tests/data/goldens/<design>/resave/rocket.ork, format 1.11): the 16 examples, now files by
// themselves whose images the decal registry names anew, and the 13 test rockets, which no
// other design file of the tests holds.
TEST(RocketElement, ReadsTheResavedDesignsAsOpenRocket)
{
    EXPECT_EQ(failedDesignFiles(QtRocket::Test::testDataDir() / "goldens", kResaveFiles, true),
              Texts{});
}

// Not tests: they print what QtRocket makes of the design files, for scripts/make_tables.py,
// and the states behind the digests, to compare with the probe's dump files.
TEST(RocketElement, DISABLED_PrintsTheResavedDesigns)
{
    std::cout << printedDesignFiles(QtRocket::Test::testDataDir() / "goldens", kResaveFiles, true,
                                    false);
}

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
