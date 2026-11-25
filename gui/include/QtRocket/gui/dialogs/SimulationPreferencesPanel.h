#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/util/Signal.h"

class QCheckBox;
class QEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class EnumComboBox;
class FieldValidator;
class ObjectSelection;
class StyledLabel;

/// The Simulation page of the preferences dialog (OpenRocket's SimulationPreferencesPanel, swing
/// gui/dialogs/preferences/): the defaults of new simulations and two behaviours of the
/// Simulations tab, each written to the preferences at once.
///
/// What it shows (Java's MigLayout("fillx") column):
/// - "Confirm deletion of simulations." (ConfirmDeleteSimulation) and "Run out-dated simulations
///   when you open the simulation tab." (AutoRunSimulations), check boxes whose clicks store the
///   preference (Java's ActionListeners);
/// - 40 pixels below them, "Simulator options" (simulatorOptionsPanel), a titled box with a grid
///   (Java: "[grow][min!][min!][]", the labels' column taking the width left):
///   - the bold dark-red NOTE "the settings below apply only to newly created simulations. To
///     change the settings for an existing simulation, select that simulation and edit it."
///     (launchWarning, its tooltip "You would not be the first one to be confused by this :)");
///   - "Calculation method:" and "Extended Barrowman", "Simulation method:" and Java's literal
///     "6-DOF Runge-Kutta 4" (labels, with the method's tooltips: Java's page offers no other
///     stepper);
///   - "Geodetic calculations:" (GeodeticComputation: GeodeticComputationStrategy, SPHERICAL by
///     default; its tooltip the chosen strategy's description) and "Gravity model:"
///     (GravityModel: WGS or Constant; its tooltip the chosen model's), combo boxes;
///   - "Gravity value:" (ConstantGravityValue, 9.807 m/s^2 by default, slider 0 to 20 m/s^2, 100
///     pixels wide), shown only while the gravity model is Constant;
///   - "Time step:" (TimeStep: SimulationTimeStep, 0.01 to 1 s, slider 0.01 to 0.2 s, 100 pixels
///     wide) and "Maximum simulation time:" (MaxSimulationTime: SimulationMaxTime, at least 1 s,
///     no slider), with the options tab's tooltips;
///   - "Use fixed random seed:" (fixedRandomSeedCheckBox, SimulationRandomSeedFixed) and the
///     seed's field (randomSeedField, SimulationRandomSeed; see below);
///   - below the grid, "Reset to default" (resetButton; its tooltip names the recommended time
///     step): resetToDefault().
///
/// The seed, as Java's (its own controls over the preferences, not the options tab's
/// RandomSeedControls): the box shows whether the seed is fixed, the field the seed while it is
/// and nothing (disabled) otherwise. Checking the box draws a new seed (Java: new
/// Random().nextInt()), stores it and fixes it; unchecking unfixes it. Every change of the field's
/// text that is a seed (RandomSeedControls::parseRandomSeed(), Java's Integer.valueOf(trim()))
/// stores it while the box is checked. The field's validator (FieldValidator, with its message
/// popup) shows "Enter a signed 32-bit integer." while the box is checked and the text is no seed.
/// The focus leaving the field for good with a text that is no seed while the box is checked draws
/// a new seed (Java's replaceInvalidRandomSeed() on a non-temporary focus loss). prepareForSave()
/// resolves the controls before the preferences are kept (the dialog's OK).
///
/// The bounds: Java's minima and its maxima where it has them (the time step's 1 s); where Java's
/// are open, those of the simulation's input ranges (the user's decision U-M4-7: the constant
/// gravity at most 100 m/s^2, SimulationInputRanges::kConstantGravity, the maximum time at most
/// 1000000 s, kMaxSimulationTime), as on the options tab (SimulationOptionsPanel).
///
/// Lifetime: the bindings and the selection of the preferences (ObjectSelection::make<
/// Preferences>()) are kept by @p scope, which the dialog commits (BindingScope::commitAll())
/// before Apply and OK and releases when it closes; from then on the fields write nothing.
///
/// Deviations from OpenRocket:
/// - The maxima where Java has none (see above).
/// - The combo boxes' tooltips and the gravity value's row follow their bindings: after "Reset to
///   default" they show the strategy and the model restored (the row hidden for WGS), where
///   Java's keep what they showed before the reset (their ActionListeners do not hear the models'
///   setSelectedItem()), so that its gravity row stays shown beside "WGS".
/// - "Reset to default" first commits a spin box's pending edit (a click on a button that takes
///   no focus would leave it pending, where Java's focus loss commits it).
/// - The temporary focus losses of Java (FocusEvent.isTemporary()) are Qt's
///   ActiveWindowFocusReason, PopupFocusReason and MenuBarFocusReason (as RandomSeedControls).
/// - The seed drawn by the check box and by an invalid seed's focus loss comes from
///   QRandomGenerator::global(), Java's from new Random().
/// - Layout approximations: a QGroupBox for the titled border, a QGridLayout for MigLayout's
///   grid.
class SimulationPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// Java's "w 100" of the gravity value's and the time step's sliders.
    static constexpr int kSliderWidth = 100;
    /// Java's "gaptop 40lp" above the simulator options.
    static constexpr int kOptionsGap = 40;
    /// The seed field's minimum width (Java: "w 130lp!").
    static constexpr int kSeedFieldWidth = 130;

    /// The page over @p preferences, which must outlive it, its bindings kept by @p scope (see
    /// the class comment).
    SimulationPreferencesPanel(Preferences& preferences, BindingScope& scope, QWidget* parent);
    ~SimulationPreferencesPanel() override;
    SimulationPreferencesPanel(const SimulationPreferencesPanel&)            = delete;
    SimulationPreferencesPanel& operator=(const SimulationPreferencesPanel&) = delete;
    SimulationPreferencesPanel(SimulationPreferencesPanel&&)                 = delete;
    SimulationPreferencesPanel& operator=(SimulationPreferencesPanel&&)      = delete;

    /// "Confirm deletion of simulations." (ConfirmDeleteSimulation).
    [[nodiscard]] QCheckBox& confirmDeletionCheckBox() const noexcept { return *m_confirmDelete; }
    /// "Run out-dated simulations when you open the simulation tab." (AutoRunSimulations).
    [[nodiscard]] QCheckBox&   autoRunCheckBox() const noexcept { return *m_autoRun; }
    [[nodiscard]] QGroupBox&   simulatorOptionsBox() const noexcept { return *m_optionsBox; }
    [[nodiscard]] StyledLabel& launchWarningLabel() const noexcept { return *m_launchWarning; }
    [[nodiscard]] QLabel& calculationMethodLabel() const noexcept { return *m_calculationLabel; }
    [[nodiscard]] QLabel& extendedBarrowmanLabel() const noexcept { return *m_barrowmanLabel; }
    [[nodiscard]] QLabel& simulationMethodLabel() const noexcept { return *m_methodLabel; }
    /// Java's literal "6-DOF Runge-Kutta 4".
    [[nodiscard]] QLabel& simulationMethodValueLabel() const noexcept { return *m_methodValue; }
    [[nodiscard]] QLabel& geodeticLabel() const noexcept { return *m_geodeticLabel; }
    /// The geodetic computation (GeodeticComputation).
    [[nodiscard]] EnumComboBox& geodeticComboBox() const noexcept { return *m_geodeticCombo; }
    [[nodiscard]] QLabel&       gravityModelLabel() const noexcept { return *m_gravityModelLabel; }
    /// The gravity model (GravityModel).
    [[nodiscard]] EnumComboBox& gravityModelComboBox() const noexcept { return *m_gravityCombo; }
    /// The constant gravity's row (ConstantGravityValue), hidden unless the model is Constant.
    [[nodiscard]] const ConfigFields::NumberWidgets& gravityValue() const noexcept
    {
        return m_gravityValue;
    }
    /// "Time step:" (TimeStep) and "Maximum simulation time:" (MaxSimulationTime).
    [[nodiscard]] const ConfigFields::NumberWidgets& timeStep() const noexcept
    {
        return m_timeStep;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& maximumTime() const noexcept
    {
        return m_maximumTime;
    }
    /// "Use fixed random seed:" (fixedRandomSeedCheckBox) and the seed's field (randomSeedField)
    /// with its validator.
    [[nodiscard]] QCheckBox&      randomSeedCheckBox() const noexcept { return *m_seedCheckBox; }
    [[nodiscard]] QLineEdit&      randomSeedField() const noexcept { return *m_seedField; }
    [[nodiscard]] FieldValidator& randomSeedValidator() const noexcept { return *m_seedValidator; }
    /// "Reset to default" (resetButton).
    [[nodiscard]] QPushButton& resetButton() const noexcept { return *m_resetButton; }

    /// Java's prepareForSave(): the seed the field holds while the box is checked is stored and
    /// fixed; anything else (no seed in the field, the box unchecked) unfixes the seed. Then the
    /// controls show the preferences. The preferences dialog calls it as it keeps the preferences
    /// (OK, Escape, its close button; Java's windowClosed listener).
    void prepareForSave();

    /// "Reset to default" (Java: its ActionListener): the time step 0.05 s, the maximum time
    /// 1200 s, the geodetic computation SPHERICAL, the gravity model WGS, the constant gravity
    /// 9.807 m/s^2 and an unfixed seed (the stored seed stays), then the seed's controls show the
    /// preferences. It first commits a pending edit (see the class comment).
    void resetToDefault();

    /// The seed field's focus loss (see the class comment); the events always go on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // The parts of the grid @p grid of the widget @p parent, in Java's order.
    void buildMethods(QWidget& parent, QGridLayout& grid);
    void buildGeodeticAndGravity(QWidget& parent, QGridLayout& grid, BindingScope& scope);
    void buildTimes(QGridLayout& grid, BindingScope& scope);
    void buildRandomSeed(QWidget& parent, QGridLayout& grid);
    /// Shows the gravity value's row while the model is Constant.
    void showGravityValue();
    /// Commits the pending edit of the page's spin boxes.
    void commitEdits();

    /// The user checked or unchecked the box (Java: its ActionListener).
    void seedCheckBoxClicked(bool checked);
    /// Java's updateRandomSeedFromField().
    void seedFromField();
    /// Java's replaceInvalidRandomSeed().
    void replaceInvalidRandomSeed();
    /// Java's updateRandomSeedControlsFromPreferences().
    void showRandomSeed();

    Preferences*                     m_preferences;
    std::shared_ptr<ObjectSelection> m_selection;

    QCheckBox*                  m_confirmDelete{nullptr};
    QCheckBox*                  m_autoRun{nullptr};
    QGroupBox*                  m_optionsBox{nullptr};
    StyledLabel*                m_launchWarning{nullptr};
    QLabel*                     m_calculationLabel{nullptr};
    QLabel*                     m_barrowmanLabel{nullptr};
    QLabel*                     m_methodLabel{nullptr};
    QLabel*                     m_methodValue{nullptr};
    QLabel*                     m_geodeticLabel{nullptr};
    EnumComboBox*               m_geodeticCombo{nullptr};
    QLabel*                     m_gravityModelLabel{nullptr};
    EnumComboBox*               m_gravityCombo{nullptr};
    ConfigFields::NumberWidgets m_gravityValue;
    ConfigFields::NumberWidgets m_timeStep;
    ConfigFields::NumberWidgets m_maximumTime;
    QCheckBox*                  m_seedCheckBox{nullptr};
    QLineEdit*                  m_seedField{nullptr};
    FieldValidator*             m_seedValidator{nullptr};
    QPushButton*                m_resetButton{nullptr};
    /// Java's updatingRandomSeedControls: set while the page writes the seed's widgets itself.
    bool m_updatingSeed{false};

    /// The combo boxes' tooltips and the gravity row follow their bindings; held here, as they
    /// call the page.
    Signal<>::ScopedConnection m_geodeticConnection;
    Signal<>::ScopedConnection m_gravityConnection;
};

}  // namespace QtRocket::Gui
