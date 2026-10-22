# Images for "Import from image"

Test images of the freeform fin editor's "Import from image" and `report.txt`, what OpenRocket
(5f164fd0e, the commit of `tests/data/goldens/manifest.json`; JDK 17, Linux) makes of them, both written
by `tools/openrocket-fin-import/generate.sh` (the dumper `FinImportDumper.java` there draws the images
with Java2D or sets their pixels and writes them with `ImageIO`; its README describes the report's
blocks). Edit these files through the generator only. `tests/gui/util/CustomFinImporterTests.cpp` and
`tests/gui/configdialog/fin_import_golden_tests.cpp` read them (`tests/gui/util/FinImportReport.h`).

The fin is the dark area that touches the bottom row; one pixel is one millimetre. In the table, a
rectangle is `x, y, width, height` in pixels, from the top left.

| File | Size | What it holds |
|---|---|---|
| `trapezoid.png`, `rectangle.png`, `floating.png` | 80 x 50, 40 x 30, 20 x 20 | The M2 scouting's three images (its `FinImageProbe`), byte for byte: a black trapezoid on the bottom, a black rectangle on the bottom, a black square that does not touch the bottom (refused). |
| `white.png` | 10 x 10 | White only (refused). |
| `threshold.png` | 40 x 30 | A black rectangle 5, 10, 30, 20 with notches 3 x 4 in its top of grey 200 (black), grey 201 (white), (149, 217, 255) (black: 200.99999999999997 in doubles) and grey 202 (white). |
| `colours.png` | 40 x 30 | A red fin on yellow, a blue block at its right, a notch of light cyan. |
| `greyscale.png` | 40 x 30 | An 8-bit greyscale PNG: the fin of level 100, notches of 148, 149, 180, 201 and 230 (Java converts the levels to sRGB: all but 148 are white). |
| `greyscale16.png` | 40 x 30 | A 16-bit greyscale PNG: the fin 25700, notches of 30000, 38064 (black), 38065 (white) and 50000. |
| `greyscale4.png` | 40 x 30 | A 4-bit greyscale PNG: the fin of level 6 (102), notches of 8 (136), 11 (187) and 12 (204, white). |
| `greyscale-alpha.png` | 40 x 30 | An 8-bit greyscale PNG with alpha: the fin 100, notches of 149, 201, 149 and 230 where `threshold.png` has its four. |
| `transparent.png` | 40 x 30 | An RGBA PNG: a transparent white background, the fin transparent black on its left half and opaque black on its right, a notch of quarter-opaque white and one of opaque grey 128. |
| `transparent-background.png` | 30 x 20 | Transparent black everywhere, an opaque black block: all of it is fin. |
| `palette-without-black.png` | 40 x 30 | A palette PNG (white, navy, grey 128, grey 230): a navy rectangle 5, 10, 30, 20. |
| `palette-with-black.png` | 40 x 30 | A palette PNG (white, black, red): a black rectangle with a red block. |
| `palette-transparent.png` | 40 x 30 | A palette PNG (transparent white, black, white): a black rectangle on transparent white. |
| `one-pixel-tall.png` | 30 x 10 | A black run on the bottom row only (Java's walk never ends). |
| `single-pixel.png`, `one-pixel-wide.png` | 10 x 10 | One black pixel on the bottom row; a black column one pixel wide. |
| `full-black.png`, `one-by-one.png` | 12 x 8, 1 x 1 | Black everywhere (Java's walk never ends on the one pixel). |
| `edges.png` | 30 x 20 | A fin along the left, right and top edges of the image. |
| `staircase.png` | 90 x 30 | A right triangle whose hypotenuse climbs 1 pixel in 3. |
| `overhang.png` | 80 x 45 | A fin whose tip reaches beyond its root. |
| `arch.png` | 50 x 40 | An arch on two legs. |
| `swept.png` | 100 x 60 | A swept fin drawn anti-aliased (grey edges), with a hole, a second dark area on the bottom row and a speck above. |
| `not-an-image.png` | 22 bytes | The text "This is not an image." and a line end. |
| `truncated.png` | 60 bytes | The first 60 bytes of `trapezoid.png`. |
