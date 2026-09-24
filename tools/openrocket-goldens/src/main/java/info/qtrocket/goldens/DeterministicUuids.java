package info.qtrocket.goldens;

import java.nio.charset.StandardCharsets;
import java.security.Provider;
import java.security.SecureRandomSpi;
import java.security.Security;
import java.util.Random;
import java.util.UUID;

/**
 * Makes {@link UUID#randomUUID()} deterministic for the whole JVM.
 * <p>
 * OpenRocket gives every new component, flight configuration, simulation and event a random UUID.
 * Those identities are hash keys ({@code RocketComponent.hashCode()} is {@code id.hashCode()}), so the
 * iteration order of OpenRocket's hash maps (e.g. the {@code InstanceMap} that aerodynamic and mass
 * sums walk) and therefore the last bits of summed values would change from run to run for the
 * TestRockets designs, and the UUIDs themselves would appear in the re-saved {@code .ork} files. Designs
 * loaded from files keep the UUIDs stored in the file and are not affected.
 * <p>
 * {@code UUID.randomUUID()} draws from a {@code new SecureRandom()}, which takes the first registered
 * SecureRandom service, so a provider inserted at position 1 before the first UUID is made replaces
 * it. {@link #reseed(String)} restarts the sequence; the dumper calls it before each input.
 */
final class DeterministicUuids {

	private static final String NAME = "QtRocketGoldensDeterministicRandom";
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
			super(NAME, "1.0", "Deterministic SecureRandom for reproducible golden data");
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
