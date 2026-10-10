#include "QtRocket/file/openrocket/CsvLookupHandler.h"

#include <algorithm>
#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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
#include "QtRocket/util/Strings.h"
#include "TestTempDir.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationOptionsSupport.h"

// The cases are <conditions> elements with lookup elements, run through
// SimulationConditionsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe CondProbe of run 9b, part S1, run again for run
// 9d with the cases "u5: ..."; see ConditionsTestSupport.h), but where a case states that
// QtRocket differs. "{DIR}" is a directory with the files of writeLookupFiles(): the design's
// directory "{DIR}/design" with tables/drag.csv, tables/stability.csv and tables/nan.csv, and
// "{DIR}/outside" with tables no design may read. A relative name of a case is one of the
// design's directory on both sides: the probe ran with that directory as its working
// directory, which is what OpenRocket resolves a relative name against. A name with "{DIR}"
// is an absolute path.

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
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::describeConditions;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::genericUtf8;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::neutralPaths;
using QtRocket::Test::replaceAll;
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
  drag=path:{DIR}/design/does/not/exist.csv table:[aoa=true mach=0.3..0.6 cd=0.375] rows:[Mach,AoA,Cd|0.30,0,0.35|0.60,0,0.40]
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
  W[Other,NORMAL] Failed to load draglookup from file 'does/not/exist.csv': Failed to read lookup table from {DIR}/design/does/not/exist.csv
  W[Other,NORMAL] Failed to load stabilitylookup from file 'does/not/exist2.csv': Failed to read lookup table from {DIR}/design/does/not/exist2.csv
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
  W[Other,NORMAL] Failed to load drag lookup CSV 'does/not/exist3.csv', ignoring. Reason: Failed to read lookup table from {DIR}/design/does/not/exist3.csv
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
  <draglookup file="tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <column name="x"/>
    <row>1,2.0</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  closed=conditions {file=tables/drag.csv} []
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
  <draglookup file="tables/drag.csv"/>
  <stabilitylookup file=" tables/stability.csv "/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/design/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows preferred to the file",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
    <row>1,2.0</row>
  </draglookup>
  <stabilitylookup file="tables/missing.csv">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=2.0] rows:[Mach,Cd|0,2.0|1,2.0]
  stability=path:{DIR}/design/tables/missing.csv table:[aoa=false mach=0.0..0.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup rows that do not parse fall back to the file",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,abc</row>
  </draglookup>
  <stabilitylookup file="tables/stability.csv">
    <row>Mach,Cn</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'abc' in column 'cd'
  W[Other,NORMAL] Failed to parse embedded CSV data in stabilitylookup: Lookup table header missing required column 'cm'. Make sure the column is included and you are using the correct field separator.
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/design/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file that does not parse",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/stability.csv"/>
  <stabilitylookup file="tables/drag.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file 'tables/stability.csv': Lookup table header missing required column 'cd'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load stabilitylookup from file 'tables/drag.csv': Lookup table header missing required column 'cn'. Make sure the column is included and you are using the correct field separator.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file with numbers that are not finite",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/nan.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/nan.csv table:[aoa=false mach=0.0..1.0 cd=NaN] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to load draglookup from file 'tables/nan.csv': Illegal numeric value 'NaN' in column 'cd'
)out",
     .why      = "decision U3: a table with a number that is not finite is not taken"},
    {.name     = "s1: lookup file that is a directory and an empty file attribute",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables"/>
  <stabilitylookup file="   ">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file 'tables': Failed to read lookup table from {DIR}/design/tables
  stability=path:null table:[aoa=false mach=0.0..0.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: lookup file spelled with doubled separators and dots",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables//./sub/..//missing.csv"/>
  <stabilitylookup file="tables/../tables//stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file 'tables/./sub/../missing.csv': Failed to read lookup table from {DIR}/design/tables/missing.csv
  stability=path:{DIR}/design/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
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
  <draglookup file="tables/drag.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:[Mach,Cd|0,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: second lookup element that fails keeps the first",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
  </draglookup>
  <draglookup file="tables/missing.csv">
    <row>Mach</row>
  </draglookup>
  <draglookup/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Lookup table header missing required column 'cd'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load draglookup from file 'tables/missing.csv': Failed to read lookup table from {DIR}/design/tables/missing.csv
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..0.0 cd=2.0] rows:[Mach,Cd|0,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup elements from files",
     .xml      = R"xml(
<conditions>
  <draglookupcsv> tables/drag.csv </draglookupcsv>
  <stabilitylookupcsv>tables/stability.csv</stabilitylookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/design/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup elements empty and failing",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>   </draglookupcsv>
  <stabilitylookupcsv>tables/drag.csv</stabilitylookupcsv>
  <draglookupcsv>tables/missing.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load stability lookup CSV 'tables/drag.csv', ignoring. Reason: Lookup table header missing required column 'cn'. Make sure the column is included and you are using the correct field separator.
  W[Other,NORMAL] Failed to load drag lookup CSV 'tables/missing.csv', ignoring. Reason: Failed to read lookup table from {DIR}/design/tables/missing.csv
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: legacy lookup element before the lookup element",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>tables/drag.csv</draglookupcsv>
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
  <draglookupcsv>tables/drag.csv</draglookupcsv>
  <stabilitylookup>
    <row>garbage</row>
  </stabilitylookup>
  <stabilitylookupcsv>tables/stability.csv</stabilitylookupcsv>
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
  <draglookupcsv>tables/nan.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/nan.csv table:[aoa=false mach=0.0..1.0 cd=NaN] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to load drag lookup CSV 'tables/nan.csv', ignoring. Reason: Illegal numeric value 'NaN' in column 'cd'
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
    {.name     = "u5: lookup file by an absolute path",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/design/tables/drag.csv"/>
  <stabilitylookup file="{DIR}/outside/stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/outside/stability.csv table:[aoa=false mach=0.0..2.0 cn=9.0 cm=9.0 cp=9.0] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file '{DIR}/design/tables/drag.csv' of element 'draglookup' was not read: a design may name such a file only by a relative path inside its own directory.
  W[Other,NORMAL] The lookup table file '{DIR}/outside/stability.csv' of element 'stabilitylookup' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why =
         "decision U5: a design reads a lookup file only by a relative name inside its own "
         "directory; an absolute path is refused also when it points into the design's directory"},
    {.name     = "u5: lookup file through two dots at the start",
     .xml      = R"xml(
<conditions>
  <draglookup file="../outside/drag.csv"/>
  <stabilitylookup file=".."/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load stabilitylookup from file '..': Failed to read lookup table from {DIR}
  drag=path:{DIR}/outside/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.9] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file '../outside/drag.csv' of element 'draglookup' was not read: a design may name such a file only by a relative path inside its own directory.
  W[Other,NORMAL] The lookup table file '..' of element 'stabilitylookup' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why      = "decision U5: a design reads a lookup file only by a relative name inside its own "
                 "directory"},
    {.name     = "u5: lookup file through two dots in the middle",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/../../outside/drag.csv"/>
  <stabilitylookup file="tables/../../design/tables/stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/outside/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.9] rows:null
  stability=path:{DIR}/design/tables/stability.csv table:[aoa=false mach=0.0..2.0 cn=2.0 cm=3.0 cp=4.0] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file 'tables/../../outside/drag.csv' of element 'draglookup' was not read: a design may name such a file only by a relative path inside its own directory.
  W[Other,NORMAL] The lookup table file 'tables/../../design/tables/stability.csv' of element 'stabilitylookup' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why =
         "decision U5: a design reads a lookup file only by a relative name inside its own "
         "directory; a name that leaves the directory is refused also when it comes back into it"},
    {.name     = "u5: lookup file through two dots after a subdirectory",
     .xml      = R"xml(
<conditions>
  <draglookup file="tables/sub/../drag.csv"/>
  <stabilitylookup file="tables/sub/../../../outside/stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
  stability=path:{DIR}/outside/stability.csv table:[aoa=false mach=0.0..2.0 cn=9.0 cm=9.0 cp=9.0] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file 'tables/sub/../../../outside/stability.csv' of element 'stabilitylookup' was not read: a design may name such a file only by a relative path inside its own directory.
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
)out",
     .why      = "decision U5: a design reads a lookup file only by a relative name inside its own "
                 "directory; two dots that stay inside the directory are read as in OpenRocket"},
    {.name     = "u5: lookup rows keep the name of a file outside",
     .xml      = R"xml(
<conditions>
  <draglookup file="{DIR}/outside/drag.csv">
    <row>Mach,Cd</row>
    <row>0,2.0</row>
  </draglookup>
  <stabilitylookup file="../outside/stability.csv">
    <row>Mach,Cn,Cm,Cp</row>
    <row>0,1,1,2.0</row>
  </stabilitylookup>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/outside/drag.csv table:[aoa=false mach=0.0..0.0 cd=2.0] rows:[Mach,Cd|0,2.0]
  stability=path:{DIR}/outside/stability.csv table:[aoa=false mach=0.0..0.0 cn=1.0 cm=1.0 cp=2.0] rows:[Mach,Cn,Cm,Cp|0,1,1,2.0]
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "u5: lookup rows that do not parse and a file outside",
     .xml      = R"xml(
<conditions>
  <draglookup file="../outside/drag.csv">
    <row>Mach,Cd</row>
    <row>0,abc</row>
  </draglookup>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'abc' in column 'cd'
  drag=path:{DIR}/outside/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.9] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Failed to parse embedded CSV data in draglookup: Illegal numeric value 'abc' in column 'cd'
  W[Other,NORMAL] The lookup table file '../outside/drag.csv' of element 'draglookup' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why      = "decision U5: a design reads a lookup file only by a relative name inside its own "
                 "directory"},
    {.name     = "u5: legacy lookup elements by an absolute path and through two dots",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>{DIR}/outside/drag.csv</draglookupcsv>
  <stabilitylookupcsv> ../outside/stability.csv </stabilitylookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/outside/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.9] rows:null
  stability=path:{DIR}/outside/stability.csv table:[aoa=false mach=0.0..2.0 cn=9.0 cm=9.0 cp=9.0] rows:null
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file '{DIR}/outside/drag.csv' of element 'draglookupcsv' was not read: a design may name such a file only by a relative path inside its own directory.
  W[Other,NORMAL] The lookup table file '../outside/stability.csv' of element 'stabilitylookupcsv' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why      = "decision U5: a design reads a lookup file only by a relative name inside its own "
                 "directory"},
    {.name     = "u5: legacy lookup element through two dots that stays inside",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>tables/../tables/./drag.csv</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] rows:null
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "u5: lookup file with the name of a device",
     .xml      = R"xml(
