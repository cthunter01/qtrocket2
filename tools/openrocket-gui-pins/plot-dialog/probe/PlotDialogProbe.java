package probe;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.io.File;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import javax.swing.AbstractButton;
import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;

import org.jfree.chart.ChartPanel;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.xy.XYItemRenderer;
import org.jfree.chart.ui.Layer;
import org.jfree.data.Range;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.SimulationAbort;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.plot.SimulationPlotConfiguration;
import info.openrocket.swing.gui.plot.SimulationPlotDialog;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;
import net.miginfocom.swing.MigLayout;

/**
 * What OpenRocket's plot dialog (swing gui/plot/PlotDialog.java and SimulationPlotDialog.java) is
 * and does, for Milestone 4's part G4 (dialogs/plot/PlotDialog and SimulationPlotDialog): made
 * through SimulationPlotDialog.getPlot() as the plot tab's doPlot() makes it, never shown (a
 * JDialog, which a headless AWT refuses to make: the probe runs under xvfb-run), under FlatLaf
 * Light with SwingPreferences at their defaults, over stored simulations of the examples
 * (data/examples): "A simple model rocket" #1 (one stage, over the time and over the Mach
 * number), "Three stage low power rocket" #1 (three stages) and a copy of that flight with an
 * abort added to its first and last stage (as the plot scout's probe adds them). For each: the
 * window (title, modality, close operation, Escape, the default button), the panel's components
 * in order (kind, text, icon, visibility, state, MigLayout constraints, the stage box's items) and
 * after pack() their bounds; then what the check boxes, the stage box and the zoom buttons do to
 * the chart and the preferences, and what the dialog remembers of its size and place. Bounds
 * depend on the fonts: the tests pin only what does not (the chart's size after pack(), the gap
 * below it, the order of the row). The whole transcript is pinned by hand (hand-pinned.txt).
 */
public class PlotDialogProbe {

	public static void main(String[] args) throws Exception {
		bootstrap();
		File examples = new File(args[0]);
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> GUIUtil.setUITheme(UITheme.Themes.LIGHT));
			OpenRocketDocument simple = load(new File(examples, "A simple model rocket.ork"));
			OpenRocketDocument three = load(new File(examples, "Three stage low power rocket.ork"));
			Simulation simpleSim = simple.getSimulations().get(0);
			Simulation threeSim = three.getSimulations().get(0);
			Simulation aborted = three.getSimulations().get(1);
			injectAborts(aborted, threeSim, new int[] { 0, 2 });

