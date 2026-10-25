package info.qtrocket.goldens;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.google.inject.Guice;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.material.Material;
import info.openrocket.core.motor.Manufacturer;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.TypedKey;
import info.openrocket.core.preset.TypedPropertyMap;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.ExternalComponent.Finish;
import info.openrocket.core.rocketcomponent.Transition.Shape;
import info.openrocket.core.startup.Application;

/**
 * Writes {@value #OUTPUT} (relative to the goldens directory): every component preset OpenRocket's
 * {@link OpenRocketComponentLoader} reads from the bundled .orc files, file by file, with every
 * property in the order the loader put it and its value in SI units, and the order in which
 * OpenRocket's {@link ComponentPresetDatabase} holds them once each file was added with addAll().
 * QtRocket's preset golden test (tests/core/goldens/preset_golden_tests.cpp) loads data/components,
 * the same files byte for byte, with its ComponentPresetDatabaseLoader and compares. See README.md
 * ("presets/presets.json").
 * <p>
 * The files are those of the harness's preset database ({@link GoldenDumper#presetFiles}: internal/
 * from OpenRocket's resources, database/ from the openrocket-database submodule, never the build copy),
 * read in the order of QtRocket's DirectoryIterator, by which ComponentPresetDatabaseLoader reads the
 * bundled directory: depth first, the entries of each directory in the order of their UTF-8 names
 * compared byte by byte ({@link #DIRECTORY_ORDER}). For the bundled files that is also the order of the
 * harness's sorted paths, which manifest.json lists.
 * <p>
 * GoldenDumper writes the file in every full run and its entry into manifest.json ({@code presets}).
 * It runs alone as well ({@link #main}, through {@code generate.sh --presets}): then it writes the file
 * and replaces the line of that entry in the existing manifest.json, and nothing else. Both give the
 * same bytes: what is written depends on the preset files and OpenRocket's code alone. The UUID
 * sequence is reseeded before the files are read ({@link DeterministicUuids}), although reading presets
 * makes no UUID today.
 * <p>
 * Usage: {@code PresetDumper --openrocket <checkout> --out <goldens directory>}
 */
public final class PresetDumper {

	/** The golden file, relative to the goldens directory. */
	static final String OUTPUT = "presets/presets.json";

	/** The key of the entry in manifest.json. */
	static final String MANIFEST_KEY = "presets";

	/**
	 * QtRocket's DirectoryIterator order of two relative paths ('/' between their parts): part by part,
	 * each compared as its UTF-8 bytes (unsigned), a path that runs out first before the longer one.
	 */
	static final java.util.Comparator<String> DIRECTORY_ORDER = (a, b) -> {
		String[] x = a.split("/", -1);
		String[] y = b.split("/", -1);
		for (int i = 0; i < Math.min(x.length, y.length); i++) {
			int c = Arrays.compareUnsigned(x[i].getBytes(StandardCharsets.UTF_8), y[i].getBytes(StandardCharsets.UTF_8));
			if (c != 0) {
				return c;
			}
		}
		return Integer.compare(x.length, y.length);
	};

	private PresetDumper() {
	}

