#include "QtRocket/gui/configdialog/FinTabCalculator.h"

#include <algorithm>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "FinPanelPins.h"
#include "PanelReplay.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using FinTabCalculator::computeFinTabLength;
using FinTabCalculator::TabPlacement;
using Test::javaDouble;
using Test::kFinTabComputePins;

/// A centering ring of the length @p length at @p offset in @p method, set in the order of
/// FinSetConfigTest (the length first).
[[nodiscard]] std::unique_ptr<CenteringRing> lengthFirstRing(double length, AxialMethod method,
                                                             double offset)
{
    auto ring = std::make_unique<CenteringRing>();
    ring->setLength(length);
    ring->setAxialMethod(method);
    ring->setAxialOffset(offset);
    return ring;
}

/// The same, the method set first (FinSetConfigTest sets the method first in some tests).
[[nodiscard]] std::unique_ptr<CenteringRing> methodFirstRing(double length, AxialMethod method,
                                                             double offset)
{
    auto ring = std::make_unique<CenteringRing>();
    ring->setAxialMethod(method);
    ring->setLength(length);
    ring->setAxialOffset(offset);
    return ring;
}

/// Adds @p rings to @p parent, the rings' pointers in that order.
[[nodiscard]] std::vector<const CenteringRing*> addRings(
    RocketComponent& parent, std::vector<std::unique_ptr<CenteringRing>> rings)
{
    std::vector<const CenteringRing*> added;
    added.reserve(rings.size());
    for (std::unique_ptr<CenteringRing>& ring : rings)
    {
        added.push_back(&parent.addChild(std::move(ring)));
    }
    return added;
}

/// Two rings, @p first and @p second, in a vector (unique_ptr has no initializer list).
[[nodiscard]] std::vector<std::unique_ptr<CenteringRing>> twoRings(
    std::unique_ptr<CenteringRing> first, std::unique_ptr<CenteringRing> second)
{
    std::vector<std::unique_ptr<CenteringRing>> rings;
    rings.push_back(std::move(first));
    rings.push_back(std::move(second));
    return rings;
}

/// The probe's line of a computeFinTabLength() result: "result=<length> offset=<offset>".
[[nodiscard]] std::string computeLine(const TabPlacement& placement)
{
    return std::format("result={} offset={}", javaDouble(placement.length),
                       javaDouble(placement.offset));
}

/// The pinned line of the probe's computeFinTabLength() run @p name.
[[nodiscard]] std::string_view computePin(std::string_view name)
{
    const auto* found = std::ranges::find_if(
        kFinTabComputePins, [name](const Test::FinTabComputePin& pin) { return pin.name == name; });
    QTROCKET_ASSERT(found != kFinTabComputePins.end());
    return found->line;
}

// Port of FinSetConfigTest.testComputeFinTabLength (swing
// gui/configdialog/FinSetConfigTest.java): no centering rings.
TEST(FinTabCalculatorTest, ComputeFinTabLength)
{
    // (Java's parent, a BodyTube, is passed as the unused relativeTo.)
    const std::vector<const CenteringRing*> rings;

    TabPlacement result = computeFinTabLength(rings, 10.0, 11.0);
    // Java's assertEquals(0.0001, 11.0d, result) has its arguments out of order (expected 0.0001,
    // actual 11, delta the result), kept as written.
    EXPECT_NEAR(0.0001, 11.0, result.length);
    // Java's null list of rings: no rings.
    result = computeFinTabLength({}, 10.0, 11.0);
    EXPECT_NEAR(11.0, result.length, 0.0001);
}

// Port of FinSetConfigTest.testCompute2LeadingRings: two rings both ahead of the fin.
TEST(FinTabCalculatorTest, Compute2LeadingRings)
{
    BodyTube                                parent;
    const std::vector<const CenteringRing*> rings =
        addRings(parent, twoRings(lengthFirstRing(0.004, AxialMethod::TOP, 0.43),
                                  lengthFirstRing(0.004, AxialMethod::TOP, 0.45)));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.01, result.length, 0.0001);
}

// Port of FinSetConfigTest.testCompute1Ring: one ring, ahead of the fin.
TEST(FinTabCalculatorTest, Compute1Ring)
{
    BodyTube                                    parent;
    std::vector<std::unique_ptr<CenteringRing>> made;
    made.push_back(lengthFirstRing(0.004, AxialMethod::TOP, 0.43));
    const std::vector<const CenteringRing*> rings = addRings(parent, std::move(made));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.01, result.length, 0.0001);
}

