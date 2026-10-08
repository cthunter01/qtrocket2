#include "QtRocket/file/openrocket/WindHandler.h"

#include <array>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationOptionsSupport.h"

// The cases are <conditions> elements with <wind> elements, run through
// SimulationConditionsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe CondProbe of run 9b, part S1; see
// ConditionsTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::ElementHandler;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::SimulationOptions;
using QtRocket::WarningSet;
using QtRocket::WindHandler;
using QtRocket::WindModel;
using QtRocket::WindModelType;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::ConditionsCase;
using QtRocket::Test::conditionsCaseTestName;
using QtRocket::Test::describeConditions;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::javaNumber;
using QtRocket::Test::warningTexts;

using Attributes = ElementHandler::Attributes;
using Texts      = std::vector<std::string>;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

constexpr auto kWindCases = std::to_array<ConditionsCase>({
    // BEGIN GENERATED: wind
    {.name     = "cond: wind model unknown and missing",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="gusty"><speed>5</speed></wind>
  <wind><speed>6</speed></wind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown wind model type 'gusty', using default.
  W[Other,NORMAL] Unknown wind model type 'null', using default.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: wind average NaN, Infinity and negative values",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="average">
    <speed>Infinity</speed>
    <direction>Infinity</direction>
    <standarddeviation>-Infinity</standarddeviation>
    <bogus>1</bogus>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(Infinity,NaN,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why = "decision U3: a number that is not finite is not applied; OpenRocket has no warning "
            "for these, so the three share the general one"},
    {.name     = "cond: wind average negative speed and NaN",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="average">
    <speed>-3</speed>
    <direction>NaN</direction>
    <standarddeviation>NaN</standarddeviation>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(3.0,4.71238898038469,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: multilevel without altituderef, unsorted levels, only multilevel",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel">
    <windlevel altitude="500" speed="5" direction="1" standarddeviation="0.5"/>
    <windlevel altitude="100" speed="3" direction="2" standarddeviation="0.3"/>
    <bogus/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(100.0,3.0,2.0,0.3)(500.0,5.0,1.0,0.5)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: multilevel altituderef agl and garbage",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="agl">
    <windlevel altitude="0" speed="5" direction="1" standarddeviation="0.5"/>
  </wind>
  <wind model="multilevel" altituderef="bogus">
    <windlevel altitude="10" speed="5" direction="1" standarddeviation="0.5"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=null(10.0,5.0,1.0,0.5)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Unknown wind altitude reference 'bogus', using MSL.
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(10.0,5.0,1.0,0.5)
)out",
     .why      = "decision L9: an unknown altitude reference is mean sea level, with a warning"},
    {.name     = "cond: multilevel duplicate altitude",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="100" speed="5" direction="1" standarddeviation="0.5"/>
    <windlevel altitude="100" speed="3" direction="2" standarddeviation="0.3"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Wind level already exists for altitude: 100.0]
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL(100.0,5.0,1.0,0.5)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: multilevel missing attribute",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="100" speed="5" direction="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why = "decision D9: the level is skipped where OpenRocket dies of a NullPointerException"},
    {.name     = "cond: multilevel garbage attribute",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="abc" speed="5" direction="1" standarddeviation="0.5"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "abc"]
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: multilevel NaN and Infinity attributes",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="NaN" speed="NaN" direction="NaN" standarddeviation="NaN"/>
    <windlevel altitude="Infinity" speed="Infinity" direction="Infinity" standarddeviation="Infinity"/>
    <windlevel altitude="-Infinity" speed="-2" direction="1" standarddeviation="-1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(-Infinity,2.0,4.141592653589793,0.0)(Infinity,Infinity,NaN,Infinity)(NaN,NaN,NaN,NaN)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why      = "decision L3: a level with an attribute that is not finite is skipped"},
    {.name     = "cond: multilevel two NaN altitudes",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <wind model="multilevel" altituderef="msl">
    <windlevel altitude="NaN" speed="1" direction="1" standarddeviation="0"/>
    <windlevel altitude="NaN" speed="2" direction="1" standarddeviation="0"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Wind level already exists for altitude: NaN]
  closed=(not closed)
  fcid=11111111-1111-1111-1111-111111111111
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL(NaN,1.0,1.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why      = "decision L3: both levels are skipped, so no altitude is there twice"},
    {.name     = "s1: wind average element sets the type",
     .xml      = R"xml(
<conditions>
  <windmodeltype>MultiLevel</windmodeltype>
  <wind model="average">
    <speed>3</speed>
    <direction>2</direction>
    <standarddeviation>0.5</standarddeviation>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(3.0,2.0,0.5) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average children in another order",
     .xml      = R"xml(
<conditions>
  <wind model="average">
    <standarddeviation>0.5</standarddeviation>
    <speed>3</speed>
    <direction>2</direction>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(3.0,2.0,3.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average children that are no numbers",
     .xml      = R"xml(
<conditions>
  <wind model="average">
    <speed>fast</speed>
    <direction></direction>
    <standarddeviation>1,5</standarddeviation>
  </wind>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average with text, attributes and unknown children",
     .xml      = R"xml(
<conditions>
  <wind model="average" altituderef="agl" extra="1">
    <speed unit="m/s">3</speed>
    <windlevel altitude="1" speed="2" direction="3" standarddeviation="4"/>
    <gust>9</gust>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(3.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average child with a child",
     .xml      = R"xml(
<conditions>
  <wind model="average">
    <speed>3<x/>4</speed>
    <direction>2</direction>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  closed=conditions {model=average} []
  wind=AVERAGE avg=(4.0,2.0,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind model in capitals",
     .xml      = R"xml(
<conditions>
  <wind model="Average"><speed>3</speed></wind>
  <wind model="MULTILEVEL"><windlevel altitude="1" speed="2" direction="3" standarddeviation="4"/></wind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown wind model type 'Average', using default.
  W[Other,NORMAL] Unknown wind model type 'MULTILEVEL', using default.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind multilevel then the type average last",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef="agl">
    <windlevel altitude="10" speed="2" direction="3" standarddeviation="0.4"/>
  </wind>
  <windmodeltype>Average</windmodeltype>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=AGL(10.0,2.0,3.0,0.4)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind type multilevel then an average element last",
     .xml      = R"xml(
<conditions>
  <windmodeltype>MultiLevel</windmodeltype>
  <wind model="average"><speed>1</speed></wind>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(1.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average then multilevel elements",
     .xml      = R"xml(
<conditions>
  <wind model="average"><speed>1</speed></wind>
  <wind model="multilevel">
    <windlevel altitude="10" speed="2" direction="3" standarddeviation="0.4"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(1.0,1.5707963267948966,0.0) multi=MSL(10.0,2.0,3.0,0.4)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: second multilevel element drops the levels of the first",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef="agl">
    <windlevel altitude="10" speed="2" direction="3" standarddeviation="0.4"/>
    <windlevel altitude="20" speed="3" direction="3" standarddeviation="0.4"/>
  </wind>
  <wind model="multilevel">
    <windlevel altitude="30" speed="4" direction="1" standarddeviation="0.1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=AGL(30.0,4.0,1.0,0.1)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel element without levels",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel"/>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel altituderef padded",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef=" agl "/>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=AGL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel altituderef in capitals",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef="AGL"/>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Unknown wind altitude reference 'AGL', using MSL.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why      = "decision L9: an unknown altitude reference is mean sea level, with a warning"},
    {.name     = "s1: multilevel altituderef empty",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef=""/>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Unknown wind altitude reference '', using MSL.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why      = "decision L9: an unknown altitude reference is mean sea level, with a warning"},
    {.name     = "s1: multilevel altituderef msl after agl",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel" altituderef="agl"/>
  <wind model="multilevel" altituderef="msl"/>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level attribute forms",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude=" 5 " speed="1e1" direction="0x1p1" standarddeviation="0.5d" extra="x"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(5.0,10.0,2.0,0.5)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level with a negative speed and deviation",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="-5" speed="-2" direction="1" standarddeviation="-1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(-5.0,2.0,4.141592653589793,0.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level direction beyond a turn",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="2" direction="7" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(5.0,2.0,0.7168146928204138,1.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level with an empty attribute",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [empty String]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level with multiple points",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="1.2.3" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [multiple points]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level with the Inf spelling",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="2" direction="1" standarddeviation="Inf"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "Inf"]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level missing altitude and garbage speed",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel speed="abc" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why = "decision D9: the level is skipped at its first missing attribute, where OpenRocket "
            "dies of a NullPointerException before it looks at the speed"},
    {.name     = "s1: multilevel level garbage altitude and missing speed",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="abc" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "abc"]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level NaN altitude and garbage deviation",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="NaN" speed="1" direction="1" standarddeviation="x"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [For input string: "x"]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel level with a child",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="2" direction="1" standarddeviation="1"><x/></windlevel>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  THROWN java.lang.NullPointerException: Cannot invoke "String.trim()" because "in" is null
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  closed=conditions {model=multilevel} []
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL
)out",
     .why      = "decision D9: after the slip the level has no attributes and is skipped, where "
                 "OpenRocket dies of a NullPointerException"},
    {.name     = "s1: multilevel levels after a failure stay",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="5" speed="2" direction="1" standarddeviation="1"/>
    <windlevel altitude="5" speed="3" direction="1" standarddeviation="1"/>
    <windlevel altitude="6" speed="3" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  FAILED INVALID_ARGUMENT [Wind level already exists for altitude: 5.0]
  closed=(not closed)
  wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL(5.0,2.0,1.0,1.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: multilevel levels at plus and minus zero",
     .xml      = R"xml(
<conditions>
  <wind model="multilevel">
    <windlevel altitude="0" speed="2" direction="1" standarddeviation="1"/>
    <windlevel altitude="-0.0" speed="3" direction="1" standarddeviation="1"/>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=MULTI_LEVEL avg=(0.0,1.5707963267948966,0.0) multi=MSL(-0.0,3.0,1.0,1.0)(0.0,2.0,1.0,1.0)
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: windlevel in an element without a model",
     .xml      = R"xml(
<conditions>
  <wind>
    <windlevel altitude="5" speed="2" direction="1" standarddeviation="1"/>
    <speed>3</speed>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown wind model type 'null', using default.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: wind average speed that takes an infinite deviation along",
     .xml      = R"xml(
<conditions>
  <wind model="average">
    <speed>0.001</speed>
    <standarddeviation>1e306</standarddeviation>
    <speed>2</speed>
    <direction>1</direction>
  </wind>
</conditions>
)xml",
     .java     = R"out(
  wind=AVERAGE avg=(2.0,1.0,Infinity) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Invalid parameter encountered, ignoring.
  wind=AVERAGE avg=(0.001,1.0,1.0E306) multi=MSL(0.0,0.0,0.0,0.0)
)out",
     .why      = "decision L3: the second speed times the turbulence intensity would be infinite"},
    // END GENERATED: wind
});

class WindElements : public ::testing::TestWithParam<ConditionsCase>
{ };

TEST_P(WindElements, LoadAsInOpenRocketButWhereStated)
{
    expectConditionsCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, WindElements, ::testing::ValuesIn(kWindCases),
                         conditionsCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// The wind line of @p options in the notation of the cases: the wind model in use, the average
/// model and the multi-level model with its levels.
[[nodiscard]] std::string windOf(const SimulationOptions& options)
{
    return describeConditions(options).at(1);
}

/// Options with a multi-level model that refers to the ground and has the levels 100 m and
/// 200 m, as a first <wind model="multilevel"> element leaves them.
[[nodiscard]] SimulationOptions optionsWithTwoLevels()
{
    SimulationOptions             options;
    MultiLevelPinkNoiseWindModel& multi = options.getMultiLevelWindModel();
    multi.clearLevels();
    multi.setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_TRUE(multi.addWindLevel(100, 1, 1, 0.1).has_value());
    EXPECT_TRUE(multi.addWindLevel(200, 2, 2, 0.2).has_value());
    return options;
}

constexpr std::string_view kTwoLevels =
    "wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=AGL(100.0,1.0,1.0,0.1)(200.0,2.0,2.0,0.2)";

TEST(WindHandler, MakingAMultiLevelHandlerClearsTheLevelsAtOnce)
{
    SimulationOptions options = optionsWithTwoLevels();
    ASSERT_EQ(windOf(options), kTwoLevels);
    const ChangeCounter changes(options.changed());
    WarningSet          warnings;

    const WindHandler handler(std::string("multilevel"), options, {}, warnings);

    // Without an altituderef attribute the reference stays what it was.
    EXPECT_EQ(windOf(options), "wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=AGL");
    EXPECT_EQ(handler.getModel(), std::optional<std::string>("multilevel"));
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(changes.count(), 1) << "the levels were cleared";
}

TEST(WindHandler, MakingAMultiLevelHandlerSetsTheAltitudeReference)
{
    SimulationOptions options = optionsWithTwoLevels();
    WarningSet        warnings;

    const WindHandler handler(std::string("multilevel"), options, {{"altituderef", " msl "}},
                              warnings);

    EXPECT_EQ(windOf(options), "wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL");
    EXPECT_TRUE(warnings.empty());
}

// Decision L9: OpenRocket stores null for a reference it does not know.
TEST(WindHandler, AnUnknownAltitudeReferenceIsMeanSeaLevelWithAWarning)
{
    SimulationOptions options = optionsWithTwoLevels();
    WarningSet        warnings;

    const WindHandler handler(std::string("multilevel"), options, {{"altituderef", "Agl"}},
                              warnings);

    EXPECT_EQ(windOf(options), "wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL");
    EXPECT_EQ(warningTexts(warnings), Texts{"Unknown wind altitude reference 'Agl', using MSL."});
}

TEST(WindHandler, MakingAnyOtherHandlerLeavesTheOptionsAlone)
{
    SimulationOptions   options = optionsWithTwoLevels();
    const ChangeCounter changes(options.changed());
    WarningSet          warnings;
    const Attributes    attributes{{"altituderef", "msl"}, {"model", "average"}};

    const WindHandler average(std::string("average"), options, attributes, warnings);
    const WindHandler capitals(std::string("Multilevel"), options, attributes, warnings);
    const WindHandler none(std::nullopt, options, attributes, warnings);

    EXPECT_EQ(windOf(options), kTwoLevels);
    EXPECT_EQ(changes.count(), 0);
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(none.getModel(), std::nullopt);
}

TEST(WindHandler, EveryChildIsPlainText)
{
    SimulationOptions             options;
    WarningSet                    warnings;
    WindHandler                   handler(std::string("multilevel"), options, {}, warnings);
    const Result<ElementHandler*> opened = handler.openElement("windlevel", {}, warnings);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, &PlainTextHandler::instance());
    const Result<ElementHandler*> other = handler.openElement("bogus", {{"a", "1"}}, warnings);
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, &PlainTextHandler::instance());
    EXPECT_TRUE(warnings.empty());
}

/// What closing a <windlevel> with @p attributes on a multi-level handler gives: "ok", or the
/// code and the message of the failure; then the warnings and the levels the options then have.
[[nodiscard]] std::string closeLevel(const Attributes& attributes)
{
    SimulationOptions  options;
    WarningSet         warnings;
    WindHandler        handler(std::string("multilevel"), options, {}, warnings);
    const Result<void> closed = handler.closeElement("windlevel", attributes, "", warnings);
    const std::string  result =
        closed.has_value()
            ? "ok"
            : std::format("{} [{}]", toString(closed.error().code), closed.error().message);
    return std::format("{} | {} | {}", result,
                       QtRocket::Strings::join("; ", warningTexts(warnings)),
                       QtRocket::Test::describeLevels(options.getMultiLevelWindModel()));
}

TEST(WindHandler, AWindLevelIsAddedFromItsFourAttributes)
{
    EXPECT_EQ(closeLevel({{"altitude", "100"},
                          {"speed", "5"},
                          {"direction", "1"},
                          {"standarddeviation", "0.5"}}),
              "ok |  | [100, 5, 1, 0.5]");
    // The text of the element and other attributes do not matter.
    EXPECT_EQ(closeLevel({{"altitude", " 1e2 "},
                          {"speed", "0x1p2"},
                          {"direction", "1d"},
                          {"standarddeviation", ".5"},
                          {"extra", "x"}}),
              "ok |  | [100, 4, 1, 0.5]");
}

// Java: Double.parseDouble() of the attribute, whose NumberFormatException ends the load.
TEST(WindHandler, AWindLevelAttributeThatIsNoNumberFailsTheLoad)
{
    EXPECT_EQ(closeLevel({{"altitude", "high"},
                          {"speed", "5"},
                          {"direction", "1"},
                          {"standarddeviation", "0.5"}}),
              R"(INVALID_ARGUMENT [For input string: "high"] |  | )");
    EXPECT_EQ(
        closeLevel(
            {{"altitude", "100"}, {"speed", "5"}, {"direction", "1"}, {"standarddeviation", ""}}),
        "INVALID_ARGUMENT [empty String] |  | ");
    EXPECT_EQ(closeLevel({{"altitude", "100"},
                          {"speed", "5"},
                          {"direction", "1.2.3"},
                          {"standarddeviation", "0.5"}}),
              "INVALID_ARGUMENT [multiple points] |  | ");
    EXPECT_EQ(closeLevel({{"altitude", "100"},
                          {"speed", " Inf "},
                          {"direction", "1"},
                          {"standarddeviation", "0.5"}}),
              R"(INVALID_ARGUMENT [For input string: "Inf"] |  | )");
}

// Decision D9 for the missing attribute (Java: NullPointerException) and decision L3 for the
// number that is not finite (Java adds the level).
TEST(WindHandler, AWindLevelWithAMissingOrNonFiniteAttributeIsSkippedWithAWarning)
{
    const std::string_view skipped = "ok | Invalid parameter encountered, ignoring. | ";
    EXPECT_EQ(closeLevel({}), skipped);
    EXPECT_EQ(closeLevel({{"altitude", "100"}, {"speed", "5"}, {"direction", "1"}}), skipped);
    EXPECT_EQ(closeLevel({{"speed", "5"}, {"direction", "1"}, {"standarddeviation", "0.5"}}),
              skipped);
    EXPECT_EQ(closeLevel({{"altitude", "100"},
                          {"speed", "5"},
                          {"direction", "1"},
                          {"standarddeviation", "NaN"}}),
              skipped);
    EXPECT_EQ(closeLevel({{"altitude", "-Infinity"},
                          {"speed", "5"},
                          {"direction", "1"},
                          {"standarddeviation", "0.5"}}),
              skipped);
    EXPECT_EQ(closeLevel({{"altitude", "100"},
                          {"speed", "1e999"},
                          {"direction", "1"},
                          {"standarddeviation", "0.5"}}),
              skipped);
}

// The attributes are looked at in Java's order, and the first that is missing or is no number
// decides.
TEST(WindHandler, TheFirstBadAttributeOfAWindLevelDecides)
{
    // The altitude is missing: skipped, and the speed is not looked at.
    EXPECT_EQ(closeLevel({{"speed", "fast"}, {"direction", "1"}, {"standarddeviation", "0.5"}}),
              "ok | Invalid parameter encountered, ignoring. | ");
    // The altitude is no number: the load fails, although the speed is missing.
    EXPECT_EQ(closeLevel({{"altitude", "high"}, {"direction", "1"}, {"standarddeviation", "0.5"}}),
              R"(INVALID_ARGUMENT [For input string: "high"] |  | )");
    // A NaN is a number: the deviation behind it still fails the load.
    EXPECT_EQ(closeLevel({{"altitude", "NaN"},
                          {"speed", "5"},
                          {"direction", "1"},
                          {"standarddeviation", "wide"}}),
              R"(INVALID_ARGUMENT [For input string: "wide"] |  | )");
}

/// The failure of adding the levels @p altitudes, each with a wind of 1 m/s, in that order, and
/// the levels the options have afterwards.
[[nodiscard]] std::string addLevels(std::initializer_list<std::string_view> altitudes)
{
    SimulationOptions options;
    WarningSet        warnings;
    WindHandler       handler(std::string("multilevel"), options, {}, warnings);
    std::string       failures;
    for (const std::string_view altitude : altitudes)
    {
        const Result<void> closed = handler.closeElement("windlevel",
                                                         {{"altitude", std::string(altitude)},
                                                          {"speed", "1"},
                                                          {"direction", "1"},
                                                          {"standarddeviation", "0"}},
                                                         "", warnings);
        if (!closed.has_value())
        {
            failures +=
                std::format("{} [{}] ", toString(closed.error().code), closed.error().message);
        }
    }
    return failures + QtRocket::Test::describeLevels(options.getMultiLevelWindModel());
}

TEST(WindHandler, LevelsAreSortedAndASecondLevelAtAnAltitudeFailsTheLoad)
{
    EXPECT_EQ(addLevels({"300", "100", "200"}), "[100, 1, 1, 0][200, 1, 1, 0][300, 1, 1, 0]");
    // Java's message, with the altitude as Java prints a double.
    EXPECT_EQ(addLevels({"100", "200", "1e2"}),
              "INVALID_ARGUMENT [Wind level already exists for altitude: 100.0] "
              "[100, 1, 1, 0][200, 1, 1, 0]");
    // Zero and negative zero are two altitudes, as for Java's Double.compare.
    EXPECT_EQ(addLevels({"0", "-0.0"}), "[-0, 1, 1, 0][0, 1, 1, 0]");
}

/// The wind of options after the children @p children of a <wind model="average"> element
/// were closed, as element and text, and the warnings.
[[nodiscard]] std::string closeAverageChildren(
    std::initializer_list<std::pair<std::string_view, std::string_view>> children)
{
    SimulationOptions options;
    WarningSet        warnings;
    WindHandler       handler(std::string("average"), options, {}, warnings);
    for (const auto& [element, content] : children)
    {
        EXPECT_TRUE(handler.closeElement(element, {}, content, warnings).has_value());
    }
    return std::format("{} | {}", QtRocket::Strings::join("; ", warningTexts(warnings)),
                       windOf(options).substr(0, windOf(options).find(" multi=")));
}

TEST(WindHandler, TheAverageModelTakesSpeedDirectionAndDeviation)
{
    EXPECT_EQ(
        closeAverageChildren({{"speed", "3"}, {"direction", "2"}, {"standarddeviation", "0.5"}}),
        " | wind=AVERAGE avg=(3.0,2.0,0.5)");
    // A text that is no number and a NaN are passed over silently, as in OpenRocket.
    EXPECT_EQ(closeAverageChildren({{"speed", "3"},
                                    {"speed", "fast"},
                                    {"direction", "NaN"},
                                    {"standarddeviation", ""},
                                    {"gust", "9"}}),
              " | wind=AVERAGE avg=(3.0,1.5707963267948966,0.0)");
    // Not OpenRocket's, which stores an infinity: decision U3.
    EXPECT_EQ(closeAverageChildren({{"speed", "3"},
                                    {"speed", "Infinity"},
                                    {"direction", "-Infinity"},
                                    {"standarddeviation", "1e999"}}),
              "Invalid parameter encountered, ignoring. | "
              "wind=AVERAGE avg=(3.0,1.5707963267948966,0.0)");
}

/// The average wind model of @p options: "speed direction deviation", as Java prints doubles.
[[nodiscard]] std::string averageOf(const SimulationOptions& options)
{
    const PinkNoiseWindModel& average = options.getAverageWindModel();
    return std::format("{} {} {}", javaNumber(average.getAverage()),
                       javaNumber(average.getDirection()),
                       javaNumber(average.getStandardDeviation()));
}

// Decision U3 for what the model computes: both numbers are finite, their product is not.
// (OpenRocket stores the infinite standard deviation: CondProbe, "s1: wind average speed that
// takes an infinite deviation along".)
TEST(WindHandler, SetAverageWindRefusesAFiniteNumberThatMakesTheModelNotFinite)
{
    SimulationOptions options;
    EXPECT_TRUE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, 0.001));
    EXPECT_TRUE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setStandardDeviation, 1e306));
    ASSERT_EQ(averageOf(options), "0.001 1.5707963267948966 1.0E306");
    const ChangeCounter changes(options.changed());

    // The speed takes the turbulence intensity along, which is beyond the range of a double.
    EXPECT_FALSE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, 2.0));
    EXPECT_FALSE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, -2.0));

    EXPECT_EQ(averageOf(options), "0.001 1.5707963267948966 1.0E306") << "nothing changed";
    EXPECT_EQ(changes.count(), 0) << "and nothing was announced";

    // What stays finite is applied, with its change events.
    EXPECT_TRUE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setTurbulenceIntensity, 0.5));
    EXPECT_TRUE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setDirection, 7.0));
    EXPECT_EQ(averageOf(options), "0.001 0.7168146928204138 5.0E-4");
    EXPECT_EQ(changes.count(), 2);

    // A turbulence intensity is multiplied by the speed.
    EXPECT_TRUE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, 1e200));
    ASSERT_EQ(averageOf(options), "1.0E200 0.7168146928204138 5.0E199");
    EXPECT_FALSE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setTurbulenceIntensity, 1e200));
    EXPECT_EQ(averageOf(options), "1.0E200 0.7168146928204138 5.0E199");
}

