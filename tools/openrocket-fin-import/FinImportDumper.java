import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.Polygon;
import java.awt.RenderingHints;
import java.awt.Transparency;
import java.awt.color.ColorSpace;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Path2D;
import java.awt.image.BufferedImage;
import java.awt.image.ComponentColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.IndexColorModel;
import java.awt.image.WritableRaster;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

import javax.imageio.ImageIO;

import com.google.inject.Guice;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.ArrayList;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.util.CustomFinImporter;

/**
 * What OpenRocket's "Import from image" of the freeform fin editor makes of images (swing
 * gui/util/CustomFinImporter.java and FreeformFinSetConfig.importImage()), for the tests of QtRocket's
 * gui/util/CustomFinImporter and the import action of its FreeformFinSetConfig.
 * <p>
 * Writes the test images (PNG files and two TIFF files, through ImageIO) into the directory args[0] and prints
 * the report on standard output, in blocks that start with a line "== <title>":
 * <ul>
 * <li>"== image NAME": "read type=T size=WxH", the image as ImageIO reads it (its BufferedImage type and size;
 * "read null" when ImageIO has no reader for it, "read CLASS: MESSAGE" for its exception), then "raw N:
 * POINTS", the walk around the fin before its simplification
 * (CustomFinImporter's private validateImage() and loadFin(), called by reflection), and "points N: POINTS",
 * what getPoints() returns. Instead of the points: "invalid" (validateImage() found no dark pixel on the
 * bottom row), "error CLASS: MESSAGE" (the exception, its getLocalizedMessage()) or "never ends" (the walk
 * runs until the heap is full of points).</li>
 * <li>"== optimize NAME": the private optimizePoints() (by reflection) on the points "input N: POINTS"
 * built here, the first of them Coordinate.NUL as in getPoints(): "once N: POINTS" after one pass, "loop
 * passes=P N: POINTS" after getPoints()' loop of passes until a pass removes nothing.</li>
 * <li>"== grey8", "== grey16", "== grey1/2/4" and "== grey-alpha8": what getRGB() gives for every level of a
 * greyscale PNG of that depth (written and read back through ImageIO): "read type=T" (the BufferedImage type),
 * then "srgb V0 V1 ..." (the blue byte of getRGB() for the levels 0, 1, ...) or, for 16 bits, "firsts V@L ..."
 * (the first level L whose getRGB() gives V).</li>
 * <li>"== import NAME [multi]": FreeformFinSetConfig.importImage()'s steps on a design built here (Rocket >
 * Stage > body tube 30 cm long, radius 2.1 cm, not automatic > a new freeform fin set; "multi": a second
 * freeform fin set with the points (0, 0) (11.5, 7.2) (25.5, 7.2) (25.5, 3.7) (15, 0) cm, the first one's
 * config listener): "imported N: POINTS" (getPoints()), then startUndo("Import freeform fin set"),
 * setPoints(), stopUndo(), "after points N: POINTS length=L [other N: POINTS] undo=U redo=R", then the
 * document's undo ("undo ...") and redo ("redo ...").</li>
 * </ul>
 * POINTS are "(x,y) (x,y) ..." in m, every double as Double.toString() writes it (Java 17).
 * <p>
 * Run it through generate.sh, which compiles it with swing's ServicesForTesting against OpenRocket's
 * compiled core and swing classes.
 */
public class FinImportDumper {
	/** How long a walk may take before it counts as one that never ends (it fills the heap long before). */
	private static final long WALK_TIMEOUT_MS = 60000;

	private static PrintStream out;

	public static void main(String[] args) throws Exception {
		File dir = new File(args[0]);
		dir.mkdirs();
		out = new PrintStream(System.out, true, StandardCharsets.UTF_8);
		Application.setInjector(Guice.createInjector(new ServicesForTesting(), new PluginModule()));
		for (String name : makeImages(dir)) {
			image(new File(dir, name));
		}
		image(new File(dir, "missing.png"));
		optimize();
		greys();
		imports(dir);
		out.flush();
		System.exit(0);
	}

	// ----------------------------------------------------------------------------- the images