			dialog("simple-vertical-motion", simpleSim, 0);
			dialog("simple-drag-mach", simpleSim, 5);
			dialog("three-stage", threeSim, 0);
			dialog("three-stage-aborts", aborted, 0);
			remembered(simpleSim);
		} catch (Exception e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	static OpenRocketDocument load(File file) throws Exception {
		DeterministicUuids.reseed(file.getName());
		return new GeneralRocketLoader(file).load();
	}

	/** Gives {@code target} the data of {@code source} with a SIM_ABORT in the branches {@code aborted}. */
	static void injectAborts(Simulation target, Simulation source, int[] aborted) throws Exception {
		List<FlightDataBranch> copies = new ArrayList<>();
		SimulationAbort.Cause[] causes = SimulationAbort.Cause.values();
		int b = 0;
		for (FlightDataBranch branch : source.getSimulatedData().getBranches()) {
			FlightDataBranch copy = branch.clone();
			for (int a : aborted) {
				if (a == b) {
					List<Double> time = copy.getView(FlightDataType.TYPE_TIME);
					double t = time.get(time.size() - 1) * 0.6;
					copy.addEvent(new FlightEvent(FlightEvent.Type.SIM_ABORT, t, null,
							new SimulationAbort(causes[b % causes.length])));
				}
			}
			copies.add(copy);
			b++;
		}
		FlightData data = new FlightData(copies.toArray(new FlightDataBranch[0]));
		Field f = Simulation.class.getDeclaredField("simulatedData");
		f.setAccessible(true);
		f.set(target, data);
		Field name = Simulation.class.getDeclaredField("name");
		name.setAccessible(true);
		name.set(target, "Aborted copy");
	}

	static SimulationPlotConfiguration preset(int k) {
		// SimulationPlotPanel's preset combo box: a copy in the default units.
		return SimulationPlotConfiguration.DEFAULT_CONFIGURATIONS[k].clone().resetUnits();
	}

	static void dialog(String name, Simulation sim, int presetIndex) throws Exception {
		System.out.println("== dialog " + name + ": '" + sim.getName() + "' preset " + presetIndex + " branches "
				+ sim.getSimulatedData().getBranchCount() + " hasErrors " + sim.hasErrors());
		SimulationPlotDialog[] made = new SimulationPlotDialog[1];
		SwingUtilities.invokeAndWait(() -> {
			made[0] = SimulationPlotDialog.getPlot(null, sim, preset(presetIndex));
			window(made[0]);
			components(made[0]);
		});
		SimulationPlotDialog dialog = made[0];
		SwingUtilities.invokeAndWait(() -> {
			try {
				behaviour(dialog, sim);
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		});
		SwingUtilities.invokeAndWait(dialog::dispose);
		// The check boxes store their preferences: back to the defaults for the next dialog.
		Application.getPreferences().putBoolean(ApplicationPreferences.PLOT_SHOW_POINTS, false);
		Application.getPreferences().putBoolean(ApplicationPreferences.PLOT_SHOW_EVENTS, true);
	}

	static void window(JDialog dialog) {
		System.out.println("title='" + dialog.getTitle() + "'");
		System.out.println("modality=" + dialog.getModalityType());
		System.out.println("defaultCloseOperation="
				+ (dialog.getDefaultCloseOperation() == JDialog.DISPOSE_ON_CLOSE ? "DISPOSE_ON_CLOSE"
						: Integer.toString(dialog.getDefaultCloseOperation())));
		Object escape = dialog.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
				.get(KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0));
		System.out.println("escapeCloses=" + (escape != null));
		JButton defaultButton = dialog.getRootPane().getDefaultButton();
		System.out.println("defaultButton=" + (defaultButton == null ? "none" : "'" + defaultButton.getText() + "'"));
		System.out.println("resizable=" + dialog.isResizable());
		System.out.println("owner=" + (dialog.getOwner() == null ? "none" : dialog.getOwner().getClass().getName()));
	}

	static JPanel panelOf(JDialog dialog) {
		Container content = dialog.getContentPane();
		for (Component c : content.getComponents()) {
			if (c instanceof JPanel panel && panel.getLayout() instanceof MigLayout) {
				return panel;
			}
		}
		throw new IllegalStateException("no MigLayout panel");
	}

	static String iconName(Icon icon) {
		if (icon == null) {
			return "none";
		}
		if (icon == Icons.ZOOM_IN) {
			return "ZOOM_IN";
		}
		if (icon == Icons.ZOOM_OUT) {
			return "ZOOM_OUT";
		}
		if (icon == Icons.ZOOM_RESET) {
			return "ZOOM_RESET";
		}
		return "other";
	}

	static String bounds(Rectangle r) {
		return r.x + "," + r.y + " " + r.width + "x" + r.height;
	}

	static String size(Dimension d) {
		return d.width + "x" + d.height;
	}

	static void components(JDialog dialog) {
		JPanel panel = panelOf(dialog);
		MigLayout layout = (MigLayout) panel.getLayout();
		System.out.println("layout='" + layout.getLayoutConstraints() + "' columns='" + layout.getColumnConstraints()
				+ "' rows='" + layout.getRowConstraints() + "'");
		Component[] all = panel.getComponents();
		Rectangle packedChart = all[0].getBounds();
		// SimulationPlotDialog hides "Show errors" after pack() (its constructor, after PlotDialog's): the
		// layout that shows the dialog leaves it out (hidemode 3).
		dialog.validate();
		int i = 0;
		JLabel plainLabel = new JLabel();
		for (Component c : all) {
			StringBuilder line = new StringBuilder("component " + i + ": " + kind(c));
			if (c instanceof JLabel label) {
				line.append(" text='").append(label.getText()).append("'");
				line.append(" font size relative to a label's=")
						.append(label.getFont().getSize2D() - plainLabel.getFont().getSize2D());
				line.append(" foreground=").append(Integer.toHexString(label.getForeground().getRGB()));
			}
			if (c instanceof AbstractButton button) {
				line.append(" text='").append(button.getText()).append("' icon=").append(iconName(button.getIcon()));
				line.append(" tooltip=").append(button.getToolTipText() == null ? "none"
						: "'" + button.getToolTipText() + "'");
			}
			if (c instanceof JCheckBox box) {
				line.append(" selected=").append(box.isSelected());
			}
			if (c instanceof JComboBox<?> combo) {
				line.append(" items=[");
				for (int k = 0; k < combo.getItemCount(); k++) {
					line.append(k == 0 ? "" : " | ").append(combo.getItemAt(k));
				}
				line.append("] selected=").append(combo.getSelectedIndex());
			}
			if (c instanceof ChartPanel chart) {
				line.append(" preferred=").append(size(chart.getPreferredSize())).append(" minimum=")
						.append(size(chart.getMinimumSize()));
			}
			line.append(" visible=").append(c.isVisible()).append(" enabled=").append(c.isEnabled());
			line.append(" constraints='").append(layout.getComponentConstraints(c)).append("'");
			System.out.println(line);
			i++;
		}
		// What does not depend on the fonts.
		Rectangle chart = all[0].getBounds();
		Rectangle label = all[1].getBounds();
		System.out.println("chart after pack=" + bounds(packedChart) + " gap below the chart="
				+ (label.y - (chart.y + chart.height)) + " chart right edge to the panel's="
				+ (panel.getWidth() - (chart.x + chart.width)));
		Component close = all[all.length - 1];
		System.out.println("Close's right edge is the chart's=" + (close.getX() + close.getWidth() == chart.x
				+ chart.width));
		List<Component> row = new ArrayList<>();
		for (Component c : all) {
			if (c.isVisible() && c.getY() > label.y && !(c instanceof JLabel)) {
				row.add(c);
			}
		}
		StringBuilder order = new StringBuilder("row, left to right:");
		StringBuilder gaps = new StringBuilder("gaps between the row's neighbours but Close:");
		for (int k = 0; k < row.size(); k++) {
			Component c = row.get(k);
			order.append(" ").append(c instanceof AbstractButton b && !b.getText().isEmpty() ? b.getText()
					: c instanceof AbstractButton b ? iconName(b.getIcon()) : kind(c));
			if (k > 0 && k + 1 < row.size()) {
				Component previous = row.get(k - 1);
				gaps.append(" ").append(c.getX() - (previous.getX() + previous.getWidth()));
			}
		}
		System.out.println(order);
		System.out.println(gaps);
		boolean oneRow = true;
		for (Component c : row) {
			oneRow &= c.getY() + c.getHeight() / 2 == row.get(0).getY() + row.get(0).getHeight() / 2
					|| Math.abs((c.getY() + c.getHeight() / 2.0) - (row.get(0).getY() + row.get(0).getHeight() / 2.0)) <= 1;
		}
		System.out.println("the row's components centred on one line=" + oneRow);
	}

	static String kind(Component c) {
		if (c instanceof ChartPanel) {
			return c.getClass().getSimpleName() + "(ChartPanel)";
		}
		return c.getClass().getSimpleName();
	}

	static ChartPanel chartOf(JDialog dialog) {
		return (ChartPanel) panelOf(dialog).getComponent(0);
	}

	static List<Component> controls(JDialog dialog, Class<?> type) {
		List<Component> found = new ArrayList<>();
		for (Component c : panelOf(dialog).getComponents()) {
			if (type.isInstance(c)) {
				found.add(c);
			}
		}
		return found;
	}

	static String ranges(XYPlot plot) {
		StringBuilder text = new StringBuilder("x=" + range(plot.getDomainAxis().getRange()));
		for (int a = 0; a < plot.getRangeAxisCount(); a++) {
			if (plot.getRangeAxis(a) != null) {
				text.append(" y").append(a).append("=").append(range(plot.getRangeAxis(a).getRange()));
			}
		}
		return text.toString();
	}

	static String range(Range r) {
		return "[" + r.getLowerBound() + ", " + r.getUpperBound() + "]";
	}

	/** The ranges after a zoom, to 6 digits: ChartPanel zooms about the middle of its data area, in pixels. */
	static String zoomed(XYPlot plot) {
		StringBuilder text = new StringBuilder("x=" + rounded(plot.getDomainAxis().getRange()));
		for (int a = 0; a < plot.getRangeAxisCount(); a++) {
			if (plot.getRangeAxis(a) != null) {
				text.append(" y").append(a).append("=").append(rounded(plot.getRangeAxis(a).getRange()));
			}
		}
		return text.toString();
	}

	static String rounded(Range r) {
		return String.format(java.util.Locale.ROOT, "[%.6g, %.6g]", r.getLowerBound(), r.getUpperBound());
	}

	static String state(XYPlot plot) {
		Collection<?> markers = plot.getDomainMarkers(Layer.FOREGROUND);
		Collection<?> background = plot.getDomainMarkers(Layer.BACKGROUND);
		StringBuilder visible = new StringBuilder();
		for (int d = 0; d < plot.getDatasetCount(); d++) {
			XYItemRenderer renderer = plot.getRenderer(d);
			int count = plot.getDataset(d).getSeriesCount();
			for (int s = 0; s < count; s++) {
				visible.append(renderer.isSeriesVisible(s) ? "1" : "0");
			}
			visible.append(d + 1 < plot.getDatasetCount() ? "|" : "");
		}
		boolean shapes = false;
		for (int d = 0; d < plot.getDatasetCount(); d++) {
			if (plot.getRenderer(d) instanceof org.jfree.chart.renderer.xy.XYLineAndShapeRenderer r
					&& r.getDefaultShapesVisible()) {
				shapes = true;
			}
		}
		return "domainMarkers=" + (markers == null ? 0 : markers.size()) + "+" + (background == null ? 0
				: background.size()) + " annotations=" + plot.getAnnotations().size() + " seriesVisible=" + visible
				+ " shapes=" + shapes;
	}

	static void click(AbstractButton button, int modifiers) {
		for (ActionListener l : button.getActionListeners()) {
			l.actionPerformed(new ActionEvent(button, ActionEvent.ACTION_PERFORMED, button.getActionCommand(),
					modifiers));
		}
	}

	static void behaviour(SimulationPlotDialog dialog, Simulation sim) throws Exception {
		ChartPanel chartPanel = chartOf(dialog);
		XYPlot plot = chartPanel.getChart().getXYPlot();
		ApplicationPreferences prefs = Application.getPreferences();
		System.out.println("at open: " + state(plot) + " " + ranges(plot));
		List<Component> boxes = controls(dialog, JCheckBox.class);
		JCheckBox points = (JCheckBox) boxes.get(0);
		JCheckBox events = (JCheckBox) boxes.get(1);
		JCheckBox errors = (JCheckBox) boxes.get(2);
		points.doClick();
		System.out.println("Show data points clicked: selected=" + points.isSelected() + " pref ShowPlotPoints="
				+ prefs.getBoolean(ApplicationPreferences.PLOT_SHOW_POINTS, false) + " " + state(plot));
		points.doClick();
		System.out.println("Show data points clicked again: selected=" + points.isSelected()
				+ " pref ShowPlotPoints=" + prefs.getBoolean(ApplicationPreferences.PLOT_SHOW_POINTS, false) + " "
				+ state(plot));
		events.doClick();
		System.out.println("Show events clicked: selected=" + events.isSelected() + " pref ShowPlotEvents="
				+ prefs.getBoolean(ApplicationPreferences.PLOT_SHOW_EVENTS, true) + " " + state(plot));
		events.doClick();
		System.out.println("Show events clicked again: selected=" + events.isSelected() + " pref ShowPlotEvents="
				+ prefs.getBoolean(ApplicationPreferences.PLOT_SHOW_EVENTS, true) + " " + state(plot));
		errors.doClick();
		System.out.println("Show errors clicked: selected=" + errors.isSelected() + " " + state(plot));
		errors.doClick();
		System.out.println("Show errors clicked again: selected=" + errors.isSelected() + " " + state(plot));

		List<Component> combos = controls(dialog, JComboBox.class);
		if (combos.isEmpty()) {
			System.out.println("no stage box in the panel");
		}
		JComboBox<?> stages = combos.isEmpty() ? new JComboBox<String>() : (JComboBox<?>) combos.get(0);
		for (int k = 1; k < stages.getItemCount(); k++) {
			stages.setSelectedIndex(k);
			System.out.println("stage " + k + " '" + stages.getItemAt(k) + "': errors enabled=" + errors.isEnabled()
					+ " " + state(plot));
		}
		if (stages.getItemCount() > 1) {
			stages.setSelectedIndex(0);
			System.out.println("stage 0 '" + stages.getItemAt(0) + "': errors enabled=" + errors.isEnabled() + " "
					+ state(plot));
			// The stage box with Show events unchecked.
			events.doClick();
			stages.setSelectedIndex(1);
			System.out.println("Show events unchecked, stage 1: " + state(plot));
			stages.setSelectedIndex(0);
			System.out.println("Show events unchecked, stage 0: " + state(plot));
			events.doClick();
		}

		List<Component> buttons = new ArrayList<>();
		for (Component c : controls(dialog, JButton.class)) {
			buttons.add(c);
		}
		JButton zoomIn = (JButton) buttons.get(0);
		JButton reset = (JButton) buttons.get(1);
		JButton zoomOut = (JButton) buttons.get(2);
		// ChartPanel's zoom is about the centre of its last drawn data area: lay it out at its size.
		chartPanel.setSize(chartPanel.getPreferredSize());
		java.awt.image.BufferedImage image = new java.awt.image.BufferedImage(chartPanel.getWidth(),
				chartPanel.getHeight(), java.awt.image.BufferedImage.TYPE_INT_ARGB);
		java.awt.Graphics2D g = image.createGraphics();
		chartPanel.paint(g);
		g.dispose();
		System.out.println("before zoom: " + zoomed(plot));
		click(zoomIn, 0);
		System.out.println("zoom in: " + zoomed(plot));
		click(reset, 0);
		System.out.println("reset: " + zoomed(plot));
		click(zoomIn, InputEvent.ALT_MASK);
		System.out.println("zoom in, Alt: " + zoomed(plot));
		click(reset, 0);
		click(zoomOut, 0);
		System.out.println("zoom out: " + zoomed(plot));
		click(reset, 0);
		click(zoomOut, InputEvent.ALT_MASK);
		System.out.println("zoom out, Alt: " + zoomed(plot));
		click(reset, InputEvent.ALT_MASK);
		System.out.println("reset, Alt: " + zoomed(plot));
	}

	static void remembered(Simulation sim) throws Exception {
		System.out.println("== remembered size and place");
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		SimulationPlotDialog[] made = new SimulationPlotDialog[1];
		SwingUtilities.invokeAndWait(() -> made[0] = SimulationPlotDialog.getPlot(null, sim, preset(0)));
		System.out.println("first, made (not shown): stored size is its size="
				+ made[0].getSize().equals(prefs.getWindowSize(SimulationPlotDialog.class)) + " a position stored="
				+ (prefs.getWindowPosition(SimulationPlotDialog.class) != null));
		SwingUtilities.invokeAndWait(() -> {
			made[0].setSize(900, 700);
			made[0].setLocation(40, 30);
		});
		// The component listeners run in later turns of the event queue.
		SwingUtilities.invokeAndWait(() -> { });
		SwingUtilities.invokeAndWait(() -> { });
		System.out.println("after setSize(900, 700), setLocation(40, 30): stored size="
				+ prefs.getWindowSize(SimulationPlotDialog.class) + " stored position="
				+ prefs.getWindowPosition(SimulationPlotDialog.class) + " keys="
				+ String.join(",", prefs.getWindowsPreferences().keys()));
		SwingUtilities.invokeAndWait(made[0]::dispose);
		SwingUtilities.invokeAndWait(() -> made[0] = SimulationPlotDialog.getPlot(null, sim, preset(4)));
		Point location = made[0].getLocation();
		System.out.println("next dialog: size=" + size(made[0].getSize()) + " location=" + location.x + ","
				+ location.y);
		SwingUtilities.invokeAndWait(made[0]::dispose);
	}

	static void bootstrap() throws Exception {
		// Before anything makes a UUID (the pins' rule: every probe is seeded).
		DeterministicUuids.install();
		DeterministicUuids.reseed("plot-dialog");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
