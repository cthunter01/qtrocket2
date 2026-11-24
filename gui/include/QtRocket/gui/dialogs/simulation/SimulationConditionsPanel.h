#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/util/Signal.h"

class QEvent;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QStackedWidget;

namespace QtRocket
{
class SimulationOptions;
class SimulationOptionsInterface;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class BoolCheckBox;
class FieldValidator;
class ObjectSelection;
class WindLevelSummary;

/// The launch conditions (OpenRocket's SimulationConditionsPanel, swing gui/simulation/): the
/// "Launch conditions" tab of the simulation settings dialog, and the conditions of the Launch
/// preference page. Its fields edit a SimulationOptionsInterface live, as Java's DoubleModels and
/// BooleanModels do: a simulation's SimulationOptions (the dialog; it adds its undo position when
/// it opens, and Cancel undoes) or the launch preferences (LaunchPreferences, the page).
///
/// What it shows (Java: addSimulationConditionsPanel(), in two columns):
/// - "Wind" (windPanel): with Variant::SIMULATION the choice "Wind model to use:" between
///   "Average" and "Multi-level" (radio buttons, which set SimulationOptions::setWindModelType()
///   and follow it when the options change elsewhere, as "Reset to default" does), a line, and the
///   card of the model chosen: the average wind's fields, or the multi-level wind's summary with
///   its "Edit levels" button (WindLevelSummary); with Variant::LAUNCH_PREFERENCES the average
///   wind's fields alone. The average wind's fields edit the target's PinkNoiseWindModel
///   (getAverageWindModel()): "Average windspeed:" (Average), "Standard deviation:"
///   (StandardDeviation, at most the average speed, its slider ending there), "Turbulence
///   intensity:" (TurbulenceIntensity), the intensity's description below its spin box
///   (TurbulenceIntensityText), and "Wind direction:" (Direction).
/// - "Atmospheric conditions" (atmospherePanel): "Use International Standard Atmosphere"
///   (ISAAtmosphere), and "Temperature:", "Pressure:" and "Relative humidity:"
///   (LaunchTemperature, LaunchPressure, LaunchRelativeHumidity), each row disabled while the
///   standard atmosphere is in use.
/// - "Launch site" (launchSitePanel): "Latitude:", "Longitude:" and "Altitude:"
///   (LaunchLatitude, LaunchLongitude, LaunchAltitude).
/// - "Launch rod" (launchRodPanel): "Length:" (LaunchRodLength), "Always launch directly up-wind or
///   down-wind" (LaunchIntoWind), "Angle:" (LaunchRodAngle) and "Direction:"
///   (LaunchRodDirection), which is disabled while launching into the wind and then shows the
///   wind's direction (SimulationOptionsInterface::getLaunchRodDirection()).
/// - with Variant::SIMULATION, below them on the right, "Reset to default" (restoreDefaults: the
///   options take DefaultSimulationOptionFactory::getDefault(), everything copyConditionsFrom()
///   copies, as Java) and "Save as default" (saveDefaults: DefaultSimulationOptionFactory::
///   saveDefault()); each first commits a spin box's pending edit (a click on a button that takes
///   no focus would leave it pending, where Java's focus loss commits it).
/// A number row is a label, a spin box, a unit selector and a slider (ConfigFields), the three
/// fields named after the Java property (objectName), with Java's unit group, slider and tooltips
/// (the average wind's rows: the spin box's alone, as Java's addDoubleModelRow()).
///
/// The bounds: Java's minima, and its maxima where it has them (the altitude's 11018.06 m,
/// ExtendedIsaModel::getMaximumAllowedAltitude(); the latitude, longitude, rod angle, the
/// directions, the turbulence intensity 1, the deviation's average); where Java's are open, the
/// maxima of the simulation's input ranges (SimulationInputRanges: the average wind 1000 m/s, the
/// temperature 1000 K, the pressure 1000000 Pa, the humidity 1, the rod length 1000 m; the user's
/// decision U-M4-7), so that a value typed cannot be one the run refuses for being too large.
/// The rod direction has no minimum, as in Java, whose DoubleModel is given the Integer 0 for it
/// and takes an Object that is no Double as no bound (the setter turns every direction into
/// 0 ... 360 degrees: "-90" is 270 degrees).
///
/// The validation (FieldValidator, Java's FlatLafOutlines, each with its message popup):
/// - the average wind speed: an error from 20 m/s ("Extremely high wind speed"), a warning from
///   10 m/s ("High wind speed");
/// - with the standard atmosphere off, the temperature: errors at or below 193.15 K ("Extremely low
///   temperature") and at or above 353.15 K ("Extremely high temperature"), warnings at or below
///   233.15 K ("Low temperature") and at or above 323.15 K ("High temperature");
/// - with the standard atmosphere off, the pressure: a warning when it is below 0.75 or above 1.25
///   times the standard atmosphere's pressure at the launch altitude ("Low pressure", "High
///   pressure");
/// - the rod angle: a warning from 45 degrees either way ("Large launch rod angle"), an error from
///   60 degrees ("Extremely large launch rod angle").
/// OpenRocket bug 35, fixed (Milestone 4): the message of the lowest temperatures is the key
/// simedtdlg.error.temperatureTooLow, which no messages.properties of OpenRocket holds: its
/// translator reports an internal error the first time (the bug report dialog) and the outline's
/// tooltip shows the key. QtRocket shows "Extremely low temperature", after its sibling
/// simedtdlg.error.temperatureTooHigh, "Extremely high temperature".
///
/// Java's easter egg, ported (the orchestrator's decision O-M4-19): typing into the turbulence
/// intensity's spin box so that its text and the typed character make "42" shows the message
/// "The answer to the ultimate question of life, the universe, and everything." titled "42"
/// (Java's literals, UserPrompts::showMessage()).
///
/// Lifetime: the bindings and the selections of the target and of its average wind model are kept
/// by @p scope (BindingScope::make(), keepContext()), which the dialog or page that owns it
/// commits (BindingScope::commitAll()) and releases when it closes; from then on the fields write
/// nothing. The panel may go before or after the scope. The target must stay at its address while
/// the panel shows it (a simulation's options do, for the life of the simulation; hold the
/// simulation by its std::shared_ptr); should it go before the scope is released, the selections
/// learn of it from its change signal: from then on the fields, the wind model's buttons and the
/// default buttons write nothing and read nothing (the fields keep what they showed, as nothing
/// tells them), and neither does the multi-level summary (WindLevelSummary).
///
/// Deviations from OpenRocket:
/// - The maxima of the spin boxes where Java has none (see above).
/// - The wind editor (MultiLevelWindEditDialog) is a ModalDialog child of the panel's window,
///   opened by editWindLevels() after editWindLevelsRequested() (see there).
/// - Variant::SIMULATION needs a SimulationOptions (a BugError otherwise); Java's static method
///   takes the wind model radios for any target and makes them do nothing for one that is not.
/// - The multi-level summary is written again on every change of the levels (OpenRocket bug 36,
///   WindLevelSummary).
class SimulationConditionsPanel final : public QWidget
{
    Q_OBJECT

public:
    /// Which of Java's two panels.
    enum class Variant
    {
        /// The settings dialog's tab (Java: SimulationConditionsPanel(simulation)): the wind
        /// model choice with both cards, and "Reset to default" and "Save as default". The target
        /// is a SimulationOptions.
        SIMULATION,
        /// The Launch preference page's (Java: addSimulationConditionsPanel(page, preferences,
        /// false)): the average wind alone, no buttons.
        LAUNCH_PREFERENCES,
    };

