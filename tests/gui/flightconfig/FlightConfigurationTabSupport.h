#pragma once

// The "Motors & Configuration" tab in a window of its own for the tests of its panels
// (FlightConfigurationPanel and its sub-panels), over a design of two stages or a new one.

#include <memory>
#include <string>
#include <string_view>

#include <QPoint>
#include <Qt>

#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"

class QMenu;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Rocket;
class RocketComponent;
class Motor;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class ComponentTreeModel;
class ComponentTreeView;
class DocumentSelectionModel;
class FlightConfigurablePanel;
class FlightConfigurationPanel;
class MotorConfigurationPanel;
class RecoveryConfigurationPanel;
class SeparationConfigurationPanel;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The tab of a design in a window of its own (beside the design's component tree, whose
/// selection is the design's selection the tab sets), over test services; shown offscreen by
/// show(). It owns the design. Make it in a test's body, so that the window has gone before
/// GuiTest's TearDown().
class FlightConfigurationTab
{
public:
    /// The designs of the tests.
    enum class Design
    {
        /// A sustainer (Nose, Body acting as a mount with Motor tube inside, also a mount, and
        /// Chute in Body) and a booster stage (Booster, with Booster tube acting as a mount);
        /// three configurations: the first with Estes C6-5 in Body, A8-3 in Motor tube and C6-0
        /// in Booster tube, the second with C6-5 in Body only, the third without a motor; the
        /// first selected, no undo history.
        TWO_STAGES,
        /// A new design, as File > New makes it (one stage, nothing in it).
        NEW,
    };

    /// The tab of a design of @p design.
    explicit FlightConfigurationTab(Design design);
    ~FlightConfigurationTab();
    FlightConfigurationTab(FlightConfigurationTab&&)                 = delete;
    FlightConfigurationTab& operator=(FlightConfigurationTab&&)      = delete;
    FlightConfigurationTab(const FlightConfigurationTab&)            = delete;
    FlightConfigurationTab& operator=(const FlightConfigurationTab&) = delete;

    [[nodiscard]] FlightConfigurationPanel&     panel() const noexcept { return *m_panel; }
    [[nodiscard]] MotorConfigurationPanel&      motors() const;
    [[nodiscard]] RecoveryConfigurationPanel&   recovery() const;
    [[nodiscard]] SeparationConfigurationPanel& stages() const;
    [[nodiscard]] OpenRocketDocument&           document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&                       rocket() const;
    [[nodiscard]] ScriptedUserPrompts&          prompts() noexcept { return m_services.prompts(); }
    [[nodiscard]] DocumentSelectionModel& designSelection() const noexcept { return *m_selection; }
    [[nodiscard]] QWidget&                window() const noexcept { return *m_window; }

    /// Shows the window offscreen, at a size that holds the tables.
    void show() const;

    /// Estes' motor of @p designation in the tests' motor database.
    [[nodiscard]] std::shared_ptr<const Motor> estes(std::string_view designation) const;

    /// The component named @p name.
    [[nodiscard]] RocketComponent& find(std::string_view name) const;

    /// The configuration id of the rocket's row @p row (getIds()).
    [[nodiscard]] FlightConfigurationId id(int row) const;

    /// The row of the rocket's selected configuration; -1 for the default one.
    [[nodiscard]] int selectedRow() const;

    /// The names of the design's selected components, "[A, B]".
    [[nodiscard]] std::string designSelectionNames() const;

    /// The selected rows and columns of @p panel's table, "rows=[0, 1] cols=[2]".
    [[nodiscard]] static std::string lines(const FlightConfigurablePanel& panel);

    /// The centre of the cell (@p row, @p column) of @p panel's table, in its viewport.
    [[nodiscard]] static QPoint cellCenter(const FlightConfigurablePanel& panel, int row,
                                           int column);

    /// A click of the left button on the cell (@p row, @p column) of @p panel's table.
    static void click(const FlightConfigurablePanel& panel, int row, int column,
                      Qt::KeyboardModifiers modifiers = {});

    /// A double click of the left button on the cell (@p row, @p column) of @p panel's table.
    static void doubleClick(const FlightConfigurablePanel& panel, int row, int column);

    /// A click of the right button at @p position of @p panel's table's viewport, with its context
    /// menu event: the popup menu that opened (closed again, its warnings of the offscreen
    /// platform expected), null when none did.
    [[nodiscard]] static QMenu* rightClickAt(const FlightConfigurablePanel& panel,
                                             const QPoint&                  position);

    /// rightClickAt() the cell (@p row, @p column).
    [[nodiscard]] static QMenu* rightClick(const FlightConfigurablePanel& panel, int row,
                                           int column);

    /// The object names of @p menu's items ("-" for a separator), each with " (disabled)" when it
    /// is.
    [[nodiscard]] static std::string items(const QMenu& menu);

private:
    /// Builds the design of TWO_STAGES into the document.
    void buildTwoStages() const;

    TestGuiServices                     m_services;
    ThrustCurveMotorSetDatabase         m_motorDatabase;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<ComponentTreeModel> m_treeModel;
    std::unique_ptr<QWidget>            m_window;
    ComponentTreeView*                  m_tree      = nullptr;
    DocumentSelectionModel*             m_selection = nullptr;
    FlightConfigurationPanel*           m_panel     = nullptr;
};

}  // namespace QtRocket::Gui::Test
