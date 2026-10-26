package probe;

import java.io.File;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.util.List;

import javax.swing.SwingUtilities;

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
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ChooserScenarios;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * What OpenRocket's motor chooser shows and decides for its bundled motor database (the family
 * "chooser" of tools/openrocket-motor-chooser, M3 run 1 part C4): builds OpenRocket's real
 * ThrustCurveMotorSelectionPanel headless and drives it (ChooserScenarios), one fact per line.
 * <p>
 * usage: MotorChooserProbe MOTOR_DATABASE CUSTOM_MOTOR_FILE
 * <p>
 * The database is read with ThrustCurveMotorSQLiteDatabase.readDatabase() and filled into a
 * ThrustCurveMotorSetDatabase with addMotor() in the order read, as OpenRocket's start-up and
 * QtRocket's Test::bundledMotorDatabase() do. The preferences are OpenRocket's SwingPreferences over
 * the java.util.prefs store generate.sh isolates; each group of scenarios starts from cleared
 * preferences and default units. The UUIDs of the rockets the scenarios build are seeded
 * (DeterministicUuids), so the transcript is the same on every run.
 */
public final class MotorChooserProbe {

	private MotorChooserProbe() {
	}

	public static void main(String[] args) throws Exception {
		DeterministicUuids.install();
		System.setProperty("openrocket.3d.disable", "true");
		File database = new File(args[0]);
		File customMotor = new File(args[1]);
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		List<ThrustCurveMotor> read = ThrustCurveMotorSQLiteDatabase.readDatabase(database);
		for (ThrustCurveMotor motor : read) {
			motors.addMotor(motor);
		}
		bootstrap(motors);
		UITheme.Themes.LIGHT.applyTheme();
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				ChooserScenarios.run(motors, read.size(), customMotor, out);
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

	/** The Guice injector OpenRocket's tests use, with SwingPreferences and the motor database. */
	private static void bootstrap(ThrustCurveMotorSetDatabase motors) {
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
	}

	/** Restarts the UUID sequence (before each design a scenario builds). */
	public static void reseed(String key) {
		DeterministicUuids.reseed(key);
	}
}