	private static List<String> makeImages(File dir) throws Exception {
		java.util.ArrayList<String> names = new java.util.ArrayList<>();
		// The three images of the M2 scouting (config-dialogs, FinImageProbe), byte for byte.
		BufferedImage a = blank(80, 50);
		fill(a, new int[] {5, 25, 65, 45}, new int[] {50, 10, 10, 50});
		names.add(write(dir, "trapezoid", a));
		BufferedImage b = blank(40, 30);
		fill(b, new int[] {5, 5, 35, 35}, new int[] {30, 10, 10, 30});
		names.add(write(dir, "rectangle", b));
		BufferedImage c = blank(20, 20);
		fill(c, new int[] {2, 2, 10, 10}, new int[] {15, 5, 5, 15});
		names.add(write(dir, "floating", c));

		// No dark pixel at all.
		names.add(write(dir, "white", blank(10, 10)));

		// The threshold (int)(0.299 r + 0.587 g + 0.114 b) > 200 (white): notches 3 px wide and 4 px deep in the
		// top of a black rectangle, of grey 200 (200.0: black), grey 201 (201.0: white), (149, 217, 255) (exactly
		// 201, but 200.99999999999997 in doubles: black) and grey 202 (201.99999999999997: white).
		BufferedImage t = blank(40, 30);
		rect(t, 5, 10, 30, 20, 0x000000);
		rect(t, 8, 10, 3, 4, 0xC8C8C8);
		rect(t, 13, 10, 3, 4, 0xC9C9C9);
		rect(t, 18, 10, 3, 4, (149 << 16) | (217 << 8) | 255);
		rect(t, 23, 10, 3, 4, 0xCACACA);
		names.add(write(dir, "threshold", t));

		// Colours: a red fin (76) on yellow (225), a blue block (29) at its right, a notch of light cyan (238.6).
		BufferedImage col = new BufferedImage(40, 30, BufferedImage.TYPE_INT_RGB);
		rect(col, 0, 0, 40, 30, 0xFFFF00);
		Graphics2D g = col.createGraphics();
		g.setColor(new Color(255, 0, 0));
		g.fillPolygon(new Polygon(new int[] {4, 4, 30, 36}, new int[] {30, 6, 6, 30}, 4));
		g.dispose();
		rect(col, 30, 20, 8, 10, 0x0000FF);
		rect(col, 12, 6, 5, 6, 0xC8FFFF);
		names.add(write(dir, "colours", col));

		// An 8-bit greyscale PNG (TYPE_BYTE_GRAY), its levels set in the raster: background 255, the fin 100,
		// notches of 148, 149, 180, 201 and 230 in its top. getRGB() converts the levels from linear grey to sRGB
		// (100 -> 168, 148 -> 200, 149 -> 201): every notch but 148 is white.
		BufferedImage gray = new BufferedImage(40, 30, BufferedImage.TYPE_BYTE_GRAY);
		level(gray, 0, 0, 40, 30, 255);
		level(gray, 5, 10, 30, 20, 100);
		level(gray, 8, 10, 3, 4, 148);
		level(gray, 13, 10, 3, 4, 149);
		level(gray, 18, 10, 3, 4, 180);
		level(gray, 23, 10, 3, 4, 201);
		level(gray, 28, 10, 3, 4, 230);
		names.add(write(dir, "greyscale", gray));

		// A 16-bit greyscale PNG (TYPE_USHORT_GRAY): background 65535, the fin 25700, notches of 30000, 38064
		// (sRGB 200: black), 38065 (sRGB 201: white) and 50000.
		BufferedImage gray16 = new BufferedImage(40, 30, BufferedImage.TYPE_USHORT_GRAY);
		level(gray16, 0, 0, 40, 30, 65535);
		level(gray16, 5, 10, 30, 20, 25700);
		level(gray16, 8, 10, 3, 4, 30000);
		level(gray16, 13, 10, 3, 4, 38064);
		level(gray16, 18, 10, 3, 4, 38065);
		level(gray16, 23, 10, 3, 4, 50000);
		names.add(write(dir, "greyscale16", gray16));

		// A 4-bit greyscale PNG (an IndexColorModel of 16 greys, 17 apart, as ImageIO reads it back): background
		// 15 (255), the fin 6 (102), notches of 8 (136), 11 (187: black) and 12 (204: white).
		BufferedImage gray4 = greys(40, 30, 4);
		level(gray4, 0, 0, 40, 30, 15);
		level(gray4, 5, 10, 30, 20, 6);
		level(gray4, 8, 10, 3, 4, 8);
		level(gray4, 13, 10, 3, 4, 11);
		level(gray4, 18, 10, 3, 4, 12);
		names.add(write(dir, "greyscale4", gray4));

		// An 8-bit greyscale PNG with alpha (opaque everywhere): background 255, the fin 100, notches of 149, 201,
		// 149 and 230 where threshold.png has its four, converted to sRGB by getRGB() as the plain greyscale (149
		// -> 201: every notch is white).
		ComponentColorModel greyAlpha = new ComponentColorModel(ColorSpace.getInstance(ColorSpace.CS_GRAY), true,
				false, Transparency.TRANSLUCENT, DataBuffer.TYPE_BYTE);
		WritableRaster raster = greyAlpha.createCompatibleWritableRaster(40, 30);
		BufferedImage ga = new BufferedImage(greyAlpha, raster, false, null);
		greyAlphaLevel(ga, 0, 0, 40, 30, 255);
		greyAlphaLevel(ga, 5, 10, 30, 20, 100);
		greyAlphaLevel(ga, 8, 10, 3, 4, 149);
		greyAlphaLevel(ga, 13, 10, 3, 4, 201);
		greyAlphaLevel(ga, 18, 10, 3, 4, 149);
		greyAlphaLevel(ga, 23, 10, 3, 4, 230);
		names.add(write(dir, "greyscale-alpha", ga));

		// Transparency (TYPE_INT_ARGB, an RGBA PNG): background transparent white, the fin transparent black on
		// its left half and opaque black on its right, a notch of quarter-opaque white and one of opaque grey 128.
		BufferedImage tr = new BufferedImage(40, 30, BufferedImage.TYPE_INT_ARGB);
		argb(tr, 0, 0, 40, 30, 0x00FFFFFF);
		argb(tr, 5, 10, 15, 20, 0x00000000);
		argb(tr, 20, 10, 15, 20, 0xFF000000);
		argb(tr, 25, 10, 4, 4, 0x40FFFFFF);
		argb(tr, 9, 10, 3, 4, 0xFF808080);
		names.add(write(dir, "transparent", tr));

		// The usual transparent background: transparent black everywhere, an opaque black fin.
		BufferedImage tb = new BufferedImage(30, 20, BufferedImage.TYPE_INT_ARGB);
		argb(tb, 0, 0, 30, 20, 0x00000000);
		argb(tb, 5, 8, 15, 12, 0xFF000000);
		names.add(write(dir, "transparent-background", tb));

		// Palettes (TYPE_BYTE_INDEXED, palette PNGs), the indices set in the raster.
		// Without black: white, navy, grey 128, grey 230; the fin navy.
		BufferedImage pn = indexed(40, 30, new int[] {0xFFFFFFFF, 0xFF000080, 0xFF808080, 0xFFE6E6E6});
		level(pn, 0, 0, 40, 30, 0);
		level(pn, 5, 10, 30, 20, 1);
		names.add(write(dir, "palette-without-black", pn));
		// With black: white, black, red; the fin black with a red block.
		BufferedImage pb = indexed(40, 30, new int[] {0xFFFFFFFF, 0xFF000000, 0xFFFF0000});
		level(pb, 0, 0, 40, 30, 0);
		level(pb, 5, 10, 30, 20, 1);
		level(pb, 25, 10, 10, 10, 2);
		names.add(write(dir, "palette-with-black", pb));
		// With a transparent entry: transparent white (the background), black (the fin), opaque white.
		BufferedImage pt = indexed(40, 30, new int[] {0x00FFFFFF, 0xFF000000, 0xFFFFFFFF});
		level(pt, 0, 0, 40, 30, 0);
		level(pt, 5, 10, 30, 20, 1);
		names.add(write(dir, "palette-transparent", pt));

		// Walks.
		// A fin one pixel tall (only on the bottom row): the walk never leaves the bottom row.
		BufferedImage op = blank(30, 10);
		rect(op, 5, 9, 16, 1, 0x000000);
		names.add(write(dir, "one-pixel-tall", op));
		// A single pixel on the bottom row.
		BufferedImage sp = blank(10, 10);
		rect(sp, 4, 9, 1, 1, 0x000000);
		names.add(write(dir, "single-pixel", sp));
		// A column one pixel wide.
		BufferedImage cw = blank(10, 10);
		rect(cw, 4, 3, 1, 7, 0x000000);
		names.add(write(dir, "one-pixel-wide", cw));
		// Black everywhere.
		BufferedImage fb = new BufferedImage(12, 8, BufferedImage.TYPE_INT_RGB);
		rect(fb, 0, 0, 12, 8, 0x000000);
		names.add(write(dir, "full-black", fb));
		// One black pixel and nothing else: the walk never leaves it.
		names.add(write(dir, "one-by-one", new BufferedImage(1, 1, BufferedImage.TYPE_INT_RGB)));
		// The fin along the left, right and top edges of the image, with a bump touching the top.
		BufferedImage ed = blank(30, 20);
		rect(ed, 0, 3, 30, 17, 0x000000);
		rect(ed, 10, 0, 6, 3, 0x000000);
		names.add(write(dir, "edges", ed));
		// A right triangle whose hypotenuse climbs 1 px in 3: the simplification of a staircase.
		BufferedImage st = blank(90, 30);
		fill(st, new int[] {0, 0, 90}, new int[] {30, 0, 30});
		names.add(write(dir, "staircase", st));
		// An overhang: the tip reaches beyond the root.
		BufferedImage ov = blank(80, 45);
		fill(ov, new int[] {5, 20, 75, 75, 40, 40}, new int[] {45, 5, 5, 18, 18, 45});
		names.add(write(dir, "overhang", ov));
		// An arch on two legs: the walk ends at the second leg.
		BufferedImage ar = blank(50, 40);
		rect(ar, 5, 10, 40, 8, 0x000000);
		rect(ar, 5, 10, 6, 30, 0x000000);
		rect(ar, 39, 10, 6, 30, 0x000000);
		names.add(write(dir, "arch", ar));
		// A swept fin drawn anti-aliased (grey edges), with a hole, a second dark area on the bottom row and a
		// speck above.
		BufferedImage sw = blank(100, 60);
		Graphics2D s = sw.createGraphics();
		s.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
		s.setColor(Color.BLACK);
		Path2D.Double path = new Path2D.Double();
		path.moveTo(8, 60);
		path.quadTo(26, 6, 66, 8);
		path.lineTo(80, 10);
		path.lineTo(70, 36);
		path.lineTo(74, 60);
		path.closePath();
		s.fill(path);
		s.setColor(Color.WHITE);
		s.fill(new Ellipse2D.Double(40, 26, 12, 10));
		s.setColor(Color.BLACK);
		s.fillRect(86, 52, 8, 8);
		s.fillRect(90, 4, 3, 3);
		s.dispose();
		names.add(write(dir, "swept", sw));

		// TIFF files with alpha (TYPE_INT_ARGB; ImageIO's TIFF writer stores the alpha unassociated, its reader
		// gives it back so). A transparent white background, an opaque black fin: the size of full-black.png.
		BufferedImage tw = new BufferedImage(12, 8, BufferedImage.TYPE_INT_ARGB);
		argb(tw, 0, 0, 12, 8, 0x00FFFFFF);
		argb(tw, 3, 3, 6, 5, 0xFF000000);
		names.add(write(dir, "transparent-white", "tiff", tw));
		// An opaque white background, an opaque black fin, white of alpha 100 on the bottom row left of the fin
		// and in a notch in its top.
		BufferedImage st2 = new BufferedImage(30, 20, BufferedImage.TYPE_INT_ARGB);
		argb(st2, 0, 0, 30, 20, 0xFFFFFFFF);
		argb(st2, 5, 8, 15, 12, 0xFF000000);
		argb(st2, 4, 19, 1, 1, 0x64FFFFFF);
		argb(st2, 9, 8, 3, 4, 0x64FFFFFF);
		names.add(write(dir, "semi-transparent", "tiff", st2));

		// Not images: text, and the first 60 bytes of trapezoid.png.
		Files.writeString(new File(dir, "not-an-image.png").toPath(), "This is not an image.\n",
				StandardCharsets.US_ASCII);
		names.add("not-an-image.png");
		byte[] bytes = Files.readAllBytes(new File(dir, "trapezoid.png").toPath());
		Files.write(new File(dir, "truncated.png").toPath(), Arrays.copyOf(bytes, 60));
		names.add("truncated.png");
		return names;
	}

