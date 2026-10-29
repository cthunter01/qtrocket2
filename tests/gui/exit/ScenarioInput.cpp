#include "exit/ScenarioInput.h"

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "main/FileMenusTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui::Test::ScenarioInput
{

void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

void settleAndDelete()
{
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

std::string shortClassName(const QObject& object)
{
    const std::string_view name = object.metaObject()->className();
    const std::size_t      at   = name.rfind("::");
    return std::string(at == std::string_view::npos ? name : name.substr(at + 2));
}

bool activateWindowOf(const QWidget& widget)
{
    QWidget* const window = widget.window();
    if (!window->isActiveWindow())
    {
        window->activateWindow();
        return QTest::qWaitForWindowActive(window);
    }
    return true;
}

std::string focusOn(QWidget& widget, std::string_view field)
{
    if (!activateWindowOf(widget))
    {
        return std::format("the window of the field {} does not become active", field);
    }
    widget.setFocus(Qt::MouseFocusReason);
    settle();
    return widget.hasFocus() ? std::string()
                             : std::format("the field {} does not take the focus", field);
}

std::string chooseUnit(UnitSelector& selector, std::string_view field, std::string_view unit)
{
    const QString wanted = QtStrings::fromUtf8(unit);
    if (selector.text() == wanted)
    {
        return {};
    }
    const OffscreenPopupWarnings popup;
    QTest::mouseClick(&selector, Qt::LeftButton);
    QMenu* const menu = selector.unitMenu();
    if (menu == nullptr || !menu->isVisible())
    {
        return std::format("the unit selector of {} opens no menu", field);
    }
    QAction* const item = menuItem(*menu, wanted);
    if (item == nullptr)
    {
        menu->hide();
        return std::format("the unit selector of {} offers no {}", field, unit);
    }
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(item).center());
    if (selector.text() != wanted)
    {
        return std::format("the unit selector of {} shows {} after {} was chosen", field,
                           QtStrings::toUtf8(selector.text()), unit);
    }
    return {};
}

std::string typeNumber(QAbstractSpinBox& spinBox, std::string_view field, std::string_view text)
{
    if (!activateWindowOf(spinBox))
    {
        return std::format("the window of the field {} does not become active", field);
    }
    typeInto(spinBox, QtStrings::fromUtf8(text));
    QTest::keyClick(&spinBox, Qt::Key_Return);
    settle();
    return {};
}

std::string triggerAction(QAction* action, std::string_view name)
{
    if (action == nullptr || !action->isEnabled())
    {
        return std::format("{} is not enabled", name);
    }
    action->trigger();
    settle();
    return {};
}

}  // namespace QtRocket::Gui::Test::ScenarioInput
