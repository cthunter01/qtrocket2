package probe;

import java.awt.Color;
import java.awt.Image;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import java.util.prefs.Preferences;

import javax.swing.JComboBox;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.PlotAppearance;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.SimulationAbort;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.LineStyle;
import info.openrocket.core.util.LinearInterpolator;
import info.openrocket.core.util.ORColor;
import info.openrocket.swing.gui.plot.Axis;
import info.openrocket.swing.gui.plot.Plot;
import info.openrocket.swing.gui.plot.PlotPanel;
import info.openrocket.swing.gui.plot.SimulationPlot;
import info.openrocket.swing.gui.plot.SimulationPlotConfiguration;
import info.openrocket.swing.gui.plot.Util;
import info.openrocket.swing.gui.simulation.SimulationPlotPanel;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.utils.DecimalFormatter;

import org.jfree.chart.JFreeChart;
import org.jfree.chart.LegendItem;
import org.jfree.chart.LegendItemCollection;
import org.jfree.chart.annotations.XYTitleAnnotation;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.chart.labels.XYToolTipGenerator;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.xy.XYItemRenderer;
import org.jfree.chart.title.TextTitle;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

/**
 * Milestone 4 run 1, part P2: what QtRocket's core plot model (plot/SimulationPlotModel, plot/PlotUtil,
 * plot/PlotPreferences, util/DecimalFormatter, util/DecimalFormat::toPattern()) ports from OpenRocket's swing
 * classes, on OpenRocket's core with ServicesForTesting, SwingPreferences and the bundled motor database
 * (data/motors/initial_motors.db), headless:
 * <ul>
 * <li>DecimalFormat.toPattern() and the digit counts of the patterns QtRocket's DecimalFormat reads;</li>
 * <li>DecimalFormatter.df() for values about the powers of ten, signs, zeros and the special values, also the
 * subnormal values for which it throws;</li>
 * <li>Util.generateSeriesLabels() for lists of branch names, also one for which it throws;</li>
 * <li>SwingPreferences' default plot appearances: what setDefaultPlotAppearance() stores in the node
 * PlotDefaultAppearances and what getDefaultPlotAppearance() reads from stored texts;</li>
 * <li>the plots of stored example simulations (data/examples) through OpenRocket's own SimulationPlotPanel
 * (its preset combo box) and SimulationPlot: the title, the axes with their labels and ranges (the domain before
 * the events are drawn), the series with their colours as set (Plot) and as drawn (bug 42), the colours the panel
 * shows, the legend, read-out tool tips and the multi-stage series names, the events, and the event groups, the
 * domain widening and the icons replayed in eventList order, each group at its first event (bug 41: Java's own
 * order is a HashSet's), with their tool tips through Plot.formatTooltip();</li>
 * <li>the abort annotation of each branch of a copy of a simulation's data with an abort in every branch.</li>
 * </ul>
 * tests/core/plot/SimulationPlotModelTests.cpp, PlotUtilTests.cpp, PlotPreferencesTests.cpp and
 * tests/core/util/DecimalFormatterTests.cpp, DecimalFormatTests.cpp pin what it prints by hand (expected.txt; see
 * ../README.md). Every double of the plots is printed with Double.toString and, in braces, Double.toHexString.
 */
public class PlotModelProbe {
	static final Map<Image, String> IMAGE_NAMES = new java.util.HashMap<>();

	public static void main(String[] args) throws Exception {
		bootstrap();
		collectImageNames();
		decimalFormatPatterns();
		decimalFormatter();
		seriesLabels();
		appearances();
		Path examples = Path.of(System.getProperty("qtrocket.repo")).resolve("data").resolve("examples");
		plots(examples.resolve("A simple model rocket.ork"), 0, new int[] { 0, 2, 4, 5 });
		plots(examples.resolve("Three stage low power rocket.ork"), 0, new int[] { 0, 2 });
		plots(examples.resolve("3D printable nose cone and fins.ork"), 2, new int[] { 0, 2 });
		aborts(examples.resolve("Three stage low power rocket.ork"), 0);
	}

