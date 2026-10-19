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
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setFocusPolicy(Qt::NoFocus);
    // Every button's press comes to mousePressEvent(), the right one's too (no context menu).
    setContextMenuPolicy(Qt::PreventContextMenu);
    connectBinding();
    refresh();
}

UnitSelector::~UnitSelector() = default;

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
    connectBinding();
    refresh();
}

void UnitSelector::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    refresh();
}

bool UnitSelector::hasChoice() const
{
    return m_binding->getUnitGroup().getUnitCount() > 1;
}

void UnitSelector::refresh()
{
    setText(symbolText(m_binding->getCurrentUnit()));
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
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
    for (const Unit* const unit : m_binding->getUnitGroup().getUnits())
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
    // The menu is made from the binding's group; a binding set since is of another one.
    if (m_binding->getUnitGroup().contains(unit))
    {
        m_binding->setCurrentUnit(unit);
    }
}

void UnitSelector::mousePressEvent(QMouseEvent* event)
{
    // Any button (Java: mouseClicked of any button); the menu takes the release.
    event->accept();
    showUnitMenu();
}

}  // namespace QtRocket::Gui
