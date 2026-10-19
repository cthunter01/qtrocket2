package info.qtrocket.guireference;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.Pair;
import info.openrocket.swing.gui.configdialog.ComponentConfigDialog;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.OpenRocketClipboard;
import info.openrocket.swing.gui.main.RocketActions;

/**
 * Edit, Copy and Paste of OpenRocket's RocketActions (swing/.../gui/main/RocketActions.java), whose
 * action classes cannot be made in a headless JVM: their constructors read the menu shortcut key of
 * the AWT toolkit (RocketActions.KeyStrokes), which the headless toolkit refuses with a
 * HeadlessException. What their actionPerformed does for components is copied here line for line,
 * with the enabling rules of their clipboardChanged(); the clipboard is OpenRocket's own
 * OpenRocketClipboard, and the copies are made by RocketActions.copyComponentsMaintainParent. Left
 * out: the branches for selected simulations (the document has none) and the last call of Copy and
 * Paste, parentFrame.selectTab(BasicFrame.DESIGN_TAB), which only shows the design tab.
 */
final class EditActions {

	private final OpenRocketDocument document;
	private final DocumentSelectionModel selectionModel;

	EditActions(OpenRocketDocument document, DocumentSelectionModel selectionModel) {
		this.document = document;
		this.selectionModel = selectionModel;
	}

	// ------------------------------------------------------------------ EditAction (:581-633)

	/** EditAction.clipboardChanged(). */
	boolean isEditEnabled() {
		return !selectionModel.getSelectedComponents().isEmpty();
	}

	/**
	 * EditAction.editComponents() up to ComponentConfigDialog.showDialog(parentFrame, document,
	 * component), which the caller does: returns the component whose dialog opens.
	 */
	RocketComponent edit() {
		List<RocketComponent> components = selectionModel.getSelectedComponents();

		if (components == null || components.isEmpty()) {
			return null;
		}

		if (ComponentConfigDialog.isDialogVisible())
			ComponentConfigDialog.disposeDialog();

		RocketComponent component = components.get(0);
		component.clearConfigListeners();
		if (components.size() > 1) {
			for (int i = 1; i < components.size(); i++) {
				RocketComponent listener = components.get(i);
				listener.clearConfigListeners(); // Make sure all the listeners are cleared (should not be possible, but just in case)
				component.addConfigListener(listener);
			}
		}
		return component;
	}

	// ------------------------------------------------------------------ CopyAction (:707-756)

	/** CopyAction.clipboardChanged(). */
	boolean isCopyEnabled() {
		return isCopyable(selectionModel.getSelectedComponent());
	}

	/** CopyAction.copyComponents(). */
	void copy() {
		List<RocketComponent> components = selectionModel.getSelectedComponents();
		if (!components.isEmpty()) {
			components = new ArrayList<>(components);
			fillInMissingSelections(components);

			components.sort(Comparator.comparing(c -> c.getParent() != null ? -c.getParent().getChildPosition(c) : 0));
		}

		if (!isCopyable(components)) {
			return;
		}

		List<RocketComponent> copiedComponents = new LinkedList<>(
				RocketActions.copyComponentsMaintainParent(components));
		copiedComponents.sort(Comparator.comparing(c -> c.getParent() != null ? -c.getParent().getChildPosition(c) : 0));

		OpenRocketClipboard.setClipboard(copiedComponents);
	}

	// ------------------------------------------------------------------ PasteAction (:765-835)

	/** PasteAction.clipboardChanged(). */
	boolean isPasteEnabled() {
		return !getPastePositions(OpenRocketClipboard.getClipboardComponents()).isEmpty();
	}

