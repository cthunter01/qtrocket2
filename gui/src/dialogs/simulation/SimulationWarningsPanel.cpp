#include "QtRocket/gui/dialogs/simulation/SimulationWarningsPanel.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <QAbstractScrollArea>
#include <QChar>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's reports (Guard).
QString context()
{
    return QStringLiteral("Simulation warnings");
}

/// MigLayout's related gap ("wrap rel").
constexpr int kRelatedGap = 6;
/// The size of a group's title (Java: StyledLabel(title, 2f, BOLD)) and of "No Warnings!"
/// (1.1f), in points above the default font's.
constexpr double kTitleSize      = 2;
constexpr double kNoWarningsSize = 1.1;
/// The gap before a group's description (Java: "gapleft 10lp") and the group panel's insets
/// (Java: MigLayout("fillx, insets 1")).
constexpr int kDescriptionGap = 10;
constexpr int kGroupInsets    = 1;

/// A tab's scroll area (Java: createTabScrollPane()): its preferred height Java's
/// PREFERRED_SCROLL_HEIGHT, its width the scroll area's own.
class TabScrollArea final : public QScrollArea
{
public:
    explicit TabScrollArea(QWidget* parent) : QScrollArea(parent) { }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {QScrollArea::sizeHint().width(), SimulationWarningsPanel::kPreferredScrollHeight};
    }
};

/// What a group of warnings shows (Java: the arguments of createWarningsPanel()).
struct GroupLook
{
    Icons::Id        icon;
    QString          title;
    QString          description;
    ThemeColors::Key color;
    MessagePriority  priority;
};

/// The warnings of @p simulation of @p priority, in their order (Java: getCriticalWarnings(),
/// ...); none without data.
[[nodiscard]] std::vector<const Warning*> warningsOf(const Simulation& simulation,
                                                     MessagePriority   priority)
{
    const WarningSet* const warnings = simulation.getSimulatedWarnings();
    if (warnings == nullptr)
    {
        return {};
    }
    switch (priority)
    {
        case MessagePriority::HIGH:
            return warnings->criticalWarnings();
        case MessagePriority::NORMAL:
            return warnings->normalWarnings();
        case MessagePriority::LOW:
            return warnings->informationalWarnings();
    }
    return {};
}

}  // namespace

SimulationWarningsPanel::SimulationWarningsPanel(std::shared_ptr<Simulation> simulation,
                                                 bool                        showSimulationWarnings,
                                                 Preferences& preferences, BindingScope& scope,
                                                 QWidget* parent)
  : QWidget(parent), m_simulation(std::move(simulation)), m_preferences(&preferences)
{
    if (m_simulation == nullptr)
    {
        bug("The warnings tab needs a simulation");
    }
    setObjectName(QStringLiteral("simulationWarningsPanel"));
    setMinimumSize(0, 0);

    m_tabs = makeChild<QTabWidget>(this);
    m_tabs->setObjectName(QStringLiteral("warningTabs"));
    m_tabs->setMinimumSize(0, 0);
    buildMessages(showSimulationWarnings);
    //: SimulationWarningsPanel.tab.Messages
    m_tabs->addTab(m_messagesScroll, tr("Messages"));
    buildConfiguration(scope);
    //: simpanel.col.Configuration
    m_tabs->addTab(m_configurationScroll, tr("Configuration"));
    if (!showSimulationWarnings)
    {
        // Multi-simulation editing has no single set of run messages to display.
        m_tabs->setCurrentIndex(1);
    }

    // Java: MigLayout("fill", "[grow]", "[grow]"), the tabs "grow, wmin 0, hmin 0".
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(m_tabs);
}

SimulationWarningsPanel::~SimulationWarningsPanel() = default;

// ------------------------------------------------------------------------------ the messages

void SimulationWarningsPanel::buildMessages(bool showSimulationWarnings)
{
    // Java: createMessagesPanel(), MigLayout("fillx", "[grow]").
    m_messagesPanel = makeChild<QWidget>(this);
    m_messagesPanel->setObjectName(QStringLiteral("messagesPanel"));
    auto* column = makeChild<QVBoxLayout>(m_messagesPanel);
    if (showSimulationWarnings)
    {
        buildWarningGroups(*m_messagesPanel, *column);
    }
    else
    {
        m_unavailable = makeChild<StyledLabel>(
            m_messagesPanel,
            //: SimulationConfigDialog.tab.warnDis.ttip
            tr("Warnings not supported for multi-simulation editing"), StyledLabel::Style::ITALIC);
        m_unavailable->setObjectName(QStringLiteral("unavailableLabel"));
        column->addWidget(m_unavailable, 0, Qt::AlignHCenter);
        column->addSpacing(kGroupGap);
    }
    // Java's filler: "grow, spanx, pushy, growy 0.5".
    column->addStretch(1);
    m_messagesScroll = makeTabScrollArea(m_messagesPanel, QStringLiteral("messagesScrollPane"));
}

