#include "QtRocket/gui/util/SimulationBatch.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// The names of @p simulations (Java: simulationNames, read as the dialog is made); "" for a null
/// one, which start() refuses.
[[nodiscard]] std::vector<QString> namesOf(
    const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    std::vector<QString> names;
    names.reserve(simulations.size());
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        names.push_back(simulation == nullptr ? QString()
                                              : QtStrings::fromUtf8(simulation->getName()));
    }
    return names;
}

}  // namespace

SimulationBatch::SimulationBatch(UserPrompts&                              prompts,
                                 const UserPrompts::RunSimulationsRequest& request, QObject* parent)
  : SimulationBatch(prompts, request, Options{}, parent)
{
}

SimulationBatch::SimulationBatch(UserPrompts&                              prompts,
                                 const UserPrompts::RunSimulationsRequest& request, Options options,
                                 QObject* parent)
  : QObject(parent),
    m_prompts(&prompts),
    m_window(request.window),
    m_document(request.document),
    m_executor(request.executor),
    m_cancelBackgroundRuns(request.cancelBackgroundRuns),
    m_simulations(request.simulations),
    m_options(std::move(options)),
    m_completion(request.simulations.size())
{
    std::vector<QString> names = namesOf(m_simulations);
    m_entries.resize(m_simulations.size());
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        m_entries[i].simulation = m_simulations[i];
        m_entries[i].name       = std::move(names[i]);
    }
}

SimulationBatch::~SimulationBatch()
{
    // No callback of a job comes after this: they capture the batch.
    if (m_executor == nullptr)
    {
        return;
    }
    for (const Entry& entry : m_entries)
    {
        if (entry.job != SimulationExecutor::kNoJob)
        {
            m_executor->abandon(entry.job);
        }
    }
}

void SimulationBatch::start()
{
    if (m_started)
    {
        bug("A simulation batch is started once");
    }
    if (m_document == nullptr || m_executor == nullptr)
    {
        bug("A simulation batch needs a document and an executor");
    }
    // Java: IllegalArgumentException("Called with no simulations to run").
    if (m_simulations.empty())
    {
        bug("A simulation batch needs a simulation to run");
    }
    if (std::ranges::any_of(m_simulations, [](const std::shared_ptr<Simulation>& simulation) {
            return simulation == nullptr;
        }))
    {
        bug("A simulation batch's simulation is not null");
    }
    m_started = true;
    // "Randomize seeds unless the user requested reproducible simulation runs": every seed before
    // any run is made, as Java's constructor draws them.
    for (const std::shared_ptr<Simulation>& simulation : m_simulations)
    {
        simulation->getOptions().randomizeSeedIfNotFixed();
    }
    if (m_cancelBackgroundRuns)
    {
        m_cancelBackgroundRuns(m_simulations);
    }
    prepareRuns();
    submitRuns();
}

void SimulationBatch::prepareRuns()
{
    // All runs are prepared before the first is submitted: one that cannot be prepared (a
    // BugError) leaves nothing running.
    std::vector<Entry> prepared = m_entries;
    for (Entry& entry : prepared)
    {
        entry.run = std::make_shared<SimulationRun>(*entry.simulation);
        // Java's InteractiveSimulationWorker reads the burn-out estimate from the simulation's
        // configuration as it is made; the duplicate's is the same.
        entry.estimator.emplace(entry.run->getDuplicate().getActiveConfiguration());
        entry.sampler = m_options.sampleClock
                            ? std::make_shared<RunSampleListener>(m_options.sampleClock)
                            : std::make_shared<RunSampleListener>();
    }
    m_entries = std::move(prepared);
}

void SimulationBatch::submitRuns()
{
    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        Entry&                                           entry = m_entries[index];
        std::stop_source                                 stop;
        std::vector<std::shared_ptr<SimulationListener>> extraListeners;
        if (m_options.extraListeners)
        {
            extraListeners = m_options.extraListeners(index, stop.get_token());
        }
        SimulationExecutor::Callbacks callbacks{
            .progressed =
                [this, index](std::span<const RunSample> samples) {
                    runProgressed(index, samples);
                },
            .finished =
                [this, index](const std::exception_ptr& exception) {
                    runFinished(index, exception);
                }};
        // No callback comes from inside submit(): the job's id is known before.
        entry.job =
            m_executor->submit(m_window, entry.run, entry.sampler, std::move(extraListeners),
                               std::move(stop), std::move(callbacks));
    }
}

void SimulationBatch::cancel()
{
    if (m_finished)
    {
        return;
    }
    m_cancelled = true;
    if (!m_started)
    {
        m_finished = true;
        return;
    }
    // Java's cancelSimulations(): every worker cancelled, the queued ones purged. Their ends come
    // in a later turn of the executor; the ids are copied, as nothing changes them meanwhile.
    std::vector<SimulationExecutor::JobId> jobs;
    for (const Entry& entry : m_entries)
    {
        if (entry.job != SimulationExecutor::kNoJob)
        {
            jobs.push_back(entry.job);
        }
    }
    for (const SimulationExecutor::JobId job : jobs)
    {
        m_executor->cancel(job);
    }
}

bool SimulationBatch::isAllSuccessful() const noexcept
{
    return m_finished && !m_cancelled && m_started &&
           std::ranges::all_of(m_entries, [](const Entry& entry) {
               return entry.run != nullptr &&
                      entry.run->getOutcome() == SimulationRun::Outcome::SUCCEEDED;
           });
}

