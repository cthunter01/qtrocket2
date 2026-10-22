#include "QtRocket/gui/main/ComponentButton.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <QEvent>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/tree/ComponentLimits.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using Question = UserPrompts::InsertPositionQuestion;

/// Java's insert position choices (getChoice(key, 2, 0)): ask, insert after, at the end.
constexpr int kAsk         = 0;
constexpr int kInsertAfter = 1;
constexpr int kAtTheEnd    = 2;

/// @p kind, which the factory makes.
ComponentKind checkedKind(ComponentKind kind)
{
    QTROCKET_ASSERT(ComponentFactory::isCreatable(kind));
    return kind;
}

/// askPosition(): the answer to @p question as a choice, CANCEL as kAsk (Java's 0); with "Do not
/// ask me again" ticked an answer that is not a cancel is stored under @p key.
[[nodiscard]] int askPosition(QWidget* parent, Question question, std::string_view key,
                              Preferences& preferences, UserPrompts& prompts)
{
    const UserPrompts::InsertPositionAnswer answer = prompts.askInsertPosition(parent, question);
    int                                     choice = kAsk;
    switch (answer.position)
    {
        case UserPrompts::InsertPosition::INSERT_HERE:
            choice = kInsertAfter;
            break;
        case UserPrompts::InsertPosition::ADD_TO_END:
            choice = kAtTheEnd;
            break;
        case UserPrompts::InsertPosition::CANCEL:
            return kAsk;
    }
    if (answer.doNotAskAgain)
    {
        preferences.putInt(key, choice);
    }
    return choice;
}

/// Java's getChildPosition(): the position of @p child among @p parent's children, -1 when it
/// is not one of them.
[[nodiscard]] long long javaChildPosition(const RocketComponent& parent,
                                          const RocketComponent& child)
{
    const std::optional<std::size_t> position = parent.getChildPosition(&child);
    return position.has_value() ? static_cast<long long>(*position) : -1;
}

/// The place beside @p reference among @p parent's children by the preference @p key, asking
/// @p question when it says so (the second half of BodyComponentButton's and StageButton's
/// getAdditionPosition()); nullopt for a cancel. Both are found again by id after the question.
[[nodiscard]] std::optional<ComponentButton::Position> besideBy(
    Rocket& rocket, const RocketComponent& parent, const RocketComponent& reference,
    std::string_view key, Question question, Preferences& preferences, UserPrompts& prompts,
    QWidget* dialogParent)
{
    const Uuid parentId    = parent.getId();
    const Uuid referenceId = reference.getId();
    int        choice      = preferences.getChoice(key, kAtTheEnd, kAsk);
    if (choice == kAsk)
    {
        const auto last = static_cast<long long>(parent.getChildCount()) - 1;
        choice          = javaChildPosition(parent, reference) == last
                              ? kAtTheEnd
                              : askPosition(dialogParent, question, key, preferences, prompts);
    }
    // The question's dialog runs an event loop: the components are found again.
    RocketComponent*       parentNow    = rocket.findComponent(parentId);
    const RocketComponent* referenceNow = rocket.findComponent(referenceId);
    if (parentNow == nullptr || referenceNow == nullptr)
    {
        return std::nullopt;
    }
    switch (choice)
    {
        case kInsertAfter:
            // -1 + 1 = 0 for a booster set, which is not among the Rocket's children (Java).
            return ComponentButton::Position{.parent = parentNow,
                                             .index  = static_cast<std::size_t>(
                                                 javaChildPosition(*parentNow, *referenceNow) + 1)};
        case kAtTheEnd:
            return ComponentButton::Position{.parent = parentNow, .index = std::nullopt};
        default:
            return std::nullopt;
    }
}

}  // namespace

ComponentButton::ComponentButton(DocumentSelectionModel& selection, const GuiServices& services,
                                 ComponentKind kind, const QString& text, Placement placement,
                                 QWidget* parent)
  : QToolButton(parent),
    m_selection(&selection),
    m_services(services),
    m_kind(checkedKind(kind)),
    m_placement(placement)
{
    setObjectName(QtStrings::fromUtf8(className(kind)));
    // IconTextButton: the icon above the text, both centred; "\n" breaks the line.
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setText(MenuText::escaped(text));
    updateIcon();
    connect(&selection, &DocumentSelectionModel::changed, this,
            guarded([this] { updateEnabled(); }, QStringLiteral("Add component button")));
    connect(
        this, &QToolButton::clicked, this,
        guarded([this] { static_cast<void>(addComponent()); }, QStringLiteral("Add component")));
    updateEnabled();
}

ComponentButton::~ComponentButton() = default;

bool ComponentButton::isAddable(const RocketComponent* selected) const
{
    switch (m_placement)
    {
        case Placement::STAGE:
            return true;
        case Placement::BODY_COMPONENT:
            if (selected != nullptr && isBodyComponent(selected->kind()))
            {
                return true;
            }
            break;
        case Placement::INTO_SELECTED:
            break;
    }
    return selected != nullptr && selected->isCompatible(m_kind);
}

