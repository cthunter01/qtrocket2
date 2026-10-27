#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/util/Signal.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QGridLayout;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTableView;

namespace QtRocket
{
class FlightConfigurationId;
class MotorMount;
class ThrustCurveMotor;
class ThrustCurveMotorSelection;
class ThrustCurveMotorSet;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;
class MotorFilterPanel;
class MotorInformationPanel;
class MotorTableModel;
class UnitSelector;
class UnitSpinBox;

/// The motor chooser's panel (OpenRocket's ThrustCurveMotorSelectionPanel, swing
/// gui/dialogs/motor/thrustcurve/ThrustCurveMotorSelectionPanel.java, a MotorSelector): the
/// widgets of the chooser's ThrustCurveMotorSelection, which holds what it decides (the table's
/// rows, the selected set and curve, the curve list, the delay and the nozzle exit diameter). The
/// chooser dialog (MotorChooserDialog) holds one for its life, so that the search, the filters,
/// the sort and the selection survive between questions.
///
/// On the left, in a scroll area, from the top:
/// - "Select thrust curve:" and the curve list (getCurveItems(): each curve by its name in the
///   name column, Motor::getMotorName(), in the colour of its index, PlotColors::curveColor(); the
///   selected item of the open list on that colour), enabled with its label for more than one
///   curve; choosing a curve selects it (select()). The list refills from getCurveItems() with
///   its signals blocked: its own refill never selects (QtRocket fixes OpenRocket's re-entry, the
///   user's bug 20, as ThrustCurveMotorSelection does: the curve chosen stays selected).
/// - "Ejection charge delay:" and an editable list of the set's delays (getDelayItems()) showing
///   getDelayText(), with "(or type in desired delay in seconds)" under it. The list follows
///   OpenRocket's JComboBox as the probe tools/openrocket-probes/motor-selection-panel pins it:
///   choosing an item is one action event, which reads its text (setDelayText()); Enter is two,
///   the typed text (when it differs from the selected item) and then the selected item, which the
///   first event's listener has made the delay's own text (so "5.04" ends as 5, as in OpenRocket);
///   losing the focus is the same as Enter when the typed text differs from the selected item, and
///   nothing otherwise. A text the delay cannot take keeps it ("abc"; "NaN", which QtRocket
///   refuses: bug 11, ThrustCurveMotorSelection). The list's own refill fires nothing, as the
///   listener's own setSelectedItem() fires nothing in Java (its re-entrancy guard), and the second
///   editingFinished of a Return finds nothing to commit.
/// - "Nozzle exit diameter:", a spin box (length units, step 0.1 of the unit, from 0 to the
///   selected curve's diameter: ThrustCurveMotorSelection::setNozzleExitDiameter()) with its unit
///   selector, and "Enter zero when unknown; no powered base-drag correction is applied for this
///   motor."; the delay and nozzle rows are enabled while a set is selected.
/// - "Hide very similar thrust curves" (setHideSimilar(), stored) and "Hide motors which are not
///   in regular production" (the filter panel's setHideUnavailable(), stored), one point smaller.
/// - "Motor name column:" with "Use common name" and "Use manufacturer's designation"
///   (Preferences::setMotorNameColumn()): the table and the curve list show the other name, and
///   the selection stays on its motor set (QtRocket fixes M3's bug 10: OpenRocket re-selects the
///   table row of the same index, which names another set when the table is sorted by its Name
///   column); the table then scrolls to it.
/// - The table (MotorTableModel, one row selected at a time; a selection selects the set's default
///   curve, selectMotorFromTable(); a double click of the left button anywhere on it, also below
///   its rows, emits okRequested()), with JTable's keys and buttons: no type-ahead, Enter selects
///   the next row and Shift+Enter the one before (from the last row to the first and back, the
///   current cell moving to the next or the previous column), the other buttons than the left one
///   select nothing, and the columns can be dragged into another order.
/// - "Number of motors: <rows>" ("None" for none) and "Motor DB version: <version>" ("Unknown"
///   without one), italic, two points smaller, in the dim text colour.
/// - "Search:" and the search field: every change of its text sets the filter's search terms
///   (MotorRowFilter::setSearchText()), sorts the table again and scrolls to the selection.
/// A vertical separator, then the tabs "Filter Motors" (MotorFilterPanel, whose every change sorts
/// the table again and scrolls to the selection) and "Show Details" (MotorInformationPanel, the
/// selected curve with the curve list's curves).
///
/// When the selected set leaves the table (filtered out), it stays selected (the curve list, the
/// details, the delay and OK keep it) and is selected in the table again when it comes back
/// (scrollSelectionVisible()). The widgets are updated where OpenRocket updates them: after
/// setMotorMountAndConfig() for a configuration without a motor they still show the motor of
/// before while its row stays selected (the probe's STALE lines), until selectMotorFromTable()
/// (the dialog's open()) selects the table's row again; when the set of before was not selected
/// in the table (a filter of the last question hid it) and the new mount's filter shows it, its
/// row is selected again, which selects its default curve at once (the probe's RESELECT lines:
/// OpenRocket's first motorFilterPanel.setMotorMount() scrolls to the set of before, before the
/// reset). Constructing the panel stores "Hide very similar thrust curves" and
/// "Hide motors which are not in regular production" as read, as OpenRocket's constructor does by
/// running their listeners once (ThrustCurveMotorSelection itself stores nothing).
///
/// The objects' names are Java's fields' (curveSelectionLabel, curveSelectionBox,
/// ejectionChargeDelayLabel, delayBox, nozzleExitDiameterLabel, nozzleExitDiameterSpinner,
/// nozzleExitDiameterUnitSelector, hideSimilarBox, hideUnavailableBox, table, nrOfMotorsLabel,
/// motorDbVersionLabel, searchField, motorFilterPanel, motorInformationPanel) and locals'
/// (motorNameColumn, commonName, designation, rightSide, scrollPane, infoRow); the panel is
/// "selectionPanel" (the dialog's field).
///
/// The panel's ThrustCurveMotorSelection lives as long as the panel; the motor database and the
/// preferences of the services given must outlive it.
///
/// Deviations from OpenRocket:
/// - Opened for a configuration with a motor (setMotorMountAndConfig()), the panel first clears
///   the table's selection, so that the row an earlier question selected cannot replace the
///   configured motor. OpenRocket keeps that row selected while it sorts the table for the new
///   mount, and when the mount's limits ("Limit motor diameter to mount diameter", "Limit motor
///   length to mount length") hide the configured motor's set, the row stays and the table's
///   selection listener selects its set: the chooser shows, and OK writes, the motor of the
///   earlier question (tools/openrocket-gui-pins' motor-chooser family: "Airstart timing" opened
///   for its 54 mm mount's K550W after its 38 mm mount's I211W selects the I211W; "Two stage high
///   power rocket" replaces I59WN-P and I357T-14 by H148R-10). OpenRocket bug 21, fixed by the
///   user's decision of 2026-10-27 (beyond the bugs 1-20 of plan 10.3). The table then shows no
///   selection until the configured set is shown, as when a filter hides the selected set.
/// - The curve chosen stays selected when the curve list refills (bug 20, above).
/// - The name column switch keeps the selected set (bug 10, above) and scrolls the table to it.
/// - The table scrolls to the selection with Qt's EnsureVisible (OpenRocket scrolls a rectangle
///   100 pixels above and below the row into view). A press of another button than the left one
///   gives the table the focus (Qt gives it before the table sees the press; Swing's table takes
///   none), though it selects nothing. When a filter hides the current row, Qt's current cell goes
///   with it (a JTable keeps its column lead), so that the next Enter and the selection shown
///   again start from the first column.
/// - The curve list's closed box shows the selected curve's name in its colour, also with the
///   focus (OpenRocket's renderer then paints the box in that colour, the text in the list's
///   background); the open list's items are drawn by an item delegate in the style's item look
///   (QStyledItemDelegate), not the menu look some styles give a combo box's popup.
/// - Both lists take the wheel only while they have the focus (as UnitSpinBox and the other combo
///   boxes; Java's take none), and opening the delay list's popup commits a typed text first
///   (Qt's line edit loses the focus to the popup; Java's keeps the edit), which the item chosen
///   then replaces, as in Java.
/// - The layout is a grid of two columns (labels, fields) after MigLayout's "[][grow]": the name
///   column's radio buttons sit together in the field column, the paragraph gaps are 12 pixels,
///   and the table has its least height of 80 pixels but not the preferred width of 500 (the
///   chooser dialog sizes itself). The left column's scroll area and the tabs have no least size
///   (Java's setMinimumSize(0, 0); 1 pixel here, since Qt's layouts take 0 for none), so that the
///   chooser can be made as small as OpenRocket's: the left column then scrolls, and the tabs'
///   widgets are squeezed (OpenRocket's layout clips them).
class ThrustCurveMotorSelectionPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The panel over the motor sets of @p services (motors()), with its preferences (the filters,
    /// the boxes, the name column, the preferred curves) and its prompts (the plot dialog's
    /// export); the services must outlive it. No motor is selected.
    ThrustCurveMotorSelectionPanel(const GuiServices& services, QWidget* parent);
    ~ThrustCurveMotorSelectionPanel() override;
    ThrustCurveMotorSelectionPanel(const ThrustCurveMotorSelectionPanel&)            = delete;
    ThrustCurveMotorSelectionPanel& operator=(const ThrustCurveMotorSelectionPanel&) = delete;
    ThrustCurveMotorSelectionPanel(ThrustCurveMotorSelectionPanel&&)                 = delete;
    ThrustCurveMotorSelectionPanel& operator=(ThrustCurveMotorSelectionPanel&&)      = delete;

    /// setMotorMountAndConfig(fcid, mount): the chooser is opened for @p mount's motor
    /// configuration in @p fcid (its override, else the mount's default): the filter takes the
    /// mount (its size, its motors for "Hide motors already used in the mount"), and the
    /// configuration's motor is selected with its delay (also a non-standard one: bug 8) and
    /// nozzle exit diameter, or nothing is (ThrustCurveMotorSelection::setMotorMountAndConfig(),
    /// MotorChooserInput::fromMount()). Only the motors and the mount's size are kept. For a
    /// configuration with a motor the table's selection is cleared first (a deviation, see the
    /// class comment). One without keeps it, which the dialog's open() then selects
    /// (selectMotorFromTable()), and selects the row of the set selected until now when the
    /// mount's filter shows it (as OpenRocket's first motorFilterPanel.setMotorMount() does before
    /// the reset): a row not selected yet selects its set's default curve at once.
    void setMotorMountAndConfig(const FlightConfigurationId& fcid, const MotorMount& mount);

    /// selectMotorFromTable(): the set of the table's selected row, when it is not the selected
    /// one, gets its default curve selected (the dialog's open() calls it, so that a chooser
    /// opened for an empty configuration selects the row the last session left).
    void selectMotorFromTable();

    /// The selected curve (getSelectedMotor()), or null.
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> getSelectedMotor() const;
    /// The selected ejection delay in s, Motor::kPluggedDelay for plugged (getSelectedDelay()).
    [[nodiscard]] double getSelectedDelay() const;
    /// The nozzle exit diameter in m (getSelectedNozzleExitDiameter()).
    [[nodiscard]] double getSelectedNozzleExitDiameter() const;

    /// selectedMotor(motor), on OK: stores @p motor as the preferred curve of its set.
    void selectedMotor(const ThrustCurveMotor& motor);

    /// The widget that has the focus first (getDefaultFocus()): the search field.
    [[nodiscard]] QWidget& getDefaultFocus() const noexcept;

    /// Commits what is being typed in the delay list (as its focus loss does) and in the nozzle
    /// spin box (UnitSpinBox::commitEdit()): the dialog calls it before it reads the selection on
    /// OK (a click on a macOS button takes no focus).
    void commitEdits();

    /// "Motor DB version:" shows @p version, the database_version of the motor database's
    /// metadata.json; "Unknown" for none or one not above 0 (getLocalMotorDatabaseVersionText()).
    /// The panel starts with none (QtRocket ships metadata.json in Milestone 3, run 3).
    void setMotorDatabaseVersion(std::optional<std::int64_t> version);

    /// The chooser's logic, which the panel shows (for the dialog and the tests).
    [[nodiscard]] ThrustCurveMotorSelection&       selection() noexcept { return *m_selection; }
    [[nodiscard]] const ThrustCurveMotorSelection& selection() const noexcept
    {
        return *m_selection;
    }

    /// The table's model.
    [[nodiscard]] MotorTableModel& tableModel() const noexcept { return *m_tableModel; }

    /// The wheel over the curve and delay lists, the double click on the table.
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// The table was double-clicked with the left button (Java: dialog.close(true), its OK).
    void okRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    /// The rows of the left column (Java's blocks, in order).
    void makeCurveRow(QWidget& panel, QGridLayout& grid);
    void makeDelayRows(QWidget& panel, QGridLayout& grid);
    void makeNozzleRows(QWidget& panel, QGridLayout& grid);
    void makeHideBoxes(QWidget& panel, QGridLayout& grid);
    void makeNameColumnRow(QWidget& panel, QGridLayout& grid);
    void makeTable(QWidget& panel, QGridLayout& grid);
    void makeInfoRow(QWidget& panel, QGridLayout& grid);
    void makeSearchRow(QWidget& panel, QGridLayout& grid);

    /// select(motor) of the curve list (its item @p index chosen).
    void curveChosen(int index);
    /// What Java's select() does after a curve change, the set before it being @p previousSet: the
    /// nozzle, updateData(), the delays when the set changed, scrollSelectionVisible().
    void motorSelected(const ThrustCurveMotorSet* previousSet);
    /// updateData(): the curve list, the enabled states and the details of the selection.
    void updateData();
    /// Refills the curve list from getCurveItems() with its signals blocked, and colours it.
    void fillCurveList();
    /// The curve list's items' colours and the box's text colour, from the palette.
    void updateCurveColors();
    /// setDelays(): the delay list's items and the delay's text.
    void showDelays();
    /// The delay list's action listener for the selected item @p selected.
    void delayAction(const std::optional<QString>& selected);
    /// The delay list's item @p index chosen; Enter in its editor; its focus loss.
    void delayItemChosen(int index);
    void delayEntered();
    void delayEditFinished();
    /// The nozzle exit diameter shown again (its maximum the selected curve's diameter).
    void showNozzle();
    /// The nozzle spin box's value changed.
    void nozzleChanged();
    /// A name column radio button was clicked: the designation (@p designation) or the common name.
    void nameColumnClicked(bool designation);
    /// The search field's text changed.
    void searchChanged(const QString& text);
    /// The filter panel's filterChanged() (onSelectionChanged()).
    void filterChanged();
    /// scrollSelectionVisible(): selects the selected set's row and scrolls to it, when shown.
    void scrollSelectionVisible();
    /// Selects @p set's row and scrolls to it, when shown (the current cell keeps its column).
    void selectRowOf(const ThrustCurveMotorSet& set);
    /// updateNrOfMotors() and updateMotorDatabaseVersion().
    void updateNrOfMotors();
    void updateMotorDatabaseVersion();
    /// The dim text colour of the information row's labels, from the palette.
    void updateDimColors();
    /// Whether the delay and nozzle rows are enabled.
    void setSelectionWidgetsEnabled(bool enabled);

    GuiServices                                m_services;
    std::unique_ptr<ThrustCurveMotorSelection> m_selection;

    QLabel*                m_curveSelectionLabel{nullptr};
    QComboBox*             m_curveSelectionBox{nullptr};
    QLabel*                m_ejectionChargeDelayLabel{nullptr};
    QComboBox*             m_delayBox{nullptr};
    QLabel*                m_nozzleExitDiameterLabel{nullptr};
    UnitSpinBox*           m_nozzleExitDiameterSpinner{nullptr};
    UnitSelector*          m_nozzleExitDiameterUnitSelector{nullptr};
    QCheckBox*             m_hideSimilarBox{nullptr};
    QCheckBox*             m_hideUnavailableBox{nullptr};
    QRadioButton*          m_commonName{nullptr};
    QRadioButton*          m_designation{nullptr};
    QTableView*            m_table{nullptr};
    MotorTableModel*       m_tableModel{nullptr};
    QLabel*                m_nrOfMotorsLabel{nullptr};
    QLabel*                m_motorDbVersionLabel{nullptr};
    QLineEdit*             m_searchField{nullptr};
    MotorFilterPanel*      m_motorFilterPanel{nullptr};
    MotorInformationPanel* m_motorInformationPanel{nullptr};

    /// The nozzle exit diameter's model (Java's nozzleExitDiameterModel) and its listener,
    /// destroyed before it.
    std::shared_ptr<DoubleBinding> m_nozzleExitDiameterModel;
    Signal<>::ScopedConnection     m_nozzleConnection;
    /// The curves of the curve list, in its order (the items of getCurveItems()).
    std::vector<std::shared_ptr<const ThrustCurveMotor>> m_curves;
    /// The delay list's selected item (JComboBox.getSelectedItem()): null when the list is
    /// emptied without a motor.
    std::optional<QString> m_delaySelected;
    /// The version "Motor DB version:" shows.
    std::optional<std::int64_t> m_motorDatabaseVersion;
    /// Where the left button went down for the second time of a double click on the table, until
    /// it comes up (Swing's mouseClicked with a click count of 2).
    std::optional<QPoint> m_doubleClickPosition;
};

}  // namespace QtRocket::Gui
