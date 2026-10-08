#include "QtRocket/file/openrocket/FinSetPointHandler.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/util/Coordinate.h"
#include "document/DocumentTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// FinSetPointHandler: the handler of the <finpoints> element of a freeform fin set. What a
// file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::FinSetPointHandler;
using QtRocket::FreeformFinSet;
using QtRocket::PlainTextHandler;
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
// probes/tier9b-component-handlers from the cases (scripts/make_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES FinSetPointHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 27> kJava{{
    {.name = "fp-basic", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-any-element-is-a-point", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><bogus x="0.02" y="0.05"/><POINT x="0.08" y="0.05"/><finpoints x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints a="1">txt<point x="0" y="0"/><point x="0.02" y="0.05" z="1">t</point><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'point', ignoring.
W Unknown attributes in element 'point', ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-missing-coordinates", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02"/><point y="0.05"/><point/><point x="0.05" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.05,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-unreadable-coordinates", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="a" y="0.05"/><point x="0.02" y="b"/><point x="" y=""/><point x="0.05" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.05,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-number-forms", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x=" 0.02 " y="0x1p-5"/><point x="8e-2d" y="+0.05"/><point x=".1" y="-0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.03125;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-two-points", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-one-point", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=1.0 len=0.0 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.0:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-one-point-off-the-origin", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0.3" y="0.2"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=1.0 len=0.0 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.0:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-crossing-outline", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.1" y="0.1"/><point x="0.0" y="0.1"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-not-from-the-origin", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0.2" y="0.1"/><point x="0.22" y="0.15"/><point x="0.28" y="0.15"/><point x="0.3" y="0.1"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.09999999999999998 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.024999999999999988 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.01999999999999999,0.04999999999999999;0.08000000000000002,0.04999999999999999;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-large", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="1" y="3"/><point x="4" y="5"/><point x="3" y="1"/><point x="6" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=-1.5 len=2.5 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:1.225 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;1.0,2.5;4.0,2.5;2.5,1.0;2.5,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-negative", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="-0.02" y="0.05"/><point x="0.05" y="-0.01"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;-0.02,0.05;0.05,0.0;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-backwards", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0.1" y="0"/><point x="0.08" y="0.05"/><point x="0.02" y="0.05"/><point x="0" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=1.1 len=-0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.0:MIDDLE:0.0:-0.05 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;-0.020000000000000004,0.05;-0.08,0.05;-0.10000000000000009,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-below-the-surface", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.03" y="-0.02"/><point x="0.06" y="0.05"/><point x="0.1" y="0.02"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.03,0.0;0.06,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-many", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0.000" y="0.00"/><point x="0.001" y="0.01"/><point x="0.002" y="0.02"/><point x="0.003" y="0.03"/><point x="0.004" y="0.04"/><point x="0.005" y="0.05"/><point x="0.006" y="0.06"/><point x="0.007" y="0.00"/><point x="0.008" y="0.01"/><point x="0.009" y="0.02"/><point x="0.010" y="0.03"/><point x="0.011" y="0.04"/><point x="0.012" y="0.05"/><point x="0.013" y="0.06"/><point x="0.014" y="0.00"/><point x="0.015" y="0.01"/><point x="0.016" y="0.02"/><point x="0.017" y="0.03"/><point x="0.018" y="0.04"/><point x="0.019" y="0.05"/><point x="0.020" y="0.06"/><point x="0.021" y="0.00"/><point x="0.022" y="0.01"/><point x="0.023" y="0.02"/><point x="0.024" y="0.03"/><point x="0.025" y="0.04"/><point x="0.026" y="0.05"/><point x="0.027" y="0.06"/><point x="0.028" y="0.00"/><point x="0.029" y="0.01"/><point x="0.030" y="0.02"/><point x="0.031" y="0.03"/><point x="0.032" y="0.04"/><point x="0.033" y="0.05"/><point x="0.034" y="0.06"/><point x="0.035" y="0.00"/><point x="0.036" y="0.01"/><point x="0.037" y="0.02"/><point x="0.038" y="0.03"/><point x="0.039" y="0.04"/><point x="0.040" y="0.05"/><point x="0.041" y="0.06"/><point x="0.042" y="0.00"/><point x="0.043" y="0.01"/><point x="0.044" y="0.02"/><point x="0.045" y="0.03"/><point x="0.046" y="0.04"/><point x="0.047" y="0.05"/><point x="0.048" y="0.06"/><point x="0.049" y="0.00"/><point x="0.050" y="0.01"/><point x="0.051" y="0.02"/><point x="0.052" y="0.03"/><point x="0.053" y="0.04"/><point x="0.054" y="0.05"/><point x="0.055" y="0.06"/><point x="0.056" y="0.00"/><point x="0.057" y="0.01"/><point x="0.058" y="0.02"/><point x="0.059" y="0.03"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.001,0.01;0.002,0.02;0.003,0.03;0.004,0.04;0.005,0.05;0.006,0.06;0.007,0.0;0.008,0.01;0.009,0.02;0.01,0.03;0.011,0.04;0.012,0.05;0.013,0.06;0.014,0.0;0.015,0.01;0.016,0.02;0.017,0.03;0.018,0.04;0.019,0.05;0.02,0.06;0.021,0.0;0.022,0.01;0.023,0.02;0.024,0.03;0.025,0.04;0.026,0.05;0.027,0.06;0.028,0.0;0.029,0.01;0.03,0.02;0.031,0.03;0.032,0.04;0.033,0.05;0.034,0.06;0.035,0.0;0.036,0.01;0.037,0.02;0.038,0.03;0.039,0.04;0.04,0.05;0.041,0.06;0.042,0.0;0.043,0.01;0.044,0.02;0.045,0.03;0.046,0.04;0.047,0.05;0.048,0.06;0.049,0.0;0.05,0.01;0.051,0.02;0.052,0.03;0.053,0.04;0.054,0.05;0.055,0.06;0.056,0.0;0.057,0.01;0.058,0.02;0.059,0.03;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-tab-is-placed-again", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="end">-0.01</tabposition><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {mass=3, mass,aero=3, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999995:0.07 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-tab-front", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="front">0.01</tabposition><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {mass=3, mass,aero=3, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:TOP:0.01:0.01 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-tab-center", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="center">0.0</tabposition><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {mass=3, mass,aero=3, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:MIDDLE:0.0:0.04 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-tab-behind-the-points", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints><tabheight>0.01</tabheight><tablength>0.02</tablength><tabposition relativeto="end">-0.01</tabposition></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 10 {mass=3, mass,aero=3, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:BOTTOM:-0.009999999999999995:0.07 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-tab-longer-than-the-fin", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><tabheight>0.01</tabheight><tablength>0.05</tablength><tabposition relativeto="front">0.0</tabposition><finpoints><point x="0" y="0"/><point x="0.01" y="0.02"/><point x="0.02" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass=2, mass,aero=3, mass,aero,tree=2, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.98 len=0.02 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.01:0.02:TOP:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.01,0.02;0.020000000000000018,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-finpoints-twice", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints><finpoints><point x="0" y="0"/><point x="0.03" y="0.03"/><point x="0.06" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero=4, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.94 len=0.06 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.03499999999999999:MIDDLE:0.0:0.012500000000000004 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.03,0.03;0.06000000000000005,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-child-in-a-point", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"><z/></point><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element z, ignoring.
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-on-a-transition", .xml = R"xml(<subcomponents><stage><subcomponents><transition><length>0.2</length><foreradius>0.02</foreradius><aftradius>0.05</aftradius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></transition></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero=4, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     Transition 'Transition' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=CONICAL:0.0:false fore=0.02:false aft=0.05:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.1,0.015
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-on-a-nose-cone", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><length>0.2</length><aftradius>0.05</aftradius><subcomponents><freeformfinset><axialoffset method="bottom">0</axialoffset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero=4, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.05:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.1 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.1,0.01193220895354237
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-with-position-and-count", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><instancecount>4</instancecount><axialoffset method="bottom">-0.05</axialoffset><thickness>0.004</thickness><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass,aero=6, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:-0.05 x=0.85 len=0.1 inst=4 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=4 cant=0.0 thick=0.004 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "fp-position-middle", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><axialoffset method="middle">0.1</axialoffset><finpoints><point x="0.2" y="0.1"/><point x="0.22" y="0.15"/><point x="0.28" y="0.15"/><point x="0.3" y="0.1"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero=4, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=MIDDLE:0.1 x=0.55 len=0.09999999999999998 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.024999999999999988 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.01999999999999999,0.04999999999999999;0.08000000000000002,0.04999999999999999;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 6> kOwn{{
    // Decision L4: without a point OpenRocket dies of an IndexOutOfBoundsException; here the outline stays, with a warning.
    // OpenRocket: RESULT THROWN java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
    {.name = "fp-no-point", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: without a readable point OpenRocket dies of an IndexOutOfBoundsException; here the outline stays (the warning is the one the points gave already).
    // OpenRocket: RESULT THROWN java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
    {.name = "fp-no-readable-point", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="a" y="0"/><point/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 5 {mass,aero=2, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.025,0.05;0.075,0.05;0.050000000000000044,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a point with a NaN is passed over (OpenRocket takes it, and its fin set then gives the whole outline up).
    // OpenRocket: |       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardbo ...
    {.name = "fp-nan", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="NaN" y="0.1"/><point x="0.05" y="0.05"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.05,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a point with an infinity is passed over (OpenRocket takes it, and its fin set then gives the whole outline up).
    // OpenRocket: |       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardbo ...
    {.name = "fp-infinity", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.05" y="Infinity"/><point x="-Infinity" y="0.05"/><point x="0.05" y="0.05"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.05,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L3: a coordinate too large for a double is an infinity; the point is passed over (OpenRocket takes it).
    // OpenRocket: |       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.95 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardbo ...
    {.name = "fp-overflowing", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="1e400" y="0.05"/><point x="0.05" y="0.05"/><point x="0.1" y="0.0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.05,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: without a point OpenRocket dies of an IndexOutOfBoundsException; here the outline of the first element stays, with a warning.
    // OpenRocket: RESULT THROWN java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
    {.name = "fp-second-finpoints-empty", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><length>1</length><radius>0.05</radius><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.02" y="0.05"/><point x="0.08" y="0.05"/><point x="0.1" y="0"/></finpoints><finpoints/></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Illegal fin points specification, ignoring.
ROOT rocket {} []
EVENTS 6 {mass,aero=3, mass,aero,tree=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.0 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=1.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.05:false thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false
|       FreeformFinSet 'Freeform Fin Set' axial=BOTTOM:0.0 x=0.9 len=0.1 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.025 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.02,0.05;0.08,0.05;0.09999999999999998,0.0
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES FinSetPointHandler
// clang-format on

TEST(FinSetPointHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(FinSetPointHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(FinSetPointHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(FinSetPointHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

// The points are collected while the element is read and become the outline, all at once, when
// it closes: one event of the fin set, not one per point.
TEST(FinSetPointHandler, SetsThePointsWhenItsElementCloses)
{
    RocketLoadFixture             fixture;
    AxialStage&                   stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&                     tube  = stage.addChild(std::make_unique<BodyTube>());
    FreeformFinSet&               fins  = tube.addChild(std::make_unique<FreeformFinSet>());
    const std::vector<Coordinate> before = fins.getFinPoints();
    RocketEventRecorder           events(fixture.rocket());
    FinSetPointHandler            handler(fins, fixture.context());
    WarningSet                    warnings;

    // Every child is plain text, whatever its name.
    EXPECT_EQ(handler.openElement("point", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());

    ASSERT_TRUE(handler.closeElement("point", {{"x", "0"}, {"y", "0"}}, "", warnings).has_value());
    ASSERT_TRUE(
        handler.closeElement("point", {{"x", "0.04"}, {"y", "0.06"}}, "", warnings).has_value());
    ASSERT_TRUE(
        handler.closeElement("point", {{"x", "0.08"}, {"y", "0"}}, "", warnings).has_value());
    EXPECT_EQ(fins.getFinPoints(), before);
    EXPECT_EQ(events.take(), "");

    ASSERT_TRUE(handler.endHandler("finpoints", {}, "", warnings).has_value());
    EXPECT_EQ(
        fins.getFinPoints(),
        (std::vector<Coordinate>{Coordinate(0, 0), Coordinate(0.04, 0.06), Coordinate(0.08, 0)}));
    EXPECT_EQ(fins.getLength(), 0.08);
    EXPECT_EQ(events.take(), "C[FreeformFinSet]");
    EXPECT_TRUE(warnings.empty());
}

// Decision L4: OpenRocket dies of an IndexOutOfBoundsException when no point could be read.
TEST(FinSetPointHandler, LeavesTheOutlineWhenItHasNoPoint)
{
    RocketLoadFixture             fixture;
    AxialStage&                   stage = fixture.rocket().addChild(std::make_unique<AxialStage>());
    BodyTube&                     tube  = stage.addChild(std::make_unique<BodyTube>());
    FreeformFinSet&               fins  = tube.addChild(std::make_unique<FreeformFinSet>());
    const std::vector<Coordinate> before = fins.getFinPoints();
    RocketEventRecorder           events(fixture.rocket());
    FinSetPointHandler            handler(fins, fixture.context());
    WarningSet                    warnings;

    ASSERT_TRUE(handler.endHandler("finpoints", {}, "", warnings).has_value());
    EXPECT_EQ(fins.getFinPoints(), before);
    EXPECT_EQ(events.take(), "");
    EXPECT_EQ(warningTexts(warnings), Texts{"Illegal fin points specification, ignoring."});
}

}  // namespace
