#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class Rocket;

/// Reads one <motorconfiguration> element of the rocket (and the <flightconfiguration> element
/// of one development version, which is the same): one flight configuration, its name and
/// which stages are active in it (OpenRocket's
/// file/openrocket/importt/MotorConfigurationHandler).
///
/// The children:
/// - <name>, once: the name, the text as it is. A second <name> is ignored with
///   Warning::kFileInvalidParameter.
/// - <stage number="..." active="...">, any number of them: whether the stage of that number
///   is active. The number is Integer.parseInt of the attribute; a number that is none, or no
///   attribute, fails the load (ErrorCode::INVALID_ARGUMENT with the message of Java's
///   NumberFormatException, DocumentConfig::parseInt()). active is Boolean.parseBoolean: true
///   for "true" in any letter case, without trimming, false for everything else and for no
///   attribute. A later <stage> of the same number replaces the earlier one.
/// - any other child is ignored with Warning::kFileInvalidParameter.
/// Neither the text nor other attributes of a <name> or <stage> are warned of.
///
/// Everything is applied when the element closes (endHandler()):
/// - the id is DocumentConfig::configurationId() of the element: a new random id without the
///   attribute "configid" or with an empty one, and the id OpenRocket makes of a text that is
///   no UUID, as the oldest files have them. An id that is not valid gives
///   Warning::kFileInvalidParameter, and nothing else happens;
/// - the rocket gets a flight configuration of that id when it has none
///   (Rocket::createFlightConfiguration(), a TREE_CHANGE event);
/// - a name that is not blank (String.trim()) becomes the configuration's name, untrimmed;
/// - when there are <stage> entries and none of them said "active", stage 0 is made active;
/// - every entry is handed to FlightConfiguration::preloadStageActiveness(): the stages do not
///   exist yet, the <motorconfiguration> elements standing before the <subcomponents>, so the
///   top-level loader applies the entries when the whole file has been read
///   (applyPreloadedStageActiveness());
/// - default="true", exactly so, selects the configuration
///   (Rocket::setSelectedConfiguration());
/// - AbstractElementHandler's warnings for the element's own text and for attributes other
///   than configid and default.
///
/// Because an ignored child shifts DelegatorHandler's bookkeeping, a <motorconfiguration> with
/// a child that is neither <name> nor <stage> is read with that child's attributes: it loses
/// its configid, gets a random id and is not selected, as in OpenRocket.
///
/// Deviations from OpenRocket:
/// - The "not valid" warning cannot be given in Java, where an id made from a text never is
///   the error id (see FlightConfigurationId); here it is given for a configid that spells out
///   the error id's key. A configid that spells out the key of the default id names the
///   rocket's default configuration here as in Java: its name and stage entries go to that
///   one.
/// - A flight configuration the rocket does not have yet is not made when it would take the
///   rocket beyond DocumentConfig::kMaxInstances component instances over all its flight
///   configurations (DocumentConfig::flightConfigurationFits()): Warning::kFileInvalidParameter,
///   and nothing else happens, as for an id that is not valid. Every flight configuration
///   keeps the instances of the whole rocket and builds them anew at every change, so 800
///   empty <motorconfiguration/> elements beside one launch lug of 10000 instances held 4 GB.
///   OpenRocket has no bound and runs out of memory.
/// - Java removes configid and default from the attribute map before it warns of the others;
///   the attributes being const here, the warning is decided on a copy without the two.
/// - The constructor takes the loading context, as every handler of the loader does, and does
///   not need it; neither does Java's.
class MotorConfigurationHandler final : public AbstractElementHandler
{
public:
    /// The handler of a <motorconfiguration> element of @p rocket, which must outlive it.
    MotorConfigurationHandler(Rocket& rocket, const DocumentLoadingContext& context) noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    Rocket*                    m_rocket;
    std::optional<std::string> m_name;
    /// Whether a <name> has been opened; never reset, so a second one is refused.
    bool m_inNameElement{false};
    /// The <stage> entries by stage number (Java: a HashMap, whose order plays no part).
    std::map<int, bool> m_stageActiveness;
    /// Whether any <stage> entry said "active", also one that a later entry replaced.
    bool m_hasActiveStage{false};
};

}  // namespace QtRocket
