// TubeFinSet's own tests. OpenRocket has no JUnit test class for TubeFinSet, so the values pinned
// here (the Pins below and the numbers in the tests) were computed with OpenRocket itself: a small
// Java program built the same components with the same calls, in the same order, and printed
// what OpenRocket's TubeFinSet answers (commit 5f164fd0e on JDK 17). Values that went through
// sin() or cos() are compared with a relative tolerance of 1e-12, so the pins hold with every
// math library.
//
// Ported from JUnit: MassCalculatorTest.testTubeFinMass (suite TubeFinSetMass). Java's
// BodyTube.addChild(), which gives a tube fin set without a thickness the body tube's, is
// tested here as well (suite TubeFinSetBodyTubeHook), and so is which parents accept a tube fin
// set, a launch lug and a rail button (suite AttachmentCompatibility).

#include "QtRocket/rocket/TubeFinSet.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/JavaValueDifferences.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::TubeFinSet;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::matchesJavaValue;

/// The collector of the differences from Java's values.
using Differences = QtRocket::Test::JavaValueDifferences;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// The event types of the setters: AEROMASS_CHANGE and NONFUNCTIONAL_CHANGE; and TREE_CHANGE,
/// which adding and removing children fire.
constexpr int kBoth          = ComponentChangeEvent::kBothChange;
constexpr int kNonFunctional = ComponentChangeEvent::kNonFunctionalChange;
constexpr int kTree          = ComponentChangeEvent::kTreeChange;

using Events = std::vector<int>;

/// What OpenRocket's TubeFinSet answers in one state.
struct Pins
{
    int                     finCount{};
    bool                    autoRadius{};
    double                  outerRadius{};
    double                  innerRadius{};
    double                  thickness{};
    double                  bodyRadius{};
    double                  tubeSeparation{};
    double                  boundingRadius{};
    double                  finRotation{};
    double                  angleOffset{};
    double                  length{};
    double                  axialOffset{};
    Coordinate              position;
    double                  componentVolume{};
    double                  componentMass{};
    Coordinate              componentCG;
    double                  longitudinalUnitInertia{};
    double                  rotationalUnitInertia{};
    std::vector<Coordinate> componentBounds;
    Coordinate              boxMin;
    Coordinate              boxMax;
    std::vector<double>     instanceAngles;
    std::vector<Coordinate> instanceOffsets;
    std::vector<Coordinate> componentLocations;
};

/// The differences between @p fins and Java's @p expected, empty when there are none.
[[nodiscard]] std::string differences(const Pins& expected, const TubeFinSet& fins)
{
    Differences d;
    d.integer("finCount", expected.finCount, fins.getFinCount());
    d.integer("instanceCount", expected.finCount, fins.getInstanceCount());
    d.integer("autoRadius", static_cast<int>(expected.autoRadius),
              static_cast<int>(fins.isOuterRadiusAutomatic()));
    d.number("outerRadius", expected.outerRadius, fins.getOuterRadius());
    d.number("innerRadius", expected.innerRadius, fins.getInnerRadius());
    d.number("thickness", expected.thickness, fins.getThickness());
    d.number("bodyRadius", expected.bodyRadius, fins.getBodyRadius());
    d.number("tubeSeparation", expected.tubeSeparation, fins.getTubeSeparation());
    d.number("boundingRadius", expected.boundingRadius, fins.getBoundingRadius());
    d.number("finRotation", expected.finRotation, fins.getFinRotation());
    d.number("instanceAngleIncrement", expected.finRotation, fins.getInstanceAngleIncrement());
    d.number("angleOffset", expected.angleOffset, fins.getAngleOffset());
    d.number("baseRotation", expected.angleOffset, fins.getBaseRotation());
    d.number("length", expected.length, fins.getLength());
    d.number("axialOffset", expected.axialOffset, fins.getAxialOffset());
    d.coordinate("position", expected.position, fins.getPosition());
    d.number("componentVolume", expected.componentVolume, fins.getComponentVolume());
    d.number("componentMass", expected.componentMass, fins.getComponentMass());
    d.coordinate("componentCG", expected.componentCG, fins.getComponentCG());
    d.number("longitudinalUnitInertia", expected.longitudinalUnitInertia,
             fins.getLongitudinalUnitInertia());
    d.number("rotationalUnitInertia", expected.rotationalUnitInertia,
             fins.getRotationalUnitInertia());
    d.coordinates("componentBounds", expected.componentBounds, fins.getComponentBounds());
    const BoundingBox box = fins.getInstanceBoundingBox();
    d.coordinate("instanceBoundingBox.min", expected.boxMin, box.min());
    d.coordinate("instanceBoundingBox.max", expected.boxMax, box.max());
    d.numbers("instanceAngles", expected.instanceAngles, fins.getInstanceAngles());
    d.coordinates("instanceOffsets", expected.instanceOffsets, fins.getInstanceOffsets());
    d.coordinates("componentLocations", expected.componentLocations, fins.getComponentLocations());
    return d.text();
}

// ---- Java's values, one function per state (printed by the Java program)

[[nodiscard]] Pins pinsDetached()
{
    return Pins{
        .finCount                = 6,
        .autoRadius              = true,
        .outerRadius             = 0.0,
        .innerRadius             = kNaN,
        .thickness               = kNaN,
        .bodyRadius              = 0.0,
        .tubeSeparation          = 0.0,
        .boundingRadius          = 0.0,
        .finRotation             = 1.0471975511965976,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume         = kNaN,
        .componentMass           = kNaN,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, kNaN},
        .longitudinalUnitInertia = kNaN,
        .rotationalUnitInertia   = kNaN,
        .componentBounds    = {Coordinate{0.0, -0.0, -0.0, 0.0}, Coordinate{0.0, 0.0, -0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, -0.0, 0.0, 0.0},
                               Coordinate{0.1, -0.0, -0.0, 0.0}, Coordinate{0.1, 0.0, -0.0, 0.0},
                               Coordinate{0.1, 0.0, 0.0, 0.0}, Coordinate{0.1, -0.0, 0.0, 0.0}},
        .boxMin             = Coordinate{0.0, -0.0, -0.0, 0.0},
        .boxMax             = Coordinate{0.1, -0.0, -0.0, 0.0},
        .instanceAngles     = {0.0, 1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                               4.1887902047863905, 5.235987755982988},
        .instanceOffsets    = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsOnBody6()
{
    return Pins{
        .finCount                = 6,
        .autoRadius              = true,
        .outerRadius             = 0.024999999999999998,
        .innerRadius             = 0.023,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.0,
        .boundingRadius          = 0.05,
        .finRotation             = 1.0471975511965976,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 1.80955736846772E-4,
        .componentMass           = 0.12304990105580496,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.12304990105580496},
        .longitudinalUnitInertia = 0.006731,
        .rotationalUnitInertia   = 0.15721200000000002,
        .componentBounds    = {Coordinate{0.0, -0.1, -0.1, 0.0}, Coordinate{0.0, 0.1, -0.1, 0.0},
                               Coordinate{0.0, 0.1, 0.1, 0.0}, Coordinate{0.0, -0.1, 0.1, 0.0},
                               Coordinate{0.1, -0.1, -0.1, 0.0}, Coordinate{0.1, 0.1, -0.1, 0.0},
                               Coordinate{0.1, 0.1, 0.1, 0.0}, Coordinate{0.1, -0.1, 0.1, 0.0}},
        .boxMin             = Coordinate{0.0, -0.024999999999999998, -0.024999999999999998, 0.0},
        .boxMax             = Coordinate{0.1, 0.024999999999999998, 0.024999999999999998, 0.0},
        .instanceAngles     = {0.0, 1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                               4.1887902047863905, 5.235987755982988},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0},
                               Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
                               Coordinate{0.0, -0.012499999999999995, 0.02165063509461097, 0.0},
                               Coordinate{0.0, -0.025, 3.061616997868383E-18, 0.0},
                               Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
                               Coordinate{0.0, 0.012499999999999983, -0.021650635094610977, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.025, 0.0, 0.0},
            Coordinate{0.19999999999999998, 0.012500000000000004, 0.021650635094610966, 0.0},
            Coordinate{0.19999999999999998, -0.012499999999999995, 0.02165063509461097, 0.0},
            Coordinate{0.19999999999999998, -0.025, 3.061616997868383E-18, 0.0},
            Coordinate{0.19999999999999998, -0.012500000000000011, -0.02165063509461096, 0.0},
            Coordinate{0.19999999999999998, 0.012499999999999983, -0.021650635094610977, 0.0}}};
}