<conditions>
  <draglookup file="nul"/>
  <stabilitylookup file="tables/COM1.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file 'nul': Failed to read lookup table from {DIR}/design/nul
  W[Other,NORMAL] Failed to load stabilitylookup from file 'tables/COM1.csv': Failed to read lookup table from {DIR}/design/tables/COM1.csv
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file 'nul' of element 'draglookup' was not read: its path holds a name that Windows reserves for a device.
  W[Other,NORMAL] The lookup table file 'tables/COM1.csv' of element 'stabilitylookup' was not read: its path holds a name that Windows reserves for a device.
)out",
     .why = "part K2, beyond decision U5: a name Windows reserves for a device is refused on every "
            "platform"},
    {.name     = "u5: legacy lookup element with the name of a device",
     .xml      = R"xml(
<conditions>
  <draglookupcsv>Aux.txt</draglookupcsv>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load drag lookup CSV 'Aux.txt', ignoring. Reason: Failed to read lookup table from {DIR}/design/Aux.txt
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file 'Aux.txt' of element 'draglookupcsv' was not read: its path holds a name that Windows reserves for a device.
)out",
     .why = "part K2, beyond decision U5: a name Windows reserves for a device is refused on every "
            "platform"},
    {.name     = "u5: lookup file with an element of dots and blanks",
     .xml      = R"xml(
<conditions>
  <draglookup file=".../drag.csv"/>
  <stabilitylookup file="tables/.. ./stability.csv"/>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Failed to load draglookup from file '.../drag.csv': Failed to read lookup table from {DIR}/design/.../drag.csv
  W[Other,NORMAL] Failed to load stabilitylookup from file 'tables/.. ./stability.csv': Failed to read lookup table from {DIR}/design/tables/.. ./stability.csv
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] The lookup table file '.../drag.csv' of element 'draglookup' was not read: a design may name such a file only by a relative path inside its own directory.
  W[Other,NORMAL] The lookup table file 'tables/.. ./stability.csv' of element 'stabilitylookup' was not read: a design may name such a file only by a relative path inside its own directory.
)out",
     .why = "decision U5: a design reads a lookup file only by a relative name inside its own "
            "directory; an element of dots and blanks counts as two dots, which Windows may read "
            "it as"},
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
// exist, as the absolute path the options keep and the saver writes. It is loaded as the test
// loads it, from memory, which is a design without a directory. The whole round trip waits for
// the saver (tier 10).

