package info.qtrocket.goldens;

import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Random;

import info.openrocket.core.simulation.SimulationStatus;
import info.openrocket.core.simulation.listeners.AbstractSimulationListener;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.Quaternion;

/**
 * A perturbation of a simulation run in the last bit, to measure how reproducible OpenRocket's own
 * results are for a design that is loaded from a file.
 * <p>
 * {@code UUID_SALT} measures that for the test rockets: other component ids give OpenRocket's hash
 * maps another order and its sums other last bits. A design loaded from a file keeps the ids stored
 * in it, so the salt changes nothing there. This class changes a run by as little as another
 * summation order would: after every step a listener moves components of the rocket's state to a
 * neighbouring representable number. A dump made with it is never the committed data
 * ({@code generate.sh} refuses it for {@code tests/data/goldens}, and {@code manifest.json} records
 * the pattern as {@code settings.lastBitPerturbation}).
 * <p>
 * A pattern is {@code <quantity>-<direction>} or {@code <quantity>-random-<seed>}:
 * <ul>
 * <li>quantity: {@code velocity} (the rocket's velocity), {@code rotation} (its rotation velocity),
 * {@code position} (its position), {@code orientation} (the four components of its orientation
 * quaternion), {@code both} (velocity and rotation velocity: what {@code LastBitListener} of
 * QtRocket's golden tests moves) or {@code all} (the three vectors: velocity, rotation velocity and
 * position; the orientation is not among them, so that a pattern keeps the meaning it had in the
 * measurements made before the orientation could be moved);</li>
 * <li>direction: {@code away} (every component to the next number away from zero), {@code toward}
 * (to the next number toward zero) or {@code random} (every component, after every step, one number
 * up, one down or not at all, drawn from a {@code java.util.Random} seeded from the pattern's text;
 * the seed is any text without a meaning of its own, {@code all-random-a} and {@code all-random-b}
 * are two patterns).</li>
 * </ul>
 * A component that is zero, infinite or NaN is never moved.
 * <p>
 * The listener is a system listener: it stays in the nested optimum-coast simulation and adds no
 * "listeners modified the flight simulation" warning. It is placed right after the jitter-removal
 * forces listener, where QtRocket's golden tests place their perturbation.
 */
final class LastBitPerturbation {

	/** What of the rocket's state is moved. */
	private enum Quantity {
		VELOCITY(true, false, false, false), ROTATION(false, true, false, false),
		POSITION(false, false, true, false), ORIENTATION(false, false, false, true),
		BOTH(true, true, false, false), ALL(true, true, true, false);

		private final boolean velocity;
		private final boolean rotation;
		private final boolean position;
		private final boolean orientation;

		Quantity(boolean velocity, boolean rotation, boolean position, boolean orientation) {
			this.velocity = velocity;
			this.rotation = rotation;
			this.position = position;
			this.orientation = orientation;
		}
	}

	/** Where a component is moved to. */
	private enum Direction {
		AWAY, TOWARD, RANDOM
	}

	/** The grammar, for error messages. */
	static final String GRAMMAR = "<velocity|rotation|position|orientation|both|all>-<away|toward> or "
			+ "<velocity|rotation|position|orientation|both|all>-random-<seed text>";

	private final String pattern;
	private final Quantity quantity;
	private final Direction direction;

	private LastBitPerturbation(String pattern, Quantity quantity, Direction direction) {
		this.pattern = pattern;
		this.quantity = quantity;
		this.direction = direction;
	}