[[nodiscard]] Pins pinsFins3()
{
    return Pins{
        .finCount                = 3,
        .autoRadius              = true,
        .outerRadius             = 0.1616025403784438,
        .innerRadius             = 0.1596025403784438,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.0,
        .boundingRadius          = 0.1866025403784438,
        .finRotation             = 2.0943951023931953,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 6.054573132009349E-4,
        .componentMass           = 0.4117109729766357,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.4117109729766357},
        .longitudinalUnitInertia = 0.041191263964014506,
        .rotationalUnitInertia   = 0.23072867109832865,
        .componentBounds         = {Coordinate{0.0, -0.3732050807568876, -0.3732050807568876, 0.0},
                                    Coordinate{0.0, 0.3732050807568876, -0.3732050807568876, 0.0},
                                    Coordinate{0.0, 0.3732050807568876, 0.3732050807568876, 0.0},
                                    Coordinate{0.0, -0.3732050807568876, 0.3732050807568876, 0.0},
                                    Coordinate{0.1, -0.3732050807568876, -0.3732050807568876, 0.0},
                                    Coordinate{0.1, 0.3732050807568876, -0.3732050807568876, 0.0},
                                    Coordinate{0.1, 0.3732050807568876, 0.3732050807568876, 0.0},
                                    Coordinate{0.1, -0.3732050807568876, 0.3732050807568876, 0.0}},
        .boxMin                  = Coordinate{0.0, -0.1616025403784438, -0.1616025403784438, 0.0},
        .boxMax                  = Coordinate{0.1, 0.1616025403784438, 0.1616025403784438, 0.0},
        .instanceAngles          = {0.0, 2.0943951023931953, 4.1887902047863905},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0},
                               Coordinate{0.0, -0.012499999999999995, 0.02165063509461097, 0.0},
                               Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.025, 0.0, 0.0},
            Coordinate{0.19999999999999998, -0.012499999999999995, 0.02165063509461097, 0.0},
            Coordinate{0.19999999999999998, -0.012500000000000011, -0.02165063509461096, 0.0}}};
}

[[nodiscard]] Pins pinsFins1()
{
    return Pins{
        .finCount                = 1,
        .autoRadius              = true,
        .outerRadius             = 0.025,
        .innerRadius             = 0.023,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = -0.049999999999999996,
        .boundingRadius          = 0.05,
        .finRotation             = 6.283185307179586,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 3.0159289474462067E-5,
        .componentMass           = 0.020508316842634204,
        .componentCG             = Coordinate{0.05, 0.05, 0.0, 0.020508316842634204},
        .longitudinalUnitInertia = 0.0011218333333333334,
        .rotationalUnitInertia   = 5.77E-4,
        .componentBounds    = {Coordinate{0.0, -0.1, -0.1, 0.0}, Coordinate{0.0, 0.1, -0.1, 0.0},
                               Coordinate{0.0, 0.1, 0.1, 0.0}, Coordinate{0.0, -0.1, 0.1, 0.0},
                               Coordinate{0.1, -0.1, -0.1, 0.0}, Coordinate{0.1, 0.1, -0.1, 0.0},
                               Coordinate{0.1, 0.1, 0.1, 0.0}, Coordinate{0.1, -0.1, 0.1, 0.0}},
        .boxMin             = Coordinate{0.0, -0.025, -0.025, 0.0},
        .boxMax             = Coordinate{0.1, 0.025, 0.025, 0.0},
        .instanceAngles     = {0.0},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.19999999999999998, 0.025, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsFins2()
{
    return Pins{
        .finCount                = 2,
        .autoRadius              = true,
        .outerRadius             = 0.025,
        .innerRadius             = 0.023,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = kInf,
        .boundingRadius          = 0.05,
        .finRotation             = std::numbers::pi,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 6.0318578948924135E-5,
        .componentMass           = 0.04101663368526841,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.04101663368526841},
        .longitudinalUnitInertia = 0.002243666666666667,
        .rotationalUnitInertia   = 0.052404000000000006,
        .componentBounds    = {Coordinate{0.0, -0.1, -0.1, 0.0}, Coordinate{0.0, 0.1, -0.1, 0.0},
                               Coordinate{0.0, 0.1, 0.1, 0.0}, Coordinate{0.0, -0.1, 0.1, 0.0},
                               Coordinate{0.1, -0.1, -0.1, 0.0}, Coordinate{0.1, 0.1, -0.1, 0.0},
                               Coordinate{0.1, 0.1, 0.1, 0.0}, Coordinate{0.1, -0.1, 0.1, 0.0}},
        .boxMin             = Coordinate{0.0, -0.025, -0.025, 0.0},
        .boxMax             = Coordinate{0.1, 0.025, 0.025, 0.0},
        .instanceAngles     = {0.0, std::numbers::pi},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0},
                               Coordinate{0.0, -0.025, 3.061616997868383E-18, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.025, 0.0, 0.0},
            Coordinate{0.19999999999999998, -0.025, 3.061616997868383E-18, 0.0}}};
}

[[nodiscard]] Pins pinsFins8()
{
    return Pins{
        .finCount                = 8,
        .autoRadius              = true,
        .outerRadius             = 0.01549786011054438,
        .innerRadius             = 0.01349786011054438,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.0,
        .boundingRadius          = 0.04049786011054438,
        .finRotation             = 0.7853981633974483,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 1.4574838661138794E-4,
        .componentMass           = 0.0991089028957438,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.0991089028957438},
        .longitudinalUnitInertia = 0.007511418457806325,
        .rotationalUnitInertia   = 0.20361097292632735,
        .componentBounds    = {Coordinate{0.0, -0.08099572022108877, -0.08099572022108877, 0.0},
                               Coordinate{0.0, 0.08099572022108877, -0.08099572022108877, 0.0},
                               Coordinate{0.0, 0.08099572022108877, 0.08099572022108877, 0.0},
                               Coordinate{0.0, -0.08099572022108877, 0.08099572022108877, 0.0},
                               Coordinate{0.1, -0.08099572022108877, -0.08099572022108877, 0.0},
                               Coordinate{0.1, 0.08099572022108877, -0.08099572022108877, 0.0},
                               Coordinate{0.1, 0.08099572022108877, 0.08099572022108877, 0.0},
                               Coordinate{0.1, -0.08099572022108877, 0.08099572022108877, 0.0}},
        .boxMin             = Coordinate{0.0, -0.01549786011054438, -0.01549786011054438, 0.0},
        .boxMax             = Coordinate{0.1, 0.01549786011054438, 0.01549786011054438, 0.0},
        .instanceAngles     = {0.0, 0.7853981633974483, 1.5707963267948966, 2.356194490192345,
                               std::numbers::pi, 3.9269908169872414, 4.71238898038469,
                               5.497787143782138},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0},
                               Coordinate{0.0, 0.01767766952966369, 0.017677669529663688, 0.0},
                               Coordinate{0.0, 1.5308084989341916E-18, 0.025, 0.0},
                               Coordinate{0.0, -0.017677669529663688, 0.01767766952966369, 0.0},
                               Coordinate{0.0, -0.025, 3.061616997868383E-18, 0.0},
                               Coordinate{0.0, -0.01767766952966369, -0.017677669529663688, 0.0},
                               Coordinate{0.0, -4.592425496802574E-18, -0.025, 0.0},
                               Coordinate{0.0, 0.017677669529663684, -0.01767766952966369, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.025, 0.0, 0.0},
            Coordinate{0.19999999999999998, 0.01767766952966369, 0.017677669529663688, 0.0},
            Coordinate{0.19999999999999998, 1.5308084989341916E-18, 0.025, 0.0},
            Coordinate{0.19999999999999998, -0.017677669529663688, 0.01767766952966369, 0.0},
            Coordinate{0.19999999999999998, -0.025, 3.061616997868383E-18, 0.0},
            Coordinate{0.19999999999999998, -0.01767766952966369, -0.017677669529663688, 0.0},
            Coordinate{0.19999999999999998, -4.592425496802574E-18, -0.025, 0.0},
            Coordinate{0.19999999999999998, 0.017677669529663684, -0.01767766952966369, 0.0}}};
}

[[nodiscard]] Pins pinsFins5()
{
    return Pins{
        .finCount                = 5,
        .autoRadius              = true,
        .outerRadius             = 0.03564799995398978,
        .innerRadius             = 0.03364799995398978,
        .thickness               = 0.002,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.0,
        .boundingRadius          = 0.060647999953989784,
        .finRotation             = 1.2566370614359172,
        .angleOffset             = 0.0,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 2.1769980423406798E-4,
        .componentMass           = 0.14803586687916623,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.14803586687916623},
        .longitudinalUnitInertia = 0.007170376418695858,
        .rotationalUnitInertia   = 0.13736131900765666,
        .componentBounds    = {Coordinate{0.0, -0.12129599990797957, -0.12129599990797957, 0.0},
                               Coordinate{0.0, 0.12129599990797957, -0.12129599990797957, 0.0},
                               Coordinate{0.0, 0.12129599990797957, 0.12129599990797957, 0.0},
                               Coordinate{0.0, -0.12129599990797957, 0.12129599990797957, 0.0},
                               Coordinate{0.1, -0.12129599990797957, -0.12129599990797957, 0.0},
                               Coordinate{0.1, 0.12129599990797957, -0.12129599990797957, 0.0},
                               Coordinate{0.1, 0.12129599990797957, 0.12129599990797957, 0.0},
                               Coordinate{0.1, -0.12129599990797957, 0.12129599990797957, 0.0}},
        .boxMin             = Coordinate{0.0, -0.03564799995398978, -0.03564799995398978, 0.0},
        .boxMax             = Coordinate{0.1, 0.03564799995398978, 0.03564799995398978, 0.0},
        .instanceAngles     = {0.0, 1.2566370614359172, 2.5132741228718345, 3.7699111843077517,
                               5.026548245743669},
        .instanceOffsets    = {Coordinate{0.0, 0.025, 0.0, 0.0},
                               Coordinate{0.0, 0.007725424859373687, 0.02377641290737884, 0.0},
                               Coordinate{0.0, -0.020225424859373686, 0.014694631307311832, 0.0},
                               Coordinate{0.0, -0.02022542485937369, -0.014694631307311827, 0.0},
                               Coordinate{0.0, 0.007725424859373681, -0.023776412907378842, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.025, 0.0, 0.0},
            Coordinate{0.19999999999999998, 0.007725424859373687, 0.02377641290737884, 0.0},
            Coordinate{0.19999999999999998, -0.020225424859373686, 0.014694631307311832, 0.0},
            Coordinate{0.19999999999999998, -0.02022542485937369, -0.014694631307311827, 0.0},
            Coordinate{0.19999999999999998, 0.007725424859373681, -0.023776412907378842, 0.0}}};
}

