package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Tick;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.StateChangeListener;
import info.openrocket.swing.gui.scalefigure.AbstractScaleFigure;
import info.openrocket.swing.gui.scalefigure.RocketFigure;
import info.openrocket.swing.gui.scalefigure.RocketPanel;
import info.openrocket.swing.gui.scalefigure.ScaleSelector;

import javax.swing.JComboBox;
import java.awt.Dimension;
import java.awt.Point;
import java.awt.geom.Rectangle2D;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.List;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.motor.ThrustCurveMotor;

/**
 * Prints what QtRocket's G1 part (ScaleFigure, ScaleScrollArea, Ruler, ScaleSelector) must reproduce:
 * the ScaleSelector's texts, parsing and zoom steps, RocketFigure's fit, origin and canvas numbers,
 * and the ruler's ticks (Ruler.paintComponent's arithmetic, copied).
 */
public class ScaleProbe {
	static class Recording implements ScaleSelector.ZoomModel {
		double scale;
		boolean fit;
		String log = "";

		Recording(double scale, boolean fit) {
			this.scale = scale;
			this.fit = fit;
		}

		@Override
		public double getScale() {
			return scale;
		}

		@Override
		public boolean isFit() {
			return fit;
		}

		@Override
		public void setScale(double s) {
			log += " setScale(" + s + ")";
		}

		@Override
		public void setFit() {
			log += " setFit()";
		}

		@Override
		public void addChangeListener(StateChangeListener listener) {
		}

		@Override
		public void removeChangeListener(StateChangeListener listener) {
		}
	}

