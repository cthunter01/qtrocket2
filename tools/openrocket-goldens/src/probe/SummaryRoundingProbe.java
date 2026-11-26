package info.qtrocket.goldens;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.util.TextUtil;

/**
 * The probe behind the tolerance of InteropCheck's summary values of a flight QtRocket ran (README.md, "Interop
 * check": checkRunSummary(), summaryAgreement()): does OpenRocket's own run keep the summary values of a fresh
 * flight through a save with the simulation data and a load? It runs simulation 0 of each example {@code runs}
 * times (random seeds 0 to runs - 1), saves the design with the data (OpenRocketSaver, in the harness's
 * environment), loads the save (FlightDataHandler computes the summary again from the branches, whose values the
 * save rounded to six decimal places) and prints every summary value whose stored text (TextUtil.doubleToString,
 * six places) the load changed. Not run by generate.sh or interop.sh, nor compiled by Gradle; by hand, after
 * either built the harness (build/goldens-classpath.txt), from tools/openrocket-goldens, with Java 17:
 *
 * <pre>
 * javac -cp "$(cat build/goldens-classpath.txt)" -d build/probe src/probe/SummaryRoundingProbe.java
 * java -Djava.awt.headless=true -cp "build/probe:$(cat build/goldens-classpath.txt)" \
 *     info.qtrocket.goldens.SummaryRoundingProbe &lt;openrocket checkout&gt; build/work 40 \
 *     "../../data/examples/A simple model rocket.ork" "../../data/examples/Clustered motors.ork"
 * </pre>
 *
 * What it printed (Milestone 4, run 4, part X4; OpenRocket 5f164fd0e, JDK 17): of the 400 summary values of
 * each example, the deployment velocity changed in its last digit in 3 runs of "A simple model rocket" (seeds 9,
 * 14 and 30: 2.61234 to 2.612339, 2.798468 to 2.798467, 2.801524 to 2.801525) and in 6 of "Clustered motors"
 * (seeds 3, 4, 9, 17, 19 and 28), nothing else.
 */
public final class SummaryRoundingProbe {
	public static void main(String[] args) throws Exception {
		DeterministicUuids.install();
		Locale.setDefault(Locale.US);
		Path processDirectory = GoldenDumper.useProcessDirectory(Path.of(args[1]));
		GoldenDumper.bootstrap(Path.of(args[0]).resolve("core"), processDirectory);
		int runs = Integer.parseInt(args[2]);
		for (int e = 3; e < args.length; e++) {
			int differing = 0;
			int values = 0;
			for (int k = 0; k < runs; k++) {
				OpenRocketDocument doc = new GeneralRocketLoader(Path.of(args[e]).toFile()).load();
				Simulation sim = doc.getSimulation(0);
				sim.getOptions().setRandomSeed(k);
				sim.simulate();
				Map<String, String> before = summary(sim.getSimulatedData());
				Path file = processDirectory.resolve("probe.ork");
				Files.write(file, GoldenDumper.saveXml(doc, true));
				OpenRocketDocument loaded = new GeneralRocketLoader(file.toFile()).load();
				Map<String, String> after = summary(loaded.getSimulation(0).getSimulatedData());
				Files.delete(file);
				for (Map.Entry<String, String> entry : before.entrySet()) {
					values++;
					if (!entry.getValue().equals(after.get(entry.getKey()))) {
						differing++;
						System.out.println(Path.of(args[e]).getFileName() + " seed " + k + ": " + entry.getKey() + " stored "
								+ entry.getValue() + ", recomputed on load " + after.get(entry.getKey()));
					}
				}
			}
			System.out.println(Path.of(args[e]).getFileName() + ": " + runs + " runs, " + values + " summary values, "
					+ differing + " differ after a save and a load");
		}
		System.exit(0);
	}

	private static Map<String, String> summary(FlightData data) {
		Map<String, String> s = new LinkedHashMap<>();
		s.put("maxaltitude", text(data.getMaxAltitude()));
		s.put("maxvelocity", text(data.getMaxVelocity()));
		s.put("maxacceleration", text(data.getMaxAcceleration()));
		s.put("maxmach", text(data.getMaxMachNumber()));
		s.put("timetoapogee", text(data.getTimeToApogee()));
		s.put("flighttime", text(data.getFlightTime()));
		s.put("groundhitvelocity", text(data.getGroundHitVelocity()));
		s.put("launchrodvelocity", text(data.getLaunchRodVelocity()));
		s.put("deploymentvelocity", text(data.getDeploymentVelocity()));
		s.put("optimumdelay", text(data.getOptimumDelay()));
		return s;
	}

	private static String text(double v) {
		return TextUtil.doubleToString(v, TextUtil.STORAGE_DECIMAL_PLACES);
	}
}
