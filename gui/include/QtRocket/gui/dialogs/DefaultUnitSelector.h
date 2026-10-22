#pragma once

#include <QComboBox>
#include <QObject>

#include "QtRocket/unit/UnitGroup.h"

class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

/// The combo box of one unit group's default unit on the Units page of the preferences
/// (OpenRocket's PreferencesPanel.DefaultUnitSelector, a ComboBoxModel): its items are the units
/// of the process-wide group (unitGroup()), each shown as its symbol (Unit.toString()), and its
/// current item is the group's default unit. Choosing an item makes that unit the group's
/// default at once (UnitGroup::setDefaultUnit()); the preferences store it only when the dialog's
/// Apply or OK stores the default units. refresh() shows the default unit again after it was
/// changed elsewhere (the Default metric and Default imperial buttons; Java's fireChange()).
///
/// The object name is the group's name in OpenRocket's UNITS map ("LENGTH").
///
/// Deviation, as every combo box of QtRocket's (EnumComboBox): the wheel chooses only while the
/// combo box has the focus, which the wheel does not give it (Java's combo boxes take no wheel).
class DefaultUnitSelector final : public QComboBox
{
    Q_OBJECT

public:
    /// The combo box of the process-wide group @p group.
    DefaultUnitSelector(UnitGroupId group, QWidget* parent);
    ~DefaultUnitSelector() override;
    DefaultUnitSelector(const DefaultUnitSelector&)            = delete;
    DefaultUnitSelector& operator=(const DefaultUnitSelector&) = delete;
    DefaultUnitSelector(DefaultUnitSelector&&)                 = delete;
    DefaultUnitSelector& operator=(DefaultUnitSelector&&)      = delete;

    [[nodiscard]] UnitGroupId group() const noexcept { return m_group; }

    /// Shows the group's default unit as the current item (Java: fireChange()).
    void refresh();

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The user chose the item @p index (Java: setSelectedItem()).
    void activatedByUser(int index);

    UnitGroupId m_group;
};

}  // namespace QtRocket::Gui
