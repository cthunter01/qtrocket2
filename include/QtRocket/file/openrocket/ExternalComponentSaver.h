#pragma once

#include "QtRocket/file/openrocket/RocketComponentSaver.h"

namespace QtRocket
{

class OrkWriter;
class Preferences;
class RocketComponent;

/// The .ork savers of the external components (OpenRocket's file/openrocket/savers/
/// ExternalComponentSaver and its subclasses: BodyComponentSaver, SymmetricComponentSaver,
/// BodyTubeSaver, TransitionSaver, NoseConeSaver, FinSetSaver, TrapezoidFinSetSaver,
/// EllipticalFinSetSaver, FreeformFinSetSaver, TubeFinSetSaver, LaunchLugSaver,
/// RailButtonSaver). Each ...Elements() function gives the lines of a component's element as
/// Java's getElements() does (see RocketComponentSaver and ComponentSaver), the parameters of
/// the class chain in Java's order:
/// - every external component: RocketComponentSaver::addParams(), <finish>, <material>;
/// - a body component: <length>; a symmetric one: <thickness>, "filled" for a filled one;
/// - a transition: <shape>, <shapeclipped> when the shape can be clipped, <shapeparameter> when
///   it uses one; for a nose cone the base radius as <aftradius> ("auto " and the value when
///   automatic), its shoulder as the four <aftshoulder...> elements and <isflipped>; for any
///   other transition <foreradius> and <aftradius> ("auto " and the value when automatic) and
///   the eight shoulder elements;
/// - a body tube: <radius> ("auto " and the value when automatic), then the <motormount> of
///   RocketComponentSaver::motorMountParams() when it acts as a motor mount;
/// - a fin set: <thickness>, <crosssection>, <cant> in degrees (Math.toDegrees(),
///   MathUtil::javaToDegrees(): the one angle that is not written as angle * 180.0 / PI), the tab
///   only when its height and its length are both non-zero (MathUtil::equals(): <tabheight>,
///   <tablength>, <tabposition> with the legacy relativeto="front|center|end" and again with the
///   method's name), <filletradius>, <filletmaterial>; then a trapezoidal one's <rootchord>,
///   <tipchord>, <sweeplength>, <height>, an elliptical one's <rootchord> (its length) and
///   <height>, a freeform one's <finpoints> with a <point x y/> per point;
/// - a tube fin set: <fincount>, <rotation> in degrees (both again, after RocketComponentSaver's),
///   <radius> ("auto" alone when automatic), <length>, <thickness>;
/// - a launch lug: <radius>, <length>, <thickness>;
/// - a rail button: <outerdiameter>, <innerdiameter>, <height>, <baseheight>, <flangeheight>,
///   <screwheight>.
///
/// Deviations from OpenRocket: those of RocketComponentSaver; the functions take the preferences
/// (none of them reads them) so that all savers have the same signature (ComponentSaver).
class ExternalComponentSaver final
{
public:
    ExternalComponentSaver() = delete;

    using Elements = RocketComponentSaver::Elements;

    /// Java's ExternalComponentSaver.addParams(): RocketComponentSaver::addParams(), the finish
    /// and the material.
    static void addParams(const RocketComponent& component, OrkWriter& writer, Elements& elements);

    /// Java's BodyTubeSaver.getElements().
    [[nodiscard]] static Elements bodyTubeElements(const RocketComponent& component,
                                                   OrkWriter&             writer,
                                                   const Preferences&     preferences);

    /// Java's TransitionSaver.getElements().
    [[nodiscard]] static Elements transitionElements(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences);

    /// Java's NoseConeSaver.getElements().
    [[nodiscard]] static Elements noseConeElements(const RocketComponent& component,
                                                   OrkWriter&             writer,
                                                   const Preferences&     preferences);

    /// Java's TrapezoidFinSetSaver.getElements().
    [[nodiscard]] static Elements trapezoidFinSetElements(const RocketComponent& component,
                                                          OrkWriter&             writer,
                                                          const Preferences&     preferences);

    /// Java's EllipticalFinSetSaver.getElements().
    [[nodiscard]] static Elements ellipticalFinSetElements(const RocketComponent& component,
                                                           OrkWriter&             writer,
                                                           const Preferences&     preferences);

    /// Java's FreeformFinSetSaver.getElements().
    [[nodiscard]] static Elements freeformFinSetElements(const RocketComponent& component,
                                                         OrkWriter&             writer,
                                                         const Preferences&     preferences);

    /// Java's TubeFinSetSaver.getElements().
    [[nodiscard]] static Elements tubeFinSetElements(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences);

    /// Java's LaunchLugSaver.getElements().
    [[nodiscard]] static Elements launchLugElements(const RocketComponent& component,
                                                    OrkWriter&             writer,
                                                    const Preferences&     preferences);

    /// Java's RailButtonSaver.getElements().
    [[nodiscard]] static Elements railButtonElements(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences);
};

}  // namespace QtRocket
