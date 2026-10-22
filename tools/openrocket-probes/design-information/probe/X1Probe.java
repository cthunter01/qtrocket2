package probe;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.ComponentChangeEvent;
import info.openrocket.core.rocketcomponent.DesignType;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration.SeparationEvent;
import info.openrocket.core.startup.Application;

/**
 * Milestone 2 run 4, part X1: the undo and redo of the rocket's design information (Rocket.loadFrom()),
 * and the stage separation's copy on first access (AxialStage.getSeparationConfiguration()), on
 * OpenRocket's core with ServicesForTesting. tests/core/document/OpenRocketDocumentTests.cpp,
 * tests/core/rocket/RocketTests.cpp and tests/core/rocket/AxialStageTests.cpp pin what it prints by hand
 * (expected.txt; see ../README.md).
 */
public class X1Probe {
	static List<String> events = new ArrayList<>();

	static void listen(Rocket rocket) {
		rocket.addComponentChangeListener(e -> {
			ComponentChangeEvent ev = (ComponentChangeEvent) e;
			events.add(ev.getSource().getClass().getSimpleName() + " type=" + ev.getType());
		});
	}

	static String takeEvents() {
		String s = String.join(", ", events);
		events.clear();
		return "[" + s + "]";
	}

	static String info(Rocket r) {
		return "designer='" + r.getDesigner() + "' revision='" + r.getRevision() + "' kitName='" + r.getKitName()
				+ "' designType=" + r.getDesignType() + " name='" + r.getName() + "'";
	}

	static void field(String title, Consumer<Rocket> edit, Function<Rocket, String> get) {
		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		Rocket rocket = doc.getRocket();
		listen(rocket);
		doc.addUndoPosition("Edit " + title);
		edit.accept(rocket);
		System.out.println(title + " edit: value=" + get.apply(rocket) + " events=" + takeEvents() + " undoAvail="
				+ doc.isUndoAvailable() + " undoDesc=" + doc.getUndoDescription() + " saved=" + doc.isSaved());
		doc.undo();
		System.out.println(title + " undo: value=" + get.apply(rocket) + " events=" + takeEvents() + " redoAvail="
				+ doc.isRedoAvailable() + " redoDesc=" + doc.getRedoDescription());
		doc.redo();
		System.out.println(title + " redo: value=" + get.apply(rocket) + " events=" + takeEvents());
	}

	static String ids(Rocket r) {
		return "mod=" + r.getModID() + " mass=" + r.getMassModID() + " aero=" + r.getAerodynamicModID() + " tree="
				+ r.getTreeModID() + " func=" + r.getFunctionalModID();
	}

	static String sep(StageSeparationConfiguration s) {
		return s.getSeparationEvent().name() + "/" + s.getSeparationDelay() + "/" + s.getSeparationAltitude();
	}

