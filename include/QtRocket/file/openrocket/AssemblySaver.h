#pragma once

#include "QtRocket/file/openrocket/RocketComponentSaver.h"

namespace QtRocket
{

class OrkWriter;
class Preferences;
class RocketComponent;

/// The .ork savers of the component assemblies (OpenRocket's file/openrocket/savers/
/// RocketSaver, AxialStageSaver and ComponentAssemblySaver). Each function gives the lines of a
/// component's element as Java's getElements() does (see RocketComponentSaver and
/// ComponentSaver), with RocketComponentSaver::addParams() first.
///
/// - rocketElements(): <rocket>, then <designer> (when not empty), <designtype>, <kitname> and
///   <revision> (when not empty), one <motorconfiguration configid[ default="true"]> per flight
///   configuration (Rocket::getIds(), the default left out) with its own <name> when it has one
///   (FlightConfiguration::isNameOverridden()) and a <stage number active/> per stage, then
///   <referencetype> and, for a CUSTOM reference, <customreference>.
/// - axialStageElements(), for AXIAL_STAGE and PARALLEL_STAGE (Java finds AxialStageSaver for
///   both): a stage that is "after" (isAfter()) is written as <stage> when it is an AxialStage
///   itself, one that is not as <parallelstage> when it is a ParallelStage; anything else gives
///   no lines (OpenRocketSaver then writes nothing). A stage of a number above 0 writes the
///   default separation (<separationevent>, <separationaltitude>, <separationdelay>), then one
///   <separationconfiguration configid> per configuration of its separation set, its lines with
///   a 4-space prefix (Java's).
/// - componentAssemblyElements(), for POD_SET: <podset> for a pod set that is not "after".
///
/// Deviations from OpenRocket:
/// - The name of a flight configuration is its own when FlightConfiguration::isNameOverridden()
///   says so for @p preferences, the application's (Java reads the application preferences
///   itself).
/// - The functions take the preferences (only rocketElements() reads them), the writer for its
///   escape(), and the component as a constant (see RocketComponentSaver).
class AssemblySaver final
{
public:
    AssemblySaver() = delete;

    using Elements = RocketComponentSaver::Elements;

    /// Java's RocketSaver.getElements().
    [[nodiscard]] static Elements rocketElements(const RocketComponent& component,
                                                 OrkWriter& writer, const Preferences& preferences);

    /// Java's AxialStageSaver.getElements().
    [[nodiscard]] static Elements axialStageElements(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences);

    /// Java's ComponentAssemblySaver.getElements().
    [[nodiscard]] static Elements componentAssemblyElements(const RocketComponent& component,
                                                            OrkWriter&             writer,
                                                            const Preferences&     preferences);
};

}  // namespace QtRocket
