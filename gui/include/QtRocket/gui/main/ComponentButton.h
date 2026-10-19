#pragma once

#include <cstddef>
#include <optional>

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QToolButton>

#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/rocket/ComponentKind.h"
// RocketComponent is complete here, not declared: moc's metatype of a signal's
// RocketComponent* checks for completeness, and GCC's -Wsfinae-incomplete rejects a class that
// another moc file of the same compilation sees complete later.
#include "QtRocket/rocket/RocketComponent.h"

class QEvent;
class QWidget;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DocumentSelectionModel;
class UserPrompts;

/// One of the buttons that add a new component to the design (OpenRocket's ComponentButton, with
/// its subclasses BodyComponentButton and StageButton of ComponentAddButtons as placements): the
/// large icon of its component kind above its text, the disabled icon (ComponentIcons'
/// checkerboard) while it is disabled.
///
/// It is enabled by the first selected component of the window's selection (the first selected
/// path of Java's tree selection model; nothing selected counts as null), re-evaluated at every
/// change of the selection (isAddable()):
/// - INTO_SELECTED (ComponentButton): when that component takes a component of the kind
///   (RocketComponent::isCompatible());
/// - BODY_COMPONENT (BodyComponentButton: nose cone, body tube, transition): also when it is a
///   body component;
/// - STAGE (StageButton): always.
///
/// Where the new component goes (additionPosition(), getAdditionPosition()):
/// - INTO_SELECTED, and BODY_COMPONENT when the selected component takes the kind: the end of
///   the selected component;
/// - BODY_COMPONENT on a body component that does not take it: by the preference
///   BodyComponentInsertPosition (0 ask, 1 insert after it, 2 add at the end of its parent;
///   another value counts as 0): right after it in its parent, or at the end of its parent; asked
///   (UserPrompts::askInsertPosition(), BODY_COMPONENT), the answer "Insert here" is the first,
///   "Add to the end" the second, Cancel (or closing) adds nothing; not asked when it is its
///   parent's last child (the end, then). With "Do not ask me again" ticked the answer (1 or 2,
///   not a cancel) is stored as the preference;
/// - STAGE: with nothing or the Rocket selected the end of the Rocket; otherwise relative to the
///   selected stage or the stage of the selected component (a booster set is a stage), among
///   the Rocket's children, by the preference StageInsertPosition, asked as above (STAGE), not
///   when that stage is the Rocket's last child.
///
/// A click (addComponent(), fireActionPerformed()) makes the component with ComponentFactory
/// from the application's preferences and materials, adds the undo position "Add <component
/// name>", adds the component there, selects it alone and asks for its configuration dialog
/// (addedComponentEditRequested(): OpenRocket opens it with isNewComponent and without the
/// remembered tab). A component that would give the rocket more components than QtRocket opens
/// again (DocumentConfig::kMaxComponents) is refused with a message before anything changes
/// (decision O27, ComponentLimits).
///
/// No component pointer is kept: the selected component and the parent are found again by id
/// after the question (whose dialog runs an event loop) and after the addition.
///
/// Deviations from OpenRocket:
/// - A booster set or a component in one: its stage is the booster set (Java's getStage()),
///   which is not a child of the Rocket (Java's getChildPosition() -1). Java then always asks,
///   and "insert after" (Insert here, or the preference 1) puts the new stage first, at index 0;
///   kept as Java does it (the M2 run 2 probe T4Probe).
/// - BodyComponentButton's branch for nothing or the Rocket selected (the last stage's end) is
///   not ported: the button is disabled then, so Java never takes it.
/// - The button's padding is the platform style's (Java sets a 6 px margin); its texts are
///   escaped (a '&' of a translation stays a character, not a mnemonic).
/// - The bound (decision O27) is QtRocket's.
class ComponentButton final : public QToolButton
{
    Q_OBJECT

public:
    /// How the button places its component (which of Java's button classes it is).
    enum class Placement
    {
        /// ComponentButton: into the selected component, which must take it.
        INTO_SELECTED,
        /// BodyComponentButton: also beside a selected body component.
        BODY_COMPONENT,
        /// StageButton: among the Rocket's stages.
        STAGE,
    };

    /// Where a new component goes: into @p parent at @p index, or at its end (nullopt; Java's
    /// null index).
    struct Position
    {
        RocketComponent*           parent = nullptr;
        std::optional<std::size_t> index;
    };

    /// The button of @p kind (ComponentFactory::isCreatable()) with the text @p text ('\n'
    /// breaks the line) that places by @p placement, for the components @p selection selects,
    /// making them with @p services' preferences and materials and asking through its prompts.
    /// @p selection must outlive the button or be destroyed with it.
    /// @throws BugError for ROCKET.
    ComponentButton(DocumentSelectionModel& selection, const GuiServices& services,
                    ComponentKind kind, const QString& text, Placement placement, QWidget* parent);
    ~ComponentButton() override;

    ComponentButton(const ComponentButton&)            = delete;
    ComponentButton& operator=(const ComponentButton&) = delete;
    ComponentButton(ComponentButton&&)                 = delete;
    ComponentButton& operator=(ComponentButton&&)      = delete;

    [[nodiscard]] ComponentKind kind() const noexcept { return m_kind; }
    [[nodiscard]] Placement     placement() const noexcept { return m_placement; }

    /// Whether the button adds a component when @p selected (null for none) is the first
    /// selected component (isAddable()).
    [[nodiscard]] bool isAddable(const RocketComponent* selected) const;

    /// Where the component goes when @p selected (null for none), for which isAddable() holds,
    /// is the first selected component, by @p preferences, asking through @p prompts when they
    /// say so (getAdditionPosition()); nullopt when the user cancels (or @p selected leaves the
    /// rocket while the question is open).
    /// @throws BugError when isAddable(@p selected) does not hold.
    [[nodiscard]] std::optional<Position> additionPosition(RocketComponent* selected,
                                                           Preferences&     preferences,
                                                           UserPrompts&     prompts);

    /// What a click does (fireActionPerformed()); true when a component was added.
    bool addComponent();

    /// Enables the button for the selection as it is now.
    void updateEnabled();

    /// The icon of @p kind's buttons: the large icon of the current palette, and the
    /// checkerboard large icon for the disabled button.
    [[nodiscard]] static QIcon buttonIcon(ComponentKind kind);

Q_SIGNALS:
    /// A component was added: open its configuration dialog, as for a new component. The
    /// pointer is valid during the emission only: connect directly.
    void addedComponentEditRequested(QtRocket::RocketComponent* component);

protected:
    /// Takes the icons of a new palette (the light or dark files).
    void changeEvent(QEvent* event) override;

private:
    /// Sets the icon and its size.
    void updateIcon();

    QPointer<DocumentSelectionModel> m_selection;
    GuiServices                      m_services;
    ComponentKind                    m_kind;
    Placement                        m_placement;
};

}  // namespace QtRocket::Gui
