#pragma once

#include "QtRocket/file/openrocket/RocketComponentSaver.h"

namespace QtRocket
{

class OrkWriter;
class Preferences;
class RocketComponent;

/// The .ork savers of the internal components (OpenRocket's file/openrocket/savers/
/// InternalComponentSaver and its subclasses: StructuralComponentSaver, RingComponentSaver,
/// ThicknessRingComponentSaver, RadiusRingComponentSaver, TubeCouplerSaver, EngineBlockSaver,
/// InnerTubeSaver, CenteringRingSaver, BulkheadSaver, MassObjectSaver, MassComponentSaver,
/// ShockCordSaver, RecoveryDeviceSaver, ParachuteSaver, StreamerSaver). Each ...Elements()
/// function gives the lines of a component's element as Java's getElements() does (see
/// RocketComponentSaver and ComponentSaver), the parameters of the class chain in Java's order:
/// - every internal component: RocketComponentSaver::addParams() (InternalComponentSaver adds
///   nothing);
/// - a structural component: <material>; a ring component: <length>, <radialposition>,
///   <radialdirection> in degrees; a thickness ring (tube coupler, engine block, inner tube):
///   <outerradius> ("auto" alone when automatic), <thickness>; a radius ring (centering ring,
///   bulkhead): <outerradius> and, but for a bulkhead, <innerradius> ("auto" alone when
///   automatic);
/// - an inner tube: <clusterconfiguration>, <clusterscale>, <clusterrotation> in degrees, then
///   the <motormount> of RocketComponentSaver::motorMountParams() when it acts as a motor mount;
/// - a mass object: <packedlength>, <packedradius> ("auto " and the value when automatic),
///   <radialposition>, <radialdirection> in degrees; then a mass component's <mass> and
///   <masscomponenttype>, a shock cord's <cordlength> ("auto" alone when automatic) and
///   <material>, a recovery device's <cd> ("auto" alone when automatic), <material>,
///   <isdrogue>true</isdrogue> for a drogue, the default deployment (<deployevent>,
///   <deployaltitude>, <deploydelay>) and one <deploymentconfiguration configid> per
///   configuration that is not the default, its lines with a 2-space prefix (Java's); then a
///   parachute's <diameter>, <linecount>, <linelength> ("auto" alone when automatic),
///   <linematerial>, a streamer's <striplength> and <stripwidth>.
///
/// Deviations from OpenRocket:
/// - A mass component's <mass> is its own mass (getComponentMass()), the user's decision E1 of
///   2026-10-14: Java writes getMass(), which is the override mass when the mass is overridden,
///   so that a reload of OpenRocket's file has lost the component's own mass (the override is
///   written as <overridemass> all the same). QtRocket's file gives both back.
/// - Those of RocketComponentSaver; the functions take the preferences (none of them reads
///   them) so that all savers have the same signature (ComponentSaver).
class InternalComponentSaver final
{
public:
    InternalComponentSaver() = delete;

    using Elements = RocketComponentSaver::Elements;

    /// Java's TubeCouplerSaver.getElements().
    [[nodiscard]] static Elements tubeCouplerElements(const RocketComponent& component,
                                                      OrkWriter&             writer,
                                                      const Preferences&     preferences);

    /// Java's EngineBlockSaver.getElements().
    [[nodiscard]] static Elements engineBlockElements(const RocketComponent& component,
                                                      OrkWriter&             writer,
                                                      const Preferences&     preferences);

    /// Java's InnerTubeSaver.getElements().
    [[nodiscard]] static Elements innerTubeElements(const RocketComponent& component,
                                                    OrkWriter&             writer,
                                                    const Preferences&     preferences);

    /// Java's CenteringRingSaver.getElements().
    [[nodiscard]] static Elements centeringRingElements(const RocketComponent& component,
                                                        OrkWriter&             writer,
                                                        const Preferences&     preferences);

    /// Java's BulkheadSaver.getElements().
    [[nodiscard]] static Elements bulkheadElements(const RocketComponent& component,
                                                   OrkWriter&             writer,
                                                   const Preferences&     preferences);

    /// Java's MassComponentSaver.getElements(), with the mass as decided (see above).
    [[nodiscard]] static Elements massComponentElements(const RocketComponent& component,
                                                        OrkWriter&             writer,
                                                        const Preferences&     preferences);

    /// Java's ShockCordSaver.getElements().
    [[nodiscard]] static Elements shockCordElements(const RocketComponent& component,
                                                    OrkWriter&             writer,
                                                    const Preferences&     preferences);

    /// Java's ParachuteSaver.getElements().
    [[nodiscard]] static Elements parachuteElements(const RocketComponent& component,
                                                    OrkWriter&             writer,
                                                    const Preferences&     preferences);

    /// Java's StreamerSaver.getElements().
    [[nodiscard]] static Elements streamerElements(const RocketComponent& component,
                                                   OrkWriter&             writer,
                                                   const Preferences&     preferences);
};

}  // namespace QtRocket