	public static void main(String[] args) throws Exception {
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(swingPrefs),
				new PluginModule()));
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
		selector();
		figures(args);
		rulers();
	}

	static void selector() throws Exception {
		System.out.println("== suffix [" + Application.getTranslator().get("ScaleSelector.lbl.ZoomFitSuffix") + "]");
		Method format = ScaleSelector.class.getDeclaredMethod("formatScaleText", double.class, boolean.class);
		format.setAccessible(true);
		Method larger = ScaleSelector.class.getDeclaredMethod("getNextLargerScale", double.class);
		larger.setAccessible(true);
		Method smaller = ScaleSelector.class.getDeclaredMethod("getNextSmallerScale", double.class);
		smaller.setAccessible(true);

		Recording initial = new Recording(1.0, true);
		ScaleSelector sel = new ScaleSelector(initial);
		JComboBox<String> combo = sel.getScaleSelectorCombo();
		StringBuilder items = new StringBuilder();
		for (int i = 0; i < combo.getItemCount(); i++) {
			items.append("[").append(combo.getItemAt(i)).append("]");
		}
		System.out.println("== items " + items);
		System.out.println("== initial(1.0,fit) [" + combo.getSelectedItem() + "]");
		double[][] models = {{0.5, 0}, {1.0, 0}, {2.5, 0}, {0.123456, 0}, {0.15, 1}, {Double.NaN, 0}};
		for (double[] m : models) {
			ScaleSelector s = new ScaleSelector(new Recording(m[0], m[1] != 0));
			System.out.println("== initial(" + m[0] + "," + (m[1] != 0) + ") [" + s.getScaleSelectorCombo().getSelectedItem() + "]");
		}

		String[] texts = {"250%", "250", " 75 % ", "100% (fit)", "100%", "100", "100 (fit)", "50% (fit)", "0.1",
				"0.4%", "0.5%", "0.6%", "abc", "", "NaN", "Infinity", "-Infinity", "-5", "1e3", "12.345", "1,5", "2.5d",
				"0x10p0", "100% (fit) ", " (fit)", "100.00001% (fit)", "100.01% (fit)", "99.995% (fit)", "%",
				"25%%", "1e-400", "1e400", "200 %", "+50", "100.0% (fit)"};
		for (String t : texts) {
			Recording r = new Recording(1.0, true);
			ScaleSelector s = new ScaleSelector(r);
			JComboBox<String> c = s.getScaleSelectorCombo();
			c.setSelectedItem(t);
			System.out.println("== text [" + t + "] ->" + r.log + " shows [" + c.getSelectedItem() + "]");
		}

		double[] scales = {0.001, 0.005, 0.1, 0.15, 0.2, 0.2001, 0.25, 0.3, 0.5, 0.55, 0.7, 0.75, 0.8, 0.9, 0.95,
				0.951, 1.0, 1.04, 1.06, 1.2, 1.5, 1.549, 1.55, 1.6, 1.95, 2.0, 2.05, 2.1, 2.5, 3.0, 3.04, 3.06, 10.0,
				0.1000001, 0.0999999, 0.2000001, 0.1999999, 1.0499999, 1.0500001, 0.7999999, 0.8000001};
		for (double s : scales) {
			System.out.println("== zoom " + s + " out " + larger.invoke(null, s) + " in " + smaller.invoke(null, s));
		}
		double[] values = {0.15, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 0.123456, 1 / 1.5, 0.001, 0.0005, 0.00049,
				0.0015, 0.0025, 1.0005, 1.0015, 0.995, 0.9995, 12.3456, 1e6, 1e12, -0.5, -0.0, 0.0, Double.NaN,
				Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, 1e307, 0.005, 1.0 / 3, 2.0 / 3, 0.0125, 0.0135};
		for (double v : values) {
			System.out.println("== format " + v + " [" + format.invoke(null, v, false) + "] fit [" + format.invoke(null, v, true) + "]");
		}
	}

	static Object field(Object o, String name) throws Exception {
		Field f = AbstractScaleFigure.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(o);
	}

	static String rect(Rectangle2D r) {
		return "(" + r.getX() + "," + r.getY() + "," + r.getWidth() + "," + r.getHeight() + ")";
	}

	static void figures(String[] files) throws Exception {
		RocketPanel.VIEW_TYPE[] views = {RocketPanel.VIEW_TYPE.SideView, RocketPanel.VIEW_TYPE.TopView,
				RocketPanel.VIEW_TYPE.BackView};
		int[][] visibles = {{760, 560}, {1000, 700}, {300, 900}, {61, 41}, {60, 40}, {59, 100}, {0, 100}};
		for (int fi = 0; fi <= files.length; fi++) {
			Rocket rocket;
			String name;
			if (fi == files.length) {
				rocket = new Rocket();
				name = "empty";
			} else {
				OpenRocketDocument doc = new GeneralRocketLoader(new File(files[fi])).load();
				rocket = doc.getRocket();
				name = new File(files[fi]).getName();
			}
			for (RocketPanel.VIEW_TYPE view : views) {
				for (int[] v : visibles) {
					RocketFigure fig = new RocketFigure(rocket);
					fig.setType(view);
					Dimension dim = new Dimension(v[0], v[1]);
					double fit = fig.getFitScale(dim);
					boolean changed = fig.scaleTo(dim);
					Dimension pref = fig.getPreferredSize();
					// The viewport lays the figure out at max(preferred, viewport): here the visible size + (3, 3).
					fig.setSize(Math.max(pref.width, v[0] + 3), Math.max(pref.height, v[1] + 3));
					fig.updateFigure();
					Point o = fig.getSubjectOrigin();
					System.out.println("== figure " + name + " " + view + " visible " + v[0] + "x" + v[1]
							+ " base " + (fig.getAbsoluteScale() / fig.getUserScale())
							+ " subject " + rect((Rectangle2D) field(fig, "subjectBounds_m"))
							+ " fit " + fit + " changed " + changed + " user " + fig.getUserScale()
							+ " abs " + fig.getAbsoluteScale() + " pref " + pref.width + "x" + pref.height
							+ " size " + fig.getWidth() + "x" + fig.getHeight() + " origin " + o.x + "," + o.y
							+ " zoompt " + fig.getAutoZoomPoint().x + "," + fig.getAutoZoomPoint().y);
				}
				// Explicit scales
				RocketFigure fig = new RocketFigure(rocket);
				fig.setType(view);
				Dimension dim = new Dimension(500, 400);
				double[] requests = {0.5, 0.504, 0.506, 2.0, Double.NaN, Double.POSITIVE_INFINITY, -1.0, 0.0, 10.0,
						11.0, 12.0, 1.0};
				StringBuilder sb = new StringBuilder();
				for (double r : requests) {
					boolean ch = fig.scaleTo(r, dim);
					sb.append(" ").append(r).append(ch ? ":Y" : ":N").append("=").append(fig.getUserScale());
				}
				fig.scaleTo(2.0, dim);
				Dimension pref = fig.getPreferredSize();
				fig.setSize(Math.max(pref.width, 503), Math.max(pref.height, 403));
				fig.updateFigure();
				Point o = fig.getSubjectOrigin();
				System.out.println("== explicit " + name + " " + view + sb + " | at 2.0 pref " + pref.width + "x"
						+ pref.height + " origin " + o.x + "," + o.y);
				// The same scale and a different visible size is a change
				RocketFigure f2 = new RocketFigure(rocket);
				f2.setType(view);
				boolean a = f2.scaleTo(1.0, new Dimension(500, 400));
				boolean b = f2.scaleTo(1.0, new Dimension(500, 400));
				boolean c = f2.scaleTo(1.0, new Dimension(501, 400));
				System.out.println("== same " + name + " " + view + " " + a + " " + b + " " + c);
			}
		}
	}

	// ScaleScrollPane.Ruler.paintComponent's arithmetic, copied
	static void rulers() {
		double[] scales = {3779.5296, 1000.0, 377.95296, 37.795296, 12345.678};
		String[] units = {"m", "cm", "mm", "in", "ft"};
		int[][] views = {{0, 0, 600, 400}, {150, 75, 600, 400}, {-30, 0, 300, 200}};
		int[][] origins = {{30, 200}, {437, 301}};
		for (double scale : scales) {
			for (String un : units) {
				Unit unit = UnitGroup.UNITS_LENGTH.getUnit(un);
				for (int[] area : views) {
					for (int[] origin : origins) {
						for (int orientation = 0; orientation < 2; orientation++) {
							int startpx = orientation == 0 ? area[0] : area[1];
							int endpx = orientation == 0 ? area[0] + area[2] : area[1] + area[3];
							double start = (orientation == 0 ? startpx - origin[0] : origin[1] - startpx) / scale;
							double end = (orientation == 0 ? endpx - origin[0] : origin[1] - endpx) / scale;
							double minor = 3 / scale;
							double major = 30 / scale;
							Tick[] ticks = orientation == 1 ? unit.getTicks(end, start, minor, major)
									: unit.getTicks(start, end, minor, major);
							StringBuilder sb = new StringBuilder();
							for (Tick t : ticks) {
								int px = (int) (t.value * scale + 0.5);
								int pos = orientation == 0 ? px + origin[0] : origin[1] - px;
								int length = t.major ? 10 : (t.notable ? 6 : 3);
								sb.append(" ").append(pos).append(":").append(length);
								if (t.major) {
									sb.append(":").append(unit.toString(t.value)).append(t.notable ? ":b" : "");
								}
							}
							System.out.println("== ruler " + scale + " " + un + " view " + area[0] + "," + area[1] + ","
									+ area[2] + "," + area[3] + " origin " + origin[0] + "," + origin[1]
									+ (orientation == 0 ? " H" : " V") + " n=" + ticks.length + sb);
						}
					}
				}
			}
		}
	}
}
