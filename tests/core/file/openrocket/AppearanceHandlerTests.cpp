#include "QtRocket/file/openrocket/AppearanceHandler.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/ZipFileAttachmentFactory.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "document/DocumentTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// AppearanceHandler: the handler of the <appearance> element of a component. What a file's text
// gives is compared with what OpenRocket makes of the same text: the cases are the content of a
// rocket element, read by the component handlers above it.

namespace
{

using QtRocket::Appearance;
using QtRocket::AppearanceHandler;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::bug;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::Coordinate;
using QtRocket::Decal;
using QtRocket::DecalImage;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::FileSystemAttachmentFactory;
using QtRocket::NoseCone;
using QtRocket::OpenRocketDocument;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::ZipFileAttachmentFactory;
using QtRocket::ZipWriter;
using QtRocket::Test::casesThatThrowWhenCutOff;
using QtRocket::Test::failedRocketCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedRocketCases;
using QtRocket::Test::RocketCase;
using QtRocket::Test::RocketEventRecorder;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::TempDir;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (HandlerProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES AppearanceHandler
// What OpenRocket makes of each case (HandlerProbe.java of part R3), which QtRocket has to
// make of it too.
constexpr std::array<RocketCase, 52> kJava{{
    {.name = "ap-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-paint", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20" blue="30"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=10,20,30,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-paint-with-alpha", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="0" green="255" blue="+7" alpha="40"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=0,255,7,40 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-paint-with-text-and-more", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="3" hue="9">text</paint></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=1,2,3,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-paint-that-is-none", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="10" green="20"/></appearance></nosecone><bodytube><appearance><paint red="256" green="0" blue="0"/></appearance></bodytube><bodytube><appearance><paint red="1" green="-1" blue="0"/></appearance></bodytube><bodytube><appearance><paint red="1" green="2" blue="x"/></appearance></bodytube><bodytube><appearance><paint red="1" green="2" blue="3" alpha="1.0"/></appearance></bodytube><bodytube><appearance><paint red=" 1" green="2" blue="3"/></appearance></bodytube><bodytube><appearance><paint/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 15 {mass,aero,tree=7, nonfunc=7, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.3499999999999999 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.95 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.15 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-paint-twice", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="3"/><paint red="4" green="5" blue="6" alpha="7"/><paint red="x" green="5" blue="6"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=4,5,6,7 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>0.5</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.5 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-forms", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine> 0.7 </shine></appearance></nosecone><bodytube><appearance><shine>2</shine></appearance></bodytube><bodytube><appearance><shine>-1</shine></appearance></bodytube><bodytube><appearance><shine>1.5d</shine></appearance></bodytube><bodytube><appearance><shine>0x1p-1</shine></appearance></bodytube><bodytube><appearance><shine>1e-400</shine></appearance></bodytube><bodytube><appearance><shine>0.1</shine><shine>0.2</shine></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 15 {mass,aero,tree=7, nonfunc=7, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.3499999999999999 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.7 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=1.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=1.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.5 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.95 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.15 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.2 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-that-is-not-finite", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>NaN</shine></appearance></nosecone><bodytube><appearance><shine>Infinity</shine></appearance></bodytube><bodytube><appearance><shine>-Infinity</shine></appearance></bodytube><bodytube><appearance><shine>1e999</shine></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass,aero,tree=4, nonfunc=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.75 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=NaN opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=1.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.0 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=1.0 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="3"/><shine>abc</shine><shine>0.5</shine></appearance></nosecone></subcomponents></stage></subcomponents><name>never set</name>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "abc"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine></shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: empty String
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-blank", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>  </shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: empty String
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-two-points", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>1..2</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: multiple points
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-shine-inf", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>Inf</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "Inf"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-opacity", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><opacityaffectstexture>true</opacityaffectstexture></appearance></nosecone><bodytube><appearance><opacityaffectstexture>TRUE</opacityaffectstexture></appearance></bodytube><bodytube><appearance><opacityaffectstexture> true </opacityaffectstexture></appearance></bodytube><bodytube><appearance><opacityaffectstexture>yes</opacityaffectstexture></appearance></bodytube><bodytube><appearance><opacityaffectstexture></opacityaffectstexture></appearance></bodytube><bodytube><appearance><opacityaffectstexture>true</opacityaffectstexture><opacityaffectstexture>false</opacityaffectstexture></appearance></bodytube><bodytube><appearance><opacityaffectstexture a="1">tRuE</opacityaffectstexture></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 15 {mass,aero,tree=7, nonfunc=7, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.3499999999999999 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=true]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=true]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.95 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.15 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=true]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-without-children", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-edge-modes", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="REPEAT"></decal></appearance></nosecone><bodytube><appearance><decal name="decals/a.png" rotation="1.5" edgemode="MIRROR"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="1.5" edgemode="CLAMP"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 9 {mass,aero,tree=4, nonfunc=4, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.75 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:MIRROR:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:CLAMP:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-edgemode-in-lower-case", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="sticker"></decal></appearance></nosecone></subcomponents></stage></subcomponents><name>never set</name>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: No enum constant info.openrocket.core.appearance.Decal.EdgeMode.sticker
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-edgemode-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode=""></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: No enum constant info.openrocket.core.appearance.Decal.EdgeMode.
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-edgemode-with-blanks", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode=" REPEAT "></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: No enum constant info.openrocket.core.appearance.Decal.EdgeMode. REPEAT 
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-rotation-forms", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation=" -0.5 " edgemode="STICKER"></decal></appearance></nosecone><bodytube><appearance><decal name="decals/a.png" rotation="1e3" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="NaN" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="Infinity" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="-1e999" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 11 {mass,aero,tree=5, nonfunc=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.95 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':-0.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1000.0:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':NaN:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':Infinity:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':-Infinity:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-rotation-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="x" edgemode="STICKER"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-rotation-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="" edgemode="STICKER"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: empty String
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-rotation-no-number-and-without-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="x"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-pair-forms", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x=" 1.5 " y="-2"/><offset x="1e2" y="0x1p1"/><scale x="NaN" y="Infinity"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=1.5,-2.0:offset=100.0,2.0:scale=NaN,Infinity]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-pair-with-text-and-more", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="1" y="2" z="3">text</center></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=1.0,2.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-pair-twice", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="1" y="2"/><center x="3" y="4"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=3.0,4.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-center-x-no-number-and-without-y", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="a"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "a"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-center-x-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="a" y="2"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "a"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-offset-y-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><offset x="1" y="b"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "b"
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-scale-x-empty", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><scale x="" y="1"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: empty String
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-pairs-outside-a-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><center x="1" y="2"/><offset x="3" y="4">text</offset><scale/><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal><scale x="5" y="6"/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown attributes in element 'center', ignoring.
W Unknown text in element 'offset', ignoring.
W Unknown attributes in element 'offset', ignoring.
W Unknown attributes in element 'scale', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-with-text-and-more", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER" extra="1">text<bogus/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'decal', ignoring.
W Unknown attributes in element 'decal', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-with-text", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER">text</decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'decal', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-with-unknown-children", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><bogus b="1">t</bogus><shine>0.6</shine><paint red="1" green="2" blue="3"/><opacityaffectstexture>true</opacityaffectstexture></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=1,2,3,255 shine=0.6 opacity=true decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-two-decals", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="REPEAT"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><decal name="decals/b.png" rotation="1" edgemode="CLAMP"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/b.png':1.0:CLAMP:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='decals/a.png','decals/b.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-decal-in-a-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><decal name="decals/b.png" rotation="1" edgemode="CLAMP" more="1"></decal><center x="1" y="2"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown attributes in element 'decal', ignoring.
W Unknown attributes in element 'center', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/b.png':1.0:CLAMP:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png','decals/b.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-two-appearances", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="3"/><shine>0.1</shine><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance><appearance><shine>0.9</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.9 opacity=false]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-unknown-children", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><bogus x="1">text</bogus><edgessameasinside>true</edgessameasinside><Paint red="1" green="2" blue="3"/><SHINE>0.5</SHINE></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
W Unknown text in element 'edgessameasinside', ignoring.
W Unknown attributes in element 'Paint', ignoring.
W Unknown text in element 'SHINE', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-text-and-attributes", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance a="1">text<shine>0.5</shine>more</appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.5 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-element-in-a-value", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance a="1"><opacityaffectstexture>true<x/></opacityaffectstexture><paint red="1" green="2" blue="3"><y/></paint></appearance></nosecone></subcomponents></stage></subcomponents><name>R</name>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Unknown element y, ignoring.
W Unknown text in element 'nosecone', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'R' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-element-in-the-shine", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><shine>0.5<x/></shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: empty String
W Unknown element x, ignoring.
EVENTS 2 {mass,aero,tree=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-element-in-a-pair", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><bogus><x p="1"/></bogus><center x="1" y="2"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=1.0,2.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-every-component", .xml = R"xml(<subcomponents><stage><appearance><shine>0.8</shine></appearance><subcomponents><nosecone><appearance><shine>0.1</shine></appearance></nosecone><bodytube><appearance><shine>0.2</shine></appearance><subcomponents><trapezoidfinset><appearance><shine>0.3</shine></appearance></trapezoidfinset><parachute><appearance><shine>0.4</shine></appearance></parachute><masscomponent><appearance><shine>0.5</shine></appearance></masscomponent><launchlug><appearance><shine>0.6</shine></appearance></launchlug><centeringring><appearance><shine>0.7</shine></appearance></centeringring></subcomponents></bodytube></subcomponents></stage></subcomponents><appearance><shine>0.9</shine></appearance>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 17 {mass,aero,tree=4, mass,tree=3, nonfunc=9, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL app=[paint=187,187,187,255 shine=0.9 opacity=false]
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.35000000000000003 stage=0 sep=EJECTION:0.0:200.0 app=[paint=187,187,187,255 shine=0.8 opacity=false]
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.1 opacity=false]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.2 opacity=false]
|       TrapezoidFinSet 'Trapezoidal Fin Set' axial=BOTTOM:0.0 x=0.15000000000000002 len=0.05 inst=3 radius=SURFACE:0.0 angle=RELATIVE:0.0 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] fins=3 cant=0.0 thick=0.003 cross=SQUARE tab=0.0:0.05:MIDDLE:0.0:0.0 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] chord=0.05:0.05 sweep=0.025 height=0.03 app=[paint=187,187,187,255 shine=0.3 opacity=false]
|       Parachute 'Parachute' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 cd=0.8:true drogue=false mat=[SURFACE|Ripstop nylon|0.067|0.0|Fabrics] deploy=EJECTION:0.0:200.0 diameter=0.3 lines=6:0.44999999999999996:true linemat=[LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics] app=[paint=187,187,187,255 shine=0.4 opacity=false]
|       MassComponent 'Mass Component' axial=TOP:0.0 x=0.0 len=0.025 packed=0.025:0.0125:false radial=0.0:0.0 mass=0.0 type=MASSCOMPONENT app=[paint=187,187,187,255 shine=0.5 opacity=false]
|       LaunchLug 'Launch Lug' axial=MIDDLE:0.0 x=0.085 len=0.03 inst=1 angle=RELATIVE:3.141592653589793 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.005 thick=0.001 spacing=0.06 app=[paint=187,187,187,255 shine=0.6 opacity=false]
|       CenteringRing 'Centering Ring' axial=BOTTOM:0.0 x=0.198 len=0.002 inst=1 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.023:true inner=0.0:true radial=0.0:0.0 spacing=0.0 app=[paint=187,187,187,255 shine=0.7 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-file-names", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></nosecone><bodytube><appearance><decal name="textures/sub/b.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="c.jpg" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="/abs/d.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 11 {mass,aero,tree=5, nonfunc=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.95 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/b.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/c.jpg':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/d.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png','decals/b.png','decals/c.jpg','decals/d.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-file-names-that-collide", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="x/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></nosecone><bodytube><appearance><decal name="y/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="z/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="y/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="z/a (1).png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 11 {mass,aero,tree=5, nonfunc=5, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.95 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a (1).png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a (2).png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a (1).png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a (3).png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a (1).png','decals/a (2).png','decals/a (3).png','decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-file-names-without-extension", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="plain" rotation="1.5" edgemode="STICKER"></decal></appearance></nosecone><bodytube><appearance><decal name="other/plain" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 7 {mass,aero,tree=3, nonfunc=3, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.55 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/plain':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal=' (1).':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal=' (2).':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals=' (1).',' (2).','decals/plain'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-archive-names", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></nosecone><bodytube><appearance><decal name="/datafiles/textures/x.jpg" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="c.jpg" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="sub/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube><bodytube><appearance><decal name="decals/a (1).png" rotation="1.5" edgemode="STICKER"></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 15 {mass,aero,tree=7, nonfunc=7, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.3499999999999999 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.15000000000000002 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='/datafiles/textures/x.jpg':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.35000000000000003 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='c.jpg':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.55 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.75 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='sub/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.95 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=1.15 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a (1).png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='','/datafiles/textures/x.jpg','c.jpg','decals/a (1).png','decals/a.png','sub/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-archive-decal", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="/datafiles/textures/balsa.jpg" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='/datafiles/textures/balsa.jpg':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='/datafiles/textures/balsa.jpg'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-fix-decal-attributes-with-one-ignored-element", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar/></foo></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    {.name = "ap-fix-decal-attributes-reach-the-component-from-two-deep", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar><baz/></bar></foo></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<RocketCase, 24> kOwn{{
    // Decision L4: a decal without name, of which OpenRocket dies; here it is ignored with everything in it and a warning, and the rest of the appearance is applied.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-decal-without-name", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><shine>0.9</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.9 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a decal without rotation, of which OpenRocket dies (the image registered by then); here it is ignored with a warning and registers nothing.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/a.png'
    {.name = "ap-decal-without-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><shine>0.9</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.9 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a decal without edgemode, of which OpenRocket dies (the image registered by then); here it is ignored with a warning and registers nothing.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Name is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/a.png'
    {.name = "ap-decal-without-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal><shine>0.9</shine></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.9 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a decal without attributes, of which OpenRocket dies; here it is ignored with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-decal-without-anything", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal/></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: the name is looked at first, as in OpenRocket, which dies of the missing one before it reads the rotation; here the decal is ignored with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-decal-without-name-and-rotation-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal rotation="x" edgemode="STICKER"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: the rotation is looked at before the edge mode, as in OpenRocket, which dies of the missing one; here the decal is ignored with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/a.png'
    {.name = "ap-decal-without-rotation-and-edgemode-none", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" edgemode="nothing"></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: OpenRocket dies of the first decal; here it is ignored with a warning and registers no image, and the second decal is the appearance's.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Name is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    // OpenRocket: | decals='decals/lost.png'
    {.name = "ap-decal-without-edgemode-then-a-full-one", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/lost.png" rotation="1.5"></decal><decal name="decals/kept.png" rotation="1.5" edgemode="STICKER"><center x="0.1" y="0.2"/><offset x="0.3" y="0.4"/><scale x="2" y="3"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/kept.png':1.5:STICKER:center=0.1,0.2:offset=0.3,0.4:scale=2.0,3.0]
| decals='decals/kept.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: OpenRocket dies of the decal without name; here it is ignored with everything in it. As after every ignored element the enclosing elements close with what belongs one level deeper, so the attribute of the appearance element is warned of as the nose cone's.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: | Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-ignored-decal-with-text-and-elements", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance a="1"><decal rotation="1.5" edgemode="STICKER" extra="1">text<center x="1"/><bogus a="1"><deeper/></bogus></decal><shine>0.4</shine></appearance></nosecone></subcomponents></stage></subcomponents><name>R</name>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
W Unknown attributes in element 'nosecone', ignoring.
ROOT rocket {} []
EVENTS 4 {mass,aero,tree=1, nonfunc=2, tree=1}
| Rocket 'R' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.4 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a center without x, of which OpenRocket dies; here it is passed over with a warning and the offset behind it is applied.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-center-without-x", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center y="2"/><offset x="5" y="6"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=5.0,6.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a center without y, of which OpenRocket dies; here it is passed over with a warning and the offset behind it is applied.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-center-without-y", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center x="1"/><offset x="5" y="6"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=5.0,6.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: an offset without x and y, of which OpenRocket dies; here it is passed over with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-offset-without-x-and-y", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><offset/><scale x="5" y="6"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=5.0,6.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a scale without y, of which OpenRocket dies; here it is passed over with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-scale-without-y", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><scale x="5"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: x is looked at first, as in OpenRocket, which dies of the missing one before it reads y; here the center is passed over with a warning.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0: ...
    {.name = "ap-center-without-x-and-y-no-number", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><center y="b"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The ignored element shifts the attributes, so the decal closes with those of <foo>, whose one attribute is called name: OpenRocket had taken name out of the decal's own map and warns of this one; here the name is left out of whatever map the decal closes with.
    // OpenRocket: W Unknown attributes in element 'decal', ignoring.
    {.name = "ap-decal-closes-with-the-names-of-its-own", .xml = R"xml(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="decals/a.png" rotation="1.5" edgemode="STICKER"><foo name="q"><bar/></foo></decal></appearance></nosecone></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 stage=0 sep=EJECTION:0.0:200.0
|     NoseCone 'Nose Cone' axial=AFTER:0.0 x=0.0 len=0.15000000000000002 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] shape=OGIVE:1.0:false fore=0.0:false aft=0.025:false thick=0.002:false foresh=0.0:0.0:0.0:false aftsh=0.0:0.0:0.0:false flipped=false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':1.5:STICKER:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The attributes are const: two elements ignored in the <decal> hand its map to the component's element, which warns of name, rotation and edgemode here; OpenRocket took them out when the decal opened.
    {.name = "ap-fix-decal-attributes-reach-the-component", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar/></foo><foo><bar/></foo></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The attributes are const: two elements ignored in one element of the <decal> hand its map to the component's element, which warns of the three here (OpenRocket took them out).
    {.name = "ap-fix-decal-attributes-reach-the-component-from-one-element", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar/><bar/></foo></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The attributes are const: an element ignored in the <decal> and one ignored behind it hand its map to the component's element, which warns of the three here (OpenRocket took them out).
    {.name = "ap-fix-decal-attributes-reach-the-component-from-behind", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar/></foo></decal><paint red="1" green="2" blue="3"><x/></paint></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
W Unknown element x, ignoring.
W Unknown attributes in element 'bodytube', ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // The attributes are const: six elements ignored in the <decal> hand its map to the parent of the rocket element, with the three here and empty in OpenRocket.
    // OpenRocket: ROOT rocket {} []
    {.name = "ap-fix-decal-attributes-reach-the-root", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><foo><bar/></foo><foo><bar/></foo><foo><bar/></foo><foo><bar/></foo><foo><bar/></foo><foo><bar/></foo></decal></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Unknown element bar, ignoring.
ROOT rocket {edgemode=REPEAT, name=a.png, rotation=0} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false decal='decals/a.png':0.0:REPEAT:center=0.0,0.0:offset=0.0,0.0:scale=1.0,1.0]
| decals='decals/a.png'
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a <decal> without name is ignored. With the attachments of an archive OpenRocket registers an image without a name and goes on; whoever lists the images then dies (the probe's dump).
    // OpenRocket: DUMP THROWN java.lang.NullPointerException: Cannot invoke "String.replace(java.lang.CharSequence, java.lang.CharSequence)" because "<parameter1>" is n ...
    {.name = "ap-fix-archive-nameless-decal", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal rotation="0" edgemode="REPEAT"><center x="1" y="2"/></decal><shine>0.7</shine></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.7 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a <decal> without name is ignored before its rotation is looked at. With the attachments of an archive OpenRocket registers an image without a name and fails the load for the rotation.
    // OpenRocket: RESULT THROWN java.lang.NumberFormatException: For input string: "x"
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: DUMP THROWN java.lang.NullPointerException: Cannot invoke "String.compareTo(String)" because the return value of "info.openrocket.core.document.DecalR ...
    {.name = "ap-fix-archive-nameless-decal-bad-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal rotation="x" edgemode="REPEAT"/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a <decal> without name is ignored before its edge mode is looked at. With the attachments of an archive OpenRocket registers an image without a name and fails the load for the edge mode.
    // OpenRocket: RESULT THROWN java.lang.IllegalArgumentException: No enum constant info.openrocket.core.appearance.Decal.EdgeMode.nothing
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: DUMP THROWN java.lang.NullPointerException: Cannot invoke "String.compareTo(String)" because the return value of "info.openrocket.core.document.DecalR ...
    {.name = "ap-fix-archive-nameless-decal-bad-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal rotation="0" edgemode="nothing"/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a <decal> without name is ignored, of which OpenRocket dies with files as attachments (a NullPointerException), before its rotation is looked at.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "ap-fix-nameless-decal-bad-rotation", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal rotation="x" edgemode="REPEAT"/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Decision L4: a <decal> without name is ignored, of which OpenRocket dies with files as attachments (a NullPointerException), before its edge mode is looked at.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: null
    // OpenRocket: EVENTS 2 {mass,aero,tree=1, tree=1}
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "ap-fix-nameless-decal-bad-edgemode", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><decal rotation="0" edgemode="nothing"/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
    // Strings::parseInt() reads ASCII digits only: a channel written with an Arabic-Indic digit makes no colour and the paint stays (Integer.parseInt reads it as 4).
    // OpenRocket: |     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false m ...
    {.name = "ap-fix-paint-with-other-digits", .xml = R"xml(<subcomponents><stage><subcomponents><bodytube><appearance><paint red="&#1636;" green="2" blue="3"/></appearance></bodytube></subcomponents></stage></subcomponents>)xml", .expected = R"out(RESULT ok
ROOT rocket {} []
EVENTS 3 {mass,aero,tree=1, nonfunc=1, tree=1}
| Rocket 'Rocket' axial=ABSOLUTE:0.0 x=0.0 ref=MAXIMUM customref=0.01 design=ORIGINAL
|   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=0.2 stage=0 sep=EJECTION:0.0:200.0
|     BodyTube 'Body Tube' axial=AFTER:0.0 x=0.0 len=0.2 finish=NORMAL mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] r=0.025:true thick=0.002:false mount=false overhang=0.0 ign=AUTOMATIC:0.0:false app=[paint=187,187,187,255 shine=0.3 opacity=false]
| selected=default
| config default name='[{motors}]' preload=null active=[true] motors=0)out"},
}};
// END GENERATED TABLES AppearanceHandler
// clang-format on

TEST(AppearanceHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedRocketCases(kJava), Texts{});
}

TEST(AppearanceHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedRocketCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(AppearanceHandler, DISABLED_PrintsCases)
{
    std::cout << printedRocketCases(kJava) << printedRocketCases(kOwn);
}

TEST(AppearanceHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(casesThatThrowWhenCutOff(kOwn), Texts{});
}

/// A rocket with a stage, a nose cone and a body tube, in the environment of a load.
struct Design
{
    explicit Design(
        RocketLoadFixture::Attachments attachments = RocketLoadFixture::Attachments::FILES)
      : fixture(false, attachments),
        stage(fixture.rocket().addChild(std::make_unique<AxialStage>())),
        nose(stage.addChild(std::make_unique<NoseCone>())),
        tube(stage.addChild(std::make_unique<BodyTube>()))
    {
    }

    RocketLoadFixture fixture;
    AxialStage&       stage;
    NoseCone&         nose;
    BodyTube&         tube;
};

/// The decal of the appearance of @p component, which has one.
[[nodiscard]] Decal decalOf(const RocketComponent& component)
{
    const std::optional<Appearance>& appearance = component.getAppearance();
    if (!appearance.has_value() || !appearance->getTexture().has_value())
    {
        bug("the component has no decal");
    }
    return appearance->getTexture().value_or(
        Decal(Coordinate(), Coordinate(), Coordinate(), 0, "", Decal::EdgeMode::REPEAT));
}

/// The bytes of the image named @p name of @p document as text, or the code of the failure to
/// read them ("NOT_FOUND"), or "no image" when the document has none of that name.
[[nodiscard]] std::string imageText(const OpenRocketDocument& document, std::string_view name)
{
    const std::shared_ptr<DecalImage> image = document.findDecalImage(name);
    if (image == nullptr)
    {
        return "no image";
    }
    const Result<std::vector<std::byte>> bytes = image->getBytes();
    return bytes.has_value() ? QtRocket::bytesToString(*bytes)
                             : std::string(toString(bytes.error().code));
}

/// The names of the images the decal registry of @p document lists.
[[nodiscard]] Texts imageNames(const OpenRocketDocument& document)
{
    Texts names;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        names.push_back(image->getName());
    }
    return names;
}

/// The attributes of a decal named @p name.
[[nodiscard]] ElementHandler::Attributes decalAttributes(std::string name)
{
    return {{"name", std::move(name)}, {"rotation", "1.5"}, {"edgemode", "STICKER"}};
}

// A <decal> is handled by the handler itself, so its children close on it; every other child is
// plain text. A decal that lacks an attribute is ignored, by decision L4.
TEST(AppearanceHandler, HandlesADecalItselfAndTakesEverythingElseAsText)
{
    Design            design;
    AppearanceHandler handler(design.nose, design.fixture.context());
    WarningSet        warnings;

    EXPECT_EQ(
        handler.openElement("decal", decalAttributes("decals/a.png"), warnings).value_or(nullptr),
        &handler);
    EXPECT_EQ(handler.openElement("center", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("shine", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_TRUE(warnings.empty());

    EXPECT_EQ(handler.openElement("decal", {{"name", "decals/b.png"}}, warnings).value_or(&handler),
              nullptr);
    EXPECT_EQ(warningTexts(warnings), Texts{Warning::kFileInvalidParameter.toString()});
    // The ignored decal registered nothing.
    EXPECT_EQ(imageNames(design.fixture.document()), Texts{"decals/a.png"});
}

// The values are collected while the element is read and become the component's appearance, all
// at once, when it ends: one event of the component, and none when a <decal> ends.
TEST(AppearanceHandler, SetsTheAppearanceWhenItsElementEnds)
{
    Design              design;
    RocketEventRecorder events(design.fixture.rocket());
    AppearanceHandler   handler(design.nose, design.fixture.context());
    WarningSet          warnings;

    ASSERT_TRUE(handler.closeElement("shine", {}, "0.5", warnings).has_value());
    ASSERT_TRUE(
        handler.closeElement("paint", {{"red", "1"}, {"green", "2"}, {"blue", "3"}}, "", warnings)
            .has_value());
    ASSERT_TRUE(
        handler.openElement("decal", decalAttributes("decals/a.png"), warnings).has_value());
    ASSERT_TRUE(handler.closeElement("center", {{"x", "1"}, {"y", "2"}}, "", warnings).has_value());
    ASSERT_TRUE(handler.endHandler("decal", {}, "", warnings).has_value());
    EXPECT_FALSE(design.nose.getAppearance().has_value());
    EXPECT_EQ(events.take(), "");

    ASSERT_TRUE(handler.endHandler("appearance", {}, "", warnings).has_value());
    ASSERT_TRUE(design.nose.getAppearance().has_value());
    EXPECT_EQ(design.nose.getAppearance(),
              Appearance(Color(1, 2, 3), 0.5,
                         Decal(Coordinate(0, 0), Coordinate(1, 2), Coordinate(1, 1), 1.5,
                               "decals/a.png", Decal::EdgeMode::STICKER)));
    EXPECT_EQ(events.take(), "C[NoseCone,nonfunctional]");
    EXPECT_FALSE(design.tube.getAppearance().has_value());
    EXPECT_TRUE(warnings.empty());
}

// What ends OpenRocket's load with an IllegalArgumentException ends this one with that
// exception's message under ErrorCode::INVALID_ARGUMENT, which the top-level loader turns into
// "Exception loading stream: <message>".
TEST(AppearanceHandler, FailsTheLoadWithTheMessageOfJavasException)
{
    Design            design;
    AppearanceHandler handler(design.nose, design.fixture.context());
    WarningSet        warnings;

    const Result<void> shine = handler.closeElement("shine", {}, "bright", warnings);
    ASSERT_FALSE(shine.has_value());
    EXPECT_EQ(shine.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(shine.error().message, "For input string: \"bright\"");

    const Result<ElementHandler*> rotation = handler.openElement(
        "decal", {{"name", "a.png"}, {"rotation", "1,5"}, {"edgemode", "REPEAT"}}, warnings);
    ASSERT_FALSE(rotation.has_value());
    EXPECT_EQ(rotation.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(rotation.error().message, "For input string: \"1,5\"");

    const Result<ElementHandler*> edgeMode = handler.openElement(
        "decal", {{"name", "a.png"}, {"rotation", "0"}, {"edgemode", "Repeat"}}, warnings);
    ASSERT_FALSE(edgeMode.has_value());
    EXPECT_EQ(edgeMode.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(edgeMode.error().message,
              "No enum constant info.openrocket.core.appearance.Decal.EdgeMode.Repeat");

    ASSERT_TRUE(handler.openElement("decal", decalAttributes("a.png"), warnings).has_value());
    const Result<void> center =
        handler.closeElement("center", {{"x", "1"}, {"y", ""}}, "", warnings);
    ASSERT_FALSE(center.has_value());
    EXPECT_EQ(center.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(center.error().message, "empty String");
    EXPECT_TRUE(warnings.empty());
}

// The images of a design that is an archive: the decal carries the name the file gives, a slash
// in front included, and the document's image of that name gives the bytes of the entry. An
// image whose entry is missing is found out when its bytes are asked for, not while the file
// loads.
TEST(AppearanceHandler, NamesTheImagesOfAnArchiveAsTheFileDoes)
{
    ZipWriter writer;
    writer.add("rocket.ork", QtRocket::stringToBytes("<openrocket/>"));
    writer.add("decals/a.png", QtRocket::stringToBytes("image a"));
    writer.add("/datafiles/textures/x.jpg", QtRocket::stringToBytes("texture x"));
    writer.add("plain.png", QtRocket::stringToBytes("plain"));
    Result<std::vector<std::byte>> archive = writer.finish();
    ASSERT_TRUE(archive.has_value());
    const ZipFileAttachmentFactory attachments(std::move(*archive));

    Design design;
    design.fixture.context().setAttachmentFactory(&attachments);
    const HandlerRun run = design.fixture.load(
        R"xml(<subcomponents><stage><subcomponents>
<nosecone><appearance><decal name="decals/a.png" rotation="0" edgemode="REPEAT"/></appearance></nosecone>
<bodytube><appearance><decal name="/datafiles/textures/x.jpg" rotation="0" edgemode="REPEAT"/></appearance>
  <insideappearance><decal name="plain.png" rotation="0" edgemode="REPEAT"/></insideappearance></bodytube>
<bodytube><appearance><decal name="decals/a.png" rotation="1" edgemode="CLAMP"/></appearance></bodytube>
<bodytube><appearance><decal name="decals/missing.png" rotation="0" edgemode="REPEAT"/></appearance></bodytube>
</subcomponents></stage></subcomponents>)xml");
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    // The stage of the file comes behind the one the fixture's design has.
    const RocketComponent&    stage    = design.fixture.rocket().getChild(1);
    const OpenRocketDocument& document = design.fixture.document();
    EXPECT_EQ(decalOf(stage.getChild(0)).getImageName(), "decals/a.png");
    EXPECT_EQ(decalOf(stage.getChild(1)).getImageName(), "/datafiles/textures/x.jpg");
    EXPECT_EQ(decalOf(stage.getChild(2)).getImageName(), "decals/a.png");
    EXPECT_EQ(decalOf(stage.getChild(3)).getImageName(), "decals/missing.png");
    EXPECT_EQ(imageNames(document), (Texts{"/datafiles/textures/x.jpg", "decals/a.png",
                                           "decals/missing.png", "plain.png"}));

    EXPECT_EQ(imageText(document, "decals/a.png"), "image a");
    EXPECT_EQ(imageText(document, "/datafiles/textures/x.jpg"), "texture x");
    EXPECT_EQ(imageText(document, "plain.png"), "plain");
    EXPECT_EQ(imageText(document, "decals/missing.png"), "NOT_FOUND");
    // The name without its slash is another name.
    EXPECT_EQ(imageText(document, "datafiles/textures/x.jpg"), "no image");
    // Two components with the same image count as two uses of it.
    const std::shared_ptr<DecalImage> shared = document.findDecalImage("decals/a.png");
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(document.countDecalUsage(*shared), 2);
}

// The images of a design that is a file by itself are the files it names, beside it. The decal
// registry names the image of a file "decals/<file name>", whatever directory the file is in,
// and numbers the name when another file has it: the decal carries that name, not the file's.
TEST(AppearanceHandler, NamesTheImagesOfFilesAsTheRegistryDoes)
{
    const TempDir directory;
    static_cast<void>(directory.write("textures/a.png", "first a"));
    static_cast<void>(directory.write("other/a.png", "second a"));
    static_cast<void>(directory.write("b.png", "image b"));
    static_cast<void>(directory.write("decals/c.png", "image c"));
    const FileSystemAttachmentFactory attachments(directory.path());

    Design design;
    design.fixture.context().setAttachmentFactory(&attachments);
    const HandlerRun run = design.fixture.load(
        R"xml(<subcomponents><stage><subcomponents>
<nosecone><appearance><decal name="textures/a.png" rotation="0" edgemode="REPEAT"/></appearance></nosecone>
<bodytube><appearance><decal name="other/a.png" rotation="0" edgemode="REPEAT"/></appearance></bodytube>
<bodytube><appearance><decal name="b.png" rotation="0" edgemode="REPEAT"/></appearance></bodytube>
<bodytube><appearance><decal name="decals/c.png" rotation="0" edgemode="REPEAT"/></appearance></bodytube>
<bodytube><appearance><decal name="textures/a.png" rotation="1" edgemode="CLAMP"/></appearance></bodytube>
<bodytube><appearance><decal name="gone/missing.png" rotation="0" edgemode="REPEAT"/></appearance></bodytube>
</subcomponents></stage></subcomponents>)xml");
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const RocketComponent&    stage    = design.fixture.rocket().getChild(1);
    const OpenRocketDocument& document = design.fixture.document();
    EXPECT_EQ(decalOf(stage.getChild(0)).getImageName(), "decals/a.png");
    EXPECT_EQ(decalOf(stage.getChild(1)).getImageName(), "decals/a (1).png");
    EXPECT_EQ(decalOf(stage.getChild(2)).getImageName(), "decals/b.png");
    EXPECT_EQ(decalOf(stage.getChild(3)).getImageName(), "decals/c.png");
    // The same file again is the same image.
    EXPECT_EQ(decalOf(stage.getChild(4)).getImageName(), "decals/a.png");
    EXPECT_EQ(decalOf(stage.getChild(5)).getImageName(), "decals/missing.png");
    EXPECT_EQ(imageNames(document), (Texts{"decals/a (1).png", "decals/a.png", "decals/b.png",
                                           "decals/c.png", "decals/missing.png"}));

    // Each image reads the file it was made for.
    EXPECT_EQ(imageText(document, "decals/a.png"), "first a");
    EXPECT_EQ(imageText(document, "decals/a (1).png"), "second a");
    EXPECT_EQ(imageText(document, "decals/b.png"), "image b");
    EXPECT_EQ(imageText(document, "decals/c.png"), "image c");
    EXPECT_EQ(imageText(document, "decals/missing.png"), "NOT_FOUND");
    EXPECT_EQ(imageText(document, "textures/a.png"), "no image");
}

// The handler asks the context's attachment factory for the attachment of a decal's name, once
// per decal, and does not read it.
TEST(AppearanceHandler, AsksTheFactoryOfTheContextForTheAttachment)
{
    Design design(RocketLoadFixture::Attachments::ARCHIVE);
    design.fixture.fixture().attachments().put("decals/a.png", "image a");
    AppearanceHandler handler(design.nose, design.fixture.fixture().context());
    WarningSet        warnings;

    ASSERT_TRUE(
        handler.openElement("decal", decalAttributes("decals/a.png"), warnings).has_value());
    ASSERT_TRUE(
        handler.openElement("decal", decalAttributes("decals/b.png"), warnings).has_value());
    EXPECT_EQ(design.fixture.fixture().attachments().asked(),
              (Texts{"decals/a.png", "decals/b.png"}));
    EXPECT_EQ(imageNames(design.fixture.document()), (Texts{"decals/a.png", "decals/b.png"}));
}

// The document is where the images are registered: a context without one cannot be loaded into.
TEST(AppearanceHandler, AContextWithoutADocumentIsABug)
{
    const DocumentLoadingContext context;
    NoseCone                     nose;
    EXPECT_THROW(AppearanceHandler(nose, context), BugError);
}

}  // namespace
