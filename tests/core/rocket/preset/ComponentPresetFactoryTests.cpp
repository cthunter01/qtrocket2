#include "QtRocket/rocket/preset/ComponentPresetFactory.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::AnyTypedKey;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using InvalidPreset = ComponentPresetFactory::InvalidPreset;
using Outcome       = std::expected<ComponentPreset, InvalidPreset>;

/// The factory against an empty material storage (the Java tests' Databases hold no material
/// they would find).
class ComponentPresetFactoryTest : public ::testing::Test
{
protected:
    [[nodiscard]] Outcome create(const TypedPropertyMap& props) const
    {
        return ComponentPresetFactory::tryCreate(props, m_materials);
    }

    QtRocket::MaterialStorage m_materials;
};

/// PresetAssertHelper.assertInvalidPresetException: @p outcome is a failure whose invalid
/// parameters are exactly @p keys (in any order) and whose errors are as many as @p messages,
/// each message contained in one of them.
void expectInvalidKeys(const InvalidPreset& problems, std::initializer_list<AnyTypedKey> keys)
{
    EXPECT_EQ(problems.invalidParameters.size(), keys.size());
    for (const AnyTypedKey& expectedKey : keys)
    {
        EXPECT_NE(std::ranges::find(problems.invalidParameters, expectedKey),
                  problems.invalidParameters.end())
            << "Expected key " << expectedKey.toString() << " not in exception";
    }
}

/// True when one of @p errors contains @p expectedMessage.
[[nodiscard]] bool reported(const std::vector<std::string>& errors,
                            std::string_view                expectedMessage)
{
    return std::ranges::any_of(errors, [expectedMessage](const std::string& error) {
        return error.contains(expectedMessage);
    });
}

void expectErrors(const InvalidPreset& problems, std::initializer_list<std::string_view> messages)
{
    EXPECT_EQ(problems.errors.size(), messages.size());
    for (const std::string_view expectedMessage : messages)
    {
        EXPECT_TRUE(reported(problems.errors, expectedMessage))
            << "Expected string \"" << expectedMessage << "\" not reported in errors";
    }
}

void expectInvalidPreset(const Outcome& outcome, std::initializer_list<AnyTypedKey> keys,
                         std::initializer_list<std::string_view> messages)
{
    ASSERT_FALSE(outcome.has_value()) << "the preset was accepted";
    expectInvalidKeys(outcome.error(), keys);
    expectErrors(outcome.error(), messages);
}

/// A spec of @p type, with a manufacturer and a part number when asked for.
[[nodiscard]] TypedPropertyMap spec(ComponentPresetType type, bool withManufacturer = true,
                                    bool withPartNo = true)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, type);
    if (withManufacturer)
    {
        props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    }
    if (withPartNo)
    {
        props.put(ComponentPreset::kPartNo, "partno");
    }
    return props;
}

[[nodiscard]] Material testMaterial()
{
    return Material::newMaterial(Material::Type::BULK, "test", 2.0, true);
}

// Deferred until the concrete components are ported: the ten preset component tests
// (BodyTubeComponentTests, BulkHeadComponentTests, CenteringRingComponentTests,
// EngineBlockComponentTests, LaunchLugComponentTests, NoseConeComponentTests,
// ParachuterComponentTests, StreamerComponentTests, TransitionComponentTests and
// TubeCouplerComponentTests), which load presets into components, and the mass tests of
// NoseConePresetTests (see ShapedPresetWithAMassTest).

// ================================================================================ tubes
// BodyTubePresetTests, TubeCouplerPresetTests, LaunchLugPresetTests, CenteringRingPresetTests
// and EngineBlockPresetTests are the same tests for five types.

class TubePresetTest : public ComponentPresetFactoryTest,
                       public ::testing::WithParamInterface<ComponentPresetType>
{ };

/// The material name a tube preset's mass gives without a material.
[[nodiscard]] std::string_view customMaterialName(ComponentPresetType type)
{
    switch (type)
    {
        case ComponentPresetType::CENTERING_RING:
            return "CenteringRingCustom";
        case ComponentPresetType::ENGINE_BLOCK:
            return "EngineBlockCustom";
        default:
            return "TubeCustom";
    }
}