[[nodiscard]] Pins pinsManual()
{
    return Pins{
        .finCount                = 4,
        .autoRadius              = false,
        .outerRadius             = 0.01,
        .innerRadius             = 0.009000000000000001,
        .thickness               = 0.001,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.10071067811865472,
        .boundingRadius          = 0.035,
        .finRotation             = 1.5707963267948966,
        .angleOffset             = 1.0471975511965976,
        .length                  = 0.08,
        .axialOffset             = -0.02,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 1.910088333382593E-5,
        .componentMass           = 0.012988600667001632,
        .componentCG             = Coordinate{0.04, 0.0, 0.0, 0.012988600667001632},
        .longitudinalUnitInertia = 0.003914333333333334,
        .rotationalUnitInertia   = 0.100762,
        .componentBounds = {Coordinate{0.0, -0.07, -0.07, 0.0}, Coordinate{0.0, 0.07, -0.07, 0.0},
                            Coordinate{0.0, 0.07, 0.07, 0.0}, Coordinate{0.0, -0.07, 0.07, 0.0},
                            Coordinate{0.08, -0.07, -0.07, 0.0}, Coordinate{0.08, 0.07, -0.07, 0.0},
                            Coordinate{0.08, 0.07, 0.07, 0.0}, Coordinate{0.08, -0.07, 0.07, 0.0}},
        .boxMin          = Coordinate{0.0, -0.01, -0.01, 0.0},
        .boxMax          = Coordinate{0.08, 0.01, 0.01, 0.0},
        .instanceAngles  = {1.0471975511965976, 2.617993877991494, 4.1887902047863905,
                            5.759586531581287},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
                            Coordinate{0.0, -0.021650635094610963, 0.01250000000000001, 0.0},
                            Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
                            Coordinate{0.0, 0.02165063509461096, -0.012500000000000011, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.012500000000000004, 0.021650635094610966, 0.0},
            Coordinate{0.19999999999999998, -0.021650635094610963, 0.01250000000000001, 0.0},
            Coordinate{0.19999999999999998, -0.012500000000000011, -0.02165063509461096, 0.0},
            Coordinate{0.19999999999999998, 0.02165063509461096, -0.012500000000000011, 0.0}}};
}

[[nodiscard]] Pins pinsTop()
{
    return Pins{
        .finCount                = 4,
        .autoRadius              = false,
        .outerRadius             = 0.01,
        .innerRadius             = 0.009000000000000001,
        .thickness               = 0.001,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.10071067811865472,
        .boundingRadius          = 0.035,
        .finRotation             = 1.5707963267948966,
        .angleOffset             = 1.0471975511965976,
        .length                  = 0.08,
        .axialOffset             = 0.19999999999999998,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 1.910088333382593E-5,
        .componentMass           = 0.012988600667001632,
        .componentCG             = Coordinate{0.04, 0.0, 0.0, 0.012988600667001632},
        .longitudinalUnitInertia = 0.1623143333333333,
        .rotationalUnitInertia   = 0.100762,
        .componentBounds = {Coordinate{0.0, -0.07, -0.07, 0.0}, Coordinate{0.0, 0.07, -0.07, 0.0},
                            Coordinate{0.0, 0.07, 0.07, 0.0}, Coordinate{0.0, -0.07, 0.07, 0.0},
                            Coordinate{0.08, -0.07, -0.07, 0.0}, Coordinate{0.08, 0.07, -0.07, 0.0},
                            Coordinate{0.08, 0.07, 0.07, 0.0}, Coordinate{0.08, -0.07, 0.07, 0.0}},
        .boxMin          = Coordinate{0.0, -0.01, -0.01, 0.0},
        .boxMax          = Coordinate{0.08, 0.01, 0.01, 0.0},
        .instanceAngles  = {1.0471975511965976, 2.617993877991494, 4.1887902047863905,
                            5.759586531581287},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
                            Coordinate{0.0, -0.021650635094610963, 0.01250000000000001, 0.0},
                            Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
                            Coordinate{0.0, 0.02165063509461096, -0.012500000000000011, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.012500000000000004, 0.021650635094610966, 0.0},
            Coordinate{0.19999999999999998, -0.021650635094610963, 0.01250000000000001, 0.0},
            Coordinate{0.19999999999999998, -0.012500000000000011, -0.02165063509461096, 0.0},
            Coordinate{0.19999999999999998, 0.02165063509461096, -0.012500000000000011, 0.0}}};
}

[[nodiscard]] Pins pinsSingle()
{
    return Pins{
        .finCount        = 1,
        .autoRadius      = false,
        .outerRadius     = 0.01,
        .innerRadius     = 0.009000000000000001,
        .thickness       = 0.001,
        .bodyRadius      = 0.025,
        .tubeSeparation  = -0.019999999999999993,
        .boundingRadius  = 0.035,
        .finRotation     = 6.283185307179586,
        .angleOffset     = 1.0471975511965976,
        .length          = 0.08,
        .axialOffset     = 0.09,
        .position        = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume = 4.775220833456482E-6,
        .componentMass   = 0.003247150166750408,
        .componentCG =
            Coordinate{0.04, 0.017500000000000005, 0.030310889132455353, 0.003247150166750408},
        .longitudinalUnitInertia = 5.785833333333334E-4,
        .rotationalUnitInertia   = 9.05E-5,
        .componentBounds = {Coordinate{0.0, -0.07, -0.07, 0.0}, Coordinate{0.0, 0.07, -0.07, 0.0},
                            Coordinate{0.0, 0.07, 0.07, 0.0}, Coordinate{0.0, -0.07, 0.07, 0.0},
                            Coordinate{0.08, -0.07, -0.07, 0.0}, Coordinate{0.08, 0.07, -0.07, 0.0},
                            Coordinate{0.08, 0.07, 0.07, 0.0}, Coordinate{0.08, -0.07, 0.07, 0.0}},
        .boxMin          = Coordinate{0.0, -0.01, -0.01, 0.0},
        .boxMax          = Coordinate{0.08, 0.01, 0.01, 0.0},
        .instanceAngles  = {1.0471975511965976},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.012500000000000004, 0.021650635094610966, 0.0}}};
}

[[nodiscard]] Pins pinsCopy()
{
    return Pins{
        .finCount                = 3,
        .autoRadius              = false,
        .outerRadius             = 0.02,
        .innerRadius             = 0.019,
        .thickness               = 0.001,
        .bodyRadius              = 0.0,
        .tubeSeparation          = -0.04,
        .boundingRadius          = 0.02,
        .finRotation             = 2.0943951023931953,
        .angleOffset             = 0.5,
        .length                  = 0.1,
        .axialOffset             = 0.0,
        .position                = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume         = 3.675663404700061E-5,
        .componentMass           = 0.03675663404700061,
        .componentCG             = Coordinate{0.05, 0.0, 0.0, 0.03675663404700061},
        .longitudinalUnitInertia = 0.0030707500000000006,
        .rotationalUnitInertia   = 0.0023415,
        .componentBounds = {Coordinate{0.0, -0.04, -0.04, 0.0}, Coordinate{0.0, 0.04, -0.04, 0.0},
                            Coordinate{0.0, 0.04, 0.04, 0.0}, Coordinate{0.0, -0.04, 0.04, 0.0},
                            Coordinate{0.1, -0.04, -0.04, 0.0}, Coordinate{0.1, 0.04, -0.04, 0.0},
                            Coordinate{0.1, 0.04, 0.04, 0.0}, Coordinate{0.1, -0.04, 0.04, 0.0}},
        .boxMin          = Coordinate{0.0, -0.02, -0.02, 0.0},
        .boxMax          = Coordinate{0.1, 0.02, 0.02, 0.0},
        .instanceAngles  = {0.5, 2.5943951023931953, 4.6887902047863905},
        .instanceOffsets = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                            Coordinate{0.0, 0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.0, 0.0, 0.0, 0.0},
                               Coordinate{0.0, 0.0, 0.0, 0.0}}};
}

