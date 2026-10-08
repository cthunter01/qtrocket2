#include "QtRocket/file/openrocket/CsvLookupHandler.h"

#include <array>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationOptionsSupport.h"

// The cases are <conditions> elements with lookup elements, run through
// SimulationConditionsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe CondProbe of run 9b, part S1; see
// ConditionsTestSupport.h), but where a case states that QtRocket differs. "{DIR}" is a
// directory with the files of writeLookupFiles(): tables/drag.csv, tables/stability.csv and
// tables/nan.csv.

namespace
{

using QtRocket::CsvLookupHandler;
using QtRocket::ElementHandler;
using QtRocket::MachAoALookup;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::SimulationConditionsHandler;
using QtRocket::SimulationOptions;
using QtRocket::WarningSet;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::ConditionsCase;
using QtRocket::Test::conditionsCaseTestName;
using QtRocket::Test::ConditionsFixture;
using QtRocket::Test::describeConditions;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::neutralPaths;
using QtRocket::Test::runHandler;
using QtRocket::Test::TempDir;
using QtRocket::Test::writeLookupFiles;

using Rows  = std::vector<std::string>;
using Texts = std::vector<std::string>;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

constexpr auto kLookupCases = std::to_array<ConditionsCase>({
    // BEGIN GENERATED: lookup
    {.name     = "cond: draglookup embedded rows",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <draglookup file="does/not/exist.csv">
    <row>Mach,AoA,Cd</row>
    <row>0.30,0,0.35</row>
    <row>0.60,0,0.40</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach;AoA;Cn;Cm;Cp</row>
    <row>0.30;0;0.1;0.2;0.3</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  drag=path:{CWD}/does/not/exist.csv table:[aoa=true mach=0.3..0.6 cd=0.375] rows:[Mach,AoA,Cd|0.30,0,0.35|0.60,0,0.40]
  stability=path:null table:[aoa=true mach=0.3..0.3 cn=0.1 cm=0.2 cp=0.3] rows:[Mach;AoA;Cn;Cm;Cp|0.30;0;0.1;0.2;0.3]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: draglookup bad rows and missing file",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <draglookup file="does/not/exist.csv">
    <row>Mach,AoA</row>
    <row>0.30,0</row>
  </draglookup>
  <stabilitylookup file="does/not/exist2.csv"/>
  <draglookupcsv>does/not/exist3.csv</draglookupcsv>
  <stabilitylookupcsv>does/not/exist4.csv</stabilitylookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Lookup table header missing required column 'cd'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load draglookup from file 'does/not/exist.csv': Failed to read lookup table from {CWD}/does/not/exist.csv
  W[Other,NORMAL] Failed to load stabilitylookup from file 'does/not/exist2.csv': Failed to read lookup table from {CWD}/does/not/exist2.csv
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: legacy draglookupcsv only",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <draglookupcsv>does/not/exist3.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load drag lookup CSV 'does/not/exist3.csv', ignoring. Reason: Failed to read lookup table from {CWD}/does/not/exist3.csv
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows with a comma",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <row>1,2.0</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
    <row>1,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:null table:[aoa=false mach=0.0..1.0 cd=2.0] rows:[Mach,Cd|0,2.0|1,2.0]
  stability=path:null table:[aoa=false mach=0.0..1.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0|1,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows with a tab and a semicolon",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach&#9;Cd</row>
    <row>0&#9;0.5</row>
    <row>2&#9;1.5</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach;AoA;Cn;Cm;Cp</row>
    <row>0;0;1;2;3</row>
    <row>0;10;3;4;5</row>
    <row>1;0;5;6;7</row>
    <row>1;10;7;8;9</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:null table:[aoa=false mach=0.0..2.0 cd=1.0] rows:[Mach<TAB>Cd|0<TAB>0.5|2<TAB>1.5]
  stability=path:null table:[aoa=true mach=0.0..1.0 cn=3.0 cm=4.0 cp=5.0] rows:[Mach;AoA;Cn;Cm;Cp|0;0;1;2;3|0;10;3;4;5|1;0;5;6;7|1;10;7;8;9]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup separator by the first row only",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row># drag, measured; twice</row>
    <row>Mach;Cd</row>
    <row>0;0.5</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Lookup table header must contain a 'mach' column. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup separator tie and none",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach;Cd,x</row>
    <row>0;0.5,1</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Lookup table header must contain a 'mach' column. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Lookup table header missing required column 'cn'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] stabilitylookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows padded, blank and commented",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>  Mach , Cd  </row>
    <row>   </row>
    <row></row>
    <row># a comment</row>
    <row> 0 , 0.25 </row>
    <row>1,0.75</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:null table:[aoa=false mach=0.0..1.0 cd=0.5] rows:[Mach , Cd|# a comment|0 , 0.25|1,0.75]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup header without rows",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: No lookup data added
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup element without rows and file",
     .xml      = R"xml(
<conditions>
  <draglookup/>
  <stabilitylookup>   </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows that do not parse and no file",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,abc</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach,Cn,Cm</row>
    <row>0,1,2</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'abc' in column 'cd'
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Lookup table header missing required column 'cp'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] stabilitylookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows with numbers that are not finite",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,NaN</row>
    <row>1,0.5</row>
  </draglookup>
  <stabilitylookup>
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,2,3</row>
    <row>Infinity,1,-Infinity,3</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:null table:[aoa=false mach=0.0..1.0 cd=NaN] rows:[Mach,Cd|0,NaN|1,0.5]
  stability=path:null table:[aoa=false mach=0.0..Infinity cn=1.0 cm=-Infinity cp=3.0] rows:[Mach,Cn,Cm,Cp|0,1,2,3|Infinity,1,-Infinity,3]
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'NaN' in column 'cd'
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Illegal numeric value 'Infinity' in column 'mach'
  W[Other,NORMAL] stabilitylookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .why      = "decision U3: a table with a number that is not finite is not taken"},
    {.name     = "s1: lookup with a child that is no row",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <column name="x"/>
    <row>1,2.0</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  closed=conditions {file={DIR}/tables/drag.csv} []
  drag=path:null table:[aoa=false mach=0.0..1.0 cd=2.0] rows:[Mach,Cd|0,2.0|1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup row with a child",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,2<x/>.5</row>
    <row>1,2.0</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Row missing value for column 'cd'
  W[Other,NORMAL] draglookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup from a file",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/drag.csv"/>
  <stabilitylookup file=" {DIR}/tables/stability.csv "/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows preferred to the file",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <row>1,2.0</row>
  </draglookup>
  <stabilitylookup file="{DIR}/tables/missing.csv">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=2.0] rows:[Mach,Cd|0,2.0|1,2.0]
  stability=path:{DIR}/tables/missing.csv table:[aoa=false mach=0.0..0.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows that do not parse fall back to the file",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,abc</row>
  </draglookup>
  <stabilitylookup file="{DIR}/tables/stability.csv">
    <row>Mach,Cn</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'abc' in column 'cd'
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Lookup table header missing required column 'cm'. Make sure the column is included and you are using the correct field separator.
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file that does not parse",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/stability.csv"/>
  <stabilitylookup file="{DIR}/tables/drag.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file '{DIR}/tables/stability.csv': Lookup table header missing required column 'cd'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load stabilitylookup from file '{DIR}/tables/drag.csv': Lookup table header missing required column 'cn'. Make sure the column is included and you are using the correct field separator.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file with numbers that are not finite",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/nan.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/nan.csv table:[aoa=false mach=0.0..1.0 cd=NaN] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to load draglookup from file '{DIR}/tables/nan.csv': Illegal numeric value 'NaN' in column 'cd'
)out",
     .why      = "decision U3: a table with a number that is not finite is not taken"},
    {.name     = "s1: lookup file that is a directory and an empty file attribute",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables"/>
  <stabilitylookup file="   ">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file '{DIR}/tables': Failed to read lookup table from {DIR}/tables
  stability=path:null table:[aoa=false mach=0.0..0.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file spelled with doubled separators and dots",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}//tables/./sub/..//missing.csv"/>
  <stabilitylookup file="{DIR}/tables/../tables//stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file '{DIR}/tables/./sub/../missing.csv': Failed to read lookup table from {DIR}/tables/missing.csv
  stability=path:{DIR}/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: second lookup element from a file keeps the rows of the first",
     .xml      = R"xml(
<conditions>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,2.0</row>
  </draglookup>
  <draglookup file="{DIR}/tables/drag.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:[Mach,Cd|0,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: second lookup element that fails keeps the first",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
  </draglookup>
  <draglookup file="{DIR}/tables/missing.csv">
    <row>Mach</row>
  </draglookup>
  <draglookup/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Lookup table header missing required column 'cd'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load draglookup from file '{DIR}/tables/missing.csv': Failed to read lookup table from {DIR}/tables/missing.csv
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..0.0 cd=2.0] rows:[Mach,Cd|0,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup elements from files",
     .xml      = R"xml(
<conditions>
  <draglookupcsv> {DIR}/tables/drag.csv </draglookupcsv>
  <stabilitylookupcsv>{DIR}/tables/stability.csv</stabilitylookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup elements empty and failing",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>   </draglookupcsv>
  <stabilitylookupcsv>{DIR}/tables/drag.csv</stabilitylookupcsv>
  <draglookupcsv>{DIR}/tables/missing.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load stability lookup CSV '{DIR}/tables/drag.csv', ignoring. Reason: Lookup table header missing required column 'cn'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load drag lookup CSV '{DIR}/tables/missing.csv', ignoring. Reason: Failed to read lookup table from {DIR}/tables/missing.csv
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup element before the lookup element",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>{DIR}/tables/drag.csv</draglookupcsv>
  <draglookup>
    <row>Mach,Cd</row>
    <row>0,2.0</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:null table:[aoa=false mach=0.0..0.0 cd=2.0] rows:[Mach,Cd|0,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup element after the lookup element is ignored",
     .xml      = R"xml(
<conditions>
  <draglookup/>
  <draglookupcsv>{DIR}/tables/drag.csv</draglookupcsv>
  <stabilitylookup>
    <row>garbage</row>
  </stabilitylookup>
  <stabilitylookupcsv>{DIR}/tables/stability.csv</stabilitylookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Lookup table header must contain a 'mach' column. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] stabilitylookup has embedded data but no file reference. Data may not persist correctly.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup file with numbers that are not finite",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>{DIR}/tables/nan.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/tables/nan.csv table:[aoa=false mach=0.0..1.0 cd=NaN] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to load drag lookup CSV '{DIR}/tables/nan.csv', ignoring. Reason: Illegal numeric value 'NaN' in column 'cd'
)out",
     .why      = "decision U3: a table with a number that is not finite is not taken"},
    {.name     = "s1: copied lookups of value 2 as the saver writes them",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <draglookup file="{DIR}/missing-beta-test-lookup.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <row>1,2.0</row>
  </draglookup>
  <stabilitylookup file="{DIR}/missing-beta-test-lookup.csv">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
    <row>1,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  drag=path:{DIR}/missing-beta-test-lookup.csv table:[aoa=false mach=0.0..1.0 cd=2.0] rows:[Mach,Cd|0,2.0|1,2.0]
  stability=path:{DIR}/missing-beta-test-lookup.csv table:[aoa=false mach=0.0..1.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0|1,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    // END GENERATED: lookup
});

class LookupElements : public ::testing::TestWithParam<ConditionsCase>
{ };

TEST_P(LookupElements, LoadAsInOpenRocketButWhereStated)
{
    expectConditionsCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, LookupElements, ::testing::ValuesIn(kLookupCases),
                         conditionsCaseTestName);

// ---- OpenRocket's SimulationLookupCopyTest, the halves that load ------------------------------
//
// The two tests save a simulation whose lookups were copied from other options and load it
// again (roundTrip()). The halves without a file are in SimulationOptionsTests.cpp (tier 8);
// here is the loading, with the <conditions> element OpenRocketSaver writes for such options
// written out by hand: the table's rows embedded, and the name of a CSV file that does not
// exist. The whole round trip waits for the saver (tier 10).

/// SimulationLookupCopyTest.CSV
constexpr std::string_view kMissingCsv = "missing-beta-test-lookup.csv";

/// The <conditions> element of options whose drag coefficient and centre of pressure are
/// @p value everywhere (SimulationLookupCopyTest.options(value), as the saver writes them);
/// @p value as Java's string concatenation gives the double ("2.0").
[[nodiscard]] std::string lookupConditions(std::string_view value)
{
    return std::format(R"(<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <draglookup file="{0}">
    <row>Mach,Cd</row>
    <row>0,{1}</row>
    <row>1,{1}</row>
  </draglookup>
  <stabilitylookup file="{0}">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,{1}</row>
    <row>1,1,1,{1}</row>
  </stabilitylookup>
</conditions>)",
                       kMissingCsv, value);
}

/// SimulationLookupCopyTest.assertLookups()
void expectLookups(const SimulationOptions& options, double value)
{
    ASSERT_TRUE(options.hasDragLookup());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(0.5, 0, "cd"), value);
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cp"), value);
}

/// Options with lookups of @p value that have stale rows (SimulationLookupCopyTest.options()).
[[nodiscard]] SimulationOptions lookupOptions(std::string_view value)
{
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());
    EXPECT_TRUE(runHandler(handler, lookupConditions(value)).result.has_value());
    // A copy keeps no pointer into the fixture but the preference store's, which only
    // setSimulationStepperMethodChoice() uses.
    SimulationOptions options;
    options.copyConditionsFrom(handler.getConditions());
    return options;
}