TEST(WindHandler, SetAverageWindRefusesANumberThatIsNotFinite)
{
    SimulationOptions   options;
    const ChangeCounter changes(options.changed());
    EXPECT_FALSE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, kInfinity));
    EXPECT_FALSE(WindHandler::setAverageWind(options, &PinkNoiseWindModel::setAverage, kNaN));
    EXPECT_FALSE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setDirection, -kInfinity));
    EXPECT_FALSE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setStandardDeviation, kNaN));
    EXPECT_FALSE(
        WindHandler::setAverageWind(options, &PinkNoiseWindModel::setStandardDeviation, kInfinity));
    EXPECT_EQ(averageOf(options), "0.0 1.5707963267948966 0.0");
    EXPECT_EQ(changes.count(), 0);
}

TEST(WindHandler, AHandlerOfAnotherModelIgnoresItsChildren)
{
    SimulationOptions options = optionsWithTwoLevels();
    WarningSet        warnings;
    WindHandler       handler(std::string("gusty"), options, {}, warnings);
    const Attributes  level{
        {"altitude", "5"}, {"speed", "x"}, {"direction", "1"}, {"standarddeviation", "1"}};

    EXPECT_TRUE(handler.closeElement("speed", {}, "3", warnings).has_value());
    EXPECT_TRUE(handler.closeElement("windlevel", level, "", warnings).has_value());

    EXPECT_EQ(windOf(options), kTwoLevels);
    EXPECT_TRUE(warnings.empty());
}

