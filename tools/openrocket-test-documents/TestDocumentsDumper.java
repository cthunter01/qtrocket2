import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.document.StorageOptions;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.file.GeneralRocketSaver;
import info.openrocket.core.file.motor.GeneralMotorLoader;
import info.openrocket.core.file.openrocket.OpenRocketSaver;
import info.openrocket.core.logging.ErrorSet;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.motor.Manufacturer;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.extension.SimulationExtension;
import info.openrocket.core.simulation.extension.impl.ScriptingExtension;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.TestRockets;

/**
 * Writes tests/data/goldens/test-documents/: OpenRocket's saves of the 16 document makers of
 * TestRockets, in the Guice set-up of OpenRocketSaverTest (no component presets, a motor database
 * of the Estes A8 of core/src/test/resources/Estes_A8.rse and the 'A' F12X 'digestA'), but with
 * ServicesForTesting's English translator in place of the test's DebugTranslator, so that the
 * default names of the components are the English ones QtRocket gives. Each maker is saved with
 * OpenRocketSaver (saveSimulationData=true, as testCreateLoadSave asks) to
 * {@code <output directory>/<maker>.ork} and loaded back with GeneralRocketLoader; it is also
 * saved with GeneralRocketSaver into a temporary directory and loaded back from there.
 *
 * Prints, per maker, what QtRocket's tests pin by hand in
 * tests/core/file/openrocket/OpenRocketSaverTests.cpp: the estimate (kJavaEstimates), the presets
 * and statuses of the made document; the plain reload's warnings ("W") and the archive reload's
 * warnings ("ZW", kJavaReloadWarnings), components, simulations and statuses
 * (kJavaReloadedDocuments).
 *
 * Usage: java TestDocumentsDumper <OpenRocket checkout> <output directory>
 *
 * Run it through generate.sh, which compiles it with OpenRocket's ServicesForTesting against
 * OpenRocket's core classes. (Written as the probe TestDocumentsProbe of QtRocket's run 10b.)
 */
public final class TestDocumentsDumper {
	/** OpenRocketSaverTest.SIMULATION_EXTENSION_SCRIPT. */
	static final String SCRIPT = "// Test <  &\n// >\n// <![CDATA[";

	private TestDocumentsDumper() {
	}

	/** OpenRocketSaverTest.readMotor(), from the checkout's test resources. */
	static ThrustCurveMotor readA8(File openrocket) throws Exception {
		File file = new File(openrocket, "core/src/test/resources/Estes_A8.rse");
		try (InputStream is = new FileInputStream(file)) {
			return new GeneralMotorLoader().load(is, "Estes_A8.rse").get(0).build();
		}
	}

	static byte[] save(OpenRocketDocument doc, StorageOptions options) throws Exception {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		WarningSet w = new WarningSet();
		ErrorSet e = new ErrorSet();
		new OpenRocketSaver().save(out, doc, options, w, e);
		if (!w.isEmpty() || !e.isEmpty()) {
			System.out.println("  save warnings=" + w + " errors=" + e);
		}
		return out.toByteArray();
	}

	static OpenRocketDocument load(File f, List<String> warnings) throws Exception {
		GeneralRocketLoader loader = new GeneralRocketLoader(f);
		OpenRocketDocument d = loader.load();
		for (Warning w : loader.getWarnings()) {
			warnings.add(w.toString());
		}
		return d;
	}

	static int count(RocketComponent c) {
		int n = 1;
		for (RocketComponent child : c.getChildren()) {
			n += count(child);
		}
		return n;
	}

	static String presets(RocketComponent c) {
		StringBuilder sb = new StringBuilder();
		for (RocketComponent x : c) {
			ComponentPreset p = x.getPresetComponent();
			if (p != null) {
				sb.append(" ").append(x.getName()).append("=").append(p.getDigest());
			}
		}
		return sb.toString();
	}

	static String simulations(OpenRocketDocument doc) {
		StringBuilder sims = new StringBuilder();
		for (Simulation s : doc.getSimulations()) {
			sims.append(" ").append(s.getStatus());
			for (SimulationExtension x : s.getSimulationExtensions()) {
				if (x instanceof ScriptingExtension se) {
					sims.append("[script enabled=").append(se.isEnabled()).append(" same=")
							.append(SCRIPT.equals(se.getScript())).append("]");
				}
			}
		}
		return sims.toString();
	}

	static String entries(File zip) throws Exception {
		StringBuilder sb = new StringBuilder();
		try (ZipFile z = new ZipFile(zip)) {
			Enumeration<? extends ZipEntry> e = z.entries();
			while (e.hasMoreElements()) {
				sb.append(" ").append(e.nextElement().getName());
			}
		}
		return sb.toString();
	}

	static void deleteTree(Path root) throws IOException {
		try (Stream<Path> paths = Files.walk(root)) {
			for (Path p : paths.sorted(Comparator.reverseOrder()).toList()) {
				Files.delete(p);
			}
		}
	}