/// The "manual" tubes with the axial method AFTER: where they were, the offset measured from the
/// end of the body (and the longitudinal unit inertia following the stored offset).
[[nodiscard]] Pins pinsAfter()
{
    return Pins{
        .finCount                = 4,
        .autoRadius              = false,
        .outerRadius             = 0.01,
        .innerRadius             = 0.009000000000000001,
        .thickness               = 0.001,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.10071067811865472,
        .boundingRadius          = 0.035,
        .finRotation             = 1.5707963267948966,
        .angleOffset             = 1.0471975511965976,
        .length                  = 0.08,
        .axialOffset             = -0.1,
        .position                = Coordinate{0.19999999999999998, 0.0, 0.0, 0.0},
        .componentVolume         = 1.910088333382593E-5,
        .componentMass           = 0.012988600667001632,
        .componentCG             = Coordinate{0.04, 0.0, 0.0, 0.012988600667001632},
        .longitudinalUnitInertia = 0.04231433333333334,
        .rotationalUnitInertia   = 0.100762,
        .componentBounds = {Coordinate{0.0, -0.07, -0.07, 0.0}, Coordinate{0.0, 0.07, -0.07, 0.0},
                            Coordinate{0.0, 0.07, 0.07, 0.0}, Coordinate{0.0, -0.07, 0.07, 0.0},
                            Coordinate{0.08, -0.07, -0.07, 0.0}, Coordinate{0.08, 0.07, -0.07, 0.0},
                            Coordinate{0.08, 0.07, 0.07, 0.0}, Coordinate{0.08, -0.07, 0.07, 0.0}},
        .boxMin          = Coordinate{0.0, -0.01, -0.01, 0.0},
        .boxMax          = Coordinate{0.08, 0.01, 0.01, 0.0},
        .instanceAngles  = {1.0471975511965976, 2.617993877991494, 4.1887902047863905,
                            5.759586531581287},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
                            Coordinate{0.0, -0.021650635094610963, 0.01250000000000001, 0.0},
                            Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
                            Coordinate{0.0, 0.02165063509461096, -0.012500000000000011, 0.0}},
        .componentLocations = {
            Coordinate{0.19999999999999998, 0.012500000000000004, 0.021650635094610966, 0.0},
            Coordinate{0.19999999999999998, -0.021650635094610963, 0.01250000000000001, 0.0},
            Coordinate{0.19999999999999998, -0.012500000000000011, -0.02165063509461096, 0.0},
            Coordinate{0.19999999999999998, 0.02165063509461096, -0.012500000000000011, 0.0}}};
}

/// pinsAfter() with the offset -0.15: the tubes start 150 mm ahead of the end of the body.
[[nodiscard]] Pins pinsAfterOffset()
{
    return Pins{
        .finCount                = 4,
        .autoRadius              = false,
        .outerRadius             = 0.01,
        .innerRadius             = 0.009000000000000001,
        .thickness               = 0.001,
        .bodyRadius              = 0.025,
        .tubeSeparation          = 0.10071067811865472,
        .boundingRadius          = 0.035,
        .finRotation             = 1.5707963267948966,
        .angleOffset             = 1.0471975511965976,
        .length                  = 0.08,
        .axialOffset             = -0.15,
        .position                = Coordinate{0.15, 0.0, 0.0, 0.0},
        .componentVolume         = 1.910088333382593E-5,
        .componentMass           = 0.012988600667001632,
        .componentCG             = Coordinate{0.04, 0.0, 0.0, 0.012988600667001632},
        .longitudinalUnitInertia = 0.09231433333333333,
        .rotationalUnitInertia   = 0.100762,
        .componentBounds = {Coordinate{0.0, -0.07, -0.07, 0.0}, Coordinate{0.0, 0.07, -0.07, 0.0},
                            Coordinate{0.0, 0.07, 0.07, 0.0}, Coordinate{0.0, -0.07, 0.07, 0.0},
                            Coordinate{0.08, -0.07, -0.07, 0.0}, Coordinate{0.08, 0.07, -0.07, 0.0},
                            Coordinate{0.08, 0.07, 0.07, 0.0}, Coordinate{0.08, -0.07, 0.07, 0.0}},
        .boxMin          = Coordinate{0.0, -0.01, -0.01, 0.0},
        .boxMax          = Coordinate{0.08, 0.01, 0.01, 0.0},
        .instanceAngles  = {1.0471975511965976, 2.617993877991494, 4.1887902047863905,
                            5.759586531581287},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
                            Coordinate{0.0, -0.021650635094610963, 0.01250000000000001, 0.0},
                            Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
                            Coordinate{0.0, 0.02165063509461096, -0.012500000000000011, 0.0}},
        .componentLocations = {Coordinate{0.15, 0.012500000000000004, 0.021650635094610966, 0.0},
                               Coordinate{0.15, -0.021650635094610963, 0.01250000000000001, 0.0},
                               Coordinate{0.15, -0.012500000000000011, -0.02165063509461096, 0.0},
                               Coordinate{0.15, 0.02165063509461096, -0.012500000000000011, 0.0}}};
}

// ================================================================================ detached

