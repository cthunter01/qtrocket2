#include "QtRocket/gui/dialogs/simulation/SimulationExportPanel.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QGroupBox>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/util/WaitCursor.h"
#include "QtRocket/gui/widgets/CsvExportPanel.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/plot/PlotUtil.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// @p simulation, which must have flight data with a branch (Java's create(): "No data for
/// panel").
[[nodiscard]] std::shared_ptr<Simulation> checkedSimulation(std::shared_ptr<Simulation> simulation)
{
    if (simulation == nullptr || !simulation->hasSimulationData())
    {
        bug("No data for panel");
    }
    return simulation;
}

/// The types of the first branch of @p simulation's flight data, sorted (Java: data.getBranch(0)
/// .getTypes()).
[[nodiscard]] std::vector<const FlightDataType*> firstBranchTypes(const Simulation& simulation)
{
    return simulation.getSimulatedData()->getBranch(0).getTypes();
}

/// The labels of the branches of @p simulation's flight data (Util.generateSeriesLabels()).
[[nodiscard]] std::vector<QString> stageLabels(const Simulation& simulation)
{
    std::vector<QString> labels;
    for (const std::string& label : PlotUtil::generateSeriesLabels(*simulation.getSimulatedData()))
    {
        labels.push_back(QtStrings::fromUtf8(label));
    }
    return labels;
}

/// CsvExport::exportCsv() of @p branch of @p simulation into @p file, under the wait cursor.
[[nodiscard]] Result<void> exportUnderWaitCursor(const std::filesystem::path&           file,
                                                 Simulation&                            simulation,
                                                 const FlightDataBranch&                branch,
                                                 std::span<const FlightDataType* const> fields,
                                                 std::span<const Unit* const>           units,
                                                 const CsvExport::Options&              options)
{
    const WaitCursor wait;
    return CsvExport::exportCsv(file, simulation, branch, fields, units, options);
}

}  // namespace

SimulationExportPanel::SimulationExportPanel(const GuiServices&          services,
                                             std::shared_ptr<Simulation> simulation,
                                             QWidget*                    parent)
  : QWidget(parent),
    m_services(services),
    m_simulation(checkedSimulation(std::move(simulation))),
    m_types(firstBranchTypes(*m_simulation))
{
    // Java: SimulationConfigDialog's field exportTab.
    setObjectName(QStringLiteral("exportTab"));
    const Preferences& preferences = m_services.preferences();

    // Get the selected types from the preferences
    std::vector<CsvExportPanel::Row> rows;
    rows.reserve(m_types.size());
    for (const FlightDataType* const type : m_types)
    {
        rows.push_back(
            {.name      = flightDataTypeText(*type),
             .unitGroup = &type->getUnitGroup(),
             .selected = CsvOptionPreferences::isFlightDataTypeExportSelected(preferences, *type)});
    }

    m_csvOptions =
        makeChild<CsvOptionPanel>(this, std::string(CsvOptionPreferences::kSimulationExportPanel),
                                  commentOptions(), CsvOptionPanel::Arrangement::ON_NEW_ROWS);
    m_csvOptions->loadPreferences(preferences);

    //// Add series selection box
    // Only show the combo box if there are at least 2 entries (ie, "Main", and one other one)
    std::vector<QWidget*>      extras;
    const std::vector<QString> stages = stageLabels(*m_simulation);
    if (stages.size() > 1)
    {
        m_stagePanel = makeStagePanel(stages);
        extras.push_back(m_stagePanel);
    }

    m_exportPanel =
        makeChild<CsvExportPanel>(this, std::move(rows), m_csvOptions,
                                  CsvExportPanel::OptionsPlacement::RIGHT_COLUMN, extras);
    m_exportPanel->setObjectName(QStringLiteral("csvExportPanel"));
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_exportPanel);
}

std::vector<CsvOptionPanel::CommentOption> SimulationExportPanel::commentOptions()
{
    //: SimExpPan.checkbox.Includesimudesc
    QString simulationText = tr("Include simulation description");
    //: SimExpPan.checkbox.ttip.Includesimudesc
    QString simulationTip =
        tr("Include a comment at the beginning of the file describing the "
           "simulation.");
    //: SimExpPan.checkbox.Includefielddesc
    QString fieldText = tr("Include field descriptions");
    //: SimExpPan.checkbox.ttip.Includefielddesc
    QString fieldTip =
        tr("Include a comment line with the descriptions of the exported variables.");
    //: SimExpPan.checkbox.Incflightevents
    QString eventText = tr("Include flight events");
    //: SimExpPan.checkbox.ttip.Incflightevents
    QString eventTip = tr("Include a comment line for every flight event.");
    return {{.text = std::move(simulationText), .toolTip = std::move(simulationTip)},
            {.text = std::move(fieldText), .toolTip = std::move(fieldTip)},
            {.text = std::move(eventText), .toolTip = std::move(eventTip)}};
}

