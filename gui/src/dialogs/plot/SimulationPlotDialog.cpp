#include "QtRocket/gui/dialogs/plot/SimulationPlotDialog.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QObject>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/plot/PlotDialog.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// SimulationPlot.create(): the model of @p simulation's flight data as @p configuration says.
[[nodiscard]] SimulationPlotModel modelOf(const Simulation&                  simulation,
                                          const SimulationPlotConfiguration& configuration)
{
    QTROCKET_ASSERT(simulation.getSimulatedData() != nullptr);
    return {simulation.getName(), simulation.getSimulatedData(), configuration,
            SimulationPlot::translator()};
}

}  // namespace

SimulationPlotDialog::SimulationPlotDialog(const GuiServices& services, SimulationPlotModel model,
                                           QWidget* parent)
  : PlotDialog(services, QString::fromLatin1(kGeometryName), parent),
    // getPlot(): the preference "Show data points" (false by default).
    m_plot(makeChild<SimulationPlot>(this, services, std::move(model),
                                     PlotPreferences::isShowPoints(services.preferences()))),
    m_checkErrors(makeShowErrorsBox()),
    m_stageSelection(makeStageBox()),
    m_timeSeriesWarning(makeTimeSeriesWarning())
{
    setWindowTitle(QtStrings::fromUtf8(m_plot->model().getTitle()));
    // getPlot(): the preference "Show events" (true by default).
    build(*m_plot, PlotPreferences::isShowEvents(services.preferences()),
          {.messages = {m_timeSeriesWarning}, .controls = {m_checkErrors, m_stageSelection}});
    // this.checkErrors.setVisible(this.simulation.hasErrors()). Only show the combo box if there
    // are at least 3 entries (ie, "All", "Main", and one other one); Java leaves it out of the
    // panel.
    m_checkErrors->setVisible(hasErrors());
    m_stageSelection->setVisible(m_stageSelection->count() > 2);
}

SimulationPlotDialog::SimulationPlotDialog(const GuiServices&                 services,
                                           const Simulation&                  simulation,
                                           const SimulationPlotConfiguration& configuration,
                                           QWidget*                           parent)
  : SimulationPlotDialog(services, modelOf(simulation, configuration), parent)
{
}

SimulationPlotDialog& SimulationPlotDialog::showPlot(
    const GuiServices& services, const Simulation& simulation,
    const SimulationPlotConfiguration& configuration, QWidget& parent)
{
    auto* dialog = makeChild<SimulationPlotDialog>(&parent, services, simulation, configuration);
    // DISPOSE_ON_CLOSE: Close, Escape and the window's close button all finish the dialog.
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->open();
    return *dialog;
}

bool SimulationPlotDialog::hasErrors(std::optional<std::size_t> branch) const
{
    // Simulation.hasErrors(branch): the branch has a SIM_ABORT event.
    return !m_plot->model().getAbortLines(branch).empty();
}

StyledLabel* SimulationPlotDialog::makeTimeSeriesWarning()
{
    // Add warning if X axis type is not time
    if (m_plot->model().hasTimeDomain())
    {
        return nullptr;
    }
    auto* warning = makeChild<StyledLabel>(
        this,
        //: PlotDialog.lbl.timeSeriesWarning (its trailing space as OpenRocket's)
        tr("The data is plotted in time order even though the X axis type is not time. "),
        kSmallTextSize);
    warning->setObjectName(QStringLiteral("timeSeriesWarning"));
    warning->setTextFormat(Qt::PlainText);
    warning->setFontColor(ThemeColors::color(ThemeColors::Key::DARK_ERROR, warning->palette()));
    return warning;
}

QCheckBox* SimulationPlotDialog::makeShowErrorsBox()
{
    //// Show errors if any
    //// Always enable 'show errors' initially; make user turn it off for themselves
    //: PlotDialog.CheckBox.ShowErrors
    auto* box = makeChild<QCheckBox>(this, tr("Show errors"));
    box->setObjectName(QStringLiteral("checkErrors"));
    box->setChecked(true);
    connect(
        box, &QCheckBox::clicked, this,
        guarded([this](bool show) { m_plot->setShowErrors(show); }, QStringLiteral("Show errors")));
    return box;
}

QComboBox* SimulationPlotDialog::makeStageBox()
{
    auto* box = makeChild<QComboBox>(this);
    box->setObjectName(QStringLiteral("stageSelection"));
    //: PlotDialog.StageDropDown.allStages
    box->addItem(tr("All Stages"));
    for (const std::string& label : m_plot->model().getSeriesLabels())
    {
        box->addItem(QtStrings::fromUtf8(label));
    }
    connect(box, &QComboBox::currentIndexChanged, this,
            guarded([this](int index) { chooseStage(index); }, QStringLiteral("Stage")));
    return box;
}

void SimulationPlotDialog::chooseStage(int index)
{
    if (index < 0)
    {
        // No item: the box was emptied (never: it keeps its items).
        return;
    }
    const std::optional<std::size_t> stage =
        index == 0 ? std::nullopt : std::optional(static_cast<std::size_t>(index - 1));
    m_checkErrors->setEnabled(hasErrors(stage));
    m_plot->setShowBranch(stage);
}

}  // namespace QtRocket::Gui