TEST(TubeFinSet, Defaults)
{
    const TubeFinSet fins;
    EXPECT_EQ(fins.kind(), ComponentKind::TUBE_FIN_SET);
    EXPECT_EQ(fins.getName(), "Tube Fin Set");
    EXPECT_EQ(fins.getComponentName(), "Tube Fin Set");
    EXPECT_EQ(fins.getMaterial(), QtRocket::ExternalComponent::defaultMaterial());
    EXPECT_EQ(fins.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(fins.getMaterial().getDensity(), 680.0);
    EXPECT_EQ(fins.getPresetType(), ComponentPresetType::BODY_TUBE);
    EXPECT_FALSE(fins.isAfter());
    EXPECT_TRUE(fins.isAerodynamic());
    EXPECT_TRUE(fins.isMassive());
    EXPECT_EQ(fins.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(fins.getAngleMethod(), AngleMethod::FIXED);
    EXPECT_EQ(fins.getRadiusMethod(), RadiusMethod::COAXIAL);
    EXPECT_EQ(fins.getRadiusOffset(), 0.0);
    EXPECT_EQ(fins.getRadiusOffset(RadiusMethod::FREE), 0.0);
    EXPECT_EQ(fins.getPatternName(), "6-tubefin-ring");
    EXPECT_EQ(fins.getDisplayOrderSide(), 3);
    EXPECT_EQ(fins.getDisplayOrderBack(), 3);
    EXPECT_TRUE(fins.getBaseRotationTransformation().isIdentity());
    EXPECT_EQ(fins.getFinRotationTransformation(),
              Transformation::rotateX(2 * std::numbers::pi / 6));
}

TEST(TubeFinSet, IsATubeWithTheInterfacesOfJava)
{
    TubeFinSet fins;
    EXPECT_NE(dynamic_cast<QtRocket::Tube*>(&fins), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::Coaxial*>(&fins), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::AxialPositionable*>(&fins), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::BoxBounded*>(&fins), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::RingInstanceable*>(&fins), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::InsideColorComponent*>(&fins), nullptr);

    // Through the interfaces the overrides answer, not RocketComponent's defaults.
    const QtRocket::RingInstanceable& ring = fins;
    EXPECT_EQ(ring.getInstanceCount(), 6);
    EXPECT_EQ(ring.getAngleMethod(), AngleMethod::FIXED);
    const RocketComponent& component = fins;
    EXPECT_EQ(component.getInstanceCount(), 6);
    EXPECT_EQ(component.getInstanceAngles().size(), 6U);
    EXPECT_EQ(component.getInstanceOffsets().size(), 6U);
    EXPECT_EQ(component.getInstanceLocations().size(), 6U);
}

TEST(TubeFinSet, DetachedValuesAreNaNUntilTheThicknessIsSet)
{
    // Without a body the automatic radius is 0, and without a thickness the mass is NaN.
    TubeFinSet fins;
    EXPECT_EQ(differences(pinsDetached(), fins), "");

    fins.setOuterRadiusAutomatic(false);
    EXPECT_EQ(fins.getOuterRadius(), 0.025) << "the stored default radius";
    EXPECT_TRUE(std::isnan(fins.getThickness()));
    EXPECT_TRUE(std::isnan(fins.getInnerRadius()));
    EXPECT_EQ(fins.getTubeSeparation(), -0.05);
}

TEST(TubeFinSet, AcceptsNoChildren)
{
    TubeFinSet fins;
    EXPECT_FALSE(fins.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(fins.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(fins.addChild(std::make_unique<QtRocket::MassComponent>()), BugError);
}

/// Whether a new @p Parent accepts a child of @p kind.
template <class Parent>
[[nodiscard]] bool accepts(ComponentKind kind)
{
    const Parent parent;
    return parent.isCompatible(kind);
}

/// Whether a new @p Parent accepts any of a tube fin set, a launch lug and a rail button.
template <class Parent>
[[nodiscard]] bool acceptsAnAttachment()
{
    return accepts<Parent>(ComponentKind::TUBE_FIN_SET) ||
           accepts<Parent>(ComponentKind::LAUNCH_LUG) ||
           accepts<Parent>(ComponentKind::RAIL_BUTTON);
}

TEST(AttachmentCompatibility, OnlyABodyTubeAcceptsTubeFinsLugsAndButtons)
{
    // Java: isCompatible(TubeFinSet.class), (LaunchLug.class) and (RailButton.class) of a new
    // instance of every component class.
    EXPECT_TRUE(accepts<BodyTube>(ComponentKind::TUBE_FIN_SET));
    EXPECT_TRUE(accepts<BodyTube>(ComponentKind::LAUNCH_LUG));
    EXPECT_TRUE(accepts<BodyTube>(ComponentKind::RAIL_BUTTON));

    EXPECT_FALSE(acceptsAnAttachment<Rocket>());
    EXPECT_FALSE(acceptsAnAttachment<AxialStage>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::ParallelStage>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::PodSet>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::Transition>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::NoseCone>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::InnerTube>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::TubeCoupler>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::EngineBlock>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::CenteringRing>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::Bulkhead>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::MassComponent>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::ShockCord>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::Parachute>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::Streamer>());
    EXPECT_FALSE(acceptsAnAttachment<TubeFinSet>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::LaunchLug>());
    EXPECT_FALSE(acceptsAnAttachment<QtRocket::RailButton>());
}

// ============================================================================ on a body tube

/// A rocket with one stage holding a body tube (0.3 m long, radius 25 mm, wall 2 mm) with a new
/// tube fin set on it; the events are enabled and recorded.
class TubeFinSetOnBody : public ::testing::Test
{
protected:
    TubeFinSetOnBody()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body  = &m_stage->addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.002));
        m_fins  = &m_body->addChild(std::make_unique<TubeFinSet>());
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    /// The types of the events fired since the last call.
    [[nodiscard]] Events takeEvents() { return std::exchange(m_types, {}); }

    /// The state the pins "manual" describe: four tubes of radius 10 mm with a 1 mm wall, 80 mm
    /// long, turned by pi / 3 and moved 20 mm forward of the body's end.
    void makeManual()
    {
        m_fins->setFinCount(4);
        m_fins->setOuterRadius(0.01);
        m_fins->setThickness(0.001);
        m_fins->setBaseRotation(std::numbers::pi / 3);
        m_fins->setAxialOffset(-0.02);
        m_fins->setLength(0.08);
        m_types.clear();
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    BodyTube*                               m_body{nullptr};
    TubeFinSet*                             m_fins{nullptr};
    Events                                  m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST_F(TubeFinSetOnBody, SixAutomaticTubesTouchAroundTheBody)
{
    EXPECT_EQ(differences(pinsOnBody6(), *m_fins), "");
    // The tubes are as wide as the body, since sin(pi / 6) is 1/2.
    EXPECT_NEAR(m_fins->getOuterRadius(), m_body->getOuterRadius(), 1e-15);
    EXPECT_EQ(m_fins->getThickness(), 0.002) << "the body tube's, taken when it was added";
}

TEST_F(TubeFinSetOnBody, AutomaticRadiusForEveryFinCount)
{
    // Three tubes and more touch each other; one or two take the body radius (two would need an
    // infinite radius to touch, which getTubeSeparation() shows).
    struct Step
    {
        int  count;
        Pins pins;
    };
    const std::vector<Step> steps{{.count = 3, .pins = pinsFins3()},
                                  {.count = 1, .pins = pinsFins1()},
                                  {.count = 2, .pins = pinsFins2()},
                                  {.count = 8, .pins = pinsFins8()},
                                  {.count = 5, .pins = pinsFins5()}};
    for (const Step& step : steps)
    {
        m_fins->setFinCount(step.count);
        EXPECT_EQ(takeEvents(), Events{kBoth}) << step.count;
        EXPECT_EQ(differences(step.pins, *m_fins), "") << step.count << " tubes";
        EXPECT_EQ(m_fins->getPatternName(), std::format("{}-tubefin-ring", step.count));
    }
}

TEST_F(TubeFinSetOnBody, ManualDimensions)
{
    m_fins->setFinCount(4);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setOuterRadius(0.01);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setThickness(0.001);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setBaseRotation(std::numbers::pi / 3);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setAxialOffset(-0.02);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setLength(0.08);
    EXPECT_EQ(takeEvents(), Events{kBoth});

    EXPECT_EQ(differences(pinsManual(), *m_fins), "");
    EXPECT_FALSE(m_fins->isOuterRadiusAutomatic());
}

TEST_F(TubeFinSetOnBody, AxialMethodKeepsThePositionAndFiresANonFunctionalChange)
{
    makeManual();

    m_fins->setAxialMethod(AxialMethod::TOP);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    // The position stays; the stored offset changes, and with it the longitudinal unit inertia
    // (OpenRocket adds the square of the stored offset).
    EXPECT_EQ(differences(pinsTop(), *m_fins), "");
    EXPECT_EQ(m_fins->getAxialMethod(), AxialMethod::TOP);

    m_fins->setAxialMethod(AxialMethod::TOP);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional}) << "also for the method in force";

    m_fins->setAxialMethod(AxialMethod::MIDDLE);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    EXPECT_TRUE(matchesJavaValue(0.09, m_fins->getAxialOffset())) << m_fins->getAxialOffset();
    EXPECT_TRUE(matchesJavaValue(0.03471433333333333, m_fins->getLongitudinalUnitInertia()))
        << m_fins->getLongitudinalUnitInertia();
}

TEST_F(TubeFinSetOnBody, AfterIsOnlyAnotherWayToDescribeThePosition)
{
    // TubeFinSet::isAfter() is always false, so a tube fin set whose axial method is AFTER is not
    // put behind its previous sibling (a rail button is): its offset is measured from the end of
    // the body, and it stays where it is.
    makeManual();
    m_body->addChild(std::make_unique<QtRocket::LaunchLug>(), 0);
    EXPECT_EQ(takeEvents(), Events{kTree | kBoth});

    m_fins->setAxialMethod(AxialMethod::AFTER);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    EXPECT_EQ(m_fins->getAxialMethod(), AxialMethod::AFTER);
    EXPECT_FALSE(m_fins->isAfter());
    EXPECT_EQ(m_fins->getAxialOffset(), m_fins->getPosition().x - m_body->getLength());
    EXPECT_EQ(differences(pinsAfter(), *m_fins), "");

    m_fins->setAxialOffset(-0.15);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(differences(pinsAfterOffset(), *m_fins), "");

    m_fins->setAxialMethod(AxialMethod::BOTTOM);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    EXPECT_TRUE(matchesJavaValue(-0.06999999999999998, m_fins->getAxialOffset()))
        << m_fins->getAxialOffset();
    EXPECT_TRUE(matchesJavaValue(0.15, m_fins->getPosition().x)) << m_fins->getPosition().x;
}

TEST_F(TubeFinSetOnBody, ASingleTubeSitsBesideTheBody)
{
    makeManual();
    m_fins->setAxialMethod(AxialMethod::MIDDLE);
    m_fins->setFinCount(1);
    // Its CG is off the axis by the outer radius plus the body radius, turned by the base
    // rotation, and its unit inertias are those of one tube.
    EXPECT_EQ(differences(pinsSingle(), *m_fins), "");
}

TEST_F(TubeFinSetOnBody, FinCountIsComparedBeforeItIsClamped)
{
    m_fins->setFinCount(6);
    EXPECT_EQ(takeEvents(), Events{}) << "the current count";

    m_fins->setFinCount(1);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setFinCount(0);
    EXPECT_EQ(takeEvents(), Events{kBoth}) << "0 differs from 1 and is then clamped to it";
    EXPECT_EQ(m_fins->getFinCount(), 1);

    m_fins->setFinCount(12);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getFinCount(), 8);
    m_fins->setFinCount(9);
    EXPECT_EQ(takeEvents(), Events{kBoth}) << "9 differs from 8 and is then clamped to it";
    EXPECT_EQ(m_fins->getFinCount(), 8);

    m_fins->setInstanceCount(6);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getFinCount(), 6);
    EXPECT_EQ(m_fins->getInstanceCount(), 6);
    m_fins->setFinCount(-3);
    EXPECT_EQ(m_fins->getInstanceCount(), 1);
}

TEST_F(TubeFinSetOnBody, RadiusAndThicknessSetters)
{
    makeManual();

    m_fins->setLength(0.08);
    EXPECT_EQ(takeEvents(), Events{}) << "the current length";
    m_fins->setOuterRadius(0.01);
    EXPECT_EQ(takeEvents(), Events{}) << "the current manual radius";

    m_fins->setOuterRadiusAutomatic(true);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_fins->setOuterRadiusAutomatic(true);
    EXPECT_EQ(takeEvents(), Events{});
    m_fins->setFinCount(6);
    EXPECT_TRUE(matchesJavaValue(0.024999999999999998, m_fins->getOuterRadius()));
    EXPECT_EQ(m_fins->getThickness(), 0.001);
    static_cast<void>(takeEvents());

    m_fins->setOuterRadius(0.01);
    EXPECT_EQ(takeEvents(), Events{kBoth}) << "the stored radius, but it was automatic";
    EXPECT_FALSE(m_fins->isOuterRadiusAutomatic());

    m_fins->setThickness(0.001);
    EXPECT_EQ(takeEvents(), Events{}) << "the current thickness";
    m_fins->setThickness(5);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getThickness(), 0.01) << "at most the outer radius";
    EXPECT_EQ(m_fins->getInnerRadius(), 0.0);
    m_fins->setThickness(-1);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getThickness(), 0.0);

    m_fins->setInnerRadius(0.004);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_TRUE(matchesJavaValue(0.006, m_fins->getThickness())) << m_fins->getThickness();
    EXPECT_TRUE(matchesJavaValue(0.004, m_fins->getInnerRadius())) << m_fins->getInnerRadius();

    m_fins->setOuterRadius(0.003);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getThickness(), 0.003) << "a wall thicker than the radius becomes it";
    EXPECT_EQ(m_fins->getOuterRadius(), 0.003);
    m_fins->setOuterRadius(-1);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getThickness(), 0.0);
    EXPECT_EQ(m_fins->getOuterRadius(), 0.0);

    // NaN never equals the stored thickness, so setting it fires every time.
    m_fins->setThickness(kNaN);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_TRUE(std::isnan(m_fins->getThickness()));
    m_fins->setThickness(kNaN);
    EXPECT_EQ(takeEvents(), Events{kBoth});
}