	private static BufferedImage blank(int w, int h) {
		BufferedImage img = new BufferedImage(w, h, BufferedImage.TYPE_INT_RGB);
		Graphics2D g = img.createGraphics();
		g.setColor(Color.WHITE);
		g.fillRect(0, 0, w, h);
		g.dispose();
		return img;
	}

	private static void fill(BufferedImage img, int[] xs, int[] ys) {
		Graphics2D g = img.createGraphics();
		g.setColor(Color.BLACK);
		g.fillPolygon(new Polygon(xs, ys, xs.length));
		g.dispose();
	}

	private static void rect(BufferedImage img, int x0, int y0, int w, int h, int rgb) {
		argb(img, x0, y0, w, h, 0xFF000000 | rgb);
	}

	private static void argb(BufferedImage img, int x0, int y0, int w, int h, int argb) {
		for (int x = x0; x < x0 + w; x++) {
			for (int y = y0; y < y0 + h; y++) {
				img.setRGB(x, y, argb);
			}
		}
	}

	/** Sets the samples (a level or a palette index) of a one-band image. */
	private static void level(BufferedImage img, int x0, int y0, int w, int h, int level) {
		for (int x = x0; x < x0 + w; x++) {
			for (int y = y0; y < y0 + h; y++) {
				img.getRaster().setSample(x, y, 0, level);
			}
		}
	}