const QString& SimulationBatch::simulationName(std::size_t index) const
{
    return entry(index).name;
}

const SimulationProgressEstimator* SimulationBatch::estimator(std::size_t index) const
{
    const Entry& found = entry(index);
    return found.estimator.has_value() ? &*found.estimator : nullptr;
}

std::shared_ptr<const SimulationRun> SimulationBatch::run(std::size_t index) const
{
    return entry(index).run;
}

bool SimulationBatch::isSimulationDone(std::size_t index) const
{
    static_cast<void>(entry(index));
    return m_completion.isSimulationComplete(index);
}

int SimulationBatch::progress() const noexcept
{
    if (m_entries.empty())
    {
        return 0;
    }
    // Java: the sum of the workers' progress, divided by their number (an int division).
    int sum = 0;
    for (const Entry& entry : m_entries)
    {
        sum += entry.estimator.has_value() ? entry.estimator->getProgress() : 0;
    }
    return sum / static_cast<int>(m_entries.size());
}

const SimulationBatch::Entry& SimulationBatch::entry(std::size_t index) const
{
    if (index >= m_entries.size())
    {
        bug("A simulation of the batch is one of its simulations");
    }
    return m_entries[index];
}

void SimulationBatch::runProgressed(std::size_t index, std::span<const RunSample> samples)
{
    // Java's process(chunks), then updateProgress().
    Entry& entry = m_entries[index];
    if (entry.estimator.has_value())
    {
        entry.estimator->process(samples);
    }
    Q_EMIT progressed();
}

void SimulationBatch::runFinished(std::size_t index, const std::exception_ptr& exception)
{
    const QPointer<SimulationBatch> self(this);
    Entry&                          entry = m_entries[index];
    entry.job                             = SimulationExecutor::kNoJob;
    // Java's simulationDone() sets the progress to 1.0 (also after a failure or a cancel).
    if (entry.estimator.has_value())
    {
        entry.estimator->finish();
    }
    // Java's worker has recorded the run in the document's simulation before done().
    adopt(entry);
    // Java's simulationInterrupted(): the message first, then simulationDone(). The message runs
    // an event loop, in which the dialog (and the batch with it) may go.
    showFailure(entry, exception);
    if (self.isNull())
    {
        return;
    }
    const bool batchComplete = m_completion.markSimulationComplete(index);
    Q_EMIT progressed();
    if (self.isNull() || !batchComplete)
    {
        return;
    }
    finish(entry);
}

void SimulationBatch::adopt(const Entry& entry) const
{
    // Only while the simulation is still in the document; isAdoptable() refuses a run that was
    // stopped (bug 40) or that recorded nothing. A defect of the adoption is reported, and the
    // batch goes on (it must still end).
    if (!m_document->getSimulationIndex(*entry.simulation).has_value())
    {
        return;
    }
    Guard::run([&entry] { static_cast<void>(entry.run->adoptInto(*entry.simulation)); });
}

void SimulationBatch::showFailure(const Entry& entry, const std::exception_ptr& exception) const
{
    const SimulationRun& run = *entry.run;
    switch (run.getOutcome())
    {
        case SimulationRun::Outcome::SUCCEEDED:
        case SimulationRun::Outcome::CANCELLED:
            // Java ignores cancellations; an aborted flight is no exception.
            return;
        case SimulationRun::Outcome::FAILED:
        {
            QStringList lines;
            if (const std::optional<std::string>& branch = run.getCalculationBranchName();
                branch.has_value())
            {
                //: SimuRunDlg.msg.branchErrorOccurred
                lines.append(tr("An error occurred during simulation branch") +
                             QStringLiteral(" \"") + QtStrings::fromUtf8(*branch) +
                             QStringLiteral("\""));
            }
            else
            {
                //: SimuRunDlg.msg.errorOccurred
                lines.append(tr("An error occurred during the simulation:"));
            }
            lines.append(QtStrings::fromUtf8(run.getError().has_value() ? run.getError()->message
                                                                        : std::string()));
            m_prompts->showMessage(
                m_messageParent.data(),
                {.severity = UserPrompts::Severity::ERROR, .title = entry.name, .lines = lines});
            return;
        }
        case SimulationRun::Outcome::EXCEPTION:
        case SimulationRun::Outcome::NOT_RUN:
            // Java: Application.getExceptionHandler().handleErrorCondition(...).
            if (exception != nullptr)
            {
                Guard::report(Guard::failureOf(
                    exception, QStringLiteral("An exception occurred during the simulation")));
            }
            return;
    }
}

void SimulationBatch::finish(const Entry& entry)
{
    m_finished = true;
    // document.fireDocumentChangeEvent(new SimulationChangeEvent(simulation)): once per batch,
    // with the simulation whose end completed it. A listener's defect is reported; the batch
    // ends all the same.
    const QPointer<SimulationBatch> self(this);
    Guard::run([this, &entry] {
        m_document->fireDocumentChangeEvent(
            {.kind   = DocumentChangeEvent::Kind::SIMULATION,
             .source = DocumentChangeEvent::Source(entry.simulation.get())});
    });
    if (!self.isNull())
    {
        Q_EMIT finished();
    }
}

}  // namespace QtRocket::Gui
