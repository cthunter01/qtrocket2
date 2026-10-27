package probe;

import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.List;

import javax.swing.JCheckBox;
import javax.swing.JLabel;
import javax.swing.JSpinner;
import javax.swing.JTable;
import javax.swing.SwingUtilities;
import javax.swing.event.RowSorterEvent;

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
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.MotorFilterPanel;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.MotorRowFilter;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorSelectionPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.gui.widgets.MultiSlider;

/**
 * Milestone 3 run 2, part M2: OpenRocket's MotorFilterPanel (swing gui/dialogs/motor/thrustcurve/
 * MotorFilterPanel.java) in its real ThrustCurveMotorSelectionPanel over the bundled motor database,
 * headless, driven as a user would: lengths typed into its two spinners (their spinner models' setValue(text),
 * which a commit of the text field calls), its length slider moved (setValueAt(), as a key or a drag does),
 * the limit boxes clicked with and without a mount, the diameter slider moved, and a panel made with the
 * limit boxes stored on. After each step it prints the filter's lengths and diameters, the sliders' thumbs,
 * what the spinners show, which controls are enabled, the mount label, the table's rows and how many times
 * the table was sorted (the panel's onSelectionChanged() sorts it once a call). The panel's members are read
 * by reflection.
 */
public final class MotorFilterPanelProbe {
	private final ThrustCurveMotorSelectionPanel panel = new ThrustCurveMotorSelectionPanel();
	private final MotorFilterPanel filters;
	private final MotorRowFilter filter;
	private final JTable table;
	private final PrintStream out;
	private int sorts;

	private MotorFilterPanelProbe(PrintStream out) throws Exception {
		this.out = out;
		filters = (MotorFilterPanel) get(panel, ThrustCurveMotorSelectionPanel.class, "motorFilterPanel");
		filter = (MotorRowFilter) get(panel, ThrustCurveMotorSelectionPanel.class, "rowFilter");
		table = (JTable) get(panel, ThrustCurveMotorSelectionPanel.class, "table");
		table.getRowSorter().addRowSorterListener(e -> {
			if (e.getType() == RowSorterEvent.Type.SORTED) {
				sorts++;
			}
		});
	}