/// SimulationLookupCopyTest.CSV
constexpr std::string_view kMissingCsv = "missing-beta-test-lookup.csv";

/// SimulationLookupCopyTest.CSV as the options of that test keep it and as its saver writes
/// it: made absolute against the current directory.
[[nodiscard]] std::filesystem::path missingCsv()
{
    return QtRocket::absolutePath(QtRocket::pathFromUtf8(kMissingCsv));
}

/// @p text as the value of an XML attribute between double quotes.
[[nodiscard]] std::string attributeText(std::string text)
{
    text = replaceAll(std::move(text), "&", "&amp;");
    text = replaceAll(std::move(text), "<", "&lt;");
    return replaceAll(std::move(text), "\"", "&quot;");
}

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
                       attributeText(genericUtf8(missingCsv())), value);
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
    // A design without a directory, as roundTrip() loads one from memory.
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
    // The file is kept as the saver wrote it, an absolute path, and is not there.
    EXPECT_EQ(loaded.getDragLookupCsvPath(), missingCsv());
    EXPECT_EQ(loaded.getStabilityLookupCsvPath(), missingCsv());
    std::error_code noFile;
    EXPECT_FALSE(std::filesystem::exists(missingCsv(), noFile));
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
    EXPECT_EQ(loadDragFile(directory, "design/tables/drag.csv"),
              "ok | drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] "
              "rows:[kept]");
    // The file is stored as an absolute path without "." and "..".
    EXPECT_EQ(loadDragFile(directory, "design/tables/./x/../drag.csv"),
              "ok | drag=path:{DIR}/design/tables/drag.csv table:[aoa=false mach=0.0..1.0 cd=0.4] "
              "rows:[kept]");
}

TEST(CsvLookupHandler, AFileThatFailsChangesNothing)
{
    const TempDir directory;
    writeLookupFiles(directory);
    const std::string_view unchanged = " | drag=path:null table:null rows:[kept]";
    EXPECT_EQ(loadDragFile(directory, "design/tables/missing.csv"),
              "IO [Failed to read lookup table from {DIR}/design/tables/missing.csv]" +
                  std::string(unchanged));
    EXPECT_EQ(loadDragFile(directory, "design/tables/stability.csv"),
              "PARSE [Lookup table header missing required column 'cd'. Make sure the column is "
              "included and you are using the correct field separator.]" +
                  std::string(unchanged));
    // Not OpenRocket's, which takes the table: decision U3.
    EXPECT_EQ(loadDragFile(directory, "design/tables/nan.csv"),
              "PARSE [Illegal numeric value 'NaN' in column 'cd']" + std::string(unchanged));
}

TEST(CsvLookupHandler, LoadsTheStabilityTableWhenItIsNotTheDragTable)
{
    const TempDir directory;
    writeLookupFiles(directory);
    SimulationOptions   options;
    const ChangeCounter changes(options.changed());

    const Result<void> loaded = CsvLookupHandler::loadFromFile(
        options, directory.resolve("design/tables/stability.csv"), false);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(options.hasDragLookup());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(1.0, 0, "cp"), 4.0);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);
    EXPECT_EQ(changes.count(), 1);
}

// ---- which file a design may read (the user's decision of 2026-10-08) -----------------------
//
// OpenRocket reads whatever path a design names. Here a design reads a lookup table only by a
// relative name that stays inside its own directory. The rule is CsvLookupHandler::
// fileOfDesign(), which looks at the name alone; the tables below are a sweep of the forms a
// name can take. What a whole load does with such names, and that no file outside the design's
// directory is opened, is in lookup_file_containment_tests.cpp.

/// The name of the constant @p refusal.
[[nodiscard]] std::string_view refusalName(CsvLookupHandler::Refusal refusal)
{
    switch (refusal)
    {
        case CsvLookupHandler::Refusal::NO_DIRECTORY:
            return "NO_DIRECTORY";
        case CsvLookupHandler::Refusal::NOT_RELATIVE:
            return "NOT_RELATIVE";
        case CsvLookupHandler::Refusal::LEAVES_DIRECTORY:
            return "LEAVES_DIRECTORY";
        case CsvLookupHandler::Refusal::DEVICE_NAME:
            return "DEVICE_NAME";
    }
    return "?";
}

/// What CsvLookupHandler::fileOfDesign() makes of the name @p file for a design whose directory
/// is @p designDirectory: the file, with '/' between its elements on every platform, or the
/// name of the refusal.
[[nodiscard]] std::string fileOfDesign(const std::optional<std::filesystem::path>& designDirectory,
                                       std::string_view                            file)
{
    const std::expected<std::filesystem::path, CsvLookupHandler::Refusal> found =
        CsvLookupHandler::fileOfDesign(designDirectory, QtRocket::pathFromUtf8(file));
    return found.has_value() ? genericUtf8(*found) : std::string(refusalName(found.error()));
}

/// A name a design gives a lookup file, and what fileOfDesign() makes of it for a design whose
/// directory is "design".
struct NameCase
{
    std::string_view file;
    std::string_view expected;
};