void SimulationWarningsPanel::buildWarningGroups(QWidget& panel, QVBoxLayout& column)
{
    const std::array<GroupLook, 3> looks{{
        {.icon = Icons::Id::WARNING_HIGH,
         //: SimulationWarningsPanel.lbl.CriticalWarnings
         .title = tr("Critical(s)"),
         //: SimulationWarningsPanel.lbl.CriticalWarnings.desc
         .description = tr("Severe flight problems that should be addressed."),
         .color       = ThemeColors::Key::DARK_ERROR,
         .priority    = MessagePriority::HIGH},
        {.icon = Icons::Id::WARNING_NORMAL,
         //: SimulationWarningsPanel.lbl.NormalWarnings
         .title = tr("Warning(s)"),
         //: SimulationWarningsPanel.lbl.NormalWarnings.desc
         .description = tr("Likely errors in the rocket/simulation configuration."),
         .color       = ThemeColors::Key::WARNING,
         .priority    = MessagePriority::NORMAL},
        {.icon = Icons::Id::WARNING_LOW,
         //: SimulationWarningsPanel.lbl.InformationalWarnings
         .title = tr("Informational(s)"),
         //: SimulationWarningsPanel.lbl.InformationalWarnings.desc
         .description = tr("Configurations that could lead to less accurate simulations."),
         .color       = ThemeColors::Key::INFO,
         .priority    = MessagePriority::LOW},
    }};
    bool                           shownBefore = false;
    for (const GroupLook& look : looks)
    {
        const std::vector<const Warning*> warnings = warningsOf(*m_simulation, look.priority);
        if (warnings.empty())
        {
            continue;
        }
        if (shownBefore)
        {
            column.addSpacing(kGroupGap);
        }
        shownBefore = true;
        // Java: createWarningsPanel(), MigLayout("fillx, insets 1").
        auto* group = makeChild<QWidget>(&panel);
        auto* grid  = makeChild<QGridLayout>(group);
        grid->setContentsMargins(kGroupInsets, kGroupInsets, kGroupInsets, kGroupInsets);
        Group& widgets = *groupOf(look.priority);
        widgets.title =
            makeChild<StyledLabel>(group, look.title, kTitleSize, StyledLabel::Style::BOLD);
        widgets.title->setFontColor(ThemeColors::color(look.color, palette()));
        grid->addWidget(widgets.title, 0, 0);
        widgets.description = makeChild<StyledLabel>(
            group, QString(QChar(0x2014)) + QStringLiteral(" ") + look.description,
            StyledLabel::Style::ITALIC);
        widgets.description->setFontColor(
            ThemeColors::color(ThemeColors::Key::TEXT_DIM, palette()));
        widgets.description->setContentsMargins(kDescriptionGap, 0, 0, 0);
        grid->addWidget(widgets.description, 0, 1);
        grid->setColumnStretch(1, 1);

        QStringList texts;
        for (const Warning* warning : warnings)
        {
            texts.append(QtStrings::fromUtf8(warning->toString()));
        }
        widgets.list     = makeChild<MessageList>(group, texts);
        const QIcon icon = Icons::icon(look.icon);
        for (int row = 0; row < widgets.list->count(); ++row)
        {
            QListWidgetItem& item = *widgets.list->item(row);
            item.setIcon(icon);
            // The icon's name, for the walk of the tests (SimulationDialogWalk.h).
            item.setData(Qt::UserRole, Icons::name(look.icon));
        }
        // Java's JList is not in a scroll pane: as tall as its rows, as wide as its longest.
        widgets.list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        widgets.list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        widgets.list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        widgets.list->setMinimumWidth(widgets.list->sizeHintForColumn(0) +
                                      (2 * widgets.list->frameWidth()));
        widgets.list->installEventFilter(this);
        grid->addWidget(widgets.list, 1, 0, 1, 2);
        column.addWidget(group);
    }
    if (!shownBefore)
    {
        // No warnings
        m_noWarnings =
            makeChild<StyledLabel>(&panel,
                                   //: SimulationWarningsPanel.lbl.NoWarnings
                                   tr("No Warnings!"), kNoWarningsSize, StyledLabel::Style::ITALIC);
        m_noWarnings->setObjectName(QStringLiteral("noWarningsLabel"));
        //: SimulationWarningsPanel.lbl.NoWarnings.ttip
        m_noWarnings->setToolTip(tr("Yay! No warnings! :)"));
        column.addWidget(m_noWarnings, 0, Qt::AlignHCenter);
        column.addSpacing(kGroupGap);
    }
}

// ------------------------------------------------------------------------- the configuration

