#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

#include "QtRocket/logging/MessagePriority.h"

class QEvent;
class QLabel;
class QPushButton;
class QScrollArea;
class QTabWidget;
class QVBoxLayout;

namespace QtRocket
{
class Preferences;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class MessageList;
class RecoveryWarningsPanel;
class StyledLabel;

/// The "Warnings" tab of the simulation settings dialog (OpenRocket's SimulationWarningsPanel,
/// swing gui/simulation/): two tabs of its own, each in a scroll area of its own below the tab
/// header (Java: a JScrollPane per tab, no border, 200 pixels high when packed, scrolled 16 pixels
/// a step):
/// - "Messages" (messagesScrollPane): the warnings of the simulation's last run
///   (Simulation::getSimulatedWarnings(), read when the panel is made), grouped by priority:
///   "Critical(s)" (the critical warnings, the WARNING_HIGH icon, the dark error colour,
///   "Severe flight problems that should be addressed."), "Warning(s)" (WARNING_NORMAL, the
///   warning colour, "Likely errors in the rocket/simulation configuration.") and
///   "Informational(s)" (WARNING_LOW, the information colour, "Configurations that could lead to
///   less accurate simulations."): for each group that has warnings, its title two points larger
///   in bold and its colour, "&mdash; " and its description in italics in the dim text colour,
///   and the list of its warnings (MessageList, the core's English texts, Warning::toString(),
///   each row with the group's icon, whose Icons::name() is the row's Qt::UserRole data; a focus
///   loss clears its selection); 20 pixels between the
///   groups. Without warnings (or data), "No Warnings!" (centred, 1.1 points larger, italic, its
///   tooltip "Yay! No warnings! :)"). In a multi-simulation edit (@p showSimulationWarnings
///   false), the italic "Warnings not supported for multi-simulation editing" in place of the
///   warnings, and the panel opens on Configuration.
/// - "Configuration" (configurationScrollPane): the recovery thresholds of the simulation's
///   options (RecoveryWarningsPanel), then "Reset to default" (the three thresholds from the
///   preferences: resetWarningThresholds()) and "Save as default" (the three into the
///   preferences: saveWarningThresholds()), each first committing a spin box's pending edit (a
///   click on a button that takes no focus would leave it pending, where Java's focus loss commits
///   it). The content takes the scroll area's width and height while they are enough for its
///   controls (Java's ConfigurationPanel, a Scrollable that tracks the viewport down to its
///   minimum width), and scrolls beyond (QScrollArea::setWidgetResizable()).
///
/// Lifetime: the simulation is held by its std::shared_ptr, so its options stay at their address;
/// the thresholds' bindings and selection are kept by @p scope, which the dialog commits and
/// releases when it closes. @p preferences (the default buttons') must outlive the panel.
///
/// Deviations from OpenRocket:
/// - The default buttons commit a pending edit first (see above).
/// - The warnings' lists are as tall as their rows and as wide as their longest row, inside the
///   scroll area (Java's JLists are not in a scroll pane either); the rows keep the platform's gap
///   between icon and text (Java: 10 pixels) and the palette's alternating colours (MessageList).
/// - Layout approximations: QGridLayouts and QVBoxLayouts for MigLayout's.
class SimulationWarningsPanel final : public QWidget
{
    Q_OBJECT

public:
    /// Java's PREFERRED_SCROLL_HEIGHT: the height of the tabs' scroll areas when packed.
    static constexpr int kPreferredScrollHeight = 200;
    /// Java's unit increment of the scroll areas.
    static constexpr int kScrollStep = 16;
    /// The gap between the warning groups and below the "No Warnings!" line (Java: "wrap 20lp").
    static constexpr int kGroupGap = 20;

    /// The Warnings tab of @p simulation, which must not be null: its warnings unless
    /// @p showSimulationWarnings is false (a multi-simulation edit), and its thresholds, whose
    /// bindings @p scope keeps; @p preferences give the default buttons' values (see the class
    /// comment).
    /// @throws BugError when @p simulation is null
    SimulationWarningsPanel(std::shared_ptr<Simulation> simulation, bool showSimulationWarnings,
                            Preferences& preferences, BindingScope& scope, QWidget* parent);
    ~SimulationWarningsPanel() override;
    SimulationWarningsPanel(const SimulationWarningsPanel&)            = delete;
    SimulationWarningsPanel& operator=(const SimulationWarningsPanel&) = delete;
    SimulationWarningsPanel(SimulationWarningsPanel&&)                 = delete;
    SimulationWarningsPanel& operator=(SimulationWarningsPanel&&)      = delete;