	private static void greyAlphaLevel(BufferedImage img, int x0, int y0, int w, int h, int level) {
		for (int x = x0; x < x0 + w; x++) {
			for (int y = y0; y < y0 + h; y++) {
				img.getRaster().setSample(x, y, 0, level);
				img.getRaster().setSample(x, y, 1, 255);
			}
		}
	}

	private static BufferedImage indexed(int w, int h, int[] argb) {
		byte[] r = new byte[argb.length];
		byte[] gr = new byte[argb.length];
		byte[] bl = new byte[argb.length];
		byte[] al = new byte[argb.length];
		for (int i = 0; i < argb.length; i++) {
			al[i] = (byte) (argb[i] >>> 24);
			r[i] = (byte) (argb[i] >> 16);
			gr[i] = (byte) (argb[i] >> 8);
			bl[i] = (byte) argb[i];
		}
		IndexColorModel model = new IndexColorModel(8, argb.length, r, gr, bl, al);
		return new BufferedImage(w, h, BufferedImage.TYPE_BYTE_INDEXED, model);
	}

	/** An image of 2^bits greys (ImageIO writes it as a greyscale PNG of that depth). */
	private static BufferedImage greys(int w, int h, int bits) {
		int n = 1 << bits;
		byte[] levels = new byte[n];
		for (int i = 0; i < n; i++) {
			levels[i] = (byte) (i * 255 / (n - 1));
		}
		IndexColorModel model = new IndexColorModel(bits, n, levels, levels, levels);
		return new BufferedImage(w, h, BufferedImage.TYPE_BYTE_BINARY, model);
	}