TEST_F(TubeFinSetOnBody, AngleMethodIsStoredAndTheRadiusSettersDoNothing)
{
    m_fins->setAngleMethod(AngleMethod::RELATIVE);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getAngleMethod(), AngleMethod::RELATIVE);
    m_fins->setAngleMethod(AngleMethod::RELATIVE);
    EXPECT_EQ(takeEvents(), Events{kBoth}) << "always, as Java";
    // The method changes nothing else.
    EXPECT_EQ(differences(pinsOnBody6(), *m_fins), "");

    m_fins->setRadius(RadiusMethod::FREE, 0.3);
    m_fins->setRadiusMethod(RadiusMethod::FREE);
    m_fins->setRadiusOffset(0.3);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(m_fins->getRadiusMethod(), RadiusMethod::COAXIAL);
    EXPECT_EQ(m_fins->getRadiusOffset(), 0.0);
}

/// One call of setAngleOffset() and what Java answers after it.
struct AngleStep
{
    double              angle;
    Events              events;
    double              stored;
    std::vector<double> instanceAngles;
};

/// The steps of TubeFinSetOnBody.AngleOffsetIsReducedToPlusMinusPi, six tubes starting at 0.
[[nodiscard]] std::vector<AngleStep> angleSteps()
{
    const double pi = std::numbers::pi;
    return {{.angle          = pi / 3,
             .events         = {kBoth},
             .stored         = 1.0471975511965976,
             .instanceAngles = {1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                                4.1887902047863905, 5.235987755982988, 6.283185307179585}},
            {.angle          = pi / 3,
             .events         = {},
             .stored         = 1.0471975511965976,
             .instanceAngles = {1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                                4.1887902047863905, 5.235987755982988, 6.283185307179585}},
            {.angle          = 4.0,
             .events         = {kBoth},
             .stored         = -2.2831853071795862,
             .instanceAngles = {4.0, 5.047197551196597, 6.094395102393195, 0.8584073464102069,
                                1.9056048976068043, 2.9528024488034017}},
            {.angle          = -pi,
             .events         = {kBoth},
             .stored         = -std::numbers::pi,
             .instanceAngles = {std::numbers::pi, 4.1887902047863905, 5.235987755982988, 0.0,
                                1.0471975511965974, 2.094395102393195}},
            {.angle          = 2 * pi,
             .events         = {kBoth},
             .stored         = 0.0,
             .instanceAngles = {0.0, 1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                                4.1887902047863905, 5.235987755982988}},
            // Within MathUtil::equals of the current offset: nothing happens.
            {.angle          = 1e-9,
             .events         = {},
             .stored         = 0.0,
             .instanceAngles = {0.0, 1.0471975511965976, 2.0943951023931953, std::numbers::pi,
                                4.1887902047863905, 5.235987755982988}},
            {.angle          = 0.25,
             .events         = {kBoth},
             .stored         = 0.25,
             .instanceAngles = {0.25, 1.2971975511965976, 2.3443951023931953, 3.391592653589793,
                                4.4387902047863905, 5.485987755982988}},
            {.angle          = 7.0,
             .events         = {kBoth},
             .stored         = 0.7168146928204138,
             .instanceAngles = {0.7168146928204138, 1.7640122440170114, 2.811209795213609,
                                3.858407346410207, 4.905604897606804, 5.952802448803402}},
            {.angle          = -4.0,
             .events         = {kBoth},
             .stored         = 2.2831853071795862,
             .instanceAngles = {2.2831853071795862, 3.3303828583761836, 4.377580409572781,
                                5.424777960769379, 0.18879020478639053, 1.235987755982988}}};
}

/// Checks what @p fins answers after setAngleOffset(step.angle) fired @p events.
void expectAngleStep(const AngleStep& step, const Events& events, const TubeFinSet& fins)
{
    EXPECT_EQ(events, step.events) << step.angle;
    EXPECT_TRUE(matchesJavaValue(step.stored, fins.getAngleOffset()))
        << step.angle << " gives " << fins.getAngleOffset();
    EXPECT_TRUE(matchesJavaValue(step.stored, fins.getBaseRotation())) << step.angle;
    EXPECT_TRUE(matchesJavaValue(step.stored, fins.getBaseRotationTransformation().xRotation()))
        << step.angle;
    Differences d;
    d.numbers("instanceAngles", step.instanceAngles, fins.getInstanceAngles());
    EXPECT_EQ(d.text(), "") << step.angle;
}

TEST_F(TubeFinSetOnBody, AngleOffsetIsReducedToPlusMinusPi)
{
    for (const AngleStep& step : angleSteps())
    {
        m_fins->setAngleOffset(step.angle);
        expectAngleStep(step, takeEvents(), *m_fins);
    }

    // An offset that counts as zero is stored, but the base rotation is the identity.
    m_fins->setAngleOffset(1e-9);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_fins->getAngleOffset(), 1e-9);
    EXPECT_TRUE(m_fins->getBaseRotationTransformation().isIdentity());
    EXPECT_EQ(m_fins->getBaseRotationTransformation().xRotation(), 0.0);
}

TEST_F(TubeFinSetOnBody, RotationTransformations)
{
    EXPECT_TRUE(m_fins->getBaseRotationTransformation().isIdentity());
    for (const int count : {1, 2, 3, 4, 5, 6, 7, 8})
    {
        m_fins->setFinCount(count);
        EXPECT_EQ(m_fins->getFinRotationTransformation(),
                  Transformation::rotateX(2 * std::numbers::pi / count))
            << count;
        EXPECT_EQ(m_fins->getFinRotation(), 2 * std::numbers::pi / count);
    }
    m_fins->setFinCount(3);
    EXPECT_TRUE(
        matchesJavaValue(2.0943951023931957, m_fins->getFinRotationTransformation().xRotation()));

    m_fins->setBaseRotation(0.5);
    EXPECT_EQ(m_fins->getBaseRotationTransformation(), Transformation::rotateX(0.5));
    // The base rotation turns the y axis towards z.
    const Coordinate turned =
        m_fins->getBaseRotationTransformation().transform(Coordinate{0, 1, 0});
    EXPECT_TRUE(matchesJavaValue(std::cos(0.5), turned.y));
    EXPECT_TRUE(matchesJavaValue(std::sin(0.5), turned.z));
}

// ===================================================================================== split

/// What OpenRocket answers for one of the tube fin sets splitInstances() leaves on the body.
struct SplitPins
{
    std::string_view name;
    double           angleOffset{};
    double           outerRadius{};
    double           mass{};
    double           instanceAngle{};
    Coordinate       instanceOffset;
    Coordinate       componentCG;
};

/// The differences between the single tubes @p split left on @p body, in the place of the
/// original, and Java's @p expected; empty when there are none. Every tube keeps the position of
/// the set, 0.2 m from the top of the body, and its @p axialOffset.
[[nodiscard]] std::string splitDifferences(std::span<const SplitPins>          expected,
                                           const RocketComponent::SplitResult& split,
                                           const BodyTube& body, double axialOffset)
{
    Differences d;
    if (split.components.size() != expected.size() || body.getChildCount() != expected.size())
    {
        d.problem(std::format("expected {} tube fin sets, got {} of {} children", expected.size(),
                              split.components.size(), body.getChildCount()));
        return d.text();
    }
    const double x = 0.19999999999999998;
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        const auto* single = dynamic_cast<const TubeFinSet*>(split.components[i]);
        if (single == nullptr || single != &body.getChild(i))
        {
            d.problem(std::format("[{}] is not the tube fin set at that index of the body", i));
            continue;
        }
        const SplitPins& pins = expected[i];
        d.name(std::format("[{}].name", i), pins.name, single->getName());
        d.integer(std::format("[{}].finCount", i), 1, single->getFinCount());
        d.number(std::format("[{}].angleOffset", i), pins.angleOffset, single->getAngleOffset());
        d.number(std::format("[{}].outerRadius", i), pins.outerRadius, single->getOuterRadius());
        d.number(std::format("[{}].mass", i), pins.mass, single->getMass());
        d.number(std::format("[{}].axialOffset", i), axialOffset, single->getAxialOffset());
        d.coordinate(std::format("[{}].position", i), Coordinate{x, 0.0, 0.0, 0.0},
                     single->getPosition());
        d.numbers(std::format("[{}].instanceAngles", i), {pins.instanceAngle},
                  single->getInstanceAngles());
        d.coordinates(std::format("[{}].instanceOffsets", i), {pins.instanceOffset},
                      single->getInstanceOffsets());
        d.coordinates(std::format("[{}].componentLocations", i),
                      {Coordinate{x, pins.instanceOffset.y, pins.instanceOffset.z, 0.0}},
                      single->getComponentLocations());
        d.coordinate(std::format("[{}].componentCG", i), pins.componentCG,
                     single->getComponentCG());
    }
    return d.text();
}