// SimulationLookupCopyTest.testCopiedLookupsSurviveSaveAndReloadWithoutExternalFiles, the load
TEST(SimulationLookupCopyLoad, CopiedLookupsSurviveTheReloadWithoutExternalFiles)
{
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());

    const HandlerRun run = runHandler(handler, lookupConditions("2.0"));

    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{}) << "the file is named and never read";
    const SimulationOptions& loaded = handler.getConditions();
    expectLookups(loaded, 2);
    // The rows are the ones the source options had (SimulationLookupCopyTest.options(2)).
    EXPECT_EQ(loaded.getDragLookupCsvRows(), (Rows{"Mach,Cd", "0,2.0", "1,2.0"}));
    EXPECT_EQ(loaded.getStabilityLookupCsvRows(),
              (Rows{"Mach,Cn,Cm,Cp", "0,1,1,2.0", "1,1,1,2.0"}));
    // The file is kept by its name, resolved against the current directory, and is not there.
    const std::filesystem::path csv = QtRocket::absolutePath(QtRocket::pathFromUtf8(kMissingCsv));
    EXPECT_EQ(loaded.getDragLookupCsvPath(), csv);
    EXPECT_EQ(loaded.getStabilityLookupCsvPath(), csv);
    std::error_code noFile;
    EXPECT_FALSE(std::filesystem::exists(csv, noFile));
}

