# What OpenRocket's "Import from image" makes of images

`tools/openrocket-fin-import` runs OpenRocket (Java) to write `tests/data/fin-images/`: test images for
the "Import from image" of the freeform fin editor and what OpenRocket makes of them
(`swing/.../gui/util/CustomFinImporter.java` and `FreeformFinSetConfig.importImage()`), for the tests of
QtRocket's `gui/util/CustomFinImporter` and of the import action of its `FreeformFinSetConfig`. The data
is committed; neither CMake nor CI ever needs Java.

- `generate.sh`: compiles the dumper with swing's `ServicesForTesting` against OpenRocket's compiled
  core and swing classes and runs it.
- `FinImportDumper.java`: the dumper. In the Guice set-up of swing's `ServicesForTesting` (its English
  translator) and `PluginModule`, it draws the test images with Java2D or sets their pixels, writes them
  as PNG files with `ImageIO`, and prints the report (below). Its comments say what each image holds.
- `logback.xml`: keeps OpenRocket's log to its warnings and errors.

## Regenerating

With the OpenRocket checkout at the commit of `tests/data/goldens/manifest.json` and Java 17:

```sh
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-fin-import/generate.sh \
    > tests/data/fin-images/report.txt
```

The script writes the images into `tests/data/fin-images/` (or into the directory given as its
argument) and prints the report to standard output; the environment variables are those of
`tools/openrocket-component-factory/generate.sh` (listed at the head of `generate.sh`). (The committed
files were written with `OPENROCKET_CLASSPATH`: the classes of a checkout built before, and the jars of
Gradle's cache.) A regeneration writes the same bytes: ImageIO's PNG writer and Java2D's rasterizer are
deterministic, and the report holds no time or address.

Two walks never end in Java (`one-pixel-tall.png`, `one-by-one.png`): they add points until the heap is
full. `generate.sh` runs Java with a heap of 256 MB, which they fill within a second; the dumper catches
the `OutOfMemoryError` (or gives up after 60 s) and writes "never ends".

## The report

Every block starts with a line `== <title>`; POINTS are `(x,y) (x,y) ...` in m, every double as Java 17's
`Double.toString()` writes it, after the number of points (`4: (0.0,0.0) ...`).

1. `== image NAME`, for every image (and `missing.png`, which is not there): `read ...`, the
   `BufferedImage` type and size `ImageIO.read()` gives (or `null`, or its exception); `raw ...`, the
   walk around the fin before its simplification (the private `validateImage()` and `loadFin()`, called
   by reflection as `getPoints()` calls them); `points ...`, what `getPoints()` returns. Instead of the
   points: `invalid` (no dark pixel on the bottom row), `error CLASS: MESSAGE` (the exception and its
   `getLocalizedMessage()`) or `never ends`.
2. `== optimize NAME`: the private `optimizePoints()` (by reflection) on the point list `input`, built by
   the dumper as `getPoints()` builds its list (the first point `Coordinate.NUL`, every other an object
   of its own): `once` after one pass, `loop passes=P` after `getPoints()`' loop of passes until a pass
   removes nothing.
3. `== grey8`, `== grey16`, `== grey1`, `== grey2`, `== grey4`, `== grey-alpha8`: what `getRGB()` gives
   for every level of a greyscale PNG of that depth written and read back through `ImageIO`: `srgb`
   (the blue byte of `getRGB()` for the levels 0, 1, ...) or, for 16 bits, `firsts V@L ...` (the first
   level L whose `getRGB()` gives V). Java's colour model converts the levels of an 8- or 16-bit greyscale
   image (with or without alpha) from linear grey to sRGB; those of 1, 2 or 4 bits are a palette.
4. `== import NAME [multi]`: `FreeformFinSetConfig.importImage()`'s steps on a design the dumper builds
   (Rocket > Stage > body tube 30 cm long with a fixed radius of 2.1 cm > a new freeform fin set, as the
   tests' `FreeformDesign`; `multi`: a second fin set, the first one's config listener): `imported`
   (`getPoints()`), then `startUndo("Import freeform fin set")`, `setPoints()`, `stopUndo()` and `after
   points ... length=L [other ...] undo=U redo=R`, then the document's undo (`undo ...`) and redo
   (`redo ...`).

`tests/gui/util/CustomFinImporterTests.cpp` compares QtRocket's walks, points, simplifications and grey
conversions with blocks 1 to 3 (`tests/gui/util/FinImportReport.h` reads the report);
`tests/gui/configdialog/fin_import_golden_tests.cpp` runs QtRocket's import action on the designs of
block 4. QtRocket's differences by design are listed in `CustomFinImporter.h`; the tests apply them to
the blocks they concern and pin Java's lines.