TEST_P(TubePresetTest, ManufacturerRequired)
{
    expectInvalidPreset(
        create(spec(GetParam(), false, false)),
        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo, ComponentPreset::kLength},
        {"No Manufacturer specified", "No PartNo specified", "No Length specified",
         "Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, PartNoRequired)
{
    expectInvalidPreset(
        create(spec(GetParam(), true, false)), {ComponentPreset::kPartNo, ComponentPreset::kLength},
        {"No PartNo specified", "No Length specified", "Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, LengthRequired)
{
    expectInvalidPreset(create(spec(GetParam())), {ComponentPreset::kLength},
                        {"No Length specified", "Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, OnlyOuterDiameter)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    expectInvalidPreset(create(props), {}, {"Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, OnlyInnerDiameter)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 2.0);
    expectInvalidPreset(create(props), {}, {"Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, OnlyThicknessDiameter)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kThickness, 2.0);
    expectInvalidPreset(create(props), {}, {"Preset dimensions underspecified"});
}

TEST_P(TubePresetTest, ComputeInnerDiameter)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kThickness, 0.5);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(1.0, preset->get(ComponentPreset::kInnerDiameter));
}

TEST_P(TubePresetTest, ComputeOuterDiameter)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kThickness, 0.5);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(2.0, preset->get(ComponentPreset::kOuterDiameter));
}

TEST_P(TubePresetTest, ComputeThickness)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(0.5, preset->get(ComponentPreset::kThickness));
}

TEST_P(TubePresetTest, ComputeThicknessLooses)
{
    // If all OUTER_DIAMETER, INNER_DIAMETER and THICKNESS are specified, THICKNESS is recomputed.
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kThickness, 15.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(0.5, preset->get(ComponentPreset::kThickness));
}

TEST_P(TubePresetTest, Material)
{
    // Not in BodyTubePresetTests, where it is commented out; the other four have it.
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "test");
    EXPECT_NEAR(2.0, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

TEST_P(TubePresetTest, ComputeDensityNoMaterial)
{
    // The commented-out testComputeDensityNoMaterial of the Java tests (Databases.findMaterial
    // could not run under Ant there).
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kMass, 100.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());

    // Compute the volume by hand here using a slightly different formula from the real
    // implementation.
    double volume = (std::numbers::pi * 1.0) - (std::numbers::pi * .25);  // outer - inner area
    volume *= 2.0;                                                        // times length
    const double density = 100.0 / volume;

    const Material& material = preset->get(ComponentPreset::kMaterial);
    EXPECT_EQ(material.getName(), customMaterialName(GetParam()));
    EXPECT_EQ(material.getType(), Material::Type::BULK);
    EXPECT_NEAR(density, material.getDensity(), 0.0005);
    // A material the storage does not know: a new user-defined document material.
    EXPECT_TRUE(material.isUserDefined());
    EXPECT_TRUE(material.isDocumentMaterial());
}

TEST_P(TubePresetTest, ComputeDensityWithMaterial)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kMass, 100.0);
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());

    double volume = (std::numbers::pi * 1.0) - (std::numbers::pi * .25);
    volume *= 2.0;
    const double density = 100.0 / volume;
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "test");
    EXPECT_NEAR(density, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

TEST_P(TubePresetTest, LengthMissingWithDiametersReportsTheLength)
{
    // Deviation: OpenRocket's computeVolumeOfTube throws a BugException from get(LENGTH).
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    expectInvalidPreset(create(props), {ComponentPreset::kLength}, {"No Length specified"});
}

TEST_P(TubePresetTest, DerivedDiametersAreAppendedToTheProperties)
{
    TypedPropertyMap props = spec(GetParam());
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kThickness, 0.25);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    const std::vector<AnyTypedKey> keys = preset->getProperties().keySet();
    ASSERT_EQ(keys.size(), 7U);
    EXPECT_EQ(keys[4], ComponentPreset::kThickness);
    EXPECT_EQ(keys[5], ComponentPreset::kOuterDiameter);
    EXPECT_EQ(keys[6], ComponentPreset::kInnerDiameter);
    EXPECT_EQ(preset->get(ComponentPreset::kInnerDiameter), 1.5);
}

INSTANTIATE_TEST_SUITE_P(ComponentPresetFactory, TubePresetTest,
                         ::testing::Values(ComponentPresetType::BODY_TUBE,
                                           ComponentPresetType::TUBE_COUPLER,
                                           ComponentPresetType::LAUNCH_LUG,
                                           ComponentPresetType::CENTERING_RING,
                                           ComponentPresetType::ENGINE_BLOCK),
                         [](const ::testing::TestParamInfo<ComponentPresetType>& paramInfo) {
                             return std::string(QtRocket::componentPresetTypeName(paramInfo.param));
                         });

