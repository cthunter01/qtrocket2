package info.qtrocket.guireference;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;

/**
 * Component paths, the names a user reads in the component tree from the stage down to a component
 * (see {@link Scenario#path}): the rows a step clicks, and how the tool writes the selection and the
 * component of a dialog.
 */
final class ComponentPaths {

	private ComponentPaths() {
	}

	/** The component @p path names, below @p rocket. */
	static RocketComponent resolve(Rocket rocket, List<Scenario.PathElement> path) {
		RocketComponent component = rocket;
		for (Scenario.PathElement element : path) {
			RocketComponent found = null;
			int seen = 0;
			for (RocketComponent child : component.getChildren()) {
				if (child.getName().equals(element.name()) && ++seen == element.occurrence()) {
					found = child;
					break;
				}
			}
			if (found == null) {
				throw new IllegalArgumentException("no component '" + element.name() + "' (occurrence "
						+ element.occurrence() + ") under '" + component.getName() + "'");
			}
			component = found;
		}
		return component;
	}

	/** The path of @p component in the form {@link #resolve} reads. */
	static List<Scenario.PathElement> pathOf(RocketComponent component) {
		List<Scenario.PathElement> path = new ArrayList<>();
		for (RocketComponent c = component; c.getParent() != null; c = c.getParent()) {
			int occurrence = 0;
			for (RocketComponent sibling : c.getParent().getChildren()) {
				if (sibling.getName().equals(c.getName())) {
					occurrence++;
				}
				if (sibling == c) {
					break;
				}
			}
			path.add(new Scenario.PathElement(c.getName(), occurrence));
		}
		Collections.reverse(path);
		return path;
	}

	/** {@link #pathOf} as JSON: a name, or {"name": ..., "occurrence": n} for a later child of that name. */
	static JsonArray json(RocketComponent component) {
		JsonArray array = new JsonArray();
		for (Scenario.PathElement element : pathOf(component)) {
			if (element.occurrence() == 1) {
				array.add(element.name());
			} else {
				JsonObject object = new JsonObject();
				object.addProperty("name", element.name());
				object.addProperty("occurrence", element.occurrence());
				array.add(object);
			}
		}
		return array;
	}
}
