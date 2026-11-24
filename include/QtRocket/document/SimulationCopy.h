#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

class OpenRocketDocument;
class Preferences;
class Simulation;

/// Simulations taken out of a document as detached copies: what the application's clipboard
/// keeps after Copy and Cut of simulations, and what Paste makes simulations of (OpenRocket: the
/// Simulation.copy() of each selected simulation that OpenRocketClipboard keeps, and the
/// Simulation.duplicateSimulation(rocket) of each that SimulationPanel's paste adds). It is
/// Qt-free, the ComponentCopy of simulations; its callers are the GUI's clipboard and the
/// simulation table's editing.
///
/// Taking a copy (copyOf()): of each simulation, what Java's copy() keeps and
/// duplicateSimulation() carries on into the paste (Simulation.java:650-683, 802-838): the name,
/// the flight configuration id, a copy of the options (SimulationOptions' copy constructor,
/// Java's clone()), clones of the extensions (SimulationExtension::clone()), the landing
/// dispersion settings, a copy of the plot appearances, and the preference store the simulation
/// was made with (Simulation::getPreferences(), a pointer: the store outlives every simulation
/// and every copy). Nothing that was simulated: neither the status, the simulated conditions,
/// the configuration description nor the data (Java's copy() drops them, and the paste starts
/// NOT_SIMULATED with the copy's null description). Nothing of the copy refers to the source
/// simulation, its rocket or its document, so the copy may outlive a design that is closed
/// before the paste. (Simulation::copy() keeps the source rocket and document, which must then
/// outlive the copy: see Simulation.)
///
/// Making simulations of it (makeSimulation(), insertInto()): a new Simulation of the target
/// document's rocket, with the target as its document and the copy's preference store
/// (Simulation(document, rocket, preferences): the options start from the store's defaults, or
/// from the built-in ones without a store), and then, in this order, the name (setName()), the
/// flight configuration id (setFlightConfigurationId(), which creates the configuration in the
/// target's rocket when the rocket lacks it, as Java's addSimulation() does), the conditions of
/// the copied options (copySimulationOptionsFrom(): SimulationOptions::copyConditionsFrom() onto
/// the new options, as Java's duplicateSimulation()), the landing dispersion settings, clones of
/// the copy's extensions (the copy keeps its own, so it can be pasted any number of times) and
/// the plot appearances (setPlotAppearances()). The new simulation is NOT_SIMULATED.
///
/// OpenRocket bug 22: Java's duplicateSimulation() makes the pasted simulation with
/// `new Simulation(this.document, newRocket)`, the document of the simulation that was copied,
/// which becomes its change listener: a pasted simulation's later changes mark the design it was
/// copied from unsaved, and the design it was pasted into stays saved. Here the document listens
/// to the simulations in its own list (see OpenRocketDocument), so the target hears a pasted
/// simulation and the source does not, and getDocument() is the target.
///
/// Undo: call OpenRocketDocument::addUndoPosition() before insertInto(), with Java's texts
/// ("Duplicate <name>" for one simulation, the name of the first copy, or "Duplicate
/// simulations"; Java's paste has no texts of its own), as SimulationPanel does. The flight
/// configurations makeSimulation() creates then belong to that step, as in Java.
///
/// Deviations from OpenRocket:
/// - The plot appearances are copied when the copy is taken. Java's copy() shares the plot
///   appearance map with its source (Object.clone()), so an appearance set on the source after
///   Copy shows in a later paste, and one set on the clipboard's copy shows in the source.
/// - makeSimulation() creates a missing flight configuration in the target's rocket. Java's
///   duplicateSimulation() sets the id without it and addSimulation() creates it, so within
///   insertInto() only the order of the events differs: the rocket's events of the new
///   configuration come before the simulation enters the list, where Java's come right after.
/// - The name rule of insertInto() (hasNumberedName()) reads ASCII digits, as Java's regex
///   [0-9] does; getNextSimulationName() reads ASCII digits only too (see OpenRocketDocument).
/// - Not copyable; movable.
class SimulationCopy
{
public:
    /// What the copy keeps of one simulation (see the class comment).
    struct Entry
    {
        /// Simulation::getName().
        std::string name;
        /// Simulation::getFlightConfigurationId().
        FlightConfigurationId configurationId{FlightConfigurationId::errorId()};
        /// A copy of Simulation::getOptions().
        SimulationOptions options;
        /// Clones of Simulation::getSimulationExtensions(), in order.
        std::vector<std::unique_ptr<const SimulationExtension>> extensions;
        /// Simulation::getLandingDispersionSettings().
        std::optional<LandingDispersionSettings> landingDispersionSettings;
        /// Simulation::getPlotAppearances().
        std::map<std::string, PlotAppearance> plotAppearances;
        /// Simulation::getPreferences(), or null.
        Preferences* preferences{nullptr};
    };

