#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "QtRocket/file/openrocket/FlightDataBranchHandler.h"
#include "QtRocket/file/openrocket/WarningHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// Reads the <flightdata> element of a simulation, its stored results (OpenRocket's
/// file/openrocket/importt/FlightDataHandler): the warnings of the flight, the branches of
/// data, or without a branch the ten summary values of its attributes. The simulation's
/// handler makes one per <flightdata> element and takes getFlightData() when the element has
/// closed; a second <flightdata> element of a simulation is a second handler, whose data
/// replace the first's.
///
///     <flightdata maxaltitude="279.136" maxvelocity="98.494" ... optimumdelay="5.573">
///       <warning type="HighSpeedDeployment"> ... </warning>
///       <databranch name="Sustainer" types="time,altitude,..."> ... </databranch>
///     </flightdata>
///
/// The children, in the order of the file:
/// - <warning>: a WarningHandler, which adds its warning to this handler's set. The events of
///   a branch find their warnings in that set, so the warnings stand before the branches (as
///   OpenRocket writes them); a warning behind a branch is still a warning of the flight data.
/// - <databranch>: a FlightDataBranchHandler for its name and types attributes. Without one of
///   the two the element is ignored with "Illegal flight data definition, ignoring.". A
///   types attribute that names no type or a type twice fails the load
///   (FlightDataBranchHandler::create()). The attributes optimumAltitude and
///   timeToOptimumAltitude (spelled so) are read with Double.parseDouble and are NaN when they
///   are no number. When the element closes, its branch is kept if it has at least one row; a
///   branch without rows is dropped with its events.
/// - any other child: "Unknown element '<name>' encountered, ignoring.", and it is ignored
///   with everything in it.
/// An ignored child shifts the attributes of the elements around it by one (DelegatorHandler):
/// the <flightdata> element then ends with the attributes of that child in place of its own,
/// so without a branch the summary values are read from the child's attributes (and are NaN
/// unless it has them), and the simulation's handler does not see the simulation's status.
/// An ignored element further down (in a <databranch>, in an <event>) does the same with the
/// attributes of the <databranch>. Text in the element is not looked at.
///
/// When the element closes:
/// - with at least one branch, the flight data are the branches, and the summary values are
///   computed from the first of them (FlightData::calculateInterestingValues()); the
///   attributes of the element are not read;
/// - without a branch (a file saved without its simulation data), the summary values are the
///   attributes maxaltitude, maxvelocity, maxacceleration, maxmach, timetoapogee, flighttime,
///   groundhitvelocity, launchrodvelocity, deploymentvelocity and optimumdelay, each read with
///   DocumentConfig::stringToDouble() and NaN when it is missing or no number.
/// The warnings are then added to the flight data, which is made immutable.
///
/// Nothing here is refused for not being finite (decision U3 of the loader concerns what a
/// simulation run takes as its input): the summary values, the optimum altitude and the data
/// points are loaded as OpenRocket loads them, NaN and infinities included.
///
/// Deviations from OpenRocket:
/// - The flight data are handed out as a std::shared_ptr, which is what the Simulation
///   constructor takes; Java's getFlightData() returns the object.
/// - The handler takes the loading context only; Java's also takes the simulation's handler,
///   through which its branch handlers reach this handler's warnings and the document.
/// - The warnings of the flight data are copies of the ones in the handler's set (a
///   WarningSet owns its warnings), with the same ids; in Java they are the same objects.
class FlightDataHandler final : public AbstractElementHandler
{
public:
    /// A handler of a <flightdata> element of a simulation of the document of @p context,
    /// which must outlive the handler.
    /// @throws BugError when @p context has no document
    explicit FlightDataHandler(const DocumentLoadingContext& context);

    /// The flight data: null until the element has closed (getFlightData()). They are
    /// immutable.
    [[nodiscard]] const std::shared_ptr<FlightData>& getFlightData() const noexcept
    {
        return m_data;
    }

    /// The warnings read so far (getWarningSet()).
    [[nodiscard]] const WarningSet& getWarningSet() const noexcept { return m_warningSet; }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// Keeps the branch of a <databranch> that has rows.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Makes the flight data (see the class comment); never fails.
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    const DocumentLoadingContext* m_context;

    /// Declared before the handlers, which refer to it.
    WarningSet                                     m_warningSet;
    std::unique_ptr<WarningHandler>                m_warningHandler;
    std::unique_ptr<FlightDataBranchHandler>       m_dataHandler;
    std::vector<std::shared_ptr<FlightDataBranch>> m_branches;
    std::shared_ptr<FlightData>                    m_data;
};

}  // namespace QtRocket