// SimulationLookupCopyTest.testCopiedLookupsReplaceStaleDestinationRows, the load: the document
// of the copied options holds the rows of the copy, and loading it gives their table, also when
// what is loaded then replaces lookups of another value.
TEST(SimulationLookupCopyLoad, CopiedLookupsReplaceStaleDestinationRows)
{
    SimulationOptions target = lookupOptions("1.0");
    expectLookups(target, 1);

    target.copyConditionsFrom(lookupOptions("2.0"));

    expectLookups(target, 2);
    EXPECT_EQ(target.getDragLookupCsvRows(), (Rows{"Mach,Cd", "0,2.0", "1,2.0"}));
    EXPECT_EQ(target.getStabilityLookupCsvRows(),
              (Rows{"Mach,Cn,Cm,Cp", "0,1,1,2.0", "1,1,1,2.0"}));
}

// ---- the handler by itself ------------------------------------------------------------------

/// The separator CsvLookupHandler tells from @p firstRow, as text: "comma", "semicolon", "tab".
[[nodiscard]] std::string_view separatorOf(std::string_view firstRow)
{
    const Rows rows{std::string(firstRow), "a;b;c;d;e"};
    switch (CsvLookupHandler::detectSeparator(rows))
    {
        case ',':
            return "comma";
        case ';':
            return "semicolon";
        case '\t':
            return "tab";
        default:
            return "?";
    }
}

