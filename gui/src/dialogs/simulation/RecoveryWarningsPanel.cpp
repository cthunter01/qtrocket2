#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"

#include <functional>
#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/CollapsiblePanel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

/// The sources of the three thresholds, of the target's class.
struct RecoveryWarningsPanel::Sources
{
    DoubleSource speed;
    DoubleSource dualLow;
    DoubleSource dualHigh;
};

namespace
{

// The columns of a section's grid (Java: "[][pref!][pref!]unrel[][pref!][pref!][grow]"): a
// threshold's label, spin box and unit selector, the unrelated gap, the second threshold's three,
// and the column that takes the width left.
constexpr int kFirstColumn  = 0;
constexpr int kGapColumn    = 3;
constexpr int kSecondColumn = 4;
constexpr int kGrowColumn   = 7;
constexpr int kColumnCount  = 8;

/// MigLayout's unrelated gap (between the two thresholds of a row) and paragraph gap ("wrap
/// para"), and its related gap ("wrap rel", between the sections).
constexpr int kUnrelatedGap = 12;
constexpr int kParagraphGap = 12;
constexpr int kRelatedGap   = 6;

/// A source of the double property of the class O read by @p get and written by @p set (Java:
/// DoubleModel(warningSource, "<Name>")).
template <class O>
[[nodiscard]] DoubleSource doubleSource(double (O::*get)() const, void (O::*set)(double))
{
    return DoubleSource::ofObject<O>({
        .get = [get](const O& object) { return std::invoke(get, object); },
        .set = [set](O& object, const double& value) { std::invoke(set, object, value); },
    });
}

/// String.trim(): @p text without the characters up to U+0020 at either end.
[[nodiscard]] QString javaTrim(const QString& text)
{
    qsizetype begin = 0;
    qsizetype end   = text.size();
    while (begin < end && text.at(begin).unicode() <= u' ')
    {
        ++begin;
    }
    while (end > begin && text.at(end - 1).unicode() <= u' ')
    {
        --end;
    }
    return text.mid(begin, end - begin);
}

/// A threshold: its label "gapright para", spin box and unit selector in @p grid's @p row from
/// @p column (Java: addThresholdEditor()).
[[nodiscard]] RecoveryWarningsPanel::Threshold addThreshold(
    QGridLayout& grid, int row, int column, const std::shared_ptr<DoubleBinding>& binding,
    const QString& label, const QString& tooltip, const QString& property)
{
    QWidget* const                   content = grid.parentWidget();
    RecoveryWarningsPanel::Threshold threshold;
    threshold.label = makeChild<QLabel>(content, label);
    threshold.label->setToolTip(tooltip);
    grid.addWidget(threshold.label, row, column);
    threshold.spinBox = makeChild<UnitSpinBox>(content, binding);
    threshold.spinBox->setObjectName(property);
    threshold.spinBox->setToolTip(tooltip);
    threshold.label->setBuddy(threshold.spinBox);
    grid.addWidget(threshold.spinBox, row, column + 1);
    threshold.unitSelector = makeChild<UnitSelector>(content, binding);
    threshold.unitSelector->setObjectName(property);
    grid.addWidget(threshold.unitSelector, row, column + 2);
    return threshold;
}

/// A binding of a threshold of @p target through @p source: the velocity units, at least 0 and at
/// most kRecoverySpeedThreshold's maximum.
[[nodiscard]] std::shared_ptr<DoubleBinding> thresholdBinding(
    BindingScope& scope, const std::shared_ptr<ObjectSelection>& target, const DoubleSource& source)
{
    return scope.make<DoubleBinding>(
        target, source, unitGroup(UnitGroupId::VELOCITY),
        DoubleBinding::Options{.min = 0,
                               .max = SimulationInputRanges::kRecoverySpeedThreshold.max});
}

/// A description in @p grid's row @p row, spanning its columns.
[[nodiscard]] QLabel* addInfoText(QGridLayout& grid, int row, const QString& text, bool italic,
                                  const QString& name)
{
    // Java: "spanx, growx, wmin 0": the width of the grid, as little as it is given.
    auto* label = makeChild<QLabel>(grid.parentWidget(),
                                    RecoveryWarningsPanel::wrappingInfoHtml(text, italic));
    label->setObjectName(name);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setMinimumWidth(0);
    grid.addWidget(label, row, 0, 1, kColumnCount);
    return label;
}

}  // namespace

RecoveryWarningsPanel::RecoveryWarningsPanel(SimulationOptions& options, BindingScope& scope,
                                             QWidget* parent)
  : RecoveryWarningsPanel(
        scope.keepContext(ObjectSelection::make<SimulationOptions>(options)),
        Sources{
            .speed   = doubleSource<SimulationOptions>(&SimulationOptions::getRecoverySpeedWarning,
                                                       &SimulationOptions::setRecoverySpeedWarning),
            .dualLow = doubleSource<SimulationOptions>(
                &SimulationOptions::getRecoveryDrogueMainLowSpeedWarning,
                &SimulationOptions::setRecoveryDrogueMainLowSpeedWarning),
            .dualHigh = doubleSource<SimulationOptions>(
                &SimulationOptions::getRecoveryDrogueMainHighSpeedWarning,
                &SimulationOptions::setRecoveryDrogueMainHighSpeedWarning),
        },
        scope, parent)
{
}