// ============================================================================ BulkHeadPresetTests

TEST_F(ComponentPresetFactoryTest, BulkHeadManufacturerRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::BULK_HEAD, false, false)),
                        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo,
                         ComponentPreset::kLength, ComponentPreset::kOuterDiameter},
                        {"No Manufacturer specified", "No PartNo specified", "No Length specified",
                         "No OuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, BulkHeadPartNoRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::BULK_HEAD, true, false)),
        {ComponentPreset::kPartNo, ComponentPreset::kLength, ComponentPreset::kOuterDiameter},
        {"No PartNo specified", "No Length specified", "No OuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, BulkHeadLengthRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::BULK_HEAD)),
                        {ComponentPreset::kLength, ComponentPreset::kOuterDiameter},
                        {"No Length specified", "No OuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, BulkHeadOuterDiameterRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    expectInvalidPreset(create(props), {ComponentPreset::kOuterDiameter},
                        {"No OuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, BulkHeadMaterial)
{
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "test");
    EXPECT_NEAR(2.0, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
    // A bulkhead's diameters are not completed.
    EXPECT_FALSE(preset->has(ComponentPreset::kInnerDiameter));
    EXPECT_FALSE(preset->has(ComponentPreset::kThickness));
}

TEST_F(ComponentPresetFactoryTest, BulkHeadComputeDensityNoMaterial)
{
    // BulkHeadPresetTests.testComputeDensityNoMaterial, commented out there.
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kMass, 100.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    const double density = 100.0 / (std::numbers::pi * 1.0 * 2.0);
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "BulkHeadCustom");
    EXPECT_NEAR(density, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

TEST_F(ComponentPresetFactoryTest, BulkHeadComputeDensityWithMaterial)
{
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kMass, 100.0);
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    const double density = 100.0 / (std::numbers::pi * 1.0 * 2.0);
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "test");
    EXPECT_NEAR(density, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

TEST_F(ComponentPresetFactoryTest, BulkHeadInnerDiameterCountsInTheVolume)
{
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kInnerDiameter, 1.0);
    props.put(ComponentPreset::kMass, 100.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    // Exactly Java's computeVolumeOfTube: PI * (or * or - ir * ir) * l.
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getDensity(),
              100.0 / (std::numbers::pi * ((1.0 * 1.0) - (0.5 * 0.5)) * 2.0));
}

TEST_F(ComponentPresetFactoryTest, BulkHeadMassWithoutDiameterReportsIt)
{
    // Deviation: OpenRocket's computeVolumeOfTube throws a BugException from
    // get(OUTER_DIAMETER).
    TypedPropertyMap props = spec(ComponentPresetType::BULK_HEAD);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kMass, 100.0);
    expectInvalidPreset(create(props), {ComponentPreset::kOuterDiameter},
                        {"No OuterDiameter specified"});
}

// ============================================================================ NoseConePresetTests

TEST_F(ComponentPresetFactoryTest, NoseConeManufacturerRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::NOSE_CONE, false, false)),
        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo, ComponentPreset::kLength,
         ComponentPreset::kAftOuterDiameter, ComponentPreset::kShape},
        {"No Manufacturer specified", "No PartNo specified", "No Length specified",
         "No AftOuterDiameter specified", "No Shape specified"});
}

TEST_F(ComponentPresetFactoryTest, NoseConePartNoRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::NOSE_CONE, true, false)),
                        {ComponentPreset::kPartNo, ComponentPreset::kLength,
                         ComponentPreset::kAftOuterDiameter, ComponentPreset::kShape},
                        {"No PartNo specified", "No Length specified",
                         "No AftOuterDiameter specified", "No Shape specified"});
}

TEST_F(ComponentPresetFactoryTest, NoseConeLengthRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::NOSE_CONE)),
        {ComponentPreset::kLength, ComponentPreset::kAftOuterDiameter, ComponentPreset::kShape},
        {"No Length specified", "No AftOuterDiameter specified", "No Shape specified"});
}

TEST_F(ComponentPresetFactoryTest, NoseConeShapeRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::NOSE_CONE);
    props.put(ComponentPreset::kLength, 2.0);
    expectInvalidPreset(create(props),
                        {ComponentPreset::kAftOuterDiameter, ComponentPreset::kShape},
                        {"No AftOuterDiameter specified", "No Shape specified"});
}

