#pragma once

#include <cstddef>
#include <vector>

#include <QIcon>
#include <QObject>
#include <QWidget>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Uuid.h"

class QHBoxLayout;
class QToolButton;

namespace QtRocket
{
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The stage buttons of the design view's ribbon (OpenRocket's StageSelector): a toggle button
/// per stage of the rocket (AxialStage and ParallelStage, in tree order), checked while the stage
/// is active in the rocket's selected flight configuration. Clicking one toggles the stage there.
///
/// - A button shows the stage's name ('&' shown as itself, not as a mnemonic). It is disabled,
///   in italics and without an icon, when the stage has no children (tooltip
///   RocketPanel.btn.Stages.NoChildren.ttip; such a stage is never active) or when it is the
///   only active stage (tooltip RocketPanel.btn.Stages.LastActive.ttip). Otherwise its tooltip
///   is RocketPanel.btn.Stages.Toggle.ttip, and an inactive stage's button shows the icon
///   COMPONENT_DISABLED. The buttons are equally high (Java's size group "sgy stagebuttons")
///   and packed at the left: the selector does not grow in a layout (horizontal size policy
///   Maximum), and a wider selector leaves the room to the right of its buttons.
/// - A click calls toggleStage(): FlightConfiguration::toggleStage() on the selected
///   configuration, then the rocket's change event kToggleChange for that configuration
///   (StageSelector.java:127-131). No undo position is added: a toggle is not an undoable edit
///   of its own, as in OpenRocket (decision O17). A selection of stages for which the force
///   analysis cannot be made (an active stage below two inactive ones) is allowed, as in Java:
///   BarrowmanCalculator::getForceAnalysis() and ComponentDrag answer it with an ordinary Error.
/// - The buttons follow the rocket: every change event updates them at once (updateButtons()).
///   They keep the stages' ids, never the stage objects (an undo or a redo replaces every
///   component): a click looks its stage up by id, and a stage that is gone updates the buttons
///   and toggles nothing.
///
/// The rocket must outlive the selector.
///
/// Deviations from OpenRocket:
/// - The selector listens to the rocket itself (RocketPanel adds Java's to the rocket's
///   listeners), and it updates the buttons on every change event, where Java updates them
///   only for events whose source is the rocket, a stage or a body tube. The merged event of
///   Rocket::thaw() comes from the last queued source, so Java can miss a stage added or
///   renamed inside a freeze; the result is otherwise the same.
/// - Java makes new buttons at every update; here the buttons are kept and changed, so that a
///   button is never deleted while its click is being delivered. Buttons no longer needed are
///   deleted later (deleteLater()).
/// - The buttons are QToolButtons with the text beside the icon (Java: JToggleButtons, text to
///   the right of the icon).
class StageSelector final : public QWidget
{
    Q_OBJECT

public:
    /// The change event a toggle fires: AEROMASS_CHANGE | MOTOR_CHANGE | TREE_CHANGE_CHILDREN.
    static constexpr int kToggleChange = ComponentChangeEvent::kAeromassChange |
                                         ComponentChangeEvent::kMotorChange |
                                         ComponentChangeEvent::kTreeChangeChildren;

    /// The buttons of @p rocket's stages, kept up to date with the rocket's change events.
    StageSelector(Rocket& rocket, QWidget* parent);

    [[nodiscard]] Rocket& rocket() const noexcept { return *m_rocket; }

    /// The number of buttons: the stages of the rocket at the last update.
    [[nodiscard]] std::size_t buttonCount() const noexcept { return m_buttons.size(); }

    /// The button of the stage at @p index (tree order).
    /// @throws BugError when @p index is not below buttonCount()
    [[nodiscard]] QToolButton* button(std::size_t index) const;

    /// The id of the stage of the button at @p index.
    /// @throws BugError when @p index is not below buttonCount()
    [[nodiscard]] const Uuid& stageId(std::size_t index) const;

    /// Brings the buttons up to date with the rocket and its selected configuration
    /// (updateButtons()). Every change event of the rocket calls it.
    void updateButtons();

    /// What a click on the button of the stage numbered @p stageNumber does
    /// (StageAction.actionPerformed()): toggles the stage in @p rocket's selected configuration
    /// (its sub-stages take its new state), then fires kToggleChange for that configuration.
    /// An unknown stage number toggles nothing, but the event is fired all the same, as in Java.
    static void toggleStage(Rocket& rocket, int stageNumber);

private:
    /// The click on the button at @p index.
    void buttonClicked(std::size_t index);

    /// Makes the buttons @p count: new ones at the end, the ones beyond it removed.
    void resizeButtons(std::size_t count);

    /// Java's size group: every button as high as the highest.
    void equalizeHeights() const;

    Rocket*                                 m_rocket;
    QHBoxLayout*                            m_layout;
    QIcon                                   m_inactiveIcon;
    std::vector<QToolButton*>               m_buttons;
    std::vector<Uuid>                       m_stageIds;
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
};

}  // namespace QtRocket::Gui
