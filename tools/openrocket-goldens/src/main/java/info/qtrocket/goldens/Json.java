package info.qtrocket.goldens;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * A minimal, deterministic JSON writer for the golden files.
 * <ul>
 * <li>Objects keep their insertion order ({@link LinkedHashMap}).</li>
 * <li>Finite doubles are written with {@link Double#toString(double)} (shortest representation that
 * round-trips exactly, e.g. {@code 0.1}, {@code 1.0E-5}); non-finite doubles, which JSON cannot
 * represent, are written as the strings {@code "NaN"}, {@code "Infinity"} and {@code "-Infinity"}.</li>
 * <li>Integers ({@link Integer}, {@link Long}) are written as plain integers.</li>
 * <li>Arrays whose elements are all scalars, and objects whose values are all scalars or such
 * arrays, are written on one line; everything else is indented by one space per level. Line ends
 * are always {@code \n}; the file ends with a newline.</li>
 * </ul>
 */
final class Json {

	private Json() {
	}

	/** A new, insertion-ordered JSON object. */
	static Map<String, Object> object() {
		return new LinkedHashMap<>();
	}

	/** A new JSON array. */
	static List<Object> array() {
		return new ArrayList<>();
	}

	/** A JSON array holding the given doubles. */
	static List<Object> numbers(double... values) {
		List<Object> list = new ArrayList<>(values.length);
		for (double v : values) {
			list.add(v);
		}
		return list;
	}

	/** Writes {@code value} to {@code file} as UTF-8. */
	static void write(Path file, Object value) throws IOException {
		Files.createDirectories(file.getParent());
		Files.write(file, bytes(value));
	}

	/** The bytes {@link #write} writes for {@code value}. */
	static byte[] bytes(Object value) {
		StringBuilder sb = new StringBuilder(1 << 16);
		append(sb, value, 0);
		sb.append('\n');
		return sb.toString().getBytes(StandardCharsets.UTF_8);
	}

	/** The JSON text of a double (see the class comment). */
	static String number(double d) {
		if (Double.isNaN(d)) {
			return "\"NaN\"";
		}
		if (Double.isInfinite(d)) {
			return d > 0 ? "\"Infinity\"" : "\"-Infinity\"";
		}
		return Double.toString(d);
	}

	private static boolean isScalar(Object value) {
		return !(value instanceof Map) && !(value instanceof List);
	}

	/** A scalar or an array of scalars: such values keep an object on one line. */
	private static boolean isFlat(Object value) {
		return isScalar(value) || (value instanceof List<?> list && list.stream().allMatch(Json::isScalar));
	}

	private static void newline(StringBuilder sb, int indent) {
		sb.append('\n');
		sb.append(" ".repeat(indent));
	}

	@SuppressWarnings("unchecked")
	private static void append(StringBuilder sb, Object value, int indent) {
		if (value == null) {
			sb.append("null");
		} else if (value instanceof String s) {
			appendString(sb, s);
		} else if (value instanceof Double d) {
			sb.append(number(d));
		} else if (value instanceof Float f) {
			sb.append(number(f.doubleValue()));
		} else if (value instanceof Integer || value instanceof Long || value instanceof Short) {
			sb.append(value);
		} else if (value instanceof Boolean b) {
			sb.append(b ? "true" : "false");
		} else if (value instanceof Map<?, ?> map) {
			if (map.isEmpty()) {
				sb.append("{}");
				return;
			}
			boolean flat = map.values().stream().allMatch(Json::isFlat);
			sb.append('{');
			boolean first = true;
			for (Map.Entry<String, Object> e : ((Map<String, Object>) map).entrySet()) {
				if (!first) {
					sb.append(flat ? ", " : ",");
				}
				first = false;
				if (!flat) {
					newline(sb, indent + 1);
				}
				appendString(sb, e.getKey());
				sb.append(": ");
				append(sb, e.getValue(), indent + 1);
			}
			if (!flat) {
				newline(sb, indent);
			}
			sb.append('}');
		} else if (value instanceof List<?> list) {
			if (list.isEmpty()) {
				sb.append("[]");
				return;
			}
			boolean allScalars = list.stream().allMatch(Json::isScalar);
			sb.append('[');
			boolean first = true;
			for (Object element : list) {
				if (!first) {
					sb.append(allScalars ? ", " : ",");
				}
				first = false;
				if (!allScalars) {
					newline(sb, indent + 1);
				}
				append(sb, element, indent + 1);
			}
			if (!allScalars) {
				newline(sb, indent);
			}
			sb.append(']');
		} else {
			throw new IllegalArgumentException("Not a JSON value: " + value.getClass().getName());
		}
	}

	private static void appendString(StringBuilder sb, String s) {
		sb.append('"');
		for (int i = 0; i < s.length(); i++) {
			char c = s.charAt(i);
			switch (c) {
				case '"' -> sb.append("\\\"");
				case '\\' -> sb.append("\\\\");
				case '\n' -> sb.append("\\n");
				case '\r' -> sb.append("\\r");
				case '\t' -> sb.append("\\t");
				case '\b' -> sb.append("\\b");
				case '\f' -> sb.append("\\f");
				default -> {
					if (c < 0x20) {
						sb.append(String.format("\\u%04x", (int) c));
					} else {
						sb.append(c);
					}
				}
			}
		}
		sb.append('"');
	}
}