TEST_F(ComponentPresetFactoryTest, NoseConeAftOuterDiameterRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::NOSE_CONE);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kShape, TransitionShape::CONICAL);
    expectInvalidPreset(create(props), {ComponentPreset::kAftOuterDiameter},
                        {"No AftOuterDiameter specified"});
}

/// A complete NOSE_CONE spec: conical, length 2, aft diameter 2.
[[nodiscard]] TypedPropertyMap noseConeSpec()
{
    TypedPropertyMap props = spec(ComponentPresetType::NOSE_CONE);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kShape, TransitionShape::CONICAL);
    props.put(ComponentPreset::kAftOuterDiameter, 2.0);
    return props;
}

TEST_F(ComponentPresetFactoryTest, NoseConeFilledParameterExplicitlyTrue)
{
    TypedPropertyMap props = noseConeSpec();
    props.put(ComponentPreset::kFilled, true);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_TRUE(preset->get(ComponentPreset::kFilled))
        << "Preset should be filled when explicitly set to true";
}

TEST_F(ComponentPresetFactoryTest, NoseConeFilledParameterExplicitlyFalse)
{
    TypedPropertyMap props = noseConeSpec();
    props.put(ComponentPreset::kFilled, false);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_FALSE(preset->get(ComponentPreset::kFilled))
        << "Preset should not be filled when explicitly set to false";
}

TEST_F(ComponentPresetFactoryTest, NoseConeFilledParameterWithThickness)
{
    TypedPropertyMap props = noseConeSpec();
    props.put(ComponentPreset::kThickness, 0.002);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_FALSE(preset->has(ComponentPreset::kFilled))
        << "Preset with thickness should not be filled";
    EXPECT_NEAR(0.002, preset->get(ComponentPreset::kThickness), 0.0001);
}

TEST_F(ComponentPresetFactoryTest, NoseConeMaterial)
{
    TypedPropertyMap props = noseConeSpec();
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_NEAR(2.0, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

// =========================================================================== TransitionPresetTests

TEST_F(ComponentPresetFactoryTest, TransitionManufacturerRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::TRANSITION, false, false)),
        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo, ComponentPreset::kLength,
         ComponentPreset::kAftOuterDiameter, ComponentPreset::kForeOuterDiameter},
        {"No Manufacturer specified", "No PartNo specified", "No Length specified",
         "No AftOuterDiameter specified", "No ForeOuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, TransitionPartNoRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::TRANSITION, true, false)),
                        {ComponentPreset::kPartNo, ComponentPreset::kLength,
                         ComponentPreset::kAftOuterDiameter, ComponentPreset::kForeOuterDiameter},
                        {"No PartNo specified", "No Length specified",
                         "No AftOuterDiameter specified", "No ForeOuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, TransitionLengthRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::TRANSITION)),
        {ComponentPreset::kLength, ComponentPreset::kAftOuterDiameter,
         ComponentPreset::kForeOuterDiameter},
        {"No Length specified", "No AftOuterDiameter specified", "No ForeOuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, TransitionAftOuterDiameterRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::TRANSITION);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kShape, TransitionShape::CONICAL);
    expectInvalidPreset(create(props),
                        {ComponentPreset::kAftOuterDiameter, ComponentPreset::kForeOuterDiameter},
                        {"No AftOuterDiameter specified", "No ForeOuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, TransitionForeOuterDiameterRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::TRANSITION);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kShape, TransitionShape::CONICAL);
    props.put(ComponentPreset::kAftOuterDiameter, 2.0);
    expectInvalidPreset(create(props), {ComponentPreset::kForeOuterDiameter},
                        {"No ForeOuterDiameter specified"});
}

TEST_F(ComponentPresetFactoryTest, TransitionMaterial)
{
    TypedPropertyMap props = spec(ComponentPresetType::TRANSITION);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kShape, TransitionShape::CONICAL);
    props.put(ComponentPreset::kAftOuterDiameter, 2.0);
    props.put(ComponentPreset::kForeOuterDiameter, 1.0);
    props.put(ComponentPreset::kFilled, true);
    props.put(ComponentPreset::kMaterial, testMaterial());
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(preset->get(ComponentPreset::kMaterial).getName(), "test");
    EXPECT_NEAR(2.0, preset->get(ComponentPreset::kMaterial).getDensity(), 0.0005);
}

TEST_F(ComponentPresetFactoryTest, TransitionShapeIsOptional)
{
    // makeTransition does not require a shape (makeNoseCone does).
    TypedPropertyMap props = spec(ComponentPresetType::TRANSITION);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kAftOuterDiameter, 2.0);
    props.put(ComponentPreset::kForeOuterDiameter, 1.0);
    EXPECT_TRUE(create(props).has_value());
}

// =========================================================================== ParachutePresetTests

TEST_F(ComponentPresetFactoryTest, ParachuteManufacturerRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::PARACHUTE, false, false)),
        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo, ComponentPreset::kDiameter,
         ComponentPreset::kLineCount, ComponentPreset::kLineLength},
        {"No Manufacturer specified", "No PartNo specified", "No Diameter specified",
         "No LineCount specified", "No LineLength specified"});
}

