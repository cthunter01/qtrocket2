#pragma once

#include <array>
#include <vector>

#include <QObject>
#include <QWidget>

#include "QtRocket/unit/UnitGroup.h"

class QCheckBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QString;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

class DefaultUnitSelector;

/// The Units page of the preferences dialog (OpenRocket's UnitsPreferencesPanel): "Select your
/// preferred units:", then two columns of a label and a combo box of the group's default unit
/// (DefaultUnitSelector) each, in Java's order (kLeftGroups, kRightGroups), the secondary
/// stability's row enabled only while "Display secondary stability unit" is checked, the buttons
/// "Default metric" and "Default imperial" (UnitGroup::setDefaultMetricUnits() and
/// setDefaultImperialUnits(), then every combo box shows the new default), and the note "The
/// effects will take place the next time you open a window.".
///
/// Every change takes effect at once: a combo box sets the process-wide default unit, the check
/// box stores DisplaySecondaryStability. The default units are stored by the dialog's Apply or
/// OK, and put back by its Cancel.
class UnitsPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The groups of the left column, top to bottom (Rocket dimensions ... Secondary Stability).
    static constexpr std::array<UnitGroupId, 11> kLeftGroups{UnitGroupId::LENGTH,
                                                             UnitGroupId::MOTOR_DIMENSIONS,
                                                             UnitGroupId::DISTANCE,
                                                             UnitGroupId::VELOCITY,
                                                             UnitGroupId::ACCELERATION,
                                                             UnitGroupId::MASS,
                                                             UnitGroupId::FORCE,
                                                             UnitGroupId::IMPULSE,
                                                             UnitGroupId::INERTIA,
                                                             UnitGroupId::STABILITY,
                                                             UnitGroupId::SECONDARY_STABILITY};
    /// The groups of the right column, top to bottom (Line density ... Longitude).
    static constexpr std::array<UnitGroupId, 12> kRightGroups{
        UnitGroupId::DENSITY_LINE, UnitGroupId::DENSITY_SURFACE,
        UnitGroupId::DENSITY_BULK, UnitGroupId::ROUGHNESS,
        UnitGroupId::AREA,         UnitGroupId::ANGLE,
        UnitGroupId::ROLL,         UnitGroupId::TEMPERATURE,
        UnitGroupId::PRESSURE,     UnitGroupId::WINDSPEED,
        UnitGroupId::LATITUDE,     UnitGroupId::LONGITUDE};
    /// The gap between the two columns (MigLayout "[]40lp[]").
    static constexpr int kColumnGap = 40;

    /// The page over @p preferences, which must outlive it.
    UnitsPreferencesPanel(Preferences& preferences, QWidget* parent);
    ~UnitsPreferencesPanel() override;
    UnitsPreferencesPanel(const UnitsPreferencesPanel&)            = delete;
    UnitsPreferencesPanel& operator=(const UnitsPreferencesPanel&) = delete;
    UnitsPreferencesPanel(UnitsPreferencesPanel&&)                 = delete;
    UnitsPreferencesPanel& operator=(UnitsPreferencesPanel&&)      = delete;

    /// The combo boxes, left column first, each in its column's order.
    [[nodiscard]] const std::vector<DefaultUnitSelector*>& unitSelectors() const noexcept
    {
        return m_unitSelectors;
    }
    /// The combo box of @p group, or null when the page has none.
    [[nodiscard]] DefaultUnitSelector* unitSelector(UnitGroupId group) const noexcept;
    [[nodiscard]] QLabel*    secondaryStabilityLabel() const noexcept { return m_secondaryLabel; }
    [[nodiscard]] QCheckBox* displaySecondaryStabilityCheckBox() const noexcept
    {
        return m_displaySecondary;
    }
    [[nodiscard]] QPushButton* defaultMetricButton() const noexcept { return m_metric; }
    [[nodiscard]] QPushButton* defaultImperialButton() const noexcept { return m_imperial; }

    /// Every combo box shows its group's default unit again (Java: fireChange() of each).
    void refreshUnitSelectors();

private:
    /// Adds the row @p row of @p label and the combo box of @p group to @p layout.
    QLabel* addRow(QGridLayout& layout, int row, const QString& label, UnitGroupId group);
    /// The secondary stability's row and the check box below the left column's rows.
    void addSecondaryStability(QGridLayout& layout, int row, const QString& label);
    void displaySecondaryToggled(bool display);
    /// Calls @p setDefaults (UnitGroup's metric or imperial defaults), then shows the new
    /// defaults.
    void setDefaultUnits(void (*setDefaults)());

    Preferences*                      m_preferences;
    std::vector<DefaultUnitSelector*> m_unitSelectors;
    QLabel*                           m_secondaryLabel{nullptr};
    DefaultUnitSelector*              m_secondarySelector{nullptr};
    QCheckBox*                        m_displaySecondary{nullptr};
    QPushButton*                      m_metric{nullptr};
    QPushButton*                      m_imperial{nullptr};
};

}  // namespace QtRocket::Gui