std::optional<ComponentButton::Position> ComponentButton::additionPosition(
    RocketComponent* selected, Preferences& preferences, UserPrompts& prompts)
{
    QTROCKET_ASSERT(isAddable(selected) && !m_selection.isNull());
    Rocket& rocket = m_selection->document().getRocket();
    if (m_placement == Placement::STAGE)
    {
        if (selected == nullptr || selected->kind() == ComponentKind::ROCKET)
        {
            return Position{.parent = &rocket, .index = std::nullopt};
        }
        // The selected stage, or the selected component's (a booster set is a stage).
        const RocketComponent* stage = selected;
        if (!isStage(selected->kind()))
        {
            const AxialStage& own = selected->getStage();
            stage                 = &own;
        }
        return besideBy(rocket, rocket, *stage, PreferenceKeys::kStageInsertPositionKey,
                        Question::STAGE, preferences, prompts, this);
    }
    QTROCKET_ASSERT(selected != nullptr);
    if (selected->isCompatible(m_kind))
    {
        return Position{.parent = selected, .index = std::nullopt};
    }
    // A body component beside a selected body component.
    const RocketComponent* parent = selected->getParent();
    QTROCKET_ASSERT(parent != nullptr);
    return besideBy(rocket, *parent, *selected, PreferenceKeys::kBodyComponentInsertPositionKey,
                    Question::BODY_COMPONENT, preferences, prompts, this);
}

bool ComponentButton::addComponent()
{
    if (m_selection.isNull())
    {
        return false;
    }
    DocumentSelectionModel& selection = *m_selection;
    OpenRocketDocument&     document  = selection.document();
    Rocket&                 rocket    = document.getRocket();
    RocketComponent*        selected  = selection.selectedComponent();
    if (!isAddable(selected))
    {
        return false;
    }
    const std::optional<Position> position =
        additionPosition(selected, m_services.preferences(), m_services.prompts());
    if (!position.has_value() || m_selection.isNull())
    {
        // "No position to add component".
        return false;
    }
    const Uuid parentId = position->parent->getId();

    std::unique_ptr<RocketComponent> component =
        ComponentFactory::create(m_kind, m_services.preferences(), m_services.materials());
    //: Hard-coded English in OpenRocket (ComponentButton): "Add " + getComponentName()
    const QString    description = tr("Add %1").arg(enumText(m_kind));
    RocketComponent* parent      = rocket.findComponent(parentId);
    if (parent == nullptr)
    {
        return false;
    }
    if (const std::optional<UserPrompts::Message> refusal = ComponentLimits::refusal(
            document, ComponentLimits::placing(*component, *parent), description);
        refusal.has_value())
    {
        m_services.prompts().showMessage(this, *refusal);
        return false;
    }
    document.addUndoPosition(QtStrings::toUtf8(description));
    const Uuid id = component->getId();
    if (position->index.has_value())
    {
        parent->addChild(std::move(component), *position->index);
    }
    else
    {
        parent->addChild(std::move(component));
    }

    // The new component alone, and its dialog; both found again, as listeners ran meanwhile.
    if (const RocketComponent* added = rocket.findComponent(id); added != nullptr)
    {
        selection.setSelectedComponent(*added);
    }
    if (RocketComponent* added = rocket.findComponent(id); added != nullptr)
    {
        Q_EMIT addedComponentEditRequested(added);
    }
    return true;
}

void ComponentButton::updateEnabled()
{
    setEnabled(!m_selection.isNull() && isAddable(m_selection->selectedComponent()));
}

QIcon ComponentButton::buttonIcon(ComponentKind kind)
{
    QIcon              normal   = ComponentIcons::largeIcon(kind);
    const QIcon        disabled = ComponentIcons::largeDisabledIcon(kind);
    const QList<QSize> sizes    = normal.availableSizes();
    if (sizes.isEmpty())
    {
        return normal;
    }
    // The PNGs at their own size (Java's ImageIcon), the checkerboard one for the disabled
    // button: QIcon's own disabled look would grey it instead.
    const QSize size = sizes.constFirst();
    QIcon       icon;
    icon.addPixmap(normal.pixmap(size, 1.0), QIcon::Normal);
    icon.addPixmap(disabled.pixmap(size, 1.0), QIcon::Disabled);
    return icon;
}

void ComponentButton::changeEvent(QEvent* event)
{
    QToolButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateIcon();
    }
}

void ComponentButton::updateIcon()
{
    const QIcon icon = buttonIcon(m_kind);
    setIcon(icon);
    const QList<QSize> sizes = icon.availableSizes();
    if (!sizes.isEmpty())
    {
        setIconSize(sizes.constFirst());
    }
}

}  // namespace QtRocket::Gui
