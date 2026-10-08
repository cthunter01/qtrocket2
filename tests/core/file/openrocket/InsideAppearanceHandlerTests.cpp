#include "QtRocket/file/openrocket/InsideAppearanceHandler.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/AppearanceHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "document/DocumentTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// InsideAppearanceHandler: the handler of the <insideappearance> element of a component. What a
// file's text gives is compared with what OpenRocket makes of the same text: the cases are the
// content of a rocket element, read by the component handlers above it.

namespace
{

using QtRocket::Appearance;
using QtRocket::AppearanceHandler;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Color;
using QtRocket::ErrorCode;
using QtRocket::InsideAppearanceHandler;
using QtRocket::InsideColorComponentHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::RocketEventRecorder;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES InsideAppearanceHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 17> kJava{{
    {.name = "ia-basic", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><edgessameasinside>true</edgessameasinside><insidesameasoutside>true</insidesameasoutside><paint red="1" green="2" blue="3"/><shine>0.1</shine></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=1,2,3,255 shine=0.1 opacity=false] insideflags=true,true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-old-names", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><inside-appearance><edgesSameAsInside>true</edgesSameAsInside><insideSameAsOutside>true</insideSameAsOutside><shine>0.2</shine></inside-appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.2 opacity=false] insideflags=true,true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-names-that-are-none", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><EdgesSameAsInside>true</EdgesSameAsInside><insidesameasOutside>true</insidesameasOutside><edges-same-as-inside a="1">true</edges-same-as-inside></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'EdgesSameAsInside', ignoring.
W Unknown text in element 'insidesameasOutside', ignoring.
W Unknown text in element 'edges-same-as-inside', ignoring.
W Unknown attributes in element 'edges-same-as-inside', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-flag-texts", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><edgessameasinside>TRUE</edgessameasinside><insidesameasoutside>tRue</insidesameasoutside></insideappearance></nosecone><bodytube><insideappearance><edgessameasinside> true </edgessameasinside><insidesameasoutside>true
</insidesameasoutside></insideappearance></bodytube><bodytube><insideappearance><edgessameasinside>yes</edgessameasinside><insidesameasoutside>1</insidesameasoutside></insideappearance></bodytube><bodytube><insideappearance><edgessameasinside></edgessameasinside><insidesameasoutside/></insideappearance></bodytube><bodytube><insideappearance><edgessameasinside>true</edgessameasinside></insideappearance></bodytube><bodytube><insideappearance><insidesameasoutside>true</insidesameasoutside></insideappearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 17 {mass,aero,tree=6, nonfunc=10, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.15 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=true,true
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=true,false
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.95 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=false,true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-flags-set-and-taken-back", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><edgessameasinside>true</edgessameasinside><edgessameasinside>true</edgessameasinside><edgessameasinside>false</edgessameasinside><insidesameasoutside>true</insidesameasoutside><insidesameasoutside>false</insidesameasoutside><insidesameasoutside>true</insidesameasoutside></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 8 {mass,aero,tree=1, nonfunc=6, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=false,true
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-flags-with-attributes-and-elements", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><edgessameasinside a="1">true</edgessameasinside><insidesameasoutside>true<x/></insidesameasoutside></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=true,false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><opacityaffectstexture>true</opacityaffectstexture></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=true decal='decals/a.png':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-archive-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><decal name="/datafiles/textures/inside.jpg" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false decal='/datafiles/textures/inside.jpg':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='/datafiles/textures/inside.jpg'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-every-component-with-an-inside", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><shine>0.1</shine></insideappearance></nosecone><bodytube><insideappearance><shine>0.2</shine></insideappearance><subcomponents><trapezoidfinset><insideappearance><shine>0.3</shine></insideappearance></trapezoidfinset><ellipticalfinset><insideappearance><shine>0.35</shine></insideappearance></ellipticalfinset><freeformfinset><insideappearance><shine>0.36</shine></insideappearance></freeformfinset><tubefinset><insideappearance><shine>0.4</shine></insideappearance></tubefinset><launchlug><insideappearance><shine>0.5</shine></insideappearance></launchlug><innertube><insideappearance><shine>0.6</shine></insideappearance></innertube></subcomponents></bodytube><transition><insideappearance><shine>0.7</shine></insideappearance></transition></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 20 {mass,aero=1, mass,aero,tree=8, mass,tree=1, nonfunc=9, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.42500000000000004 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.1 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.2 opacity=false]
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03 inside=[paint=187,187,187,255 shine=0.3 opacity=false]
|       EllipticalFinSet 'Elliptical Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] height=0.05 inside=[paint=187,187,187,255 shine=0.35 opacity=false]
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.04999999999999999,0.0 inside=[paint=187,187,187,255 shine=0.36 opacity=false]
|       TubeFinSet 'Tube Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=6 radius=COAXIAL:0.0 angle=FIXED:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=6 autoradius=true thick=0.002 inside=[paint=187,187,187,255 shine=0.4 opacity=false]
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06 inside=[paint=187,187,187,255 shine=0.5 opacity=false]
|       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.13 len=0.07 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=false overhang=0.0 ign=AUTOMATIC:0.0:false inside=[paint=187,187,187,255 shine=0.6 opacity=false]
|     Transition 'Transition' axial=AFTER:0.0 x=0.35000000000000003 len=0.07500000000000001 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.025:true aft=0.025:true thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false inside=[paint=187,187,187,255 shine=0.7 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-components-without-an-inside", .xml = R"xml(<subcomponents><stage><insideappearance><edgessameasinside>true</edgessameasinside><shine>0.8</shine></insideappearance><subcomponents><bodytube><subcomponents><parachute><insideappearance><edgessameasinside>true</edgessameasinside><insidesameasoutside>true</insidesameasoutside><shine>0.4</shine><decal name="decals/chute.png" rotation="1.5" edgemode="STICKER"></decal></insideappearance></parachute><masscomponent><insideappearance><shine>0.5</shine></insideappearance></masscomponent><railbutton><insideappearance><shine>0.6</shine></insideappearance></railbutton><centeringring><insideappearance><shine>0.7</shine></insideappearance></centeringring><tubecoupler><insideappearance><shine>0.75</shine></insideappearance></tubecoupler><bulkhead><insideappearance><shine>0.76</shine></insideappearance></bulkhead><engineblock><insideappearance><shine>0.77</shine></insideappearance></engineblock><shockcord><insideappearance><shine>0.78</shine></insideappearance></shockcord><streamer><insideappearance><shine>0.79</shine></insideappearance></streamer><podset><insideappearance><shine>0.791</shine></insideappearance></podset><parallelstage><insideappearance><shine>0.792</shine></insideappearance></parallelstage></subcomponents></bodytube></subcomponents></stage></subcomponents><insideappearance><shine>0.9</shine><bogus a="1">t</bogus></insideappearance>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 13 {mass,aero,tree=2, mass,tree=8, tree=3}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT
|       RailButton 'Rail Button' axial=MIDDLE:0.0 x=0.1 len=0.0 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Delrin|1420.0|9.46E8|Plastics] diameter=0.0097:0.008 height=0.0097:0.002:0.002:0.0 spacing=0.0582
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:true radial=0.0:0.0 spacing=0.0
|       TubeCoupler 'Tube Coupler' axial=BOTTOM:0.0 x=0.14 len=0.06 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       Bulkhead 'Bulkhead' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:false radial=0.0:0.0 spacing=0.0
|       EngineBlock 'Engine Block' axial=BOTTOM:0.0 x=0.195 len=0.005 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.023:false radial=0.0:0.0
|       ShockCord 'Shock Cord' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cord=0.6000000000000001:true mat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics]
|       Streamer 'Streamer' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.08937142857142857:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 strip=0.5:0.05
|       PodSet 'Pod Set' axial=BOTTOM:0.0 x=0.2 len=0.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
|       ParallelStage 'Booster Set' axial=BOTTOM:0.0 x=0.2 len=0.0 stage=1 sep=EJECTION:0.0:200.0 inst=2 radius=RELATIVE:0.0 angle=RELATIVE:0.0
| decals='decals/chute.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true,false] motors=0)out"},
    {.name = "ia-without-an-inside-still-fails", .xml = R"xml(<subcomponents><stage><insideappearance><shine>abc</shine></insideappearance><subcomponents></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "abc"
EVENTS 1 {tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.0 stage=0 sep=EJECTION:0.0:200.0
| selected=default
| config default name='[{motors}]' preload=null active=[false] motors=0)out"},
    {.name = "ia-inside-and-outside", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20" blue="30" alpha="40"/><shine>0.5</shine><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance><insideappearance><edgessameasinside>true</edgessameasinside><insidesameasoutside>true</insidesameasoutside><paint red="1" green="2" blue="3"/><shine>0.1</shine><decal name="decals/inner.png" rotation="0" edgemode="MIRROR"></decal></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero,tree=1, nonfunc=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=10,20,30,40 shine=0.5 opacity=false decal='decals/a.png':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0] inside=[paint=1,2,3,255 shine=0.1 opacity=false decal='decals/inner.png':0.0:MIRROR:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0] insideflags=true,true
| decals='decals/a.png','decals/inner.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-twice", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><paint red="1" green="2" blue="3"/><shine>0.1</shine><edgessameasinside>true</edgessameasinside></insideappearance><inside-appearance><shine>0.9</shine></inside-appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 5 {mass,aero,tree=1, nonfunc=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.9 opacity=false] insideflags=true,false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-shine-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><edgessameasinside>true</edgessameasinside><shine>x</shine></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false insideflags=true,false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-decal-edgemode-none", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><decal name="decals/a.png" rotation="1.5" edgemode="Sticker"></decal></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: No enum constant info.openrocket.core.appearance.Decal.EdgeMode.Sticker
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ia-pairs-outside-a-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><center x="1" y="2"/></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown attributes in element 'center', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 2> kOwn{{
    // Decision L4: a decal without name, of which OpenRocket dies; here it is ignored with a warning, and the rest of the inside appearance is applied.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ia-decal-without-name", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><decal rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><edgessameasinside>true</edgessameasinside></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false] insideflags=true,false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a center without x, of which OpenRocket dies; here it is passed over with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ia-center-without-x", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><insideappearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center y="1"/></decal></insideappearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false inside=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES InsideAppearanceHandler
// clang-format on

TEST(InsideAppearanceHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(InsideAppearanceHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(InsideAppearanceHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(InsideAppearanceHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// A rocket with a stage and a body tube, in the environment of a load.
struct Design
{
    Design()
      : stage(fixture.rocket().addChild(std::make_unique<AxialStage>())),
        tube(stage.addChild(std::make_unique<BodyTube>()))
    {
    }

    RocketLoadFixture fixture;
    AxialStage&       stage;
    BodyTube&         tube;
};

// The element's appearance becomes the inside appearance of the component when the element
// ends, and the outside appearance stays as it is. The two flags are set as they are read, each
// with an event of its own when it changes (a GRAPHIC_CHANGE, which counts as a change that is
// not functional, as the NONFUNCTIONAL_CHANGE of the appearance does).
TEST(InsideAppearanceHandler, SetsTheInsideAppearanceAndTheFlags)
{
    Design                             design;
    RocketEventRecorder                events(design.fixture.rocket());
    InsideAppearanceHandler            handler(design.tube, design.fixture.context());
    WarningSet                         warnings;
    const InsideColorComponentHandler& inside = design.tube.getInsideColorComponentHandler();

    ASSERT_TRUE(handler.closeElement("edgessameasinside", {}, "true", warnings).has_value());
    EXPECT_TRUE(inside.isEdgesSameAsInside());
    EXPECT_FALSE(inside.isSeparateInsideOutside());
    EXPECT_EQ(events.take(), "C[BodyTube,nonfunctional]");
    // A flag that is as it is set fires nothing.
    ASSERT_TRUE(handler.closeElement("edgesSameAsInside", {}, "TRUE", warnings).has_value());
    EXPECT_EQ(events.take(), "");

    // The value goes to setSeparateInsideOutside() as it is, whatever the name says.
    ASSERT_TRUE(handler.closeElement("insidesameasoutside", {}, "true", warnings).has_value());
    EXPECT_TRUE(inside.isSeparateInsideOutside());
    EXPECT_EQ(events.take(), "C[BodyTube,nonfunctional]");
    ASSERT_TRUE(handler.closeElement("insideSameAsOutside", {}, " true", warnings).has_value());
    EXPECT_FALSE(inside.isSeparateInsideOutside());
    EXPECT_EQ(events.take(), "C[BodyTube,nonfunctional]");

    ASSERT_TRUE(handler.closeElement("shine", {}, "0.2", warnings).has_value());
    EXPECT_FALSE(inside.getInsideAppearance().has_value());
    EXPECT_EQ(events.take(), "");

    ASSERT_TRUE(handler.endHandler("insideappearance", {}, "", warnings).has_value());
    EXPECT_EQ(inside.getInsideAppearance(), Appearance(Color(187, 187, 187), 0.2));
    EXPECT_FALSE(design.tube.getAppearance().has_value());
    EXPECT_EQ(events.take(), "C[BodyTube,nonfunctional]");
    EXPECT_TRUE(warnings.empty());
}

// A component that has no inside of its own colour takes nothing of the element and fires
// nothing; the element is read all the same, so its decal's image is registered and what fails
// the load of an appearance fails it here.
TEST(InsideAppearanceHandler, AComponentWithoutAnInsideTakesNothing)
{
    Design                  design;
    RocketEventRecorder     events(design.fixture.rocket());
    InsideAppearanceHandler handler(design.stage, design.fixture.context());
    WarningSet              warnings;

    ASSERT_TRUE(handler.closeElement("edgessameasinside", {}, "true", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("insidesameasoutside", {}, "true", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("shine", {}, "0.2", warnings).has_value());
    EXPECT_EQ(
        handler
            .openElement("decal", {{"name", "a.png"}, {"rotation", "0"}, {"edgemode", "REPEAT"}},
                         warnings)
            .value_or(nullptr),
        &handler);
    ASSERT_TRUE(handler.endHandler("decal", {}, "", warnings).has_value());
    ASSERT_TRUE(handler.endHandler("insideappearance", {}, "", warnings).has_value());

    EXPECT_FALSE(design.stage.getAppearance().has_value());
    EXPECT_EQ(events.take(), "");
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(design.fixture.document().getDecalList().size(), 1U);
    EXPECT_NE(design.fixture.document().findDecalImage("decals/a.png"), nullptr);

    const Result<void> shine = handler.closeElement("shine", {}, "x", warnings);
    ASSERT_FALSE(shine.has_value());
    EXPECT_EQ(shine.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(shine.error().message, "For input string: \"x\"");
}

// The two flags are children of an <insideappearance> only: in an <appearance> they are
// unknown, and their text is warned of.
TEST(InsideAppearanceHandler, TheFlagsAreNoChildrenOfAnOutsideAppearance)
{
    Design            design;
    AppearanceHandler handler(design.tube, design.fixture.context());
    WarningSet        warnings;

    ASSERT_TRUE(handler.closeElement("edgessameasinside", {}, "true", warnings).has_value());
    EXPECT_FALSE(design.tube.getInsideColorComponentHandler().isEdgesSameAsInside());
    EXPECT_EQ(warningTexts(warnings),
              Texts{"Unknown text in element 'edgessameasinside', ignoring."});
}

}  // namespace