	private static Object get(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private Object member(String name) throws Exception {
		return get(filters, MotorFilterPanel.class, name);
	}

	private MultiSlider slider(String name) throws Exception {
		return (MultiSlider) member(name);
	}

	private JCheckBox box(String name) throws Exception {
		return (JCheckBox) member(name);
	}

	private String spinner(String name) throws Exception {
		return String.valueOf(((JSpinner) member(name)).getValue());
	}

	private static String hex(double value) {
		return value + " (" + Double.toHexString(value) + ")";
	}

	private void state(String step) throws Exception {
		MultiSlider length = slider("lengthSlider");
		MultiSlider diameter = slider("diameterSlider");
		out.println("STATE\t" + step + "\tminL=" + hex(filter.getMinimumLength()) + "\tmaxL="
				+ hex(filter.getMaximumLength()) + "\tthumbs=" + length.getValueAt(0) + "," + length.getValueAt(1)
				+ "\tspinners=[" + spinner("minLengthSpinner") + "][" + spinner("maxLengthSpinner") + "]\tminD="
				+ get(filter, MotorRowFilter.class, "minimumDiameter") + "\tmaxD="
				+ get(filter, MotorRowFilter.class, "maximumDiameter") + "\tdthumbs=" + diameter.getValueAt(0) + ","
				+ diameter.getValueAt(1) + "\tenabled=length " + length.isEnabled() + " min "
				+ ((JSpinner) member("minLengthSpinner")).isEnabled() + " diameter " + diameter.isEnabled()
				+ "\tboxes=" + box("limitDiameterCheckBox").isSelected() + "," + box("limitByLengthCheckBox").isSelected()
				+ "\tlabel=[" + ((JLabel) member("motorMountDimension")).getText() + "]\trows=" + table.getRowCount()
				+ "\tsorts=" + sorts);
		sorts = 0;
	}

	private void type(String model, String text) throws Exception {
		((DoubleModel) member(model)).getSpinnerModel().setValue(text);
	}

	/** The 28 mm mount of the chooser pins, 100 mm long. */
	private static InnerTube mount() {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		stage.addChild(body);
		InnerTube mount = new InnerTube();
		body.addChild(mount);
		mount.setOuterRadius(0.0145);
		mount.setInnerRadius(0.0145 - 0.0005);
		mount.setLength(0.1);
		mount.setMotorMount(true);
		return mount;
	}

	/** A new flight configuration of the mount's rocket. */
	private static FlightConfigurationId fcid(InnerTube mount) {
		return mount.getRocket().createFlightConfiguration(null).getId();
	}

	/** The length fields and slider, the limit boxes and the diameter slider. */
	private static void lengths(PrintStream out) throws Exception {
		MotorFilterPanelProbe s = new MotorFilterPanelProbe(out);
		s.state("initial");
		s.type("minLengthModel", "50.5");
		s.state("min 50.5");
		s.type("maxLengthModel", "40");
		s.state("max 40");
		s.slider("lengthSlider").setValueAt(1, 1000);
		s.state("thumb1 1000");
		s.slider("lengthSlider").setValueAt(0, 70);
		s.state("thumb0 70");
		s.type("minLengthModel", "2000");
		s.state("min 2000");
		s.type("minLengthModel", "0");
		s.state("min 0");
		s.type("maxLengthModel", "0.0505");
		s.state("max 0.0505");
		s.type("maxLengthModel", "1500");
		s.state("max 1500");
		s.slider("lengthSlider").setValueAt(1, 300);
		s.state("thumb1 300");
		s.slider("lengthSlider").setValueAt(1, 1000);
		s.state("thumb1 1000 again");
		s.type("minLengthModel", "120");
		s.state("min 120");

		InnerTube mount = mount();
		FlightConfigurationId fcid = fcid(mount);
		s.panel.setMotorMountAndConfig(fcid, mount);
		s.state("mount, min 120");
		s.box("limitByLengthCheckBox").doClick();
		s.state("limit length on");
		s.box("limitByLengthCheckBox").doClick();
		s.state("limit length off");
		s.box("limitDiameterCheckBox").doClick();
		s.state("limit diameter on");
		s.slider("diameterSlider").setValueAt(1, 11);
		s.state("diameter thumb1 11 while limited");
		s.box("limitDiameterCheckBox").doClick();
		s.state("limit diameter off");
		s.slider("diameterSlider").setValueAt(1, 2);
		s.slider("diameterSlider").setValueAt(0, 5);
		s.state("diameter thumbs 5,2");
		s.panel.setMotorMountAndConfig(fcid, mount);
		s.state("mount again");
	}

	/** A panel made with both limit boxes stored on, then opened for the mount. */
	private static void storedLimits(PrintStream out) throws Exception {
		ApplicationPreferences preferences = Application.getPreferences();
		preferences.putBoolean("motorFilterLimitDiameter", true);
		preferences.putBoolean("motorFilterLimitLength", true);
		MotorFilterPanelProbe s = new MotorFilterPanelProbe(out);
		s.state("stored limits");
		InnerTube mount = mount();
		s.panel.setMotorMountAndConfig(fcid(mount), mount);
		s.state("stored limits, mount");
		preferences.putBoolean("motorFilterLimitDiameter", false);
		preferences.putBoolean("motorFilterLimitLength", false);
	}

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		File database = new File(System.getProperty("qtrocket.repo"), "data/motors/initial_motors.db");
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		List<ThrustCurveMotor> read = ThrustCurveMotorSQLiteDatabase.readDatabase(database);
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
		UITheme.Themes.LIGHT.applyTheme();
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				lengths(out);
				storedLimits(out);
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
