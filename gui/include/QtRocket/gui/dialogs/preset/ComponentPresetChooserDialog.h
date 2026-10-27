#pragma once

#include <string>

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/util/UserPrompts.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QWidget;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentPresetTable;
class GuiServices;
class StyledLabel;
class WindowGeometry;

/// The parts library (OpenRocket's ComponentPresetChooserDialog, swing
/// gui/dialogs/preset/ComponentPresetChooserDialog.java): the presets of a component's type in a
/// ComponentPresetTable, their filters, and the choice of one, which the caller applies
/// (UserPrompts::choosePreset(), answered by choose()).
///
/// - Modal for the whole application (Dialog.ModalityType.APPLICATION_MODAL), titled "Choose
///   component preset". The presets are those of GuiServices::presets() of the component's
///   preset type (presetTypeOf() of UserPrompts::PresetRequest::componentKind), in the
///   database's order; the favourites, the stored table layout and the dialog's own preferences
///   are GuiServices::preferences()'.
/// - The layout (MigLayout "fill, ins para"): in the first row "Filter by text:" and the text
///   field, and at their right one below the other the boxes "Show Legacy Database", "Match fore
///   diameter" and "Match aft diameter"; the table below, taking the space; below it the note "<up
///   arrow> Check to add preset to the preset drop-down menu ..." one point smaller
///   (StyledLabel(text, -1)); in the last row "Always open this dialog when creating a new
///   <component>" and the Close button at the right. The focus starts in the text field and goes
///   through the boxes to the table, then the last row (Ctrl+Tab leaves the table:
///   TableRowTraversal).
/// - The filters (updateFilters(), all of them together): the text (ComponentPresetFilterModel::
///   setTextFilter(), on every change of the field), the diameter boxes and the legacy box. A
///   diameter box is there only when the request gives its diameter and the type's table has its
///   column (OpenRocket: a previous or next symmetric component, and the column index found): it
///   starts ticked as the preference MatchForeDiameter or MatchAftDiameter says, and each toggle
///   stores the preference. For a flipped nose cone the aft box has the fore box's text and
///   tooltip (UserPrompts::PresetRequest::flippedNoseCone). "Show Legacy Database" starts
///   unticked, whatever the stored layout shows (ComponentPresetTable::loadUiPreferences()), and
///   it and the table's Legacy column follow each other (ComponentPresetTable::setLegacyShown(),
///   legacyShownChanged()).
/// - "Always open this dialog when creating a new <component>" shows and, on every click, stores
///   the preference GuiPreferenceKeys::alwaysOpenPresetKey() of the component's kind (true when
///   none is stored). Its <component> is the kind's translated name; the key is the English one.
/// - Close (the default button: Enter in the text field or a box presses it) stores the table's
///   layout under GuiPreferenceKeys::componentPresetTableName() and closes the dialog with the
///   selected row's preset (UserPrompts::PresetOutcome::CHOSEN), or with none when no row is
///   selected (NONE). A double click on a row applies it and closes without storing the layout,
///   unless the cell clicked is in the first shown column (Java: getSelectedColumn() > 0; the
///   favourite column unless it was moved). Escape and the window's close button close the
///   dialog without a preset and without storing the layout (CLOSED; Escape also when the table
///   has the focus). The favourites ticked in the table, the boxes' preferences and the window's
///   size and place are stored as they change, however the dialog is closed.
/// - The size and place are remembered (WindowGeometry, under kGeometryName: Java's
///   GUIUtil.rememberWindowSize() and rememberWindowPosition() keep them by the dialog's class).
///   Without a remembered size the dialog opens at 70 % of the width and the height of the window
///   its parent's window belongs to (Java: owner.getParent(), the main window when the parent is
///   a configuration dialog), or at its contents' size when there is none (Java: pack()).
/// - The objects' names are Java's: the text field "filterText", the table
///   "componentSelectionTable", the boxes "showLegacyCheckBox", "foreDiameterFilterCheckBox" and
///   "aftDiameterFilterCheckBox", the label "filterLabel", "Always open this dialog"
///   "alwaysOpenPreset", the button "closeButton"; the note, which has no name in Java,
///   "favoritesLabel"; the dialog kGeometryName.
///
/// Deviations from OpenRocket:
/// - The text filter, the header menu's Sort items and the Legacy column follow
///   ComponentPresetFilterModel and ComponentPresetTable (bugs 13, 14 and 17 of the plan's
///   section 10.3, fixed there); a flipped nose cone's base is compared with the previous
///   component's aft diameter (bug 16, fixed in PresetDiameterFilters, which makes the request).
/// - The answer goes back to the caller, which applies a CHOSEN preset after the dialog closed;
///   Java's dialog applies it itself (PresetModel.setSelectedItem()) right after it hid itself:
///   the same order of events.
/// - A diameter box OpenRocket makes but does not add to the dialog (no neighbour, or no column)
///   is not made at all; nothing ever showed it.
/// - Java reads its preferences with SwingPreferences.getBoolean(), which stores a default it does
///   not find (MatchForeDiameter, MatchAftDiameter, "<name>AlwaysOpenPreset" are written when the
///   dialog opens); here a preference is stored only when the user changes it.
/// - The 70 % are of the main window's size without its frame, and the dialog's size is without
///   its frame (Java's sizes include the frames, which Qt knows only once a window is shown).
///   Without a remembered place Qt centres the dialog over its parent's window (Java centres it
///   over the main window and then lets the platform place it, setLocationByPlatform(true)).
/// - The text field's least width is Java's 50 pixels; its preferred width is Qt's (Java: 320).
class ComponentPresetChooserDialog final : public QDialog
{
    Q_OBJECT

public:
    /// The dialog's object name and the name its geometry is remembered under (WindowGeometry).
    static constexpr const char* kGeometryName = "ComponentPresetChooserDialog";
    /// The share of the main window's width and height the dialog opens with.
    static constexpr double kParentShare = 0.7;
    /// The text field's least width ("width 50:320").
    static constexpr int kFilterTextMinimumWidth = 50;
    /// The note's size: one point smaller (StyledLabel(text, -1)).
    static constexpr double kFavoritesLabelSize = -1.0;