UserPrompts::SaveFileRequest SimulationExportPanel::exportRequest(const Preferences& preferences)
{
    UserPrompts::SaveFileRequest request;
    // FileHelper.CSV_FILTER, chosen, and the accept-all filter the SaveFileChooser keeps.
    request.nameFilters = QStringList{FileNames::csvFilter(), FileNames::allFilesFilter()};
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    // A SaveFileChooser.
    request.refuseIllegalNames = true;
    return request;
}

std::size_t SimulationExportPanel::stageIndex() const
{
    if (m_stageSelection == nullptr || m_stageSelection->currentIndex() < 0)
    {
        return 0;
    }
    return static_cast<std::size_t>(m_stageSelection->currentIndex());
}

QGroupBox* SimulationExportPanel::makeStagePanel(const std::vector<QString>& labels)
{
    //: SimExpPan.border.Stage
    auto* stagePanel = makeChild<QGroupBox>(this, tr("Stage to export"));
    stagePanel->setObjectName(QStringLiteral("stagePanel"));
    auto* layout     = makeChild<QVBoxLayout>(stagePanel);
    m_stageSelection = makeChild<QComboBox>(stagePanel);
    m_stageSelection->setObjectName(QStringLiteral("stageSelection"));
    for (const QString& label : labels)
    {
        m_stageSelection->addItem(label);
    }
    m_stageSelection->setCurrentIndex(0);
    layout->addWidget(m_stageSelection);
    return stagePanel;
}

bool SimulationExportPanel::doExport()
{
    // Nothing to export once a run of the settings dialog's Export left the simulation without
    // flight data (nothing is asked).
    if (!m_simulation->hasSimulationData())
    {
        return false;
    }
    Preferences& preferences = m_services.preferences();
    UserPrompts& prompts     = m_services.prompts();

    // A question runs an event loop, in which the panel may go with its dialog (with the design
    // window): nothing of the panel is used once it has gone.
    const QPointer<SimulationExportPanel>            alive(this);
    const std::optional<UserPrompts::SaveFileAnswer> answer =
        prompts.askSaveFile(this, exportRequest(preferences));
    if (alive.isNull() || !answer.has_value())
    {
        return false;
    }
    const std::filesystem::path file =
        FileNames::forceExtension(answer->file, QStringLiteral("csv"));
    // FileHelper.confirmWrite().
    std::error_code error;
    if (std::filesystem::exists(file, error))
    {
        const bool overwrite = prompts.askOverwrite(this, QtStrings::fromPath(file.filename()));
        if (alive.isNull() || !overwrite)
        {
            return false;
        }
    }

    const CsvOptionValues options = m_csvOptions->values();
    m_csvOptions->storePreferences(preferences);

    // Store preferences and export
    preferences.setDefaultDirectory(answer->directory);
    const std::vector<bool> selected = m_exportPanel->selected();
    for (std::size_t i = 0; i < m_types.size(); ++i)
    {
        CsvOptionPreferences::setFlightDataTypeExportSelected(preferences, *m_types.at(i),
                                                              selected.at(i));
    }
    return writeFile(file, options);
}

bool SimulationExportPanel::writeFile(const std::filesystem::path& file,
                                      const CsvOptionValues&       options)
{
    // OpenRocket bug 49: the flight data as it is now (the settings dialog's Export may just have
    // run the simulation again), held while it is written; never the branch of the data the panel
    // was made with.
    const std::shared_ptr<FlightData> flightData = m_simulation->getSimulatedData();
    if (flightData == nullptr || flightData->getBranchCount() == 0)
    {
        return false;
    }
    const std::size_t       stage = stageIndex();
    const FlightDataBranch& branch =
        flightData->getBranch(stage < flightData->getBranchCount() ? stage : 0);

    // The ticked rows' types and units, in the rows' order.
    const std::vector<bool>            selected = m_exportPanel->selected();
    const std::vector<const Unit*>     rowUnits = m_exportPanel->units();
    std::vector<const FlightDataType*> fields;
    std::vector<const Unit*>           units;
    for (std::size_t i = 0; i < m_types.size(); ++i)
    {
        if (selected.at(i))
        {
            fields.push_back(m_types.at(i));
            units.push_back(rowUnits.at(i));
        }
    }

    const Result<void> written =
        exportUnderWaitCursor(file, *m_simulation, branch, fields, units,
                              CsvOptionPreferences::toCsvExportOptions(options));
    if (!written.has_value())
    {
        // OpenRocket bug 53: Java shows its bug report dialog when the file cannot be opened and
        // nothing when a write fails.
        m_services.prompts().showMessage(
            this, {.severity = UserPrompts::Severity::ERROR,
                   //: error.writing.title
                   .title = tr("Error writing file"),
                   //: error.writing.desc
                   .lines = {tr("An error occurred while writing to the file:"),
                             QtStrings::fromUtf8(written.error().message)}});
        return false;
    }
    return true;
}

}  // namespace QtRocket::Gui
