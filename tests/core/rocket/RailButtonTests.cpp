// RailButtonTest.java (core/src/test/.../rocketcomponent), ported, and the rail button's own
// behaviour.
//
// The values of the suites that have no JUnit counterpart (the Pins below and the numbers in
// those tests) were computed with OpenRocket itself: a small Java program built the same
// components with the same calls and printed what OpenRocket's RailButton answers (commit
// 5f164fd0e on JDK 17). Values that went through sin() or cos() are compared with a relative
// tolerance of 1e-12.

#include "QtRocket/rocket/RailButton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

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
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::RailButton;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;

/// RailButtonTest.EPSILON (MathUtil.EPSILON).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// The event types of the setters: AEROMASS_CHANGE, AERODYNAMIC_CHANGE, MASS_CHANGE and
/// NONFUNCTIONAL_CHANGE.
constexpr int kBoth          = ComponentChangeEvent::kBothChange;
constexpr int kAerodynamic   = ComponentChangeEvent::kAerodynamicChange;
constexpr int kMass          = ComponentChangeEvent::kMassChange;
constexpr int kNonFunctional = ComponentChangeEvent::kNonFunctionalChange;

using Events = std::vector<int>;

/// The four assertions RailButtonTest makes on a CG: x, y, z and the weight within EPSILON.
void expectCG(const Coordinate& cg, double x, double y, double z, double weight)
{
    EXPECT_NEAR(x, cg.x, kEpsilon) << " RailButton CG has the wrong x value: ";
    EXPECT_NEAR(y, cg.y, kEpsilon) << " RailButton CG has the wrong y value: ";
    EXPECT_NEAR(z, cg.z, kEpsilon) << " RailButton CG has the wrong z value: ";
    EXPECT_NEAR(weight, cg.weight, kEpsilon) << " RailButton CM has the wrong value: ";
}

// =========================================================================== RailButtonTest

// Java: testCMSingleInstance
TEST(RailButtonTest, CMSingleInstance)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newButton = std::make_unique<RailButton>();
    newButton->setOuterDiameter(0.05);
    newButton->setTotalHeight(0.05);
    RailButton& button = bodyTube.addChild(std::move(newButton));

    // Test normal CG
    expectCG(button.getCG(), 0, -0.05, 0, 0.014435995);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0, 0, 0.05, 0.014435995);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0, 0.025, -0.04330127, 0.014435995);

    // Change dimensions
    button.setOuterDiameter(0.025);
    button.setTotalHeight(0.02);
    button.setAngleOffset(0);
    expectCG(button.getCG(), 0, 0.035, 0, 0.003930195);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0, 0, 0.035, 0.003930195);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0, 0.0175, -0.03031089, 0.003930195);
}

// Java: testCMSingleInstanceOverride
TEST(RailButtonTest, CMSingleInstanceOverride)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newButton = std::make_unique<RailButton>();
    newButton->setOuterDiameter(0.05);
    newButton->setTotalHeight(0.05);
    newButton->setCGOverridden(true);
    newButton->setOverrideCGX(0.0123);
    RailButton& button = bodyTube.addChild(std::move(newButton));

    // Test normal CG
    expectCG(button.getCG(), 0.0123, -0.05, 0, 0.014435995);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.0123, 0, 0.05, 0.014435995);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.0123, 0.025, -0.04330127, 0.014435995);

    // Change dimensions
    button.setOuterDiameter(0.025);
    button.setTotalHeight(0.02);
    button.setAngleOffset(0);
    button.setOverrideCGX(0.0321);
    button.setMassOverridden(true);
    button.setOverrideMass(0.1);
    expectCG(button.getCG(), 0.0321, 0.035, 0, 0.1);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.0321, 0, 0.035, 0.1);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.0321, 0.0175, -0.03031089, 0.1);
}

// Java: testCMMultipleInstances
TEST(RailButtonTest, CMMultipleInstances)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newButton = std::make_unique<RailButton>();
    newButton->setOuterDiameter(0.05);
    newButton->setTotalHeight(0.05);
    newButton->setInstanceCount(3);
    newButton->setInstanceSeparation(0.2);
    RailButton& button = bodyTube.addChild(std::move(newButton));

    // Test normal CG
    expectCG(button.getCG(), 0.2, -0.05, 0, 0.043307985);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.2, 0, 0.05, 0.043307985);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.2, 0.025, -0.04330127, 0.043307985);

    // Change dimensions
    button.setOuterDiameter(0.025);
    button.setTotalHeight(0.02);
    button.setAngleOffset(0);
    button.setInstanceCount(2);
    button.setInstanceSeparation(0.15);
    expectCG(button.getCG(), 0.075, 0.035, 0, 0.00786039);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.075, 0, 0.035, 0.00786039);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.075, 0.0175, -0.03031089, 0.00786039);
}

// Java: testCMMultipleInstancesOverride
TEST(RailButtonTest, CMMultipleInstancesOverride)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newButton = std::make_unique<RailButton>();
    newButton->setOuterDiameter(0.05);
    newButton->setTotalHeight(0.05);
    newButton->setInstanceCount(3);
    newButton->setInstanceSeparation(0.2);
    newButton->setCGOverridden(true);
    newButton->setOverrideCGX(0.0123);
    RailButton& button = bodyTube.addChild(std::move(newButton));

    // Test normal CG
    expectCG(button.getCG(), 0.0123, -0.05, 0, 0.043307985);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.0123, 0, 0.05, 0.043307985);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.0123, 0.025, -0.04330127, 0.043307985);

    // Change dimensions
    button.setOuterDiameter(0.025);
    button.setTotalHeight(0.02);
    button.setAngleOffset(0);
    button.setInstanceCount(2);
    button.setInstanceSeparation(0.15);
    button.setOverrideCGX(0.0321);
    button.setMassOverridden(true);
    button.setOverrideMass(0.2);
    expectCG(button.getCG(), 0.0321, 0.035, 0, 0.2);

    // Test rotated CG
    button.setAngleOffset(std::numbers::pi / 2);
    expectCG(button.getCG(), 0.0321, 0, 0.035, 0.2);

    button.setAngleOffset(-std::numbers::pi / 3);
    expectCG(button.getCG(), 0.0321, 0.0175, -0.03031089, 0.2);
}

// ========================================================================= Java's own values