	public static void main(String[] args) throws Exception {
		Injector injector = Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule()));
		Application.setInjector(injector);

		System.out.println("== undo and redo of each field");
		field("designer", r -> r.setDesigner("Ann"), Rocket::getDesigner);
		field("revision", r -> r.setRevision("v2"), Rocket::getRevision);
		field("kitName", r -> r.setKitName("Alpha"), Rocket::getKitName);
		field("designType", r -> r.setDesignType(DesignType.KIT_BASH), r -> r.getDesignType().name());
		field("name", r -> r.setName("Renamed"), Rocket::getName);

		System.out.println("== all four with the name in one step, two steps of undo");
		{
			OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
			Rocket rocket = doc.getRocket();
			listen(rocket);
			doc.addUndoPosition("First");
			rocket.setDesigner("Ann");
			rocket.setRevision("v2");
			rocket.setKitName("Alpha");
			rocket.setDesignType(DesignType.COMMERCIAL_KIT);
			rocket.setName("Renamed");
			doc.addUndoPosition("Second");
			rocket.setDesigner("Bob");
			rocket.setDesignType(DesignType.UPSCALE_KIT);
			takeEvents();
			System.out.println("edited: " + info(rocket));
			doc.undo();
			System.out.println("undo 1: " + info(rocket) + " events=" + takeEvents());
			doc.undo();
			System.out.println("undo 2: " + info(rocket) + " events=" + takeEvents());
			doc.redo();
			System.out.println("redo 1: " + info(rocket) + " events=" + takeEvents());
			doc.redo();
			System.out.println("redo 2: " + info(rocket) + " events=" + takeEvents());
		}

		System.out.println("== loadFrom directly");
		{
			Rocket rocket = OpenRocketDocumentFactory.createNewRocket().getRocket();
			rocket.setDesigner("Snapshot");
			rocket.setDesignType(DesignType.MODIFIED_KIT);
			Rocket snapshot = rocket.copyWithOriginalID();
			System.out.println("snapshot: " + info(snapshot));
			rocket.setDesigner("After");
			rocket.setDesignType(DesignType.KIT_BASH);
			listen(rocket);
			rocket.loadFrom(snapshot);
			System.out.println("loaded: " + info(rocket) + " events=" + takeEvents());
		}

		System.out.println("== separation copy on first access");
		{
			OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
			Rocket rocket = doc.getRocket();
			rocket.getStage(0).addChild(new BodyTube());
			AxialStage booster = new AxialStage();
			booster.setName("Booster");
			rocket.addChild(booster);
			booster.addChild(new BodyTube());
			booster.getSeparationConfigurations().getDefault().setSeparationEvent(SeparationEvent.BURNOUT);
			booster.getSeparationConfigurations().getDefault().setSeparationDelay(1.5);
			FlightConfigurationId fcid = new FlightConfigurationId();
			rocket.createFlightConfiguration(fcid);
			rocket.setSelectedConfiguration(fcid);
			doc.setSaved(true);
			doc.clearUndo();
			listen(rocket);
			String before = ids(rocket);
			System.out.println("before: ids=" + booster.getSeparationConfigurations().getIds().size() + " isDefault="
					+ booster.getSeparationConfigurations().isDefault(fcid) + " saved=" + doc.isSaved()
					+ " undoAvail=" + doc.isUndoAvailable());
			StageSeparationConfiguration copy = booster.getSeparationConfiguration();
			System.out.println("after: events=" + takeEvents() + " modIdsUnchanged=" + before.equals(ids(rocket))
					+ " ids=" + booster.getSeparationConfigurations().getIds().size() + " contains="
					+ booster.getSeparationConfigurations().getIds().contains(fcid) + " isDefault="
					+ booster.getSeparationConfigurations().isDefault(fcid) + " copy=" + sep(copy) + " default="
					+ sep(booster.getSeparationConfigurations().getDefault()) + " sameObject="
					+ (copy == booster.getSeparationConfigurations().getDefault()) + " saved=" + doc.isSaved()
					+ " undoAvail=" + doc.isUndoAvailable());
			System.out.println("second access same object: " + (booster.getSeparationConfiguration() == copy)
					+ " events=" + takeEvents());
			System.out.println("sustainer (stage 0) access: ids before="
					+ rocket.getStage(0).getSeparationConfigurations().getIds().size());
			rocket.getStage(0).getSeparationConfiguration();
			System.out.println("sustainer ids after=" + rocket.getStage(0).getSeparationConfigurations().getIds().size()
					+ " events=" + takeEvents());

			// A copy of the rocket (an undo snapshot) carries the override.
			Rocket snapshot = rocket.copyWithOriginalID();
			AxialStage snapBooster = (AxialStage) snapshot.getChild(1);
			System.out.println("snapshot has override: "
					+ snapBooster.getSeparationConfigurations().getIds().contains(fcid));

			// Undo after a later change: the history's state was taken before the access.
			doc.addUndoPosition("Rename");
			rocket.getStage(0).setName("Renamed");
			takeEvents();
			doc.undo();
			AxialStage afterUndo = (AxialStage) rocket.getChild(1);
			System.out.println("after undo: override kept="
					+ afterUndo.getSeparationConfigurations().getIds().contains(fcid) + " name="
					+ rocket.getStage(0).getName() + " events=" + takeEvents());
			doc.redo();
			AxialStage afterRedo = (AxialStage) rocket.getChild(1);
			System.out.println("after redo: override kept="
					+ afterRedo.getSeparationConfigurations().getIds().contains(fcid) + " name="
					+ rocket.getStage(0).getName() + " events=" + takeEvents());

			// With the default configuration selected: nothing is stored.
			rocket.setSelectedConfiguration(FlightConfigurationId.DEFAULT_VALUE_FCID);
			takeEvents();
			AxialStage b = (AxialStage) rocket.getChild(1);
			int n = b.getSeparationConfigurations().getIds().size();
			StageSeparationConfiguration d = b.getSeparationConfiguration();
			System.out.println("default selected: ids before=" + n + " after=" + b.getSeparationConfigurations().getIds().size()
					+ " isDefaultObject=" + (d == b.getSeparationConfigurations().getDefault()) + " events=" + takeEvents());
		}
	}
}