void SimulationWarningsPanel::buildConfiguration(BindingScope& scope)
{
    // Java: createConfigurationPanel(), a ConfigurationPanel (MigLayout("fillx", "[grow]")).
    m_configurationPanel = makeChild<QWidget>(this);
    m_configurationPanel->setObjectName(QStringLiteral("configurationPanel"));
    auto* column = makeChild<QVBoxLayout>(m_configurationPanel);
    column->setSpacing(0);
    m_recovery =
        makeChild<RecoveryWarningsPanel>(m_configurationPanel, m_simulation->getOptions(), scope);
    column->addWidget(m_recovery);
    column->addSpacing(kRelatedGap);

    // Java: createDefaultsButtons(), MigLayout("insets 0"), "split 2".
    auto* buttonRow = makeChild<QWidget>(m_configurationPanel);
    auto* buttons   = makeChild<QHBoxLayout>(buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    //: simedtdlg.but.resettodefault
    m_resetButton = makeChild<QPushButton>(buttonRow, tr("Reset to default"));
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    m_resetButton->setAutoDefault(false);
    connect(m_resetButton, &QPushButton::clicked, this,
            guarded([this] { resetWarningThresholds(); }, context()));
    buttons->addWidget(m_resetButton);
    //: simedtdlg.but.savedefault
    m_saveButton = makeChild<QPushButton>(buttonRow, tr("Save as default"));
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    m_saveButton->setAutoDefault(false);
    connect(m_saveButton, &QPushButton::clicked, this,
            guarded([this] { saveWarningThresholds(); }, context()));
    buttons->addWidget(m_saveButton);
    buttons->addStretch(1);
    column->addWidget(buttonRow);
    column->addSpacing(kGroupGap);
    // Java's filler: "grow, spanx, pushy, growy 0.5".
    column->addStretch(1);
    m_configurationScroll =
        makeTabScrollArea(m_configurationPanel, QStringLiteral("configurationScrollPane"));
}

QScrollArea* SimulationWarningsPanel::makeTabScrollArea(QWidget* content, const QString& name)
{
    auto* scrollArea = makeChild<TabScrollArea>(this);
    scrollArea->setObjectName(name);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMinimumSize(0, 0);
    scrollArea->verticalScrollBar()->setSingleStep(kScrollStep);
    // The content takes the viewport's size, down to what its controls need (Java's
    // ConfigurationPanel tracks the viewport's width until then).
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(content);
    return scrollArea;
}

void SimulationWarningsPanel::commitEdits()
{
    for (UnitSpinBox* spinBox : m_recovery->findChildren<UnitSpinBox*>())
    {
        spinBox->commitEdit();
    }
}

void SimulationWarningsPanel::resetWarningThresholds()
{
    commitEdits();
    SimulationOptions& options = m_simulation->getOptions();
    options.setRecoverySpeedWarning(m_preferences->getRecoverySpeedWarning());
    options.setRecoveryDrogueMainHighSpeedWarning(
        m_preferences->getRecoveryDrogueMainHighSpeedWarning());
    options.setRecoveryDrogueMainLowSpeedWarning(
        m_preferences->getRecoveryDrogueMainLowSpeedWarning());
}

void SimulationWarningsPanel::saveWarningThresholds()
{
    commitEdits();
    const SimulationOptions& options = m_simulation->getOptions();
    m_preferences->setRecoverySpeedWarning(options.getRecoverySpeedWarning());
    m_preferences->setRecoveryDrogueMainHighSpeedWarning(
        options.getRecoveryDrogueMainHighSpeedWarning());
    m_preferences->setRecoveryDrogueMainLowSpeedWarning(
        options.getRecoveryDrogueMainLowSpeedWarning());
}

bool SimulationWarningsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusOut)
    {
        for (const MessagePriority priority :
             {MessagePriority::HIGH, MessagePriority::NORMAL, MessagePriority::LOW})
        {
            if (MessageList* const list = warningList(priority); list != nullptr && watched == list)
            {
                list->clearSelection();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------------- the groups

MessageList* SimulationWarningsPanel::warningList(MessagePriority priority) const noexcept
{
    return groupOf(priority)->list;
}

StyledLabel* SimulationWarningsPanel::groupTitle(MessagePriority priority) const noexcept
{
    return groupOf(priority)->title;
}

StyledLabel* SimulationWarningsPanel::groupDescription(MessagePriority priority) const noexcept
{
    return groupOf(priority)->description;
}

SimulationWarningsPanel::Group* SimulationWarningsPanel::groupOf(MessagePriority priority) noexcept
{
    switch (priority)
    {
        case MessagePriority::HIGH:
            return &m_critical;
        case MessagePriority::NORMAL:
            return &m_normal;
        case MessagePriority::LOW:
            return &m_informational;
    }
    return &m_informational;
}

const SimulationWarningsPanel::Group* SimulationWarningsPanel::groupOf(
    MessagePriority priority) const noexcept
{
    switch (priority)
    {
        case MessagePriority::HIGH:
            return &m_critical;
        case MessagePriority::NORMAL:
            return &m_normal;
        case MessagePriority::LOW:
            return &m_informational;
    }
    return &m_informational;
}

}  // namespace QtRocket::Gui