	private static String write(File dir, String name, BufferedImage img) throws Exception {
		return write(dir, name, "png", img);
	}

	private static String write(File dir, String name, String format, BufferedImage img) throws Exception {
		File f = new File(dir, name + "." + format);
		if (!ImageIO.write(img, format, f)) {
			throw new IllegalStateException("no " + format + " writer for " + name);
		}
		return f.getName();
	}

	// ----------------------------------------------------------------------------- "image"

	private static void image(File f) throws Exception {
		out.println("== image " + f.getName());
		try {
			BufferedImage img = ImageIO.read(f);
			out.println(img == null ? "read null"
					: "read type=" + img.getType() + " size=" + img.getWidth() + "x" + img.getHeight());
		} catch (Exception e) {
			out.println("read " + e.getClass().getSimpleName() + ": " + e.getLocalizedMessage());
		}
		out.println(timed("raw", () -> rawWalk(f)));
		out.println(timed("points", () -> text(new CustomFinImporter().getPoints(f))));
	}

	/** validateImage() and loadFin() as getPoints() calls them, without optimizePoints(). */
	private static String rawWalk(File f) throws Exception {
		CustomFinImporter importer = new CustomFinImporter();
		BufferedImage pic = ImageIO.read(f);
		if (pic == null) {
			return "unreadable";
		}
		Field startX = CustomFinImporter.class.getDeclaredField("startX");
		startX.setAccessible(true);
		startX.setInt(importer, -1);
		Field facing = CustomFinImporter.class.getDeclaredField("facing");
		facing.setAccessible(true);
		Class<?> directions = Class.forName("info.openrocket.swing.gui.util.CustomFinImporter$FacingDirections");
		for (Object direction : directions.getEnumConstants()) {
			if (direction.toString().equals("UP")) {
				facing.set(importer, direction);
			}
		}
		Method validate = CustomFinImporter.class.getDeclaredMethod("validateImage", BufferedImage.class);
		validate.setAccessible(true);
		if (!(Boolean) validate.invoke(importer, pic)) {
			return "invalid";
		}
		ArrayList<CoordinateIF> points = new ArrayList<>();
		points.add(Coordinate.NUL);
		Method load = CustomFinImporter.class.getDeclaredMethod("loadFin", BufferedImage.class, ArrayList.class);
		load.setAccessible(true);
		load.invoke(importer, pic, points);
		return text(points);
	}

