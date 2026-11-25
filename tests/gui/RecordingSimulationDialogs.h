#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// The SimulationDialogs of the tests (TestGuiServices'): it shows no dialog, records what was
/// asked for (as values: the simulations by their names, a plot's configuration as a copy), and for
/// a settings dialog calls the test's edit, if it has one, as the user would edit while the dialog
/// is open.
class RecordingSimulationDialogs final : public SimulationDialogs
{
public:
    /// Which dialog was asked for.
    enum class Kind
    {
        /// editSimulations(): the settings dialog.
        EDIT,
        /// plotSimulation(): the plot dialog.
        PLOT,
    };

    /// One request, as it was made.
    struct Request
    {
        Kind kind = Kind::EDIT;
        /// The parent the dialog would have; only compared.
        const QWidget* parent = nullptr;
        /// The document of an EDIT (null for a PLOT); only compared.
        const OpenRocketDocument* document = nullptr;
        /// The names of the simulations (an EDIT's in order; a PLOT's one).
        QStringList simulationNames;
        /// An EDIT's tab and whether its simulation was just added.
        Tab  tab           = Tab::LAUNCH_CONDITIONS;
        bool newSimulation = false;
    };

    /// What the test does with an EDIT while the dialog would be open (its edits, as the user's).
    using Edit = std::function<void(QWidget* parent, OpenRocketDocument& document,
                                    const EditRequest& request)>;

    RecordingSimulationDialogs()                                             = default;
    ~RecordingSimulationDialogs() override                                   = default;
    RecordingSimulationDialogs(const RecordingSimulationDialogs&)            = delete;
    RecordingSimulationDialogs& operator=(const RecordingSimulationDialogs&) = delete;
    RecordingSimulationDialogs(RecordingSimulationDialogs&&)                 = delete;
    RecordingSimulationDialogs& operator=(RecordingSimulationDialogs&&)      = delete;

    /// Calls @p edit with every EDIT from now on (an empty one: none).
    void whileEditing(Edit edit) { m_edit = std::move(edit); }

    /// Every request so far, in order.
    [[nodiscard]] const std::vector<Request>& requests() const noexcept { return m_requests; }

    /// The configuration of each PLOT request so far, in order (a copy, as the plot dialog keeps
    /// its own).
    [[nodiscard]] const std::vector<SimulationPlotConfiguration>& plotConfigurations()
        const noexcept
    {
        return m_plotConfigurations;
    }

    void editSimulations(QWidget* parent, OpenRocketDocument& document,
                         const EditRequest& request) override;
    void plotSimulation(QWidget* parent, std::shared_ptr<Simulation> simulation,
                        const SimulationPlotConfiguration& configuration) override;

private:
    Edit                                     m_edit;
    std::vector<Request>                     m_requests;
    std::vector<SimulationPlotConfiguration> m_plotConfigurations;
};

}  // namespace QtRocket::Gui::Test