RecoveryWarningsPanel::RecoveryWarningsPanel(Preferences& preferences, BindingScope& scope,
                                             QWidget* parent)
  : RecoveryWarningsPanel(
        scope.keepContext(ObjectSelection::make<Preferences>(preferences)),
        Sources{
            .speed = doubleSource<Preferences>(&Preferences::getRecoverySpeedWarning,
                                               &Preferences::setRecoverySpeedWarning),
            .dualLow =
                doubleSource<Preferences>(&Preferences::getRecoveryDrogueMainLowSpeedWarning,
                                          &Preferences::setRecoveryDrogueMainLowSpeedWarning),
            .dualHigh =
                doubleSource<Preferences>(&Preferences::getRecoveryDrogueMainHighSpeedWarning,
                                          &Preferences::setRecoveryDrogueMainHighSpeedWarning),
        },
        scope, parent)
{
}

RecoveryWarningsPanel::RecoveryWarningsPanel(const std::shared_ptr<ObjectSelection>& target,
                                             const Sources& sources, BindingScope& scope,
                                             QWidget* parent)
  : QWidget(parent)
{
    setObjectName(QStringLiteral("recoveryWarningsPanel"));
    //: simedtdlg.lbl.RecoveryWarnings.desc
    const QString description =
        tr("<html>Warn when a recovery device deploys at an unsafe speed. "
           "Warnings apply independently by stage.</html>");
    //: simedtdlg.lbl.ttip.HighSpeedWarning
    const QString highTip = tr("Warn when a recovery device deploys above this speed.");
    //: simedtdlg.lbl.HighSpeedWarning
    const QString highLabel = tr("High Speed Threshold");

    // Java: createSingleDeploymentPanel().
    QGridLayout* const grid = makeSectionContent();
    m_singleDescription =
        addInfoText(*grid, 0, description, true, QStringLiteral("singleDescription"));
    grid->setRowMinimumHeight(1, kParagraphGap);
    m_highSpeed =
        addThreshold(*grid, 2, kFirstColumn, thresholdBinding(scope, target, sources.speed),
                     highLabel, highTip, QStringLiteral("RecoverySpeedWarning"));

    // Java: createDualDeploymentPanel().
    QGridLayout* const dualGrid = makeSectionContent();
    m_dualDescription =
        addInfoText(*dualGrid, 0, description, true, QStringLiteral("dualDescription"));
    dualGrid->setRowMinimumHeight(1, kParagraphGap);
    m_dualHowTo =
        addInfoText(*dualGrid, 2,
                    //: simedtdlg.lbl.DualDeployment.HowTo
                    tr("<html>Mark a recovery device as a drogue in the Recovery tab of a "
                       "Stage configuration dialog to enable dual-deployment "
                       "warnings.</html>"),
                    false, QStringLiteral("dualDeploymentHowTo"));
    dualGrid->setRowMinimumHeight(3, kRelatedGap);
    m_dualLow =
        addThreshold(*dualGrid, 4, kFirstColumn, thresholdBinding(scope, target, sources.dualLow),
                     //: simedtdlg.lbl.LowSpeedWarning
                     tr("Low Speed Threshold"),
                     //: simedtdlg.lbl.ttip.LowSpeedWarning
                     tr("Warn when a recovery device deploys below this speed."),
                     QStringLiteral("RecoveryDrogueMainLowSpeedWarning"));
    m_dualHigh =
        addThreshold(*dualGrid, 4, kSecondColumn, thresholdBinding(scope, target, sources.dualHigh),
                     highLabel, highTip, QStringLiteral("RecoveryDrogueMainHighSpeedWarning"));

    // Java: MigLayout("fillx, insets 0", "[grow]"), the sections "growx, wrap rel".
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRelatedGap);
    //: simedtdlg.border.SingleDeployment
    m_single = makeChild<CollapsiblePanel>(this, tr("Single Deployment Warnings"),
                                           grid->parentWidget(), true);
    m_single->setObjectName(QStringLiteral("singleDeploymentPanel"));
    layout->addWidget(m_single);
    //: simedtdlg.border.DualDeployment
    m_dual = makeChild<CollapsiblePanel>(this, tr("Dual Deployment Warnings"),
                                         dualGrid->parentWidget(), false);
    m_dual->setObjectName(QStringLiteral("dualDeploymentPanel"));
    layout->addWidget(m_dual);
}

RecoveryWarningsPanel::~RecoveryWarningsPanel() = default;

QGridLayout* RecoveryWarningsPanel::makeSectionContent()
{
    // Java: createDeploymentPanel(), MigLayout("insets n, fillx", ...); the content becomes the
    // section's child.
    auto* content = makeChild<QWidget>(this);
    auto* grid    = makeChild<QGridLayout>(content);
    grid->setColumnMinimumWidth(kGapColumn, kUnrelatedGap);
    grid->setColumnStretch(kGrowColumn, 1);
    return grid;
}

QString RecoveryWarningsPanel::wrappingInfoHtml(const QString& text, bool italic)
{
    // Java: getHtmlContent().
    const QString trimmed = javaTrim(text);
    const QString lower   = trimmed.toLower();
    QString       content;
    if (!lower.startsWith(QStringLiteral("<html")))
    {
        content = QtStrings::fromUtf8(Strings::escapeHtml(QtStrings::toUtf8(trimmed)));
    }
    else
    {
        const qsizetype openingTagEnd   = trimmed.indexOf(u'>');
        const qsizetype closingTagStart = lower.lastIndexOf(QStringLiteral("</html>"));
        content = openingTagEnd >= 0 && closingTagStart > openingTagEnd
                      ? trimmed.mid(openingTagEnd + 1, closingTagStart - openingTagEnd - 1)
                      : trimmed;
    }
    // Java: getStyleCss() of ITALIC or PLAIN.
    return QStringLiteral("<html><body style='margin:0;") +
           (italic ? QStringLiteral("font-style:italic;") : QString()) + QStringLiteral("'>") +
           content + QStringLiteral("</body></html>");
}

}  // namespace QtRocket::Gui
