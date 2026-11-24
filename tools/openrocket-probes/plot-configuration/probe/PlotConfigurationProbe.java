package probe;

import java.util.Arrays;
import java.util.List;
import java.util.function.ToDoubleFunction;

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.plot.Axis;
import info.openrocket.swing.gui.plot.PlotConfiguration;
import info.openrocket.swing.gui.plot.SimulationPlotConfiguration;

/**
 * Milestone 4 run 1, part P1: OpenRocket's PlotConfiguration (the automatic axes and ranges of the simulation plots)
 * and SimulationPlotConfiguration (the 9 presets) over small hand-made flight data branches, on OpenRocket's core
 * with ServicesForTesting. tests/core/plot/PlotConfigurationTests.cpp and SimulationPlotConfigurationTests.cpp pin
 * what it prints by hand (expected.txt; see ../README.md).
 *
 * Every double is printed with Double.toString and, in braces, Double.toHexString.
 */
public class PlotConfigurationProbe {
	// Before the type constants: FlightDataType translates its names when it is loaded.
	static {
		Injector injector = Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule()));
		Application.setInjector(injector);
	}

	static final FlightDataType TIME = FlightDataType.TYPE_TIME;
	static final FlightDataType ALT = FlightDataType.TYPE_ALTITUDE;
	static final FlightDataType VEL = FlightDataType.TYPE_VELOCITY_Z;
	static final FlightDataType ACC = FlightDataType.TYPE_ACCELERATION_Z;
	static final FlightDataType MACH = FlightDataType.TYPE_MACH_NUMBER;

	static String d(double v) {
		return v + "{" + Double.toHexString(v) + "}";
	}

	static String axes(PlotConfiguration<?, ?> config) {
		List<Axis> all = config.getAllAxes();
		return "left [" + d(all.get(0).getMinValue()) + ", " + d(all.get(0).getMaxValue()) + "] right ["
				+ d(all.get(1).getMinValue()) + ", " + d(all.get(1).getMaxValue()) + "]";
	}

	static String rows(PlotConfiguration<?, ?> config) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < config.getDataCount(); i++) {
			sb.append(i == 0 ? "" : " ").append(config.getAxis(i));
		}
		return "[" + sb + "]";
	}

	/** A branch with the given types; values[point][type]. */
	static FlightDataBranch branch(String name, FlightDataType[] types, double[][] values) {
		FlightDataBranch branch = new FlightDataBranch(name, types);
		for (double[] point : values) {
			branch.addPoint();
			for (int i = 0; i < types.length; i++) {
				branch.setValue(types[i], point[i]);
			}
		}
		return branch;
	}

	/** Time 0, 1, ... and the values of one more type. */
	static FlightDataBranch series(String name, FlightDataType type, double... values) {
		double[][] points = new double[values.length][];
		for (int i = 0; i < values.length; i++) {
			points[i] = new double[] { i, values[i] };
		}
		return branch(name, new FlightDataType[] { TIME, type }, points);
	}

	/** Time 0..10 with an altitude and a vertical velocity rising linearly between the given ends. */
	static FlightDataBranch linear(String name, double alt0, double alt10, double vel0, double vel10) {
		double[][] points = new double[11][];
		for (int i = 0; i <= 10; i++) {
			points[i] = new double[] { i, alt0 + (alt10 - alt0) * i / 10, vel0 + (vel10 - vel0) * i / 10 };
		}
		return branch(name, new FlightDataType[] { TIME, ALT, VEL }, points);
	}

	static void fit(String title, SimulationPlotConfiguration config, FlightDataBranch... data) {
		config.fitAxes(Arrays.asList(data));
		System.out.println("FIT " + title + " axes=" + rows(config) + " | " + axes(config));
	}

	static SimulationPlotConfiguration config(FlightDataType[] types, int[] axes) {
		SimulationPlotConfiguration config = new SimulationPlotConfiguration("probe");
		for (int i = 0; i < types.length; i++) {
			if (axes[i] < 0) {
				config.addPlotDataType(types[i]);
			} else {
				config.addPlotDataType(types[i], axes[i]);
			}
		}
		return config;
	}

	/** Prints every candidate's axes and goodness, in the order fillAutoAxes() scores them. */
	static class LoggedConfiguration extends SimulationPlotConfiguration {
		LoggedConfiguration() {
			super("logged");
		}

		@Override
		protected double getGoodnessValue(FlightDataBranch data) {
			double goodness = super.getGoodnessValue(data);
			System.out.println("  CANDIDATE " + rows(this) + " goodness=" + d(goodness) + " | " + axes(this));
			return goodness;
		}
	}

	/** Scores a candidate by its axes through a function (the clones share it). */
	static class ScoredConfiguration extends SimulationPlotConfiguration {
		ToDoubleFunction<String> score;

		ScoredConfiguration(ToDoubleFunction<String> score) {
			super("scored");
			this.score = score;
		}

		@Override
		protected double getGoodnessValue(FlightDataBranch data) {
			double goodness = score.applyAsDouble(rows(this));
			System.out.println("  SCORED " + rows(this) + " goodness=" + d(goodness));
			return goodness;
		}
	}

	static void auto(String title, FlightDataType[] types, int[] axes, FlightDataBranch main) {
		LoggedConfiguration config = new LoggedConfiguration();
		for (int i = 0; i < types.length; i++) {
			if (axes[i] < 0) {
				config.addPlotDataType(types[i]);
			} else {
				config.addPlotDataType(types[i], axes[i]);
			}
		}
		System.out.println("AUTO " + title + " rows=" + rows(config));
		PlotConfiguration<?, ?> filled = config.fillAutoAxes(main);
		System.out.println("  CHOSEN " + rows(filled) + " | " + axes(filled) + " | class="
				+ filled.getClass().getSimpleName() + " source still " + rows(config));
	}

	static void scored(String title, int autoRows, ToDoubleFunction<String> score, FlightDataBranch main) {
		ScoredConfiguration config = new ScoredConfiguration(score);
		FlightDataType[] types = { ALT, VEL, ACC };
		for (int i = 0; i < autoRows; i++) {
			config.addPlotDataType(types[i]);
		}
		System.out.println("SCORED " + title + " rows=" + rows(config));
		PlotConfiguration<?, ?> filled = config.fillAutoAxes(main);
		System.out.println("  CHOSEN " + rows(filled) + " | " + axes(filled));
	}

	static String unit(Unit u) {
		return "'" + u.getUnit() + "'";
	}

	static void presets() {
		for (SimulationPlotConfiguration preset : SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS) {
			System.out.println("PRESET \"" + preset.getName() + "\" domain=" + preset.getDomainAxisType().getName() + " "
					+ unit(preset.getDomainAxisUnit()));
			for (int i = 0; i < preset.getDataCount(); i++) {
				System.out.println("  ROW " + i + " " + preset.getType(i).getName() + " " + unit(preset.getUnit(i))
						+ " axis=" + preset.getAxis(i) + " color=" + preset.getPlotDataColor(i) + " style="
						+ preset.getPlotDataLineStyle(i).name());
			}
			StringBuilder events = new StringBuilder();
			for (FlightEvent.Type type : preset.getActiveEvents()) {
				events.append(' ').append(type.name());
			}
			System.out.println("  EVENTS" + events);
		}
	}

	static void edits() {
		SimulationPlotConfiguration config = new SimulationPlotConfiguration("edits");
		System.out.println("EDITS new: domain=" + config.getDomainAxisType().getName() + " "
				+ unit(config.getDomainAxisUnit()) + " rows=" + config.getDataCount() + " events="
				+ config.getActiveEvents());
		config.addPlotDataType(ALT);
		config.setPlotDataUnit(0, UnitGroup.UNITS_DISTANCE.getUnit("ft"));
		config.setPlotDataType(0, FlightDataType.TYPE_POSITION_X);
		System.out.println("EDITS same group: " + config.getType(0).getName() + " " + unit(config.getUnit(0)));
		config.setPlotDataType(0, VEL);
		System.out.println("EDITS other group: " + config.getType(0).getName() + " " + unit(config.getUnit(0)));
		config.setDomainAxisUnit(UnitGroup.UNITS_LONG_TIME.getUnit("min"));
		config.setDomainAxisType(FlightDataType.TYPE_TIME_STEP);
		System.out.println("EDITS domain time step: " + config.getDomainAxisType().getName() + " "
				+ unit(config.getDomainAxisUnit()) + " group same=" + (TIME.getUnitGroup() == FlightDataType.TYPE_TIME_STEP.getUnitGroup()));
		config.setDomainAxisType(MACH);
		System.out.println("EDITS domain mach: " + config.getDomainAxisType().getName() + " "
				+ unit(config.getDomainAxisUnit()));
		config.setDomainAxisType(ALT);
		config.setDomainAxisUnit(UnitGroup.UNITS_DISTANCE.getUnit("ft"));
		config.setDomainAxisType(FlightDataType.TYPE_POSITION_X);
		System.out.println("EDITS domain same group: " + config.getDomainAxisType().getName() + " "
				+ unit(config.getDomainAxisUnit()));
		config.setPlotDataUnit(0, UnitGroup.UNITS_VELOCITY.getUnit("ft/s"));
		config.resetUnits();
		System.out.println("EDITS resetUnits: domain " + unit(config.getDomainAxisUnit()) + " row "
				+ unit(config.getUnit(0)));
		for (int axis : new int[] { 2, -2 }) {
			try {
				config.setPlotDataAxis(0, axis);
				System.out.println("EDITS setPlotDataAxis(" + axis + "): axis=" + config.getAxis(0));
			} catch (RuntimeException e) {
				System.out.println("EDITS setPlotDataAxis(" + axis + "): " + e.getClass().getSimpleName() + ": "
						+ e.getMessage());
			}
		}
		try {
			config.setPlotDataUnit(0, UnitGroup.UNITS_DISTANCE.getUnit("ft"));
		} catch (RuntimeException e) {
			System.out.println("EDITS unit of another group: " + e.getClass().getSimpleName());
		}

		// What the copies share
		SimulationPlotConfiguration source = new SimulationPlotConfiguration("source");
		source.addPlotDataType(ALT, 0);
		SimulationPlotConfiguration viaClone = source.clone();
		viaClone.setEvent(FlightEvent.Type.LAUNCH, true);
		System.out.println("SHARE clone(): source has LAUNCH=" + source.isEventActive(FlightEvent.Type.LAUNCH));
		SimulationPlotConfiguration viaCloneConfiguration = source.cloneConfiguration();
		viaCloneConfiguration.setEvent(FlightEvent.Type.APOGEE, true);
		System.out.println("SHARE cloneConfiguration(): source has APOGEE="
				+ source.isEventActive(FlightEvent.Type.APOGEE));
		SimulationPlotConfiguration filled = source.fillAutoAxes(series("b", ALT, 1, 2));
		filled.setEvent(FlightEvent.Type.GROUND_HIT, true);
		System.out.println("SHARE fillAutoAxes(): source has GROUND_HIT="
				+ source.isEventActive(FlightEvent.Type.GROUND_HIT));
	}

	static void emptyConfiguration() {
		SimulationPlotConfiguration config = new SimulationPlotConfiguration("empty");
		try {
			PlotConfiguration<?, ?> filled = config.fillAutoAxes(series("b", ALT, 1, 2));
			System.out.println("EMPTY fillAutoAxes: " + rows(filled) + " | " + axes(filled));
		} catch (RuntimeException e) {
			System.out.println("EMPTY fillAutoAxes: " + e.getClass().getSimpleName() + ": " + e.getMessage());
		}
		config.fitAxes(List.of());
		System.out.println("EMPTY fitAxes(no branches): " + axes(config));
	}

	public static void main(String[] args) {
		System.out.println("UNITS distance " + unit(UnitGroup.UNITS_DISTANCE.getDefaultUnit()) + " velocity "
				+ unit(UnitGroup.UNITS_VELOCITY.getDefaultUnit()) + " acceleration "
				+ unit(UnitGroup.UNITS_ACCELERATION.getDefaultUnit()) + " time "
				+ unit(UnitGroup.UNITS_LONG_TIME.getDefaultUnit()));

		FlightDataType[] alt = { ALT };
		int[] left = { 0 };

		// The range of one row on the left axis: a flat range gets +-1, every range 3 % on both sides, and zero
		// when the nearer end is within 30 % of the range.
		fit("flat", config(alt, left), series("b", ALT, 7, 7));
		fit("single-point", config(alt, left), series("b", ALT, 7));
		fit("margins", config(alt, left), series("b", ALT, 10, 20));
		fit("zero-near-low", config(alt, left), series("b", ALT, 2, 12));
		fit("zero-just-in", config(alt, left), series("b", ALT, 3.4, 13.4));
		fit("zero-just-out", config(alt, left), series("b", ALT, 3.5, 13.5));
		fit("zero-near-high", config(alt, left), series("b", ALT, -12, -2));
		fit("spans-zero", config(alt, left), series("b", ALT, -3, 7));
		fit("flat-zero", config(alt, left), series("b", ALT, 0, 0));
		fit("tiny", config(alt, left), series("b", ALT, 1e-7, 1.00000001e-7));
		fit("right-only", config(alt, new int[] { 1 }), series("b", ALT, 10, 20));
		SimulationPlotConfiguration feet = config(alt, left);
		feet.setPlotDataUnit(0, UnitGroup.UNITS_DISTANCE.getUnit("ft"));
		fit("in-feet", feet, series("b", ALT, 10, 20));

		// Several branches: the extremes of all of them; an empty branch is skipped.
		FlightDataBranch empty = branch("empty", new FlightDataType[] { TIME, ALT }, new double[0][]);
		fit("branches", config(alt, left), series("b0", ALT, 0, 50), series("b1", ALT, -5, 80), empty);
		fit("branch-0-empty", config(alt, left), empty, series("b1", ALT, -5, 80));
		// Bug 46: a branch without the row's type.
		FlightDataBranch timeOnly = branch("t", new FlightDataType[] { TIME }, new double[][] { { 0 } });
		fit("other-branch-without-type", config(alt, left), series("b0", ALT, 5, 100), timeOnly);
		fit("main-branch-without-type", config(alt, left), timeOnly, series("b1", ALT, 5, 100));
		fit("nan-column", config(alt, left), series("b0", ALT, Double.NaN, Double.NaN),
				series("b1", ALT, 5, 100));

		// The common zero of two axes that both hold zero.
		FlightDataType[] altVel = { ALT, VEL };
		int[] leftRight = { 0, 1 };
		fit("common-zero-lowers-right-min", config(altVel, leftRight), linear("b", -10, 30, -1, 100));
		fit("common-zero-raises-right-max", config(altVel, leftRight), linear("b", -1, 100, -10, 30));
		fit("common-zero-equal", config(altVel, leftRight), linear("b", -10, 30, -10, 30));
		fit("no-common-zero-right-positive", config(altVel, leftRight), linear("b", -10, 30, 50, 100));
		// Bug 45: the left range starts (or ends) at exactly zero and the right one spans zero.
		fit("left-starts-at-zero", config(altVel, leftRight), linear("b", 5, 100, -10, 10));
		fit("left-ends-at-zero", config(altVel, leftRight), linear("b", -100, -5, -10, 10));
		fit("right-starts-at-zero", config(altVel, leftRight), linear("b", -10, 10, 5, 100));

		// The automatic axes: every candidate's goodness, then the choice.
		FlightDataType[] motion = { ALT, VEL, ACC };
		double[][] flight = new double[11][];
		for (int i = 0; i <= 10; i++) {
			double t = i;
			flight[i] = new double[] { t, 50 * t - 4.9 * t * t, 50 - 9.8 * t, -9.8 + (i < 2 ? 40 : 0) };
		}
		FlightDataBranch flightBranch = branch("flight", new FlightDataType[] { TIME, ALT, VEL, ACC }, flight);
		auto("vertical-motion", motion, new int[] { 0, -1, -1 }, flightBranch);
		auto("all-auto", motion, new int[] { -1, -1, -1 }, flightBranch);
		auto("one-auto", new FlightDataType[] { ALT }, new int[] { -1 }, flightBranch);
		auto("none-auto", motion, new int[] { 0, 1, 0 }, flightBranch);
		auto("flat-row", new FlightDataType[] { ALT, VEL }, new int[] { -1, -1 },
				linear("flat", 3, 3, -10, 10));
		auto("left-starts-at-zero", altVel, new int[] { 0, -1 }, linear("b", 5, 100, -10, 10));
		auto("missing-type", new FlightDataType[] { MACH, ALT }, new int[] { -1, -1 }, linear("b", 0, 10, 0, 1));

		// The order and the ties of the brute force, by goodness functions of the axes.
		FlightDataBranch b = series("b", ALT, 1, 2);
		scored("constant", 2, axes -> 5.0, b);
		scored("all-nan", 2, axes -> Double.NaN, b);
		scored("non-finite", 2, axes -> switch (axes) {
			case "[0 0]" -> Double.NaN;
			case "[0 1]" -> 3.0;
			case "[1 0]" -> Double.POSITIVE_INFINITY;
			default -> 3.0;
		}, b);
		scored("first-nan-later-best", 2, axes -> switch (axes) {
			case "[0 0]" -> Double.NaN;
			case "[1 1]" -> 7.0;
			default -> Double.NEGATIVE_INFINITY;
		}, b);
		scored("last-best", 3, axes -> axes.equals("[1 1 1]") ? 2.0 : 1.0, b);
		scored("negative", 2, axes -> axes.equals("[1 0]") ? -1.0 : -2.0, b);

		emptyConfiguration();
		edits();
		presets();
	}
}