// Port of FinSetConfigTest.testComputeOneLeadingOneRingWithinRoot: one ring ahead of the fin, one
// ring within the root chord.
TEST(FinTabCalculatorTest, ComputeOneLeadingOneRingWithinRoot)
{
    BodyTube                                parent(1.0, 0.01);
    const std::vector<const CenteringRing*> rings =
        addRings(parent, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.43),
                                  methodFirstRing(0.004, AxialMethod::TOP, 0.45)));

    const TabPlacement result = computeFinTabLength(rings, 0.45, 0.01);
    EXPECT_NEAR(0.01 - 0.004, result.length, 0.0001);
}

// Port of FinSetConfigTest.testComputeOneLeadingOneTrailingRing: one ring ahead of the fin, one
// ring beyond the root chord; the rings are in no parent.
TEST(FinTabCalculatorTest, ComputeOneLeadingOneTrailingRing)
{
    const std::unique_ptr<CenteringRing>    ring1 = methodFirstRing(0.004, AxialMethod::TOP, 0.43);
    const std::unique_ptr<CenteringRing>    ring2 = methodFirstRing(0.004, AxialMethod::TOP, 0.48);
    const std::vector<const CenteringRing*> rings{ring1.get(), ring2.get()};
    // (Java makes a BodyTube it does not use and passes ring1 as the unused relativeTo.)

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.01, result.length, 0.0001);
}

// Port of FinSetConfigTest.testComputeOneWithinRootOneTrailingRing: one ring within the root
// chord, another ring beyond the root chord.
TEST(FinTabCalculatorTest, ComputeOneWithinRootOneTrailingRing)
{
    BodyTube                                parent(1.0, 0.1);
    const std::vector<const CenteringRing*> rings =
        addRings(parent, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.4701),
                                  lengthFirstRing(0.004, AxialMethod::TOP, 0.48)));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.0059, result.length, 0.0001);
}

// Port of FinSetConfigTest.testBothRingsWithinRootChord: both rings within the root chord.
TEST(FinTabCalculatorTest, BothRingsWithinRootChord)
{
    BodyTube                                parent(1.0000, 0.1);
    const std::vector<const CenteringRing*> rings =
        addRings(parent, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.4701),
                                  lengthFirstRing(0.004, AxialMethod::TOP, 0.4750)));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.0009, result.length, 0.0002);
}

// Port of FinSetConfigTest.testBothRingsBeyondRootChord: both rings beyond the root chord.
TEST(FinTabCalculatorTest, BothRingsBeyondRootChord)
{
    BodyTube                                parent(1.0, 0.1);
    const std::vector<const CenteringRing*> rings =
        addRings(parent, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.48),
                                  methodFirstRing(0.004, AxialMethod::TOP, 0.49)));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.0, result.length, 0.0001);
}

// Port of FinSetConfigTest.test3RingsWithinRootChord: both rings within the root chord, the top
// ring with an adjacent ring (so 3 rings in all).
TEST(FinTabCalculatorTest, ThreeRingsWithinRootChord)
{
    BodyTube parent(1.0, 0.1);
    parent.setAxialOffset(0);
    std::vector<std::unique_ptr<CenteringRing>> made =
        twoRings(methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.47),
                 methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.4702));
    made.push_back(methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.4770));
    const std::vector<const CenteringRing*> rings = addRings(parent, std::move(made));

    const TabPlacement result = computeFinTabLength(rings, 0.47, 0.01);
    EXPECT_NEAR(0.0028, result.length, 0.0001);
}

