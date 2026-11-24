package info.qtrocket.goldens;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import java.util.zip.Deflater;
import java.util.zip.GZIPInputStream;
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
import info.openrocket.core.unit.UnitGroup;

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
 * and event comments all on), or one of six other sets of options and units ({@link #VARIANTS}).
 * Each is written gzipped to {@code <slug of the example>_sim<NN>_branch<B>[_variant<V>].csv.gz}.
 * <p>
 * The environment is that of OpenRocket's tests (ServicesForTesting overridden by PluginModule)
 * with the bundled thrust curve database, so that every motor of the examples is found and every
 * simulation has the status OpenRocket gives it after a load; no component presets are loaded (the
 * stored flight data does not depend on them). Every line of a CSV file ends with the platform's
 * line separator, so the dumper refuses to run where that is not "\n".
 * <p>
 * Usage: {@code CsvExportDumper --examples <dir> --motors <initial_motors.db> --out <dir> [--all]
 * [--check]}. {@code --all} writes every branch of every simulation of every example with the
 * default options, and the variants of the first fixture, instead of the fixtures alone (75 files,
 * 3.2 MB: for a comparison outside the repository). {@code --check} writes nothing: it compares
 * what it would write with the files in {@code --out}, after inflating them (their gzip bytes
 * depend on the JDK's zlib), prints {@code same}, {@code DIFFERS} or {@code MISSING} for each
 * and {@code EXTRA} for any other {@code .csv.gz} file there, and exits with status 1 unless every
 * file is the same. See README.md.
 */
public final class CsvExportDumper {

	/** A set of export options, in the order of CSVExport.exportCSV()'s arguments. */
	record Options(String fieldSeparator, int decimalPlaces, boolean exponentialNotation, String commentStarter,
			boolean simulationComments, boolean fieldComments, boolean eventComments) {
	}

	/** The options of OpenRocket's export dialog as it first opens. */
	static final Options DIALOG_DEFAULTS = new Options(",", 3, true, "#", true, true, true);

	/** The unit of its group that each field of a variant is exported in. */
	enum Units {
		/** The default unit of the group, the one the export dialog starts with. */
		DEFAULT,
		/** The last unit of the group, the last one the dialog's unit selector offers. */
		LAST
	}

	/** A set of options and the units of the fields, for a variant of the first fixture. */
	record Variant(Options options, Units units) {
	}

	/**
	 * The other sets of options and units, exported for the first fixture as {@code ..._variant1}
	 * to {@code ..._variant6}: every separator the dialog offers and two that a user types ("|"
	 * and the multi-character "<->"), 0, 2, 6 and 15 decimals, the fixed notation, every comment
	 * starter it offers, two of its own ("//", "!!") and the empty one, the comment options in four
	 * more combinations of on and off, and the last unit of every unit group (for 39 of the first
	 * fixture's 58 fields a unit other than the default: minutes, nautical miles, knots, G, arc
	 * minutes, rpm, pounds, kgf, the offset unit of degrees Fahrenheit, and the units of south
	 * latitude and west longitude, which negate the value).
	 */
	static final List<Variant> VARIANTS = List.of(
			new Variant(new Options(";", 6, false, "%", true, false, false), Units.DEFAULT),
			new Variant(new Options(" ", 0, true, ";", false, true, true), Units.DEFAULT),
			new Variant(new Options("\t", 15, true, "#", false, false, true), Units.DEFAULT),
			new Variant(new Options(",", 3, true, "//", true, true, false), Units.DEFAULT),
			new Variant(new Options("|", 2, false, "!!", true, true, true), Units.LAST),
			new Variant(new Options("<->", 0, false, "", true, true, true), Units.DEFAULT));

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
		boolean check = false;
		for (int i = 0; i < args.length; i++) {
			switch (args[i]) {
				case "--examples" -> examples = Path.of(args[++i]);
				case "--motors" -> motors = Path.of(args[++i]);
				case "--out" -> out = Path.of(args[++i]);
				case "--all" -> all = true;
				case "--check" -> check = true;
				default -> throw new IllegalArgumentException("unknown argument " + args[i]);
			}
		}
		if (examples == null || motors == null || out == null) {
			throw new IllegalArgumentException("usage: CsvExportDumper --examples <dir> --motors <initial_motors.db>"
					+ " --out <dir> [--all] [--check]");
		}
		if (!System.lineSeparator().equals("\n")) {
			throw new IllegalStateException("the line separator is not \\n: CSVExport would write another file");
		}
		Locale.setDefault(Locale.US);
		if (!check) {
			Files.createDirectories(out);
		}
		bootstrap(motors);

		List<Fixture> fixtures = all ? everyBranch(examples) : FIXTURES;
		Target target = check ? new FixtureChecker(out) : new FixtureWriter(out);
		for (Fixture fixture : fixtures) {
			dump(examples, target, fixture);
		}
		if (!target.finish()) {
			System.exit(1);
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
	private static void dump(Path examples, Target target, Fixture fixture) throws Exception {
		Path file = examples.resolve(fixture.example());
		OpenRocketDocument doc = new GeneralRocketLoader(file.toFile()).load();
		Simulation sim = doc.getSimulation(fixture.simulation());
		FlightDataBranch branch = sim.getSimulatedData().getBranch(fixture.branch());
		FlightDataType[] types = branch.getTypes();
		String stem = String.format(Locale.ROOT, "%s_sim%02d_branch%d",
				GoldenDumper.slug(fixture.example().replaceAll("\\.ork$", "")), fixture.simulation(),
				fixture.branch());
		export(target, stem + ".csv.gz", sim, branch, types, units(types, Units.DEFAULT), DIALOG_DEFAULTS);
		if (fixture.withVariants()) {
			for (int v = 0; v < VARIANTS.size(); v++) {
				Variant variant = VARIANTS.get(v);
				export(target, stem + "_variant" + (v + 1) + ".csv.gz", sim, branch, types,
						units(types, variant.units()), variant.options());
			}
		}
	}

	/** The unit of each of {@code types}, in order, that {@code choice} names. */
	private static Unit[] units(FlightDataType[] types, Units choice) {
		Unit[] units = new Unit[types.length];
		for (int i = 0; i < types.length; i++) {
			UnitGroup group = types[i].getUnitGroup();
			units[i] = switch (choice) {
				case DEFAULT -> group.getDefaultUnit();
				case LAST -> group.getUnit(group.getUnitCount() - 1);
			};
		}
		return units;
	}

	/** Exports {@code branch} with {@code options} and hands the text to {@code target} as {@code name}. */
	private static void export(Target target, String name, Simulation sim, FlightDataBranch branch,
			FlightDataType[] types, Unit[] units, Options options) throws IOException {
		ByteArrayOutputStream csv = new ByteArrayOutputStream();
		CSVExport.exportCSV(csv, sim, branch, types, units, options.fieldSeparator(), options.decimalPlaces(),
				options.exponentialNotation(), options.commentStarter(), options.simulationComments(),
				options.fieldComments(), options.eventComments());
		target.accept(name, csv.toByteArray(), sim);
	}

	/** What becomes of an exported fixture: written, or compared with the file already there. */
	private interface Target {
		/** Takes the text {@code csv} of the fixture file {@code name}, exported from {@code sim}. */
		void accept(String name, byte[] csv, Simulation sim) throws IOException;

		/** Called after the last fixture: whether all went well. */
		boolean finish() throws IOException;
	}

	/** Writes each fixture into the output directory, gzipped at the best compression. */
	private record FixtureWriter(Path out) implements Target {
		@Override
		public void accept(String name, byte[] csv, Simulation sim) throws IOException {
			try (GZIPOutputStream gzip = new BestCompressionGzip(Files.newOutputStream(out.resolve(name)))) {
				gzip.write(csv);
			}
			System.out.println(name + ": " + csv.length + " bytes, status " + sim.getStatus());
		}

		@Override
		public boolean finish() {
			return true;
		}
	}

	/**
	 * Compares each fixture with the file of the same name in the output directory, inflated, and
	 * remembers the names it was given, so that {@link #finish()} can name the other files there.
	 */
	private static final class FixtureChecker implements Target {
		private final Path out;
		private final Set<String> names = new TreeSet<>();
		private boolean allSame = true;

		FixtureChecker(Path out) {
			this.out = out;
		}

		@Override
		public void accept(String name, byte[] csv, Simulation sim) throws IOException {
			names.add(name);
			Path file = out.resolve(name);
			String verdict;
			if (!Files.isRegularFile(file)) {
				verdict = "MISSING";
			} else {
				byte[] stored;
				try (GZIPInputStream gzip = new GZIPInputStream(Files.newInputStream(file))) {
					stored = gzip.readAllBytes();
				}
				verdict = Arrays.equals(stored, csv) ? "same" : "DIFFERS";
			}
			allSame &= verdict.equals("same");
			System.out.println(name + ": " + verdict);
		}

		/** Prints the {@code .csv.gz} files of the directory that no fixture names; whether all was the same. */
		@Override
		public boolean finish() throws IOException {
			if (Files.isDirectory(out)) {
				try (var list = Files.list(out)) {
					for (Path file : list.sorted().toList()) {
						String name = file.getFileName().toString();
						if (name.endsWith(".csv.gz") && !names.contains(name)) {
							System.out.println(name + ": EXTRA");
							allSame = false;
						}
					}
				}
			}
			System.out.println(allSame ? "check: same" : "check: DIFFERS");
			return allSame;
		}
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
