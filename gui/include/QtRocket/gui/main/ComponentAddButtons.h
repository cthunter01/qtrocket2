#pragma once

#include <vector>

#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/rocket/ComponentKind.h"
// RocketComponent is complete here, not declared: moc's metatype of a signal's
// RocketComponent* checks for completeness, and GCC's -Wsfinae-incomplete rejects a class that
// another moc file of the same compilation sees complete later.
#include "QtRocket/rocket/RocketComponent.h"

class QEvent;
class QLabel;
class QVBoxLayout;

namespace QtRocket::Gui
{

class DocumentSelectionModel;
class FlowLayout;

/// The buttons that add new components to the design, in four groups (OpenRocket's
/// ComponentAddButtons, the right half of the design tab under "Add new component"), each a
/// label and its buttons (ComponentButton) in a row that wraps to the panel's width:
/// - "Assembly Components": Stage, Boosters, Pods;
/// - "Body Components and Fin Sets": Nose Cone, Body Tube, Transition, Trapezoidal, Elliptical,
///   Freeform, Tube Fins, Rail Button, Launch Lug;
/// - "Inner Components": Inner Tube, Tube Coupler, Centering Ring, Bulkhead, Engine Block;
/// - "Mass Components": Parachute, Streamer, Shock Cord, Mass Component.
/// Nose Cone, Body Tube and Transition place as body components, Stage as a stage, the others
/// into the selected component (ComponentButton::Placement). Every button has the size of the
/// largest (uniformButtonSizes()), with kButtonGap between buttons and between the rows of a
/// group (MigLayout's "[]9[]" and "gapright 9"), and a larger gap above each group but the first
/// ("gaptop unrel").
///
/// The panel is meant for a vertically scrolling QScrollArea that resizes it to its width
/// (DesignPanel's scroll pane titled "Add new component", with no horizontal scroll bar): the
/// rows' height follows the width (FlowLayout's height for width).
///
/// Deviations from OpenRocket: the rows wrap where the next button would pass the panel's right
/// edge (FlowLayout), where Java computes the buttons per row from its viewport's width less 35
/// pixels; the buttons' sizes follow the platform style (with its padding, Java sets 6 px) and
/// are made the same again when the style or the font changes.
class ComponentAddButtons final : public QWidget
{
    Q_OBJECT

public:
    /// The space between two buttons, and between two rows of a group (BUTTON_GAP).
    static constexpr int kButtonGap = 9;
    /// The extra space above a group's label, but the first (MigLayout's unrelated gap).
    static constexpr int kGroupGap = 12;

    /// The buttons for the components @p selection selects (which must outlive the panel or be
    /// destroyed with it), making the components with @p services' preferences and materials.
    ComponentAddButtons(DocumentSelectionModel& selection, const GuiServices& services,
                        QWidget* parent = nullptr);

    /// Every button, in the order of the groups and within each group.
    [[nodiscard]] const std::vector<ComponentButton*>& buttons() const noexcept
    {
        return m_buttons;
    }

    /// The button of @p kind; null for ROCKET.
    [[nodiscard]] ComponentButton* button(ComponentKind kind) const;

    /// The size of every button: the largest size hint among them.
    [[nodiscard]] QSize buttonSize() const noexcept { return m_buttonSize; }

    /// The groups' labels, in their order.
    [[nodiscard]] QStringList groupTitles() const;

    /// The title of the scroll pane around the panel ("Add new component",
    /// BasicFrame.title.Addnewcomp; bold in OpenRocket's DesignPanel).
    [[nodiscard]] static QString title();

Q_SIGNALS:
    /// A button added @p component: open its configuration dialog, as for a new component
    /// (ComponentConfigDialog.showDialog(..., false, true)). The pointer is valid during the
    /// emission only: connect directly.
    void addedComponentEditRequested(QtRocket::RocketComponent* component);

protected:
    /// Makes the buttons the same size again after a change of the style or the font.
    void changeEvent(QEvent* event) override;

private:
    /// What a button is made of.
    struct ButtonSpec
    {
        ComponentKind              kind;
        QString                    text;
        ComponentButton::Placement placement;
    };

    /// Adds a group titled @p title with the buttons @p specs to @p layout
    /// (addButtonGroup()).
    void addGroup(QVBoxLayout& layout, const QString& title, const std::vector<ButtonSpec>& specs,
                  bool first);
    /// Gives every button the largest size hint among them (uniformButtonSizes()).
    void uniformButtonSizes();

    DocumentSelectionModel*       m_selection;
    GuiServices                   m_services;
    std::vector<ComponentButton*> m_buttons;
    std::vector<QLabel*>          m_titles;
    /// The rows of the groups' buttons.
    std::vector<FlowLayout*> m_rows;
    QSize                    m_buttonSize;
};

}  // namespace QtRocket::Gui
