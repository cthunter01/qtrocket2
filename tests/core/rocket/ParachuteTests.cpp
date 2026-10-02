// ParachuteTest.java (core/src/test/.../rocketcomponent; but for the .ork round trip, which needs
// the document and the file format) and ParachuterComponentTests.java (core/src/test/.../preset),
// ported, and the parachute's own behaviour.

#include "QtRocket/rocket/Parachute.h"

#include <memory>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestBodyComponent.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::TestBodyComponent;

constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// The factory's preset of @p props (with an empty material storage).
[[nodiscard]] ComponentPreset makePreset(const TypedPropertyMap& props)
{
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

/// The properties every parachute preset needs: type, manufacturer, part number, diameter, line
/// count and line length.
[[nodiscard]] TypedPropertyMap parachuteProps(double diameter, int lineCount, double lineLength)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kDiameter, diameter);
    props.put(ComponentPreset::kLineCount, lineCount);
    props.put(ComponentPreset::kLineLength, lineLength);
    return props;
}

// ============================================================================ ParachuteTest

// Java: testParachuteLineLengthAutomaticDefault
TEST(ParachuteTest, ParachuteLineLengthAutomaticDefault)
{
    const Parachute parachute;
    EXPECT_TRUE(parachute.isLineLengthAutomatic()) << "Expected line length automatic by default";
    EXPECT_NEAR(parachute.getDiameter() * 1.5, parachute.getLineLength(), kEpsilon)
        << "Auto line length should be 1.5x diameter";
}

// Java: testParachuteLineLengthManualDisablesAutomatic
TEST(ParachuteTest, ParachuteLineLengthManualDisablesAutomatic)
{
    Parachute parachute;
    parachute.setLineLengthAutomatic(true);
    parachute.setLineLength(0.25);
    EXPECT_FALSE(parachute.isLineLengthAutomatic())
        << "Manual line length should disable automatic";
    EXPECT_NEAR(0.25, parachute.getLineLength(), kEpsilon)
        << "Manual line length should be retained";
}

// Java: testParachuteLineLengthAutomaticSet
TEST(ParachuteTest, ParachuteLineLengthAutomaticSet)
{
    Parachute parachute;
    parachute.setLineLength(0.2);
    parachute.setDiameter(0.4);
    parachute.setLineLengthAutomatic(true);
    EXPECT_TRUE(parachute.isLineLengthAutomatic())
        << "Line length should be automatic after enabling";
    EXPECT_NEAR(0.6, parachute.getLineLength(), kEpsilon)
        << "Auto line length should match diameter";
}

// ================================================================ ParachuterComponentTests

/// ParachuterComponentTests.createPreset(): diameter 20 m, 8 lines of 12 m, the canopy material
/// "testMaterial" (2 kg/m^2) and the line material "testLineMaterial" (3 kg/m).
[[nodiscard]] ComponentPreset parachutePreset()
{
    TypedPropertyMap presetspec = parachuteProps(20.0, 8, 12.0);
    presetspec.put(ComponentPreset::kMaterial,
                   Material::newMaterial(Material::Type::SURFACE, "testMaterial", 2.0, true));
    presetspec.put(ComponentPreset::kLineMaterial,
                   Material::newMaterial(Material::Type::LINE, "testLineMaterial", 3, true));
    return makePreset(presetspec);
}

class ParachuterComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{parachutePreset()};
};

// Java: testComponentType
TEST_F(ParachuterComponentTest, ComponentType)
{
    const Parachute cr;
    EXPECT_EQ(cr.getPresetType(), ComponentPresetType::PARACHUTE);
}

