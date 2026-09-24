package info.qtrocket.goldens;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;

/**
 * Stable, portable names for the components of a rocket.
 * <p>
 * A component's <em>path</em> is the list of child indices from the rocket: the rocket is {@code "/"},
 * its first stage {@code "/0"}, that stage's second child {@code "/0/1"}. Paths identify the same
 * component in OpenRocket and in the C++ port even where the UUIDs differ (the TestRockets designs are
 * rebuilt in C++ with fresh UUIDs). Components of a simulation's private rocket copy keep the UUIDs of
 * the original ({@code copyWithOriginalID}), so they map to the same paths.
 */
final class ComponentIndex {

	private final Map<UUID, String> pathById = new HashMap<>();
	private final List<RocketComponent> components = new ArrayList<>();

	ComponentIndex(Rocket rocket) {
		visit(rocket, "/");
	}

	private void visit(RocketComponent component, String path) {
		components.add(component);
		pathById.put(component.getID(), path);
		String prefix = "/".equals(path) ? "" : path;
		int index = 0;
		for (RocketComponent child : component.getChildren()) {
			visit(child, prefix + "/" + index);
			index++;
		}
	}

	/** All components in depth-first pre-order, the rocket first. */
	List<RocketComponent> components() {
		return Collections.unmodifiableList(components);
	}

	/** The path of {@code component} (or of the component with its UUID), or null. */
	String path(RocketComponent component) {
		return component == null ? null : pathById.get(component.getID());
	}

	/** The path of the component with this UUID, or null. */
	String path(UUID id) {
		return id == null ? null : pathById.get(id);
	}
}