/// The cases of @p cases that fileOfDesign() answers otherwise, each as "<file>: <answer>, not
/// <expected>"; empty when it answers all of them as expected.
[[nodiscard]] Texts wrongAnswers(std::span<const NameCase> cases)
{
    const std::filesystem::path design("design");
    Texts                       wrong;
    for (const NameCase& nameCase : cases)
    {
        const std::string answer = fileOfDesign(design, nameCase.file);
        if (answer != nameCase.expected)
        {
            wrong.push_back(
                std::format("{}: {}, not {}", nameCase.file, answer, nameCase.expected));
        }
    }
    return wrong;
}

constexpr auto kNamesInside = std::to_array<NameCase>({
    {.file = "drag.csv", .expected = "design/drag.csv"},
    {.file = "tables/drag.csv", .expected = "design/tables/drag.csv"},
    {.file = "./tables/./drag.csv", .expected = "design/tables/drag.csv"},
    {.file = "tables//drag.csv", .expected = "design/tables/drag.csv"},
    // Two dots that do not lead out: the normal form is what is read.
    {.file = "tables/sub/../drag.csv", .expected = "design/tables/drag.csv"},
    {.file = "a/b/../../drag.csv", .expected = "design/drag.csv"},
    {.file = "a/../b/../c/drag.csv", .expected = "design/c/drag.csv"},
    // Back to the directory itself, which is no file: reading it fails.
    {.file = "tables/..", .expected = "design/."},
    // Dots that are part of a name.
    {.file = "..drag.csv", .expected = "design/..drag.csv"},
    {.file = "drag..csv", .expected = "design/drag..csv"},
    {.file = ".hidden/drag.csv", .expected = "design/.hidden/drag.csv"},
    {.file = "tables/drag csv", .expected = "design/tables/drag csv"},
    // A name that is not ASCII, as UTF-8.
    {.file = "tr\xC3\xA4gheit.csv", .expected = "design/tr\xC3\xA4gheit.csv"},
});

