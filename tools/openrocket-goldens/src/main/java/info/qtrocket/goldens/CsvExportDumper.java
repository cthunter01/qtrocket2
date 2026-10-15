package info.qtrocket.goldens;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.Deflater;
import java.util.zip.GZIPOutputStream;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
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
import info.openrocket.core.file.CSVExport;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;

/**
 * Writes the fixtures of QtRocket's CsvExport golden test (tests/data/csv): what OpenRocket's
 * {@code CSVExport.exportCSV()} writes for flight data STORED in the example designs. The designs
 * are loaded and never simulated again, so the numbers are those of the file and the export alone
 * decides the text, which QtRocket must reproduce byte for byte
 * (tests/core/file/csv_export_golden_tests.cpp lists the same fixtures).
 * <p>
 * A fixture is one branch of one simulation of one example, exported as OpenRocket's export dialog
 * exports it (SimulationExportPanel, CsvOptionPanel): every type of the branch in the order of
 * {@code getTypes()}, each in the default unit of its group, and the dialog's default options
 * ("," between fields, 3 decimals, exponential notation, "#" for comments, the simulation, field
 * and event comments all on), or one of four other sets of options ({@link #VARIANTS}). Each is
 * written gzipped to {@code <slug of the example>_sim<NN>_branch<B>[_variant<V>].csv.gz}.
 * <p>
 * The environment is that of OpenRocket's tests (ServicesForTesting overridden by PluginModule)
 * with the bundled thrust curve database, so that every motor of the examples is found and every
 * simulation has the status OpenRocket gives it after a load; no component presets are loaded (the
 * stored flight data does not depend on them). Every line of a CSV file ends with the platform's
 * line separator, so the dumper refuses to run where that is not "\n".
 * <p>
 * Usage: {@code CsvExportDumper --examples <dir> --motors <initial_motors.db> --out <dir> [--all]}.
 * {@code --all} writes every branch of every simulation of every example with the default options,
 * and the variants of the first fixture, instead of the fixtures alone (73 files, 3.1 MB: for a
 * comparison outside the repository). See README.md.
 */
public final class CsvExportDumper {

	/** A set of export options, in the order of CSVExport.exportCSV()'s arguments. */
	record Options(String fieldSeparator, int decimalPlaces, boolean exponentialNotation, String commentStarter,
			boolean simulationComments, boolean fieldComments, boolean eventComments) {
	}

	/** The options of OpenRocket's export dialog as it first opens. */
	static final Options DIALOG_DEFAULTS = new Options(",", 3, true, "#", true, true, true);

	/**
	 * The other sets of options, exported for the first fixture as {@code ..._variant1} to
	 * {@code ..._variant4}: every separator the dialog offers, 0, 6 and 15 decimals, the fixed
	 * notation, every comment starter it offers and one of its own, and the comment options in four
	 * more combinations of on and off.
	 */
	static final List<Options> VARIANTS = List.of(
			new Options(";", 6, false, "%", true, false, false),
			new Options(" ", 0, true, ";", false, true, true),
			new Options("\t", 15, true, "#", false, false, true),
			new Options(",", 3, true, "//", true, true, false));

	/** A branch to export: the file name in data/examples, the simulation and branch indices. */
	record Fixture(String example, int simulation, int branch, boolean withVariants) {
	}

	/**
	 * The committed fixtures: the tube-fin rocket's first simulation (with the variants), the
	 * booster branch of the two-stage rocket, the dual-parachute simulation with simulation warnings,
	 * the simulation of the extensions example with a SIM_WARN event and an extension's column (whose
	 * unit is the zero-width space of a dimensionless group), and the third branch of the
	 * three-stage rocket.
	 */
	static final List<Fixture> FIXTURES = List.of(
			new Fixture("Tube fin rocket.ork", 0, 0, true),
			new Fixture("Two stage high power rocket.ork", 0, 1, false),
			new Fixture("Dual parachute deployment.ork", 2, 0, false),
			new Fixture("Simulation extensions.ork", 2, 0, false),
			new Fixture("Three stage low power rocket.ork", 0, 2, false));

	private CsvExportDumper() {
	}

	public static void main(String[] args) throws Exception {
		Path examples = null;
		Path motors = null;
		Path out = null;
		boolean all = false;
		for (int i = 0; i < args.length; i++) {
			switch (args[i]) {
				case "--examples" -> examples = Path.of(args[++i]);
				case "--motors" -> motors = Path.of(args[++i]);
				case "--out" -> out = Path.of(args[++i]);
				case "--all" -> all = true;
				default -> throw new IllegalArgumentException("unknown argument " + args[i]);
			}
		}
		if (examples == null || motors == null || out == null) {
			throw new IllegalArgumentException(
					"usage: CsvExportDumper --examples <dir> --motors <initial_motors.db> --out <dir> [--all]");
		}
		if (!System.lineSeparator().equals("\n")) {
			throw new IllegalStateException("the line separator is not \\n: CSVExport would write another file");
		}
		Locale.setDefault(Locale.US);
		Files.createDirectories(out);
		bootstrap(motors);

		List<Fixture> fixtures = all ? everyBranch(examples) : FIXTURES;
		for (Fixture fixture : fixtures) {
			dump(examples, out, fixture);
		}
	}