	/**
	 * The perturbation {@code pattern} names, or null for an empty pattern (no perturbation: the
	 * committed goldens).
	 *
	 * @throws IllegalArgumentException when the pattern is not of the grammar
	 */
	static LastBitPerturbation parse(String pattern) {
		if (pattern.isEmpty()) {
			return null;
		}
		String[] parts = pattern.split("-", 3);
		Quantity quantity = null;
		Direction direction = null;
		if (parts.length >= 2) {
			for (Quantity q : Quantity.values()) {
				if (q.name().toLowerCase(Locale.ROOT).equals(parts[0])) {
					quantity = q;
				}
			}
			for (Direction d : Direction.values()) {
				if (d.name().toLowerCase(Locale.ROOT).equals(parts[1])) {
					direction = d;
				}
			}
		}
		// A random pattern has a seed text, the others have none.
		boolean seedAsExpected = direction != null
				&& (direction == Direction.RANDOM) == (parts.length == 3 && !parts[2].isEmpty());
		if (quantity == null || !seedAsExpected) {
			throw new IllegalArgumentException(
					"Not a last-bit perturbation pattern: " + pattern + "; a pattern is " + GRAMMAR);
		}
		return new LastBitPerturbation(pattern, quantity, direction);
	}

	/** The pattern's text, as given. */
	String pattern() {
		return pattern;
	}

	/** The listener of one simulation run (a random pattern starts its sequence anew in each). */
	AbstractSimulationListener listener() {
		return new Listener(quantity, direction, new Random(seed(pattern)));
	}

	/** The seed of a random pattern: a hash of its text (as DeterministicUuids seeds from a name). */
	private static long seed(String text) {
		long seed = 1125899906842597L;
		for (byte b : text.getBytes(StandardCharsets.UTF_8)) {
			seed = 31 * seed + b;
		}
		return seed;
	}

	/** {@code value} moved to a neighbouring number; zero, the infinities and NaN are left alone. */
	private static double moved(double value, Direction direction, Random random) {
		if (value == 0 || Double.isNaN(value) || Double.isInfinite(value)) {
			return value;
		}
		return switch (direction) {
			case AWAY -> Math.nextAfter(value, value > 0 ? Double.POSITIVE_INFINITY : Double.NEGATIVE_INFINITY);
			case TOWARD -> Math.nextAfter(value, 0.0);
			case RANDOM -> switch (random.nextInt(3)) {
				case 0 -> Math.nextUp(value);
				case 1 -> Math.nextDown(value);
				default -> value;
			};
		};
	}

	private static final class Listener extends AbstractSimulationListener {
		private final Quantity quantity;
		private final Direction direction;
		/**
		 * The generator of a random pattern. The clone made for the nested optimum-coast simulation
		 * shares it, which is deterministic: that simulation runs to completion inside one step of
		 * the outer one.
		 */
		private final Random random;

		Listener(Quantity quantity, Direction direction, Random random) {
			this.quantity = quantity;
			this.direction = direction;
			this.random = random;
		}

		@Override
		public boolean isSystemListener() {
			return true;
		}

		@Override
		public void postStep(SimulationStatus status) {
			if (quantity.velocity) {
				status.setRocketVelocity(moved(status.getRocketVelocity()));
			}
			if (quantity.rotation) {
				status.setRocketRotationVelocity(moved(status.getRocketRotationVelocity()));
			}
			if (quantity.position) {
				status.setRocketPosition(moved(status.getRocketPosition()));
			}
			if (quantity.orientation) {
				status.setRocketOrientationQuaternion(moved(status.getRocketOrientationQuaternion()));
			}
		}

		private CoordinateIF moved(CoordinateIF c) {
			double x = LastBitPerturbation.moved(c.getX(), direction, random);
			double y = LastBitPerturbation.moved(c.getY(), direction, random);
			double z = LastBitPerturbation.moved(c.getZ(), direction, random);
			return new Coordinate(x, y, z, c.getWeight());
		}

		/** The quaternion with each of its components moved, in the order w, x, y, z. */
		private Quaternion moved(Quaternion q) {
			double w = LastBitPerturbation.moved(q.getW(), direction, random);
			double x = LastBitPerturbation.moved(q.getX(), direction, random);
			double y = LastBitPerturbation.moved(q.getY(), direction, random);
			double z = LastBitPerturbation.moved(q.getZ(), direction, random);
			return new Quaternion(w, x, y, z);
		}
	}
}