/// Whether @p actual is the Java value @p expected: both NaN, equal, or within 1e-12 relative,
/// plus 1e-15 absolute for values near zero such as sin(pi) * r.
[[nodiscard]] bool matches(double expected, double actual)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (expected == actual)
    {
        return true;
    }
    return std::abs(actual - expected) <=
           (1e-12 * std::max(std::abs(expected), std::abs(actual))) + 1e-15;
}

/// Collects the differences between Java's values and the computed ones, one line each.
class Differences
{
public:
    void number(std::string_view field, double expected, double actual)
    {
        if (!matches(expected, actual))
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    void coordinate(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        number(std::format("{}.x", field), expected.x, actual.x);
        number(std::format("{}.y", field), expected.y, actual.y);
        number(std::format("{}.z", field), expected.z, actual.z);
        number(std::format("{}.weight", field), expected.weight, actual.weight);
    }

    void coordinates(std::string_view field, const std::vector<Coordinate>& expected,
                     const std::vector<Coordinate>& actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: expected {} points, got {}\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < expected.size(); i++)
        {
            coordinate(std::format("{}[{}]", field, i), expected[i], actual[i]);
        }
    }

    /// Empty when everything matched.
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

private:
    std::string m_text;
};

/// What OpenRocket's RailButton answers in one state.
struct Pins
{
    int                     instanceCount{};
    double                  outerDiameter{};
    double                  innerDiameter{};
    double                  totalHeight{};
    double                  flangeHeight{};
    double                  baseHeight{};
    double                  innerHeight{};
    double                  screwHeight{};
    double                  maxBaseHeight{};
    double                  maxFlangeHeight{};
    double                  minTotalHeight{};
    double                  angleOffset{};
    double                  instanceSeparation{};
    double                  axialOffset{};
    Coordinate              position;
    double                  componentVolume{};
    double                  componentMass{};
    Coordinate              componentCG;
    std::vector<Coordinate> componentBounds;
    Coordinate              boxMin;
    Coordinate              boxMax;
    std::vector<Coordinate> instanceOffsets;
    std::vector<Coordinate> componentLocations;
};

/// The differences between @p button and Java's @p expected, empty when there are none.
[[nodiscard]] std::string differences(const Pins& expected, const RailButton& button)
{
    Differences d;
    d.number("instanceCount", expected.instanceCount, button.getInstanceCount());
    d.number("outerDiameter", expected.outerDiameter, button.getOuterDiameter());
    d.number("innerDiameter", expected.innerDiameter, button.getInnerDiameter());
    d.number("totalHeight", expected.totalHeight, button.getTotalHeight());
    d.number("flangeHeight", expected.flangeHeight, button.getFlangeHeight());
    d.number("baseHeight", expected.baseHeight, button.getBaseHeight());
    d.number("innerHeight", expected.innerHeight, button.getInnerHeight());
    d.number("screwHeight", expected.screwHeight, button.getScrewHeight());
    d.number("maxBaseHeight", expected.maxBaseHeight, button.getMaxBaseHeight());
    d.number("maxFlangeHeight", expected.maxFlangeHeight, button.getMaxFlangeHeight());
    d.number("minTotalHeight", expected.minTotalHeight, button.getMinTotalHeight());
    d.number("angleOffset", expected.angleOffset, button.getAngleOffset());
    d.number("instanceSeparation", expected.instanceSeparation, button.getInstanceSeparation());
    d.number("length", 0.0, button.getLength());
    d.number("axialOffset", expected.axialOffset, button.getAxialOffset());
    d.coordinate("position", expected.position, button.getPosition());
    d.number("componentVolume", expected.componentVolume, button.getComponentVolume());
    d.number("componentMass", expected.componentMass, button.getComponentMass());
    d.coordinate("componentCG", expected.componentCG, button.getComponentCG());
    d.number("longitudinalUnitInertia", 0.0, button.getLongitudinalUnitInertia());
    d.number("rotationalUnitInertia", 0.0, button.getRotationalUnitInertia());
    d.coordinates("componentBounds", expected.componentBounds, button.getComponentBounds());
    const BoundingBox box = button.getInstanceBoundingBox();
    d.coordinate("instanceBoundingBox.min", expected.boxMin, box.min());
    d.coordinate("instanceBoundingBox.max", expected.boxMax, box.max());
    d.coordinates("instanceOffsets", expected.instanceOffsets, button.getInstanceOffsets());
    d.coordinates("componentLocations", expected.componentLocations,
                  button.getComponentLocations());
    return d.text();
}

// ---- Java's values, one function per state (printed by the Java program)

