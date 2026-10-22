#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("wrap paragraph", "wrap para").
constexpr int kParagraphGap = 12;
/// The note's size relative to the page's font (new StyledLabel(..., -2, Style.ITALIC)).
constexpr double kNoteSize = -2.0;

/// The context of the page's reports (Guard).
QString context()
{
    return QStringLiteral("Units preferences");
}

}  // namespace

UnitsPreferencesPanel::UnitsPreferencesPanel(Preferences& preferences, QWidget* parent)
  : QWidget(parent), m_preferences(&preferences)
{
    setObjectName(QStringLiteral("unitsPreferencesPanel"));
    // MigLayout("", "[]40lp[]"): the two columns at their preferred widths, the rest of the page
    // empty (column 2, row 4).
    auto* layout = makeChild<QGridLayout>(this);
    layout->setHorizontalSpacing(kColumnGap);

    //: pref.dlg.lbl.Selectprefunits
    layout->addWidget(makeChild<QLabel>(this, tr("Select your preferred units:")), 0, 0, 1, 2);
    layout->setRowMinimumHeight(1, kParagraphGap);

    // The left column (JPanel(new MigLayout("ins 0"))): a label and a combo box per row, the
    // combo boxes of one width (sizegroup boxes).
    auto* leftPanel = makeChild<QWidget>(this);
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    auto* left = makeChild<QGridLayout>(leftPanel);
    left->setContentsMargins(0, 0, 0, 0);
    //: pref.dlg.lbl.Rocketdimensions
    addRow(*left, 0, tr("Rocket dimensions:"), UnitGroupId::LENGTH);
    //: pref.dlg.lbl.Motordimensions
    addRow(*left, 1, tr("Motor dimensions:"), UnitGroupId::MOTOR_DIMENSIONS);
    //: pref.dlg.lbl.Distance
    addRow(*left, 2, tr("Distance:"), UnitGroupId::DISTANCE);
    //: pref.dlg.lbl.Velocity
    addRow(*left, 3, tr("Velocity:"), UnitGroupId::VELOCITY);
    //: pref.dlg.lbl.Acceleration
    addRow(*left, 4, tr("Acceleration:"), UnitGroupId::ACCELERATION);
    //: pref.dlg.lbl.Mass
    addRow(*left, 5, tr("Mass:"), UnitGroupId::MASS);
    //: pref.dlg.lbl.Force
    addRow(*left, 6, tr("Force:"), UnitGroupId::FORCE);
    //: pref.dlg.lbl.Totalimpulse
    addRow(*left, 7, tr("Total impulse:"), UnitGroupId::IMPULSE);
    //: pref.dlg.lbl.Momentofinertia
    addRow(*left, 8, tr("Moment of inertia:"), UnitGroupId::INERTIA);
    //: pref.dlg.lbl.Stability
    addRow(*left, 9, tr("Stability:"), UnitGroupId::STABILITY);
    //: pref.dlg.lbl.SecondaryStability
    addSecondaryStability(*left, 10, tr("Secondary Stability:"));

    // The right column.
    auto* rightPanel = makeChild<QWidget>(this);
    rightPanel->setObjectName(QStringLiteral("rightPanel"));
    auto* right = makeChild<QGridLayout>(rightPanel);
    right->setContentsMargins(0, 0, 0, 0);
    //: pref.dlg.lbl.Linedensity
    addRow(*right, 0, tr("Line density:"), UnitGroupId::DENSITY_LINE);
    //: pref.dlg.lbl.Surfacedensity
    addRow(*right, 1, tr("Surface density:"), UnitGroupId::DENSITY_SURFACE);
    //: pref.dlg.lbl.Bulkdensity
    addRow(*right, 2, tr("Bulk density:"), UnitGroupId::DENSITY_BULK);
    //: pref.dlg.lbl.Surfaceroughness
    addRow(*right, 3, tr("Surface roughness:"), UnitGroupId::ROUGHNESS);
    //: pref.dlg.lbl.Area
    addRow(*right, 4, tr("Area:"), UnitGroupId::AREA);
    //: pref.dlg.lbl.Angle
    addRow(*right, 5, tr("Angle:"), UnitGroupId::ANGLE);
    //: pref.dlg.lbl.Rollrate
    addRow(*right, 6, tr("Roll rate:"), UnitGroupId::ROLL);
    //: pref.dlg.lbl.Temperature
    addRow(*right, 7, tr("Temperature:"), UnitGroupId::TEMPERATURE);
    //: pref.dlg.lbl.Pressure
    addRow(*right, 8, tr("Pressure:"), UnitGroupId::PRESSURE);
    //: pref.dlg.lbl.Windspeed
    addRow(*right, 9, tr("Wind speed"), UnitGroupId::WINDSPEED);
    //: pref.dlg.lbl.Latitude
    addRow(*right, 10, tr("Latitude:"), UnitGroupId::LATITUDE);
    //: pref.dlg.lbl.Longitude
    addRow(*right, 11, tr("Longitude:"), UnitGroupId::LONGITUDE);
    right->setRowStretch(12, 1);

    layout->addWidget(leftPanel, 2, 0, Qt::AlignTop);
    layout->addWidget(rightPanel, 2, 1, Qt::AlignTop);
    layout->setRowMinimumHeight(3, kParagraphGap);

    // The buttons over both columns, sharing their width ("spanx, split 2, grow").
    auto* buttonPanel = makeChild<QWidget>(this);
    buttonPanel->setObjectName(QStringLiteral("buttonPanel"));
    auto* buttons = makeChild<QHBoxLayout>(buttonPanel);
    buttons->setContentsMargins(0, 0, 0, 0);
    //: pref.dlg.but.defaultmetric
    m_metric = makeChild<QPushButton>(buttonPanel, tr("Default metric"));
    m_metric->setObjectName(QStringLiteral("defaultMetricButton"));
    connect(m_metric, &QPushButton::clicked, this,
            guarded([this] { setDefaultUnits(UnitGroup::setDefaultMetricUnits); }, context()));
    buttons->addWidget(m_metric, 1);
    //: pref.dlg.but.defaultimperial
    m_imperial = makeChild<QPushButton>(buttonPanel, tr("Default imperial"));
    m_imperial->setObjectName(QStringLiteral("defaultImperialButton"));
    connect(m_imperial, &QPushButton::clicked, this,
            guarded([this] { setDefaultUnits(UnitGroup::setDefaultImperialUnits); }, context()));
    buttons->addWidget(m_imperial, 1);
    layout->addWidget(buttonPanel, 4, 0, 1, 2);
    layout->setRowMinimumHeight(5, kParagraphGap);

    //: pref.dlg.lbl.effect1
    auto* note = makeChild<StyledLabel>(this,
                                        tr("The effects will take place the next time you "
                                           "open a window."),
                                        kNoteSize, StyledLabel::Style::ITALIC);
    note->setObjectName(QStringLiteral("effectNote"));
    layout->addWidget(note, 6, 0, 1, 2);

    layout->setColumnStretch(2, 1);
    layout->setRowStretch(7, 1);
}