    /// A copy of nothing (the clipboard before any simulation was copied).
    SimulationCopy()  = default;
    ~SimulationCopy() = default;

    SimulationCopy(const SimulationCopy&)                = delete;
    SimulationCopy& operator=(const SimulationCopy&)     = delete;
    SimulationCopy(SimulationCopy&&) noexcept            = default;
    SimulationCopy& operator=(SimulationCopy&&) noexcept = default;

    /// A copy of @p simulations, in that order (see the class comment). Reads them on the thread
    /// that owns them.
    /// @throws BugError when a simulation is null or holds a null extension
    [[nodiscard]] static SimulationCopy copyOf(const std::vector<const Simulation*>& simulations);

    /// The number of copied simulations.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }
    /// Whether no simulation was copied.
    [[nodiscard]] bool empty() const noexcept { return m_entries.empty(); }

    /// What the copy keeps of the simulation at @p index, below size().
    /// @throws BugError when @p index is out of range
    [[nodiscard]] const Entry& getEntry(std::size_t index) const;

    /// A new simulation of @p target made of the copy at @p index (see the class comment), not
    /// yet added to @p target. Java: duplicateSimulation(target.getRocket()) of the clipboard's
    /// copy, with the target as the document (bug 22).
    /// @throws BugError when @p index is out of range, or the copy's configuration id is the
    ///         error id (no simulation has it: Simulation::setFlightConfigurationId() refuses it)
    [[nodiscard]] std::shared_ptr<Simulation> makeSimulation(std::size_t         index,
                                                             OpenRocketDocument& target) const;

    /// Pastes the copy into @p target at @p index (0 to its simulation count): Java's
    /// SimulationPanel.duplicateSimulation(Simulation[], int) without the undo position and the
    /// selection. For each copy in order: makeSimulation(); when its name is a numbered one
    /// (hasNumberedName()), the name becomes target.getNextSimulationName(), asked after the
    /// earlier copies went in; then OpenRocketDocument::addSimulation() at @p index, @p index + 1
    /// and so on (one SIMULATION event each). Returns the simulations in that order. The copy
    /// keeps its entries.
    /// @throws BugError when @p index is above the simulation count of @p target, also when a
    ///         slot of an earlier insertion's event removed simulations so that the next index no
    ///         longer fits (what went in stays in)
    std::vector<std::shared_ptr<Simulation>> insertInto(OpenRocketDocument& target,
                                                        std::size_t         index) const;

    /// Whether @p name is a numbered simulation name, which a paste or a duplicate replaces with
    /// the next free one: Java's name.matches("Simulation [0-9]+ *"), the whole name, so
    /// "Simulation 12" and "Simulation 9  " are, "Simulation 3 - too short delay", "Simulation"
    /// and "Simulation -1" are not (OpenRocketDocument::kSimulationNamePrefix and ASCII
    /// digits, then spaces only).
    [[nodiscard]] static bool hasNumberedName(std::string_view name) noexcept;

private:
    explicit SimulationCopy(std::vector<Entry> entries) noexcept;

    std::vector<Entry> m_entries;
};

}  // namespace QtRocket