    /// The launch conditions of @p target, as @p variant has them; @p services give the
    /// preferences of the default buttons and the prompts of the easter egg; @p scope keeps the
    /// bindings (see the class comment).
    /// @throws BugError for Variant::SIMULATION with a target that is no SimulationOptions
    SimulationConditionsPanel(SimulationOptionsInterface& target, Variant variant,
                              const GuiServices& services, BindingScope& scope, QWidget* parent);
    ~SimulationConditionsPanel() override;
    SimulationConditionsPanel(const SimulationConditionsPanel&)            = delete;
    SimulationConditionsPanel& operator=(const SimulationConditionsPanel&) = delete;
    SimulationConditionsPanel(SimulationConditionsPanel&&)                 = delete;
    SimulationConditionsPanel& operator=(SimulationConditionsPanel&&)      = delete;

    [[nodiscard]] Variant variant() const noexcept { return m_variant; }

    // ------------------------------------------------------------------------------- the wind

    [[nodiscard]] QGroupBox& windBox() const noexcept { return *m_windBox; }
    /// "Average" and "Multi-level" (objectNames averageButton, multiLevelButton); null with
    /// Variant::LAUNCH_PREFERENCES.
    [[nodiscard]] QRadioButton* averageWindButton() const noexcept { return m_averageButton; }
    [[nodiscard]] QRadioButton* multiLevelWindButton() const noexcept { return m_multiLevelButton; }
    /// The two cards (windSettingsPanel): the average wind's fields at index 0, the multi-level
    /// summary at index 1; null with Variant::LAUNCH_PREFERENCES.
    [[nodiscard]] QStackedWidget* windCards() const noexcept { return m_windCards; }
    /// The multi-level wind's card (multiLevelPanel); null with Variant::LAUNCH_PREFERENCES.
    [[nodiscard]] WindLevelSummary* windLevelSummary() const noexcept { return m_windLevelSummary; }

