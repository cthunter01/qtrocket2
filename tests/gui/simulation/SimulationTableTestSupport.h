#pragma once

// The simulations table in the GUI tests: an example design loaded as the simulation-table pins'
// probe loads it (tests/gui/simulation/SimulationTablePins.h), and the table of a design: its
// model under QAbstractItemModelTester and a view of it, with the points of its cells and headers
// for the mouse.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemModelTester>
#include <QPoint>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui::Test
{

/// The preferences of the examples' loads (DesignFileEnvironment: a motor by its common name, as
/// the pins' probe has it).
[[nodiscard]] const Preferences& simulationExamplePreferences();

/// The example design whose file in data/examples is @p file ("A simple model rocket.ork"), as
/// loaded; null (and a test failure) when it cannot be loaded.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> loadSimulationExample(std::string_view file);

/// The simulations table of a design: the document, its model under QAbstractItemModelTester
/// (Warning mode) and a view of it, 1600 x 400 pixels (every column fits), shown by show() only.
/// The view goes first, the document last.
class SimulationTableFixture
{
public:
    using Column = SimulationColumns::Column;

    /// The table of @p document, which must not be null.
    explicit SimulationTableFixture(std::unique_ptr<OpenRocketDocument> document);
    ~SimulationTableFixture();

    SimulationTableFixture(const SimulationTableFixture&)            = delete;
    SimulationTableFixture& operator=(const SimulationTableFixture&) = delete;
    SimulationTableFixture(SimulationTableFixture&&)                 = delete;
    SimulationTableFixture& operator=(SimulationTableFixture&&)      = delete;

    [[nodiscard]] OpenRocketDocument&   document() const noexcept { return *m_document; }
    [[nodiscard]] SimulationTableModel& model() noexcept { return m_model; }
    [[nodiscard]] SimulationTableView&  view() const noexcept { return *m_view; }

    /// Shows the view (offscreen) and lets it lay itself out.
    void show();

    /// The centre of the cell of @p row in @p column, in the view's viewport.
    [[nodiscard]] QPoint cellCentre(int row, Column column) const;

    /// The centre of @p column's header section, in the header's viewport.
    [[nodiscard]] QPoint headerCentre(Column column) const;

    /// A point of the viewport below the last row.
    [[nodiscard]] QPoint belowTheRows() const;

    /// The names of the rows' simulations, from the top.
    [[nodiscard]] std::vector<std::string> rowNames() const;

    /// The names of the selected simulations, from the top.
    [[nodiscard]] std::vector<std::string> selectedNames() const;

    /// The ids of the columns shown, from the left, joined by ','.
    [[nodiscard]] std::string visibleIds() const;

    /// Moves @p column's header section to the place @p visual (QHeaderView::moveSection()).
    void moveColumn(Column column, int visual) const;

private:
    std::unique_ptr<OpenRocketDocument>  m_document;
    SimulationTableModel                 m_model;
    QAbstractItemModelTester             m_tester;
    std::unique_ptr<SimulationTableView> m_view;
};

}  // namespace QtRocket::Gui::Test