/// The wind model in use after a handler of @p model stored its settings into options that
/// used @p before, and the warnings.
[[nodiscard]] std::string storedOver(WindModelType before, std::optional<std::string> model)
{
    SimulationOptions options;
    options.setWindModelType(before);
    WarningSet        warnings;
    const WindHandler handler(std::move(model), options, {}, warnings);
    handler.storeSettings(options, warnings);
    return std::format("{} {}", windModelTypeName(options.getWindModelType()),
                       QtRocket::Strings::join("; ", warningTexts(warnings)));
}

TEST(WindHandler, StoresTheWindModelInUse)
{
    EXPECT_EQ(storedOver(WindModelType::MULTI_LEVEL, "average"), "AVERAGE ");
    EXPECT_EQ(storedOver(WindModelType::AVERAGE, "multilevel"), "MULTI_LEVEL ");
    // Anything else leaves the model in use and says so.
    EXPECT_EQ(storedOver(WindModelType::MULTI_LEVEL, "Average"),
              "MULTI_LEVEL Unknown wind model type 'Average', using default.");
    EXPECT_EQ(storedOver(WindModelType::AVERAGE, ""),
              "AVERAGE Unknown wind model type '', using default.");
    // Java prints the null of a missing attribute.
    EXPECT_EQ(storedOver(WindModelType::MULTI_LEVEL, std::nullopt),
              "MULTI_LEVEL Unknown wind model type 'null', using default.");
}

}  // namespace
