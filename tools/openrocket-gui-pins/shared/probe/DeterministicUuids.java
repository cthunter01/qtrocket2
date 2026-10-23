package probe;

import java.nio.charset.StandardCharsets;
import java.security.Provider;
import java.security.SecureRandomSpi;
import java.security.Security;
import java.util.Random;
import java.util.UUID;

/**
 * Makes {@link UUID#randomUUID()} deterministic for the whole JVM (a copy of
 * tools/openrocket-goldens' DeterministicUuids, in the probes' package).
 * <p>
 * OpenRocket gives every new component a random UUID, and {@code RocketComponent.hashCode()} is
 * {@code id.hashCode()}: the iteration order of OpenRocket's hash maps keyed by components (the
 * motors of a FlightConfiguration, the InstanceMap the CG and CP sums walk, the shapes RocketFigure
 * paints at one display order) would change from run to run for a design a probe builds (the
 * TestRockets makers, a new design). Designs loaded from a file keep the UUIDs stored in it.
 * <p>
 * {@code UUID.randomUUID()} draws from a {@code new SecureRandom()}, made at its first call, which
 * takes the first registered SecureRandom service, so a provider inserted at position 1 before the
 * first UUID is made replaces it. {@link #reseed(String)} restarts the sequence; a probe calls it
 * before it builds each scenario's design, so that a scenario's UUIDs do not depend on the scenarios
 * before it.
 */
public final class DeterministicUuids {

	private static final String NAME = "QtRocketGuiPinsDeterministicRandom";
	private static final Object LOCK = new Object();
	private static Random random = new Random(0);

	private DeterministicUuids() {
	}

	/** Installs the deterministic generator; must run before anything calls UUID.randomUUID(). */
	public static void install() {
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
	public static void reseed(String key) {
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
			super(NAME, "1.0", "Deterministic SecureRandom for reproducible GUI pins");
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