	/**
	 * "PREFIX " and what @p call gives, run in a thread of its own: its text, "error CLASS: MESSAGE" for an
	 * exception, or "never ends" for a walk that fills the heap or runs longer than WALK_TIMEOUT_MS.
	 */
	private static String timed(String prefix, Callable<String> call) throws Exception {
		final String[] result = {null};
		Thread worker = new Thread(() -> {
			try {
				result[0] = call.call();
			} catch (Throwable e) {
				Throwable cause = e instanceof InvocationTargetException ? e.getCause() : e;
				if (cause instanceof OutOfMemoryError) {
					System.err.println(prefix + ": OutOfMemoryError (the walk filled the heap with points)");
					result[0] = "never ends";
				} else {
					result[0] = "error " + cause.getClass().getSimpleName() + ": " + cause.getLocalizedMessage();
				}
			}
		});
		worker.setDaemon(true);
		worker.start();
		worker.join(WALK_TIMEOUT_MS);
		if (worker.isAlive()) {
			System.err.println(prefix + ": still running after " + WALK_TIMEOUT_MS + " ms");
			return prefix + " never ends";
		}
		return prefix + " " + result[0];
	}

	private static String text(List<CoordinateIF> points) {
		StringBuilder sb = new StringBuilder();
		sb.append(points.size()).append(':');
		for (CoordinateIF p : points) {
			sb.append(" (").append(p.getX()).append(',').append(p.getY()).append(')');
		}
		return sb.toString();
	}

	// ----------------------------------------------------------------------------- "optimize"

