package info.qtrocket.goldens;

import java.util.ArrayList;
import java.util.List;

import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;

/**
 * Brings a design's automatic dimensions into their settled state before anything is dumped.
 * <p>
 * OpenRocket computes automatic dimensions lazily and stores the result in the component: a getter
 * such as {@code BodyTube.getOuterRadius()} (auto radius) or {@code MassObject.getRadius()} (auto
 * radius, which also rescales the packed length) refreshes the stored field, and other getters read
 * the stored field without refreshing it ({@code MassObject.getLength()} divides the volume by the
 * stored radius; {@code BodyTube}'s auto radius depends on which neighbour it last took its radius
 * from). Right after a design is loaded (or built) the stored values can therefore be stale, and the
 * first calculations mix stale and refreshed values: the first {@code MassCalculator} result for
 * "Dual parachute deployment.ork" has its structure CG at x = 0.885775814 m (the value
 * {@code ExampleFilesTest} pins), later ones at 0.8784464812860964 m. Which state a dump sees would
 * then depend on which getters ran before it.
 * <p>
 * {@link #settle} calls the refreshing getters of every component, in tree order, until a whole pass
 * leaves every value (and every body tube's choice of neighbour) unchanged. The goldens describe
 * that settled state, which does not depend on the order of later calls. The re-save
 * ({@code resave/rocket.ork}) is written before this step, so it is OpenRocket's save of the design
 * exactly as loaded (and can hold stale automatic values).
 */
final class AutomaticDimensions {

	/** Passes after which the dimensions must have settled. */
	private static final int MAX_PASSES = 20;

	private AutomaticDimensions() {
	}

	/**
	 * Settles the automatic dimensions of {@code rocket} (see the class comment).
	 *
	 * @return how many passes returned values that differed from those of the pass before
	 * @throws IllegalStateException if they do not settle within {@link #MAX_PASSES} passes
	 */
	static int settle(Rocket rocket, ComponentIndex index) {
		List<Object> previous = snapshot(index);
		for (int pass = 0; pass < MAX_PASSES; pass++) {
			List<Object> current = snapshot(index);
			if (current.equals(previous)) {
				return pass;
			}
			previous = current;
		}
		throw new IllegalStateException(
				"The automatic dimensions of " + rocket.getName() + " did not settle in " + MAX_PASSES + " passes");
	}

	/** One pass: calls the refreshing getters in tree order and returns what they returned. */
	private static List<Object> snapshot(ComponentIndex index) {
		List<Object> values = new ArrayList<>();
		for (RocketComponent c : index.components()) {
			if (c instanceof BodyTube tube) {
				values.add(tube.getOuterRadius());
				values.add(tube.getInnerRadius());
			}
			if (c instanceof SymmetricComponent s) {
				values.add(s.getForeRadius());
				values.add(s.getAftRadius());
				values.add(s.usesPreviousCompAutomatic());
				values.add(s.usesNextCompAutomatic());
			}
			if (c instanceof RingComponent ring) {
				values.add(ring.getOuterRadius());
				values.add(ring.getInnerRadius());
			}
			if (c instanceof MassObject m) {
				values.add(m.getRadius());
				values.add(m.getLength());
			}
		}
		return values;
	}
}
