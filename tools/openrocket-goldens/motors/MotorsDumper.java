import ch.qos.logback.classic.Level;
import ch.qos.logback.classic.Logger;
import ch.qos.logback.classic.spi.ILoggingEvent;
import ch.qos.logback.core.read.ListAppender;

import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.file.motor.GeneralMotorLoader;
import info.openrocket.core.motor.ThrustCurveMotor;

import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Writes tests/data/goldens/motors.json: every thrust curve OpenRocket loads from the bundled
 * motor database and from the motor test files, with the values QtRocket must reproduce, and
 * the curves OpenRocket's database reader skips.
 *
 * Usage: java MotorsDumper <initial_motors.db> <motor test directory> <output.json> <version>
 *
 * Run it through dump-motors.sh, which compiles it against OpenRocket's core classes.
 */
public final class MotorsDumper {

	private MotorsDumper() {
	}

	public static void main(String[] args) throws Exception {
		if (args.length != 4) {
			System.err.println("usage: MotorsDumper <initial_motors.db> <motor dir> <output.json> <version>");
			System.exit(2);
		}
		File database = new File(args[0]);
		File motorDirectory = new File(args[1]);
		File output = new File(args[2]);
		String version = args[3];

		// OpenRocket logs the curves it skips instead of returning them: capture the log.
		Logger root = (Logger) LoggerFactory.getLogger(org.slf4j.Logger.ROOT_LOGGER_NAME);
		root.setLevel(Level.WARN);
		Logger dbLogger = (Logger) LoggerFactory.getLogger(ThrustCurveMotorSQLiteDatabase.class);
		dbLogger.setLevel(Level.DEBUG);
		dbLogger.setAdditive(false);
		ListAppender<ILoggingEvent> appender = new ListAppender<>();
		appender.start();
		dbLogger.addAppender(appender);

		List<ThrustCurveMotor> motors = ThrustCurveMotorSQLiteDatabase.readDatabase(database);

		StringBuilder json = new StringBuilder();
		json.append("{\n");
		json.append("  \"generator\": \"tools/openrocket-goldens/motors/MotorsDumper.java\",\n");
		json.append("  \"openrocket\": ").append(string(version)).append(",\n");
		json.append("  \"database\": {\n");
		json.append("    \"file\": \"data/motors/initial_motors.db\",\n");
		json.append("    \"curves\": [\n");
		for (int i = 0; i < motors.size(); i++) {
			json.append("      ").append(motor(motors.get(i)));
			json.append(i + 1 < motors.size() ? ",\n" : "\n");
		}
		json.append("    ],\n");
		json.append("    \"skipped\": [\n");
		List<String> skipped = new ArrayList<>();
		for (ILoggingEvent event : appender.list) {
			String entry = skipped(event);
			if (entry != null) {
				skipped.add(entry);
			}
		}
		for (int i = 0; i < skipped.size(); i++) {
			json.append("      ").append(skipped.get(i));
			json.append(i + 1 < skipped.size() ? ",\n" : "\n");
		}
		json.append("    ]\n");
		json.append("  },\n");

		json.append("  \"files\": [\n");
		File[] files = motorDirectory.listFiles(File::isFile);
		if (files == null) {
			throw new IOException("cannot list " + motorDirectory);
		}
		Arrays.sort(files, (a, b) -> a.getName().compareTo(b.getName()));
		for (int f = 0; f < files.length; f++) {
			json.append("    ").append(file(files[f]));
			json.append(f + 1 < files.length ? ",\n" : "\n");
		}
		json.append("  ]\n");
		json.append("}\n");

		Files.write(output.toPath(), json.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("Wrote " + motors.size() + " database curves, " + skipped.size()
				+ " skipped, " + files.length + " files to " + output);
	}

	/** One motor file as GeneralMotorLoader reads it: its motors, or the exception. */
	private static String file(File file) {
		StringBuilder sb = new StringBuilder();
		sb.append("{\"file\": ").append(string(file.getName()));
		List<String> curves = new ArrayList<>();
		String error = null;
		try (InputStream in = new FileInputStream(file)) {
			List<ThrustCurveMotor.Builder> builders = new GeneralMotorLoader().load(in, file.getName());
			for (ThrustCurveMotor.Builder builder : builders) {
				try {
					curves.add(motor(builder.build()));
				} catch (IllegalArgumentException e) {
					curves.add("{\"buildError\": " + string(e.getMessage()) + "}");
				}
			}
		} catch (Exception e) {
			error = e.getClass().getSimpleName() + ": " + e.getMessage();
		}
		sb.append(", \"error\": ").append(error == null ? "null" : string(error));
		sb.append(", \"curves\": [");
		for (int i = 0; i < curves.size(); i++) {
			sb.append("\n      ").append(curves.get(i));
			if (i + 1 < curves.size()) {
				sb.append(",");
			}
		}
		sb.append(curves.isEmpty() ? "]}" : "\n    ]}");
		return sb.toString();
	}

	/** The values of one motor that QtRocket compares. */
	private static String motor(ThrustCurveMotor m) {
		StringBuilder sb = new StringBuilder();
		sb.append("{");
		field(sb, "manufacturer", string(m.getManufacturer().getDisplayName()));
		field(sb, "manufacturerSimpleName", string(m.getManufacturer().getSimpleName()));
		field(sb, "code", string(m.getCode()));
		field(sb, "designation", string(m.getDesignation()));
		field(sb, "commonName", string(m.getCommonName()));
		field(sb, "description", string(m.getDescription()));
		field(sb, "type", string(m.getMotorType().name()));
		field(sb, "caseInfo", string(m.getCaseInfo()));
		field(sb, "propellantInfo", string(m.getPropellantInfo()));
		field(sb, "tcMotorId", string(m.getTcMotorId()));
		field(sb, "infoUrl", string(m.getInfoUrl()));
		field(sb, "dataFiles", m.getDataFiles() == null ? "null" : m.getDataFiles().toString());
		field(sb, "updatedOn", string(m.getUpdatedOn()));
		field(sb, "dataSource", string(m.getDataSource()));
		field(sb, "sparky", Boolean.toString(m.isSparky()));
		field(sb, "available", Boolean.toString(m.isAvailable()));
		field(sb, "diameter", number(m.getDiameter()));
		field(sb, "length", number(m.getLength()));
		field(sb, "initialMass", number(m.getInitialMass()));
		StringBuilder delays = new StringBuilder("[");
		double[] d = m.getStandardDelays();
		for (int i = 0; i < d.length; i++) {
			delays.append(i > 0 ? ", " : "").append(number(d[i]));
		}
		field(sb, "delays", delays.append("]").toString());
		field(sb, "totalImpulse", number(m.getTotalImpulseEstimate()));
		field(sb, "averageThrust", number(m.getAverageThrustEstimate()));
		field(sb, "maxThrust", number(m.getMaxThrustEstimate()));
		field(sb, "burnTimeEstimate", number(m.getBurnTimeEstimate()));
		field(sb, "burnTime", number(m.getBurnTime()));
		field(sb, "launchMass", number(m.getLaunchMass()));
		field(sb, "burnoutMass", number(m.getBurnoutMass()));
		field(sb, "launchCG", number(m.getLaunchCGx()));
		field(sb, "burnoutCG", number(m.getBurnoutCGx()));
		field(sb, "points", Integer.toString(m.getTimePoints().length));
		field(sb, "digest", string(m.getDigest()));
		sb.setLength(sb.length() - 2);
		return sb.append("}").toString();
	}

	/** A skipped curve from one of ThrustCurveMotorSQLiteDatabase's log events, or null. */
	private static String skipped(ILoggingEvent event) {
		String pattern = event.getMessage();
		Object[] a = event.getArgumentArray();
		String reason;
		Object curveId = null;
		Object designation;
		Object message = null;
		if (pattern.startsWith("Skipping motor with no thrust curves")) {
			reason = "NO_CURVES";
			designation = a[0];
		} else if (pattern.startsWith("Skipping curve {} with no thrust data")) {
			reason = "NO_DATA";
			curveId = a[0];
			designation = a[1];
		} else if (pattern.startsWith("Skipping curve {} with invalid thrust data")) {
			reason = "INVALID_DATA";
			curveId = a[0];
			designation = a[1];
		} else if (pattern.startsWith("Skipping invalid curve")) {
			reason = "INVALID_MOTOR";
			curveId = a[0];
			designation = a[1];
			message = a[2];
		} else {
			return null;
		}
		return "{\"reason\": " + string(reason)
				+ ", \"curveId\": " + (curveId == null ? "null" : curveId.toString())
				+ ", \"designation\": " + (designation == null ? "null" : string(designation.toString()))
				+ ", \"message\": " + (message == null ? "null" : string(message.toString())) + "}";
	}

	private static void field(StringBuilder sb, String name, String value) {
		sb.append(string(name)).append(": ").append(value).append(", ");
	}

	/** A double as JSON: Double.toString, which reads back exactly; non-finite values as strings. */
	private static String number(double value) {
		if (Double.isNaN(value) || Double.isInfinite(value)) {
			return string(Double.toString(value));
		}
		return Double.toString(value);
	}

	private static String string(String value) {
		if (value == null) {
			return "null";
		}
		StringBuilder sb = new StringBuilder("\"");
		for (int i = 0; i < value.length(); i++) {
			char c = value.charAt(i);
			switch (c) {
				case '"':
					sb.append("\\\"");
					break;
				case '\\':
					sb.append("\\\\");
					break;
				case '\n':
					sb.append("\\n");
					break;
				case '\r':
					sb.append("\\r");
					break;
				case '\t':
					sb.append("\\t");
					break;
				default:
					if (c < 0x20) {
						sb.append(String.format("\\u%04x", (int) c));
					} else {
						sb.append(c);
					}
			}
		}
		return sb.append('"').toString();
	}
}
