package probe;

import java.awt.Component;
import java.awt.Container;
import java.awt.Rectangle;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import javax.swing.AbstractButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.SpinnerModel;
import javax.swing.SwingUtilities;
import javax.swing.border.TitledBorder;

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
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.core.startup.ExceptionHandler;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.simulation.SimulationConditionsPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.FlatLafOutlines;
import info.openrocket.swing.gui.util.SwingPreferences;
import net.miginfocom.swing.MigLayout;

/**
 * Milestone 4 run 2, part S1: OpenRocket's launch conditions tab (swing gui/simulation/
 * SimulationConditionsPanel.java), headless under FlatLaf Light with SwingPreferences of a fresh store, over the
 * first simulation of the example "A simple model rocket" (data/examples), and the same panel as the Launch
 * preference page makes it (addSimulationConditionsPanel(page, preferences, false): the average wind only).
 * It prints:
 * - WALK: the panel's components in the order of their containers, in the form of the M4 sim-config scout's
 *   dialog walk (BORDER for a titled panel, its children indented; LABEL, SPINNER, SLIDER, CHECK, RADIO,
 *   BUTTON, SEPARATOR, each with " disabled" and " tip=..." as they apply; a hidden card is skipped);
 * - MODEL: every spinner's DoubleModel (read by reflection): the property, the unit group (the name of its
 *   UnitGroup.UNITS_* field), the bounds (a bound model by its property) and the slider's model;
 * - BOX: the bounds of the titled panels and of the components the walk prints (the line cut short), relative to
 *   the panel, after a layout at the preferred size;
 * - the steps a user takes (texts typed into spinners through their spinner models, clicks with doClick()),
 *   each followed by the state of the fields it concerns: the spinners' texts, the sliders' positions,
 *   enabled states, and for the validated fields (FlatLafOutlines) the outline client property and the tool
 *   tip;
 * - SUMMARY: the multi-level wind's summary (the private updateWindLevelSummary(), called by reflection, as
 *   the panel calls it when the wind editor closes) for level sets the summary treats apart, also in other
 *   units.
 * Application.getTranslator() is an ExceptionSuppressingTranslator, as in the application, so the missing key simedtdlg.error.temperatureTooLow (M4's bug 35) is reported once to the exception handler
 * (printed as HANDLER: the application shows its bug report dialog) and shown as the key.
 */
public final class SimulationConditionsProbe {
	private final PrintStream out;

	private SimulationConditionsProbe(PrintStream out) {
		this.out = out;
	}

	// ------------------------------------------------------------------------------------- the walk