constexpr auto kNamesThatLeave = std::to_array<NameCase>({
    // Two dots at the start ...
    {.file = "..", .expected = "LEAVES_DIRECTORY"},
    {.file = "../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "../outside/drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "../../../../../../etc/passwd", .expected = "LEAVES_DIRECTORY"},
    {.file = "./../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "..//drag.csv", .expected = "LEAVES_DIRECTORY"},
    // ... in the middle ...
    {.file = "tables/../../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/../../outside/drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/./../.././drag.csv", .expected = "LEAVES_DIRECTORY"},
    // ... and after a subdirectory, further up than the name went down.
    {.file = "tables/sub/../../../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "a/b/c/../../../../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/../..", .expected = "LEAVES_DIRECTORY"},
    // Out and in again, into the design's own directory: refused all the same.
    {.file = "../design/tables/drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/../../design/tables/drag.csv", .expected = "LEAVES_DIRECTORY"},
    // An element of dots and blanks, which Windows may read as one or two dots.
    {.file = "...", .expected = "LEAVES_DIRECTORY"},
    {.file = ".../drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/.. /drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/. ./drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/ /drag.csv", .expected = "LEAVES_DIRECTORY"},
    {.file = "tables/..../drag.csv", .expected = "LEAVES_DIRECTORY"},
});

constexpr auto kNamesWithARoot = std::to_array<NameCase>({
    {.file = "/", .expected = "NOT_RELATIVE"},
    {.file = "/drag.csv", .expected = "NOT_RELATIVE"},
    {.file = "/etc/passwd", .expected = "NOT_RELATIVE"},
    {.file = "/dev/zero", .expected = "NOT_RELATIVE"},
    {.file = "//drag.csv", .expected = "NOT_RELATIVE"},
    {.file = "///", .expected = "NOT_RELATIVE"},
    {.file = "//server/share/drag.csv", .expected = "NOT_RELATIVE"},
    {.file = "/../drag.csv", .expected = "NOT_RELATIVE"},
    // An absolute path into the design's own directory.
    {.file = "/design/tables/drag.csv", .expected = "NOT_RELATIVE"},
});

constexpr auto kNamesOfDevices = std::to_array<NameCase>({
    {.file = "nul", .expected = "DEVICE_NAME"},
    {.file = "NUL", .expected = "DEVICE_NAME"},
    {.file = "tables/nul", .expected = "DEVICE_NAME"},
    {.file = "tables/Con.csv", .expected = "DEVICE_NAME"},
    {.file = "com1", .expected = "DEVICE_NAME"},
    {.file = "tables/sub/LPT9.txt", .expected = "DEVICE_NAME"},
    {.file = "aux.tar.gz", .expected = "DEVICE_NAME"},
    // A directory of such a name.
    {.file = "prn/drag.csv", .expected = "DEVICE_NAME"},
    // Leaving the directory is looked for first.
    {.file = "../nul", .expected = "LEAVES_DIRECTORY"},
    {.file = "nul/.../drag.csv", .expected = "LEAVES_DIRECTORY"},
    // No device: the name goes on.
    {.file = "null", .expected = "design/null"},
    {.file = "tables/console.csv", .expected = "design/tables/console.csv"},
    {.file = "drag.nul", .expected = "design/drag.nul"},
});

TEST(CsvLookupHandler, ARelativeNameThatStaysInsideIsAFileOfTheDesignsDirectory)
{
    EXPECT_EQ(wrongAnswers(kNamesInside), Texts{});
}

TEST(CsvLookupHandler, ANameThatLeavesTheDesignsDirectoryIsRefused)
{
    EXPECT_EQ(wrongAnswers(kNamesThatLeave), Texts{});
}

TEST(CsvLookupHandler, ANameWithARootIsRefused)
{
    EXPECT_EQ(wrongAnswers(kNamesWithARoot), Texts{});
}

TEST(CsvLookupHandler, ANameOfADeviceOfWindowsIsRefusedOnEveryPlatform)
{
    EXPECT_EQ(wrongAnswers(kNamesOfDevices), Texts{});
}

// The forms of a Windows path. Which of them has a root, and where its elements end, is what
// std::filesystem::path of the platform says: on Windows a drive, a server and a device path
// are roots and a backslash separates elements, so every one of these is refused. Elsewhere a
// backslash and a colon are characters of a name like any other, and each of these is one odd
// name (or, with slashes, a path of odd names) inside the design's directory, where no such
// file is.
constexpr auto kWindowsForms = std::to_array<NameCase>({
#ifdef _WIN32
    {.file = R"(C:\tables\drag.csv)", .expected = "NOT_RELATIVE"},
    {.file = "C:/tables/drag.csv", .expected = "NOT_RELATIVE"},
    // A drive without a root directory: a file of that drive's current directory.
    {.file = "C:drag.csv", .expected = "NOT_RELATIVE"},
    // The root of the current drive.
    {.file = R"(\tables\drag.csv)", .expected = "NOT_RELATIVE"},
    // A server (UNC).
    {.file = R"(\\server\share\drag.csv)", .expected = "NOT_RELATIVE"},
    // Device paths.
    {.file = R"(\\.\NUL)", .expected = "NOT_RELATIVE"},
    {.file = R"(\\.\C:\tables\drag.csv)", .expected = "NOT_RELATIVE"},
    {.file = R"(\\?\C:\tables\drag.csv)", .expected = "NOT_RELATIVE"},
    // Two dots between backslashes.
    {.file = R"(..\outside\drag.csv)", .expected = "LEAVES_DIRECTORY"},
    {.file = R"(tables\..\..\outside\drag.csv)", .expected = "LEAVES_DIRECTORY"},
    {.file = R"(tables\sub\..\drag.csv)", .expected = "design/tables/drag.csv"},
    // The name of a device behind a backslash.
    {.file = R"(tables\nul)", .expected = "DEVICE_NAME"},
#else
    {.file = R"(C:\tables\drag.csv)", .expected = R"(design/C:\tables\drag.csv)"},
    {.file = "C:/tables/drag.csv", .expected = "design/C:/tables/drag.csv"},
    {.file = "C:drag.csv", .expected = "design/C:drag.csv"},
    {.file = R"(\tables\drag.csv)", .expected = R"(design/\tables\drag.csv)"},
    {.file = R"(\\server\share\drag.csv)", .expected = R"(design/\\server\share\drag.csv)"},
    {.file = R"(\\.\NUL)", .expected = R"(design/\\.\NUL)"},
    {.file = R"(\\.\C:\tables\drag.csv)", .expected = R"(design/\\.\C:\tables\drag.csv)"},
    {.file = R"(\\?\C:\tables\drag.csv)", .expected = R"(design/\\?\C:\tables\drag.csv)"},
    {.file = R"(..\outside\drag.csv)", .expected = R"(design/..\outside\drag.csv)"},
    {.file     = R"(tables\..\..\outside\drag.csv)",
     .expected = R"(design/tables\..\..\outside\drag.csv)"},
    {.file = R"(tables\sub\..\drag.csv)", .expected = R"(design/tables\sub\..\drag.csv)"},
    {.file = R"(tables\nul)", .expected = R"(design/tables\nul)"},
#endif
});

TEST(CsvLookupHandler, TheFormsOfAWindowsPathAreWhatThePlatformsLibraryTakesThemFor)
{
    EXPECT_EQ(wrongAnswers(kWindowsForms), Texts{});
}

// A design read from memory, with no base directory given: whatever it names, nothing is read.
TEST(CsvLookupHandler, ADesignWithoutADirectoryReadsNoFile)
{
    EXPECT_EQ(fileOfDesign(std::nullopt, "drag.csv"), "NO_DIRECTORY");
    EXPECT_EQ(fileOfDesign(std::nullopt, "tables/drag.csv"), "NO_DIRECTORY");
    EXPECT_EQ(fileOfDesign(std::nullopt, "../drag.csv"), "NO_DIRECTORY");
    EXPECT_EQ(fileOfDesign(std::nullopt, "/etc/passwd"), "NO_DIRECTORY");
    EXPECT_EQ(fileOfDesign(std::nullopt, "nul"), "NO_DIRECTORY");
    // An empty directory is one: the current directory, as "." is.
    EXPECT_EQ(fileOfDesign(std::filesystem::path(), "tables/drag.csv"), "tables/drag.csv");
    EXPECT_EQ(fileOfDesign(std::filesystem::path("."), "tables/drag.csv"), "./tables/drag.csv");
    EXPECT_EQ(fileOfDesign(std::filesystem::path(), "../drag.csv"), "LEAVES_DIRECTORY");
}

// The design's directory is taken as it is given: it may be relative, and have two dots.
TEST(CsvLookupHandler, TheDesignsDirectoryIsTakenAsItIsGiven)
{
    EXPECT_EQ(fileOfDesign(std::filesystem::path("../designs/mine"), "tables/drag.csv"),
              "../designs/mine/tables/drag.csv");
    EXPECT_EQ(fileOfDesign(std::filesystem::path("/home/someone"), "drag.csv"),
              "/home/someone/drag.csv");
}

/// The names of @p names for which CsvLookupHandler::isDeviceName() does not answer @p expected.
[[nodiscard]] Texts wrongDeviceAnswers(std::span<const std::string_view> names, bool expected)
{
    Texts wrong;
    for (const std::string_view name : names)
    {
        if (CsvLookupHandler::isDeviceName(name) != expected)
        {
            wrong.emplace_back(name);
        }
    }
    return wrong;
}

constexpr auto kDeviceNames = std::to_array<std::string_view>({
    "CON",
    "PRN",
    "AUX",
    "NUL",
    "COM0",
    "COM1",
    "COM9",
    "LPT0",
    "LPT1",
    "LPT9",
    "CONIN$",
    "CONOUT$",
    // In any case, ...
    "con",
    "Nul",
    "cOm3",
    "lpt7",
    "conin$",
    // ... whatever follows a first dot or colon, ...
    "nul.csv",
    "NUL.tar.gz",
    "aux.",
    "com1:",
    "nul:stream",
    // ... with blanks at the end, ...
    "NUL ",
    "nul  .txt",
    // ... and numbered with a superscript one, two or three.
    "COM\xC2\xB9",
    "com\xC2\xB2.csv",
    "LPT\xC2\xB3",
});

constexpr auto kOtherNames = std::to_array<std::string_view>({
    "",
    ".",
    "..",
    "null",
    "nu",
    "con1",
    "console",
    "com",
    "com10",
    "com1x",
    "lpt",
    "lpt10",
    "comA",
    "auxiliary",
    "prnt",
    "drag.nul",
    "x.con",
    " nul",
    "_nul",
    "nul_",
    "conin",
    "conout$x",
    // The masculine ordinal indicator is no digit, two superscripts are no number, and a
    // letter that is not ASCII is not folded.
    "COM\xC2\xBA",
    "COM\xC2\xB9\xC2\xB9",
    "c\xC3\xB6n",
});

TEST(CsvLookupHandler, TellsTheNamesWindowsReservesForADevice)
{
    EXPECT_EQ(wrongDeviceAnswers(kDeviceNames, true), Texts{});
    EXPECT_EQ(wrongDeviceAnswers(kOtherNames, false), Texts{});
}

// ---- every name of a sweep, against a model of the rule ---------------------------------------
//
// The tables above are forms someone thought of. Here is every name that can be made of up to
// four of nine elements with a '/' between them (7380 names): what fileOfDesign() makes of each
// is compared with a model of the rule that is written out by hand below, without
// std::filesystem, and a name that is taken must lie inside the design's directory by the
// library's own reckoning.

/// The elements the names of the sweep are made of: two dots, one dot, nothing (a doubled or
/// a leading separator), plain names, elements of dots and blanks, and the name of a device.
constexpr auto kSweepElements =
    std::to_array<std::string_view>({"..", ".", "", "a", "tables", "...", ".. ", "nul", "x.csv"});

/// Whether @p names has the name @p name.
[[nodiscard]] bool holds(const std::vector<std::string>& names, std::string_view name)
{
    return std::ranges::any_of(names, [name](const std::string& one) { return one == name; });
}

/// What the rule makes of the name @p name, whose elements are separated by '/', by a model of
/// its own: the name of the refusal, or "inside". A name that starts with a separator has a
/// root. Otherwise the elements are walked through as a normal form is made: nothing and one
/// dot are passed over, a name goes onto a stack, and two dots take the last name off it, or
/// lead out of the directory when there is none. What is left on the stack decides the rest.
[[nodiscard]] std::string_view modelOf(std::string_view name)
{
    if (name.starts_with('/'))
    {
        return "NOT_RELATIVE";
    }
    std::vector<std::string> stack;
    for (const std::string& element : QtRocket::Strings::split(name, '/'))
    {
        if (element.empty() || element == ".")
        {
            continue;
        }
        if (element != "..")
        {
            stack.push_back(element);
        }
        else if (stack.empty())
        {
            return "LEAVES_DIRECTORY";
        }
        else
        {
            stack.pop_back();
        }
    }
    if (holds(stack, "...") || holds(stack, ".. "))
    {
        return "LEAVES_DIRECTORY";
    }
    return holds(stack, "nul") ? "DEVICE_NAME" : "inside";
}

/// How many names of a sweep ended in each way, and the names that fileOfDesign() and the
/// model disagree about or that are taken and do not lie inside the design's directory.
struct SweepResult
{
    int   inside{0};
    int   notRelative{0};
    int   leaves{0};
    int   device{0};
    Texts wrong;
};

/// Adds what fileOfDesign() makes of @p name to @p result.
void sweepName(const std::string& name, SweepResult& result)
{
    const std::filesystem::path                                           design("/top/design");
    const std::expected<std::filesystem::path, CsvLookupHandler::Refusal> found =
        CsvLookupHandler::fileOfDesign(design, QtRocket::pathFromUtf8(name));
    const std::string_view answer = found.has_value() ? "inside" : refusalName(found.error());
    const std::string_view model  = modelOf(name);
    if (answer != model)
    {
        result.wrong.push_back(std::format("'{}': {}, not {}", name, answer, model));
    }
    else if (found.has_value())
    {
        // The library's own reckoning: from the design's directory to the file, no step leads
        // up.
        const std::filesystem::path steps = found->lexically_normal().lexically_relative(design);
        if (steps.empty() || *steps.begin() == "..")
        {
            result.wrong.push_back(
                std::format("'{}' is taken and is '{}'", name, genericUtf8(*found)));
        }
    }
    result.inside += answer == "inside" ? 1 : 0;
    result.notRelative += answer == "NOT_RELATIVE" ? 1 : 0;
    result.leaves += answer == "LEAVES_DIRECTORY" ? 1 : 0;
    result.device += answer == "DEVICE_NAME" ? 1 : 0;
}

/// Sweeps every name of up to @p length elements of kSweepElements that starts with @p name.
void sweepNames(const std::string& name, int length, SweepResult& result)
{
    sweepName(name, result);
    if (length <= 1)
    {
        return;
    }
    for (const std::string_view element : kSweepElements)
    {
        sweepNames(std::format("{}/{}", name, element), length - 1, result);
    }
}

/// The sweep of every name of one to four elements.
[[nodiscard]] SweepResult sweepEveryName()
{
    SweepResult result;
    for (const std::string_view element : kSweepElements)
    {
        sweepNames(std::string(element), 4, result);
    }
    return result;
}

TEST(CsvLookupHandler, NoNameOfTheSweepLeadsOutOfTheDesignsDirectory)
{
    const SweepResult result = sweepEveryName();
    EXPECT_EQ(result.wrong, Texts{});
    // Every name of the sweep, and names of every kind (the numbers are the model's).
    EXPECT_EQ(result.inside + result.notRelative + result.leaves + result.device,
              9 + 81 + 729 + 6561);
    EXPECT_EQ(result.inside, 1291);
    EXPECT_EQ(result.notRelative, 819);
    EXPECT_EQ(result.leaves, 4377);
    EXPECT_EQ(result.device, 893);
}
/// CsvLookupHandler::nameOfDesign() of @p file for a design whose directory is
/// @p designDirectory, with '/' between its elements, or "null".
[[nodiscard]] std::string nameOfDesign(const std::optional<std::filesystem::path>& designDirectory,
                                       std::string_view                            file)
{
    const std::optional<std::filesystem::path> name =
        CsvLookupHandler::nameOfDesign(designDirectory, QtRocket::pathFromUtf8(file));
    return name.has_value() ? genericUtf8(*name) : "null";
}

// The file beside rows is a name that is kept, whatever it is: nothing opens it.
TEST(CsvLookupHandler, KeepsTheFileBesideTheRowsAsAName)
{
    const std::filesystem::path design("design");
    // A relative name is one of the design's directory, also when it leads out of it.
    EXPECT_EQ(nameOfDesign(design, "drag.csv"), "design/drag.csv");
    EXPECT_EQ(nameOfDesign(design, "tables/drag.csv"), "design/tables/drag.csv");
    EXPECT_EQ(nameOfDesign(design, "../outside/drag.csv"), "design/../outside/drag.csv");
    EXPECT_EQ(nameOfDesign(design, "nul"), "design/nul");
    // A name with a root stays what it is, as in OpenRocket.
    EXPECT_EQ(nameOfDesign(design, "/home/someone/drag.csv"), "/home/someone/drag.csv");
    EXPECT_EQ(nameOfDesign(std::nullopt, "/home/someone/drag.csv"), "/home/someone/drag.csv");
    // Without a directory a relative name names nothing.
    EXPECT_EQ(nameOfDesign(std::nullopt, "drag.csv"), "null");
    EXPECT_EQ(nameOfDesign(std::nullopt, "../outside/drag.csv"), "null");
}

TEST(CsvLookupHandler, GivesARefusalTheWarningOfItsKind)
{
    const std::string outside =
        "The lookup table file '../x.csv' of element 'draglookup' was not read: a design may "
        "name such a file only by a relative path inside its own directory.";
    using Refusal = CsvLookupHandler::Refusal;
    EXPECT_EQ(CsvLookupHandler::refusalWarning(Refusal::NO_DIRECTORY, "draglookup", "../x.csv"),
              outside);
    EXPECT_EQ(CsvLookupHandler::refusalWarning(Refusal::NOT_RELATIVE, "draglookup", "../x.csv"),
              outside);
    EXPECT_EQ(CsvLookupHandler::refusalWarning(Refusal::LEAVES_DIRECTORY, "draglookup", "../x.csv"),
              outside);
    EXPECT_EQ(CsvLookupHandler::refusalWarning(Refusal::DEVICE_NAME, "stabilitylookupcsv", "nul"),
              "The lookup table file 'nul' of element 'stabilitylookupcsv' was not read: its "
              "path holds a name that Windows reserves for a device.");
}

/// What the <conditions> element @p xml gives a design whose directory is @p designDirectory
/// (nullopt: a design without one): which tables the options have ("drag", "stability", both
/// or "none"), then every warning, each behind " | ". Files are named as neutralPaths() names
/// them, with @p directory as the test's directory.
[[nodiscard]] std::string loadLookups(std::string_view                            xml,
                                      const std::optional<std::filesystem::path>& designDirectory,
                                      const std::filesystem::path&                directory = {})
{
    ConditionsFixture fixture;
    fixture.context().setDesignDirectory(designDirectory);
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run = runHandler(handler, xml);
    if (!run.result.has_value())
    {
        return "failed: " + run.result.error().message;
    }
    const SimulationOptions& options = handler.getConditions();
    std::string              text    = options.hasDragLookup() ? "drag" : "";
    if (options.hasStabilityLookup())
    {
        text += text.empty() ? "stability" : " stability";
    }
    if (text.empty())
    {
        text = "none";
    }
    for (const std::string& warning : run.texts())
    {
        text += " | " + warning;
    }
    return neutralPaths(std::move(text), directory);
}

/// The warning for the file @p file of the element @p element that a design may not read
/// because it is not a relative name inside its directory.
[[nodiscard]] std::string outsideWarning(std::string_view element, std::string_view file)
{
    return std::format(
        "The lookup table file '{}' of element '{}' was not read: a design may name "
        "such a file only by a relative path inside its own directory.",
        file, element);
}

// Hostile input: a design names a device of the system, by its absolute path, as its lookup
// table. OpenRocket would read /dev/zero without end. The name is refused as it stands, and
// nothing is asked of the file system, with a directory and without one.
TEST(CsvLookupHandler, AnAbsolutePathIsRefusedWhateverItNames)
{
    const std::string_view xml =
        "<conditions><draglookup file='/dev/zero'/>"
        "<stabilitylookupcsv>/dev/zero</stabilitylookupcsv>"
        "</conditions>";
    const std::string refused =
        std::format("none | {} | {}", outsideWarning("draglookup", "/dev/zero"),
                    outsideWarning("stabilitylookupcsv", "/dev/zero"));
    EXPECT_EQ(loadLookups(xml, std::filesystem::path("design")), refused);
    EXPECT_EQ(loadLookups(xml, std::nullopt), refused);
}

// A file attribute of separators only names the root. (OpenRocket reads it and fails: "Failed
// to load draglookup from file '/': Failed to read lookup table from /".) The warning has the
// name as the design has it, trimmed.
TEST(CsvLookupHandler, AFileOfSeparatorsOnlyIsRefusedAsARoot)
{
    EXPECT_EQ(loadLookups("<conditions><draglookup file='///'/>"
                          "<stabilitylookup file=' // '/></conditions>",
                          std::filesystem::path("design")),
              std::format("none | {} | {}", outsideWarning("draglookup", "///"),
                          outsideWarning("stabilitylookup", "//")));
}

// A design without a directory reads no file, also not one that is there under the name in
// the current directory of the process; with the directory the same element finds its table.
TEST(CsvLookupHandler, ADesignWithoutADirectoryDoesNotReadTheCurrentDirectory)
{
    const TempDir directory;
    writeLookupFiles(directory);
    const CurrentDirectoryGuard inside(directory.resolve("design"));
    const std::string_view      xml =
        "<conditions><draglookup file='tables/drag.csv'/>"
        "<stabilitylookupcsv>tables/stability.csv"
        "</stabilitylookupcsv></conditions>";

    EXPECT_EQ(loadLookups(xml, std::nullopt),
              std::format("none | {} | {}", outsideWarning("draglookup", "tables/drag.csv"),
                          outsideWarning("stabilitylookupcsv", "tables/stability.csv")));
    EXPECT_EQ(loadLookups(xml, directory.resolve("design")), "drag stability");
}

/// Makes @p link a symbolic link to @p target; false where that cannot be done (on Windows it
/// takes a privilege).
[[nodiscard]] bool makeLink(const std::filesystem::path& target, const std::filesystem::path& link)
{
    std::error_code error;
    std::filesystem::create_symlink(target, link, error);
    return !error;
}

// What the rule does not do: a symbolic link in the design's directory is part of that
// directory, which is the user's own (a design file cannot make one), and is followed wherever
// it points.
TEST(CsvLookupHandler, ALinkInsideTheDesignsDirectoryIsFollowed)
{
    const TempDir directory;
    writeLookupFiles(directory);
    if (!makeLink(directory.resolve("outside/drag.csv"), directory.resolve("design/linked.csv")))
    {
        GTEST_SKIP() << "no symbolic link can be made here";
    }

    ConditionsFixture fixture;
    fixture.context().setDesignDirectory(directory.resolve("design"));
    SimulationConditionsHandler handler(fixture.context());
    const HandlerRun            run = runHandler(handler,
                                                 "<conditions><draglookup file='linked.csv'/>"
                                                 "</conditions>");

    EXPECT_EQ(run.texts(), Texts{});
    ASSERT_TRUE(handler.getConditions().hasDragLookup());
    // The table outside, whose drag coefficient is 0.9 everywhere.
    EXPECT_EQ(handler.getConditions().getDragLookupTable()->interpolate(0.5, 0, "cd"), 0.9);
    EXPECT_EQ(handler.getConditions().getDragLookupCsvPath(),
              directory.resolve("design/linked.csv").lexically_normal());
}

/// Whether this machine has the device @p device (a POSIX system).
[[nodiscard]] bool hasDevice(const std::filesystem::path& device)
{
    std::error_code error;
    return std::filesystem::is_character_file(device, error);
}

// A device that a link of the design's directory leads to is found by the name, and is not
// opened: a lookup file is read only when it is a regular file (CsvMachAoALookup).
TEST(CsvLookupHandler, ADeviceBehindALinkOfTheDesignsDirectoryIsNotRead)
{
    const TempDir directory;
    writeLookupFiles(directory);
    if (!hasDevice("/dev/zero") ||
        !makeLink(std::filesystem::path("/dev/zero"), directory.resolve("design/zero.csv")))
    {
        GTEST_SKIP() << "no /dev/zero here, or no symbolic link can be made";
    }

    EXPECT_EQ(loadLookups("<conditions><draglookup file='zero.csv'/>"
                          "<stabilitylookupcsv>zero.csv</stabilitylookupcsv></conditions>",
                          directory.resolve("design"), directory.path()),
              "none | Failed to load draglookup from file 'zero.csv': Failed to read lookup "
              "table from {DIR}/design/zero.csv | Failed to load stability lookup CSV "
              "'zero.csv', ignoring. Reason: Failed to read lookup table from "
              "{DIR}/design/zero.csv");
}

/// A handler of a drag table over @p options, with the rows @p rows closed on it, for a design
/// whose directory is @p designDirectory.
[[nodiscard]] std::unique_ptr<CsvLookupHandler> handlerWithRows(
    SimulationOptions& options, const Rows& rows,
    std::optional<std::filesystem::path> designDirectory = std::nullopt)
{
    auto handler =
        std::make_unique<CsvLookupHandler>(options, Rows{"cd"}, true, std::move(designDirectory));
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
    CsvLookupHandler  handler(options, Rows{"cd"}, true, std::nullopt);
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
// element's own attribute, which is a file of the design's directory and need not exist.
TEST(CsvLookupHandler, StoresTheTableWhenTheElementEnds)
{
    SimulationOptions           options;
    const std::filesystem::path design =
        QtRocket::absolutePath(std::filesystem::path("a-design-directory-that-is-not-there"));
    const std::unique_ptr<CsvLookupHandler> handler =
        handlerWithRows(options, {"Mach;Cd", "0;0.5", "2;1.5"}, design);
    WarningSet          warnings;
    const ChangeCounter changes(options.changed());

    const Result<void> ended =
        handler->endHandler("draglookup", {{"file", " not-there.csv "}}, "text", warnings);

    ASSERT_TRUE(ended.has_value());
    EXPECT_TRUE(warnings.empty());
    ASSERT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(1.0, 0, "cd"), 1.0);
    EXPECT_EQ(options.getDragLookupCsvRows(), (Rows{"Mach;Cd", "0;0.5", "2;1.5"}));
    EXPECT_EQ(options.getDragLookupCsvPath(), design / "not-there.csv");
    EXPECT_EQ(changes.count(), 1);
}

// The same for a design without a directory: the table of the rows, and no file, a relative
// name being one of no directory. (OpenRocket keeps the name against the current directory.)
TEST(CsvLookupHandler, ARelativeNameBesideTheRowsOfADesignWithoutADirectoryIsNotKept)
{
    SimulationOptions                       options;
    const std::unique_ptr<CsvLookupHandler> handler =
        handlerWithRows(options, {"Mach;Cd", "0;0.5", "2;1.5"});
    WarningSet warnings;

    const Result<void> ended =
        handler->endHandler("draglookup", {{"file", " not-there.csv "}}, "text", warnings);

    ASSERT_TRUE(ended.has_value());
    EXPECT_TRUE(warnings.empty());
    ASSERT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvRows(), (Rows{"Mach;Cd", "0;0.5", "2;1.5"}));
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
}

}  // namespace
