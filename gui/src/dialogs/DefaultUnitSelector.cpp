#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"

#include <QComboBox>
#include <QString>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

DefaultUnitSelector::DefaultUnitSelector(UnitGroupId group, QWidget* parent)
  : QComboBox(parent), m_group(group)
{
    setObjectName(QtStrings::fromUtf8(unitGroupName(group)));
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    const UnitGroup& units = unitGroup(group);
    for (int index = 0; index < units.getUnitCount(); ++index)
    {
        addItem(QtStrings::unitSymbol(units.getUnit(index)));
    }
    refresh();
    connect(this, &QComboBox::activated, this,
            guarded([this](int index) { activatedByUser(index); }, objectName()));
}

DefaultUnitSelector::~DefaultUnitSelector() = default;

void DefaultUnitSelector::refresh()
{
    setCurrentIndex(unitGroup(m_group).getDefaultUnitIndex());
}

void DefaultUnitSelector::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it
        return;
    }
    QComboBox::wheelEvent(event);
}

void DefaultUnitSelector::activatedByUser(int index)
{
    if (index < 0)
    {
        return;
    }
    unitGroup(m_group).setDefaultUnit(index);
    refresh();
}

}  // namespace QtRocket::Gui
