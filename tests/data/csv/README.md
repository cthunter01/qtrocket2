# CSV export fixtures

What OpenRocket's `CSVExport.exportCSV()` (`core/file/CSVExport.java`, the overload for flight
data) writes for flight data stored in the example designs of `data/examples/`, gzipped. They are
the fixtures of QtRocket's `CsvExport` golden test, `tests/core/file/csv_export_golden_tests.cpp`,
which exports the same branches with the same options and compares the text byte for byte (after
inflating the fixture: the gzip bytes themselves depend on the zlib of the JDK that wrote them).

Each file is one branch of one simulation of an example, as OpenRocket loads it (the design is
never simulated again, so the numbers are the file's and the export alone decides the text):
every flight data type of the branch, in the order of `FlightDataBranch.getTypes()`, each in the
default unit of its unit group, with the options of OpenRocket's export dialog as it first opens
(`,` between fields, 3 decimal places, exponential notation, `#` before a comment, the simulation,
field and event comments all on) or, for the `_variant` files, another set of options.

| File | Example | Simulation | Branch | Options (separator, decimals, exponential, comment, simulation/field/event comments) |
|---|---|---|---|---|
| `tube-fin-rocket_sim00_branch0.csv.gz` | Tube fin rocket | 0 | 0 | the dialog's |
| `tube-fin-rocket_sim00_branch0_variant1.csv.gz` | Tube fin rocket | 0 | 0 | `;`, 6, no, `%`, on/off/off |
| `tube-fin-rocket_sim00_branch0_variant2.csv.gz` | Tube fin rocket | 0 | 0 | space, 0, yes, `;`, off/on/on |
| `tube-fin-rocket_sim00_branch0_variant3.csv.gz` | Tube fin rocket | 0 | 0 | tab, 15, yes, `#`, off/off/on |
| `tube-fin-rocket_sim00_branch0_variant4.csv.gz` | Tube fin rocket | 0 | 0 | `,`, 3, yes, `//`, on/on/off |
| `two-stage-high-power-rocket_sim00_branch1.csv.gz` | Two stage high power rocket | 0 | 1 (the booster) | the dialog's |
| `dual-parachute-deployment_sim02_branch0.csv.gz` | Dual parachute deployment | 2 | 0 | the dialog's |
| `simulation-extensions_sim02_branch0.csv.gz` | Simulation extensions | 2 | 0 | the dialog's |
| `three-stage-low-power-rocket_sim00_branch2.csv.gz` | Three stage low power rocket | 0 | 2 | the dialog's |

Between them they hold five combinations of the three comment options, every separator and
comment starter the dialog offers and one starter of its own, 0, 3, 6 and 15 decimals with and
without the exponential notation, simulation warnings (with the components they name), a
SIM_WARN event, the zero-width space (U+200B) that is the unit of a dimensionless type, the column
of a simulation extension, and events at the same time whose order depends on the stage of their
source, which a loaded event knows by id only.

They were written by `tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/CsvExportDumper.java`
with OpenRocket 5f164fd0e (the commit of `tests/data/goldens/manifest.json`) on JDK 17.0.20.1,
Linux (CSVExport ends every line with the platform's line separator, which must be `\n`). How to
run it: `tools/openrocket-goldens/README.md`, section "CSV export fixtures". `.gitattributes` marks
`*.gz` binary. A fixture that is added or removed needs its row here, in `CsvExportDumper.FIXTURES`
and in the test's list.

The fixtures hold data of OpenRocket's example designs, Copyright (C) 2007-2025 Sampo Niskanen and
others, licensed under the GNU General Public License version 3 or later (see `NOTICE`).
