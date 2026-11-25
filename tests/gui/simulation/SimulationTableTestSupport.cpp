#include "simulation/SimulationTableTestSupport.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QHeaderView>
#include <QPoint>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"

namespace QtRocket::Gui::Test
{

const Preferences& simulationExamplePreferences()
{
    return QtRocket::Test::goldenExampleEnvironment().preferences();
}

std::unique_ptr<OpenRocketDocument> loadSimulationExample(std::string_view file)
{
    const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example " << file;
        return nullptr;
    }
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return std::move(loaded->document);
}

SimulationTableFixture::SimulationTableFixture(std::unique_ptr<OpenRocketDocument> document)
  : m_document(std::move(document)),
    m_model(*m_document, simulationExamplePreferences(), nullptr),
    m_tester(&m_model, QAbstractItemModelTester::FailureReportingMode::Warning),
    m_view(std::make_unique<SimulationTableView>(m_model, nullptr))
{
    m_view->resize(1600, 400);
}

SimulationTableFixture::~SimulationTableFixture() = default;

void SimulationTableFixture::show()
{
    m_view->show();
    QCoreApplication::processEvents();
}

QPoint SimulationTableFixture::cellCentre(int row, Column column) const
{
    return m_view->visualRect(m_model.index(row, SimulationColumns::indexOf(column))).center();
}

QPoint SimulationTableFixture::headerCentre(Column column) const
{
    const QHeaderView& header  = *m_view->horizontalHeader();
    const int          section = SimulationColumns::indexOf(column);
    return {header.sectionViewportPosition(section) + (header.sectionSize(section) / 2),
            header.height() / 2};
}

QPoint SimulationTableFixture::belowTheRows() const
{
    const int rows = m_model.rowCount();
    const int last =
        rows == 0 ? 0 : m_view->rowViewportPosition(rows - 1) + m_view->rowHeight(rows - 1);
    return {m_view->columnViewportPosition(SimulationColumns::indexOf(Column::NAME)) + 10,
            last + 40};
}

std::vector<std::string> SimulationTableFixture::rowNames() const
{
    std::vector<std::string> names;
    for (int row = 0; row < m_model.rowCount(); ++row)
    {
        names.push_back(m_model.simulation(row)->getName());
    }
    return names;
}

std::vector<std::string> SimulationTableFixture::selectedNames() const
{
    std::vector<std::string> names;
    for (const std::shared_ptr<Simulation>& simulation : m_view->selectedSimulations())
    {
        names.push_back(simulation->getName());
    }
    return names;
}

std::string SimulationTableFixture::visibleIds() const
{
    std::string ids;
    for (const Column column : m_view->visibleColumns())
    {
        if (!ids.empty())
        {
            ids += ',';
        }
        ids += SimulationColumns::id(column);
    }
    return ids;
}

void SimulationTableFixture::moveColumn(Column column, int visual) const
{
    QHeaderView& header = *m_view->horizontalHeader();
    header.moveSection(header.visualIndex(SimulationColumns::indexOf(column)), visual);
}

}  // namespace QtRocket::Gui::Test