TEST_F(ComponentPresetFactoryTest, ParachutePartNoRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::PARACHUTE, true, false)),
                        {ComponentPreset::kPartNo, ComponentPreset::kDiameter,
                         ComponentPreset::kLineCount, ComponentPreset::kLineLength},
                        {"No PartNo specified", "No Diameter specified", "No LineCount specified",
                         "No LineLength specified"});
}

TEST_F(ComponentPresetFactoryTest, ParachuteDiameterRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::PARACHUTE)),
        {ComponentPreset::kDiameter, ComponentPreset::kLineCount, ComponentPreset::kLineLength},
        {"No Diameter specified", "No LineCount specified", "No LineLength specified"});
}

TEST_F(ComponentPresetFactoryTest, ParachuteLineCountRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, 2.0);
    expectInvalidPreset(create(props), {ComponentPreset::kLineCount, ComponentPreset::kLineLength},
                        {"No LineCount specified", "No LineLength specified"});
}

TEST_F(ComponentPresetFactoryTest, ParachuteLineLengthRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, 2.0);
    props.put(ComponentPreset::kLineCount, 6);
    expectInvalidPreset(create(props), {ComponentPreset::kLineLength}, {"No LineLength specified"});
}

TEST_F(ComponentPresetFactoryTest, ParachuteMassIsKept)
{
    // Recovery devices keep their mass; no material is derived from it.
    TypedPropertyMap props = spec(ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, 2.0);
    props.put(ComponentPreset::kLineCount, 6);
    props.put(ComponentPreset::kLineLength, 1.5);
    props.put(ComponentPreset::kMass, 0.05);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(preset->get(ComponentPreset::kMass), 0.05);
    EXPECT_FALSE(preset->has(ComponentPreset::kMaterial));
}

// ============================================================================ StreamerPresetTests

TEST_F(ComponentPresetFactoryTest, StreamerManufacturerRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::STREAMER, false, false)),
                        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo,
                         ComponentPreset::kLength, ComponentPreset::kWidth},
                        {"No Manufacturer specified", "No PartNo specified", "No Length specified",
                         "No Width specified"});
}

TEST_F(ComponentPresetFactoryTest, StreamerPartNoRequired)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::STREAMER, true, false)),
        {ComponentPreset::kPartNo, ComponentPreset::kLength, ComponentPreset::kWidth},
        {"No PartNo specified", "No Length specified", "No Width specified"});
}

TEST_F(ComponentPresetFactoryTest, StreamerLengthRequired)
{
    expectInvalidPreset(create(spec(ComponentPresetType::STREAMER)),
                        {ComponentPreset::kLength, ComponentPreset::kWidth},
                        {"No Length specified", "No Width specified"});
}

TEST_F(ComponentPresetFactoryTest, StreamerWidthRequired)
{
    TypedPropertyMap props = spec(ComponentPresetType::STREAMER);
    props.put(ComponentPreset::kLength, 2.0);
    expectInvalidPreset(create(props), {ComponentPreset::kWidth}, {"No Width specified"});
}

// ============================================================================== rail button

