#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/util/Signal.h"

class QEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;

namespace QtRocket
{
class Simulation;
class SimulationOptions;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class DescriptionArea;
class EnumComboBox;
class ObjectSelection;
class RandomSeedControls;
class SimulationExtensionList;

/// The "Simulation options" tab of the simulation settings dialog (OpenRocket's
/// SimulationOptionsPanel, swing gui/simulation/), over the simulation's SimulationOptions, which
/// its fields edit live (the dialog adds its undo position when it opens, and Cancel undoes).
///
/// What it shows (Java's two titled panels side by side):
/// - "Simulator options" (simulatorOptionsPanel), a grid:
///   - "Calculation method:" and "Extended Barrowman" (labels, both with the method's tooltip);
///   - "Simulation method:" with the stepper's combo box (SimulationStepperMethodChoice: RK4 and
///     RK6 by their names, "6-DOF Runge-Kutta 4", ...; its tooltip the chosen method's
///     description; the core's setter stores the choice in the preferences too, as Java's);
///   - "Aerodynamic data:", "Configure..." (configureLookupButton), which opens the aerodynamic
///     lookup dialog (openLookupDialog()), and a help icon (lookupSummaryIcon) whose tooltip
///     names the tables in use: "Drag: <summary>" and "Stability: <summary>" (AerodynamicLookup
///     Dialog::summaryText(), each escaped as HTML), written again whenever the options change;
///   - "Geodetic calculations:" with its combo box (GeodeticComputation: Flat Earth, Spherical
///     approximation, WGS84 ellipsoid; its tooltip the chosen strategy's description);
///   - "Gravity model:" with its combo box (GravityModelType: WGS, Constant; its tooltip the
///     chosen model's), and "Gravity value:" (ConstantGravity, m/s^2 by default, slider 0 to 20),
///     shown only for the constant model;
///   - "Time step:" (TimeStep, 0.01 to 1 s, slider 0.01 to 0.2) and "Maximum simulation time:"
///     (MaxSimulationTime, at least 1 s, no slider), with Java's tooltips (the time step's tooltip
///     names the recommended 0.05 s in the time step's default unit);
///   - "Use fixed random seed:" and the seed's field (RandomSeedControls);
///   - below the grid "Reset to default" (resetButton; its tooltip names the recommended time
///     step) and "Save as default" (saveButton): the time step, the maximum time, the geodetic
///     computation and the seed from or to the preferences (resetToDefault(), saveAsDefault()),
///     not the gravity or the stepper, as Java;
/// - "Simulation extensions" (simulationExtensionsPanel): the description of extensions
///   (DescriptionArea, 3 rows) and the simulation's extensions (SimulationExtensionList: Help and
///   Delete only, the user's decision U-M4-3: Java's "Add extension" button and its menu with
///   "Copy extension" come with Milestone 7).
///
/// The bounds: Java's minima, its maxima where it has them (the time step's 1 s), and where its
/// are open those of the simulation's input ranges (SimulationInputRanges, the user's decision
/// U-M4-7): the constant gravity at most 100 m/s^2, the maximum simulation time at most 1000000 s.
///
/// Lifetime: the bindings and the selection of the options (ObjectSelection::make<
/// SimulationOptions>()) are kept by @p scope, which the dialog commits (BindingScope::
/// commitAll()) and releases when it closes; from then on the fields write nothing. The
/// simulation is held by its std::shared_ptr, so its options stay at their address while the
/// panel shows them. The dialog's OK calls prepareForSimulation() first.
///
/// Deviations from OpenRocket:
/// - The maxima where Java has none (see above).
/// - The combo boxes follow the options (EnumBinding: the scout's decision D16): "Reset to
///   default" shows the geodetic computation it restored, where Java's combo box keeps what it
///   showed (its EnumModel reads its property once); the gravity row follows its combo box.
/// - The random seed's controls follow the options (RandomSeedControls).
/// - The lookup summary's tooltip comes after the platform's usual delay; Java shows it at once
///   while the mouse is over the icon (configureImmediateTooltipDelay()). It is written again at
///   once on a change of the options, where Java writes it in a later turn of the event loop
///   (invokeLater).
/// - The extension names and descriptions are the core's English texts (SimulationExtensionList).
/// - Layout approximations: QGroupBoxes for the titled panels, a QGridLayout for MigLayout's
///   "[grow][min!][]" (the labels' column takes the width left; the combo boxes span the field
///   columns; the gravity value's spin box, unit selector and slider sit in the time step's
///   columns, where Java's gravity row has a column of its own for each).
class SimulationOptionsPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The tab over @p simulation's options; @p services give the preferences of the default
    /// buttons and the lookup dialog's, and its prompts; @p scope keeps the bindings (see the
    /// class comment).
    /// @throws BugError when @p simulation is null
    SimulationOptionsPanel(std::shared_ptr<Simulation> simulation, const GuiServices& services,
                           BindingScope& scope, QWidget* parent);
    ~SimulationOptionsPanel() override;
    SimulationOptionsPanel(const SimulationOptionsPanel&)            = delete;
    SimulationOptionsPanel& operator=(const SimulationOptionsPanel&) = delete;
    SimulationOptionsPanel(SimulationOptionsPanel&&)                 = delete;
    SimulationOptionsPanel& operator=(SimulationOptionsPanel&&)      = delete;

