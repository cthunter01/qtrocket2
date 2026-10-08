#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/FlightDataHandler.h"
#include "QtRocket/file/openrocket/LandingDispersionSettingsHandler.h"
#include "QtRocket/file/openrocket/SimulationConditionsHandler.h"
#include "QtRocket/file/openrocket/SimulationPlotAppearanceHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class OpenRocketDocument;
class Simulation;
class SimulationExtension;

/// Reads one <simulation> element of a design file and adds the simulation it describes to
/// the document (OpenRocket's file/openrocket/importt/SingleSimulationHandler). The handler of
/// the <simulations> element makes one per <simulation>.
///
///     <simulation status="uptodate">
///       <name>Simulation 1</name>
///       <simulator>RK4Simulator</simulator>
///       <calculator>BarrowmanCalculator</calculator>
///       <conditions> ... </conditions>
///       <extension extensionid="..."> <entry key="..." type="...">...</entry> </extension>
///       <flightdata ...> ... </flightdata>
///     </simulation>
///
/// The children, each applied when it closes, in the order of the file:
/// - <name>: the name, the text as it is (not trimmed); the last one counts. Without the
///   element the name is "Simulation".
/// - <simulator>, <calculator>: nothing is chosen by them. A trimmed text other than
///   "RK4Simulator" and "BarrowmanCalculator" gives "Unknown simulator '<text>' specified,
///   ignoring." and "Unknown calculator '<text>' specified, ignoring.".
/// - <listener>: the simulation listener class that a file of the formats up to 1.6 names
///   (OpenRocket before 15.03, which brought the extensions). A text that is not blank adds a
///   JavaCode extension with the trimmed text as its class name (an extension that keeps the
///   name and cannot run: see JavaCode).
/// - <conditions>: a SimulationConditionsHandler, new for every such element, so that a
///   second <conditions> replaces the first altogether: the options and the flight
///   configuration id of the simulation.
/// - <extension>: a ConfigHandler for its <entry> children, and when the element closes with
///   an extensionid attribute that is not blank, one extension more, in the order of the file
///   among the listeners and extensions. The id is the attribute as it is, with every
///   "net.sf.openrocket" replaced by "info.openrocket.core" (the package of OpenRocket's
///   classes up to 23.09 and the one since). The extension registry of the loading context makes
///   the extension (SimulationExtensionRegistry::create()), which is given the entries as its
///   configuration. For an id no provider knows, and with no registry in the context, the warning
///   is "Simulation extension with id '<id>' not found." and the extension is kept as an
///   UnknownSimulationExtension with the id and the entries (see "Deviations"). An <extension>
///   without the attribute, or with a blank one, is passed over silently.
/// - <flightdata>: a FlightDataHandler, new for every such element: the stored results. A
///   second <flightdata> replaces the first.
/// - <plotappearance>: a SimulationPlotAppearanceHandler, new for every such element.
/// - <landingdispersion>: a LandingDispersionSettingsHandler, new for every such element.
/// - any other child: "Unknown element '<name>', ignoring.", and it is ignored with
///   everything in it.
/// The attributes and the text of the children are otherwise not looked at, and there is no
/// warning for them.
///
/// An ignored child, here or anywhere below (an unknown child of <flightdata>, a child of a
/// <name>), shifts the attributes of the elements around it by one (DelegatorHandler's
/// bookkeeping, which is OpenRocket's): the <simulation> then ends with the attributes of
/// another element in place of its own, so its status is unknown and the simulation OUTDATED,
/// and the elements around it get "Unknown attributes in element ..." warnings. One unknown
/// child of a <simulation> so gives four warnings. Nothing here corrects that (decision L1).
///
/// When the element ends (endHandler()), in this order:
/// 1. The status attribute is read as DocumentConfig::findEnum() reads an enum ("uptodate",
///    "loaded", "outdated", "external", "notsimulated", "cantrun", "aborted"). Anything else,
///    and no attribute, is OUTDATED with "Simulation status unknown, assuming outdated.".
/// 2. Without a <conditions> element: "Simulation conditions not defined, using defaults.".
/// 3. The status the simulation gets: NOT_SIMULATED without a <flightdata> element, whatever
///    the attribute says; with one, OUTDATED when the attribute said so or was unknown, and
///    LOADED for every other value (also for "notsimulated", "cantrun" and "aborted": what the
///    file calls the status is recomputed by Simulation::getStatus()).
/// 4. The load fails when there is no flight configuration id (see "Failures").
/// 5. The Simulation is made with the constructor for loaded simulations: the document and
///    its rocket, the status, the name, the options (moved out of the conditions handler), the
///    extensions, the flight data, the plot appearances and the preference store of the
///    loading context. It then gets the flight configuration id of <configid>
///    (Simulation::setFlightConfigurationId(), which creates the configuration in the rocket
///    when the rocket lacks it; an empty <configid/> is a new, random id), the landing
///    dispersion settings when there was a <landingdispersion> element, and is added to the
///    document (OpenRocketDocument::addSimulation()).
/// One thing reaches outside the document: a <simulationsteppermethod> in the conditions
/// writes the stepper it names into the preference store of the loading context, as
/// OpenRocket's loader writes it into the application's preferences (decision L8; see
/// SimulationConditionsHandler). It is written when the element is read, so also when the
/// load fails later.
///
/// What the loader does after the whole document (OpenRocketLoader) is not done here: for
/// every simulation Simulation::syncModId(), and SimulationExtension::documentLoaded() of
/// every extension. Until syncModId() the simulated modification id of a simulation is the one
/// of the rocket's selected configuration (the constructor takes it before the simulation gets
/// its own configuration), so Simulation::getStatus() of a LOADED simulation says OUTDATED
/// when its configuration has another modification id than the selected one.
///
/// Failures, which end the load (decision L2; ErrorCode::INVALID_ARGUMENT with Java's message):
/// - a simulation without a flight configuration id, "Attempted to set the configuration to
///   an error id. Not Allowed!": no <conditions> element, a <conditions> without <configid>,
///   and a <configid> that spells out the error id (decision L8). Nothing was added to the
///   document for that simulation; the simulations before it stay;
/// - whatever a child handler fails with (see SimulationConditionsHandler and
///   FlightDataHandler).
///
/// Deviations from OpenRocket:
/// - An extension whose id no provider knows is kept (decisions D11 and L10); OpenRocket
///   gives the same warning and drops it, so that a save loses it.
/// - The list entries of an extension's configuration are loaded (see ConfigHandler);
///   OpenRocket drops them.
/// - A <configid> that spells out the error id ("ffffffff-f4f2-f1f0-0000-0000000009b9") fails
///   the load here. In OpenRocket it is an id like any other, because only the one ERROR_FCID
///   object counts as the error id there, not an id equal to it (see FlightConfigurationId);
///   OpenRocket never writes that id.
/// - Non-finite numbers of the <conditions> are not applied (see SimulationConditionsHandler),
///   so the simulation's options always equal their copy, the simulated conditions.
/// - Without a <landingdispersion> element the simulation has no landing dispersion settings,
///   as in OpenRocket; with one it always has what the element holds, where OpenRocket has
///   none when it refuses the element (see LandingDispersionSettingsHandler).
/// - The constructor takes the loading context only (Java: the document and the context; the
///   document is the context's), and getWarningSet(), through which Java's flight data branch
///   handler reaches the flight data handler's warnings, is not needed (see
///   FlightDataBranchHandler::create()). getSimulation() is an addition.
/// - Without a document or a preference store in the context the constructor is a BugError
///   (Java asks the application's preferences, which are always there).
class SingleSimulationHandler final : public AbstractElementHandler
{
public:
    /// A handler of one <simulation> element of the document of @p context. The context and
    /// what it points at must outlive the handler; its preference store must outlive the
    /// simulation the handler makes (see DocumentLoadingContext::getPreferences()).
    /// @throws BugError when @p context has no document or no preference store
    explicit SingleSimulationHandler(const DocumentLoadingContext& context);

