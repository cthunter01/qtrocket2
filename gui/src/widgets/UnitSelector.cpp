#include "QtRocket/gui/widgets/UnitSelector.h"

#include <memory>
#include <utility>

#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
[[nodiscard]] std::shared_ptr<DoubleBinding> checkedBinding(std::shared_ptr<DoubleBinding> binding)
{
    if (binding == nullptr)
    {
        bug("A unit selector needs a binding");
    }
    return binding;
}

/// The symbol of @p unit as a button's or a menu item's text, which would take an '&' for the
/// mark of a shortcut.
[[nodiscard]] QString symbolText(const Unit& unit)
{
    return QtStrings::unitSymbol(unit).replace(u'&', QStringLiteral("&&"));
}

}  // namespace

UnitSelector::UnitSelector(std::shared_ptr<DoubleBinding> binding, QWidget* parent)
  : QToolButton(parent),
    m_binding(checkedBinding(std::move(binding))),
    m_menu(makeChild<QMenu>(this))
{
    setUp();
    connectBinding();
    refresh();
}

UnitSelector::UnitSelector(const UnitGroup& group, QWidget* parent)
  : QToolButton(parent),
    m_group(&group),
    m_unit(&group.getDefaultUnit()),
    m_menu(makeChild<QMenu>(this))
{
    setUp();
    refresh();
}

UnitSelector::~UnitSelector() = default;

void UnitSelector::setUp()
{
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setFocusPolicy(Qt::NoFocus);
    // Every button's press comes to mousePressEvent(), the right one's too (no context menu).
    setContextMenuPolicy(Qt::PreventContextMenu);
}

void UnitSelector::connectBinding()
{
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
}

void UnitSelector::setBinding(std::shared_ptr<DoubleBinding> binding)
{
    std::shared_ptr<DoubleBinding> checked = checkedBinding(std::move(binding));
    m_menu->hide();
    m_connection.disconnect();
    m_binding = std::move(checked);
    m_group   = nullptr;
    m_unit    = nullptr;
    connectBinding();
    refresh();
}

const UnitGroup& UnitSelector::getUnitGroup() const noexcept
{
    return m_binding != nullptr ? m_binding->getUnitGroup() : *m_group;
}

void UnitSelector::setUnitGroup(const UnitGroup& group)
{
    if (m_binding != nullptr)
    {
        bug("The unit group of a unit selector over a binding is the binding's");
    }
    if (m_group == &group)
    {
        return;
    }
    m_menu->hide();
    m_group = &group;
    m_unit  = &group.getDefaultUnit();
    refresh();
}

const Unit& UnitSelector::getSelectedUnit() const noexcept
{
    return m_binding != nullptr ? m_binding->getCurrentUnit() : *m_unit;
}

void UnitSelector::setSelectedUnit(const Unit& unit)
{
    if (!getUnitGroup().contains(unit))
    {
        bug("The unit selected is not one of the unit selector's group");
    }
    if (m_binding != nullptr)
    {
        m_binding->setCurrentUnit(unit);
    }
    else
    {
        // The group's own unit, as DoubleBinding::setCurrentUnit() keeps it: contains() compares
        // with Unit::equals(), so @p unit may be another group's equal one (the distances' "m"
        // for the lengths'), or a unit of an old menu chosen after setUnitGroup(), which a caller
        // that compares units by address would not find in the group. Java keeps the unit given.
        // A group that contains a unit it does not list (a FixedUnitGroup contains every unit)
        // keeps @p unit, as Java.
        const int index = m_group->getUnitIndex(unit);
        m_unit          = index >= 0 ? &m_group->getUnit(index) : &unit;
    }
    refresh();
    Q_EMIT unitSelected();
}

void UnitSelector::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    refresh();
}

bool UnitSelector::hasChoice() const
{
    return getUnitGroup().getUnitCount() > 1;
}

void UnitSelector::refresh()
{
    setText(symbolText(getSelectedUnit()));
    setEnabled(m_fieldEnabled && (m_binding == nullptr || m_binding->isEnabled()));
    // Without a choice, the mouse goes past the button: no hover, no menu.
    setAttribute(Qt::WA_TransparentForMouseEvents, !hasChoice());
}

void UnitSelector::showUnitMenu()
{
    if (!hasChoice() || !isEnabled())
    {
        return;
    }
    m_menu->clear();
    for (const Unit* const unit : getUnitGroup().getUnits())
    {
        const QAction* const action = m_menu->addAction(symbolText(*unit));
        connect(action, &QAction::triggered, this,
                [this, unit] { Guard::run([this, unit] { chooseUnit(*unit); }, objectName()); });
    }
    // Java: popup.show(this, 0, height), below the label.
    m_menu->popup(mapToGlobal(QPoint(0, height())));
}

void UnitSelector::chooseUnit(const Unit& unit)
{
    // The menu is made from the group; a binding or a group set since is of another one.
    if (getUnitGroup().contains(unit))
    {
        setSelectedUnit(unit);
    }
}

void UnitSelector::mousePressEvent(QMouseEvent* event)
{
    // Any button (Java: mouseClicked of any button); the menu takes the release.
    event->accept();
    showUnitMenu();
}

}  // namespace QtRocket::Gui
