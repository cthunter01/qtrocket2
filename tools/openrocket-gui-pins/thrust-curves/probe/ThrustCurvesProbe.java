package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorComparator;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorPlotDialog;
import info.openrocket.swing.gui.plot.Util;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;
import org.jfree.chart.ChartPanel;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.LegendItemCollection;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.xy.AbstractXYItemRenderer;
import org.jfree.chart.renderer.xy.StandardXYItemRenderer;
import org.jfree.chart.renderer.xy.XYItemRenderer;
import org.jfree.chart.renderer.xy.XYLineAndShapeRenderer;
import org.jfree.chart.title.TextTitle;
import org.jfree.data.xy.XYDataset;

import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;
import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Paint;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.Stroke;
import java.awt.event.KeyEvent;
import java.awt.geom.Rectangle2D;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The thrust-curve charts of OpenRocket's motor chooser, made by OpenRocket's own classes and read back
 * through JFreeChart's API: MotorInformationPanel's preview (made through its private constructor, updateData()
 * and clearData() called by reflection) and ThrustCurveMotorPlotDialog (a JDialog, so the probe needs a display:
 * generate.sh runs it under xvfb-run with -Djava.awt.headless=false; it is never shown). Written for Milestone 3,
 * run 1, part K5, from the M3 plot scout's ThrustCurveChartProbe.
 * <p>
 * The curves of a motor set are its getMotors() sorted with the chooser's ThrustCurveMotorComparator, the list
 * ThrustCurveMotorSelectionPanel hands both charts when "Hide similar thrust curves" is off. Every double is
 * printed with Double.toHexString, every colour as #rrggbb (all are opaque: an alpha would be printed as @a).
 * <p>
 * Lines (each field separated by " | " where a text may hold spaces):
 * <ul>
 * <li>COLOR i | light | dark: Util.getPlotColor(i) and ThrustCurveMotorSelectionPanel.getColor(i) in the dark
 * theme (brighter().brighter()), i = 0..11.</li>
 * <li>TEXT key | text: the charts' and the dialog's texts (previewTitle, dialogWindowTitle, dialogChartTitle,
 * dialogTimeLabel, dialogThrustLabel, closeButton).</li>
 * <li>DIALOG key | value: the dialog's modality, default close operation, Esc binding, default button, the chart's
 * size after pack() and the close button's place against it, the buttons in order.</li>
 * <li>EMPTY xmin | xmax | ymin | ymax: the preview after clearData().</li>
 * <li>SET n | manufacturer | designation | curves: the database's motor sets in its order (n from 0),
 * manufacturer's display name; each followed by CURVE manufacturer | designation | digest | points (its curves in
 * the comparator's order: the manufacturer's display name, the curve's own designation and digest, which together
 * name one curve of the database, where a digest alone does not) and
 * RANGE chart | xmin | xmax | ymin | ymax for the preview and the dialog with the first curve selected.</li>
 * <li>CHART theme | set | selected: for every set of more than one curve, the first and the last curve selected
 * (and, in the dark theme, those of the set with the most curves); each followed by the preview's and the
 * dialog's series, SERIES chart | i | key | paint | width | marker | items, and the dialog's LEGEND i | label.</li>
 * <li>UNIT unit | label | ymin | ymax: the dialog of the first set of more than one curve (first selected) with
 * each unit of UNITS_FORCE as the default unit: its thrust axis' label and range.</li>
 * </ul>
 */
public class ThrustCurvesProbe {