    /// The parts library for the component @p request describes, over the presets and
    /// preferences of @p services, which outlive the dialog. Not shown yet.
    /// @throws BugError when the request's kind takes no presets
    ComponentPresetChooserDialog(const GuiServices& services, UserPrompts::PresetRequest request,
                                 QWidget* parent = nullptr);
    ~ComponentPresetChooserDialog() override;

    ComponentPresetChooserDialog(const ComponentPresetChooserDialog&)            = delete;
    ComponentPresetChooserDialog& operator=(const ComponentPresetChooserDialog&) = delete;
    ComponentPresetChooserDialog(ComponentPresetChooserDialog&&)                 = delete;
    ComponentPresetChooserDialog& operator=(ComponentPresetChooserDialog&&)      = delete;

    /// UserPrompts::choosePreset(): shows a new dialog for @p request as a child of @p parent
    /// (ModalDialog: one destroyed with its parent while it is open answers CLOSED) modal and
    /// returns how it was left.
    /// @throws BugError when the request's kind takes no presets
    [[nodiscard]] static UserPrompts::PresetAnswer choose(
        QWidget* parent, const GuiServices& services, const UserPrompts::PresetRequest& request);

    /// How the dialog was left: CLOSED until Close or a double click on a row closes it.
    [[nodiscard]] const UserPrompts::PresetAnswer& answer() const noexcept { return m_answer; }

    // ---- the parts (for the tests)

    [[nodiscard]] QLineEdit&            filterText() const noexcept { return *m_filterText; }
    [[nodiscard]] ComponentPresetTable& table() const noexcept { return *m_table; }
    [[nodiscard]] QCheckBox& showLegacyCheckBox() const noexcept { return *m_showLegacy; }
    /// The "Match fore diameter" box, null when the dialog has none.
    [[nodiscard]] QCheckBox* foreDiameterFilterCheckBox() const noexcept { return m_foreBox; }
    /// The "Match aft diameter" box, null when the dialog has none.
    [[nodiscard]] QCheckBox*   aftDiameterFilterCheckBox() const noexcept { return m_aftBox; }
    [[nodiscard]] QCheckBox&   alwaysOpenPresetCheckBox() const noexcept { return *m_alwaysOpen; }
    [[nodiscard]] QPushButton& closeButton() const noexcept { return *m_close; }
    [[nodiscard]] QLabel&      filterLabel() const noexcept { return *m_filterLabel; }
    [[nodiscard]] StyledLabel& favoritesLabel() const noexcept { return *m_favoritesLabel; }

private:
    /// Makes the diameter boxes the request and the table's columns call for.
    void makeDiameterBoxes();
    /// Lays the parts out (see the class comment) and sets the focus order.
    void layOut();
    /// Gives the dialog its remembered geometry, or 70 % of the main window's size.
    void applyInitialGeometry(QWidget* parent);
    /// The filters of the text and the boxes (updateFilters()).
    void updateFilters();
    /// Close: stores the layout and closes with the selected preset, or none.
    void closeWithSelection();
    /// A double click on @p index: applies its row unless it is in the first shown column.
    void applyDoubleClicked(const QModelIndex& index);

    Preferences*               m_preferences;
    UserPrompts::PresetRequest m_request;
    std::string                m_tableName;
    UserPrompts::PresetAnswer  m_answer;
    ComponentPresetTable*      m_table;
    QLabel*                    m_filterLabel;
    QLineEdit*                 m_filterText;
    QCheckBox*                 m_showLegacy;
    QCheckBox*                 m_foreBox = nullptr;
    QCheckBox*                 m_aftBox  = nullptr;
    StyledLabel*               m_favoritesLabel;
    QCheckBox*                 m_alwaysOpen;
    QPushButton*               m_close;
    /// Remembers the dialog's size and place.
    WindowGeometry* m_geometry = nullptr;
};

}  // namespace QtRocket::Gui