// Java: testLoadFromPresetIsSane
TEST_F(ParachuterComponentTest, LoadFromPresetIsSane)
{
    Parachute cr;
    cr.loadPreset(&m_preset);

    EXPECT_EQ(cr.getDiameter(), 20.0);
    EXPECT_EQ(cr.getLineCount(), 8);
    EXPECT_EQ(cr.getLineLength(), 12.0);

    EXPECT_EQ(cr.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_EQ(cr.getLineMaterial(), m_preset.get(ComponentPreset::kLineMaterial));
    EXPECT_EQ(cr.getMaterial().getName(), "testMaterial");
    EXPECT_EQ(cr.getLineMaterial().getName(), "testLineMaterial");
}

// Java: changeDiameterClearsPreset
TEST_F(ParachuterComponentTest, ChangeDiameterClearsPreset)
{
    Parachute cr;
    cr.loadPreset(&m_preset);
    cr.setDiameter(1.0);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeAreaClearsPreset
TEST_F(ParachuterComponentTest, ChangeAreaClearsPreset)
{
    Parachute cr;
    cr.loadPreset(&m_preset);
    cr.setArea(1.0);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeLineCountClearsPreset
TEST_F(ParachuterComponentTest, ChangeLineCountClearsPreset)
{
    Parachute cr;
    cr.loadPreset(&m_preset);
    cr.setLineCount(12);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(ParachuterComponentTest, ChangeMaterialClearsPreset)
{
    Parachute cr;
    cr.loadPreset(&m_preset);
    cr.setMaterial(Material::newMaterial(Material::Type::SURFACE, "new", 1.0, true));
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

TEST_F(ParachuterComponentTest, ChangeLineLengthAndLineMaterialClearThePreset)
{
    // Java's changeLineLengthLeavesPreset and changeLineMaterialLeavesPreset are commented out
    // ("test fails"): with lines, both setters clear the preset.
    Parachute cr;
    cr.loadPreset(&m_preset);
    cr.setLineLength(24);
    EXPECT_EQ(cr.getPresetComponent(), nullptr);

    cr.loadPreset(&m_preset);
    cr.setLineMaterial(Material::newMaterial(Material::Type::LINE, "new", 1.0, true));
    EXPECT_EQ(cr.getPresetComponent(), nullptr);
}

// ================================================================================== Parachute

TEST(Parachute, Defaults)
{
    const Parachute chute;
    EXPECT_EQ(chute.kind(), ComponentKind::PARACHUTE);
    EXPECT_EQ(chute.getName(), "Parachute");
    EXPECT_EQ(chute.getDiameter(), 0.3);
    EXPECT_EQ(chute.getLineCount(), 6);
    EXPECT_NEAR(chute.getLineLength(), 0.45, 1e-15);
    EXPECT_EQ(chute.getCD(), Parachute::kDefaultCd);
    EXPECT_TRUE(chute.isCDAutomatic());
    EXPECT_EQ(chute.getLineMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_EQ(chute.getMaterial().getName(), "Ripstop nylon");
    EXPECT_EQ(chute.getDisplayOrderSide(), 11);
    EXPECT_EQ(chute.getDisplayOrderBack(), 9);
    EXPECT_NEAR(chute.getArea(), std::numbers::pi * 0.15 * 0.15, 1e-15);
    // The canopy plus 6 lines of 0.45 m.
    EXPECT_NEAR(chute.getComponentMass(),
                (std::numbers::pi * 0.15 * 0.15 * 0.067) + (6 * 0.45 * 0.0018), 1e-15);
}

TEST(Parachute, HoldsNoChildren)
{
    Parachute chute;
    EXPECT_FALSE(chute.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(chute.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(chute.addChild(std::make_unique<Parachute>()), BugError);
}

TEST(Parachute, AreaAndDiameter)
{
    Parachute chute;
    chute.setArea(std::numbers::pi);  // a radius of 1
    EXPECT_NEAR(chute.getDiameter(), 2.0, 1e-15);
    chute.setArea(-1.0);  // MathUtil.safeSqrt: a negative area gives 0
    EXPECT_EQ(chute.getDiameter(), 0.0);
    chute.setDiameter(-0.5);  // not clamped, as in Java
    EXPECT_EQ(chute.getDiameter(), -0.5);
}

TEST(Parachute, LineMaterialMustBeALineMaterial)
{
    Parachute chute;
    EXPECT_THROW(
        chute.setLineMaterial(Material::newMaterial(Material::Type::SURFACE, "s", 1.0, true)),
        BugError);
}

TEST(Parachute, SettersFire)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.5, 0.03));
    auto&  chute = body.addChild(std::make_unique<Parachute>());
    rocket.enableEvents();
    std::vector<int>                              types;
    const ComponentChangeSignal::ScopedConnection connection{rocket.addComponentChangeListener(
        [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};

    chute.setDiameter(0.3);
    chute.setArea(chute.getArea());
    chute.setLineCount(6);
    chute.setLineLengthAutomatic(true);
    chute.setLineMaterial(chute.getLineMaterial());
    EXPECT_TRUE(types.empty());

    chute.setDiameter(0.5);
    chute.setLineCount(8);
    chute.setLineLength(0.5);
    chute.setLineLengthAutomatic(true);
    chute.setLineMaterial(Material::newMaterial(Material::Type::LINE, "new", 0.001, true));
    EXPECT_EQ(types, (std::vector<int>{
                         ComponentChangeEvent::kAeromassChange, ComponentChangeEvent::kMassChange,
                         ComponentChangeEvent::kMassChange, ComponentChangeEvent::kMassChange,
                         ComponentChangeEvent::kMassChange}));

    // Without lines, the line settings are not functional.
    chute.setLineCount(0);
    types.clear();
    chute.setLineLength(0.7);
    chute.setLineLengthAutomatic(true);
    chute.setLineMaterial(Material::newMaterial(Material::Type::LINE, "other", 0.002, true));
    EXPECT_EQ(types, std::vector<int>(3, ComponentChangeEvent::kNonFunctionalChange));
}

// =========================================================================== loadFromPreset

TEST(Parachute, PresetSubstitutesDefaultsForMissingValues)
{
    // The factory needs a diameter, a line count and a line length; non-positive ones give the
    // defaults.
    TypedPropertyMap props = parachuteProps(-1.0, 0, -2.0);
    props.put(ComponentPreset::kCd, 0.0);
    props.put(ComponentPreset::kMass, -1.0);
    const ComponentPreset preset = makePreset(props);

    Parachute chute;
    chute.setName("My chute");
    chute.setCD(1.5);
    chute.setLineMaterial(Material::newMaterial(Material::Type::LINE, "Kevlar", 0.003, true));
    chute.setMassOverridden(true);
    chute.setOverrideMass(0.2);
    chute.loadPreset(&preset);

    EXPECT_EQ(chute.getName(), "Parachute");  // no description: the component name
    EXPECT_EQ(chute.getDiameter(), Parachute::kDefaultDiameter);
    EXPECT_TRUE(chute.isCDAutomatic());
    EXPECT_EQ(chute.getCD(), Parachute::kDefaultCd);
    EXPECT_EQ(chute.getLineCount(), Parachute::kDefaultLineCount);
    EXPECT_FALSE(chute.isLineLengthAutomatic());
    EXPECT_EQ(chute.getLineLength(), Parachute::kDefaultLineLength);
    EXPECT_EQ(chute.getLineMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_EQ(chute.getMaterial().getName(), "Ripstop nylon");  // no MATERIAL
    EXPECT_FALSE(chute.isMassOverridden());
    EXPECT_EQ(chute.getOverrideMass(), chute.getComponentMass());
}

TEST(Parachute, PresetWithoutALineMaterialLeavesTheDefaultLineMaterial)
{
    // Java's DEFAULT_LINE_MATERIAL and defaultMaterial, which its constructors read from the
    // preferences.
    Parachute chute;
    EXPECT_EQ(chute.getDefaultLineMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_EQ(chute.getDefaultLineMaterial(), chute.getLineMaterial());
    EXPECT_THROW(chute.setDefaultLineMaterial(
                     Material::newMaterial(Material::Type::SURFACE, "s", 1.0, true)),
                 BugError);
    EXPECT_THROW(
        chute.setDefaultLineMaterial(Material::newMaterial(Material::Type::BULK, "b", 1.0, true)),
        BugError);

    const Material kevlar = Material::newMaterial(Material::Type::LINE, "Kevlar", 0.003, true);
    const Material silk   = Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true);
    chute.setDefaultLineMaterial(kevlar);
    chute.setDefaultMaterial(silk);
    EXPECT_EQ(chute.getDefaultLineMaterial(), kevlar);
    EXPECT_EQ(chute.getLineMaterial().getName(), "Elastic cord (round 2 mm, 1/16 in)");

    // Neither MATERIAL nor LINE_MATERIAL: both defaults.
    const ComponentPreset preset = makePreset(parachuteProps(1.0, 6, 1.0));
    chute.loadPreset(&preset);
    EXPECT_EQ(chute.getLineMaterial(), kevlar);
    EXPECT_EQ(chute.getMaterial(), silk);
    EXPECT_EQ(chute.getPresetComponent(), &preset);

    // A line material whose description is too short counts as none.
    const Material unnamedLine = Material::newMaterial(Material::Type::LINE, "", 0.001, true);
    ASSERT_LE(QtRocket::Strings::javaLength(unnamedLine.toString()), 12U);
    TypedPropertyMap shortName = parachuteProps(1.0, 6, 1.0);
    shortName.put(ComponentPreset::kLineMaterial, unnamedLine);
    const ComponentPreset unnamed = makePreset(shortName);
    chute.setLineMaterial(Material::newMaterial(Material::Type::LINE, "Nylon", 0.002, true));
    chute.loadPreset(&unnamed);
    EXPECT_EQ(chute.getLineMaterial(), kevlar);

    // A copy keeps it, as Java's clone keeps the final field.
    const std::unique_ptr<Parachute> copy =
        QtRocket::componentCast<Parachute>(chute.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getDefaultLineMaterial(), kevlar);
    EXPECT_EQ(copy->getDefaultMaterial(), silk);
}

TEST(Parachute, PresetValues)
{
    TypedPropertyMap props = parachuteProps(1.2, 8, 1.1);
    props.put(ComponentPreset::kDescription, "Fancy chute");
    props.put(ComponentPreset::kCd, 1.5);
    props.put(ComponentPreset::kMass, 0.05);
    const ComponentPreset preset = makePreset(props);

    Parachute chute;
    chute.loadPreset(&preset);
    EXPECT_EQ(chute.getName(), "Fancy chute");
    EXPECT_EQ(chute.getDiameter(), 1.2);
    EXPECT_FALSE(chute.isCDAutomatic());
    EXPECT_EQ(chute.getCD(), 1.5);
    EXPECT_EQ(chute.getLineCount(), 8);
    EXPECT_EQ(chute.getLineLength(), 1.1);
    EXPECT_TRUE(chute.isMassOverridden());
    EXPECT_EQ(chute.getMass(), 0.05);
    EXPECT_EQ(chute.getPresetComponent(), &preset);

    // An empty description gives the component name.
    TypedPropertyMap unnamedProps = parachuteProps(1.2, 8, 1.1);
    unnamedProps.put(ComponentPreset::kDescription, "");
    const ComponentPreset unnamed = makePreset(unnamedProps);
    chute.loadPreset(&unnamed);
    EXPECT_EQ(chute.getName(), "Parachute");
}

TEST(Parachute, PresetPackedDimensionsAndTheAutomaticRadius)
{
    TypedPropertyMap props = parachuteProps(1.2, 8, 1.1);
    props.put(ComponentPreset::kPackedLength, 0.1);
    props.put(ComponentPreset::kPackedDiameter, 0.04);
    const ComponentPreset preset = makePreset(props);

    // Without the option, the radius becomes automatic.
    Parachute chute;
    chute.loadPreset(&preset);
    EXPECT_NEAR(chute.getLength(), 0.1, 1e-15);
    EXPECT_TRUE(chute.isRadiusAutomatic());
    EXPECT_EQ(chute.getRadius(), 0.02);  // no parent: the stored radius

    // The .ork loader passes false: the radius stays manual.
    Parachute loaded;
    loaded.loadPreset(&preset, RocketComponent::PresetLoadOptions{.allowAutoRadius = false});
    EXPECT_EQ(loaded.getLength(), 0.1);
    EXPECT_EQ(loaded.getRadius(), 0.02);
    EXPECT_FALSE(loaded.isRadiusAutomatic());

    // With only a packed length, the radius stays manual too.
    TypedPropertyMap lengthOnly = parachuteProps(1.2, 8, 1.1);
    lengthOnly.put(ComponentPreset::kPackedLength, 0.1);
    const ComponentPreset lengthPreset = makePreset(lengthOnly);
    Parachute             other;
    other.loadPreset(&lengthPreset);
    EXPECT_EQ(other.getLength(), 0.1);
    EXPECT_FALSE(other.isRadiusAutomatic());
}

TEST(Parachute, PresetFitsThePackIntoTheBody)
{
    TypedPropertyMap props = parachuteProps(1.2, 8, 1.1);
    props.put(ComponentPreset::kPackedLength, 0.1);
    props.put(ComponentPreset::kPackedDiameter, 0.04);
    const ComponentPreset preset = makePreset(props);

    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.5, 0.03));
    body.setInnerRadius(0.029);
    auto& chute = body.addChild(std::make_unique<Parachute>());
    rocket.enableEvents();
    chute.loadPreset(&preset);
    // The packed volume is kept: 0.02^2 * 0.1 = 0.029^2 * length.
    EXPECT_EQ(chute.getRadius(), 0.029);
    EXPECT_NEAR(chute.getLength(), (0.02 * 0.02 * 0.1) / (0.029 * 0.029), 1e-15);
}

}  // namespace