TEST(FinTabCalculatorTest, TheLengthsAndOffsetsAreOpenRocketsExactly)
{
    // FinSetConfigTest's inputs as above, with the tab offset each gives its model, as
    // FinPanelProbe printed them (computeFinTabLength() is + - * only, so the doubles are Java's
    // exactly).
    BodyTube                                plainParent;
    const std::vector<const CenteringRing*> leading =
        addRings(plainParent, twoRings(lengthFirstRing(0.004, AxialMethod::TOP, 0.43),
                                       lengthFirstRing(0.004, AxialMethod::TOP, 0.45)));
    EXPECT_EQ(computeLine(computeFinTabLength({}, 10.0, 11.0)),
              computePin("testComputeFinTabLength empty"));
    EXPECT_EQ(computeLine(computeFinTabLength(leading, 0.47, 0.01)),
              computePin("testCompute2LeadingRings"));

    BodyTube                                thinParent(1.0, 0.01);
    const std::vector<const CenteringRing*> leadingAndWithin =
        addRings(thinParent, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.43),
                                      methodFirstRing(0.004, AxialMethod::TOP, 0.45)));
    EXPECT_EQ(computeLine(computeFinTabLength(leadingAndWithin, 0.45, 0.01)),
              computePin("testComputeOneLeadingOneRingWithinRoot"));

    BodyTube                                within(1.0, 0.1);
    const std::vector<const CenteringRing*> withinAndTrailing =
        addRings(within, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.4701),
                                  lengthFirstRing(0.004, AxialMethod::TOP, 0.48)));
    EXPECT_EQ(computeLine(computeFinTabLength(withinAndTrailing, 0.47, 0.01)),
              computePin("testComputeOneWithinRootOneTrailingRing"));

    BodyTube                                both(1.0, 0.1);
    const std::vector<const CenteringRing*> bothWithin =
        addRings(both, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.4701),
                                lengthFirstRing(0.004, AxialMethod::TOP, 0.4750)));
    EXPECT_EQ(computeLine(computeFinTabLength(bothWithin, 0.47, 0.01)),
              computePin("testBothRingsWithinRootChord"));

    BodyTube                                beyond(1.0, 0.1);
    const std::vector<const CenteringRing*> bothBeyond =
        addRings(beyond, twoRings(methodFirstRing(0.004, AxialMethod::TOP, 0.48),
                                  methodFirstRing(0.004, AxialMethod::TOP, 0.49)));
    EXPECT_EQ(computeLine(computeFinTabLength(bothBeyond, 0.47, 0.01)),
              computePin("testBothRingsBeyondRootChord"));
}

TEST(FinTabCalculatorTest, AdjacentRingsMergeAsOpenRocket)
{
    BodyTube three(1.0, 0.1);
    three.setAxialOffset(0);
    std::vector<std::unique_ptr<CenteringRing>> made =
        twoRings(methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.47),
                 methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.4702));
    made.push_back(methodFirstRing(0.004, AxialMethod::ABSOLUTE, 0.4770));
    const std::vector<const CenteringRing*> rings = addRings(three, std::move(made));
    EXPECT_EQ(computeLine(computeFinTabLength(rings, 0.47, 0.01)),
              computePin("test3RingsWithinRootChord"));
}

TEST(FinTabCalculatorTest, RingsWithinAMillimetreSortAsOpenRocket)
{
    // OpenRocket's comparator calls rings less than 1 mm apart equal, an order that is not
    // transitive; the rings are sorted as Java's List.sort() sorts them (FinPanelProbe's "unsorted
    // rings": six rings 0.6 mm long within 7 mm, given out of order).
    BodyTube                                    parent(1.0, 0.1);
    std::vector<std::unique_ptr<CenteringRing>> made;
    for (const double top : {0.4705, 0.4740, 0.4712, 0.4699, 0.4768, 0.4751})
    {
        made.push_back(lengthFirstRing(0.0006, AxialMethod::TOP, top));
    }
    const std::vector<const CenteringRing*> rings = addRings(parent, std::move(made));
    EXPECT_EQ(computeLine(computeFinTabLength(rings, 0.47, 0.01)), computePin("unsorted rings"));
}

/// The probe's computeTops(): computeFinTabLength() of rings @p length long at @p tops, in that
/// order, in a body tube 1 m long, for a fin at 47 cm, 1 cm long; the probe's line.
[[nodiscard]] std::string computeTops(std::initializer_list<double> tops, double length)
{
    BodyTube                                    parent(1.0, 0.1);
    std::vector<std::unique_ptr<CenteringRing>> made;
    for (const double top : tops)
    {
        made.push_back(lengthFirstRing(length, AxialMethod::TOP, top));
    }
    const std::vector<const CenteringRing*> rings = addRings(parent, std::move(made));
    return computeLine(computeFinTabLength(rings, 0.47, 0.01));
}