// CsvLookupHandler.detectSeparator(): the most frequent of the three in the first row.
TEST(CsvLookupHandler, TellsTheSeparatorFromTheFirstRow)
{
    EXPECT_EQ(separatorOf("Mach,AoA,Cd"), "comma");
    EXPECT_EQ(separatorOf("Mach;AoA;Cd"), "semicolon");
    EXPECT_EQ(separatorOf("Mach\tAoA\tCd"), "tab");
    EXPECT_EQ(separatorOf("Mach;AoA;Cd,x"), "semicolon");
    EXPECT_EQ(separatorOf("Mach\tAoA\tCd\tx;y;z,w"), "tab");
    // A tie goes to the comma first and to the semicolon before the tab.
    EXPECT_EQ(separatorOf("a,b;c"), "comma");
    EXPECT_EQ(separatorOf("a,b\tc"), "comma");
    EXPECT_EQ(separatorOf("a;b\tc"), "semicolon");
    EXPECT_EQ(separatorOf("a,b;c\td"), "comma");
    // None of them, a blank and other separators: the comma.
    EXPECT_EQ(separatorOf("Mach"), "comma");
    EXPECT_EQ(separatorOf("Mach AoA Cd"), "comma");
    EXPECT_EQ(separatorOf("Mach|AoA|Cd"), "comma");
    EXPECT_EQ(CsvLookupHandler::detectSeparator({}), ',');
}