	/** PasteAction.pasteComponents(). */
	void paste() {
		List<RocketComponent> components = new LinkedList<>(OpenRocketClipboard.getClipboardComponents());

		if (components.isEmpty()) {
			return;
		}
		ComponentConfigDialog.disposeDialog();

		List<RocketComponent> pasted = new LinkedList<>();
		for (RocketComponent component : components) {
			pasted.add(component.copy());
		}

		List<Pair<RocketComponent, Integer>> positions = getPastePositions(pasted);

		if (pasted.size() == 1) {
			document.addUndoPosition("Paste " + pasted.get(0).getComponentName());
		} else {
			document.addUndoPosition("Paste components");
		}

		List<RocketComponent> successfullyPasted = new LinkedList<>();
		for (int i = 0; i < pasted.size(); i++) {
			if (positions.get(i) == null) {
				// JOptionPane.showMessageDialog(RocketActions.PasteAct.invalidPosition.msg): a modal message.
				throw new IllegalStateException("Paste would report an invalid position for "
						+ pasted.get(i).getComponentName());
			} else {
				RocketComponent parent = positions.get(i).getU();
				RocketComponent child = pasted.get(i);
				if (parent != null && parent.isCompatible(child)) {
					parent.addChild(child, positions.get(i).getV());
					successfullyPasted.add(pasted.get(i));
				} else {
					throw new IllegalStateException("Pasted component " + child + " is not compatible with " + parent);
				}
			}
		}

		selectionModel.setSelectedComponents(successfullyPasted);
	}

	// ------------------------------------------------------------------ the helpers (:328-565)

	private boolean isCopyable(RocketComponent c) {
		if (c == null)
			return false;
		if (c instanceof Rocket)
			return false;
		return true;
	}

	private boolean isCopyable(List<RocketComponent> components) {
		if (components == null || components.isEmpty())
			return false;
		for (RocketComponent component : components) {
			if (!isCopyable(component))
				return false;
		}
		return true;
	}

	private void selectAllUnselectedInParent(List<RocketComponent> selections, RocketComponent parent) {
		if (parent.getChildCount() == 0)
			return;

		boolean noChildrenSelected = true;
		for (RocketComponent child : parent.getChildren()) {
			if (selections.contains(child)) {
				noChildrenSelected = false;
				break;
			}
		}

		// Add children to selection if none of them were selected
		if (noChildrenSelected) {
			selections.addAll(parent.getChildren());
		}

		// Recursively select all unselected children. Unselected children will not undergo this recursive updating.
		for (RocketComponent child : parent.getChildren()) {
			if (!noChildrenSelected && selections.contains(child)) {
				selectAllUnselectedInParent(selections, child);
			}
		}
	}

	private void fillInMissingSelections(List<RocketComponent> selections) {
		List<RocketComponent> initSelections = new LinkedList<>(selections);
		for (RocketComponent component : initSelections) {
			selectAllUnselectedInParent(selections, component);

			// If there is a component in the selection, but its parent (or the parent of the parent) is still
			// not selected, add it to the selection
			RocketComponent temp = component;
			if (RocketComponent.listContainsParent(selections, temp) && !selections.contains(temp.getParent())) {
				while (!selections.contains(temp.getParent())) {
					selections.add(temp.getParent());
					temp = temp.getParent();
				}
			}
		}
	}

	private Pair<RocketComponent, Integer> getPastePosition(RocketComponent srcComponent,
			RocketComponent destComponent) {
		if (destComponent == null)
			return new Pair<>(null, null);

		if (srcComponent == null)
			return new Pair<>(null, null);

		if (destComponent.isCompatible(srcComponent))
			return new Pair<>(destComponent, destComponent.getChildCount());

		RocketComponent parent = destComponent.getParent();
		return getPastePositionFromParent(srcComponent, destComponent, parent);
	}

	private Pair<RocketComponent, Integer> getPastePositionFromParent(RocketComponent srcComponent,
			RocketComponent destComponent, RocketComponent parent) {
		if (parent != null && parent.isCompatible(srcComponent)) {
			int index = parent.getChildPosition(destComponent) + 1;
			return new Pair<>(parent, index);
		}

		return new Pair<>(null, null);
	}

	private List<Pair<RocketComponent, Integer>> getPastePositions(List<RocketComponent> clipboard) {
		List<Pair<RocketComponent, Integer>> result = new LinkedList<>();
		RocketComponent selected = selectionModel.getSelectedComponent();
		for (RocketComponent component : clipboard) {
			Pair<RocketComponent, Integer> position = getPastePosition(component, selected);
			if (position != null) {
				result.add(position);
			}
		}
		return result;
	}
}