    [[nodiscard]] const ConfigFields::NumberWidgets& averageWind() const noexcept
    {
        return m_averageWind;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& standardDeviation() const noexcept
    {
        return m_standardDeviation;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& turbulenceIntensity() const noexcept
    {
        return m_turbulenceIntensity;
    }
    /// The description of the turbulence intensity (intensityLabel).
    [[nodiscard]] QLabel& intensityLabel() const noexcept { return *m_intensityLabel; }
    [[nodiscard]] const ConfigFields::NumberWidgets& windDirection() const noexcept
    {
        return m_windDirection;
    }

    // ------------------------------------------------------------------------- the atmosphere

    [[nodiscard]] QGroupBox&    atmosphereBox() const noexcept { return *m_atmosphereBox; }
    [[nodiscard]] BoolCheckBox& isaCheckBox() const noexcept { return *m_isaCheckBox; }
    [[nodiscard]] const ConfigFields::NumberWidgets& temperature() const noexcept
    {
        return m_temperature;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& pressure() const noexcept
    {
        return m_pressure;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& relativeHumidity() const noexcept
    {
        return m_relativeHumidity;
    }

    // ------------------------------------------------------------------------ the launch site

    [[nodiscard]] QGroupBox& launchSiteBox() const noexcept { return *m_launchSiteBox; }
    [[nodiscard]] const ConfigFields::NumberWidgets& latitude() const noexcept
    {
        return m_latitude;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& longitude() const noexcept
    {
        return m_longitude;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& altitude() const noexcept
    {
        return m_altitude;
    }

    // ------------------------------------------------------------------------- the launch rod

    [[nodiscard]] QGroupBox& launchRodBox() const noexcept { return *m_launchRodBox; }
    [[nodiscard]] const ConfigFields::NumberWidgets& rodLength() const noexcept
    {
        return m_rodLength;
    }
    [[nodiscard]] BoolCheckBox& intoWindCheckBox() const noexcept { return *m_intoWindCheckBox; }
    [[nodiscard]] const ConfigFields::NumberWidgets& rodAngle() const noexcept
    {
        return m_rodAngle;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& rodDirection() const noexcept
    {
        return m_rodDirection;
    }

    // ---------------------------------------------------------------------------- the buttons

    /// "Reset to default" and "Save as default"; null with Variant::LAUNCH_PREFERENCES.
    [[nodiscard]] QPushButton* resetButton() const noexcept { return m_resetButton; }
    [[nodiscard]] QPushButton* saveButton() const noexcept { return m_saveButton; }

    /// The validators of the average wind speed, the temperature, the pressure and the rod
    /// angle (see the class comment).
    [[nodiscard]] FieldValidator& windSpeedValidator() const noexcept { return *m_windValidator; }
    [[nodiscard]] FieldValidator& temperatureValidator() const noexcept
    {
        return *m_temperatureValidator;
    }
    [[nodiscard]] FieldValidator& pressureValidator() const noexcept
    {
        return *m_pressureValidator;
    }
    [[nodiscard]] FieldValidator& rodAngleValidator() const noexcept
    {
        return *m_rodAngleValidator;
    }

    /// The easter egg (see the class comment); the events always go on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// What "Edit levels" does (Java: new MultiLevelWindEditDialog(window,
    /// model).setVisible(true)): the wind editor of the options' MultiLevelPinkNoiseWindModel,
    /// modal for the application, as a ModalDialog child of the panel's window; returns when it is
    /// closed (the summary has followed every change: WindLevelSummary). Nothing with
    /// Variant::LAUNCH_PREFERENCES or once the options have gone.
    void editWindLevels();

Q_SIGNALS:
    /// The user clicked the multi-level card's "Edit levels" (WindLevelSummary::
    /// editLevelsRequested()); the panel then opens the wind editor (editWindLevels()).
    void editWindLevelsRequested();

private:
    /// The selections the bindings are made over.
    struct Contexts
    {
        std::shared_ptr<ObjectSelection> target;
        std::shared_ptr<ObjectSelection> averageWind;
    };

    void buildWind(const Contexts& contexts, BindingScope& scope);
    void buildWindModelChoice(QWidget& parent);
    void buildAverageWind(QWidget& card, const Contexts& contexts, BindingScope& scope);
    void buildAtmosphere(const Contexts& contexts, BindingScope& scope);
    void buildLaunchSite(const Contexts& contexts, BindingScope& scope);
    void buildLaunchRod(const Contexts& contexts, BindingScope& scope);
    void buildValidators();
    void buildDefaultButtons();
    void layOut();

    /// Shows the radio button and the card of the options' wind model (Java:
    /// updateWindModelSelection()).
    void showWindModel();
    /// The user chose a wind model (Java: the radio buttons' ActionListeners).
    void chooseWindModel(bool multiLevel);
    /// Commits the pending edit of the panel's spin boxes.
    void commitEdits();
    void resetToDefault();
    void saveAsDefault();
    /// The options of Variant::SIMULATION while their selection is valid; null otherwise.
    [[nodiscard]] SimulationOptions* liveOptions() const noexcept;

    Variant                          m_variant;
    GuiServices                      m_services;
    SimulationOptions*               m_options{nullptr};
    std::shared_ptr<ObjectSelection> m_targetSelection;

    QGroupBox*                  m_windBox{nullptr};
    QRadioButton*               m_averageButton{nullptr};
    QRadioButton*               m_multiLevelButton{nullptr};
    QStackedWidget*             m_windCards{nullptr};
    WindLevelSummary*           m_windLevelSummary{nullptr};
    ConfigFields::NumberWidgets m_averageWind;
    ConfigFields::NumberWidgets m_standardDeviation;
    ConfigFields::NumberWidgets m_turbulenceIntensity;
    QLabel*                     m_intensityLabel{nullptr};
    ConfigFields::NumberWidgets m_windDirection;

    QGroupBox*                  m_atmosphereBox{nullptr};
    BoolCheckBox*               m_isaCheckBox{nullptr};
    ConfigFields::NumberWidgets m_temperature;
    ConfigFields::NumberWidgets m_pressure;
    ConfigFields::NumberWidgets m_relativeHumidity;

    QGroupBox*                  m_launchSiteBox{nullptr};
    ConfigFields::NumberWidgets m_latitude;
    ConfigFields::NumberWidgets m_longitude;
    ConfigFields::NumberWidgets m_altitude;

    QGroupBox*                  m_launchRodBox{nullptr};
    ConfigFields::NumberWidgets m_rodLength;
    BoolCheckBox*               m_intoWindCheckBox{nullptr};
    ConfigFields::NumberWidgets m_rodAngle;
    ConfigFields::NumberWidgets m_rodDirection;

    QPushButton* m_resetButton{nullptr};
    QPushButton* m_saveButton{nullptr};

    FieldValidator* m_windValidator{nullptr};
    FieldValidator* m_temperatureValidator{nullptr};
    FieldValidator* m_pressureValidator{nullptr};
    FieldValidator* m_rodAngleValidator{nullptr};

    /// The intensity's description follows the turbulence intensity's binding, the wind model's
    /// choice the target's selection; held here, as they call the panel.
    Signal<>::ScopedConnection                    m_intensityConnection;
    BindingContext::PhaseSignal::ScopedConnection m_windModelConnection;
};

}  // namespace QtRocket::Gui