/// CsvLookupHandler::nonFiniteReason() of a drag table with angles of attack whose one row is
/// @p mach, @p aoa and @p cd, or "finite".
[[nodiscard]] std::string reasonOfRow(double mach, double aoa, double cd)
{
    const Result<MachAoALookup> table =
        MachAoALookup::dragBuilder().addDragData(0.1, 0.0, 0.3).addDragData(mach, aoa, cd).build();
    if (!table.has_value())
    {
        return "no table: " + table.error().message;
    }
    return CsvLookupHandler::nonFiniteReason(*table).value_or("finite");
}

// The text of the CSV reader for a field that is no number, with the number as Java prints it.
TEST(CsvLookupHandler, NamesTheFirstNumberOfATableThatIsNotFinite)
{
    EXPECT_EQ(reasonOfRow(0.5, 2.0, 0.4), "finite");
    EXPECT_EQ(reasonOfRow(0.5, 2.0, kNaN), "Illegal numeric value 'NaN' in column 'cd'");
    EXPECT_EQ(reasonOfRow(0.5, 2.0, -kInfinity),
              "Illegal numeric value '-Infinity' in column 'cd'");
    EXPECT_EQ(reasonOfRow(0.5, kInfinity, 0.4), "Illegal numeric value 'Infinity' in column 'aoa'");
    EXPECT_EQ(reasonOfRow(kNaN, 2.0, 0.4), "Illegal numeric value 'NaN' in column 'mach'");
    // The Mach number of a row comes before its angle and its value.
    EXPECT_EQ(reasonOfRow(kInfinity, kNaN, kNaN),
              "Illegal numeric value 'Infinity' in column 'mach'");
}

/// What CsvLookupHandler::loadFromFile() of @p file under @p directory gives for the drag table
/// of options that hold the rows of an earlier element: "ok" or the failure, then the drag line
/// of the options in the notation of the cases.
[[nodiscard]] std::string loadDragFile(const TempDir& directory, std::string_view file)
{
    SimulationOptions options;
    options.setDragLookup(std::nullopt, nullptr, Rows{"kept"});
    const Result<void> loaded =
        CsvLookupHandler::loadFromFile(options, directory.resolve(file), true);
    const std::string result =
        loaded.has_value()
            ? "ok"
            : std::format("{} [{}]", toString(loaded.error().code), loaded.error().message);
    return neutralPaths(std::format("{} | {}", result, describeConditions(options).at(8)),
                        directory.path());
}

TEST(CsvLookupHandler, LoadsATableFromAFileAndKeepsTheRowsOfTheOptions)
{
    const TempDir directory;
    writeLookupFiles(directory);
    EXPECT_EQ(loadDragFile(directory, "tables/drag.csv"),
              "ok | drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] "
              "rows:[kept]");
    // The file is stored as an absolute path without "." and "..".
    EXPECT_EQ(loadDragFile(directory, "tables/./x/../drag.csv"),
              "ok | drag=path:{DIR}/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] "
              "rows:[kept]");
}

TEST(CsvLookupHandler, AFileThatFailsChangesNothing)
{
    const TempDir directory;
    writeLookupFiles(directory);
    const std::string_view unchanged = " | drag=path:null table:null rows:[kept]";
    EXPECT_EQ(
        loadDragFile(directory, "tables/missing.csv"),
        "IO [Failed to read lookup table from {DIR}/tables/missing.csv]" + std::string(unchanged));
    EXPECT_EQ(loadDragFile(directory, "tables/stability.csv"),
              "PARSE [Lookup table header missing required column 'cd'. Make sure the column is "
              "included and you are using the correct field separator.]" +
                  std::string(unchanged));
    // Not OpenRocket's, which takes the table: decision U3.
    EXPECT_EQ(loadDragFile(directory, "tables/nan.csv"),
              "PARSE [Illegal numeric value 'NaN' in column 'cd']" + std::string(unchanged));
}