TEST_F(ComponentPresetFactoryTest, RailButtonRequiredFields)
{
    expectInvalidPreset(
        create(spec(ComponentPresetType::RAIL_BUTTON)),
        {ComponentPreset::kHeight, ComponentPreset::kOuterDiameter, ComponentPreset::kInnerDiameter,
         ComponentPreset::kFlangeHeight, ComponentPreset::kBaseHeight},
        {"No Height specified", "No OuterDiameter specified", "No InnerDiameter specified",
         "No FlangeHeight specified", "No BaseHeight specified"});
    TypedPropertyMap props = spec(ComponentPresetType::RAIL_BUTTON);
    props.put(ComponentPreset::kHeight, 0.01);
    props.put(ComponentPreset::kOuterDiameter, 0.01);
    props.put(ComponentPreset::kInnerDiameter, 0.005);
    props.put(ComponentPreset::kFlangeHeight, 0.002);
    props.put(ComponentPreset::kBaseHeight, 0.002);
    EXPECT_TRUE(create(props).has_value());
}

// ================================================================ shaped presets with a mass

/// A complete spec of the NOSE_CONE, TRANSITION or RAIL_BUTTON @p type: every required key.
[[nodiscard]] TypedPropertyMap shapedSpec(ComponentPresetType type)
{
    if (type == ComponentPresetType::NOSE_CONE)
    {
        return noseConeSpec();
    }
    TypedPropertyMap props = spec(type);
    if (type == ComponentPresetType::TRANSITION)
    {
        props.put(ComponentPreset::kLength, 2.0);
        props.put(ComponentPreset::kAftOuterDiameter, 2.0);
        props.put(ComponentPreset::kForeOuterDiameter, 1.0);
    }
    else
    {
        props.put(ComponentPreset::kHeight, 0.01);
        props.put(ComponentPreset::kOuterDiameter, 0.01);
        props.put(ComponentPreset::kInnerDiameter, 0.005);
        props.put(ComponentPreset::kFlangeHeight, 0.002);
        props.put(ComponentPreset::kBaseHeight, 0.002);
    }
    return props;
}

/// The presets whose density OpenRocket derives through their component's getComponentVolume()
/// (ComponentPresetFactory.makeNoseCone, makeTransition and makeRailButton).
class ShapedPresetWithAMassTest : public ComponentPresetFactoryTest,
                                  public ::testing::WithParamInterface<ComponentPresetType>
{ };

TEST_P(ShapedPresetWithAMassTest, IsAcceptedWithoutAMass)
{
    const Outcome preset = create(shapedSpec(GetParam()));
    ASSERT_TRUE(preset.has_value());
    EXPECT_FALSE(preset->has(ComponentPreset::kMass));
}

TEST_P(ShapedPresetWithAMassTest, WaitsForItsComponent)
{
    // Interim: the density needs the component's volume, so such a preset is refused, with an
    // error that names no parameter.
    // TODO(presets): when NoseCone, Transition and RailButton are ported, derive the
    // "<Type>Custom" material from mass / getComponentVolume() and port
    // NoseConePresetTests.testOverriddenMass, testComputeDensityNoMaterial and
    // testComputeDensityWithMaterial (TransitionPresetTests has the last two commented out; no
    // RailButton preset test exists).
    TypedPropertyMap props = shapedSpec(GetParam());
    props.put(ComponentPreset::kMass, 0.123);
    const Outcome outcome = create(props);
    ASSERT_FALSE(outcome.has_value()) << "the preset was accepted";
    const InvalidPreset& problems = outcome.error();
    EXPECT_TRUE(problems.invalidParameters.empty());
    const std::string name(QtRocket::componentPresetTypeName(GetParam()));
    ASSERT_EQ(problems.errors.size(), 1U);
    EXPECT_EQ(problems.errors[0], "Mass of a " + name + " preset needs the " + name +
                                      " component, which is not ported yet");
    EXPECT_EQ(problems.problemCount(), 1U);
}

INSTANTIATE_TEST_SUITE_P(ComponentPresetFactory, ShapedPresetWithAMassTest,
                         ::testing::Values(ComponentPresetType::NOSE_CONE,
                                           ComponentPresetType::TRANSITION,
                                           ComponentPresetType::RAIL_BUTTON),
                         [](const ::testing::TestParamInfo<ComponentPresetType>& paramInfo) {
                             return std::string(QtRocket::componentPresetTypeName(paramInfo.param));
                         });

// ================================================================================== general

TEST_F(ComponentPresetFactoryTest, TypeMissingStopsAtOnce)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kLength, 2.0);
    expectInvalidPreset(
        create(props),
        {ComponentPreset::kManufacturer, ComponentPreset::kPartNo, ComponentPreset::kType},
        {"No Manufacturer specified", "No PartNo specified", "No Type specified"});
    TypedPropertyMap named = spec(ComponentPresetType::STREAMER);
    ASSERT_TRUE(named.remove(ComponentPreset::kType));
    expectInvalidPreset(create(named), {ComponentPreset::kType}, {"No Type specified"});
}

