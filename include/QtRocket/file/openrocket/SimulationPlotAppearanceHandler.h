#pragma once

#include <map>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the <plotappearance> element of a simulation: how the user wants single data series
/// of the simulation's plot drawn (OpenRocket's
/// file/openrocket/importt/SimulationPlotAppearanceHandler). The simulation's handler makes
/// one per <plotappearance> element and takes getPlotAppearances() when the simulation closes;
/// a second <plotappearance> element of a simulation is a second handler, whose appearances
/// replace the first's.
///
///     <plotappearance>
///       <series symbol="h" linestyle="dashed" red="255" green="0" blue="0" alpha="255"/>
///     </plotappearance>
///
/// A <series> element is one appearance, stored under its symbol attribute, which is the
/// symbol of the flight data type the series plots (FlightDataType::getSymbol()), taken as it
/// is written:
/// - without a symbol, or with one that is blank, the series is ignored with "Plot appearance
///   series missing symbol, ignoring.";
/// - linestyle is "solid", "dashed", "dotted" or "dashdot", read as DocumentConfig::findEnum()
///   reads an enum (trimmed, then compared exactly: "Dashed" is no style); anything else, and
///   no attribute, is no line style, without a warning;
/// - red, green and blue, and alpha when it is there, are the colour (Color::
///   fromXmlAttributes(): integers from 0 to 255, alpha 255 without the attribute); a missing
///   or unreadable one of them is no colour, without a warning;
/// - a series with neither a line style nor a colour is not stored (and does not take away an
///   appearance an earlier series of the same symbol stored);
/// - a later series of a symbol replaces an earlier one.
/// Text in a <series> is ignored, and a child of it gives PlainTextHandler's "Unknown element
/// <child>, ignoring." and DelegatorHandler's slip (the series then closes with the child's
/// attributes).
///
/// Any other child of <plotappearance> is ignored with "Unknown element '<name>', ignoring.",
/// which shifts the attributes of the elements around it by one (DelegatorHandler): the
/// simulation then ends with the attributes of the <plotappearance> element in place of its
/// own and does not see its status.
///
/// Deviations from OpenRocket:
/// - The appearances are kept in a std::map, ordered by symbol (Java: a HashMap); the
///   Simulation they go to keeps them the same way.
/// - A colour channel reads ASCII digits only (Color::fromXmlAttributes()); Integer.parseInt
///   also takes the decimal digits of other scripts.
class SimulationPlotAppearanceHandler final : public AbstractElementHandler
{
public:
    SimulationPlotAppearanceHandler() = default;

    /// The appearances read so far, by symbol (getPlotAppearances()); none of them is empty.
    [[nodiscard]] const std::map<std::string, PlotAppearance>& getPlotAppearances() const noexcept
    {
        return m_plotAppearances;
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// Stores the appearance of a <series>; never fails.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    std::map<std::string, PlotAppearance> m_plotAppearances;
};

}  // namespace QtRocket
