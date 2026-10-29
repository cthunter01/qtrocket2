package info.qtrocket.guireference;

import java.awt.HeadlessException;
import java.awt.event.ActionEvent;
import java.awt.geom.Point2D;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.swing.Action;
import javax.swing.DefaultListSelectionModel;
import javax.swing.tree.DefaultTreeSelectionModel;
import javax.swing.tree.TreePath;
import javax.swing.tree.TreeSelectionModel;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.document.StorageOptions.FileType;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.file.GeneralRocketSaver;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.choosers.StorageOptionChooser;
import info.openrocket.swing.gui.configdialog.ComponentConfigDialog;
import info.openrocket.swing.gui.components.BasicTree;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;
import info.openrocket.swing.gui.main.ComponentAddButtons;
import info.openrocket.swing.gui.main.ComponentButton;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.UndoRedoAction;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import info.openrocket.swing.gui.util.FileHelper;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * One design window of OpenRocket, made of the parts of BasicFrame that run without a display, and
 * the steps of the scenario done on it.
 * <p>
 * What runs is OpenRocket's own code wherever it runs headless: the component tree (a BasicTree with
 * a ComponentTreeModel, as ComponentTree is, without its renderer and drag and drop) and its
 * selection model, the DocumentSelectionModel, the add buttons (ComponentAddButtons and its
 * ComponentButtons, clicked with doClick()) and the UndoRedoActions, wired as BasicFrame and
 * DesignPanel wire them. What cannot run headless is replaced by the core calls it makes, line for
 * line but for the calls that change only the window, which each place lists (see README.md, "What
 * runs and what is replaced"):
 * <ul>
 * <li>the configuration dialog, a JDialog: an add button stops with a HeadlessException where
 * ComponentConfigDialog.showDialog makes it, so the tool does what showDialog does after that (the
 * undo position "Modify ..." of an existing component), and a field of the dialog is set through the
 * model the dialog builds for it (DialogFields);</li>
 * <li>Edit, Copy and Paste of RocketActions, whose action classes read the menu shortcut key of the
 * toolkit, which a headless one refuses: their code, copied (EditActions);</li>
 * <li>the fin point table, the "Convert to freeform" button and Save as (BasicFrame.saveAsAction with its
 * design information dialog and file chooser): their action code, copied;</li>
 * <li>the parts library OpenRocket opens for a new component with presets
 * (ComponentConfigDialog.showDialog, AutoOpenPartsLibrary): never, as if closed without a choice.</li>
 * </ul>
 * A session of a version 2 scenario (Session.open: File &gt; Open, on the event dispatch thread of a JVM
 * with a display) has the same design window, without the add buttons' steps, Copy and Paste, and with
 * OpenRocket's real Motors &amp; Configuration tab (FlightConfigurationTab), the window's tab, QtRocket's
 * fix of bug 6 in the undo history (DefaultIgnitions) and the rocket's listeners in the order they were
 * registered (orderListeners).
 */
final class Session {

	private static final Translator trans = Application.getTranslator();

	/**
	 * An open dialog: its panel class, its component, the Automatic check boxes its panel set when it was
	 * made, and the controls the scenario used.
	 */
	private static final class Dialog {
		final String panel;
		final RocketComponent component;
		final DialogFields.AutomaticBoxes automaticBoxes;
		final Map<String, DialogFields.Control> controls = new LinkedHashMap<>();

		Dialog(String panel, RocketComponent component) {
			this.panel = panel;
			this.component = component;
			automaticBoxes = new DialogFields.AutomaticBoxes(panel, component);
		}
	}

	private final OpenRocketDocument document;
	private final Rocket rocket;
	private final DefaultTreeSelectionModel componentSelectionModel;
	private final DocumentSelectionModel selectionModel;
	private final EditActions actions;
	private final BasicTree tree;
	private final ComponentButton[][] buttons;
	private final Action undoAction;
	private final Action redoAction;
	private final File saveDirectory;

	private Dialog dialog = null;
	private boolean fileChooserOpen = false;
	private File savedFile = null;
	/** Whether the rocket fired a tree or undo change since the flag was last cleared. */
	private boolean treeOrUndoChange = false;

	/** The Motors &amp; Configuration tab of a version 2 session (null in version 1). */
	private final FlightConfigurationTab tab;
	/** QtRocket's fix of bug 6 in the undo history of a version 2 session (null in version 1). */
	private final DefaultIgnitions defaultIgnitions;
	/** The window's tab in a version 2 session: "design" or "flightConfiguration". */
	private String windowTab = "design";

	/**
	 * File &gt; New (BasicFrame.newAction): a new document and its window, with the parts of the
	 * BasicFrame constructor (:216-372) and of DesignPanel (:217) that run headless (version 1).
	 */
	Session(File saveDirectory) throws Exception {
		this(saveDirectory, OpenRocketDocumentFactory.createNewRocket(), false);
	}

	/**
	 * File &gt; Open of @p file (version 2, on the event dispatch thread of a JVM with a display):
	 * BasicFrame.open(file, parent) (:1586-1673) with OpenFileWorker.doInBackground (the load, then
	 * setFile and setSaved(true)); the warnings dialog it shows for a load with warnings is a failure of
	 * the tool. Left out: MRUDesignFile.addFile (the list of recent files) and closeIfReplaceable() (the
	 * window it replaces). Then the window, as for File &gt; New, with the Motors &amp; Configuration tab.
	 */
	static Session open(File saveDirectory, File file) throws Exception {
		GeneralRocketLoader loader = new GeneralRocketLoader(file);
		OpenRocketDocument document;
		try (InputStream is = new BufferedInputStream(new FileInputStream(file))) {
			String fileName = file.getName().replaceFirst("[.][^.]+$", "");
			document = loader.load(is, fileName);
		}
		document.setFile(file);
		document.setSaved(true);
		if (!loader.getWarnings().isEmpty()) {
			throw new IllegalStateException("the load of " + file + " has warnings, which File > Open shows: "
					+ loader.getWarnings());
		}
		return new Session(saveDirectory, document, true);
	}

	/**
	 * The window of @p openedDocument: the parts of the BasicFrame constructor (:216-372) and of
	 * DesignPanel (:217) that run headless, and in version 2 (@p withTab) the Motors &amp; Configuration
	 * tab after them (:251).
	 */
	private Session(File saveDirectory, OpenRocketDocument openedDocument, boolean withTab) throws Exception {
		this.saveDirectory = saveDirectory;
		document = openedDocument;
		rocket = document.getRocket();
		if (withTab) {
			orderListeners(rocket);
		}

		componentSelectionModel = new DefaultTreeSelectionModel();
		componentSelectionModel.setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
		selectionModel = new DocumentSelectionModel(document);
		selectionModel.attachComponentTreeSelectionModel(componentSelectionModel);
		// The selection model of the simulation table (SimulationPanel's JTable); the document has no
		// simulation, and SimulationPanel cannot be made headless (its actions read the menu shortcut key).
		selectionModel.attachSimulationListSelectionModel(new DefaultListSelectionModel());
		actions = new EditActions(document, selectionModel);
		// ComponentTree (componenttree/ComponentTree.java:17-46) is a BasicTree with a ComponentTreeModel,
		// expanded; its renderer, drag and drop and tool tips are left out (setDragEnabled refuses to run
		// headless).
		tree = new BasicTree();
		tree.setModel(new ComponentTreeModel(rocket, tree));
		tree.expandTree();
		tree.setSelectionModel(componentSelectionModel);

		ComponentAddButtons addButtons = new ComponentAddButtons(document, tree.getSelectionModel(), null);
		Field field = ComponentAddButtons.class.getDeclaredField("buttons");
		field.setAccessible(true);
		buttons = (ComponentButton[][]) field.get(addButtons);

		// BasicFrame.java:251: the Motors & Configuration tab, after the design panel.
		tab = withTab ? new FlightConfigurationTab(document, selectionModel) : null;
		defaultIgnitions = withTab ? new DefaultIgnitions(document) : null;

		// BasicFrame.java:363-372: the sustainer is selected when the window opens.
		componentSelectionModel.setSelectionPath(new TreePath(rocket.getChild(0)));
		tree.setSelectionRow(1);

		// BasicFrame.java:717 and :727: Edit > Undo and Edit > Redo.
		undoAction = UndoRedoAction.newUndoAction(document);
		redoAction = UndoRedoAction.newRedoAction(document);

		rocket.addComponentChangeListener(e -> {
			if (e.isTreeChange() || e.isUndoChange()) {
				treeOrUndoChange = true;
			}
		});
	}

	/**
	 * Version 2: the rocket's listeners notified in the order they were registered. OpenRocket's Rocket keeps
	 * them in a HashSet (Rocket.java:46, notifyAllListeners :765-779), most of them hashed by identity, so that
	 * the order in which the panels and the table models of the Motors &amp; Configuration tab see an event
	 * changes from one run to the next, and with it what the tables keep selected: a table model's
	 * fireTableStructureChanged() clears its table's selection, and FlightConfigurationPanel.stateChanged()
	 * selects the rocket's configuration again, so the table ends with a selection only when the panel comes
	 * second. The tool replaces the set with a LinkedHashSet (one of the orders OpenRocket may take) before any
	 * listener of the window is registered, and the tab registers its FlightConfigurationPanel again after its
	 * table models (FlightConfigurationTab), the order QtRocket's panel connects in. The document's own
	 * listener, the only one the load registers, stays first.
	 */
	private static void orderListeners(Rocket rocket) {
		Set<?> listeners = (Set<?>) Reflection.get(Rocket.class, rocket, "listenerList");
		if (listeners.size() > 1) {
			throw new IllegalStateException("the loaded rocket has " + listeners.size()
					+ " listeners, whose order is unknown");
		}
		Reflection.set(Rocket.class, rocket, "listenerList", new LinkedHashSet<>(listeners));
	}

	/** Closes the window of a version 2 session (its tab's frame and dialogs). */
	void dispose() {
		if (tab != null) {
			tab.dispose();
		}
	}

	/** The Motors &amp; Configuration tab of a version 2 session, null in version 1. */
	FlightConfigurationTab tab() {
		return tab;
	}

	/** The window's tab of a version 2 session: "design" or "flightConfiguration". */
	String windowTab() {
		return windowTab;
	}

	OpenRocketDocument document() {
		return document;
	}

	File savedFile() {
		return savedFile;
	}

	/** What a step leaves open: "dialog <panel>" with the dialog's component, the file chooser, or nothing. */
	String openWindow() {
		if (dialog != null) {
			return dialog.panel;
		}
		return fileChooserOpen ? "file chooser" : null;
	}

	RocketComponent dialogComponent() {
		return dialog == null ? null : dialog.component;
	}

	List<RocketComponent> selection() {
		return new ArrayList<>(selectionModel.getSelectedComponents());
	}

	Action undoAction() {
		return undoAction;
	}

	Action redoAction() {
		return redoAction;
	}

	// ------------------------------------------------------------------ the steps

	void run(Scenario.Step step) throws Exception {
		if (tab == null) {
			runStep(step);
			return;
		}
		// Version 2 (on the event dispatch thread): QtRocket's fix of bug 6 around every step.
		defaultIgnitions.beginStep();
		tab.beginStep();
		runStep(step);
		if (step.kind() == Scenario.Kind.UNDO || step.kind() == Scenario.Kind.REDO) {
			defaultIgnitions.restoreAfterUndoOrRedo();
		}
		defaultIgnitions.endStep();
	}

	private void runStep(Scenario.Step step) throws Exception {
		Scenario.Kind kind = step.kind();
		boolean inDialog = switch (kind) {
			case TYPE, CHOOSE, CHECK, EDIT_POINT, INSERT_POINT, BUTTON, OK -> true;
			default -> false;
		};
		if (inDialog && dialog == null) {
			throw new IllegalStateException("no dialog is open");
		}
		if (!inDialog && dialog != null) {
			throw new IllegalStateException("the dialog " + dialog.panel + " is still open");
		}
		if (fileChooserOpen != (kind == Scenario.Kind.CHOOSE_FILE)) {
			throw new IllegalStateException(fileChooserOpen ? "the file chooser is open" : "no file chooser is open");
		}
		if (tab != null) {
			requireWindowTab(kind);
		}
		treeOrUndoChange = false;
		if (Scenario.TAB_KINDS.contains(kind)) {
			tab.run(step);
			return;
		}
		switch (kind) {
			case NEW -> throw new IllegalStateException("File > New is the first step only");
			case OPEN -> throw new IllegalStateException("File > Open is the first step only");
			case TAB -> selectWindowTab(step.tab());
			case SELECT -> select(step.paths());
			case ADD -> add(step.component());
			case EDIT -> edit();
			case TYPE -> control(step.field()).type(step.text(), step.unit());
			case CHOOSE -> control(step.field()).choose(step.item());
			case CHECK -> control(step.field()).check(step.checked());
			case EDIT_POINT -> editPoint(step.row(), step.column(), step.text(), step.unit());
			case INSERT_POINT -> insertPoint(step.row());
			case BUTTON -> button(step.button());
			case OK -> ok();
			case UNDO -> perform(undoAction, "Undo");
			case REDO -> perform(redoAction, "Redo");
			case COPY -> {
				if (!actions.isCopyEnabled()) {
					throw new IllegalStateException("Copy is disabled");
				}
				actions.copy();
			}
			case PASTE -> {
				if (!actions.isPasteEnabled()) {
					throw new IllegalStateException("Paste is disabled");
				}
				actions.paste();
			}
			case SAVE_AS -> saveAs();
			case CHOOSE_FILE -> chooseFile(step.file());
			default -> throw new IllegalStateException("no step of the design: " + kind.name);
		}
		// ComponentConfigDialog.componentChanged closes the dialog on a tree or undo change: a step in a
		// dialog that makes one would leave no dialog in the GUI.
		if (inDialog && kind != Scenario.Kind.BUTTON && treeOrUndoChange) {
			throw new IllegalStateException("the step changed the tree, which closes the dialog");
		}
	}

	/**
	 * Version 2: the steps of the Motors &amp; Configuration tab need that tab, a click on the component tree
	 * the design tab; the menus (Edit, Undo, Redo, Save as) and an open dialog work on either.
	 */
	private void requireWindowTab(Scenario.Kind kind) {
		if (Scenario.TAB_KINDS.contains(kind) && !windowTab.equals("flightConfiguration")) {
			throw new IllegalStateException("the step " + kind.name + " needs the tab flightConfiguration");
		}
		if (kind == Scenario.Kind.SELECT && !windowTab.equals("design")) {
			throw new IllegalStateException("the component tree is on the design tab");
		}
	}

	/**
	 * A click on the window's tab @p name: BasicFrame.stateChanged (:2518-2537), which gives the spotlight to
	 * the tab's panel: DesignPanel.takeTheSpotlight() (the focus to the component tree) or
	 * FlightConfigurationPanel.takeTheSpotlight() (the focus to the current sub-tab's table, and the design
	 * selection becomes its selected columns' components).
	 */
	private void selectWindowTab(String name) {
		if (windowTab.equals(name)) {
			throw new IllegalStateException("the tab " + name + " is the current one already");
		}
		windowTab = name;
		if (name.equals("flightConfiguration")) {
			tab.takeTheSpotlight();
		} else {
			tree.requestFocusInWindow();
		}
	}

	/**
	 * Version 2: StorageOptionChooser.verifyStorageOptions (:136-181) asks how to store the simulated data
	 * when a simulation holds some and the document's storage options are not set explicitly: a question the
	 * scenarios do not answer (QtRocket's exit compares saves without stored flights). The tool fails before
	 * it would be asked, with the same rule.
	 */
	private void requireNoStorageQuestion() {
		if (document.getDefaultStorageOptions().isExplicitlySet()) {
			return;
		}
		for (Simulation s : document.getSimulations()) {
			if (s.getStatus() == Simulation.Status.NOT_SIMULATED || s.getStatus() == Simulation.Status.EXTERNAL) {
				continue;
			}
			FlightData data = s.getSimulatedData();
			if (data == null) {
				continue;
			}
			for (int i = 0; i < data.getBranchCount(); i++) {
				FlightDataBranch branch = data.getBranch(i);
				if (branch != null && branch.getLength() > 0) {
					throw new IllegalStateException("Save as would ask how to store the data of the simulation "
							+ s.getName());
				}
			}
		}
	}

	/** Clicks on the rows of the components (with Ctrl for more than one). */
	private void select(List<List<Scenario.PathElement>> paths) {
		TreePath[] treePaths = new TreePath[paths.size()];
		for (int i = 0; i < paths.size(); i++) {
			treePaths[i] = ComponentTreeModel.makeTreePath(ComponentPaths.resolve(rocket, paths.get(i)));
		}
		tree.setSelectionPaths(treePaths);
	}

	/** Clicks the add button of the class named @p className (ComponentButton.fireActionPerformed). */
	private void add(String className) throws Exception {
		Class<?> componentClass = Class.forName("info.openrocket.core.rocketcomponent." + className);
		Field classField = ComponentButton.class.getDeclaredField("componentClass");
		classField.setAccessible(true);
		ComponentButton button = null;
		for (ComponentButton[] row : buttons) {
			for (ComponentButton b : row == null ? new ComponentButton[0] : row) {
				if (classField.get(b) == componentClass) {
					button = b;
				}
			}
		}
		if (button == null) {
			throw new IllegalArgumentException("no add button for " + className);
		}
		if (!button.isEnabled()) {
			throw new IllegalStateException("the add button of " + className + " is disabled");
		}
		List<RocketComponent> before = new ArrayList<>();
		rocket.forEach(before::add);
		try {
			button.doClick();
			throw new IllegalStateException("the add button opened no configuration dialog");
		} catch (HeadlessException e) {
			// ComponentConfigDialog.showDialog(parent, document, component, false, true): the dialog.
		}
		RocketComponent added = selectionModel.getSelectedComponent();
		if (added == null || before.contains(added) || added.getClass() != componentClass) {
			throw new IllegalStateException("the add button of " + className + " did not select a new component");
		}
		open(added, true);
	}

	/**
	 * Edit &gt; Edit (RocketActions.EditAction), which a double click on the selected row does too
	 * (DesignPanel.java:107-128): the dialog of the first selected component.
	 */
	private void edit() throws Exception {
		if (!actions.isEditEnabled()) {
			throw new IllegalStateException("Edit is disabled");
		}
		// ComponentConfigDialog.showDialog(parentFrame, document, component).
		open(actions.edit(), false);
	}

	/**
	 * The configuration dialog of @p component opens: what ComponentConfigDialog.showDialog
	 * (ComponentConfigDialog.java:246-292) does after it made the dialog (:269), which a headless JVM cannot
	 * make; of the making, what the panel does to its Automatic check boxes (DialogFields.AutomaticBoxes).
	 * The parts library it may open for a new component (:285-291) is closed without a choice.
	 */
	private void open(RocketComponent component, boolean isNewComponent) throws Exception {
		dialog = new Dialog(configPanelOf(component), component);
		if (!isNewComponent) {
			if (component.getConfigListeners().isEmpty()) {
				document.addUndoPosition(trans.get("ComponentCfgDlg.Modify") + " " + component.getComponentName());
			} else {
				document.addUndoPosition(trans.get("ComponentCfgDlg.ModifyComponents"));
			}
		}
	}

	/** The simple name of the panel class ComponentConfigDialog picks for @p component. */
	@SuppressWarnings("unchecked")
	static String configPanelOf(RocketComponent component) throws Exception {
		Method find = ComponentConfigDialog.class.getDeclaredMethod("findDialogContentsConstructor",
				RocketComponent.class);
		find.setAccessible(true);
		Constructor<? extends RocketComponentConfig> constructor =
				(Constructor<? extends RocketComponentConfig>) find.invoke(null, component);
		return constructor.getDeclaringClass().getSimpleName();
	}

	/**
	 * The control of @p field in the open dialog, made when a step first uses it. "<field>Automatic" is
	 * the "Automatic" check box of the spinner of <field>, which shares its model.
	 */
	private DialogFields.Control control(String field) {
		DialogFields.Control control = dialog.controls.get(field);
		if (control == null) {
			String suffix = "Automatic";
			if (field.endsWith(suffix) && field.length() > suffix.length()) {
				String property = field.substring(0, field.length() - suffix.length());
				DoubleModel model = control(property).doubleModel();
				if (model == null) {
					throw new IllegalArgumentException("the field " + field + " is no spinner's Automatic check box");
				}
				control = DialogFields.automatic(model, property, dialog.automaticBoxes);
			} else {
				control = DialogFields.make(dialog.panel, field, document, dialog.component, dialog.automaticBoxes);
			}
			dialog.controls.put(field, control);
		}
		return control;
	}

	/**
	 * Closes the dialog with OK (RocketComponentConfig.java:389-392, disposeDialog()), as the dialog's
	 * windowClosed handler does (ComponentConfigDialog.java:73-81): the models are invalidated and the
	 * component's config listeners cleared. The design information dialog of Save as hands on to the
	 * file chooser.
	 */
	private void ok() {
		close();
		if (fileChooserPending) {
			fileChooserPending = false;
			fileChooserOpen = true;
		}
	}

	private boolean fileChooserPending = false;

	private void close() {
		// The last made first: an Automatic check box listens to the model of its spinner.
		List<DialogFields.Control> controls = new ArrayList<>(dialog.controls.values());
		Collections.reverse(controls);
		for (DialogFields.Control control : controls) {
			control.invalidateMe();
		}
		// The design information dialog is a plain JDialog, without ComponentConfigDialog's handler.
		if (!dialog.panel.equals("SaveDesignInfoPanel") && ComponentConfigDialog.clearConfigListeners) {
			dialog.component.clearConfigListeners();
		}
		dialog = null;
	}

	/**
	 * The fin point table of FreeformFinSetConfig: its cell editor's commit (FinPointTableModel.setValueAt,
	 * :1000-1036). Left out: beginFinPointEdit() and commitFinPointEdit() around setPoint (:1024-1029),
	 * which keep the fin editor's own edit history (its FinPointFigure's, apart from the document's undo),
	 * and updateFields() (:1031), which refreshes the dialog's fields and figure.
	 */
	private void editPoint(int row, String column, String text, String unit) throws Exception {
		FreeformFinSet finset = finPointTable();
		// The table shows the points in the default length unit (Columns, :934-956); a cell takes text in it.
		String shown = UnitGroup.UNITS_LENGTH.getDefaultUnit().getUnit();
		if (unit != null && !unit.equals(shown)) {
			throw new IllegalArgumentException("the fin point table shows " + shown + ", not " + unit);
		}
		int columnIndex = switch (column) {
			case "X" -> 0;
			case "Y" -> 1;
			default -> throw new IllegalArgumentException("the fin point table has no column " + column);
		};
		// FinPointTableModel.isCellEditable (:993-999): the first and last points move along the root only.
		int rows = finset.getPointCount();
		if ((row == 0 || row == rows - 1) && columnIndex != 0 || row < 0 || row >= rows) {
			throw new IllegalArgumentException("the cell of row " + row + " and column " + column + " cannot be edited");
		}
		double value = UnitGroup.UNITS_LENGTH.fromString(text);
		CoordinateIF c = finset.getFinPoints()[row];
		if (columnIndex == 0) {
			c = c.setX(value);
		} else {
			c = c.setY(value);
		}
		finset.setPoint(row, c.getX(), c.getY());
	}

	/**
	 * Row @p row selected, "Insert point" of the table's context menu (InsertPointAction, insertPoint()
	 * :592-607). Left out: beginFinPointEdit() and commitFinPointEdit() around addPoint (:601-606), the fin
	 * editor's own edit history.
	 */
	private void insertPoint(int row) throws Exception {
		FreeformFinSet finset = finPointTable();
		// InsertPointAction.updateEnabledState (:1151-1154).
		if (row < 0 || row >= finset.getPointCount() - 1) {
			throw new IllegalArgumentException("Insert point is disabled for row " + row);
		}
		CoordinateIF currentPoint = finset.getFinPoints()[row];
		CoordinateIF nextPoint = finset.getFinPoints()[row + 1];
		Point2D.Double toAdd = new Point2D.Double((currentPoint.getX() + nextPoint.getX()) / 2,
				(currentPoint.getY() + nextPoint.getY()) / 2);
		finset.addPoint(row + 1, toAdd);
	}

	private FreeformFinSet finPointTable() {
		if (!dialog.panel.equals("FreeformFinSetConfig")) {
			throw new IllegalStateException("the dialog " + dialog.panel + " has no fin point table");
		}
		return (FreeformFinSet) dialog.component;
	}

	/** A button of the dialog. */
	private void button(String name) throws Exception {
		if (!name.equals("convertToFreeform")) {
			throw new IllegalArgumentException("the tool knows no dialog button " + name);
		}
		// FinSetConfig.addFinSetButtons (:66-94): the button exists for a fin set that is no freeform one.
		if (!(dialog.component instanceof FinSet finset) || dialog.component instanceof FreeformFinSet) {
			throw new IllegalStateException("the dialog " + dialog.panel + " has no button Convert to freeform");
		}
		// The click queues the conversion and disposes the dialog; the conversion then runs before the
		// dialog's windowClosed handler (both on the event thread, in that order).
		document.addUndoPosition(trans.get("FinSetConfig.Convertfinset"));
		RocketComponent freeform = FreeformFinSet.convertFinSet(finset);
		close();
		// ComponentConfigDialog.showDialog(freeform): the dialog of the new fin set, as of an existing component.
		open(freeform, false);
	}

	private static void perform(Action action, String name) {
		if (!action.isEnabled()) {
			throw new IllegalStateException(name + " is disabled");
		}
		action.actionPerformed(new ActionEvent(Session.class, ActionEvent.ACTION_PERFORMED, name));
	}

	/**
	 * File &gt; Save as (BasicFrame.saveAsAction, :2167-2185): the design information dialog
	 * (showSaveRocketInfoDialog, :2189-2206) when the preference asks for it, with the rocket selected,
	 * and then the file chooser.
	 */
	private void saveAs() {
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		if (prefs.isShowSaveRocketInfo()) {
			selectionModel.setSelectedComponent(rocket);
			dialog = new Dialog("SaveDesignInfoPanel", rocket);
			fileChooserPending = true;
		} else {
			fileChooserOpen = true;
		}
	}

	/**
	 * The file chooser of Save as: @p name typed, Save clicked (openFileSaveAsDialog), then
	 * saveAsAction (:2174-2179) and saveAsOpenRocket (:2216-2250) without the preview image. Left out, as
	 * they change only the application and the window: MRUDesignFile.addFile (:2180-2181, the list of
	 * recent files), setTitle() (:2250, the window's title) and restoreFocus() (:2185).
	 */
	private void chooseFile(String name) throws Exception {
		fileChooserOpen = false;
		File file = FileHelper.forceExtension(new File(saveDirectory, name), "ork");
		// FileHelper.confirmWrite asks before it overwrites a file.
		if (file.exists()) {
			throw new IllegalStateException(file + " exists: Save as would ask whether to overwrite it");
		}
		file = FileHelper.forceExtension(file, "ork");
		if (tab != null) {
			requireNoStorageQuestion();
		}
		if (!StorageOptionChooser.verifyStorageOptions(document, null)) {
			throw new IllegalStateException("the storage options were not confirmed");
		}
		// generatePreviewImage() draws the rocket panel's figure; QtRocket saves no preview before
		// Milestone 5 (decision O26), and neither does the reference.
		document.getDefaultStorageOptions().clearPreviewImage();
		document.getDefaultStorageOptions().setFileType(FileType.OPENROCKET);
		// SaveFileWorker.doInBackground: saver.save(file, document, progressCallback).
		new GeneralRocketSaver().save(file, document, (GeneralRocketSaver.SavingProgress) null);
		document.setFile(file);
		document.setSaved(true);
		savedFile = file;
	}
}