    /// The document the simulation is added to (getDocument()).
    [[nodiscard]] OpenRocketDocument& getDocument() const noexcept { return *m_document; }

    /// The simulation this handler made and added to the document: null until the element has
    /// ended, after a failure, and once the simulation is gone (the handler does not keep it
    /// alive: the document owns it). (An addition: Java's callers find it in the document.)
    [[nodiscard]] std::shared_ptr<Simulation> getSimulation() const noexcept
    {
        return m_simulation.lock();
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// Applies a plain child and adds the extension of an <extension>; never fails.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Makes the simulation and adds it to the document (see the class comment).
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    /// closeElement() for an <extension> that closed with @p attributes.
    void closeExtension(const Attributes& attributes, WarningSet& warnings);

    const DocumentLoadingContext* m_context;
    OpenRocketDocument*           m_document;

    /// Java: null until a <name> closed.
    std::optional<std::string> m_name;

    std::unique_ptr<SimulationConditionsHandler>      m_conditionHandler;
    std::unique_ptr<ConfigHandler>                    m_configHandler;
    std::unique_ptr<FlightDataHandler>                m_dataHandler;
    std::unique_ptr<SimulationPlotAppearanceHandler>  m_plotAppearanceHandler;
    std::unique_ptr<LandingDispersionSettingsHandler> m_landingDispersionSettingsHandler;

    std::vector<std::shared_ptr<SimulationExtension>> m_extensions;
    /// The simulation made; not owned, so that a handler that outlives the document's
    /// simulation does not keep it (and its references to the rocket) alive.
    std::weak_ptr<Simulation> m_simulation;
};

}  // namespace QtRocket