	public static void main(String[] args) throws Exception {
		// Before anything creates a UUID (see DeterministicUuids).
		DeterministicUuids.install();
		Locale.setDefault(Locale.US);

		Path openrocket = null;
		Path out = null;
		for (int i = 0; i < args.length; i += 2) {
			String option = args[i];
			if (i + 1 >= args.length) {
				throw new IllegalArgumentException("Missing value for " + option);
			}
			Path value = Path.of(args[i + 1]).toAbsolutePath().normalize();
			switch (option) {
				case "--openrocket" -> openrocket = value;
				case "--out" -> out = value;
				default -> throw new IllegalArgumentException("Unknown option " + option);
			}
		}
		if (openrocket == null || out == null) {
			throw new IllegalArgumentException("Usage: PresetDumper --openrocket <checkout> --out <goldens directory>");
		}

		// BaseTestCase.setUp(), the first step of GoldenDumper.bootstrap(): what the loader reads of the
		// application (the materials of Databases, the preferences) is ServicesForTesting's.
		Application.setInjector(
				Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule())));

		Map<String, Object> entry = writeTo(openrocket.resolve("core"), out);
		replaceManifestEntry(out.resolve("manifest.json"), entry);
		GoldenDumper.log("presets: " + entry);
	}

	/**
	 * Reads the bundled preset files of OpenRocket's {@code core} directory, writes {@value #OUTPUT}
	 * under {@code out} and returns its manifest entry: the file, its SHA-256 and the counts.
	 */
	static Map<String, Object> writeTo(Path core, Path out) throws IOException {
		DeterministicUuids.reseed(MANIFEST_KEY);
		List<Map.Entry<String, Path>> files = new ArrayList<>(GoldenDumper.presetFiles(core).entrySet());
		files.sort(Map.Entry.comparingByKey(DIRECTORY_ORDER));

		ComponentPresetDatabase database = new ComponentPresetDatabase();
		IdentityHashMap<ComponentPreset, String> ids = new IdentityHashMap<>();
		List<Object> fileList = Json.array();
		int presetCount = 0;
		for (Map.Entry<String, Path> file : files) {
			List<ComponentPreset> loaded;
			try (InputStream is = Files.newInputStream(file.getValue())) {
				loaded = new ArrayList<>(new OpenRocketComponentLoader().load(is,
						file.getValue().getFileName().toString()));
			}
			Map<String, Object> f = Json.object();
			f.put("file", file.getKey());
			f.put("sha256", GoldenDumper.sha256(file.getValue()));
			f.put("presetCount", loaded.size());
			List<Object> presets = Json.array();
			for (int i = 0; i < loaded.size(); i++) {
				ids.put(loaded.get(i), file.getKey() + "#" + i);
				presets.add(preset(loaded.get(i)));
			}
			f.put("presets", presets);
			fileList.add(f);
			presetCount += loaded.size();
			database.addAll(loaded);
		}

		List<Object> order = Json.array();
		for (ComponentPreset preset : database.listAll()) {
			order.add(ids.get(preset));
		}

		Map<String, Object> root = Json.object();
		root.put("schema", "presets");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("generator", "tools/openrocket-goldens (PresetDumper)");
		root.put("fileCount", files.size());
		root.put("presetCount", presetCount);
		root.put("files", fileList);
		root.put("databaseCount", database.listAll().size());
		root.put("databaseOrder", order);
		byte[] bytes = Json.bytes(root);
		Path file = out.resolve(OUTPUT);
		Files.createDirectories(file.getParent());
		Files.write(file, bytes);

		Map<String, Object> entry = Json.object();
		entry.put("json", OUTPUT);
		entry.put("sha256", GoldenDumper.sha256(file));
		entry.put("fileCount", files.size());
		entry.put("presetCount", presetCount);
		entry.put("databaseCount", database.listAll().size());
		return entry;
	}

	/**
	 * Replaces the line of the {@value #MANIFEST_KEY} entry in the manifest {@code manifest} with
	 * {@code entry}, written as the full run writes it (one line: every value is a scalar). Fails when
	 * the manifest has no such line: it is then a manifest of before the preset golden, which only a
	 * full run brings up to date.
	 */
	static void replaceManifestEntry(Path manifest, Map<String, Object> entry) throws IOException {
		Map<String, Object> wrapper = new LinkedHashMap<>();
		wrapper.put(MANIFEST_KEY, entry);
		wrapper.put("end", 0);
		// {"presets": {...}, "end": 0} is not flat, so Json writes one member per line: the second line
		// is the entry as it stands in the manifest, ending with the comma of a member that is followed
		// by another, or without it as the last member.
		String[] lines = new String(Json.bytes(wrapper), StandardCharsets.UTF_8).split("\n", -1);
		String line = lines[1].substring(0, lines[1].length() - 1);
		String prefix = " \"" + MANIFEST_KEY + "\": {";
		List<String> text = new ArrayList<>(Arrays.asList(
				new String(Files.readAllBytes(manifest), StandardCharsets.UTF_8).split("\n", -1)));
		int found = -1;
		for (int i = 0; i < text.size(); i++) {
			if (text.get(i).startsWith(prefix)) {
				if (found >= 0) {
					throw new IOException(manifest + " has two lines of the " + MANIFEST_KEY + " entry");
				}
				found = i;
			}
		}
		if (found < 0) {
			throw new IOException(manifest + " has no " + MANIFEST_KEY + " entry: run the full generate.sh");
		}
		boolean comma = text.get(found).endsWith(",");
		text.set(found, comma ? line + "," : line);
		Files.write(manifest, String.join("\n", text).getBytes(StandardCharsets.UTF_8));
	}

	/** A preset: its type, manufacturer, part number and digest, then every property. */
	static Map<String, Object> preset(ComponentPreset p) {
		Map<String, Object> o = Json.object();
		o.put("type", p.getType().name());
		o.put("manufacturer", p.getManufacturer().getSimpleName());
		o.put("manufacturerDisplayName", p.getManufacturer().getDisplayName());
		o.put("partNo", p.getPartNo());
		o.put("digest", p.getDigest());
		Map<String, Object> properties = Json.object();
		TypedPropertyMap map = properties(p);
		for (TypedKey<?> key : map.keySet()) {
			properties.put(key.getName(), value(map.get(key)));
		}
		o.put("properties", properties);
		return o;
	}

	/**
	 * The property map of {@code p} itself, which keeps the order of the loader's put() calls (the
	 * preset has no public accessor for it).
	 */
	static TypedPropertyMap properties(ComponentPreset p) {
		try {
			Field field = ComponentPreset.class.getDeclaredField("properties");
			field.setAccessible(true);
			return (TypedPropertyMap) field.get(p);
		} catch (ReflectiveOperationException e) {
			throw new IllegalStateException(e);
		}
	}

	/**
	 * The JSON value of a property: a number (a double by Double.toString, an integer as such), a boolean
	 * or a string as it is; a manufacturer by its simple name; a preset type, a shape and a finish by
	 * the constant's name; a material as an object; an image as its bytes in base64.
	 */
	static Object value(Object v) {
		if (v instanceof Double || v instanceof Integer || v instanceof Boolean || v instanceof String) {
			return v;
		}
		if (v instanceof Manufacturer m) {
			return m.getSimpleName();
		}
		if (v instanceof ComponentPreset.Type t) {
			return t.name();
		}
		if (v instanceof Shape s) {
			return s.name();
		}
		if (v instanceof Finish f) {
			return f.name();
		}
		if (v instanceof Material m) {
			return material(m);
		}
		if (v instanceof byte[] b) {
			return Base64.getEncoder().encodeToString(b);
		}
		throw new IllegalStateException("A property value of an unknown kind: " + (v == null ? "null" : v.getClass()));
	}

	/** A material: its type's name, name, density and shear modulus in SI units, group and flags. */
	static Map<String, Object> material(Material m) {
		Map<String, Object> o = Json.object();
		o.put("type", m.getType().name());
		o.put("name", m.getName());
		o.put("density", m.getDensity());
		o.put("shearModulus", m.getInPlaneShearModulus());
		o.put("group", m.getGroup().getDatabaseString());
		o.put("userDefined", m.isUserDefined());
		o.put("documentMaterial", m.isDocumentMaterial());
		return o;
	}
}