	static String d(double v) {
		return v + "{" + Double.toHexString(v) + "}";
	}

	// ---------------------------------------------------------------------------------------------
	// DecimalFormat.toPattern()

	static void decimalFormatPatterns() {
		String[] patterns = { "#", "##", "0", "00", "000", "#0", "##0", "0.0", "0.#", "0.0##", "#.###", "0.00", "0.##",
				"#.##", "#.00", "#.000", "#.#", "##0.#", "00.##", "#0.0#", "#.####", ".##", ".00", ".0#", "#.", "0.",
				"0.00E0", "##0.##E0", "0.###E0", "00.0E00", "#E0", "0E0", "#.#E0", "#0.0E0" };
		for (String p : patterns) {
			try {
				DecimalFormat f = new DecimalFormat(p);
				System.out.println("PATTERN \"" + p + "\" -> \"" + f.toPattern() + "\" int=" + f.getMinimumIntegerDigits()
						+ ".." + f.getMaximumIntegerDigits() + " fraction=" + f.getMinimumFractionDigits() + ".."
						+ f.getMaximumFractionDigits() + " pointAlwaysShown=" + f.isDecimalSeparatorAlwaysShown()
						+ " grouping=" + f.isGroupingUsed() + "/" + f.getGroupingSize());
			} catch (RuntimeException e) {
				System.out.println("PATTERN \"" + p + "\" -> " + e.getClass().getSimpleName() + ": " + e.getMessage());
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// DecimalFormatter.df()

	static void decimalFormatter() {
		double[] values = { 12.34, 8.0, -8.75, 123.456, 8.5, 0.00345, 0.0, -0.0, 0.1, 0.09999999999999999, 0.099,
				0.05, 0.010000000000000002, 0.01, 0.009999999999999998, 0.001, 9.999E-4, 2.6E-4, -2.599E-4, 1.0E-5,
				-0.00345, 25.625, 0.125, 1234567.891, 1.0E-300, 1.0E-308, 2.2250738585072014E-308, 1.0E-310, 4.9E-324,
				Double.NaN, Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY };
		for (int decimals : new int[] { 2, 3 }) {
			for (boolean zeros : new boolean[] { false, true }) {
				for (double v : values) {
					df(v, decimals, zeros);
				}
			}
		}
		for (int decimals : new int[] { -1, 0, 1, 4 }) {
			for (double v : new double[] { 12.34, 0.00345, 0.0, 0.5, 0.05, 1.0E-310 }) {
				df(v, decimals, false);
				df(v, decimals, true);
			}
		}
	}

	static void df(double v, int decimals, boolean zeros) {
		String head = "DF " + v + " " + decimals + " " + zeros + " -> ";
		try {
			DecimalFormat f = DecimalFormatter.df(v, decimals, zeros);
			System.out.println(head + "\"" + f.toPattern() + "\" fraction=" + f.getMinimumFractionDigits() + ".."
					+ f.getMaximumFractionDigits() + " format=\"" + f.format(v) + "\"");
		} catch (RuntimeException e) {
			System.out.println(head + e.getClass().getSimpleName() + ": " + e.getMessage());
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Util.generateSeriesLabels()

	static void seriesLabels() {
		String[][] cases = { {}, { "Sustainer" }, { "Sustainer", "Booster stage", "Booster stage" },
				{ "A", "B", "A" }, { "A", "A", "A" }, { "A", "A", "B", "B", "A" }, { "<b>Stage</b>", "Stage" },
				{ "a<b", "a<b" }, { "", "" }, { "A(1)", "A", "A" }, { "A", "A(1)", "A" }, { "A", "A", "A(1)" },
				{ "Sustainer", "Booster<i> stage</i>", "Booster stage" } };
		for (String[] names : cases) {
			List<FlightDataBranch> branches = new ArrayList<>();
			for (String n : names) {
				branches.add(new FlightDataBranch(n, FlightDataType.TYPE_TIME));
			}
			String head = "LABELS " + quoted(Arrays.asList(names)) + " -> ";
			try {
				System.out.println(head + quoted(Util.generateSeriesLabels(branches)));
			} catch (RuntimeException e) {
				System.out.println(head + e.getClass().getSimpleName() + ": " + e.getMessage());
			}
		}
	}

	static String quoted(List<String> list) {
		StringBuilder sb = new StringBuilder("[");
		for (int i = 0; i < list.size(); i++) {
			sb.append(i == 0 ? "" : ", ").append('"').append(list.get(i)).append('"');
		}
		return sb.append(']').toString();
	}

	// ---------------------------------------------------------------------------------------------
	// SwingPreferences.getDefaultPlotAppearance() and setDefaultPlotAppearance()

	static void appearances() throws Exception {
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		Preferences node = prefs.getNode("PlotDefaultAppearances");
		FlightDataType alt = FlightDataType.TYPE_ALTITUDE;
		FlightDataType vel = FlightDataType.TYPE_VELOCITY_Z;

		prefs.setDefaultPlotAppearance(alt, new PlotAppearance(new ORColor(1, 2, 3), LineStyle.DASHDOT));
		dumpNode("SET altitude 1,2,3 DASHDOT", node);
		System.out.println("  GET altitude " + appearance(prefs.getDefaultPlotAppearance(alt)));
		prefs.setDefaultPlotAppearance(alt, new PlotAppearance(new ORColor(10, 20, 30, 40), null));
		dumpNode("SET altitude 10,20,30,40 null", node);
		System.out.println("  GET altitude " + appearance(prefs.getDefaultPlotAppearance(alt)));
		prefs.setDefaultPlotAppearance(alt, new PlotAppearance(null, LineStyle.SOLID));
		dumpNode("SET altitude null SOLID", node);
		System.out.println("  GET altitude " + appearance(prefs.getDefaultPlotAppearance(alt)));
		prefs.setDefaultPlotAppearance(vel, new PlotAppearance(new ORColor(0, 114, 189), LineStyle.DOTTED));
		prefs.setDefaultPlotAppearance(alt, new PlotAppearance(null, null));
		dumpNode("SET altitude null null (vertical velocity 0,114,189 DOTTED)", node);
		System.out.println("  GET altitude " + appearance(prefs.getDefaultPlotAppearance(alt)));
		prefs.setDefaultPlotAppearance(vel, null);
		dumpNode("SET vertical velocity null", node);

		String[][] stored = { { "0,114,189", "DASHED" }, { "300,-5,20", null }, { null, "dashed" },
				{ "1,2", "SOLID" }, { "a,b,c", null }, { " 1,2,3", null }, { "1,2,3,", "DOTTED " },
				{ "1,2,3,4", "DASHDOT" }, { "", "" }, { null, "Solid" } };
		for (String[] s : stored) {
			if (s[0] != null) {
				node.put(alt.getName() + ".color", s[0]);
			} else {
				node.remove(alt.getName() + ".color");
			}
			if (s[1] != null) {
				node.put(alt.getName() + ".lineStyle", s[1]);
			} else {
				node.remove(alt.getName() + ".lineStyle");
			}
			System.out.println("STORED color=" + text(s[0]) + " lineStyle=" + text(s[1]) + " -> GET "
					+ appearance(prefs.getDefaultPlotAppearance(alt)));
		}
		node.clear();
	}

	static String text(String s) {
		return s == null ? "absent" : "\"" + s + "\"";
	}

	static void dumpNode(String title, Preferences node) throws Exception {
		System.out.println(title);
		String[] keys = node.keys();
		Arrays.sort(keys);
		for (String k : keys) {
			System.out.println("  " + k + "=" + node.get(k, null));
		}
	}

	static String appearance(PlotAppearance a) {
		if (a == null) {
			return "null";
		}
		ORColor c = a.getColor();
		return "color=" + (c == null ? "null" : c.getRed() + "," + c.getGreen() + "," + c.getBlue() + "," + c.getAlpha())
				+ " lineStyle=" + (a.getLineStyle() == null ? "null" : a.getLineStyle().name());
	}

	// ---------------------------------------------------------------------------------------------
	// The plots

	static void plots(Path file, int simIndex, int[] presets) throws Exception {
		OpenRocketDocument doc = new GeneralRocketLoader(file.toFile()).load();
		Simulation sim = doc.getSimulations().get(simIndex);
		List<FlightDataBranch> branches = sim.getSimulatedData().getBranches();
		System.out.println();
		StringBuilder names = new StringBuilder();
		for (FlightDataBranch b : branches) {
			names.append(" [").append(b.getName()).append(" length=").append(b.getLength()).append(" separation=")
					.append(b.getSeparationTime()).append(" index=")
					.append(b.getDataIndexOfTime(b.getSeparationTime())).append("]");
		}
		System.out.println("=== " + file.getFileName() + " simulation " + simIndex + " \"" + sim.getName()
				+ "\" branches " + branches.size() + ":" + names);
		System.out.println("SERIES-LABELS " + quoted(Util.generateSeriesLabels(branches)));

		SimulationPlotPanel panel = SimulationPlotPanel.create(sim);
		JComboBox<?> selector = (JComboBox<?>) field(PlotPanel.class, "configurationSelector").get(panel);
		Method colors = SimulationPlotPanel.class.getDeclaredMethod("computeDefaultPlotColors");
		colors.setAccessible(true);
		for (int k : presets) {
			SimulationPlotConfiguration preset = SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS[k];
			selector.setSelectedIndex(selector.getItemCount() - 1);
			selector.setSelectedItem(preset);
			SimulationPlotConfiguration config = panel.getConfiguration();
			System.out.println("--- preset " + k + " \"" + config.getName() + "\"");
			@SuppressWarnings("unchecked")
			Map<Integer, Color> panelColors = (Map<Integer, Color>) colors.invoke(panel);
			StringBuilder pc = new StringBuilder();
			for (int i = 0; i < config.getDataCount(); i++) {
				pc.append(i == 0 ? "" : " ").append(i).append("->").append(colorIndex(panelColors.get(i)));
			}
			System.out.println("PANEL-COLORS " + pc);
			SimulationPlot plot = SimulationPlot.create(sim, config, false);
			dumpChart(plot);
			dumpEvents(plot, sim, config);
		}
	}

	static String colorIndex(Color c) {
		if (c == null) {
			return "null";
		}
		for (int i = 0; i < 10; i++) {
			if (Util.getPlotColor(i).equals(c)) {
				return "c" + i;
			}
		}
		return String.format("#%02x%02x%02x", c.getRed(), c.getGreen(), c.getBlue());
	}

	static String colorIndex(Object paint) {
		return paint instanceof Color c ? colorIndex(c) : String.valueOf(paint);
	}

	static void dumpChart(SimulationPlot plot) throws Exception {
		JFreeChart chart = plot.getJFreeChart();
		XYPlot xy = chart.getXYPlot();
		System.out.println("TITLE \"" + chart.getTitle().getText() + "\" SUBTITLE \""
				+ ((TextTitle) chart.getSubtitle(0)).getText() + "\"");
		ValueAxis domain = xy.getDomainAxis();
		System.out.println("DOMAIN label=\"" + domain.getLabel() + "\" range [" + d(domain.getLowerBound()) + ", "
				+ d(domain.getUpperBound()) + "]");
		SimulationPlotConfiguration filled = filled(plot);
		StringBuilder fa = new StringBuilder();
		for (int i = 0; i < filled.getDataCount(); i++) {
			fa.append(i == 0 ? "" : " ").append(i).append("->").append(filled.getAxis(i));
		}
		List<Axis> all = filled.getAllAxes();
		System.out.println("FILLED " + fa + " | left [" + d(all.get(0).getMinValue()) + ", " + d(all.get(0).getMaxValue())
				+ "] right [" + d(all.get(1).getMinValue()) + ", " + d(all.get(1).getMaxValue()) + "]");
		for (int a = 0; a < xy.getDatasetCount(); a++) {
			XYSeriesCollection col = (XYSeriesCollection) xy.getDataset(a);
			if (col == null) {
				continue;
			}
			ValueAxis ra = xy.getRangeAxis(a);
			System.out.println("AXIS " + a + " label=\"" + ra.getLabel() + "\" range [" + d(ra.getLowerBound()) + ", "
					+ d(ra.getUpperBound()) + "] series " + col.getSeriesCount());
			XYItemRenderer r = xy.getRenderer(a);
			for (int s = 0; s < col.getSeriesCount(); s++) {
				XYSeries ser = col.getSeries(s);
				int n = ser.getItemCount();
				System.out.println("  SERIES " + s + " \"" + ser.getDescription() + "\" branch=" + call(ser, "getBranchIdx")
						+ " row=" + call(ser, "getDataIdx") + " unit='" + call(ser, "getUnit") + "' n=" + n + " set="
						+ colorIndex(r.getSeriesPaint(s)) + " drawn="
						+ colorIndex(((org.jfree.chart.renderer.AbstractRenderer) r).lookupSeriesPaint(s)) + " first="
						+ point(ser, 0) + " middle=" + point(ser, n / 2) + " last=" + point(ser, n - 1));
			}
			XYToolTipGenerator tg = r.getDefaultToolTipGenerator();
			for (int s = 0; s < col.getSeriesCount(); s++) {
				int n = col.getSeries(s).getItemCount();
				for (int item : new int[] { 0, n / 2, n - 1 }) {
					System.out.println("  TOOLTIP " + s + " " + item + ": " + tg.generateToolTip(col, s, item));
				}
			}
		}
		Object legendItems = field(Plot.class, "legendItems").get(plot);
		Method getItems = legendItems.getClass().getMethod("getLegendItems");
		getItems.setAccessible(true);
		LegendItemCollection li = (LegendItemCollection) getItems.invoke(legendItems);
		StringBuilder lg = new StringBuilder();
		for (int i = 0; i < li.getItemCount(); i++) {
			LegendItem it = li.get(i);
			lg.append(i == 0 ? "" : " | ").append('"').append(it.getLabel()).append("\" drawn=")
					.append(colorIndex(it.getLinePaint()));
		}
		System.out.println("LEGEND " + li.getItemCount() + ": " + lg);
	}

	static String point(XYSeries s, int i) {
		if (i < 0 || i >= s.getItemCount()) {
			return "-";
		}
		return d(s.getX(i).doubleValue()) + "," + d(s.getY(i).doubleValue());
	}

	static Object call(Object o, String method) throws Exception {
		Method m = null;
		for (Class<?> c = o.getClass(); c != null && m == null; c = c.getSuperclass()) {
			try {
				m = c.getDeclaredMethod(method);
			} catch (NoSuchMethodException e) {
				// the superclass
			}
		}
		m.setAccessible(true);
		return m.invoke(o);
	}

	static SimulationPlotConfiguration filled(SimulationPlot plot) throws Exception {
		return (SimulationPlotConfiguration) field(Plot.class, "filledConfig").get(plot);
	}

	/** One entry of SimulationPlot's eventList. */
	record Info(int stage, double time, FlightEvent event) {
	}

	static List<Info> eventList(SimulationPlot plot) throws Exception {
		List<Info> out = new ArrayList<>();
		for (Object info : (List<?>) field(SimulationPlot.class, "eventList").get(plot)) {
			out.add(new Info((int) field(info.getClass(), "stage").get(info),
					(double) field(info.getClass(), "time").get(info),
					(FlightEvent) field(info.getClass(), "event").get(info)));
		}
		return out;
	}

	/** One group of fillEventLists() with its events in eventList order, and the event it takes its look from. */
	record Group(List<Info> infos, Info look) {
	}

	/** SimulationPlot.fillEventLists() for @p branch (-1: all), each group's events in eventList order. */
	static List<Group> groups(List<Info> eventList, int branch) {
		List<Group> out = new ArrayList<>();
		List<Info> current = new ArrayList<>();
		Set<FlightEvent.Type> typeSet = new HashSet<>();
		double prevTime = -100;
		int maxOrdinal = -1;
		Info look = null;
		for (Info info : eventList) {
			if (branch >= 0 && branch != info.stage()) {
				continue;
			}
			double t = info.time();
			FlightEvent.Type type = info.event().getType();
			if (Math.abs(t - prevTime) <= 0.05) {
				if (!typeSet.contains(type)) {
					if (type.ordinal() > maxOrdinal) {
						maxOrdinal = type.ordinal();
						look = info;
					}
					typeSet.add(type);
					current.add(info);
				}
			} else {
				if (!current.isEmpty()) {
					out.add(new Group(current, look));
				}
				prevTime = t;
				typeSet.clear();
				typeSet.add(type);
				current = new ArrayList<>();
				current.add(info);
				maxOrdinal = type.ordinal();
				look = info;
			}
		}
		if (!current.isEmpty()) {
			out.add(new Group(current, look));
		}
		return out;
	}

	static String label(Group g) {
		StringBuilder sb = new StringBuilder();
		for (Info info : g.infos()) {
			sb.append(sb.length() == 0 ? "" : ", ").append(info.event().getType().toString());
		}
		return sb.toString();
	}

	static Object eventGraphics(String method, FlightEvent event) throws Exception {
		Method m = Class.forName("info.openrocket.swing.gui.plot.EventGraphics").getDeclaredMethod(method,
				FlightEvent.class);
		m.setAccessible(true);
		return m.invoke(null, event);
	}

	static String describe(Group g) throws Exception {
		StringBuilder events = new StringBuilder();
		for (Info info : g.infos()) {
			events.append(events.length() == 0 ? "" : " ").append(info.event().getType().name()).append('@')
					.append(info.time()).append("/b").append(info.stage());
		}
		FlightEvent look = g.look().event();
		return "t=" + d(g.infos().get(0).time()) + " look=" + look.getType().name() + " image="
				+ IMAGE_NAMES.get((Image) eventGraphics("getEventImage", look)) + " events=[" + events + "] label=\""
				+ label(g) + "\"";
	}

	static void dumpEvents(SimulationPlot plot, Simulation sim, SimulationPlotConfiguration config) throws Exception {
		List<Info> eventList = eventList(plot);
		StringBuilder el = new StringBuilder();
		for (Info info : eventList) {
			el.append(el.length() == 0 ? "" : " ").append(info.event().getType().name()).append('@')
					.append(info.time()).append("/b").append(info.stage());
		}
		System.out.println("EVENT-LIST " + eventList.size() + ": " + el);
		int branchCount = sim.getSimulatedData().getBranchCount();
		for (int b = -1; b < branchCount; b++) {
			List<Group> gs = groups(eventList, b);
			System.out.println("GROUPS " + (b < 0 ? "all" : "branch " + b) + " " + gs.size());
			for (Group g : gs) {
				System.out.println("  G " + describe(g));
			}
		}
		// plotVerticalLineMarkers()'s widening of the domain (the time domain only), the groups at their first
		// events.
		if (config.getDomainAxisType() == FlightDataType.TYPE_TIME) {
			ValueAxis domain = plot.getJFreeChart().getXYPlot().getDomainAxis();
			double lower = domain.getLowerBound();
			double upper = domain.getUpperBound();
			double markerWidth = 0.01 * upper;
			for (Group g : groups(eventList, -1)) {
				double t = g.infos().get(0).time();
				if (t > upper - markerWidth) {
					upper = t + markerWidth;
				}
			}
			System.out.println("DOMAIN-WITH-EVENT-LINES [" + d(lower) + ", " + d(upper) + "]");
		} else {
			System.out.println("DOMAIN-WITH-EVENT-LINES none (not the time domain)");
		}
		icons(plot, sim, config, eventList);
	}

	/** SimulationPlot.plotIconMarkers() for every branch from the last, the groups at their first events. */
	static void icons(SimulationPlot plot, Simulation sim, SimulationPlotConfiguration config, List<Info> eventList)
			throws Exception {
		SimulationPlotConfiguration filled = filled(plot);
		List<Axis> axes = filled.getAllAxes();
		XYSeriesCollection[] data = (XYSeriesCollection[]) field(Plot.class, "data").get(plot);
		Class<?> metadata = Class.forName("info.openrocket.swing.gui.plot.Plot$MetadataXYSeries");
		Method name = SimulationPlot.class.getDeclaredMethod("getNameBasedOnIdxAndSeries", metadata, int.class);
		name.setAccessible(true);
		Method format = Plot.class.getDeclaredMethod("formatTooltip", String.class, String.class, double.class,
				String.class, String.class, double.class, String.class, String.class, double.class, String.class,
				Set.class);
		format.setAccessible(true);
		int branchCount = sim.getSimulatedData().getBranchCount();
		System.out.println("ICONS (from the last branch)");
		for (int b = branchCount - 1; b >= 0; b--) {
			FlightDataBranch branch = sim.getSimulatedData().getBranch(b);
			List<Double> time = branch.getView(FlightDataType.TYPE_TIME);
			LinearInterpolator di = new LinearInterpolator(time, branch.getView(config.getDomainAxisType()));
			List<Group> gs = groups(eventList, b);
			for (int axisno = 0; axisno < data.length; axisno++) {
				double minLeft = axes.get(0).getMinValue();
				double maxLeft = axes.get(0).getMaxValue();
				double minThis = axes.get(axisno).getMinValue();
				double maxThis = axes.get(axisno).getMaxValue();
				double slope = (maxLeft - minLeft) / (maxThis - minThis);
				double intercept = (maxThis * minLeft - maxLeft * minThis) / (maxThis - minThis);
				for (int s = 0; s < data[axisno].getSeriesCount(); s++) {
					Object series = data[axisno].getSeries(s);
					if ((int) call(series, "getBranchIdx") != b) {
						continue;
					}
					int row = (int) call(series, "getDataIdx");
					FlightDataType type = config.getType(row);
					LinearInterpolator ri = new LinearInterpolator(time, branch.getView(type));
					for (Group g : gs) {
						double t = g.infos().get(0).time();
						double x = config.getDomainAxisUnit().toUnit(di.getValue(t));
						double y = config.getUnit(row).toUnit(ri.getValue(t));
						if (Double.isNaN(y)) {
							continue;
						}
						Optional<Double> closest = time.stream()
								.min(Comparator.comparingDouble(sample -> Math.abs(sample - t)));
						int sampleIdx = closest.map(time::indexOf).orElse(-1);
						Set<FlightEvent> events = new LinkedHashSet<>();
						for (Info info : g.infos()) {
							events.add(info.event());
						}
						String tooltip = (String) format.invoke(plot, name.invoke(plot, series, sampleIdx),
								FlightDataType.TYPE_TIME.getName(), t,
								FlightDataType.TYPE_TIME.getUnitGroup().getDefaultUnit().toString(),
								config.getDomainAxisType().getName(), x, config.getDomainAxisUnit().getUnit(),
								type.toString(), y, (String) call(series, "getUnit"), events);
						double yloc = slope * y + intercept;
						System.out.println("  I b" + b + " axis" + axisno + " series" + s + " row" + row + " t=" + d(t)
								+ " x=" + d(x) + " y=" + d(y) + " yloc=" + d(yloc) + " sample=" + sampleIdx + " image="
								+ IMAGE_NAMES.get((Image) eventGraphics("getEventImage", g.look().event()))
								+ (Double.isNaN(x) ? " (x NaN: no icon)" : "") + " tooltip=" + tooltip);
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The abort annotation

	static void aborts(Path file, int simIndex) throws Exception {
		OpenRocketDocument doc = new GeneralRocketLoader(file.toFile()).load();
		Simulation sim = doc.getSimulations().get(simIndex);
		List<FlightDataBranch> copies = new ArrayList<>();
		SimulationAbort.Cause[] causes = SimulationAbort.Cause.values();
		int b = 0;
		for (FlightDataBranch branch : sim.getSimulatedData().getBranches()) {
			FlightDataBranch copy = branch.clone();
			List<Double> time = copy.getView(FlightDataType.TYPE_TIME);
			double t = time.get(time.size() - 1) * 0.6;
			copy.addEvent(new FlightEvent(FlightEvent.Type.SIM_ABORT, t, null,
					new SimulationAbort(causes[(3 * b + 4) % causes.length])));
			if (b == 1) {
				copy.addEvent(new FlightEvent(FlightEvent.Type.SIM_ABORT, t + 1,
						null, new SimulationAbort(causes[0])));
			}
			copies.add(copy);
			b++;
		}
		Field f = Simulation.class.getDeclaredField("simulatedData");
		f.setAccessible(true);
		f.set(sim, new FlightData(copies.toArray(new FlightDataBranch[0])));
		System.out.println();
		System.out.println("=== aborts: " + file.getFileName() + " simulation " + simIndex
				+ ", a SIM_ABORT at 0.6 times each branch's last time, causes (3 b + 4) mod 10, branch 1 a second one"
				+ " a second later");
		for (FlightDataBranch branch : copies) {
			FlightEvent abort = branch.getFirstEvent(FlightEvent.Type.SIM_ABORT);
			System.out.println("BRANCH \"" + branch.getName() + "\" abort t=" + d(abort.getTime()) + " cause="
					+ ((SimulationAbort) abort.getData()).getCause().name());
		}
		SimulationPlot plot = SimulationPlot.create(sim, SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS[0], false);
		Object set = field(SimulationPlot.class, "errorAnnotations").get(plot);
		Method create = set.getClass().getDeclaredMethod("createAnnotation", int.class);
		create.setAccessible(true);
		for (int branch = -1; branch < copies.size(); branch++) {
			XYTitleAnnotation a = (XYTitleAnnotation) create.invoke(set, branch);
			System.out.println("ABORT " + (branch < 0 ? "all" : "branch " + branch) + ": "
					+ (a == null ? "none" : "\"" + ((TextTitle) a.getTitle()).getText().replace("\n", "\\n") + "\" at ("
							+ a.getX() + ", " + a.getY() + ")"));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Start-up

	static Field field(Class<?> cls, String name) throws Exception {
		Field f = cls.getDeclaredField(name);
		f.setAccessible(true);
		return f;
	}

	@SuppressWarnings("unchecked")
	static void collectImageNames() throws Exception {
		Class<?> graphics = Class.forName("info.openrocket.swing.gui.plot.EventGraphics");
		Map<FlightEvent.Type, Image> eventImages = (Map<FlightEvent.Type, Image>) field(graphics, "EVENT_IMAGES")
				.get(null);
		for (Map.Entry<FlightEvent.Type, Image> e : new TreeMap<>(eventImages).entrySet()) {
			IMAGE_NAMES.putIfAbsent(e.getValue(), "event:" + e.getKey().name());
		}
		Map<?, Image> messageImages = (Map<?, Image>) field(graphics, "MESSAGE_IMAGES").get(null);
		for (Map.Entry<?, Image> e : messageImages.entrySet()) {
			IMAGE_NAMES.putIfAbsent(e.getValue(), "warning:" + e.getKey());
		}
	}

	static void bootstrap() throws Exception {
		Path bundledDb = Path.of(System.getProperty("qtrocket.repo")).resolve("data").resolve("motors")
				.resolve("initial_motors.db");
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
	}
}