TEST(CsvLookupHandler, LoadsTheStabilityTableWhenItIsNotTheDragTable)
{
    const TempDir directory;
    writeLookupFiles(directory);
    SimulationOptions   options;
    const ChangeCounter changes(options.changed());

    const Result<void> loaded =
        CsvLookupHandler::loadFromFile(options, directory.resolve("tables/stability.csv"), false);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(options.hasDragLookup());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(1.0, 0, "cp"), 4.0);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);
    EXPECT_EQ(changes.count(), 1);
}

/// Whether this machine has the device @p device (a POSIX system).
[[nodiscard]] bool hasDevice(const std::filesystem::path& device)
{
    std::error_code error;
    return std::filesystem::is_character_file(device, error);
}

// Hostile input: a design can name any file as its lookup table. A device is not opened, where
// OpenRocket would read it without end.
TEST(CsvLookupHandler, ADeviceNamedAsTheFileIsNotRead)
{
    if (!hasDevice("/dev/zero"))
    {
        GTEST_SKIP() << "no /dev/zero here";
    }
    ConditionsFixture           fixture;
    SimulationConditionsHandler handler(fixture.context());

    const HandlerRun run = runHandler(handler,
                                      "<conditions><draglookup file='/dev/zero'/>"
                                      "<stabilitylookupcsv>/dev/zero</stabilitylookupcsv>"
                                      "</conditions>");

    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(),
              (Texts{"Failed to load draglookup from file '/dev/zero': Failed to read lookup "
                     "table from /dev/zero",
                     "Failed to load stability lookup CSV '/dev/zero', ignoring. Reason: Failed "
                     "to read lookup table from /dev/zero"}));
    EXPECT_FALSE(handler.getConditions().hasDragLookup());
    EXPECT_FALSE(handler.getConditions().hasStabilityLookup());
}

/// A handler of a drag table over @p options, with the rows @p rows closed on it.
[[nodiscard]] std::unique_ptr<CsvLookupHandler> handlerWithRows(SimulationOptions& options,
                                                                const Rows&        rows)
{
    auto       handler = std::make_unique<CsvLookupHandler>(options, Rows{"cd"}, true);
    WarningSet warnings;
    for (const std::string& row : rows)
    {
        EXPECT_TRUE(handler->closeElement("row", {}, row, warnings).has_value());
    }
    EXPECT_TRUE(warnings.empty());
    return handler;
}

TEST(CsvLookupHandler, OnlyARowIsAChild)
{
    SimulationOptions options;
    CsvLookupHandler  handler(options, Rows{"cd"}, true);
    WarningSet        warnings;

    const Result<ElementHandler*> row = handler.openElement("row", {{"a", "1"}}, warnings);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(*row, &PlainTextHandler::instance());
    // Any other child is ignored with what is in it, and without a warning.
    const Result<ElementHandler*> other = handler.openElement("Row", {}, warnings);
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, nullptr);
    EXPECT_TRUE(warnings.empty());
}

TEST(CsvLookupHandler, KeepsTheRowsTrimmedAndWithoutTheBlankOnes)
{
    SimulationOptions                       options;
    const std::unique_ptr<CsvLookupHandler> handler =
        handlerWithRows(options, {"  Mach , Cd \n", "", " \t ", "# note", "0,0.5"});
    EXPECT_EQ(handler->getRows(), (Rows{"Mach , Cd", "# note", "0,0.5"}));
    // Nothing is stored before the element ends.
    EXPECT_FALSE(options.hasDragLookup());
}

// endHandler() by itself: the table goes into the options with the rows, and the file of the
// element's own attribute, which need not exist.
TEST(CsvLookupHandler, StoresTheTableWhenTheElementEnds)
{
    SimulationOptions                       options;
    const std::unique_ptr<CsvLookupHandler> handler =
        handlerWithRows(options, {"Mach;Cd", "0;0.5", "2;1.5"});
    WarningSet          warnings;
    const ChangeCounter changes(options.changed());

    const Result<void> ended =
        handler->endHandler("draglookup", {{"file", " not-there.csv "}}, "text", warnings);

    ASSERT_TRUE(ended.has_value());
    EXPECT_TRUE(warnings.empty());
    ASSERT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(1.0, 0, "cd"), 1.0);
    EXPECT_EQ(options.getDragLookupCsvRows(), (Rows{"Mach;Cd", "0;0.5", "2;1.5"}));
    EXPECT_EQ(options.getDragLookupCsvPath(),
              QtRocket::absolutePath(QtRocket::pathFromUtf8("not-there.csv")));
    EXPECT_EQ(changes.count(), 1);
}

}  // namespace