[[nodiscard]] Pins pinsDetached()
{
    return Pins{.instanceCount      = 1,
                .outerDiameter      = 0.0097,
                .innerDiameter      = 0.008,
                .totalHeight        = 0.0097,
                .flangeHeight       = 0.002,
                .baseHeight         = 0.002,
                .innerHeight        = 0.0057,
                .screwHeight        = 0.0,
                .maxBaseHeight      = 0.0077,
                .maxFlangeHeight    = 0.0077,
                .minTotalHeight     = 0.004,
                .angleOffset        = std::numbers::pi,
                .instanceSeparation = 0.0582,
                .axialOffset        = 0.0,
                .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
                .componentVolume    = 5.821057027836528E-7,
                .componentMass      = 8.265900979527869E-4,
                .componentCG        = Coordinate{0.0, -0.004849999999999999, 5.939536975864662E-19,
                                                 8.265900979527869E-4},
                .componentBounds    = {Coordinate{0.00485, 0.0097, 0.00485, 0.0},
                                       Coordinate{0.00485, 0.0097, -0.00485, 0.0},
                                       Coordinate{0.00485, 0.0, 0.00485, 0.0},
                                       Coordinate{0.00485, 0.0, -0.00485, 0.0},
                                       Coordinate{-0.00485, 0.0, 0.00485, 0.0},
                                       Coordinate{-0.00485, 0.0, -0.00485, 0.0},
                                       Coordinate{-0.00485, 0.0097, 0.00485, 0.0},
                                       Coordinate{-0.00485, 0.0097, -0.00485, 0.0}},
                .boxMin             = Coordinate{-0.00485, -0.0097, -0.00485, 0.0},
                .boxMax             = Coordinate{0.00485, 0.0097, 0.00485, 0.0},
                .instanceOffsets    = {Coordinate{0.0, -0.0, 0.0, 0.0}},
                .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor2()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.02,
        .innerDiameter      = 0.008,
        .totalHeight        = 0.015,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.002,
        .innerHeight        = 0.011,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.013,
        .maxFlangeHeight    = 0.013,
        .minTotalHeight     = 0.004,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.0582,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 1.809557368467721E-6,
        .componentMass      = 0.0025695714632241635,
        .componentCG =
            Coordinate{0.0, -0.007500000000000001, 9.18485099360515E-19, 0.0025695714632241635},
        .componentBounds = {Coordinate{0.01, 0.015, 0.01, 0.0}, Coordinate{0.01, 0.015, -0.01, 0.0},
                            Coordinate{0.01, 0.0, 0.01, 0.0}, Coordinate{0.01, 0.0, -0.01, 0.0},
                            Coordinate{-0.01, 0.0, 0.01, 0.0}, Coordinate{-0.01, 0.0, -0.01, 0.0},
                            Coordinate{-0.01, 0.015, 0.01, 0.0},
                            Coordinate{-0.01, 0.015, -0.01, 0.0}},
        .boxMin          = Coordinate{-0.01, -0.015, -0.01, 0.0},
        .boxMax          = Coordinate{0.01, 0.015, 0.01, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor2small()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.005,
        .innerDiameter      = 0.005,
        .totalHeight        = 0.004,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.002,
        .innerHeight        = 0.0,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.002,
        .maxFlangeHeight    = 0.002,
        .minTotalHeight     = 0.004,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.0582,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 7.853981633974483E-8,
        .componentMass      = 1.1152653920243767E-4,
        .componentCG = Coordinate{0.0, -0.002, 2.4492935982947063E-19, 1.1152653920243767E-4},
        .componentBounds =
            {Coordinate{0.0025, 0.004, 0.0025, 0.0}, Coordinate{0.0025, 0.004, -0.0025, 0.0},
             Coordinate{0.0025, 0.0, 0.0025, 0.0}, Coordinate{0.0025, 0.0, -0.0025, 0.0},
             Coordinate{-0.0025, 0.0, 0.0025, 0.0}, Coordinate{-0.0025, 0.0, -0.0025, 0.0},
             Coordinate{-0.0025, 0.004, 0.0025, 0.0}, Coordinate{-0.0025, 0.004, -0.0025, 0.0}},
        .boxMin             = Coordinate{-0.0025, -0.004, -0.0025, 0.0},
        .boxMax             = Coordinate{0.0025, 0.004, 0.0025, 0.0},
        .instanceOffsets    = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor5()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.006,
        .totalHeight        = 0.01,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.003,
        .innerHeight        = 0.005,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.008,
        .maxFlangeHeight    = 0.007,
        .minTotalHeight     = 0.005,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.024,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 7.068583470577034E-7,
        .componentMass      = 0.0010037388528219388,
        .componentCG        = Coordinate{0.0, -0.0047, 5.75583995599256E-19, 0.0010037388528219388},
        .componentBounds = {Coordinate{0.006, 0.01, 0.006, 0.0},
                            Coordinate{0.006, 0.01, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.01, 0.006, 0.0},
                            Coordinate{-0.006, 0.01, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.01, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.01, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor5clamped()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.006,
        .totalHeight        = 0.01,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.008,
        .innerHeight        = 0.0,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.008,
        .maxFlangeHeight    = 0.002,
        .minTotalHeight     = 0.01,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.024,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 1.1309733552923255E-6,
        .componentMass      = 0.0016059821645151022,
        .componentCG        = Coordinate{0.0, -0.005, 6.123233995736766E-19, 0.0016059821645151022},
        .componentBounds = {Coordinate{0.006, 0.01, 0.006, 0.0},
                            Coordinate{0.006, 0.01, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.01, 0.006, 0.0},
                            Coordinate{-0.006, 0.01, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.01, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.01, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor5negative()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.006,
        .totalHeight        = 0.01,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.0,
        .innerHeight        = 0.008,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.008,
        .maxFlangeHeight    = 0.01,
        .minTotalHeight     = 0.002,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.024,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 4.523893421169302E-7,
        .componentMass      = 6.423928658060409E-4,
        .componentCG        = Coordinate{0.0, -0.0065, 7.960204194457796E-19, 6.423928658060409E-4},
        .componentBounds = {Coordinate{0.006, 0.01, 0.006, 0.0},
                            Coordinate{0.006, 0.01, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.01, 0.006, 0.0},
                            Coordinate{-0.006, 0.01, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.01, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.01, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsCtor5tallFlange()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.006,
        .totalHeight        = 0.01,
        .flangeHeight       = 0.02,
        .baseHeight         = -0.01,
        .innerHeight        = 0.0,
        .screwHeight        = 0.0,
        .maxBaseHeight      = -0.01,
        .maxFlangeHeight    = 0.02,
        .minTotalHeight     = 0.01,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.024,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 1.1309733552923255E-6,
        .componentMass      = 0.0016059821645151022,
        .componentCG        = Coordinate{0.0, -0.005, 6.123233995736766E-19, 0.0016059821645151022},
        .componentBounds = {Coordinate{0.006, 0.01, 0.006, 0.0},
                            Coordinate{0.006, 0.01, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.01, 0.006, 0.0},
                            Coordinate{-0.006, 0.01, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.01, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.01, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsOnBody()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.0097,
        .innerDiameter      = 0.008,
        .totalHeight        = 0.0097,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.002,
        .innerHeight        = 0.0057,
        .screwHeight        = 0.0,
        .maxBaseHeight      = 0.0077,
        .maxFlangeHeight    = 0.0077,
        .minTotalHeight     = 0.004,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.0582,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.15, 0.0, 0.0, 0.0},
        .componentVolume    = 5.821057027836528E-7,
        .componentMass      = 8.265900979527869E-4,
        .componentCG        = Coordinate{0.0, -0.02985, 3.65557069545485E-18, 8.265900979527869E-4},
        .componentBounds    = {Coordinate{0.00485, 0.0097, 0.00485, 0.0},
                               Coordinate{0.00485, 0.0097, -0.00485, 0.0},
                               Coordinate{0.00485, 0.0, 0.00485, 0.0},
                               Coordinate{0.00485, 0.0, -0.00485, 0.0},
                               Coordinate{-0.00485, 0.0, 0.00485, 0.0},
                               Coordinate{-0.00485, 0.0, -0.00485, 0.0},
                               Coordinate{-0.00485, 0.0097, 0.00485, 0.0},
                               Coordinate{-0.00485, 0.0097, -0.00485, 0.0}},
        .boxMin             = Coordinate{-0.00485, -0.0097, -0.00485, 0.0},
        .boxMax             = Coordinate{0.00485, 0.0097, 0.00485, 0.0},
        .instanceOffsets    = {Coordinate{0.0, -0.025, 3.061616997868383E-18, 0.0}},
        .componentLocations = {Coordinate{0.22, -0.025, 3.061616997868383E-18, 0.0}}};
}

[[nodiscard]] Pins pinsSized()
{
    return Pins{.instanceCount      = 1,
                .outerDiameter      = 0.012,
                .innerDiameter      = 0.005,
                .totalHeight        = 0.02,
                .flangeHeight       = 0.003,
                .baseHeight         = 0.004,
                .innerHeight        = 0.013000000000000001,
                .screwHeight        = 0.002,
                .maxBaseHeight      = 0.017,
                .maxFlangeHeight    = 0.016,
                .minTotalHeight     = 0.007,
                .angleOffset        = std::numbers::pi,
                .instanceSeparation = 0.0582,
                .axialOffset        = 0.0,
                .position           = Coordinate{0.15, 0.0, 0.0, 0.0},
                .componentVolume    = 1.197732203675193E-6,
                .componentMass      = 0.001700779729218774,
                .componentCG        = Coordinate{0.0, -0.03585867194681326, 4.3914207821339846E-18,
                                                 0.001700779729218774},
                .componentBounds =
                    {Coordinate{0.006, 0.02, 0.006, 0.0}, Coordinate{0.006, 0.02, -0.006, 0.0},
                     Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                     Coordinate{-0.006, 0.0, 0.006, 0.0}, Coordinate{-0.006, 0.0, -0.006, 0.0},
                     Coordinate{-0.006, 0.02, 0.006, 0.0}, Coordinate{-0.006, 0.02, -0.006, 0.0}},
                .boxMin             = Coordinate{-0.006, -0.022, -0.006, 0.0},
                .boxMax             = Coordinate{0.006, 0.022, 0.006, 0.0},
                .instanceOffsets    = {Coordinate{0.0, -0.025, 3.061616997868383E-18, 0.0}},
                .componentLocations = {Coordinate{0.22, -0.025, 3.061616997868383E-18, 0.0}}};
}

[[nodiscard]] Pins pinsThree()
{
    return Pins{
        .instanceCount      = 3,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.005,
        .totalHeight        = 0.02,
        .flangeHeight       = 0.003,
        .baseHeight         = 0.004,
        .innerHeight        = 0.013000000000000001,
        .screwHeight        = 0.002,
        .maxBaseHeight      = 0.017,
        .maxFlangeHeight    = 0.016,
        .minTotalHeight     = 0.007,
        .angleOffset        = -1.0471975511965976,
        .instanceSeparation = 0.1,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.15, 0.0, 0.0, 0.0},
        .componentVolume    = 3.593196611025579E-6,
        .componentMass      = 0.005102339187656322,
        .componentCG =
            Coordinate{0.1, 0.017929335973406634, -0.031054520851912674, 0.005102339187656322},
        .componentBounds = {Coordinate{0.006, 0.02, 0.006, 0.0},
                            Coordinate{0.006, 0.02, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.02, 0.006, 0.0},
                            Coordinate{-0.006, 0.02, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.022, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.022, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, 0.012500000000000004, -0.021650635094610966, 0.0},
                            Coordinate{0.1, 0.012500000000000004, -0.021650635094610966, 0.0},
                            Coordinate{0.2, 0.012500000000000004, -0.021650635094610966, 0.0}},
        .componentLocations = {Coordinate{0.22, 0.012500000000000004, -0.021650635094610966, 0.0},
                               Coordinate{0.32, 0.012500000000000004, -0.021650635094610966, 0.0},
                               Coordinate{0.42, 0.012500000000000004, -0.021650635094610966, 0.0}}};
}

[[nodiscard]] Pins pinsTop()
{
    return Pins{
        .instanceCount      = 3,
        .outerDiameter      = 0.012,
        .innerDiameter      = 0.005,
        .totalHeight        = 0.02,
        .flangeHeight       = 0.003,
        .baseHeight         = 0.004,
        .innerHeight        = 0.013000000000000001,
        .screwHeight        = 0.002,
        .maxBaseHeight      = 0.017,
        .maxFlangeHeight    = 0.016,
        .minTotalHeight     = 0.007,
        .angleOffset        = -std::numbers::pi,
        .instanceSeparation = 0.1,
        .axialOffset        = 0.05,
        .position           = Coordinate{0.05, 0.0, 0.0, 0.0},
        .componentVolume    = 3.593196611025579E-6,
        .componentMass      = 0.005102339187656322,
        .componentCG =
            Coordinate{0.1, -0.03585867194681326, -4.3914207821339846E-18, 0.005102339187656322},
        .componentBounds = {Coordinate{0.006, 0.02, 0.006, 0.0},
                            Coordinate{0.006, 0.02, -0.006, 0.0},
                            Coordinate{0.006, 0.0, 0.006, 0.0}, Coordinate{0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.0, 0.006, 0.0},
                            Coordinate{-0.006, 0.0, -0.006, 0.0},
                            Coordinate{-0.006, 0.02, 0.006, 0.0},
                            Coordinate{-0.006, 0.02, -0.006, 0.0}},
        .boxMin          = Coordinate{-0.006, -0.022, -0.006, 0.0},
        .boxMax          = Coordinate{0.006, 0.022, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.025, -3.061616997868383E-18, 0.0},
                            Coordinate{0.1, -0.025, -3.061616997868383E-18, 0.0},
                            Coordinate{0.2, -0.025, -3.061616997868383E-18, 0.0}},
        .componentLocations = {Coordinate{0.12000000000000001, -0.025, -3.061616997868383E-18, 0.0},
                               Coordinate{0.22000000000000003, -0.025, -3.061616997868383E-18, 0.0},
                               Coordinate{0.32, -0.025, -3.061616997868383E-18, 0.0}}};
}

[[nodiscard]] Pins pinsPreset()
{
    return Pins{
        .instanceCount      = 1,
        .outerDiameter      = 0.01,
        .innerDiameter      = 0.005,
        .totalHeight        = 0.01,
        .flangeHeight       = 0.002,
        .baseHeight         = 0.002,
        .innerHeight        = 0.006,
        .screwHeight        = 0.003,
        .maxBaseHeight      = 0.008,
        .maxFlangeHeight    = 0.008,
        .minTotalHeight     = 0.004,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.0582,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 5.890486272294242E-7,
        .componentMass      = 0.123,
        .componentCG        = Coordinate{0.0, -0.0066728639151565445, 8.171901434842338E-19, 0.123},
        .componentBounds = {Coordinate{0.005, 0.01, 0.005, 0.0},
                            Coordinate{0.005, 0.01, -0.005, 0.0},
                            Coordinate{0.005, 0.0, 0.005, 0.0}, Coordinate{0.005, 0.0, -0.005, 0.0},
                            Coordinate{-0.005, 0.0, 0.005, 0.0},
                            Coordinate{-0.005, 0.0, -0.005, 0.0},
                            Coordinate{-0.005, 0.01, 0.005, 0.0},
                            Coordinate{-0.005, 0.01, -0.005, 0.0}},
        .boxMin          = Coordinate{-0.005, -0.013000000000000001, -0.005, 0.0},
        .boxMax          = Coordinate{0.005, 0.013000000000000001, 0.005, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

// ================================================================================ detached

TEST(RailButton, Defaults)
{
    const RailButton button;
    EXPECT_EQ(button.kind(), ComponentKind::RAIL_BUTTON);
    EXPECT_EQ(button.getName(), "Rail Button");
    EXPECT_EQ(button.getPresetType(), ComponentPresetType::RAIL_BUTTON);
    EXPECT_EQ(button.getFinish(), Finish::NORMAL);
    EXPECT_FALSE(button.isAfter());
    EXPECT_TRUE(button.isAerodynamic());
    EXPECT_TRUE(button.isMassive());
    EXPECT_EQ(button.getAxialMethod(), AxialMethod::MIDDLE);
    EXPECT_EQ(button.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(button.getAngleOffset(), std::numbers::pi);
    EXPECT_EQ(button.getPatternName(), "1-Line");
    EXPECT_EQ(button.getInstanceAngles(), std::vector<double>{0.0});
    EXPECT_EQ(button.getDisplayOrderSide(), 14);
    EXPECT_EQ(button.getDisplayOrderBack(), 11);
    EXPECT_EQ(differences(pinsDetached(), button), "");
}

TEST(RailButton, IsMadeOfTheBuiltInDelrin)
{
    const RailButton button;
    const Material&  delrin = button.getMaterial();
    EXPECT_EQ(delrin, RailButton::defaultRailButtonMaterial());
    EXPECT_EQ(delrin.getType(), Material::Type::BULK);
    EXPECT_EQ(delrin.getName(), "Delrin");
    EXPECT_EQ(delrin.getDensity(), 1420.0);
    EXPECT_EQ(delrin.getGroup(), QtRocket::MaterialGroup::PLASTICS);
    EXPECT_FALSE(delrin.isUserDefined());
    EXPECT_FALSE(delrin.isDocumentMaterial());

    // The other constructors too.
    EXPECT_EQ(RailButton(0.02, 0.015).getMaterial(), delrin);
    EXPECT_EQ(RailButton(0.012, 0.006, 0.01, 0.002, 0.003).getMaterial(), delrin);
}

TEST(RailButton, ApplyDefaultMaterialEndsWithDelrin)
{
    // Java's constructor sets Delrin after ExternalComponent's has applied the preferences'
    // default, so the preferences never decide a rail button's material.
    QtRocket::MaterialStorage     storage;
    QtRocket::InMemoryPreferences prefs;
    QtRocket::addBuiltinMaterials(storage);
    const Material balsa =
        storage.findMaterial(Material::Type::BULK, "Balsa")
            .value_or(Material::newMaterial(Material::Type::BULK, "<not found>", 0, true));
    ASSERT_EQ(balsa.getName(), "Balsa");
    QtRocket::setDefaultComponentMaterial(prefs, "RailButton", balsa);
    QtRocket::setDefaultComponentMaterial(prefs, "ExternalComponent", balsa);

    RailButton button;
    button.setMaterial(balsa);
    QtRocket::ExternalComponent& external = button;
    external.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(button.getMaterial(), RailButton::defaultRailButtonMaterial());

    // Without a Delrin in the storage (Java: a NullPointerException), the built-in one.
    button.setMaterial(balsa);
    EXPECT_TRUE(storage.removeMaterial(RailButton::defaultRailButtonMaterial()));
    button.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(button.getMaterial(), RailButton::defaultRailButtonMaterial());
}

TEST(RailButton, TwoArgumentConstructor)
{
    // The default button with setOuterDiameter() and setTotalHeight() applied; the instance
    // separation stays six default diameters.
    EXPECT_EQ(differences(pinsCtor2(), RailButton(0.02, 0.015)), "");
    // A small one: the inner diameter follows the outer one down, and the total height cannot go
    // below the base plus the flange.
    EXPECT_EQ(differences(pinsCtor2small(), RailButton(0.005, 0.001)), "");
}

TEST(RailButton, FiveArgumentConstructor)
{
    EXPECT_EQ(differences(pinsCtor5(), RailButton(0.012, 0.006, 0.01, 0.002, 0.003)), "");
    // Only the base height goes through its setter: at most the total less the flange height,
    // at least 0.
    EXPECT_EQ(differences(pinsCtor5clamped(), RailButton(0.012, 0.006, 0.01, 0.002, 0.02)), "");
    EXPECT_EQ(differences(pinsCtor5negative(), RailButton(0.012, 0.006, 0.01, 0.002, -0.02)), "");
    // A flange taller than the button is stored as given and makes the base height negative.
    EXPECT_EQ(differences(pinsCtor5tallFlange(), RailButton(0.012, 0.006, 0.01, 0.02, 0.0)), "");
}

TEST(RailButton, HasTheInterfacesOfJava)
{
    RailButton button;
    EXPECT_NE(dynamic_cast<QtRocket::ExternalComponent*>(&button), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::AnglePositionable*>(&button), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::AxialPositionable*>(&button), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::BoxBounded*>(&button), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::LineInstanceable*>(&button), nullptr);

    // Through the interfaces the overrides answer, not RocketComponent's defaults.
    button.setInstanceCount(2);
    const RocketComponent& component = button;
    EXPECT_EQ(component.getInstanceCount(), 2);
    EXPECT_EQ(component.getAngleOffset(), std::numbers::pi);
    EXPECT_EQ(component.getInstanceOffsets().size(), 2U);
    EXPECT_EQ(component.getInstanceLocations().size(), 2U);
    EXPECT_EQ(component.getInstanceAngles(), (std::vector<double>{0.0, 0.0}));
}

TEST(RailButton, AcceptsNoChildren)
{
    RailButton button;
    EXPECT_FALSE(button.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(button.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(button.addChild(std::make_unique<QtRocket::MassComponent>()), BugError);
}

TEST(RailButton, WithoutAnEventTheInstancesStayOnTheParentsAxis)
{
    // A tree that is not in a Rocket delivers no events, so componentChanged() never stores the
    // body tube's radius; the CG reads the parent directly.
    BodyTube tube(0.3, 0.03, 0.002);
    auto     newButton = std::make_unique<RailButton>();
    newButton->setAxialMethod(AxialMethod::TOP);
    newButton->setAxialOffset(0.1);
    const RailButton& button = tube.addChild(std::move(newButton));

    Differences d;
    d.coordinate("componentCG",
                 Coordinate{0.0, -0.03485, 4.267894095028526E-18, 8.265900979527869E-4},
                 button.getComponentCG());
    EXPECT_EQ(d.text(), "");
    EXPECT_EQ(button.getInstanceOffsets().at(0).y, 0.0);
}

TEST(RailButton, CopyKeepsEveryField)
{
    RailButton original(0.012, 0.006, 0.01, 0.002, 0.003);
    original.setScrewHeight(0.001);
    original.setInstanceCount(2);
    original.setInstanceSeparation(0.1);
    original.setAngleOffset(0.5);
    original.setMaterial(Material::newMaterial(Material::Type::BULK, "x", 1000, true));

    const std::unique_ptr<RailButton> copy =
        QtRocket::componentCast<RailButton>(original.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getOuterDiameter(), 0.012);
    EXPECT_EQ(copy->getInnerDiameter(), 0.006);
    EXPECT_EQ(copy->getTotalHeight(), 0.01);
    EXPECT_EQ(copy->getFlangeHeight(), 0.002);
    EXPECT_EQ(copy->getBaseHeight(), 0.003);
    EXPECT_EQ(copy->getScrewHeight(), 0.001);
    EXPECT_EQ(copy->getInstanceCount(), 2);
    EXPECT_EQ(copy->getInstanceSeparation(), 0.1);
    EXPECT_EQ(copy->getAngleOffset(), 0.5);
    EXPECT_EQ(copy->getMaterial(), original.getMaterial());
    EXPECT_EQ(copy->getComponentVolume(), original.getComponentVolume());
    EXPECT_NE(copy->getId(), original.getId());

    const std::unique_ptr<RocketComponent> sameId = original.copyWithOriginalId();
    EXPECT_EQ(sameId->getId(), original.getId());
    EXPECT_EQ(sameId->kind(), ComponentKind::RAIL_BUTTON);

    original.setInstanceCount(5);
    EXPECT_EQ(copy->getInstanceCount(), 2);
}

// ============================================================================ on a body tube

/// A rocket with one stage holding a nose cone (70 mm) and a body tube (0.3 m long, radius 25 mm,
/// wall 2 mm) with a new rail button on it; the events are enabled and recorded.
class RailButtonOnBody : public ::testing::Test
{
protected:
    RailButtonOnBody()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body   = &m_stage->addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.002));
        m_button = &m_body->addChild(std::make_unique<RailButton>());
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    /// The types of the events fired since the last call.
    [[nodiscard]] Events takeEvents() { return std::exchange(m_types, {}); }

    /// The state the pins "sized" describe: a 12 mm button 20 mm high with a 5 mm core, a 3 mm
    /// flange, a 4 mm base and a 2 mm screw head.
    void makeSized()
    {
        m_button->setOuterDiameter(0.012);
        m_button->setInnerDiameter(0.005);
        m_button->setTotalHeight(0.02);
        m_button->setFlangeHeight(0.003);
        m_button->setBaseHeight(0.004);
        m_button->setScrewHeight(0.002);
        m_types.clear();
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    BodyTube*                               m_body{nullptr};
    RailButton*                             m_button{nullptr};
    Events                                  m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(RailButton, BeforeEventsAreEnabledTheInstancesAreOnTheAxis)
{
    Rocket      rocket;
    auto&       stage  = rocket.addChild(std::make_unique<AxialStage>());
    auto&       body   = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.002));
    const auto& button = body.addChild(std::make_unique<RailButton>());
    EXPECT_EQ(button.getInstanceOffsets().at(0).y, 0.0);

    rocket.enableEvents();
    EXPECT_TRUE(matches(-0.025, button.getInstanceOffsets().at(0).y)) << "the body tube's radius";
}

TEST_F(RailButtonOnBody, SitsOnTheBodyTubesSurface)
{
    EXPECT_EQ(differences(pinsOnBody(), *m_button), "");
}

TEST_F(RailButtonOnBody, DiameterSettersAlwaysFire)
{
    // setOuterDiameter() fires twice: once from the setInnerDiameter() it calls.
    m_button->setOuterDiameter(0.012);
    EXPECT_EQ(takeEvents(), (Events{kBoth, kBoth}));
    m_button->setOuterDiameter(0.012);
    EXPECT_EQ(takeEvents(), (Events{kBoth, kBoth})) << "also for the current value";
    m_button->setOuterDiameter(0.006);
    EXPECT_EQ(takeEvents(), (Events{kBoth, kBoth}));
    EXPECT_EQ(m_button->getInnerDiameter(), 0.006) << "at most the outer diameter";

    m_button->setOuterDiameter(0.012);
    m_button->setInnerDiameter(0.02);
    EXPECT_EQ(m_button->getInnerDiameter(), 0.012);
    static_cast<void>(takeEvents());
    m_button->setInnerDiameter(0.005);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setInnerDiameter(-1);
    EXPECT_EQ(m_button->getInnerDiameter(), -1.0) << "no lower limit, as in OpenRocket";
}

TEST_F(RailButtonOnBody, HeightsConstrainEachOther)
{
    m_button->setOuterDiameter(0.012);
    m_button->setInnerDiameter(0.005);
    static_cast<void>(takeEvents());

    m_button->setTotalHeight(0.02);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setTotalHeight(0.001);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(m_button->getTotalHeight(), 0.004) << "at least the base plus the flange";
    m_button->setTotalHeight(0.02);
    static_cast<void>(takeEvents());

    m_button->setFlangeHeight(0.003);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setFlangeHeight(1);
    EXPECT_EQ(m_button->getFlangeHeight(), 0.018000000000000002)
        << "at most the total less the base";
    EXPECT_EQ(m_button->getFlangeHeight(), m_button->getMaxFlangeHeight());
    m_button->setFlangeHeight(-1);
    EXPECT_EQ(m_button->getFlangeHeight(), 0.0);
    m_button->setFlangeHeight(0.003);
    static_cast<void>(takeEvents());

    m_button->setBaseHeight(0.004);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setBaseHeight(1);
    EXPECT_EQ(m_button->getBaseHeight(), 0.017) << "at most the total less the flange";
    EXPECT_EQ(m_button->getBaseHeight(), m_button->getMaxBaseHeight());
    m_button->setBaseHeight(-1);
    EXPECT_EQ(m_button->getBaseHeight(), 0.0);
    m_button->setBaseHeight(0.004);
    static_cast<void>(takeEvents());

    m_button->setScrewHeight(0.002);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setScrewHeight(-1);
    EXPECT_EQ(m_button->getScrewHeight(), 0.0);
    m_button->setScrewHeight(0.002);

    // The screw head adds to the volume, moves the CG outwards and widens the instance box.
    EXPECT_EQ(differences(pinsSized(), *m_button), "");
}

TEST_F(RailButtonOnBody, InstancesAndAngle)
{
    makeSized();

    m_button->setInstanceSeparation(0.1);
    EXPECT_EQ(takeEvents(), Events{kAerodynamic});
    m_button->setInstanceSeparation(0.1);
    EXPECT_EQ(takeEvents(), Events{});
    m_button->setInstanceCount(3);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    m_button->setInstanceCount(3);
    EXPECT_EQ(takeEvents(), Events{});
    m_button->setInstanceCount(0);
    EXPECT_EQ(takeEvents(), Events{});
    m_button->setInstanceCount(-1);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(m_button->getPatternName(), "3-Line");

    // The angle fires an aerodynamic change only, as in OpenRocket.
    m_button->setAngleOffset(-std::numbers::pi / 3);
    EXPECT_EQ(takeEvents(), Events{kAerodynamic});
    m_button->setAngleOffset(-std::numbers::pi / 3);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(differences(pinsThree(), *m_button), "");

    m_button->setAngleOffset(4);
    EXPECT_EQ(takeEvents(), Events{kAerodynamic});
    EXPECT_EQ(m_button->getAngleOffset(), std::numbers::pi) << "clamped, not reduced";
    m_button->setAngleOffset(-4);
    EXPECT_EQ(takeEvents(), Events{kAerodynamic});
    EXPECT_EQ(m_button->getAngleOffset(), -std::numbers::pi);

    m_button->setAngleMethod(AngleMethod::FIXED);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(m_button->getAngleMethod(), AngleMethod::RELATIVE);
}

TEST_F(RailButtonOnBody, AxialPositionAndMaterial)
{
    makeSized();
    m_button->setInstanceSeparation(0.1);
    m_button->setInstanceCount(3);
    m_button->setAngleOffset(-4);
    static_cast<void>(takeEvents());

    m_button->setAxialMethod(AxialMethod::TOP);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional});
    EXPECT_EQ(m_button->getAxialOffset(), 0.15) << "the same position, measured from the top";
    m_button->setAxialMethod(AxialMethod::TOP);
    EXPECT_EQ(takeEvents(), Events{kNonFunctional}) << "also for the method in force";
    m_button->setAxialOffset(0.05);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(differences(pinsTop(), *m_button), "");

    m_button->setMaterial(Material::newMaterial(Material::Type::BULK, "x", 1000, true));
    EXPECT_EQ(takeEvents(), Events{kMass});
    EXPECT_TRUE(matches(0.003593196611025579, m_button->getComponentMass()));
}

// =================================================================================== presets

/// A RAIL_BUTTON preset: 10 mm high and wide with a 5 mm core, a 2 mm flange and base, and a
/// mass of 123 g.
[[nodiscard]] TypedPropertyMap buttonSpec()
{
    TypedPropertyMap spec;
    spec.put(ComponentPreset::kType, ComponentPresetType::RAIL_BUTTON);
    spec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    spec.put(ComponentPreset::kPartNo, "partno");
    spec.put(ComponentPreset::kHeight, 0.01);
    spec.put(ComponentPreset::kOuterDiameter, 0.01);
    spec.put(ComponentPreset::kInnerDiameter, 0.005);
    spec.put(ComponentPreset::kFlangeHeight, 0.002);
    spec.put(ComponentPreset::kBaseHeight, 0.002);
    spec.put(ComponentPreset::kMass, 0.123);
    return spec;
}

/// buttonSpec() with a screw head, screw and nut masses, a drag coefficient and a material.
[[nodiscard]] TypedPropertyMap fullButtonSpec()
{
    TypedPropertyMap spec = buttonSpec();
    spec.put(ComponentPreset::kScrewHeight, 0.003);
    spec.put(ComponentPreset::kScrewMass, 0.001);
    spec.put(ComponentPreset::kNutMass, 0.002);
    spec.put(ComponentPreset::kCd, 0.7);
    spec.put(ComponentPreset::kMaterial,
             Material::newMaterial(Material::Type::BULK, "test", 2.0, true));
    return spec;
}

[[nodiscard]] ComponentPreset makePreset(const TypedPropertyMap& spec)
{
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(spec, materials).value();
}

TEST(RailButtonPreset, LoadsTheDimensionsAndOverridesTheMassAndTheCD)
{
    const ComponentPreset preset = makePreset(fullButtonSpec());
    RailButton            button;
    button.loadPreset(&preset);

    EXPECT_EQ(button.getPresetComponent(), &preset);
    EXPECT_EQ(differences(pinsPreset(), button), "");
    EXPECT_EQ(button.getMaterial(), preset.get(ComponentPreset::kMaterial));
    EXPECT_EQ(button.getMaterial().getName(), "test");
    EXPECT_TRUE(matches(208811.28367708367, button.getMaterial().getDensity()));
    // The mass override is the button's mass plus the screw's and the nut's.
    EXPECT_TRUE(button.isMassOverridden());
    EXPECT_TRUE(matches(0.126, button.getOverrideMass())) << button.getOverrideMass();
    EXPECT_TRUE(matches(0.126, button.getMass()));
    EXPECT_TRUE(button.isCDOverridden());
    EXPECT_EQ(button.getOverrideCD(), 0.7);
}

TEST(RailButtonPreset, AMassAloneOverridesTheMass)
{
    const ComponentPreset preset = makePreset(buttonSpec());
    RailButton            button;
    button.loadPreset(&preset);

    EXPECT_TRUE(button.isMassOverridden());
    EXPECT_EQ(button.getOverrideMass(), 0.123);
    EXPECT_FALSE(button.isCDOverridden());
    EXPECT_EQ(button.getScrewHeight(), 0.0);
    EXPECT_TRUE(matches(4.319689898685966E-7, button.getComponentVolume()));
    // The factory's density gives the preset's mass back.
    EXPECT_TRUE(matches(0.12300000000000001, button.getComponentMass()));
    EXPECT_EQ(button.getMaterial().getName(), "RailButtonCustom");
}

TEST(RailButtonPreset, WithoutAMassNothingIsOverridden)
{
    TypedPropertyMap spec = buttonSpec();
    ASSERT_TRUE(spec.remove(ComponentPreset::kMass));
    spec.put(ComponentPreset::kCd, 0.0);  // not positive: no CD override
    const ComponentPreset preset = makePreset(spec);

    RailButton button;
    button.loadPreset(&preset);
    EXPECT_FALSE(button.isMassOverridden());
    EXPECT_FALSE(button.isCDOverridden());
    EXPECT_EQ(button.getMaterial(), RailButton::defaultRailButtonMaterial());
    EXPECT_EQ(button.getOuterDiameter(), 0.01);
}

TEST_F(RailButtonOnBody, PresetEvents)
{
    const ComponentPreset preset = makePreset(fullButtonSpec());
    m_button->loadPreset(&preset);
    // loadFromPreset()'s changes combined (as the rocket thaws), then loadPreset()'s.
    EXPECT_EQ(takeEvents(), (Events{kBoth, kNonFunctional}));

    // The preset is cleared before the setter's own change fires.
    m_button->setTotalHeight(0.012);
    EXPECT_EQ(takeEvents(), (Events{kNonFunctional, kBoth}));
    EXPECT_EQ(m_button->getPresetComponent(), nullptr);

    // Where the button sits is not the preset's business.
    m_button->loadPreset(&preset);
    m_button->setInstanceCount(2);
    m_button->setInstanceSeparation(0.2);
    m_button->setAngleOffset(1);
    m_button->setAxialOffset(0.01);
    m_button->setAxialMethod(AxialMethod::BOTTOM);
    EXPECT_EQ(m_button->getPresetComponent(), &preset);
}

/// A setter call, by name.
struct Change
{
    std::string_view name;
    void (*apply)(RailButton&);
};

TEST(RailButtonPreset, EveryDimensionSetterClearsThePreset)
{
    const std::array<Change, 7> changes{{
        {.name = "setOuterDiameter", .apply = [](RailButton& b) { b.setOuterDiameter(0.02); }},
        {.name = "setInnerDiameter", .apply = [](RailButton& b) { b.setInnerDiameter(0.004); }},
        {.name = "setTotalHeight", .apply = [](RailButton& b) { b.setTotalHeight(0.02); }},
        {.name = "setFlangeHeight", .apply = [](RailButton& b) { b.setFlangeHeight(0.001); }},
        {.name = "setBaseHeight", .apply = [](RailButton& b) { b.setBaseHeight(0.001); }},
        {.name = "setScrewHeight", .apply = [](RailButton& b) { b.setScrewHeight(0.001); }},
        {.name = "setMaterial",
         .apply =
             [](RailButton& b) {
                 b.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
             }},
    }};
    const ComponentPreset       preset = makePreset(buttonSpec());
    RailButton                  button;
    for (const Change& change : changes)
    {
        button.loadPreset(&preset);
        change.apply(button);
        EXPECT_EQ(button.getPresetComponent(), nullptr) << change.name;
    }

    // The finish is not the preset's business (ExternalComponent).
    button.loadPreset(&preset);
    button.setFinish(Finish::POLISHED);
    EXPECT_EQ(button.getPresetComponent(), &preset);
}

// ======================================================================================= CG

TEST(RailButtonCG, AButtonWithoutAnyVolumeHasNoCG)
{
    const RailButton zero(0.0, 0.0, 0.01, 0.002, 0.002);
    EXPECT_EQ(zero.getComponentVolume(), 0.0);
    const Coordinate cg = zero.getComponentCG();
    EXPECT_EQ(cg.x, 0.0);
    EXPECT_TRUE(std::isnan(cg.y));
    EXPECT_TRUE(std::isnan(cg.z));
    EXPECT_EQ(cg.weight, 0.0);
}

TEST(RailButtonCG, InconsistentPresetDimensionsAreABug)
{
    // Java throws a BugException when the CG comes out above the button. The setters'
    // constraints rule that out; a preset's values are stored unchecked, and one whose core is
    // wider than the button and whose base and flange are taller than it gets there.
    TypedPropertyMap spec;
    spec.put(ComponentPreset::kType, ComponentPresetType::RAIL_BUTTON);
    spec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    spec.put(ComponentPreset::kPartNo, "inconsistent");
    spec.put(ComponentPreset::kHeight, 0.01);
    spec.put(ComponentPreset::kOuterDiameter, 0.01);
    spec.put(ComponentPreset::kInnerDiameter, 0.021);
    spec.put(ComponentPreset::kFlangeHeight, 0.004);
    spec.put(ComponentPreset::kBaseHeight, 0.009);
    const ComponentPreset preset = makePreset(spec);

    RailButton button;
    button.loadPreset(&preset);
    EXPECT_TRUE(matches(-0.002999999999999999, button.getInnerHeight()));
    EXPECT_TRUE(matches(-1.8064157758141057E-8, button.getComponentVolume()));
    try
    {
        static_cast<void>(button.getComponentCG());
        ADD_FAILURE() << "no BugError";
    }
    catch (const BugError& error)
    {
        const std::string_view what = error.what();
        EXPECT_TRUE(
            what.contains(" bug found while computing the CG of a RailButton: Rail Button\n"
                          " height of CG: 0.1161956521739"))
            << what;
    }
}

}  // namespace
