package info.qtrocket.goldens;

import java.lang.reflect.Method;
import java.util.Arrays;

import info.openrocket.core.aerodynamics.AerodynamicForces;
import info.openrocket.core.aerodynamics.FlightConditions;
import info.openrocket.core.simulation.SimulationStatus;
import info.openrocket.core.simulation.exception.SimulationException;
import info.openrocket.core.simulation.listeners.AbstractSimulationListener;

/**
 * Removes OpenRocket's pitch/yaw moment jitter from a simulation.
 * <p>
 * {@code AbstractRKSimulationStepper.calculateForces} adds
 * {@code PITCH_YAW_RANDOM * 2 * (random.nextDouble() - 0.5)} (up to +/-0.0005) to Cm and Cyaw after
 * every aerodynamic calculation of the RK4/RK6 flight stepper, from a {@code java.util.Random} seeded
 * with {@code randomSeed ^ 0x23E3A01F}. The C++ port cannot reproduce Java's generator, so the goldens
 * are computed without the jitter: when the post-aerodynamic-calculation hook is fired from that
 * method, {@link #forcesListener()} answers it with the calculator's result for the same configuration
 * and flight conditions (captured by {@link #conditionsListener()}), which is exactly what the stepper
 * computed before it added the random terms. The hook fired by the landing and tumble steppers (which
 * build their forces from a drag coefficient, without jitter) is left alone.
 * <p>
 * Placement: the forces listener must be the first simulation listener (so that later listeners,
 * e.g. of simulation extensions, see and modify the jitter-free forces) and the conditions listener
 * the last one (so that it captures the flight conditions after every other listener has had its
 * say). Both are system listeners: that keeps them in the nested optimum-coast simulation
 * ({@code BasicEventSimulationEngine.computeCoastTime} drops the other listeners) and keeps
 * OpenRocket from flagging the simulation with {@code Warning.LISTENERS_AFFECTED}. The listener
 * clones made for the nested simulation share the capture holder, which is safe because the nested
 * simulation runs to completion inside one step of the outer one and every aerodynamic calculation
 * is preceded by a flight-conditions calculation.
 * <p>
 * One theoretical gap: OpenRocket keeps the original forces when a listener's result equals them
 * within {@code MathUtil.EPSILON} (relative 1e-8), so a step whose random terms were both that small
 * would keep them. That needs two uniform draws within ~1e-8 of 0.5 in the same step.
 */
final class JitterRemoval {

	/** The class and method that add the jitter (checked to exist when this class is loaded). */
	private static final String JITTER_CLASS = "info.openrocket.core.simulation.AbstractRKSimulationStepper";
	private static final String JITTER_METHOD = "calculateForces";
	private static final StackWalker WALKER = StackWalker.getInstance();

	static {
		try {
			Method[] methods = Class.forName(JITTER_CLASS).getDeclaredMethods();
			if (Arrays.stream(methods).noneMatch(m -> m.getName().equals(JITTER_METHOD))) {
				throw new IllegalStateException(JITTER_CLASS + "." + JITTER_METHOD + " not found");
			}
		} catch (ClassNotFoundException e) {
			throw new IllegalStateException(e);
		}
	}

	private final Holder holder = new Holder();

	/** State shared by the two listeners (and by their clones in the nested coast simulation). */
	private static final class Holder {
		private FlightConditions conditions;
		private long replacements;
	}

	/** The listener that replaces the jittered forces; install it first. */
	AbstractSimulationListener forcesListener() {
		return new ForcesListener(holder);
	}

	/** The listener that captures the flight conditions; install it last. */
	AbstractSimulationListener conditionsListener() {
		return new ConditionsListener(holder);
	}

	/** How many jittered force results were replaced (RK stepper aerodynamic calculations). */
	long replacements() {
		return holder.replacements;
	}

	private static boolean calledFromJitteringMethod() {
		return WALKER.walk(frames -> frames.anyMatch(
				f -> JITTER_METHOD.equals(f.getMethodName()) && JITTER_CLASS.equals(f.getClassName())));
	}

	private static final class ConditionsListener extends AbstractSimulationListener {
		private final Holder holder;

		ConditionsListener(Holder holder) {
			this.holder = holder;
		}

		@Override
		public boolean isSystemListener() {
			return true;
		}

		@Override
		public FlightConditions postFlightConditions(SimulationStatus status, FlightConditions flightConditions) {
			// The argument is a clone of the stepper's conditions after all earlier listeners.
			holder.conditions = flightConditions.clone();
			return null;
		}
	}

	private static final class ForcesListener extends AbstractSimulationListener {
		private final Holder holder;

		ForcesListener(Holder holder) {
			this.holder = holder;
		}

		@Override
		public boolean isSystemListener() {
			return true;
		}

		@Override
		public AerodynamicForces postAerodynamicCalculation(SimulationStatus status, AerodynamicForces forces)
				throws SimulationException {
			if (!calledFromJitteringMethod()) {
				return null;
			}
			FlightConditions conditions = holder.conditions;
			if (conditions == null) {
				throw new SimulationException("Golden harness: no flight conditions captured before the "
						+ "aerodynamic calculation");
			}
			holder.conditions = null;
			holder.replacements++;
			return status.getSimulationConditions().getAerodynamicCalculator()
					.getAerodynamicForces(status.getConfiguration(), conditions, null);
		}
	}
}
