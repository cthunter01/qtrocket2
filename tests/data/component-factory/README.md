# What OpenRocket's add buttons make

OpenRocket's own output (5f164fd0e, the commit of `tests/data/goldens/manifest.json`; JDK 17.0.20.1,
Linux), written by `tools/openrocket-component-factory/generate.sh` (the dumper
`ComponentFactoryDumper.java` there; its README says how to run it and what the report holds). Edit
these files through the generator only.

- `report.txt`: what the dumper printed: the materials of a new component of each of the 21 classes of
  the add buttons with three sets of preferences, every value Java's constructors give each class, and
  25 clicks on the add buttons with their undo texts and places. `tests/core/rocket/ComponentFactoryTests.cpp`
  and `tests/core/file/openrocket/add_buttons_save_tests.cpp` read its blocks
  (`tests/core/rocket/ComponentFactoryReport.h`) and compare QtRocket's values, written in the report's
  notation, with them.
- `add-buttons-rocket.ork.xml`: the `rocket.ork` of the archive OpenRocket's `GeneralRocketSaver` writes
  for the design of those 25 clicks, byte for byte. `add_buttons_save_tests.cpp` compares QtRocket's save
  of the same design with it, line for line, after numbering the ids of the `<id>` elements (random
  UUIDs) in the order of their first appearance and hiding the value of the `creator` attribute.
