package info.openrocket.swing.gui.plot;

import java.awt.Color;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

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
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.core.util.LinearInterpolator;
import info.openrocket.core.util.MathUtil;
import info.openrocket.swing.gui.simulation.SimulationPlotPanel;
import info.openrocket.swing.gui.util.SwingPreferences;

import org.jfree.chart.JFreeChart;
import org.jfree.chart.LegendItemCollection;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.chart.labels.XYToolTipGenerator;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.xy.XYItemRenderer;
import org.jfree.chart.title.TextTitle;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

import probe.DeterministicUuids;

/**
 * Milestone 4 run 1, part P3 (tools/openrocket-plots): the plots OpenRocket makes of the stored simulations of its
 * example designs, for QtRocket's core plot model (plot/SimulationPlotModel and plot/SimulationPlotConfiguration).
 * In OpenRocket's package info.openrocket.swing.gui.plot, for its package-private and protected members; headless,
 * on the core's ServicesForTesting with SwingPreferences and the bundled motor database.
 * <p>
 * For every simulation of the given designs that has stored data, and every one of the 9 preset plot configurations
 * chosen through OpenRocket's own SimulationPlotPanel (its preset combo box, which takes a copy in the default units),
 * it prints what SimulationPlot.create() makes, before setShowEvents() draws anything: the configuration, the
 * automatic axes fillAutoAxes() chooses on the first branch with the goodness of every candidate and the ranges it was
 * scored on, the filled configuration's ranges over every branch, the titles, the axes' labels and ranges, the colours
 * the panel shows, the series (a digest of their points, their extremes and their first, middle and last point), the
 * legend, the events and readout tool tips. The event groups, the domain's widening for the event lines and the icons (with their tool
 * tips through Plot.formatTooltip()) are replayed from SimulationPlot's eventList, each group's events in that order
 * and the group at its first event (QtRocket's fix of M4's bug 41: Java keeps them in a HashSet, so its own lines,
 * labels and icons change from run to run).
 * <p>
 * M4's bug 45 (a right axis of -Infinity when the left range starts or ends at exactly zero and the right one spans
 * zero) is fixed as QtRocket fixes it: every configuration is a {@link CommonZeroFixedConfiguration}, which skips the
 * common zero when it would give an infinite bound; where that changes what OpenRocket computes, the probe also
 * prints OpenRocket's own choice, ranges and candidates (the lines JAVA-AUTO, JAVA-FILLED and JAVA-GOODNESS). M4's bug
 * 46 (a branch without values of a row's type makes OpenRocket drop the row's range) is not replayed: the probe prints
 * NAN-EXTREME where it would change a range, which the generator refuses (no stored simulation has such a branch).
 * Everything else is OpenRocket's own code. Every double is printed with Double.toHexString; a tool tip with a value
 * whose digits could depend on the libm is marked "risky" (see risky()).
 * <p>
 * Output: one record per line, its fields separated by tabs, a field's backslash, tab, line feed and carriage return
 * escaped as \\, \t, \n and \r. gen_simulation_plots.py turns it into tests/data/goldens/plots/*.json.
 * <p>
 * Arguments: the design files, in the order to print them. System properties: qtrocket.repo (the QtRocket checkout:
 * data/motors/initial_motors.db is read).
 */
public class SimulationPlotsProbe {

	/** The domain widening of the event lines: 1 % of the upper bound (SimulationPlot.plotVerticalLineMarkers). */
	private static final double MARKER_WIDTH_FRACTION = 0.01;

	public static void main(String[] args) throws Exception {
		DeterministicUuids.install();
		bootstrap();
		line("META", BuildProperties.getVersion(), System.getProperty("java.version"));
		presets();
		for (String f : args) {
			File file = new File(f);
			DeterministicUuids.reseed(file.getName());
			OpenRocketDocument doc = new GeneralRocketLoader(file).load();
			int simIndex = 0;
			for (Simulation sim : doc.getSimulations()) {
				if (sim.getSimulatedData() != null && sim.getSimulatedData().getBranchCount() > 0) {
					simulation(file.getName(), simIndex, sim);
				}
				simIndex++;
			}
		}
		line("END");
	}