	public static void main(String[] args) throws Exception {
		if (args.length != 2) {
			System.err.println("usage: TestDocumentsDumper <OpenRocket checkout> <output directory>");
			System.exit(2);
		}
		File openrocket = new File(args[0]);
		File makers = new File(args[1]);
		makers.mkdirs();
		Path zips = Files.createTempDirectory("test-documents-zips");

		ThrustCurveMotorSetDatabase db = new ThrustCurveMotorSetDatabase();
		db.addMotor(readA8(openrocket));
		db.addMotor(new ThrustCurveMotor.Builder().setManufacturer(Manufacturer.getManufacturer("A"))
				.setDesignation("F12X").setDescription("Desc").setMotorType(Motor.Type.UNKNOWN)
				.setStandardDelays(new double[] {}).setDiameter(0.024).setLength(0.07)
				.setTimePoints(new double[] { 0, 1, 2 }).setThrustPoints(new double[] { 0, 1, 0 })
				.setCGPoints(new CoordinateIF[] { Coordinate.NUL, Coordinate.NUL, Coordinate.NUL })
				.setDigest("digestA").build());
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ComponentPresetDao.class).toInstance(new ComponentPresetDatabase());
				bind(MotorDatabase.class).toInstance(db);
			}
		};
		Injector injector = Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule());
		Application.setInjector(injector);

		// The order of OpenRocketSaverTest.testCreateLoadSave.
		Map<String, Supplier<OpenRocketDocument>> list = new LinkedHashMap<>();
		list.put("v100", TestRockets::makeTestRocket_v100);
		list.put("v101_withFinTabs", TestRockets::makeTestRocket_v101_withFinTabs);
		list.put("v101_withTubeCouplerChild", TestRockets::makeTestRocket_v101_withTubeCouplerChild);
		list.put("v104_withSimulationData", TestRockets::makeTestRocket_v104_withSimulationData);
		list.put("v104_withMotor", TestRockets::makeTestRocket_v104_withMotor);
		list.put("v105_withComponentPreset", TestRockets::makeTestRocket_v105_withComponentPreset);
		list.put("v105_withCustomExpression", TestRockets::makeTestRocket_v105_withCustomExpression);
		list.put("v105_withLowerStageRecoveryDevice", TestRockets::makeTestRocket_v105_withLowerStageRecoveryDevice);
		list.put("v106_withAppearance", TestRockets::makeTestRocket_v106_withAppearance);
		list.put("v106_withMotorMountIgnitionConfig", TestRockets::makeTestRocket_v106_withMotorMountIgnitionConfig);
		list.put("v106_withRecoveryDeviceDeploymentConfig",
				TestRockets::makeTestRocket_v106_withRecoveryDeviceDeploymentConfig);
		list.put("v106_withStageSeparationConfig", TestRockets::makeTestRocket_v106_withStageSeparationConfig);
		list.put("v110_withSimulationExtension", () -> TestRockets.makeTestRocket_v110_withSimulationExtension(SCRIPT));
		list.put("v108_withBoosters", TestRockets::makeTestRocket_v108_withBoosters);
		list.put("v108_withDisabledStage", TestRockets::makeTestRocket_v108_withDisabledStage);
		list.put("for_estimateFileSize", TestRockets::makeTestRocket_for_estimateFileSize);
		StorageOptions withData = new StorageOptions();
		withData.setSaveSimulationData(true);
		try {
			System.out.println("== makers (OpenRocketSaver, saveSimulationData=true, then GeneralRocketLoader)");
			for (Map.Entry<String, Supplier<OpenRocketDocument>> m : list.entrySet()) {
				OpenRocketDocument doc = m.getValue().get();
				System.out.printf("%s estimate=%d presets=%s statuses=%s%n", m.getKey(),
						new OpenRocketSaver().estimateFileSize(doc, withData), presets(doc.getRocket()),
						simulations(doc));
				byte[] xml = save(doc, withData);
				File f = new File(makers, m.getKey() + ".ork");
				Files.write(f.toPath(), xml);
				List<String> warnings = new ArrayList<>();
				OpenRocketDocument back = load(f, warnings);
				System.out.printf("  plain bytes=%d components=%d sims=%d statuses=%s%n", xml.length,
						count(back.getRocket()), back.getSimulationCount(), simulations(back));
				for (String w : warnings) {
					System.out.println("  W " + w);
				}
				File z = zips.resolve(m.getKey() + ".ork").toFile();
				new GeneralRocketSaver().save(z, doc, withData);
				List<String> zipWarnings = new ArrayList<>();
				OpenRocketDocument zipBack = load(z, zipWarnings);
				System.out.printf("  zip entries=%s components=%d sims=%d statuses=%s same-warnings=%b%n",
						entries(z), count(zipBack.getRocket()), zipBack.getSimulationCount(),
						simulations(zipBack), zipWarnings.equals(warnings));
				for (String w : zipWarnings) {
					System.out.println("  ZW " + w);
				}
			}
		} finally {
			deleteTree(zips);
		}
	}
}