UnitsPreferencesPanel::~UnitsPreferencesPanel() = default;

DefaultUnitSelector* UnitsPreferencesPanel::unitSelector(UnitGroupId group) const noexcept
{
    for (DefaultUnitSelector* selector : m_unitSelectors)
    {
        if (selector->group() == group)
        {
            return selector;
        }
    }
    return nullptr;
}

void UnitsPreferencesPanel::refreshUnitSelectors()
{
    for (DefaultUnitSelector* selector : m_unitSelectors)
    {
        selector->refresh();
    }
}

QLabel* UnitsPreferencesPanel::addRow(QGridLayout& layout, int row, const QString& label,
                                      UnitGroupId group)
{
    QWidget* const panel    = layout.parentWidget();
    auto*          text     = makeChild<QLabel>(panel, label);
    auto*          selector = makeChild<DefaultUnitSelector>(panel, group);
    text->setBuddy(selector);
    layout.addWidget(text, row, 0);
    layout.addWidget(selector, row, 1);
    m_unitSelectors.push_back(selector);
    return text;
}

void UnitsPreferencesPanel::addSecondaryStability(QGridLayout& layout, int row,
                                                  const QString& label)
{
    const bool display = m_preferences->isDisplaySecondaryStability();
    //: pref.dlg.lbl.SecondaryStability.ttip
    const QString tip =
        tr("Select the stability unit that will be displayed alongside the main "
           "stability unit in the design view.");
    m_secondaryLabel = addRow(layout, row, label, UnitGroupId::SECONDARY_STABILITY);
    m_secondaryLabel->setToolTip(tip);
    m_secondaryLabel->setEnabled(display);
    m_secondarySelector = m_unitSelectors.back();
    m_secondarySelector->setToolTip(tip);
    m_secondarySelector->setEnabled(display);

    //: pref.dlg.checkbox.DisplaySecondaryStability
    m_displaySecondary =
        makeChild<QCheckBox>(layout.parentWidget(), tr("Display secondary stability unit"));
    m_displaySecondary->setObjectName(
        QtStrings::fromUtf8(PreferenceKeys::kDisplaySecondaryStability));
    //: pref.dlg.checkbox.DisplaySecondaryStability.ttip
    m_displaySecondary->setToolTip(
        tr("If checked, a secondary stability unit will be displayed in the rocket design view."));
    m_displaySecondary->setChecked(display);
    // Java's ItemListener: every change of the state.
    connect(m_displaySecondary, &QCheckBox::toggled, this,
            guarded([this](bool checked) { displaySecondaryToggled(checked); }, context()));
    layout.addWidget(m_displaySecondary, row + 1, 0, 1, 2);
}

void UnitsPreferencesPanel::setDefaultUnits(void (*setDefaults)())
{
    setDefaults();
    refreshUnitSelectors();
}

void UnitsPreferencesPanel::displaySecondaryToggled(bool display)
{
    m_preferences->setDisplaySecondaryStability(display);
    m_secondaryLabel->setEnabled(display);
    m_secondarySelector->setEnabled(display);
}

}  // namespace QtRocket::Gui
