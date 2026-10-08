#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The exit check of the loader's rocket side (tier 9b): everything the designs hold has
// someone to read it. A pair is the element of a component and an element directly in it, and
// the table lists every pair that occurs in the 16 example designs (data/examples) or in the 13
// test rockets as OpenRocket saved them (tests/data/goldens/testrocket-*/resave/rocket.ork):
// 287 and 223 parameters, which a setter of DocumentConfig's table has to read, and 33 and 11
// elements that ComponentParameterHandler hands to a handler of their own.
//
// That the designs themselves are read as OpenRocket reads them, value by value, is what
// rocket_element_tests.cpp compares; this is about the vocabulary, pair by pair, so that a
// pair that loses its setter is named.

namespace
{

using QtRocket::ComponentKind;
using QtRocket::DocumentConfig;
using QtRocket::RocketComponent;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::RocketLoadFixture;

using Texts = std::vector<std::string>;

/// The element of a component and an element directly in it, and where the pair occurs.
struct ElementPair
{
    /// The element of the component ("bodytube"; "rocket" for the rocket itself).
    std::string_view component;
    /// The element in it ("length").
    std::string_view element;
    /// Whether ComponentParameterHandler hands the element to a handler of its own
    /// ("subcomponents", "motormount", ...); else it is a parameter, read by a setter.
    bool container;
    /// Whether one of the 16 example designs has the pair.
    bool inExamples;
    /// Whether one of the 13 test rockets has it.
    bool inTestRockets;
};

// The table is written by scripts/vocabulary.py of the scratchpad's
// probes/tier9b-document-handlers from the design files themselves: do not edit it by hand.
// clang-format off
// BEGIN GENERATED TABLE pairs
constexpr std::array<ElementPair, 337> kPairs{{
    {.component = "bodytube", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "bodytube", .element = "finish", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "insideappearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "bodytube", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "motormount", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "preset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "bodytube", .element = "radius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "bodytube", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "instanceseparation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "outerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "bulkhead", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "centeringring", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "centeringring", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "innerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "instanceseparation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "outerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "centeringring", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "ellipticalfinset", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "cant", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "crosssection", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "filletmaterial", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "filletradius", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "fincount", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "finish", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "height", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "id", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "material", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "name", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "position", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "rootchord", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "rotation", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "ellipticalfinset", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "engineblock", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "engineblock", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "outerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "engineblock", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "freeformfinset", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "cant", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "crosssection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "filletmaterial", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "filletradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "fincount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "finish", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "finpoints", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "rotation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "freeformfinset", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "innertube", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "clusterconfiguration", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "clusterrotation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "clusterscale", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "innertube", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "motormount", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "outerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "innertube", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "launchlug", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "finish", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "instanceseparation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "radius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "launchlug", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "masscomponent", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "mass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "masscomponenttype", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "packedlength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "packedradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "masscomponent", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "aftradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "aftshouldercapped", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "aftshoulderlength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "aftshoulderradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "aftshoulderthickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "color", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "finish", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "insideappearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "isflipped", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "preset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "shape", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "shapeclipped", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "shapeparameter", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "nosecone", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "nosecone", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "parachute", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "cd", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "deployaltitude", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "deploydelay", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "deployevent", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "deploymentconfiguration", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "parachute", .element = "diameter", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "linecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "linelength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "linematerial", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "packedlength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "packedradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "preset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "parachute", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parachute", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "separationaltitude", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "separationconfiguration", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "separationdelay", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "separationevent", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "parallelstage", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "podset", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "railbutton", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "baseheight", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "finish", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "flangeheight", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "height", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "id", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "innerdiameter", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "instanceseparation", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "material", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "name", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "outerdiameter", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "position", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "railbutton", .element = "screwheight", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "rocket", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "rocket", .element = "designer", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "rocket", .element = "designtype", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "motorconfiguration", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "referencetype", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "rocket", .element = "revision", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "rocket", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "shockcord", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "shockcord", .element = "cordlength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "packedlength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "packedradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "shockcord", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "stage", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "stage", .element = "separationaltitude", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "separationconfiguration", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "stage", .element = "separationdelay", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "separationevent", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "stage", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = true},
    {.component = "streamer", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "cd", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "deployaltitude", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "deploydelay", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "deployevent", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "id", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "material", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "name", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "packedlength", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "packedradius", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "position", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "striplength", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "streamer", .element = "stripwidth", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "transition", .element = "aftradius", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "aftshouldercapped", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "aftshoulderlength", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "aftshoulderradius", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "aftshoulderthickness", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "finish", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "foreradius", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "foreshouldercapped", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "foreshoulderlength", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "foreshoulderradius", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "foreshoulderthickness", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "id", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "length", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "material", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "name", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "shape", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "transition", .element = "thickness", .container = false, .inExamples = false, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "cant", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "comment", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "crosssection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "filletmaterial", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "filletradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "fincount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "finish", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "height", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "insideappearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "rootchord", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "rotation", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "sweeplength", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "tabheight", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "tablength", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "tabposition", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "trapezoidfinset", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "trapezoidfinset", .element = "tipchord", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "id", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "length", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "material", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "name", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "outerradius", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "overridemass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "overridesubcomponentsmass", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "position", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "radialdirection", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "radialposition", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubecoupler", .element = "subcomponents", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "tubecoupler", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = true},
    {.component = "tubefinset", .element = "angleoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "appearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "axialoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "fincount", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "finish", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "id", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "insideappearance", .container = true, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "instancecount", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "length", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "material", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "name", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "position", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "radius", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "radiusoffset", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "rotation", .container = false, .inExamples = true, .inTestRockets = false},
    {.component = "tubefinset", .element = "thickness", .container = false, .inExamples = true, .inTestRockets = false},
}};
// END GENERATED TABLE pairs
// clang-format on

/// How many pairs of the table are @p container and occur where @p where says.
[[nodiscard]] std::ptrdiff_t countPairs(bool container, bool ElementPair::* where)
{
    return std::ranges::count_if(kPairs, [container, where](const ElementPair& pair) {
        return pair.container == container && pair.*where;
    });
}

/// The kind of the component whose element is @p element.
[[nodiscard]] ComponentKind kindOf(std::string_view element)
{
    if (element == "rocket")
    {
        return ComponentKind::ROCKET;
    }
    const std::unique_ptr<RocketComponent> component = DocumentConfig::createComponent(element);
    if (component == nullptr)
    {
        QtRocket::bug(std::format("no component for the element {}", element));
    }
    return component->kind();
}

/// The parameters of the table for which the setter table has no setter.
[[nodiscard]] Texts parametersWithoutASetter()
{
    Texts without;
    for (const ElementPair& pair : kPairs)
    {
        if (!pair.container &&
            DocumentConfig::findSetter(kindOf(pair.component), pair.element).setter == nullptr)
        {
            without.push_back(std::format("{}/{}", pair.component, pair.element));
        }
    }
    return without;
}

/// The content of a rocket element in which a component of the element @p component, where a
/// design has one, holds the empty element @p element and nothing else.
[[nodiscard]] std::string designWith(std::string_view component, std::string_view element)
{
    if (component == "rocket")
    {
        return std::format("<{}/>", element);
    }
    const std::string one = std::format("<{0}><{1}/></{0}>", component, element);
    if (component == "stage")
    {
        return std::format("<subcomponents>{}</subcomponents>", one);
    }
    if (component == "nosecone" || component == "bodytube" || component == "transition")
    {
        return std::format(
            "<subcomponents><stage><subcomponents>{}</subcomponents></stage>"
            "</subcomponents>",
            one);
    }
    return std::format(
        "<subcomponents><stage><subcomponents><bodytube><subcomponents>{}"
        "</subcomponents></bodytube></subcomponents></stage></subcomponents>",
        one);
}

/// Whether @p warning says that nobody reads an element, or that a component has no place.
[[nodiscard]] bool tellsOfSomethingUnknown(std::string_view warning)
{
    return warning.starts_with("Unknown parameter type") ||
           warning.starts_with("Unknown element") || warning.starts_with("Illegal component") ||
           warning.contains("cannot be attached");
}

/// The pairs of the table that are not read when a design with nothing but the pair goes
/// through the test root: a warning says that the element or the component is unknown, or the
/// component is not in the rocket afterwards. The element is empty, so its setter or handler
/// may refuse its value (a warning of its own, or a failed load for an id): what counts is
/// that the element reached one.
[[nodiscard]] Texts pairsNobodyReads()
{
    Texts unread;
    for (const ElementPair& pair : kPairs)
    {
        RocketLoadFixture fixture;
        const HandlerRun  run = fixture.load(designWith(pair.component, pair.element));
        for (const std::string& warning : run.texts())
        {
            if (tellsOfSomethingUnknown(warning))
            {
                unread.push_back(std::format("{}/{}: {}", pair.component, pair.element, warning));
            }
        }
        const ComponentKind kind  = kindOf(pair.component);
        bool                found = false;
        for (const RocketComponent& component : fixture.rocket().subtree())
        {
            found = found || component.kind() == kind;
        }
        if (!found)
        {
            unread.push_back(std::format("{}/{}: the component is not in the rocket",
                                         pair.component, pair.element));
        }
    }
    return unread;
}

// The counts of the scout's survey of the same files (tier9-scout-loader-components,
// out/vocab-examples.out and vocab-testrockets.out), which this table repeats.
TEST(DesignVocabulary, HasThePairsOfTheExamplesAndOfTheTestRockets)
{
    EXPECT_EQ(countPairs(false, &ElementPair::inExamples), 287);
    EXPECT_EQ(countPairs(true, &ElementPair::inExamples), 33);
    EXPECT_EQ(countPairs(false, &ElementPair::inTestRockets), 223);
    EXPECT_EQ(countPairs(true, &ElementPair::inTestRockets), 11);
    EXPECT_EQ(kPairs.size(), 337U);
}

TEST(DesignVocabulary, EveryParameterOfTheDesignsHasASetter)
{
    EXPECT_EQ(parametersWithoutASetter(), Texts{});
}

TEST(DesignVocabulary, EveryPairOfTheDesignsIsReadThroughTheRoot)
{
    EXPECT_EQ(pairsNobodyReads(), Texts{});
}

// The check checks: a pair no design has is told of.
TEST(DesignVocabulary, APairNobodyReadsIsToldOf)
{
    RocketLoadFixture fixture;
    const Texts       warnings = fixture.load(designWith("nosecone", "foreradius")).texts();
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(tellsOfSomethingUnknown(warnings.front()));
    EXPECT_EQ(DocumentConfig::findSetter(kindOf("nosecone"), "foreradius").setter, nullptr);

    RocketLoadFixture misplaced;
    const Texts       place = misplaced.load(designWith("stage", "motormount")).texts();
    ASSERT_EQ(place.size(), 1U);
    EXPECT_TRUE(tellsOfSomethingUnknown(place.front()));
    EXPECT_FALSE(tellsOfSomethingUnknown("Invalid parameter encountered, ignoring."));
}

}  // namespace