TEST_F(TubeFinSetOnBody, SplitInstancesLeavesSingleTubesAroundTheBody)
{
    m_fins->setFinCount(3);
    static_cast<void>(takeEvents());

    const RocketComponent::SplitResult split = m_fins->splitInstances();
    EXPECT_EQ(takeEvents(), Events{kTree | kBoth}) << "one event, as the rocket thaws";
    EXPECT_EQ(split.original.get(), m_fins) << "the original is out of the tree";
    EXPECT_EQ(m_fins->getParent(), nullptr);

    // setInstanceCount(1) is setFinCount(1), and the angles 2 pi i / 3 are reduced to -pi ... pi
    // (a launch lug and a rail button clamp theirs). A single automatic tube is as wide as the
    // body, and its CG is beside the body. Java's values.
    const double                 mass = 0.020508316842634204;
    const std::vector<SplitPins> expected{
        {.name           = "Tube Fin Set #1",
         .angleOffset    = 0.0,
         .outerRadius    = 0.025,
         .mass           = mass,
         .instanceAngle  = 0.0,
         .instanceOffset = Coordinate{0.0, 0.025, 0.0, 0.0},
         .componentCG    = Coordinate{0.05, 0.05, 0.0, mass}},
        {.name           = "Tube Fin Set #2",
         .angleOffset    = 2.0943951023931953,
         .outerRadius    = 0.025,
         .mass           = mass,
         .instanceAngle  = 2.0943951023931953,
         .instanceOffset = Coordinate{0.0, -0.012499999999999995, 0.02165063509461097, 0.0},
         .componentCG    = Coordinate{0.05, -0.02499999999999999, 0.04330127018922194, mass}},
        {.name           = "Tube Fin Set #3",
         .angleOffset    = -2.0943951023931957,
         .outerRadius    = 0.025,
         .mass           = mass,
         .instanceAngle  = 4.1887902047863905,
         .instanceOffset = Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
         .componentCG    = Coordinate{0.05, -0.025000000000000012, -0.043301270189221926, mass}}};
    EXPECT_EQ(splitDifferences(expected, split, *m_body, 0.0), "");
    const auto* last = dynamic_cast<const TubeFinSet*>(&m_body->getChild(2));
    ASSERT_NE(last, nullptr);
    EXPECT_TRUE(last->isOuterRadiusAutomatic());
    EXPECT_EQ(last->getThickness(), 0.002);
    EXPECT_FALSE(last->isMassOverridden());
}

TEST_F(TubeFinSetOnBody, SplitInstancesSharesOutTheOverrideMass)
{
    makeManual();
    m_fins->setMassOverridden(true);
    m_fins->setOverrideMass(0.2);
    static_cast<void>(takeEvents());

    const RocketComponent::SplitResult split = m_fins->splitInstances();
    EXPECT_EQ(takeEvents(), Events{kTree | kBoth});

    // Four manual tubes from the base rotation pi / 3 on. Java's values.
    const double                 mass = 0.003247150166750408;
    const std::vector<SplitPins> expected{
        {.name           = "Tube Fin Set #1",
         .angleOffset    = 1.0471975511965976,
         .outerRadius    = 0.01,
         .mass           = 0.05,
         .instanceAngle  = 1.0471975511965976,
         .instanceOffset = Coordinate{0.0, 0.012500000000000004, 0.021650635094610966, 0.0},
         .componentCG    = Coordinate{0.04, 0.017500000000000005, 0.030310889132455353, mass}},
        {.name           = "Tube Fin Set #2",
         .angleOffset    = 2.617993877991494,
         .outerRadius    = 0.01,
         .mass           = 0.05,
         .instanceAngle  = 2.617993877991494,
         .instanceOffset = Coordinate{0.0, -0.021650635094610963, 0.01250000000000001, 0.0},
         .componentCG    = Coordinate{0.04, -0.03031088913245535, 0.017500000000000012, mass}},
        {.name           = "Tube Fin Set #3",
         .angleOffset    = -2.0943951023931957,
         .outerRadius    = 0.01,
         .mass           = 0.05,
         .instanceAngle  = 4.1887902047863905,
         .instanceOffset = Coordinate{0.0, -0.012500000000000011, -0.02165063509461096, 0.0},
         .componentCG    = Coordinate{0.04, -0.01750000000000001, -0.03031088913245535, mass}},
        {.name           = "Tube Fin Set #4",
         .angleOffset    = -0.5235987755982991,
         .outerRadius    = 0.01,
         .mass           = 0.05,
         .instanceAngle  = 5.759586531581287,
         .instanceOffset = Coordinate{0.0, 0.02165063509461096, -0.012500000000000011, 0.0},
         .componentCG    = Coordinate{0.04, 0.03031088913245535, -0.01750000000000001, mass}}};
    EXPECT_EQ(splitDifferences(expected, split, *m_body, -0.02), "");
    EXPECT_EQ(m_body->getChild(3).getOverrideMass(), 0.05);
    EXPECT_TRUE(m_body->getChild(3).isMassOverridden());
}

TEST_F(TubeFinSetOnBody, SplitInstancesOfASingleTubeLeavesIt)
{
    m_fins->setFinCount(1);
    static_cast<void>(takeEvents());

    const RocketComponent::SplitResult split = m_fins->splitInstances();
    EXPECT_EQ(takeEvents(), Events{kTree});
    EXPECT_EQ(split.original, nullptr);
    ASSERT_EQ(split.components.size(), 1U);
    EXPECT_EQ(split.components.front(), m_fins);
    EXPECT_EQ(m_body->getChildCount(), 1U);
    EXPECT_EQ(m_fins->getName(), "Tube Fin Set");
}

// =================================================================================== presets

/// A BODY_TUBE preset 2 m long with outer diameter 2 m, inner diameter 1 m and mass 100 kg (the
/// preset of the Java preset component tests).
[[nodiscard]] ComponentPreset bodyTubePreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

TEST(TubeFinSetPreset, LoadsABodyTubePreset)
{
    const ComponentPreset preset = bodyTubePreset();
    TubeFinSet            fins;
    fins.loadPreset(&preset);

    EXPECT_EQ(fins.getPresetComponent(), &preset);
    EXPECT_EQ(fins.getLength(), 2.0);
    EXPECT_FALSE(fins.isOuterRadiusAutomatic());
    EXPECT_EQ(fins.getOuterRadius(), 1.0);
    EXPECT_EQ(fins.getInnerRadius(), 0.5);
    EXPECT_EQ(fins.getThickness(), 0.5);
    EXPECT_EQ(fins.getFinCount(), 6);
    EXPECT_EQ(fins.getMaterial(), preset.get(ComponentPreset::kMaterial));
    EXPECT_EQ(fins.getMaterial().getName(), "TubeCustom");
    EXPECT_TRUE(matchesJavaValue(21.22065907891938, fins.getMaterial().getDensity()));
    // The preset's mass is that of one tube; the set has six.
    EXPECT_NEAR(fins.getMass(), 600.0, 1e-9);
}

TEST(TubeFinSetPreset, APresetWithoutAnInnerDiameterKeepsTheThickness)
{
    // No factory makes such a BODY_TUBE preset; a BULK_HEAD one has an outer diameter only (a
    // component loads any preset it is given, as in Java).
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::BULK_HEAD);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 0.5);
    presetspec.put(ComponentPreset::kOuterDiameter, 0.1);
    const QtRocket::MaterialStorage materials;
    const ComponentPreset preset = ComponentPresetFactory::create(presetspec, materials).value();

    TubeFinSet fins;
    fins.setThickness(0.0);  // detached: clamped to the automatic radius, 0
    fins.loadPreset(&preset);
    EXPECT_EQ(fins.getLength(), 0.5);
    EXPECT_FALSE(fins.isOuterRadiusAutomatic());
    EXPECT_EQ(fins.getOuterRadius(), 0.05);
    EXPECT_EQ(fins.getThickness(), 0.0);
    EXPECT_EQ(fins.getMaterial(), QtRocket::ExternalComponent::defaultMaterial());
}

TEST(TubeFinSetPreset, TheRadiusAndThicknessSettersClearThePreset)
{
    const ComponentPreset preset = bodyTubePreset();
    TubeFinSet            fins;
    fins.loadPreset(&preset);

    fins.setLength(1.0);
    EXPECT_EQ(fins.getPresetComponent(), &preset);
    fins.setFinCount(3);
    EXPECT_EQ(fins.getPresetComponent(), &preset);
    fins.setBaseRotation(1.0);
    EXPECT_EQ(fins.getPresetComponent(), &preset);

    fins.setThickness(0.1);
    EXPECT_EQ(fins.getPresetComponent(), nullptr);

    fins.loadPreset(&preset);
    fins.setOuterRadius(0.5);
    EXPECT_EQ(fins.getPresetComponent(), nullptr);

    fins.loadPreset(&preset);
    fins.setOuterRadiusAutomatic(true);
    EXPECT_EQ(fins.getPresetComponent(), nullptr);

    fins.loadPreset(&preset);
    fins.setInnerRadius(0.25);
    EXPECT_EQ(fins.getPresetComponent(), nullptr);
}