	private static void optimize() throws Exception {
		// Duplicates and straight runs: NUL, its duplicate, up the left side, along the top, down the right.
		optimizeCase("square", 0, 0, 0, 0, 0, 0.001, 0, 0.002, 0.001, 0.002, 0.002, 0.002, 0.002, 0.001, 0.002, 0);
		// A middle point 0.8 mm from the line (the tolerance), and 0.81 mm.
		optimizeCase("tolerance", 0, 0, 0.005, 0.0008, 0.01, 0);
		optimizeCase("beyond", 0, 0, 0.005, 0.00081, 0.01, 0);
		// A point behind the start and one beyond the end (closestPointOnSegment()'s ends).
		optimizeCase("ends", 0, 0, -0.0005, 0.0002, 0.004, 0.0001, 0.0105, 0.0003, 0.01, 0);
		// A point 2 mm behind the start, 2 mm past the end, close to the line through them: the distance is the
		// one to the end of the segment, not to the line.
		optimizeCase("behind", 0, 0, -0.002, 0.0001, 0.01, 0);
		optimizeCase("past", 0, 0, 0.012, 0.0001, 0.01, 0);
		// A staircase: x up by 3 for every y.
		optimizeCase("stairs", 0, 0, 0.001, 0, 0.002, 0, 0.003, 0, 0.003, 0.001, 0.004, 0.001, 0.005, 0.001, 0.006,
				0.001, 0.006, 0.002, 0.007, 0.002, 0.008, 0.002, 0.009, 0.002, 0.009, 0.003);
		// A zigzag that one pass does not finish.
		optimizeCase("zigzag", 0, 0, 0.001, 0.0007, 0.002, 0.0014, 0.003, 0.0007, 0.004, 0.0021, 0.005, 0.0028,
				0.006, 0.0021, 0.007, 0.0035, 0.008, 0.0028, 0.009, 0.0042, 0.010, 0);
		// Two points; one point.
		optimizeCase("two", 0, 0, 0.01, 0);
		optimizeCase("one", 0, 0);
		// The last point equal to the first (a closed loop): the distances to a chord of length zero.
		optimizeCase("loop", 0, 0, 0.0004, 0.0003, 0.002, 0.003, 0.0003, 0.0002, 0, 0);
	}

	private static void optimizeCase(String name, double... xy) throws Exception {
		Method opt = CustomFinImporter.class.getDeclaredMethod("optimizePoints", ArrayList.class);
		opt.setAccessible(true);
		CustomFinImporter importer = new CustomFinImporter();
		ArrayList<CoordinateIF> once = points(xy);
		opt.invoke(importer, once);
		ArrayList<CoordinateIF> loop = points(xy);
		int count;
		int passes = 0;
		do {
			count = loop.size();
			opt.invoke(importer, loop);
			passes++;
		} while (count != loop.size());
		out.println("== optimize " + name);
		out.println("input " + text(points(xy)));
		out.println("once " + text(once));
		out.println("loop passes=" + passes + " " + text(loop));
	}

	/** The points (x, y, ...), each an object of its own, the first Coordinate.NUL for (0, 0) as in getPoints(). */
	private static ArrayList<CoordinateIF> points(double... xy) {
		ArrayList<CoordinateIF> points = new ArrayList<>();
		for (int i = 0; i + 1 < xy.length; i += 2) {
			if (i == 0 && xy[0] == 0 && xy[1] == 0) {
				points.add(Coordinate.NUL);
			} else {
				points.add(new Coordinate(xy[i], xy[i + 1]));
			}
		}
		return points;
	}

	// ----------------------------------------------------------------------------- "grey"

	private static void greys() throws Exception {
		Path work = Files.createTempDirectory("fin-import-greys");
		try {
			BufferedImage g8 = new BufferedImage(256, 1, BufferedImage.TYPE_BYTE_GRAY);
			for (int i = 0; i < 256; i++) {
				g8.getRaster().setSample(i, 0, 0, i);
			}
			greyLine("grey8", readBack(work, g8));
			BufferedImage g16 = new BufferedImage(256, 256, BufferedImage.TYPE_USHORT_GRAY);
			for (int i = 0; i < 65536; i++) {
				g16.getRaster().setSample(i % 256, i / 256, 0, i);
			}
			BufferedImage r16 = readBack(work, g16);
			StringBuilder sb = new StringBuilder("firsts");
			int last = -1;
			for (int i = 0; i < 65536; i++) {
				int v = r16.getRGB(i % 256, i / 256) & 0xFF;
				if (v != last) {
					sb.append(' ').append(v).append('@').append(i);
					last = v;
				}
			}
			out.println("== grey16");
			out.println("read type=" + r16.getType());
			out.println(sb);
			for (int bits : new int[] {1, 2, 4}) {
				BufferedImage gb = greys(1 << bits, 1, bits);
				for (int i = 0; i < 1 << bits; i++) {
					gb.getRaster().setSample(i, 0, 0, i);
				}
				greyLine("grey" + bits, readBack(work, gb));
			}
			ComponentColorModel model = new ComponentColorModel(ColorSpace.getInstance(ColorSpace.CS_GRAY), true,
					false, Transparency.TRANSLUCENT, DataBuffer.TYPE_BYTE);
			BufferedImage ga = new BufferedImage(model, model.createCompatibleWritableRaster(256, 1), false, null);
			greyAlphaLevel(ga, 0, 0, 256, 1, 0);
			for (int i = 0; i < 256; i++) {
				ga.getRaster().setSample(i, 0, 0, i);
			}
			greyLine("grey-alpha8", readBack(work, ga));
		} finally {
			try (var files = Files.list(work)) {
				for (Path p : (Iterable<Path>) files::iterator) {
					Files.delete(p);
				}
			}
			Files.delete(work);
		}
	}