    /// Java's "w 100" of the time step's slider and "w 90lp" of the gravity's.
    static constexpr int kTimeStepSliderWidth = 100;
    static constexpr int kGravitySliderWidth  = 90;
    /// The rows of the extensions' description (Java: EXTENSION_DESCRIPTION_ROWS) and its
    /// minimum height (EXTENSION_DESCRIPTION_MIN_HEIGHT).
    static constexpr int kExtensionDescriptionRows      = 3;
    static constexpr int kExtensionDescriptionMinHeight = 60;
    /// The minimum width of the extensions' box (Java: "wmin 300lp").
    static constexpr int kExtensionsMinimumWidth = 300;

    /// The simulation whose options are shown.
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }

    // ----------------------------------------------------------------------- the widgets

    [[nodiscard]] QGroupBox& simulatorOptionsBox() const noexcept { return *m_optionsBox; }
    [[nodiscard]] QLabel&    calculationMethodLabel() const noexcept { return *m_calculationLabel; }
    [[nodiscard]] QLabel&    extendedBarrowmanLabel() const noexcept { return *m_barrowmanLabel; }
    [[nodiscard]] QLabel&    stepperLabel() const noexcept { return *m_stepperLabel; }
    /// The stepper's combo box (objectName SimulationStepperMethodChoice).
    [[nodiscard]] EnumComboBox& stepperComboBox() const noexcept { return *m_stepperCombo; }
    [[nodiscard]] QLabel&       aerodynamicDataLabel() const noexcept { return *m_lookupLabel; }
    /// "Configure..." (configureLookupButton).
    [[nodiscard]] QPushButton& configureLookupButton() const noexcept { return *m_configureButton; }
    /// The help icon of the lookup summary (aerodynamicLookupSummaryIconLabel).
    [[nodiscard]] QLabel& lookupSummaryIcon() const noexcept { return *m_lookupSummaryIcon; }
    [[nodiscard]] QLabel& geodeticLabel() const noexcept { return *m_geodeticLabel; }
    /// The geodetic computation's combo box (GeodeticComputation).
    [[nodiscard]] EnumComboBox& geodeticComboBox() const noexcept { return *m_geodeticCombo; }
    [[nodiscard]] QLabel&       gravityModelLabel() const noexcept { return *m_gravityModelLabel; }
    /// The gravity model's combo box (GravityModelType).
    [[nodiscard]] EnumComboBox& gravityModelComboBox() const noexcept { return *m_gravityCombo; }
    /// The constant gravity's row (ConstantGravity), hidden unless the model is constant.
    [[nodiscard]] const ConfigFields::NumberWidgets& gravityValue() const noexcept
    {
        return m_gravityValue;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& timeStep() const noexcept
    {
        return m_timeStep;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& maximumTime() const noexcept
    {
        return m_maximumTime;
    }
    [[nodiscard]] RandomSeedControls& randomSeed() const noexcept { return *m_randomSeed; }
    /// "Reset to default" (resetButton) and "Save as default" (saveButton).
    [[nodiscard]] QPushButton& resetButton() const noexcept { return *m_resetButton; }
    [[nodiscard]] QPushButton& saveButton() const noexcept { return *m_saveButton; }

    [[nodiscard]] QGroupBox&       extensionsBox() const noexcept { return *m_extensionsBox; }
    [[nodiscard]] DescriptionArea& extensionsDescription() const noexcept
    {
        return *m_extensionsDescription;
    }
    [[nodiscard]] SimulationExtensionList& extensionList() const noexcept
    {
        return *m_extensionList;
    }

    // ----------------------------------------------------------------------- the actions

    /// Java's prepareForSimulation(): resolves the seed controls before a simulation starts
    /// (RandomSeedControls::prepareForSimulation()); the settings dialog's OK calls it.
    void prepareForSimulation();

    /// "Reset to default" (Java: resetBtn's ActionListener): the time step, the maximum time
    /// (the preferences' SimulationTimeStep and SimulationMaxTime, 0.05 and 1200 s without them),
    /// the geodetic computation (GeodeticComputationStrategy, SPHERICAL without it) and the seed
    /// (fixed to the preferences' seed while they fix one, unfixed otherwise), then the seed
    /// controls show the options. It first commits a spin box's pending edit (a click on a
    /// button that takes no focus would leave it pending, where Java's focus loss commits it).
    void resetToDefault();

    /// "Save as default" (Java: saveBtn's ActionListener): the preferences take the time step,
    /// the maximum time and the geodetic computation, then prepareForSimulation(), then the seed
    /// (when fixed) and whether it is fixed. It first commits a pending edit, as
    /// resetToDefault().
    void saveAsDefault();

    /// "Configure..." (Java: openLookupDialog()): the aerodynamic lookup dialog of the options
    /// (AerodynamicLookupDialog), a ModalDialog child of the panel's window; returns when it is
    /// closed, then writes the summary again. Nothing once the options have gone.
    void openLookupDialog();

    /// The tooltip of the lookup summary's icon (Java: buildLookupSummaryTooltip()):
    /// "<html>Drag: <summary><br>Stability: <summary></html>", each line escaped.
    [[nodiscard]] static QString lookupSummaryTooltip(const SimulationOptions& options);

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildMethods(BindingScope& scope);
    void buildGravity(BindingScope& scope);
    void buildTimes(BindingScope& scope);
    void buildDefaultButtons();
    void buildExtensions();
    void layOut();

    /// Writes the lookup summary's tooltip again (Java: updateLookupSummary()).
    void updateLookupSummary();
    /// The help icon of the lookup summary, in the palette's colours at the font's size.
    void updateHelpIcon();
    /// Shows the gravity value's row while the model is constant.
    void showGravityValue();
    /// Commits the pending edit of the panel's spin boxes.
    void commitEdits();
    /// The options while their selection is valid; null otherwise.
    [[nodiscard]] SimulationOptions* liveOptions() const;

    std::shared_ptr<Simulation>      m_simulation;
    GuiServices                      m_services;
    std::shared_ptr<ObjectSelection> m_selection;

    QGroupBox*                  m_optionsBox{nullptr};
    QWidget*                    m_grid{nullptr};
    QGridLayout*                m_gridLayout{nullptr};
    QLabel*                     m_calculationLabel{nullptr};
    QLabel*                     m_barrowmanLabel{nullptr};
    QLabel*                     m_stepperLabel{nullptr};
    EnumComboBox*               m_stepperCombo{nullptr};
    QLabel*                     m_lookupLabel{nullptr};
    QPushButton*                m_configureButton{nullptr};
    QLabel*                     m_lookupSummaryIcon{nullptr};
    QLabel*                     m_geodeticLabel{nullptr};
    EnumComboBox*               m_geodeticCombo{nullptr};
    QLabel*                     m_gravityModelLabel{nullptr};
    EnumComboBox*               m_gravityCombo{nullptr};
    ConfigFields::NumberWidgets m_gravityValue;
    ConfigFields::NumberWidgets m_timeStep;
    ConfigFields::NumberWidgets m_maximumTime;
    RandomSeedControls*         m_randomSeed{nullptr};
    QPushButton*                m_resetButton{nullptr};
    QPushButton*                m_saveButton{nullptr};

    QGroupBox*               m_extensionsBox{nullptr};
    DescriptionArea*         m_extensionsDescription{nullptr};
    SimulationExtensionList* m_extensionList{nullptr};

    /// The combo boxes' tooltips and the gravity row follow their bindings, the lookup summary
    /// the options' selection; held here, as they call the panel.
    Signal<>::ScopedConnection                    m_stepperConnection;
    Signal<>::ScopedConnection                    m_geodeticConnection;
    Signal<>::ScopedConnection                    m_gravityConnection;
    BindingContext::PhaseSignal::ScopedConnection m_optionsConnection;
};

}  // namespace QtRocket::Gui