TEST_F(TubeFinSetOnBody, LoadingAPresetFiresOnlyTheNonFunctionalChange)
{
    // Unlike BodyTube and LaunchLug, TubeFinSet.loadFromPreset() fires nothing of its own.
    const ComponentPreset preset = bodyTubePreset();
    m_fins->loadPreset(&preset);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    EXPECT_EQ(m_fins->getThickness(), 0.5);

    // With a preset, a setter that clears it fires its own change and then the clearing.
    m_fins->setOuterRadius(0.75);
    EXPECT_EQ(takeEvents(), (Events{kBoth, kNonFunctional}));
    EXPECT_EQ(m_fins->getPresetComponent(), nullptr);
}

// ====================================================================================== mass

// MassCalculatorTest.testTubeFinMass. Its rocket is OpenRocketDocumentFactory.createNewRocket():
// a Rocket with one AxialStage whose stages are all active, in a document, and the document's
// constructor enables the rocket's events. So the tube fin set is added to a live rocket: the
// add event and the event of BodyTube::addChild()'s thickness reach it.
TEST(TubeFinSetMass, TubeFinMass)
{
    constexpr double kEpsilon = 0.00000001;  // MassCalculatorTest.EPSILON

    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    rocket.getSelectedConfiguration().setAllStages();
    rocket.enableEvents();
    auto& bodyTube = stage.addChild(std::make_unique<BodyTube>());
    auto  newFins  = std::make_unique<TubeFinSet>();
    newFins->setOuterRadius(0.04);
    newFins->setThickness(0.002);
    newFins->setLength(0.1);
    newFins->setInstanceCount(3);
    TubeFinSet& tubeFinSet = bodyTube.addChild(std::move(newFins));

    EXPECT_NEAR(0.0001470265, tubeFinSet.getComponentVolume(), kEpsilon);
    EXPECT_NEAR(0.0999780446, tubeFinSet.getComponentMass(), kEpsilon);
    EXPECT_NEAR(0.0999780446, tubeFinSet.getMass(), kEpsilon);
    // Java's full values (the JUnit literals are rounded).
    EXPECT_TRUE(matchesJavaValue(1.4702653618800244E-4, tubeFinSet.getComponentVolume()));
    EXPECT_TRUE(matchesJavaValue(0.09997804460784167, tubeFinSet.getComponentMass()));
    // The events placed the set at the bottom of the 0.2 m body tube (without them it stays at
    // 0), and it kept the thickness it was given.
    EXPECT_EQ(tubeFinSet.getPosition(), (Coordinate{0.1, 0.0, 0.0}));
    EXPECT_EQ(tubeFinSet.getThickness(), 0.002);

    tubeFinSet.setInstanceCount(4);

    EXPECT_NEAR(0.000196035, tubeFinSet.getComponentVolume(), kEpsilon);
    EXPECT_NEAR(0.133304059, tubeFinSet.getComponentMass(), kEpsilon);
    EXPECT_NEAR(0.133304059, tubeFinSet.getMass(), kEpsilon);
    EXPECT_TRUE(matchesJavaValue(1.9603538158400325E-4, tubeFinSet.getComponentVolume()));
    EXPECT_TRUE(matchesJavaValue(0.1333040594771222, tubeFinSet.getComponentMass()));

    tubeFinSet.setMassOverridden(true);
    tubeFinSet.setOverrideMass(0.02);

    EXPECT_NEAR(0.133304059, tubeFinSet.getComponentMass(), kEpsilon);
    EXPECT_NEAR(0.02, tubeFinSet.getMass(), kEpsilon);
}

// ====================================================================== BodyTube::addChild()

TEST(TubeFinSetBodyTubeHook, ANewTubeFinSetTakesTheBodyTubesThickness)
{
    BodyTube tube(0.3, 0.025, 0.002);
    auto     fins = std::make_unique<TubeFinSet>();
    EXPECT_TRUE(std::isnan(fins->getThickness()));
    const TubeFinSet& added = tube.addChild(std::move(fins));
    EXPECT_EQ(added.getThickness(), 0.002);

    // Also when it is inserted at an index.
    const TubeFinSet& indexed = tube.addChild(std::make_unique<TubeFinSet>(), 0);
    EXPECT_EQ(indexed.getThickness(), 0.002);
    EXPECT_EQ(&tube.getChild(0), &indexed);

    // A detached copy has no body, so its automatic radius, and with it the thickness it
    // reports, is 0.
    const std::unique_ptr<TubeFinSet> copy =
        QtRocket::componentCast<TubeFinSet>(indexed.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getThickness(), 0.0);
}

TEST(TubeFinSetBodyTubeHook, TheThicknessGoesThroughTheSetter)
{
    // A filled body tube's thickness is its radius; the setter clamps it to the tubes' radius.
    BodyTube          filled(0.3, 0.025, true);
    const TubeFinSet& onFilled = filled.addChild(std::make_unique<TubeFinSet>());
    EXPECT_TRUE(matchesJavaValue(0.024999999999999998, onFilled.getThickness()))
        << onFilled.getThickness();
    EXPECT_TRUE(matchesJavaValue(0.024999999999999998, onFilled.getOuterRadius()));
    EXPECT_NEAR(onFilled.getInnerRadius(), 0.0, 1e-15);

    BodyTube tube(0.3, 0.025, 0.002);
    auto     thin = std::make_unique<TubeFinSet>();
    thin->setOuterRadius(0.001);
    const TubeFinSet& small = tube.addChild(std::move(thin));
    EXPECT_EQ(small.getThickness(), 0.001) << "at most the outer radius";
}

TEST(TubeFinSetBodyTubeHook, AThicknessThatIsSetIsKept)
{
    BodyTube tube(0.3, 0.025, 0.002);
    auto     fins = std::make_unique<TubeFinSet>();
    fins->setOuterRadius(0.01);
    fins->setThickness(0.0005);
    const TubeFinSet& added = tube.addChild(std::move(fins));
    EXPECT_EQ(added.getThickness(), 0.0005);
}

TEST_F(TubeFinSetOnBody, TheHookFiresTheSettersEventAfterTheTreeChange)
{
    m_body->addChild(std::make_unique<TubeFinSet>());
    // addChild()'s own event (the tree, mass and aerodynamics), then setThickness()'s.
    EXPECT_EQ(takeEvents(), (Events{kTree | kBoth, kBoth}));

    // Removed and added again, the thickness is set already: only the tree change.
    std::unique_ptr<RocketComponent> removed = m_body->removeChild(m_fins);
    ASSERT_NE(removed, nullptr);
    static_cast<void>(takeEvents());
    m_fins = nullptr;
    m_body->addChild(std::move(removed));
    EXPECT_EQ(takeEvents(), Events{kTree | kBoth});
}

// ====================================================================================== copy

TEST(TubeFinSet, CopyKeepsEveryField)
{
    TubeFinSet original;
    original.setMaterial(Material::newMaterial(Material::Type::BULK, "x", 1000, true));
    original.setOuterRadius(0.02);
    original.setThickness(0.001);
    original.setFinCount(3);
    original.setBaseRotation(0.5);
    original.setAngleMethod(AngleMethod::MIRROR_XY);
    original.getInsideColorComponentHandler().setSeparateInsideOutside(true);

    const std::unique_ptr<TubeFinSet> copy =
        QtRocket::componentCast<TubeFinSet>(original.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(differences(pinsCopy(), *copy), "");
    EXPECT_EQ(copy->getMaterial(), original.getMaterial());
    EXPECT_EQ(copy->getAngleMethod(), AngleMethod::MIRROR_XY);
    EXPECT_EQ(copy->getFinRotationTransformation(), original.getFinRotationTransformation());
    EXPECT_EQ(copy->getBaseRotationTransformation(), Transformation::rotateX(0.5));
    EXPECT_TRUE(copy->getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_NE(copy->getId(), original.getId());

    const std::unique_ptr<RocketComponent> sameId = original.copyWithOriginalId();
    EXPECT_EQ(sameId->getId(), original.getId());
    EXPECT_EQ(sameId->kind(), ComponentKind::TUBE_FIN_SET);

    // The copy is independent of the original.
    original.setFinCount(5);
    EXPECT_EQ(copy->getFinCount(), 3);
}

// ========================================================================== default material

/// The built-in bulk material @p name of @p storage.
[[nodiscard]] Material builtinBulk(const QtRocket::MaterialStorage& storage, std::string_view name)
{
    return storage.findMaterial(Material::Type::BULK, name)
        .value_or(Material::newMaterial(Material::Type::BULK, "<not found>", 0, true));
}

TEST(TubeFinSet, ApplyDefaultMaterialTakesThePreferencesDefaultForItsClass)
{
    QtRocket::MaterialStorage     storage;
    QtRocket::InMemoryPreferences prefs;
    QtRocket::addBuiltinMaterials(storage);

    TubeFinSet fins;
    fins.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(fins.getMaterial().getName(), "Cardboard") << "the built-in fallback";

    const Material balsa = builtinBulk(storage, "Balsa");
    const Material birch = builtinBulk(storage, "Birch");
    ASSERT_EQ(balsa.getName(), "Balsa");
    ASSERT_EQ(birch.getName(), "Birch");
    QtRocket::setDefaultComponentMaterial(prefs, "Tube", balsa);
    fins.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(fins.getMaterial(), balsa) << "a superclass's default";
    QtRocket::setDefaultComponentMaterial(prefs, "TubeFinSet", birch);
    fins.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(fins.getMaterial(), birch) << "its own class first";
}

}  // namespace