	// ---------------------------------------------------------------------------------------------
	// Output

	static String escape(String s) {
		if (s == null) {
			return "\\0";
		}
		return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
	}

	static void line(String tag, Object... fields) {
		StringBuilder sb = new StringBuilder(tag);
		for (Object f : fields) {
			sb.append('\t');
			if (f instanceof Double d) {
				sb.append(hex(d));
			} else {
				sb.append(escape(f == null ? null : String.valueOf(f)));
			}
		}
		System.out.println(sb);
	}

	static String hex(double v) {
		return Double.toHexString(v);
	}

	static String join(List<?> list) {
		StringBuilder sb = new StringBuilder();
		for (Object o : list) {
			sb.append(sb.length() == 0 ? "" : ",").append(o);
		}
		return sb.toString();
	}

	// ---------------------------------------------------------------------------------------------
	// The presets

	static void presets() {
		SimulationPlotConfiguration[] presets = SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS;
		for (int k = 0; k < presets.length; k++) {
			SimulationPlotConfiguration p = presets[k];
			List<String> events = new ArrayList<>();
			for (FlightEvent.Type t : FlightEvent.Type.values()) {
				if (p.isEventActive(t)) {
					events.add(t.name());
				}
			}
			line("PRESET", k, p.getName(), p.getDomainAxisType().getName(), p.getDomainAxisUnit().getUnit(),
					join(events));
			for (int i = 0; i < p.getDataCount(); i++) {
				line("PRESET-ROW", i, p.getType(i).getName(), p.getUnit(i).getUnit(), p.getAxis(i));
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// A simulation

	static void simulation(String file, int simIndex, Simulation sim) throws Exception {
		List<FlightDataBranch> branches = sim.getSimulatedData().getBranches();
		line("SIM", file, simIndex, sim.getName(), branches.size());
		for (int b = 0; b < branches.size(); b++) {
			FlightDataBranch branch = branches.get(b);
			line("BRANCH", b, branch.getName(), branch.getLength(), branch.getSeparationTime(),
					branch.getDataIndexOfTime(branch.getSeparationTime()));
		}
		List<String> labels = Util.generateSeriesLabels(branches);
		line("SERIES-LABELS", labels.toArray());

		SimulationPlotPanel panel = SimulationPlotPanel.create(sim);
		JComboBox<SimulationPlotConfiguration> selector = panel.configurationSelector;
		Method colors = SimulationPlotPanel.class.getDeclaredMethod("computeDefaultPlotColors");
		colors.setAccessible(true);
		SimulationPlotConfiguration[] presets = SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS;
		for (int k = 0; k < presets.length; k++) {
			// Another item first, so that choosing the preset fires the combo box's listener.
			selector.setSelectedIndex(selector.getItemCount() - 1);
			selector.setSelectedItem(presets[k]);
			SimulationPlotConfiguration javaConfig = panel.getConfiguration();
			plot(sim, panel, colors, k, javaConfig);
		}
	}

	static void plot(Simulation sim, SimulationPlotPanel panel, Method colors, int k,
			SimulationPlotConfiguration javaConfig) throws Exception {
		List<FlightDataBranch> branches = sim.getSimulatedData().getBranches();
		FlightDataBranch main = branches.get(0);
		CommonZeroFixedConfiguration config = new CommonZeroFixedConfiguration(javaConfig);

		line("PLOT", k, SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS[k].getName(), config.getName(),
				config.getDomainAxisType().getName(), config.getDomainAxisUnit().getUnit());
		for (int i = 0; i < config.getDataCount(); i++) {
			line("ROW", i, config.getType(i).getName(), config.getUnit(i).getUnit(), config.getAxis(i),
					config.getPlotDataColor(i) == null ? "-" : color(config.getPlotDataColor(i)),
					config.getPlotDataLineStyle(i) == null ? "-" : config.getPlotDataLineStyle(i).name());
		}
		List<String> events = new ArrayList<>();
		for (FlightEvent.Type t : FlightEvent.Type.values()) {
			if (config.isEventActive(t)) {
				events.add(t.name());
			}
		}
		line("EVENTS", join(events));
		nanExtremes(config, branches);

		// The automatic axes, scored on the first branch, and every candidate's goodness in the order
		// recursiveFillAutoAxes() scores them (the first automatic row the most significant, 0 before 1).
		SimulationPlotConfiguration auto = config.fillAutoAxes(main);
		line("AUTO", axes(auto), auto.getAllAxes().get(0).getMinValue(), auto.getAllAxes().get(0).getMaxValue(),
				auto.getAllAxes().get(1).getMinValue(), auto.getAllAxes().get(1).getMaxValue());
		goodness("GOODNESS", config, main);

		// The panel's colours (computeDefaultPlotColors() on the configuration fixed for bug 45).
		Field configurationField = PlotPanel.class.getDeclaredField("configuration");
		configurationField.setAccessible(true);
		configurationField.set(panel, config);
		@SuppressWarnings("unchecked")
		Map<Integer, Color> panelColors = (Map<Integer, Color>) colors.invoke(panel);
		configurationField.set(panel, javaConfig);
		line("PANEL-COLORS", panelColorIndices(config, main, branches.size(), panelColors).toArray());

		SimulationPlot plot = SimulationPlot.create(sim, config, false);
		SimulationPlotConfiguration filled = (SimulationPlotConfiguration) plot.filledConfig;
		line("FILLED", axes(filled), filled.getAllAxes().get(0).getMinValue(),
				filled.getAllAxes().get(0).getMaxValue(), filled.getAllAxes().get(1).getMinValue(),
				filled.getAllAxes().get(1).getMaxValue());

		// OpenRocket's own choice and ranges, where its bug 45 makes them differ.
		SimulationPlotConfiguration javaAuto = javaConfig.fillAutoAxes(main);
		SimulationPlotConfiguration javaFilled =
				(SimulationPlotConfiguration) SimulationPlot.create(sim, javaConfig, false).filledConfig;
		if (!sameAxes(javaAuto, auto) || !sameAxes(javaFilled, filled)) {
			line("JAVA-AUTO", axes(javaAuto), javaAuto.getAllAxes().get(0).getMinValue(),
					javaAuto.getAllAxes().get(0).getMaxValue(), javaAuto.getAllAxes().get(1).getMinValue(),
					javaAuto.getAllAxes().get(1).getMaxValue());
			line("JAVA-FILLED", axes(javaFilled), javaFilled.getAllAxes().get(0).getMinValue(),
					javaFilled.getAllAxes().get(0).getMaxValue(), javaFilled.getAllAxes().get(1).getMinValue(),
					javaFilled.getAllAxes().get(1).getMaxValue());
			goodness("JAVA-GOODNESS", javaConfig, main);
		}

		chart(plot);
		List<Info> eventList = eventList(plot);
		events(plot, sim, config, eventList);
	}

	static String axes(PlotConfiguration<?, ?> c) {
		List<Integer> axes = new ArrayList<>();
		for (int i = 0; i < c.getDataCount(); i++) {
			axes.add(c.getAxis(i));
		}
		return join(axes);
	}

	static boolean sameAxes(PlotConfiguration<?, ?> a, PlotConfiguration<?, ?> b) {
		if (!axes(a).equals(axes(b))) {
			return false;
		}
		for (int i = 0; i < 2; i++) {
			Axis x = a.getAllAxes().get(i);
			Axis y = b.getAllAxes().get(i);
			if (Double.doubleToLongBits(x.getMinValue()) != Double.doubleToLongBits(y.getMinValue())
					|| Double.doubleToLongBits(x.getMaxValue()) != Double.doubleToLongBits(y.getMaxValue())) {
				return false;
			}
		}
		return true;
	}

	/** Every candidate of fillAutoAxes(): its axes for the automatic rows and its goodness on @p main. */
	static void goodness(String tag, SimulationPlotConfiguration config, FlightDataBranch main) {
		List<Integer> autoRows = new ArrayList<>();
		for (int i = 0; i < config.getDataCount(); i++) {
			if (config.getAxis(i) < 0) {
				autoRows.add(i);
			}
		}
		for (int mask = 0; mask < (1 << autoRows.size()); mask++) {
			SimulationPlotConfiguration candidate = config.cloneConfiguration();
			StringBuilder key = new StringBuilder();
			for (int j = 0; j < autoRows.size(); j++) {
				int axis = (mask >> (autoRows.size() - 1 - j)) & 1;
				candidate.setPlotDataAxis(autoRows.get(j), axis);
				key.append(axis);
			}
			double g = candidate.getGoodnessValue(main);
			line(tag, axes(candidate), g, candidate.getAllAxes().get(0).getMinValue(),
					candidate.getAllAxes().get(0).getMaxValue(), candidate.getAllAxes().get(1).getMinValue(),
					candidate.getAllAxes().get(1).getMaxValue());
		}
	}

	static String color(Color c) {
		return String.format("#%02x%02x%02x%02x", c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha());
	}

	/**
	 * The index of the palette colour the panel shows for each row of @p config: computeDefaultPlotColors()'s
	 * arithmetic replayed (the rows of the left axis, then those of the right one, each axis after the previous one's
	 * rows times the branch count), before Util.getPlotColor() wraps it to the palette's 10 colours, as QtRocket's
	 * SimulationPlotModel::computeDefaultColorIndices() gives it. Each index is checked against the colour the panel
	 * computed (@p panelColors).
	 */
	static List<String> panelColorIndices(SimulationPlotConfiguration config, FlightDataBranch main, int branchCount,
			Map<Integer, Color> panelColors) {
		SimulationPlotConfiguration filled = config.fillAutoAxes(main);
		int safeBranchCount = Math.max(branchCount, 1);
		int[] indices = new int[filled.getDataCount()];
		int cumulativeSeriesCount = 0;
		for (int axis = 0; axis < 2; axis++) {
			int relativeIndex = 0;
			for (int i = 0; i < filled.getDataCount(); i++) {
				int rowAxis = filled.getAxis(i);
				if (rowAxis < 0 || rowAxis >= 2) {
					rowAxis = 0;
				}
				if (rowAxis == axis) {
					indices[i] = cumulativeSeriesCount + relativeIndex;
					relativeIndex++;
				}
			}
			cumulativeSeriesCount += relativeIndex * safeBranchCount;
		}
		List<String> out = new ArrayList<>();
		for (int i = 0; i < config.getDataCount(); i++) {
			Color shown = panelColors.get(i);
			if (shown == null || !Util.getPlotColor(indices[i]).equals(shown)) {
				throw new IllegalStateException("the panel's colour of row " + i + " is not the palette's colour "
						+ indices[i] + ": " + (shown == null ? "none" : color(shown)));
			}
			out.add(String.valueOf(indices[i]));
		}
		return out;
	}

	/**
	 * Prints NAN-EXTREME for each row of @p config whose extremes over the branches fitAxes() combines (the first
	 * branch, and every other one with points) are NaN in some of them and not in others: a branch without the row's
	 * type, or whose values of it are all NaN. There OpenRocket's Math.min and Math.max make the row's range NaN and
	 * drop it from its axis (M4's bug 46), which QtRocket's fix does not; the probe does not replay that fix, so
	 * gen_simulation_plots.py refuses such a plot.
	 */
	static void nanExtremes(SimulationPlotConfiguration config, List<FlightDataBranch> branches) {
		for (int i = 0; i < config.getDataCount(); i++) {
			FlightDataType type = config.getType(i);
			boolean nan = false;
			boolean value = false;
			for (int b = 0; b < branches.size(); b++) {
				FlightDataBranch branch = branches.get(b);
				if (b > 0 && branch.getLength() == 0) {
					continue;
				}
				for (double extreme : new double[] { branch.getMinimum(type), branch.getMaximum(type) }) {
					if (Double.isNaN(extreme)) {
						nan = true;
					} else {
						value = true;
					}
				}
			}
			if (nan && value) {
				line("NAN-EXTREME", i, type.getName());
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The chart

	static void chart(SimulationPlot plot) throws Exception {
		JFreeChart chart = plot.getJFreeChart();
		XYPlot xy = chart.getXYPlot();
		ValueAxis domain = xy.getDomainAxis();
		line("TITLE", chart.getTitle().getText(), ((TextTitle) chart.getSubtitle(0)).getText());
		line("DOMAIN", domain.getLabel(), domain.getLowerBound(), domain.getUpperBound());
		int offset = 0;
		for (int a = 0; a < plot.data.length; a++) {
			XYSeriesCollection col = plot.data[a];
			ValueAxis ra = col.getSeriesCount() > 0 ? xy.getRangeAxis(a) : null;
			line("Y-AXIS", a, col.getSeriesCount(), ra == null ? "-" : ra.getLabel(),
					ra == null ? Double.NaN : ra.getLowerBound(), ra == null ? Double.NaN : ra.getUpperBound());
			XYItemRenderer r = col.getSeriesCount() > 0 ? xy.getRenderer(a) : null;
			for (int s = 0; s < col.getSeriesCount(); s++) {
				Plot.MetadataXYSeries ser = (Plot.MetadataXYSeries) col.getSeries(s);
				int n = ser.getItemCount();
				line("SERIES", offset + s, a, ser.getDescription(), ser.getBranchIdx(), ser.getDataIdx(), ser.getUnit(),
						n, digest(ser), ser.getMinX(), ser.getMaxX(), ser.getMinY(), ser.getMaxY(),
						point(ser, 0), point(ser, n / 2), point(ser, n - 1));
			}
			if (r != null) {
				XYToolTipGenerator tg = r.getDefaultToolTipGenerator();
				for (int s = 0; s < col.getSeriesCount(); s++) {
					Plot.MetadataXYSeries ser = (Plot.MetadataXYSeries) col.getSeries(s);
					List<Double> time = plot.allBranches.get(ser.getBranchIdx()).getView(FlightDataType.TYPE_TIME);
					int n = ser.getItemCount();
					for (int item : new int[] { 0, n / 2, n - 1 }) {
						boolean risky = risky(ser.getX(item).doubleValue()) || risky(ser.getY(item).doubleValue())
								|| (time != null && risky(time.get(item)));
						line("TOOLTIP", offset + s, item, tg.generateToolTip(col, s, item),
								plot.getNameBasedOnIdxAndSeries(ser, item), risky ? "risky" : "-");
					}
				}
			}
			offset += col.getSeriesCount();
		}
		LegendItemCollection li = plot.legendItems.getLegendItems();
		List<String> legend = new ArrayList<>();
		for (int i = 0; i < li.getItemCount(); i++) {
			legend.add(li.get(i).getLabel());
		}
		line("LEGEND", legend.toArray());
	}

	static String point(XYSeries s, int i) {
		if (i < 0 || i >= s.getItemCount()) {
			return "-";
		}
		return hex(s.getX(i).doubleValue()) + "," + hex(s.getY(i).doubleValue());
	}

	/**
	 * The first 16 hexadecimal digits of the SHA-256 digest of a series' points: the x values, then the y values, each
	 * as the 8 little-endian bytes of Double.doubleToLongBits() (every NaN as 0x7ff8000000000000).
	 */
	static String digest(XYSeries s) throws Exception {
		MessageDigest md = MessageDigest.getInstance("SHA-256");
		ByteBuffer buffer = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
		for (int pass = 0; pass < 2; pass++) {
			for (int i = 0; i < s.getItemCount(); i++) {
				double v = pass == 0 ? s.getX(i).doubleValue() : s.getY(i).doubleValue();
				buffer.clear();
				buffer.putLong(Double.doubleToLongBits(v));
				md.update(buffer.array());
			}
		}
		return hexBytes(md.digest()).substring(0, 16);
	}

	static String hexBytes(byte[] bytes) {
		StringBuilder sb = new StringBuilder();
		for (byte b : bytes) {
			sb.append(String.format("%02x", b & 0xff));
		}
		return sb.toString();
	}

	// ---------------------------------------------------------------------------------------------
	// The events, replayed from SimulationPlot's eventList (bug 41's fix)

	/** One entry of SimulationPlot's eventList. */
	record Info(int stage, double time, FlightEvent event) {
	}

	static Field field(Class<?> cls, String name) throws Exception {
		Field f = cls.getDeclaredField(name);
		f.setAccessible(true);
		return f;
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

	/** One group of fillEventLists(), its events in eventList order, and the event it takes its look from. */
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

	/** The look of a group: its look event's type, and for a warning its priority. */
	static String look(Group g) {
		FlightEvent e = g.look().event();
		if (e.getType() == FlightEvent.Type.SIM_WARN) {
			return e.getType().name() + "/" + ((Warning) e.getData()).getPriority().name();
		}
		return e.getType().name();
	}

	static String eventText(Info info) {
		return info.event().getType().name() + "@" + hex(info.time()) + "/b" + info.stage();
	}

	static void events(SimulationPlot plot, Simulation sim, SimulationPlotConfiguration config, List<Info> eventList)
			throws Exception {
		List<String> el = new ArrayList<>();
		for (Info info : eventList) {
			el.add(eventText(info));
		}
		line("EVENT-LIST", el.toArray());
		int branchCount = sim.getSimulatedData().getBranchCount();
		for (int b = -1; b < branchCount; b++) {
			for (Group g : groups(eventList, b)) {
				List<String> ev = new ArrayList<>();
				StringBuilder label = new StringBuilder();
				for (Info info : g.infos()) {
					ev.add(eventText(info));
					label.append(label.length() == 0 ? "" : ", ").append(info.event().getType().toString());
				}
				line("GROUP", b < 0 ? "all" : String.valueOf(b), g.infos().get(0).time(), look(g), String.join(" ", ev),
						label.toString());
			}
		}
		// plotVerticalLineMarkers()'s widening of the domain (the time domain only), the groups at their first
		// events.
		if (config.getDomainAxisType() == FlightDataType.TYPE_TIME) {
			ValueAxis domain = plot.getJFreeChart().getXYPlot().getDomainAxis();
			double upper = domain.getUpperBound();
			double markerWidth = MARKER_WIDTH_FRACTION * upper;
			for (Group g : groups(eventList, -1)) {
				double t = g.infos().get(0).time();
				if (t > upper - markerWidth) {
					upper = t + markerWidth;
				}
			}
			line("DOMAIN-WITH-EVENT-LINES", domain.getLowerBound(), upper);
		}
		icons(plot, sim, config, eventList);
	}

	/** SimulationPlot.plotIconMarkers() for every branch from the last, the groups at their first events. */
	static void icons(SimulationPlot plot, Simulation sim, SimulationPlotConfiguration config, List<Info> eventList)
			throws Exception {
		List<Axis> axes = plot.filledConfig.getAllAxes();
		XYSeriesCollection[] data = plot.data;
		Method format = Plot.class.getDeclaredMethod("formatTooltip", String.class, String.class, double.class,
				String.class, String.class, double.class, String.class, String.class, double.class, String.class,
				Set.class);
		format.setAccessible(true);
		int branchCount = sim.getSimulatedData().getBranchCount();
		for (int b = branchCount - 1; b >= 0; b--) {
			FlightDataBranch branch = sim.getSimulatedData().getBranch(b);
			List<Double> time = branch.getView(FlightDataType.TYPE_TIME);
			LinearInterpolator di = new LinearInterpolator(time, branch.getView(config.getDomainAxisType()));
			List<Group> gs = groups(eventList, b);
			int offset = 0;
			for (int axisno = 0; axisno < data.length; axisno++) {
				double minLeft = axes.get(0).getMinValue();
				double maxLeft = axes.get(0).getMaxValue();
				double minThis = axes.get(axisno).getMinValue();
				double maxThis = axes.get(axisno).getMaxValue();
				double slope = (maxLeft - minLeft) / (maxThis - minThis);
				double intercept = (maxThis * minLeft - maxLeft * minThis) / (maxThis - minThis);
				for (int s = 0; s < data[axisno].getSeriesCount(); s++) {
					Plot.MetadataXYSeries series = (Plot.MetadataXYSeries) data[axisno].getSeries(s);
					if (series.getBranchIdx() != b) {
						continue;
					}
					int row = series.getDataIdx();
					FlightDataType type = config.getType(row);
					LinearInterpolator ri = new LinearInterpolator(time, branch.getView(type));
					for (int i = 0; i < gs.size(); i++) {
						Group g = gs.get(i);
						double t = g.infos().get(0).time();
						double x = config.getDomainAxisUnit().toUnit(di.getValue(t));
						double y = config.getUnit(row).toUnit(ri.getValue(t));
						if (Double.isNaN(y) || Double.isNaN(x)) {
							continue;
						}
						Optional<Double> closest = time.stream()
								.min(Comparator.comparingDouble(sample -> Math.abs(sample - t)));
						int sampleIdx = closest.map(time::indexOf).orElse(-1);
						Set<FlightEvent> events = new LinkedHashSet<>();
						for (Info info : g.infos()) {
							events.add(info.event());
						}
						String tooltip = (String) format.invoke(plot, plot.getNameBasedOnIdxAndSeries(series, sampleIdx),
								FlightDataType.TYPE_TIME.getName(), t,
								FlightDataType.TYPE_TIME.getUnitGroup().getDefaultUnit().toString(),
								config.getDomainAxisType().getName(), x, config.getDomainAxisUnit().getUnit(),
								type.toString(), y, series.getUnit(), events);
						double yloc = slope * y + intercept;
						line("ICON", offset + s, i, t, x, y, yloc, look(g), sampleIdx, tooltip,
								risky(t) || risky(x) || risky(y) ? "risky" : "-");
					}
				}
				offset += data[axisno].getSeriesCount();
			}
		}
	}

	/**
	 * Whether the text DecimalFormatter.df(v, 2, false) gives @p v could depend on the libm: for a value below 0.1 it
	 * counts the digits by Math.floor(Math.log10(1 / v)), which another log10 may floor to the integer below or above
	 * when 1 / v is within 1e-12 (relative) of a power of ten. Risky when the two digit counts format @p v
	 * differently (an exact power of ten, 1 / 0.01 = 100, formats alike either way).
	 */
	static boolean risky(double v) {
		double a = Math.abs(v);
		if (!(a < 0.1 && a > 0)) {
			return false;
		}
		double l = Math.log10(1 / a);
		if (Math.abs(l - Math.rint(l)) >= 1e-12) {
			return false;
		}
		int k = (int) Math.rint(l);
		String below = new java.text.DecimalFormat("#." + "#".repeat(k + 1)).format(v);
		String above = new java.text.DecimalFormat("#." + "#".repeat(k + 2)).format(v);
		return !below.equals(above);
	}

	// ---------------------------------------------------------------------------------------------
	// Bug 45's fix

	/**
	 * A SimulationPlotConfiguration whose fitAxes() leaves the common zero out when it would give an axis an infinite
	 * bound, as QtRocket's PlotConfiguration::fitAxes() does (M4's bug 45): otherwise PlotConfiguration.fitAxes(), line
	 * for line. Its clones (Object.clone() keeps the class: fillAutoAxes()'s candidates, Plot's filled configuration)
	 * fit their axes the same way.
	 */
	static final class CommonZeroFixedConfiguration extends SimulationPlotConfiguration {
		private static final double INCLUDE_ZERO_DISTANCE = 0.3; // PlotConfiguration's 30 % of total range

		CommonZeroFixedConfiguration(SimulationPlotConfiguration source) {
			super(source.getName(), source.getDomainAxisType());
			setDomainAxisUnit(source.getDomainAxisUnit());
			for (int i = 0; i < source.getDataCount(); i++) {
				addPlotDataType(source.getType(i));
				setPlotDataType(i, source.getType(i), source.getUnit(i), source.getAxis(i));
				setPlotDataColor(i, source.getPlotDataColor(i));
				setPlotDataLineStyle(i, source.getPlotDataLineStyle(i));
			}
			for (FlightEvent.Type t : FlightEvent.Type.values()) {
				setEvent(t, source.isEventActive(t));
			}
		}

		@Override
		public void fitAxes(List<FlightDataBranch> data) {
			// Reset axes
			for (Axis a : allAxes) {
				a.reset();
			}

			// Add full range to the axes
			calculateAxisBounds(data);

			// Ensure non-zero (or NaN) range, add a few percent range, include zero if it is close
			for (Axis a : allAxes) {
				if (MathUtil.equals(a.getMinValue(), a.getMaxValue())) {
					a.addBound(a.getMinValue() - 1);
					a.addBound(a.getMaxValue() + 1);
				}

				double addition = a.getRangeLength() * 0.03;
				a.addBound(a.getMinValue() - addition);
				a.addBound(a.getMaxValue() + addition);

				double dist;
				dist = Math.min(Math.abs(a.getMinValue()), Math.abs(a.getMaxValue()));
				if (dist <= a.getRangeLength() * INCLUDE_ZERO_DISTANCE) {
					a.addBound(0);
				}
			}

			// Check whether to use a common zero
			Axis left = allAxes.get(0);
			Axis right = allAxes.get(1);

			if (left.getMinValue() > 0 || left.getMaxValue() < 0 || right.getMinValue() > 0
					|| right.getMaxValue() < 0 || Double.isNaN(left.getMinValue())
					|| Double.isNaN(right.getMinValue())) {
				return;
			}

			//// Compute common zero
			double min1 = left.getMinValue();
			double max1 = left.getMaxValue();
			double min2 = right.getMinValue();
			double max2 = right.getMaxValue();

			// Get the zero locations, scaled down to a value from 0 to 1 (0 = bottom of y axis, 1 = top)
			double zeroLoc1 = -min1 / left.getRangeLength();
			double zeroLoc2 = -min2 / right.getRangeLength();

			// Scale the right axis
			if (zeroLoc1 > zeroLoc2) {
				min2 = -max2 * (zeroLoc1 / (1 - zeroLoc1));
			} else {
				max2 = min2 * (-1 / zeroLoc1 + 1);
			}

			// Bug 45's fix: a bound that is not finite leaves the right axis as it is.
			if (!Double.isFinite(min2) || !Double.isFinite(max2)) {
				return;
			}

			// Apply scale
			left.addBound(min1);
			left.addBound(max1);
			right.addBound(min2);
			right.addBound(max2);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Start-up

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