	private static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n");
	}

	private static String state(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	/** The line of @p c, or null for a container the walk enters. */
	private static String describe(Component c) {
		if (c instanceof JSpinner sp) {
			return "SPINNER " + clean(((JSpinner.DefaultEditor) sp.getEditor()).getTextField().getText());
		}
		if (c instanceof JRadioButton rb) {
			return "RADIO " + rb.getText() + " selected=" + rb.isSelected();
		}
		if (c instanceof JCheckBox cb) {
			return "CHECK " + cb.getText() + " selected=" + cb.isSelected();
		}
		if (c instanceof AbstractButton b) {
			return "BUTTON " + clean(b.getText()) + (b.getIcon() != null ? " icon" : "");
		}
		if (c instanceof JSlider s) {
			return "SLIDER " + s.getMinimum() + ".." + s.getMaximum() + " at " + s.getValue();
		}
		if (c instanceof JSeparator) {
			return "SEPARATOR";
		}
		if (c instanceof JLabel l) {
			return "LABEL " + clean(l.getText()) + (l.getIcon() != null ? " icon" : "");
		}
		return null;
	}

	private void walk(Component c, String indent, List<Component> order) {
		if (!c.isVisible()) {
			return;
		}
		String line = describe(c);
		if (line != null) {
			out.println("WALK " + indent + line + state(c));
			order.add(c);
			return;
		}
		if (c instanceof Container k) {
			String inner = indent;
			if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
				out.println("WALK " + indent + "BORDER " + tb.getTitle());
				inner = indent + "  ";
			}
			for (Component child : k.getComponents()) {
				walk(child, inner, order);
			}
		}
	}

	private List<Component> walk(String title, Component panel) {
		out.println("== " + title);
		List<Component> order = new ArrayList<>();
		walk(panel, "", order);
		return order;
	}

	// -------------------------------------------------------------------------------- the models

	private static Object field(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private static DoubleModel modelOf(JSpinner spinner) throws Exception {
		SpinnerModel model = spinner.getModel();
		return (DoubleModel) field(model, model.getClass(), "this$0");
	}

	private static String unitGroupName(UnitGroup group) throws Exception {
		for (Field f : UnitGroup.class.getDeclaredFields()) {
			if (Modifier.isStatic(f.getModifiers()) && UnitGroup.class.isAssignableFrom(f.getType())
					&& f.get(null) == group) {
				return f.getName();
			}
		}
		return "?";
	}

	private static String bound(DoubleModel model, String valueField, String modelField) throws Exception {
		DoubleModel boundModel = (DoubleModel) field(model, DoubleModel.class, modelField);
		if (boundModel != null) {
			return "model:" + field(boundModel, DoubleModel.class, "valueName");
		}
		double value = (Double) field(model, DoubleModel.class, valueField);
		return Double.toString(value);
	}

	private static String slider(JSlider slider) throws Exception {
		Object model = slider.getModel();
		Class<?> type = model.getClass();
		StringBuilder sb = new StringBuilder((Boolean) field(model, type, "islinear") ? "linear" : "quadratic");
		for (String name : new String[] { "min", "mid", "max", "linearPosition" }) {
			Object value = field(model, type, name);
			if (value instanceof DoubleModel dm) {
				// A constant (DoubleModel(value)) by its value, a model of a property by its name.
				Object source = field(dm, DoubleModel.class, "source");
				sb.append(' ').append(name).append('=').append(source == null ? Double.toString(dm.getValue())
						: "model:" + field(dm, DoubleModel.class, "valueName"));
			} else {
				sb.append(' ').append(name).append('=').append(value);
			}
		}
		return sb.toString();
	}

	private void models(List<Component> order) throws Exception {
		JSlider lastSlider = null;
		for (int i = 0; i < order.size(); i++) {
			if (!(order.get(i) instanceof JSpinner spinner)) {
				continue;
			}
			DoubleModel model = modelOf(spinner);
			String sliderText = "none";
			for (int j = i + 1; j < order.size() && !(order.get(j) instanceof JSpinner); j++) {
				if (order.get(j) instanceof JSlider s) {
					sliderText = slider(s);
					lastSlider = s;
					break;
				}
			}
			out.println("MODEL " + field(model, DoubleModel.class, "valueName") + " source="
					+ field(model, DoubleModel.class, "source").getClass().getSimpleName() + " units="
					+ unitGroupName((UnitGroup) field(model, DoubleModel.class, "units")) + " min="
					+ bound(model, "minValue", "minModel") + " max=" + bound(model, "maxValue", "maxModel")
					+ " multiplier=" + field(model, DoubleModel.class, "multiplier") + " slider=" + sliderText);
		}
		if (lastSlider == null) {
			out.println("MODEL no slider");
		}
	}

	// ---------------------------------------------------------------------------------- the boxes

	private static void layout(Component c) {
		if (c instanceof Container k) {
			k.doLayout();
			for (Component child : k.getComponents()) {
				layout(child);
			}
		}
	}

	private static Rectangle boundsIn(Component c, Component ancestor) {
		Rectangle r = new Rectangle(c.getBounds());
		for (Component p = c.getParent(); p != null && p != ancestor; p = p.getParent()) {
			r.translate(p.getX(), p.getY());
		}
		return r;
	}

	private void boxes(Container panel) {
		panel.setSize(panel.getPreferredSize());
		layout(panel);
		boxes(panel, panel);
	}

	private void boxes(Container panel, Component c) {
		if (!c.isVisible()) {
			return;
		}
		String name = null;
		if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
			name = "BORDER " + tb.getTitle();
		} else if (describe(c) != null) {
			// The walk's line without its state, the text cut to 40 characters.
			String line = describe(c);
			name = line.length() > 47 ? line.substring(0, 47) : line;
		}
		if (name != null) {
			Rectangle r = boundsIn(c, panel);
			out.println("BOX " + name + " x=" + r.x + " y=" + r.y + " w=" + r.width + " h=" + r.height);
		}
		if (c instanceof Container k && !(c instanceof JSpinner)) {
			for (Component child : k.getComponents()) {
				boxes(panel, child);
			}
		}
	}

	// ------------------------------------------------------------------------------- the fields

	/** The row of a label: the label, its spinner, unit selector and slider. */
	private record Row(JLabel label, JSpinner spinner, JLabel unit, JSlider slider) {
	}

	/** The row whose label reads @p text, from the walk @p order. */
	private static Row row(List<Component> order, String text) {
		for (int i = 0; i < order.size(); i++) {
			if (order.get(i) instanceof JLabel l && !(order.get(i) instanceof AbstractButton) && text.equals(l.getText())) {
				JSpinner spinner = null;
				JLabel unit = null;
				JSlider slider = null;
				for (int j = i + 1; j < order.size() && j <= i + 3; j++) {
					Component c = order.get(j);
					if (c instanceof JSpinner s && spinner == null) {
						spinner = s;
					} else if (c instanceof JLabel u && spinner != null && unit == null) {
						unit = u;
					} else if (c instanceof JSlider s && slider == null) {
						slider = s;
					}
				}
				return new Row(l, spinner, unit, slider);
			}
		}
		throw new IllegalArgumentException("no row " + text);
	}

	private static <T extends Component> T find(List<Component> order, Class<T> type, String text) {
		for (Component c : order) {
			if (type.isInstance(c) && c instanceof AbstractButton b && text.equals(b.getText())) {
				return type.cast(c);
			}
		}
		throw new IllegalArgumentException("no " + type.getSimpleName() + " " + text);
	}

	private static String text(JSpinner spinner) {
		return ((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText();
	}

	private String field(String name, Row row) {
		FlatLafOutlines.Outline outline = FlatLafOutlines.getOutline(row.spinner());
		return name + "=[" + text(row.spinner()) + "]" + (row.slider() != null ? " at " + row.slider().getValue() : "")
				+ (row.spinner().isEnabled() ? "" : " disabled") + (outline != FlatLafOutlines.Outline.NONE ? " outline=" + outline : "")
				+ (row.spinner().getToolTipText() != null ? " tip=" + clean(row.spinner().getToolTipText()) : "");
	}

	private void type(String step, Row row, String typed, Row... shown) {
		String failure = "";
		try {
			row.spinner().getModel().setValue(typed);
		} catch (RuntimeException e) {
			failure = " EXCEPTION " + e.getClass().getName() + ": " + clean(e.getMessage());
		}
		StringBuilder sb = new StringBuilder("STEP " + step + " typed=[" + typed + "]" + failure);
		for (Row r : shown) {
			sb.append(" | ").append(field(r.label().getText(), r));
		}
		out.println(sb);
	}

	// ------------------------------------------------------------------------------- the summary

	private void summary(String title, MultiLevelPinkNoiseWindModel model) throws Exception {
		Method update = SimulationConditionsPanel.class.getDeclaredMethod("updateWindLevelSummary", JLabel.class,
				MultiLevelPinkNoiseWindModel.class);
		update.setAccessible(true);
		JLabel label = new JLabel();
		update.invoke(null, label, model);
		out.println("SUMMARY " + title + " [" + clean(label.getText()) + "]");
	}

	private void summaries() throws Exception {
		MultiLevelPinkNoiseWindModel model = new MultiLevelPinkNoiseWindModel();
		summary("initial", model);
		model.clearLevels();
		summary("no levels", model);
		model.addWindLevel(0, 3, Math.PI / 2, 0.3);
		summary("one level", model);
		model.addWindLevel(500, 7.5, Math.PI, 1.0);
		model.addWindLevel(1250.5, 12.25, 0.0, 1.0);
		summary("three levels", model);
		model.clearLevels();
		model.addWindLevel(-50, 0, 0, 0.0);
		model.addWindLevel(-10, 0, 0, 0.0);
		summary("negative altitudes, calm", model);
		model.clearLevels();
		model.addWindLevel(-200, 4, 0, 0.0);
		summary("one negative level", model);
		model.clearLevels();
		model.addWindLevel(0, 2, 0, 0.0);
		model.addWindLevel(1000, 10, 0, 0.0);
		UnitGroup.UNITS_DISTANCE.setDefaultUnit("ft");
		UnitGroup.UNITS_VELOCITY.setDefaultUnit("mph");
		UnitGroup.UNITS_WINDSPEED.setDefaultUnit("km/h");
		summary("two levels in ft and mph (wind speed km/h)", model);
		UnitGroup.setDefaultMetricUnits();
	}

	// ----------------------------------------------------------------------------------- the steps

	private void dialogTab(File repo) throws Exception {
		OpenRocketDocument document = new GeneralRocketLoader(new File(repo, "data/examples/A simple model rocket.ork"))
				.load();
		Simulation simulation = document.getSimulation(0);
		SimulationOptions options = simulation.getOptions();
		Constructor<SimulationConditionsPanel> constructor = SimulationConditionsPanel.class
				.getDeclaredConstructor(Simulation.class);
		constructor.setAccessible(true);
		SimulationConditionsPanel panel = constructor.newInstance(simulation);

		List<Component> order = walk("tab: A simple model rocket.ork, " + simulation.getName(), panel);
		models(order);
		boxes(panel);

		Row average = row(order, "Average windspeed:");
		Row deviation = row(order, "Standard deviation:");
		Row turbulence = row(order, "Turbulence intensity:");
		Row windDirection = row(order, "Wind direction:");
		Row temperature = row(order, "Temperature:");
		Row pressure = row(order, "Pressure:");
		Row humidity = row(order, "Relative humidity:");
		Row altitude = row(order, "Altitude:");
		Row length = row(order, "Length:");
		Row angle = row(order, "Angle:");
		Row rodDirection = row(order, "Direction:");
		JCheckBox isa = find(order, JCheckBox.class, "Use International Standard Atmosphere");
		JCheckBox intoWind = find(order, JCheckBox.class, "Always launch directly up-wind or down-wind");

		out.println("== the atmosphere");
		isa.doClick();
		out.println("STEP isa off selected=" + isa.isSelected() + " | " + field("T", temperature) + " | "
				+ field("p", pressure) + " | " + field("h", humidity));
		walk("atmosphere with the ISA off", isa.getParent());
		for (String t : new String[] { "15", "-39.99", "-40", "-79.99", "49.99", "50", "79.99", "80", "120", "-80",
				"-100", "-273.15", "-300", "20" }) {
			type("temperature", temperature, t, temperature);
		}
		for (String p : new String[] { "759.9", "760", "1266.5", "1266.6", "1013.25" }) {
			type("pressure at 0 m", pressure, p, pressure);
		}
		type("altitude", altitude, "1000", altitude, pressure);
		for (String p : new String[] { "674", "675", "1123", "1124" }) {
			type("pressure at 1000 m", pressure, p, pressure);
		}
		type("pressure at 1000 m", pressure, "500", pressure);
		isa.doClick();
		out.println("STEP isa on selected=" + isa.isSelected() + " | " + field("T", temperature) + " | "
				+ field("p", pressure) + " | " + field("h", humidity));
		type("altitude", altitude, "0", altitude, temperature, pressure);
		isa.doClick();
		for (String h : new String[] { "50", "100", "150", "-5" }) {
			type("humidity", humidity, h, humidity);
		}
		isa.doClick();

		out.println("== the launch site");
		for (String a : new String[] { "100", "250", "500", "1000", "5000", "11018", "12000", "-10" }) {
			type("altitude", altitude, a, altitude);
		}
		type("altitude", altitude, "0", altitude);
		Row latitude = row(order, "Latitude:");
		Row longitude = row(order, "Longitude:");
		for (String a : new String[] { "-90", "0", "33.3", "90", "95" }) {
			type("latitude", latitude, a, latitude);
		}
		for (String a : new String[] { "-180", "-80.6", "180", "200" }) {
			type("longitude", longitude, a, longitude);
		}

		out.println("== the wind");
		for (String w : new String[] { "9.99", "10", "19.99", "20", "25", "2" }) {
			type("average", average, w, average, deviation, turbulence);
		}
		type("deviation", deviation, "5", average, deviation, turbulence);
		type("deviation", deviation, "0.5", average, deviation, turbulence);
		type("turbulence", turbulence, "50", average, deviation, turbulence);
		type("turbulence", turbulence, "150", average, deviation, turbulence);
		type("average", average, "4", average, deviation, turbulence);
		type("average", average, "0", average, deviation, turbulence);
		type("average", average, "-3", average, deviation, turbulence, windDirection);
		type("average", average, "2", average, deviation, turbulence, windDirection);
		type("turbulence", turbulence, "10", average, deviation, turbulence);
		JLabel intensity = null;
		for (int i = order.indexOf(turbulence.slider()) + 1; i < order.size(); i++) {
			if (order.get(i) instanceof JLabel l) {
				intensity = l;
				break;
			}
		}
		for (String t : new String[] { "0", "0.09", "0.1", "4.99", "5", "9.99", "10", "15", "20", "25", "30" }) {
			turbulence.spinner().getModel().setValue(t);
			out.println("STEP turbulence typed=[" + t + "] | " + field("turbulence", turbulence) + " | intensity="
					+ intensity.getText());
		}
		type("turbulence", turbulence, "10", turbulence);
		for (String d : new String[] { "0", "90", "359", "360", "400", "-90" }) {
			type("wind direction", windDirection, d, windDirection, rodDirection);
		}
		type("wind direction", windDirection, "90", windDirection, rodDirection);

		out.println("== the launch rod");
		for (String l : new String[] { "0", "50", "100", "200", "500", "1000", "-5" }) {
			type("length", length, l, length);
		}
		type("length", length, "100", length);
		for (String a : new String[] { "44.99", "45", "59.99", "60", "-45", "-60", "75", "0" }) {
			type("angle", angle, a, angle);
		}
		intoWind.doClick();
		out.println("STEP into wind off selected=" + intoWind.isSelected() + " | " + field("direction", rodDirection));
		walk("launch rod with into wind off", intoWind.getParent());
		type("rod direction", rodDirection, "45", rodDirection, windDirection);
		intoWind.doClick();
		out.println("STEP into wind on selected=" + intoWind.isSelected() + " | " + field("direction", rodDirection));
		type("wind direction", windDirection, "180", windDirection, rodDirection);

		out.println("== the wind model");
		JRadioButton multiLevel = find(order, JRadioButton.class, "Multi-level");
		JRadioButton averageRadio = find(order, JRadioButton.class, "Average");
		multiLevel.doClick();
		out.println("STEP multi-level type=" + options.getWindModelType());
		walk("wind with the multi-level model", multiLevel.getParent());
		options.getMultiLevelWindModel().addWindLevel(300, 6, 0);
		walk("wind after a level was added (no refresh)", multiLevel.getParent());
		averageRadio.doClick();
		out.println("STEP average type=" + options.getWindModelType());
		options.setWindModelType(info.openrocket.core.models.wind.WindModelType.MULTI_LEVEL);
		out.println("STEP set multi-level in code: average=" + averageRadio.isSelected() + " multi-level="
				+ multiLevel.isSelected());

		out.println("== the buttons");
		type("average", average, "5", average);
		type("latitude", latitude, "10", latitude);
		find(order, AbstractButton.class, "Reset to default").doClick();
		out.println("STEP reset type=" + options.getWindModelType() + " average=" + averageRadio.isSelected()
				+ " timeStep=" + options.getTimeStep() + " maxTime=" + options.getMaxSimulationTime());
		walk("tab after Reset to default", panel);
		type("average", average, "3.5", average);
		type("length", length, "150", length);
		find(order, AbstractButton.class, "Save as default").doClick();
		ApplicationPreferences preferences = Application.getPreferences();
		out.println("STEP save windAverage=" + preferences.getAverageWindModel().getAverage() + " rodLength="
				+ preferences.getLaunchRodLength());
	}

	private void launchPage() throws Exception {
		JPanel page = new JPanel(new MigLayout("fill"));
		SimulationConditionsPanel.addSimulationConditionsPanel(page, Application.getPreferences(), false);
		List<Component> order = walk("launch page: addSimulationConditionsPanel(page, preferences, false)", page);
		models(order);
		boxes(page);
	}

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		File repo = new File(System.getProperty("qtrocket.repo"));
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		List<ThrustCurveMotor> read = ThrustCurveMotorSQLiteDatabase
				.readDatabase(new File(repo, "data/motors/initial_motors.db"));
		for (ThrustCurveMotor motor : read) {
			motors.addMotor(motor);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		// The application's handler shows the bug report dialog; here the report is printed.
		Application.setExceptionHandler(new ExceptionHandler() {
			@Override
			public void handleErrorCondition(String message) {
				out.println("HANDLER " + message);
			}

			@Override
			public void handleErrorCondition(String message, Throwable exception) {
				out.println("HANDLER " + message + " (" + exception.getClass().getName() + ")");
			}

			@Override
			public void handleErrorCondition(Throwable exception) {
				out.println("HANDLER " + exception.getClass().getName());
			}

			@Override
			public void uncaughtException(Thread thread, Throwable throwable) {
				out.println("HANDLER uncaught " + throwable.getClass().getName());
			}
		});
		UITheme.Themes.LIGHT.applyTheme();
		SimulationConditionsProbe probe = new SimulationConditionsProbe(out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				probe.launchPage();
				probe.dialogTab(repo);
				probe.summaries();
			} catch (Exception e) {
				failure[0] = e;
			}
		});
		out.flush();
		if (failure[0] != null) {
			failure[0].printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}
}