	public static void main(String[] args) throws Exception {
		bootstrap();
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					run();
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			});
		} catch (Exception e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	static void run() throws Exception {
		GUIUtil.setUITheme(UITheme.Themes.LIGHT);
		ThrustCurveMotorSetDatabase db = Application.getInjector().getInstance(ThrustCurveMotorSetDatabase.class);

		for (int i = 0; i < 12; i++) {
			Color color = Util.getPlotColor(i);
			System.out.println("COLOR " + i + " | " + rgb(color) + " | " + rgb(color.brighter().brighter()));
		}

		List<ThrustCurveMotorSet> sets = db.getMotorSets();
		List<List<ThrustCurveMotor>> curves = new ArrayList<>();
		Set<String> keys = new HashSet<>();
		int most = -1;
		int firstMulti = -1;
		for (int n = 0; n < sets.size(); n++) {
			List<ThrustCurveMotor> motors = sets.get(n).getMotors();
			motors.sort(new ThrustCurveMotorComparator());
			curves.add(motors);
			for (ThrustCurveMotor motor : motors) {
				// A digest alone is no key: 708 curves share theirs with a curve of another manufacturer or
				// designation (the same data filed twice, 354 pairs).
				String key = key(motor);
				if (!keys.add(key)) {
					throw new IllegalStateException("two curves " + key);
				}
			}
			if (most < 0 || motors.size() > curves.get(most).size()) {
				most = n;
			}
			if (firstMulti < 0 && motors.size() > 1) {
				firstMulti = n;
			}
		}

		texts(curves.get(firstMulti));

		Object empty = newPanel();
		Method clear = empty.getClass().getDeclaredMethod("clearData");
		clear.setAccessible(true);
		clear.invoke(empty);
		XYPlot emptyPlot = chartOf(empty).getXYPlot();
		System.out.println("EMPTY " + range(emptyPlot));

		for (int n = 0; n < sets.size(); n++) {
			ThrustCurveMotorSet set = sets.get(n);
			List<ThrustCurveMotor> motors = curves.get(n);
			System.out.println("SET " + n + " | " + set.getManufacturer().getDisplayName() + " | "
					+ set.getDesignation() + " | " + motors.size());
			for (ThrustCurveMotor motor : motors) {
				System.out.println("CURVE " + key(motor) + " | " + motor.getTimePoints().length);
			}
			System.out.println("RANGE preview | " + range(preview(motors, 0).getXYPlot()));
			ThrustCurveMotorPlotDialog dialog = new ThrustCurveMotorPlotDialog(motors, 0, null);
			System.out.println("RANGE dialog | " + range(chartIn(dialog).getChart().getXYPlot()));
			dialog.dispose();
		}

		for (int n = 0; n < sets.size(); n++) {
			List<ThrustCurveMotor> motors = curves.get(n);
			if (motors.size() > 1) {
				chart("light", n, motors, 0);
				chart("light", n, motors, motors.size() - 1);
			}
		}

		GUIUtil.setUITheme(UITheme.Themes.DARK);
		chart("dark", most, curves.get(most), 0);
		chart("dark", most, curves.get(most), curves.get(most).size() - 1);
		GUIUtil.setUITheme(UITheme.Themes.LIGHT);

		UnitGroup force = UnitGroup.UNITS_FORCE;
		int defaultForce = force.getDefaultUnitIndex();
		for (int u = 0; u < force.getUnitCount(); u++) {
			force.setDefaultUnit(u);
			ThrustCurveMotorPlotDialog dialog = new ThrustCurveMotorPlotDialog(curves.get(firstMulti), 0, null);
			ValueAxis axis = chartIn(dialog).getChart().getXYPlot().getRangeAxis();
			System.out.println("UNIT " + force.getUnit(u).getUnit() + " | " + axis.getLabel() + " | "
					+ Double.toHexString(axis.getLowerBound()) + " | " + Double.toHexString(axis.getUpperBound()));
			dialog.dispose();
		}
		force.setDefaultUnit(defaultForce);
	}

	/** The dialog's texts and layout, and the preview's title, for the curves of {@code motors}. */
	static void texts(List<ThrustCurveMotor> motors) throws Exception {
		TextTitle previewTitle = preview(motors, 0).getTitle();
		System.out.println("TEXT previewTitle | " + previewTitle.getText());

		ThrustCurveMotorPlotDialog dialog = new ThrustCurveMotorPlotDialog(motors, 0, null);
		ChartPanel chartPanel = chartIn(dialog);
		JFreeChart chart = chartPanel.getChart();
		List<JButton> buttons = new ArrayList<>();
		for (Component c : all(dialog.getContentPane())) {
			if (c instanceof JButton button) {
				buttons.add(button);
			}
		}
		System.out.println("TEXT dialogWindowTitle | " + dialog.getTitle());
		System.out.println("TEXT dialogChartTitle | " + chart.getTitle().getText());
		System.out.println("TEXT dialogTimeLabel | " + chart.getXYPlot().getDomainAxis().getLabel());
		System.out.println("TEXT dialogThrustLabel | " + chart.getXYPlot().getRangeAxis().getLabel());
		if (buttons.size() != 1) {
			throw new IllegalStateException("buttons: " + buttons.size());
		}
		JButton close = buttons.get(0);
		System.out.println("TEXT closeButton | " + close.getText());

		System.out.println("DIALOG modality | " + dialog.getModalityType());
		System.out.println("DIALOG defaultCloseOperation | "
				+ (dialog.getDefaultCloseOperation() == JDialog.DISPOSE_ON_CLOSE ? "DISPOSE_ON_CLOSE"
						: Integer.toString(dialog.getDefaultCloseOperation())));
		Object escape = dialog.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
				.get(KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0));
		System.out.println("DIALOG escapeCloses | " + (escape != null));
		System.out.println("DIALOG defaultButton | " + (dialog.getRootPane().getDefaultButton() != null));
		// The chart and the button share the panel: their bounds are in the same coordinates.
		if (chartPanel.getParent() != close.getParent()) {
			throw new IllegalStateException("the chart and the close button have different parents");
		}
		Rectangle chartBounds = chartPanel.getBounds();
		Rectangle closeBounds = close.getBounds();
		System.out.println("DIALOG chartSize | " + chartBounds.width + "x" + chartBounds.height);
		System.out.println("DIALOG closeRightAligned | "
				+ (closeBounds.x + closeBounds.width == chartBounds.x + chartBounds.width));
		System.out.println("DIALOG closeBelowChart | " + (closeBounds.y >= chartBounds.y + chartBounds.height));
		System.out.println("DIALOG buttons | " + close.getText());
		dialog.dispose();
	}

	/** The preview and the dialog of {@code motors} with curve {@code selected} chosen, in the theme set. */
	static void chart(String theme, int set, List<ThrustCurveMotor> motors, int selected) throws Exception {
		System.out.println("CHART " + theme + " | " + set + " | " + selected);
		series("preview", preview(motors, selected).getXYPlot());
		ThrustCurveMotorPlotDialog dialog = new ThrustCurveMotorPlotDialog(motors, selected, null);
		XYPlot plot = chartIn(dialog).getChart().getXYPlot();
		series("dialog", plot);
		LegendItemCollection legend = plot.getLegendItems();
		for (int i = 0; i < legend.getItemCount(); i++) {
			System.out.println("LEGEND " + i + " | " + legend.get(i).getLabel());
		}
		dialog.dispose();
	}

	static void series(String chart, XYPlot plot) {
		XYDataset data = plot.getDataset();
		XYItemRenderer renderer = plot.getRenderer();
		AbstractXYItemRenderer r = (AbstractXYItemRenderer) renderer;
		for (int s = 0; s < data.getSeriesCount(); s++) {
			System.out.println("SERIES " + chart + " | " + s + " | " + data.getSeriesKey(s) + " | "
					+ rgb(r.lookupSeriesPaint(s)) + " | " + width(r.lookupSeriesStroke(s)) + " | "
					+ marker(renderer, s) + " | " + data.getItemCount(s));
		}
	}

	/** The marker the renderer draws at the points of series {@code s}: none, or the shape's bounds. */
	static String marker(XYItemRenderer renderer, int s) {
		boolean visible;
		boolean filled;
		if (renderer instanceof StandardXYItemRenderer standard) {
			visible = standard.getBaseShapesVisible();
			filled = standard.getBaseShapesFilled();
		} else if (renderer instanceof XYLineAndShapeRenderer line) {
			visible = line.getItemShapeVisible(s, 0);
			filled = line.getItemShapeFilled(s, 0);
		} else {
			throw new IllegalStateException("renderer " + renderer.getClass());
		}
		Shape shape = ((AbstractXYItemRenderer) renderer).lookupSeriesShape(s);
		Rectangle2D b = shape.getBounds2D();
		if (!visible || b.isEmpty()) {
			return "none";
		}
		return shape.getClass().getSimpleName() + "(" + b.getX() + "," + b.getY() + "," + b.getWidth() + ","
				+ b.getHeight() + ")" + (filled ? " filled" : " outline");
	}

	/** A curve's key: its manufacturer's display name, its designation and its digest. */
	static String key(ThrustCurveMotor motor) {
		return motor.getManufacturer().getDisplayName() + " | " + motor.getDesignation() + " | " + motor.getDigest();
	}

	static String width(Stroke stroke) {
		if (stroke instanceof BasicStroke basic) {
			if (basic.getDashArray() != null) {
				throw new IllegalStateException("a dashed curve");
			}
			return Double.toHexString(basic.getLineWidth());
		}
		throw new IllegalStateException("stroke " + stroke);
	}

	static String range(XYPlot plot) {
		ValueAxis x = plot.getDomainAxis();
		ValueAxis y = plot.getRangeAxis();
		return Double.toHexString(x.getLowerBound()) + " | " + Double.toHexString(x.getUpperBound()) + " | "
				+ Double.toHexString(y.getLowerBound()) + " | " + Double.toHexString(y.getUpperBound());
	}

	/** A new MotorInformationPanel showing {@code motors} with curve {@code selected} chosen: its chart. */
	static JFreeChart preview(List<ThrustCurveMotor> motors, int selected) throws Exception {
		Object panel = newPanel();
		Method update = panel.getClass().getDeclaredMethod("updateData", List.class, ThrustCurveMotor.class);
		update.setAccessible(true);
		update.invoke(panel, motors, motors.get(selected));
		return chartOf(panel);
	}

	static Object newPanel() throws Exception {
		Class<?> c = Class.forName("info.openrocket.swing.gui.dialogs.motor.thrustcurve.MotorInformationPanel");
		Constructor<?> k = c.getDeclaredConstructor();
		k.setAccessible(true);
		return k.newInstance();
	}

	static JFreeChart chartOf(Object panel) throws Exception {
		Field f = panel.getClass().getDeclaredField("chart");
		f.setAccessible(true);
		return (JFreeChart) f.get(panel);
	}

	static ChartPanel chartIn(JDialog dialog) {
		for (Component c : all(dialog.getContentPane())) {
			if (c instanceof ChartPanel panel) {
				return panel;
			}
		}
		throw new IllegalStateException("no ChartPanel");
	}

	static List<Component> all(Container container) {
		List<Component> out = new ArrayList<>();
		for (Component child : container.getComponents()) {
			out.add(child);
			if (child instanceof Container inner) {
				out.addAll(all(inner));
			}
		}
		return out;
	}

	static String rgb(Paint paint) {
		if (paint instanceof Color c) {
			return String.format("#%02x%02x%02x%s", c.getRed(), c.getGreen(), c.getBlue(),
					c.getAlpha() == 255 ? "" : String.format("@%d", c.getAlpha()));
		}
		throw new IllegalStateException("paint " + paint);
	}

	static void bootstrap() throws Exception {
		// Before anything makes a UUID (the pins' rule: every probe is seeded; this one builds no design).
		DeterministicUuids.install();
		DeterministicUuids.reseed("thrust-curves");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(swingPrefs),
				new PluginModule()));
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
