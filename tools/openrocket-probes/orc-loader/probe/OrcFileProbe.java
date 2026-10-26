package probe;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;
import java.util.stream.Stream;

import com.google.inject.Guice;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.startup.Application;

/**
 * Milestone 3 run 1, part O2: feeds OpenRocketComponentLoader the .orc fixtures of QtRocket's tests
 * (tests/data/presets/*.orc, in the order of their names) and prints what comes out of each, in
 * OrcEdgeProbe's format, on OpenRocket's core with ServicesForTesting. The repository is the system
 * property qtrocket.repo, which run.sh sets. tests/core/file/preset/OpenRocketComponentLoaderTests.cpp
 * pins what this prints by hand.
 */
public final class OrcFileProbe {

	public static void main(String[] args) throws Exception {
		Locale.setDefault(Locale.US);
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule())));
		Path fixtures = Path.of(System.getProperty("qtrocket.repo"), "tests", "data", "presets");
		List<Path> files;
		try (Stream<Path> listing = Files.list(fixtures)) {
			files = listing.filter(p -> p.getFileName().toString().endsWith(".orc")).sorted().toList();
		}
		for (Path file : files) {
			OrcEdgeProbe.run("file " + file.getFileName(), Files.readAllBytes(file));
		}
	}
}
