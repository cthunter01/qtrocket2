package info.qtrocket.guireference;

import java.nio.charset.StandardCharsets;
import java.security.Provider;
import java.security.SecureRandomSpi;
import java.security.Security;
import java.util.Random;
import java.util.UUID;

/**
 * Makes {@link UUID#randomUUID()} deterministic for the whole JVM: the class of the same name of
 * tools/openrocket-goldens, copied (the two tools are separate Gradle projects).
 * <p>
 * OpenRocket gives every new component and flight configuration a random UUID. The ids are written
 * into the saved {@code .ork} file, and they are hash keys ({@code RocketComponent.hashCode()} is
 * {@code id.hashCode()}), so the iteration order of OpenRocket's hash maps follows them too. With this
 * generator two runs of the scenario make the same ids in the same order and write the same file.
 * <p>
 * {@code UUID.randomUUID()} draws from a {@code new SecureRandom()}, which takes the first registered
 * SecureRandom service, so a provider inserted at position 1 before the first UUID is made replaces
 * it. {@link #reseed(String)} restarts the sequence; the tool calls it before each run of the
 * scenario.
 */
final class DeterministicUuids {

	private static final String NAME = "QtRocketGuiReferenceDeterministicRandom";
	private static final Object LOCK = new Object();
	private static Random random = new Random(0);

	private DeterministicUuids() {
	}

	/** Installs the deterministic generator; must run before anything calls UUID.randomUUID(). */
	static void install() {
		Provider provider = new DeterministicProvider();
		if (Security.insertProviderAt(provider, 1) != 1) {
			throw new IllegalStateException("Could not install the deterministic SecureRandom provider");
		}
		reseed("self-test");
		UUID first = UUID.randomUUID();
		reseed("self-test");
		UUID second = UUID.randomUUID();
		if (!first.equals(second)) {
			throw new IllegalStateException("UUID.randomUUID() does not use the deterministic generator "
					+ "(was a UUID created before DeterministicUuids.install()?)");
		}
	}

	/** Restarts the UUID sequence from a seed derived from {@code key}. */
	static void reseed(String key) {
		long seed = 1125899906842597L;
		for (byte b : key.getBytes(StandardCharsets.UTF_8)) {
			seed = 31 * seed + b;
		}
		synchronized (LOCK) {
			random = new Random(seed);
		}
	}

	private static final class DeterministicProvider extends Provider {
		private static final long serialVersionUID = 1L;

		DeterministicProvider() {
			super(NAME, "1.0", "Deterministic SecureRandom for a reproducible GUI reference");
			putService(new Service(this, "SecureRandom", NAME, DeterministicSpi.class.getName(), null, null) {
				@Override
				public Object newInstance(Object constructorParameter) {
					return new DeterministicSpi();
				}
			});
		}
	}

	/** A SecureRandom engine backed by the shared, seeded {@link Random}. */
	static final class DeterministicSpi extends SecureRandomSpi {
		private static final long serialVersionUID = 1L;

		@Override
		protected void engineSetSeed(byte[] seed) {
			// The sequence is controlled by reseed() only.
		}

		@Override
		protected void engineNextBytes(byte[] bytes) {
			synchronized (LOCK) {
				random.nextBytes(bytes);
			}
		}

		@Override
		protected byte[] engineGenerateSeed(int numBytes) {
			byte[] bytes = new byte[numBytes];
			engineNextBytes(bytes);
			return bytes;
		}
	}
}
