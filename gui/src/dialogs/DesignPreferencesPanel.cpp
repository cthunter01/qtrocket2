#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QMargins>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/PrefChoiceSelector.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapright para", "wrap para").
constexpr int kParagraphGap = 12;
/// The page's top inset (MigLayout "ins 30lp n n n").
constexpr int kTopInset = 30;

/// The context of the page's reports (Guard).
QString context()
{
    return QStringLiteral("Design preferences");
}

/// The insert positions' choices (pref.dlg.PrefChoiseSelector1..3).
QStringList insertPositions()
{
    return {
        //: pref.dlg.PrefChoiseSelector1
        DesignPreferencesPanel::tr("Always ask"),
        //: pref.dlg.PrefChoiseSelector2
        DesignPreferencesPanel::tr("Insert in middle"),
        //: pref.dlg.PrefChoiseSelector3
        DesignPreferencesPanel::tr("Add to end"),
    };
}

}  // namespace

DesignPreferencesPanel::DesignPreferencesPanel(Preferences& preferences, BindingScope& scope,
                                               QWidget* parent)
  : QWidget(parent),
    m_preferences(&preferences),
    m_mach(scope.make<DoubleBinding>(preferences.getDefaultMach(),
                                     unitGroup(UnitGroupId::COEFFICIENT), kMinimumMach,
                                     kMaximumMach)),
    m_machSpinner(makeChild<UnitSpinBox>(this, m_mach))
{
    setObjectName(QStringLiteral("designPreferencesPanel"));
    // MigLayout("fillx, ins 30lp n n n"): the labels, then the widgets, which take the rest of
    // the width; a paragraph gap below each of the combo boxes ("wrap para").
    auto*    layout  = makeChild<QGridLayout>(this);
    QMargins margins = layout->contentsMargins();
    margins.setTop(kTopInset);
    layout->setContentsMargins(margins);
    layout->setHorizontalSpacing(kParagraphGap);
    int row = 0;
    // Adds a label and its widget as the next row.
    const auto addRow = [this, layout, &row](const QString& label, QWidget* widget,
                                             Qt::Alignment alignment) {
        auto* text = makeChild<QLabel>(this, label);
        text->setBuddy(widget);
        layout->addWidget(text, row, 0);
        layout->addWidget(widget, row, 1, alignment);
        ++row;
    };

    m_bodyPosition = makeChild<PrefChoiceSelector>(
        this, preferences, PreferenceKeys::kBodyComponentInsertPositionKey, insertPositions());
    //: pref.dlg.lbl.Positiontoinsert
    addRow(tr("Position to insert new body components:"), m_bodyPosition, {});
    layout->setRowMinimumHeight(row++, kParagraphGap);

    m_stagePosition = makeChild<PrefChoiceSelector>(
        this, preferences, PreferenceKeys::kStageInsertPositionKey, insertPositions());
    //: pref.dlg.lbl.PositiontoinsertStages
    addRow(tr("Position to insert new stages:"), m_stagePosition, {});
    layout->setRowMinimumHeight(row++, kParagraphGap);

    m_fontSize =
        makeChild<PrefChoiceSelector>(this, preferences, PreferenceKeys::kRocketInfoFontSize,
                                      QStringList{
                                          //: pref.dlg.PrefFontSmall
                                          tr("Small"),
                                          //: pref.dlg.PrefFontMedium
                                          tr("Medium"),
                                          //: pref.dlg.PrefFontLarge
                                          tr("Large"),
                                      });
    // Java's text ends with a space.
    //: pref.dlg.lbl.Rocketinfofontsize
    addRow(tr("Size of text in rocket design panel: "), m_fontSize, {});
    layout->setRowMinimumHeight(row++, kParagraphGap);

    m_machSpinner->setObjectName(QStringLiteral("DefaultMach"));
    scope.keepConnection(
        m_mach->addChangeListener([this] { Guard::run([this] { machChanged(); }, context()); }));
    //: pref.dlg.lbl.DefaultMach
    addRow(tr("Default Mach Number for C.P. Estimate:"), m_machSpinner, Qt::AlignLeft);

    // Java's ItemListeners: every change of the state is stored.
    //: pref.dlg.checkbox.AlwaysOpenLeftmost
    m_leftmostTab = makeChild<QCheckBox>(
        this, tr("Always open leftmost tab when opening a component edit dialog"));
    m_leftmostTab->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kOpenLeftmostDesignTab));
    m_leftmostTab->setChecked(preferences.isAlwaysOpenLeftmostTab());
    //: pref.dlg.checkbox.AlwaysOpenLeftmost.ttip
    m_leftmostTab->setToolTip(
        tr("<html>If checked, a component edit dialog will always pop up "
           "with the first tab selected. <br>If unchecked, the previous "
           "selected tab will be used.</html>"));
    connect(m_leftmostTab, &QCheckBox::toggled, this,
            guarded([this](bool checked) { m_preferences->setAlwaysOpenLeftmostTab(checked); },
                    context()));
    layout->addWidget(m_leftmostTab, row++, 0, 1, 2);

    //: pref.dlg.checkbox.ShowDiscardConfirmation
    m_discard =
        makeChild<QCheckBox>(this, tr("Show confirmation dialog for discarding component changes"));
    m_discard->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kShowDiscardConfirmation));
    m_discard->setChecked(preferences.isShowDiscardConfirmation());
    // OpenRocket's typo "want really want" verbatim.
    //: pref.dlg.checkbox.ShowDiscardConfirmation.ttip
    m_discard->setToolTip(
        tr("If checked, you will be asked if you want really want to discard "
           "component configuration changes."));
    connect(m_discard, &QCheckBox::toggled, this,
            guarded([this](bool checked) { m_preferences->setShowDiscardConfirmation(checked); },
                    context()));
    layout->addWidget(m_discard, row++, 0, 1, 2);

    // Java's ActionListener: the user's clicks only.
    //: pref.dlg.checkbox.Markers
    m_markers = makeChild<QCheckBox>(
        this, tr("Only show pod set/booster markers when the pod set/booster is selected"));
    m_markers->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kShowMarkers));
    m_markers->setChecked(preferences.isShowMarkers());
    //: pref.dlg.checkbox.Markers.ttip
    m_markers->setToolTip(
        tr("<html>If checked, pod set/booster markers will only be shown when "
           "the pod set/booster is selected. <br>If unchecked, pod set/booster "
           "markers will always be shown.</html>"));
    connect(m_markers, &QCheckBox::clicked, this,
            guarded(
                [this](bool checked) {
                    m_preferences->setShowMarkers(checked);
                    Q_EMIT showMarkersChanged();
                },
                context()));
    layout->addWidget(m_markers, row++, 0, 1, 2);

    layout->setColumnStretch(1, 1);
    layout->setRowStretch(row, 1);
}

DesignPreferencesPanel::~DesignPreferencesPanel() = default;

void DesignPreferencesPanel::machChanged()
{
    m_preferences->setDefaultMach(m_mach->value());
}

}  // namespace QtRocket::Gui