	/** Every branch of every simulation with stored data of every example, the first with the variants. */
	private static List<Fixture> everyBranch(Path examples) throws Exception {
		List<Path> files;
		try (var list = Files.list(examples)) {
			files = list.filter(p -> p.toString().endsWith(".ork")).sorted().toList();
		}
		List<Fixture> fixtures = new ArrayList<>();
		for (Path file : files) {
			OpenRocketDocument doc = new GeneralRocketLoader(file.toFile()).load();
			for (int s = 0; s < doc.getSimulationCount(); s++) {
				FlightData data = doc.getSimulation(s).getSimulatedData();
				for (int b = 0; data != null && b < data.getBranchCount(); b++) {
					Fixture first = FIXTURES.get(0);
					String name = file.getFileName().toString();
					boolean variants = name.equals(first.example()) && s == first.simulation() && b == first.branch();
					fixtures.add(new Fixture(name, s, b, variants));
				}
			}
		}
		return fixtures;
	}

	/** Loads the example of {@code fixture} and writes its fixture and, if it has them, its variants. */
	private static void dump(Path examples, Path out, Fixture fixture) throws Exception {
		Path file = examples.resolve(fixture.example());
		OpenRocketDocument doc = new GeneralRocketLoader(file.toFile()).load();
		Simulation sim = doc.getSimulation(fixture.simulation());
		FlightDataBranch branch = sim.getSimulatedData().getBranch(fixture.branch());
		FlightDataType[] types = branch.getTypes();
		Unit[] units = new Unit[types.length];
		for (int i = 0; i < types.length; i++) {
			units[i] = types[i].getUnitGroup().getDefaultUnit();
		}
		String stem = String.format(Locale.ROOT, "%s_sim%02d_branch%d",
				GoldenDumper.slug(fixture.example().replaceAll("\\.ork$", "")), fixture.simulation(),
				fixture.branch());
		write(out.resolve(stem + ".csv.gz"), sim, branch, types, units, DIALOG_DEFAULTS);
		if (fixture.withVariants()) {
			for (int v = 0; v < VARIANTS.size(); v++) {
				write(out.resolve(stem + "_variant" + (v + 1) + ".csv.gz"), sim, branch, types, units,
						VARIANTS.get(v));
			}
		}
	}

	/** Exports {@code branch} with {@code options} into {@code target}, gzipped at the best compression. */
	private static void write(Path target, Simulation sim, FlightDataBranch branch, FlightDataType[] types,
			Unit[] units, Options options) throws IOException {
		ByteArrayOutputStream csv = new ByteArrayOutputStream();
		CSVExport.exportCSV(csv, sim, branch, types, units, options.fieldSeparator(), options.decimalPlaces(),
				options.exponentialNotation(), options.commentStarter(), options.simulationComments(),
				options.fieldComments(), options.eventComments());
		try (GZIPOutputStream gzip = new BestCompressionGzip(Files.newOutputStream(target))) {
			csv.writeTo(gzip);
		}
		System.out.println(target.getFileName() + ": " + csv.size() + " bytes, status " + sim.getStatus());
	}

	/** A gzip stream at Deflater.BEST_COMPRESSION (GZIPOutputStream's own level is the default, 6). */
	private static final class BestCompressionGzip extends GZIPOutputStream {
		BestCompressionGzip(OutputStream out) throws IOException {
			super(out);
			def.setLevel(Deflater.BEST_COMPRESSION);
		}
	}

	/**
	 * Sets up OpenRocket's Application as its tests do (BaseTestCase.setUp(): Guice with
	 * ServicesForTesting overridden by PluginModule), then, as ExampleFilesTest.setUp() does, a second
	 * injector that also binds the thrust curve database (the bundled initial_motors.db, read from a
	 * private copy) and an empty component preset database.
	 */
	private static void bootstrap(Path motorDatabase) throws Exception {
		Module testModule = new ServicesForTesting();
		Application.setInjector(Guice.createInjector(Modules.override(testModule).with(new PluginModule())));

		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Path dbCopy = Files.createTempFile("csv-export-dumper-motors", ".db");
		dbCopy.toFile().deleteOnExit();
		Files.copy(motorDatabase, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : motorList) {
			motors.addMotor(motor);
		}

		Module applicationModule = new ServicesForTesting();
		Module pluginModule = new PluginModule();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Injector injector = Guice.createInjector(Modules.override(applicationModule).with(dbOverrides), pluginModule);
		Application.setInjector(injector);
	}
}