	private static BufferedImage readBack(Path work, BufferedImage img) throws Exception {
		File f = work.resolve("grey.png").toFile();
		ImageIO.write(img, "png", f);
		return ImageIO.read(f);
	}

	private static void greyLine(String name, BufferedImage img) {
		StringBuilder sb = new StringBuilder("srgb");
		for (int i = 0; i < img.getWidth(); i++) {
			sb.append(' ').append(img.getRGB(i, 0) & 0xFF);
		}
		out.println("== " + name);
		out.println("read type=" + img.getType());
		out.println(sb);
	}

	// ----------------------------------------------------------------------------- "import"

	private static void imports(File dir) throws Exception {
		for (String name : new String[] {"trapezoid", "rectangle", "swept", "full-black", "staircase", "overhang",
				"edges", "single-pixel", "one-pixel-wide"}) {
			importInto(dir, name, false);
		}
		importInto(dir, "trapezoid", true);
	}

	private static void importInto(File dir, String name, boolean multi) throws Exception {
		FreeformFinSet fins = new FreeformFinSet();
		FreeformFinSet other = new FreeformFinSet();
		other.setPoints(new CoordinateIF[] {new Coordinate(0, 0), new Coordinate(0.115, 0.072),
				new Coordinate(0.255, 0.072), new Coordinate(0.255, 0.037), new Coordinate(0.150, 0)});
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		body.setOuterRadiusAutomatic(false);
		body.setOuterRadius(0.021);
		body.setLength(0.3);
		stage.addChild(body);
		body.addChild(fins);
		if (multi) {
			body.addChild(other);
		}
		rocket.enableEvents();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
		doc.clearUndo();
		if (multi) {
			fins.addConfigListener(other);
		}
		out.println("== import " + name + (multi ? " multi" : ""));
		ArrayList<CoordinateIF> points = new CustomFinImporter().getPoints(new File(dir, name + ".png"));
		out.println("imported " + text(points));
		// FreeformFinSetConfig.importImage()
		doc.startUndo("Import freeform fin set");
		fins.setPoints(points);
		doc.stopUndo();
		state("after", doc, fins, multi ? other : null);
		if (doc.isUndoAvailable()) {
			doc.undo();
		}
		state("undo", doc, fins, multi ? other : null);
		if (doc.isRedoAvailable()) {
			doc.redo();
		}
		state("redo", doc, fins, multi ? other : null);
	}

	private static void state(String step, OpenRocketDocument doc, FreeformFinSet fins, FreeformFinSet other) {
		FreeformFinSet current = (FreeformFinSet) doc.getRocket().findComponent(fins.getID());
		StringBuilder sb = new StringBuilder(step);
		sb.append(" points ").append(text(Arrays.asList(current.getFinPoints())));
		sb.append(" length=").append(current.getLength());
		if (other != null) {
			FreeformFinSet o = (FreeformFinSet) doc.getRocket().findComponent(other.getID());
			sb.append(" other ").append(text(Arrays.asList(o.getFinPoints())));
		}
		sb.append(" undo=").append(doc.isUndoAvailable() ? doc.getUndoDescription() : "-");
		sb.append(" redo=").append(doc.isRedoAvailable() ? doc.getRedoDescription() : "-");
		out.println(sb);
	}
}