TEST(FinTabCalculatorTest, RingsComparedByWholeMillimetresKeepJavasOrder)
{
    // Two rings 0.5 mm apart compare equal, so they stay in the order given: the higher one second
    // overlaps the first one's position and merges with it (one ring from 47 cm to 47.07 cm), the
    // other way round they stay two rings; a strictly descending run is reversed, and a mix goes
    // through Java's binary insertion sort.
    EXPECT_EQ(computeTops({0.4705, 0.4700}, 0.0002), computePin("rings within a millimetre"));
    EXPECT_EQ(computeTops({0.4700, 0.4705}, 0.0002),
              computePin("rings within a millimetre reversed"));
    EXPECT_EQ(computeTops({0.4760, 0.4740, 0.4720, 0.4705}, 0.0002),
              computePin("descending rings"));
    EXPECT_EQ(computeTops({0.4712, 0.4705, 0.4718, 0.4699, 0.4790, 0.4771}, 0.0002),
              computePin("mixed rings"));
}

/// Rocket > Stage > body tube (30 cm, radius 2.1 cm) > @p children, then the fin set: the
/// probes' onBodyTube().
struct TabDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    TrapezoidFinSet*                    fins{nullptr};
};

[[nodiscard]] TabDesign tabDesign(double mountLength, double mountRadius, double mountOffset)
{
    TabDesign design;
    design.document = Test::onBodyTube([&](BodyTube& body) {
        auto tube = std::make_unique<InnerTube>();
        tube->setOuterRadius(mountRadius);
        tube->setLength(mountLength);
        tube->setAxialMethod(AxialMethod::BOTTOM);
        tube->setAxialOffset(mountOffset);
        body.addChild(std::move(tube));
        body.addChild(lengthFirstRing(0.005, AxialMethod::TOP, 0.21));
        body.addChild(lengthFirstRing(0.005, AxialMethod::TOP, 0.27));
        auto fins = std::make_unique<TrapezoidFinSet>();
        fins->setRootChord(0.08);
        design.fins = &body.addChild(std::move(fins));
    });
    return design;
}

TEST(FinTabCalculatorTest, TheSupportsAreTheTubesUnderTheFinAndTheRings)
{
    // A motor mount under the fin (22 ... 30 cm) and two rings larger than it.
    const TabDesign                     design   = tabDesign(0.1, 0.0125, 0.0);
    const FinTabCalculator::TabSupports supports = FinTabCalculator::findTabSupports(*design.fins);
    EXPECT_EQ(supports.maxTubeRadius, 0.0125);
    EXPECT_EQ(
        supports.maxRingRadius,
        dynamic_cast<const CenteringRing&>(design.fins->getParent()->getChild(1)).getOuterRadius());
    EXPECT_EQ(supports.rings.size(), 2U);
    EXPECT_TRUE(supports.ringsPlaceTheTab());

    // A mount ahead of the fin (10 ... 15 cm) is not under it.
    const TabDesign ahead = tabDesign(0.05, 0.0125, -0.15);
    EXPECT_FALSE(FinTabCalculator::isComponentInsideFinSpan(*ahead.fins,
                                                            ahead.fins->getParent()->getChild(0)));
    EXPECT_EQ(FinTabCalculator::findTabSupports(*ahead.fins).maxTubeRadius, 0.0);
    EXPECT_TRUE(FinTabCalculator::isComponentInsideFinSpan(*design.fins,
                                                           design.fins->getParent()->getChild(0)));

    // Rings no larger than the mount are left out; without them the rings place no tab.
    const TabDesign                     wide = tabDesign(0.1, 0.03, 0.0);
    const FinTabCalculator::TabSupports wideSupports =
        FinTabCalculator::findTabSupports(*wide.fins);
    EXPECT_TRUE(wideSupports.rings.empty());
    EXPECT_FALSE(wideSupports.ringsPlaceTheTab());
}

TEST(FinTabCalculatorTest, TheTabHeightReachesTheTubeOrNothing)
{
    const TabDesign design = tabDesign(0.1, 0.0125, 0.0);
    // The body's radius at the tab, less the mount's radius.
    EXPECT_EQ(FinTabCalculator::tabHeight(*design.fins, 0.0125),
              std::optional<double>(0.021 - 0.0125));
    EXPECT_EQ(FinTabCalculator::tabHeight(*design.fins, 0.0), std::optional<double>(0.021));
    // A tube wider than the body gives no height (Java: height >= 0).
    EXPECT_EQ(FinTabCalculator::tabHeight(*design.fins, 0.03), std::nullopt);
    // A fin set without a parent has none.
    const TrapezoidFinSet detached;
    EXPECT_EQ(FinTabCalculator::tabHeight(detached, 0.0), std::nullopt);
    EXPECT_TRUE(FinTabCalculator::findTabSupports(detached).rings.empty());
}

}  // namespace

}  // namespace QtRocket::Gui