    /// The simulation whose warnings and thresholds are shown.
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }

    /// The two tabs (warningTabs): Messages at index 0, Configuration at index 1.
    [[nodiscard]] QTabWidget&  tabs() const noexcept { return *m_tabs; }
    [[nodiscard]] QScrollArea& messagesScrollArea() const noexcept { return *m_messagesScroll; }
    [[nodiscard]] QScrollArea& configurationScrollArea() const noexcept
    {
        return *m_configurationScroll;
    }
    /// The content of the Messages tab (messagesPanel) and of the Configuration tab
    /// (configurationPanel).
    [[nodiscard]] QWidget& messagesPanel() const noexcept { return *m_messagesPanel; }
    [[nodiscard]] QWidget& configurationPanel() const noexcept { return *m_configurationPanel; }

    /// "No Warnings!" (noWarningsLabel); null when there are warnings or in a multi-simulation
    /// edit.
    [[nodiscard]] StyledLabel* noWarningsLabel() const noexcept { return m_noWarnings; }
    /// "Warnings not supported for multi-simulation editing" (unavailableLabel); null unless in a
    /// multi-simulation edit.
    [[nodiscard]] StyledLabel* unavailableLabel() const noexcept { return m_unavailable; }
    /// The list of the warnings of @p priority (criticalWarnings, normalWarnings,
    /// informationalWarnings); null when the group has none or is not shown.
    [[nodiscard]] MessageList* warningList(MessagePriority priority) const noexcept;
    /// The title and the description of the group of @p priority; null as warningList().
    [[nodiscard]] StyledLabel* groupTitle(MessagePriority priority) const noexcept;
    [[nodiscard]] StyledLabel* groupDescription(MessagePriority priority) const noexcept;

    /// The thresholds (RecoveryWarningsPanel) and the default buttons (resetButton, saveButton).
    [[nodiscard]] RecoveryWarningsPanel& recoveryWarnings() const noexcept { return *m_recovery; }
    [[nodiscard]] QPushButton&           resetButton() const noexcept { return *m_resetButton; }
    [[nodiscard]] QPushButton&           saveButton() const noexcept { return *m_saveButton; }

    /// "Reset to default" (Java: resetWarningThresholds()): the options' RecoverySpeedWarning,
    /// RecoveryDrogueMainHighSpeedWarning and RecoveryDrogueMainLowSpeedWarning from the
    /// preferences, after a pending edit is committed.
    void resetWarningThresholds();
    /// "Save as default" (Java: saveWarningThresholds()): the preferences take the three, after a
    /// pending edit is committed.
    void saveWarningThresholds();

    /// A warning list's focus loss clears its selection (Java's focusLost()); the events always go
    /// on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// One group of warnings.
    struct Group
    {
        StyledLabel* title       = nullptr;
        StyledLabel* description = nullptr;
        MessageList* list        = nullptr;
    };

    /// The Messages tab's content (Java: createMessagesPanel()).
    void buildMessages(bool showSimulationWarnings);
    /// The groups of the simulation's warnings (Java: addSimulationWarnings()) on @p panel, the
    /// Messages tab's content, into @p column, its layout.
    void buildWarningGroups(QWidget& panel, QVBoxLayout& column);
    /// The Configuration tab's content (Java: createConfigurationPanel()).
    void buildConfiguration(BindingScope& scope);
    /// A tab's scroll area showing @p content (Java: createTabScrollPane()).
    [[nodiscard]] QScrollArea* makeTabScrollArea(QWidget* content, const QString& name);
    [[nodiscard]] Group*       groupOf(MessagePriority priority) noexcept;
    [[nodiscard]] const Group* groupOf(MessagePriority priority) const noexcept;
    /// Commits the pending edit of the thresholds' spin boxes.
    void commitEdits();

    std::shared_ptr<Simulation> m_simulation;
    Preferences*                m_preferences;
    QTabWidget*                 m_tabs{nullptr};
    QScrollArea*                m_messagesScroll{nullptr};
    QScrollArea*                m_configurationScroll{nullptr};
    QWidget*                    m_messagesPanel{nullptr};
    QWidget*                    m_configurationPanel{nullptr};
    StyledLabel*                m_noWarnings{nullptr};
    StyledLabel*                m_unavailable{nullptr};
    Group                       m_critical;
    Group                       m_normal;
    Group                       m_informational;
    RecoveryWarningsPanel*      m_recovery{nullptr};
    QPushButton*                m_resetButton{nullptr};
    QPushButton*                m_saveButton{nullptr};
};

}  // namespace QtRocket::Gui
