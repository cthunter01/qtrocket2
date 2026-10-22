#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ComponentAssemblyConfig.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/util/Uuid.h"

class QComboBox;
class QRadioButton;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class EnumComboBox;
class GuiServices;
class StyledLabel;
class UnitSpinBox;

/// The configuration of a stage and of a booster set (OpenRocket's AxialStageConfig, swing
/// gui/configdialog/AxialStageConfig.java), a ComponentAssemblyConfig (a booster set's General tab
/// and "Split Boosters" are that class's), with:
///
/// - Separation ("Stage separation options"), first and selected, for a stage whose stage number
///   is above 0 (every stage but the topmost; every booster set): the bold "Select when this stage
///   separates: <dagger>", the event (StageSeparationConfiguration::SeparationEvent, all nine),
///   "plus", the delay (no unit, at least 0) and "seconds", then the note "<dagger> This
///   parameter can be overridden in each flight configuration.". The fields edit the separation
///   of the rocket's selected flight configuration (AxialStage::getSeparationConfiguration()): when
///   that configuration (other than the default one) uses the default separation, a copy of it is
///   stored for the configuration when the panel is made, without an event, as Java's first
///   access does (the separation tab's for the main stage; AxialStage.addConfigListener() for
///   every stage of a multi-component edit, the main one too).
/// - Recovery ("Stage recovery deployment options"), last, for every stage: the bold "Recovery
///   deployment type", "Single Deployment" and "Dual Deployment" (radio buttons, with their
///   tooltips) and "Drogue device" with a combo box of the recovery devices of the stage's
///   subtree (the stage and its booster sets: Java iterates the stage), by name in tree order, or
///   the one item "No recovery devices in this stage". Dual is selected when a device of the
///   subtree is a drogue (the combo box then shows the first such device), and disabled when
///   there is no device; the combo box is enabled while Dual is selected. Single clears the drogue
///   flag (RecoveryDevice::setDrogue()) of every device of the subtree; Dual and a device chosen
///   while Dual is selected clear them and make the chosen device the drogue. The devices' own
///   panels show the flag read-only (RecoveryDeviceConfig). Java sets the tab's state once, when
///   the panel is made, and so does this panel.
///
/// Every field reaches every selected stage (the user's decision U3, ComponentSelection): each
/// gets the edit on its own selected separation (Java links the stages' selected separations,
/// whose setters forward first). The Recovery tab acts on the main stage alone, as Java's.
///
/// Deviations from OpenRocket:
/// - The user's decision U4: an edit of the separation fires EVENT_CHANGE on the stage whenever the
///   configuration changed, as rocket/StageSeparationConfiguration.h prescribes, so the dialog
///   counts as modified, the document as changed, and Cancel undoes the edit with the session's
///   undo step. Java's StageSeparationConfiguration notifies nobody (fireChangeEvent() is empty;
///   D2cProbe: the rocket's modification id stays the same).
/// - The fields show and edit the separation of the configuration selected at the time; Java's
///   fields keep the object of the configuration selected when the panel was made.
/// - The Recovery tab clears the flags and sets the new drogue inside one freeze of the rocket
///   (taken only when it is not frozen already), so that the rocket fires one merged event; Java's
///   setDrogue() calls fire one each.
class AxialStageConfig : public ComponentAssemblyConfig
{
    Q_OBJECT

public:
    /// The Separation tab's fields (see the class comment); all null without the tab.
    struct SeparationWidgets
    {
        QWidget*      page  = nullptr;
        StyledLabel*  title = nullptr;
        EnumComboBox* event = nullptr;
        UnitSpinBox*  delay = nullptr;
        StyledLabel*  note  = nullptr;
    };

    /// The Recovery tab's widgets (see the class comment).
    struct RecoveryWidgets
    {
        QWidget*      page    = nullptr;
        StyledLabel*  title   = nullptr;
        QRadioButton* single  = nullptr;
        QRadioButton* dual    = nullptr;
        QComboBox*    devices = nullptr;
    };

    /// The configuration of the stages of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is no stage
    AxialStageConfig(OpenRocketDocument& document, const GuiServices& services,
                     std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const SeparationWidgets& separationWidgets() const noexcept
    {
        return m_separation;
    }
    [[nodiscard]] const RecoveryWidgets& recoveryWidgets() const noexcept { return m_recovery; }

    /// The ids of the recovery devices the combo box offers, in its order.
    [[nodiscard]] const std::vector<Uuid>& recoveryDevices() const noexcept { return m_devices; }

    /// Single Deployment chosen (Java: the radio button's action): no device of the main stage's
    /// subtree is a drogue.
    void chooseSingleDeployment();
    /// Dual Deployment chosen: the device the combo box shows becomes the only drogue; without a
    /// device Single is selected again.
    void chooseDualDeployment();
    /// The device @p index of the combo box chosen: while Dual is selected it becomes the only
    /// drogue.
    void chooseDrogueDevice(int index);

    /// The event of a stage's separation in the rocket's selected configuration (Java:
    /// EnumModel(sepConfig, "SeparationEvent")); its setter fires EVENT_CHANGE (U4).
    [[nodiscard]] static EnumSource<StageSeparationConfiguration::SeparationEvent>
    separationEventSource();
    /// The delay of a stage's separation in the rocket's selected configuration (Java:
    /// DoubleModel(sepConfig, "SeparationDelay", 0)); its setter fires EVENT_CHANGE (U4).
    [[nodiscard]] static DoubleSource separationDelaySource();

private:
    void addSeparationTab();
    void addRecoveryTab();
    /// Clears the drogue flag of every device of the main stage's subtree, then makes the device
    /// @p index of recoveryDevices() a drogue (none for nullopt).
    void setDrogue(std::optional<std::size_t> index);

    SeparationWidgets m_separation;
    RecoveryWidgets   m_recovery;
    std::vector<Uuid> m_devices;
};

}  // namespace QtRocket::Gui
