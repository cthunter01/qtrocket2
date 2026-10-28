package info.qtrocket.guireference;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Reads and writes the private fields and calls the private methods of OpenRocket's Swing classes, which
 * the tool drives as their own buttons and listeners do (the panels' operations are private, their
 * widgets private fields). A failure is a programming error of the tool: an unchecked exception.
 */
final class Reflection {

	private Reflection() {
	}

	/** The field @p name declared by @p type of @p object. */
	static Object get(Class<?> type, Object object, String name) {
		try {
			Field field = type.getDeclaredField(name);
			field.setAccessible(true);
			return field.get(object);
		} catch (ReflectiveOperationException e) {
			throw new IllegalStateException("cannot read " + type.getSimpleName() + "." + name, e);
		}
	}

	/** Sets the field @p name declared by @p type of @p object. */
	static void set(Class<?> type, Object object, String name, Object value) {
		try {
			Field field = type.getDeclaredField(name);
			field.setAccessible(true);
			field.set(object, value);
		} catch (ReflectiveOperationException e) {
			throw new IllegalStateException("cannot set " + type.getSimpleName() + "." + name, e);
		}
	}

	/**
	 * Calls the method @p name with the parameter types @p types on @p object, looked up from its class up;
	 * an exception the method throws is rethrown (unchecked ones as they are).
	 */
	static Object invoke(Object object, String name, Class<?>[] types, Object... arguments) {
		Class<?> type = object.getClass();
		while (type != null) {
			Method method;
			try {
				method = type.getDeclaredMethod(name, types);
			} catch (NoSuchMethodException e) {
				type = type.getSuperclass();
				continue;
			}
			method.setAccessible(true);
			try {
				return method.invoke(object, arguments);
			} catch (InvocationTargetException e) {
				if (e.getCause() instanceof RuntimeException runtime) {
					throw runtime;
				}
				if (e.getCause() instanceof Error error) {
					throw error;
				}
				throw new IllegalStateException(name + " failed", e.getCause());
			} catch (IllegalAccessException e) {
				throw new IllegalStateException("cannot call " + name, e);
			}
		}
		throw new IllegalStateException("no method " + name + " in " + object.getClass().getName());
	}

	/** Calls the method @p name without parameters on @p object. */
	static Object invoke(Object object, String name) {
		return invoke(object, name, new Class<?>[0]);
	}
}
