package info.qtrocket.goldens;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import info.openrocket.core.material.Material;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;

/**
 * The preferences of a fresh OpenRocket installation: every getter returns the default value that
 * {@link ApplicationPreferences} passes to it (values that were put are returned afterwards).
 * <p>
 * The dumper keeps the Guice bootstrap of OpenRocket's tests (whose {@code PreferencesForTesting}
 * answer 0, {@code false} and {@code null} to most queries), so it uses this class only to derive the
 * options of the default simulations it creates for the TestRockets designs, the options a new
 * simulation gets in a freshly installed OpenRocket.
 */
final class ApplicationDefaultsPreferences extends ApplicationPreferences {

	private final Map<String, Object> values = new HashMap<>();

	@Override
	public boolean getBoolean(String key, boolean defaultValue) {
		return (Boolean) values.getOrDefault(key, defaultValue);
	}

	@Override
	public void putBoolean(String key, boolean value) {
		values.put(key, value);
	}

	@Override
	public int getInt(String key, int defaultValue) {
		return (Integer) values.getOrDefault(key, defaultValue);
	}

	@Override
	public void putInt(String key, int value) {
		values.put(key, value);
	}

	@Override
	public double getDouble(String key, double defaultValue) {
		return (Double) values.getOrDefault(key, defaultValue);
	}

	@Override
	public void putDouble(String key, double value) {
		values.put(key, value);
	}

	@Override
	public String getString(String key, String defaultValue) {
		Object value = values.get(key);
		return value == null ? defaultValue : (String) value;
	}

	@Override
	public void putString(String key, String value) {
		if (value == null) {
			values.remove(key);
		} else {
			values.put(key, value);
		}
	}

	@Override
	public String getString(String directory, String key, String defaultValue) {
		return getString(directory + "/" + key, defaultValue);
	}

	@Override
	public void putString(String directory, String key, String value) {
		putString(directory + "/" + key, value);
	}

	@Override
	public java.util.prefs.Preferences getNode(String nodeName) {
		throw new UnsupportedOperationException("Preference nodes are not used by the golden dumper");
	}

	@Override
	public java.util.prefs.Preferences getPreferences() {
		throw new UnsupportedOperationException("Preference nodes are not used by the golden dumper");
	}

	@Override
	public void addUserMaterial(Material m) {
		throw new UnsupportedOperationException("User materials are not used by the golden dumper");
	}

	@Override
	public Set<Material> getUserMaterials() {
		return Collections.emptySet();
	}

	@Override
	public void removeUserMaterial(Material m) {
		throw new UnsupportedOperationException("User materials are not used by the golden dumper");
	}

	@Override
	public void setComponentFavorite(ComponentPreset preset, ComponentPreset.Type type, boolean favorite) {
		throw new UnsupportedOperationException("Component favorites are not used by the golden dumper");
	}

	@Override
	public Set<String> getComponentFavorites(ComponentPreset.Type type) {
		return Collections.emptySet();
	}
}