TEST_F(ComponentPresetFactoryTest, ProblemsInOrderAndAsError)
{
    const Outcome outcome = create(spec(ComponentPresetType::STREAMER, false, true));
    ASSERT_FALSE(outcome.has_value());
    const InvalidPreset& problems = outcome.error();
    EXPECT_TRUE(problems.hasProblems());
    EXPECT_EQ(problems.problemCount(), 3U);
    ASSERT_EQ(problems.errors.size(), 3U);
    EXPECT_EQ(problems.errors[0], "No Manufacturer specified");
    EXPECT_EQ(problems.errors[1], "No Length specified");
    EXPECT_EQ(problems.errors[2], "No Width specified");
    ASSERT_EQ(problems.invalidParameters.size(), 3U);
    EXPECT_EQ(problems.invalidParameters[0], ComponentPreset::kManufacturer);
    EXPECT_EQ(problems.invalidParameters[1], ComponentPreset::kLength);
    EXPECT_EQ(problems.invalidParameters[2], ComponentPreset::kWidth);

    const QtRocket::Result<ComponentPreset> result = ComponentPresetFactory::create(
        spec(ComponentPresetType::STREAMER, false, true), m_materials);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, QtRocket::ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message,
              "Invalid preset specification. No Manufacturer specified; No Length specified; No "
              "Width specified");

    // problemCount is the larger of the two lists: "Preset dimensions underspecified" has no key.
    const Outcome tube = create(spec(ComponentPresetType::BODY_TUBE));
    ASSERT_FALSE(tube.has_value());
    EXPECT_EQ(tube.error().invalidParameters.size(), 1U);
    EXPECT_EQ(tube.error().errors.size(), 2U);
    EXPECT_EQ(tube.error().problemCount(), 2U);

    const InvalidPreset none;
    EXPECT_FALSE(none.hasProblems());
    EXPECT_EQ(none.problemCount(), 0U);
    EXPECT_EQ(none.toError().message, "Invalid preset specification.");
}

TEST_F(ComponentPresetFactoryTest, DimensionsAreNotRangeChecked)
{
    // As in OpenRocket, only presence is checked.
    TypedPropertyMap props = spec(ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kLength, -2.0);
    props.put(ComponentPreset::kOuterDiameter, 1.0);
    props.put(ComponentPreset::kInnerDiameter, 3.0);
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    EXPECT_EQ(preset->get(ComponentPreset::kThickness), -1.0);
}

TEST_F(ComponentPresetFactoryTest, MassFindsAKnownMaterial)
{
    // Databases.findMaterial: a material of the storage with the name (ignoring case) and the
    // density is taken as it is.
    QtRocket::addBuiltinMaterials(m_materials);
    const double outerRadius = 0.0125;
    const double innerRadius = 0.012;
    const double length      = 0.5;
    const double volume =
        std::numbers::pi * ((outerRadius * outerRadius) - (innerRadius * innerRadius)) * length;
    TypedPropertyMap props = spec(ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kOuterDiameter, outerRadius * 2);
    props.put(ComponentPreset::kInnerDiameter, innerRadius * 2);
    props.put(ComponentPreset::kMass, 680.0 * volume);
    props.put(ComponentPreset::kMaterial,
              Material::newMaterial(Material::Type::BULK, "cardboard", 1.0, true));
    const Outcome preset = create(props);
    ASSERT_TRUE(preset.has_value());
    const Material& material = preset->get(ComponentPreset::kMaterial);
    EXPECT_EQ(material.getName(), "Cardboard");
    EXPECT_NEAR(material.getDensity(), 680.0, 1e-9);
    EXPECT_FALSE(material.isUserDefined());
}

TEST_F(ComponentPresetFactoryTest, TheSpecIsNotModified)
{
    TypedPropertyMap props = spec(ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kLength, 2.0);
    props.put(ComponentPreset::kOuterDiameter, 2.0);
    props.put(ComponentPreset::kThickness, 0.5);
    const std::size_t size = props.size();
    ASSERT_TRUE(create(props).has_value());
    EXPECT_EQ(props.size(), size);
    EXPECT_FALSE(props.containsKey(ComponentPreset::kInnerDiameter));
}

}  // namespace
