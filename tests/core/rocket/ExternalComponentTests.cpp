#include "QtRocket/rocket/ExternalComponent.h"

#include <memory>
#include <optional>
#include <type_traits>
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
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::ExternalComponent;
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;

/// A body tube whose protected copyFrom() (the in-place load of Rocket::loadFrom() and the fin
/// set conversion) is callable.
class LoadableTube : public BodyTube
{
public:
    using BodyTube::BodyTube;

    std::vector<std::unique_ptr<RocketComponent>> loadFields(const RocketComponent& source)
    {
        return copyFrom(source);
    }
};

/// A rocket with a stage holding a body tube; events enabled and counted.
class ExternalComponentTest : public ::testing::Test
{
protected:
    ExternalComponentTest()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_tube      = &stage.addChild(std::make_unique<BodyTube>(0.3, 0.02, 0.001));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener([this](const ComponentChangeEvent& e) {
            m_lastType = e.getType();
            ++m_count;
        });
    }

    Rocket                                  m_rocket;
    BodyTube*                               m_tube{nullptr};
    std::optional<int>                      m_lastType;
    int                                     m_count{0};
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(ExternalComponent, DefaultMaterialIsTheBuiltinCardboard)
{
    const Material& material = ExternalComponent::defaultMaterial();
    EXPECT_EQ(material.getName(), "Cardboard");
    EXPECT_EQ(material.getType(), Material::Type::BULK);
    EXPECT_EQ(material.getDensity(), 680.0);
    EXPECT_FALSE(material.isUserDefined());
    EXPECT_FALSE(material.isDocumentMaterial());

    const BodyTube tube;
    EXPECT_EQ(tube.getMaterial(), material);
    EXPECT_EQ(tube.getFinish(), Finish::NORMAL);
    EXPECT_TRUE(tube.isAerodynamic());
    EXPECT_TRUE(tube.isMassive());
}

TEST(ExternalComponent, MassIsDensityTimesVolume)
{
    Transition transition;
    transition.setForeRadius(0.01);
    transition.setAftRadius(0.02);
    transition.setMaterial(Material::newMaterial(Material::Type::BULK, "test", 1234.0, true));
    EXPECT_EQ(transition.getComponentMass(), 1234.0 * transition.getComponentVolume());
    EXPECT_EQ(transition.getMass(), transition.getComponentMass());
}

TEST(ExternalComponent, AllMaterialsIsTheMaterial)
{
    const NoseCone              nose;
    const std::vector<Material> materials = nose.getAllMaterials();
    ASSERT_EQ(materials.size(), 1U);
    EXPECT_EQ(materials[0], nose.getMaterial());
}

TEST(ExternalComponent, OnlyBulkMaterialsAreAccepted)
{
    BodyTube tube;
    EXPECT_THROW(
        tube.setMaterial(Material::newMaterial(Material::Type::SURFACE, "ripstop", 0.05, true)),
        BugError);
    EXPECT_THROW(tube.setMaterial(Material::newMaterial(Material::Type::LINE, "cord", 0.01, true)),
                 BugError);
    EXPECT_EQ(tube.getMaterial(), ExternalComponent::defaultMaterial());
}

TEST_F(ExternalComponentTest, SetMaterialFiresAMassChange)
{
    const Material wood = Material::newMaterial(Material::Type::BULK, "wood", 500.0, true);
    m_tube->setMaterial(wood);
    EXPECT_EQ(m_tube->getMaterial(), wood);
    EXPECT_EQ(m_lastType, ComponentChangeEvent::kMassChange);
    EXPECT_EQ(m_count, 1);

    m_tube->setMaterial(wood);
    EXPECT_EQ(m_count, 1) << "the same material fires nothing";
}

TEST_F(ExternalComponentTest, SetFinishFiresAnAerodynamicAndGraphicChange)
{
    m_tube->setFinish(Finish::POLISHED);
    EXPECT_EQ(m_tube->getFinish(), Finish::POLISHED);
    EXPECT_EQ(m_lastType,
              ComponentChangeEvent::kAerodynamicChange | ComponentChangeEvent::kGraphicChange);
    EXPECT_EQ(m_count, 1);

    m_tube->setFinish(Finish::POLISHED);
    EXPECT_EQ(m_count, 1) << "the same finish fires nothing";
}

TEST(ExternalComponent, PresetFinishAndMaterialAreLoaded)
{
    const QtRocket::MaterialStorage materials;
    TypedPropertyMap                presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 0.1);
    presetspec.put(ComponentPreset::kShape, TransitionShape::ELLIPSOID);
    presetspec.put(ComponentPreset::kAftOuterDiameter, 0.05);
    presetspec.put(ComponentPreset::kFinish, Finish::SMOOTH);
    const Material plastic = Material::newMaterial(Material::Type::BULK, "plastic", 1050.0, true);
    presetspec.put(ComponentPreset::kMaterial, plastic);
    const auto preset = ComponentPresetFactory::create(presetspec, materials);
    ASSERT_TRUE(preset.has_value());

    NoseCone nose;
    nose.loadPreset(&*preset);
    EXPECT_EQ(nose.getFinish(), Finish::SMOOTH);
    EXPECT_EQ(nose.getMaterial(), plastic);
    EXPECT_EQ(nose.getLength(), 0.1);
    EXPECT_EQ(nose.getShapeType(), TransitionShape::ELLIPSOID);
    EXPECT_EQ(nose.getPresetComponent(), &*preset);
}

TEST(ExternalComponent, CopiesKeepTheMaterialAndFinish)
{
    BodyTube tube(0.3, 0.02);
    tube.setFinish(Finish::ROUGH);
    tube.setMaterial(Material::newMaterial(Material::Type::BULK, "metal", 2700.0, true));

    const std::unique_ptr<RocketComponent> copy = tube.copyWithNewIds();
    const auto&                            bt   = dynamic_cast<const BodyTube&>(*copy);
    EXPECT_EQ(bt.getFinish(), Finish::ROUGH);
    EXPECT_EQ(bt.getMaterial(), tube.getMaterial());
}

TEST(ExternalComponent, CopyFromCopiesTheMaterialAndFinish)
{
    BodyTube source(0.3, 0.02);
    source.setFinish(Finish::MIRROR);
    source.setMaterial(Material::newMaterial(Material::Type::BULK, "glass", 2500.0, true));

    LoadableTube target(0.1, 0.01);
    static_cast<void>(target.loadFields(source));
    EXPECT_EQ(target.getFinish(), Finish::MIRROR);
    EXPECT_EQ(target.getMaterial(), source.getMaterial());
    EXPECT_EQ(target.getLength(), 0.3) << "and RocketComponent's fields";
    EXPECT_EQ(target.getId(), source.getId());

    // Only an external component can be the source.
    const AxialStage stage;
    EXPECT_THROW(static_cast<void>(target.loadFields(stage)), BugError);
}

TEST(ExternalComponent, AFailedCopyFromLeavesTheMaterialAndFinish)
{
    BodyTube source(0.3, 0.02);
    source.setFinish(Finish::MIRROR);
    source.setMaterial(Material::newMaterial(Material::Type::BULK, "glass", 2500.0, true));

    // copyFrom() is for a root component only: a parented target throws, unchanged.
    AxialStage    stage;
    LoadableTube& target = stage.addChild(std::make_unique<LoadableTube>(0.1, 0.01));
    EXPECT_THROW(static_cast<void>(target.loadFields(source)), BugError);
    EXPECT_EQ(target.getMaterial(), ExternalComponent::defaultMaterial());
    EXPECT_EQ(target.getFinish(), Finish::NORMAL);
    EXPECT_EQ(target.getLength(), 0.1);
}

/// The built-in materials and preferences holding OpenRocket's start-up component defaults
/// (SwingPreferences.loadDefaultComponentMaterials(): NoseCone Polystyrene, FinSet Balsa).
class DefaultMaterialTest : public ::testing::Test
{
protected:
    DefaultMaterialTest()
    {
        QtRocket::addBuiltinMaterials(m_storage);
        QtRocket::loadDefaultComponentMaterials(m_prefs, m_storage);
    }

    QtRocket::InMemoryPreferences m_prefs;
    QtRocket::MaterialStorage     m_storage;
};

TEST_F(DefaultMaterialTest, ApplyDefaultMaterialTakesThePerClassDefault)
{
    NoseCone   nose;
    BodyTube   tube;
    Transition transition;
    nose.applyDefaultMaterial(m_prefs, m_storage);
    tube.applyDefaultMaterial(m_prefs, m_storage);
    transition.applyDefaultMaterial(m_prefs, m_storage);
    EXPECT_EQ(nose.getMaterial().getName(), "Polystyrene");
    EXPECT_EQ(tube.getMaterial().getName(), "Cardboard") << "the built-in fallback";
    EXPECT_EQ(transition.getMaterial().getName(), "Cardboard");

    // A default for a superclass reaches every class below it, unless a nearer class has one.
    const std::optional<Material> found = m_storage.findMaterial(Material::Type::BULK, "Balsa");
    ASSERT_TRUE(found.has_value());
    const Material balsa =
        found.value_or(Material::newMaterial(Material::Type::BULK, "<not found>", 0, true));
    QtRocket::setDefaultComponentMaterial(m_prefs, "SymmetricComponent", balsa);
    nose.applyDefaultMaterial(m_prefs, m_storage);
    tube.applyDefaultMaterial(m_prefs, m_storage);
    transition.applyDefaultMaterial(m_prefs, m_storage);
    EXPECT_EQ(nose.getMaterial().getName(), "Polystyrene") << "NoseCone's own default first";
    EXPECT_EQ(tube.getMaterial(), balsa);
    EXPECT_EQ(transition.getMaterial(), balsa);
}

TEST_F(DefaultMaterialTest, ApplyDefaultMaterialFiresNothingAndKeepsThePreset)
{
    // As Java's constructor assigns the field: no event, no clearPreset().
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 0.1);
    presetspec.put(ComponentPreset::kShape, TransitionShape::OGIVE);
    presetspec.put(ComponentPreset::kAftOuterDiameter, 0.05);
    const auto preset = ComponentPresetFactory::create(presetspec, m_storage);
    ASSERT_TRUE(preset.has_value());

    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  nose  = stage.addChild(std::make_unique<NoseCone>());
    nose.loadPreset(&*preset);
    rocket.enableEvents();
    int                                           count = 0;
    const ComponentChangeSignal::ScopedConnection connection{
        rocket.addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; })};

    nose.applyDefaultMaterial(m_prefs, m_storage);
    EXPECT_EQ(nose.getMaterial().getName(), "Polystyrene");
    EXPECT_EQ(count, 0);
    EXPECT_EQ(nose.getPresetComponent(), &*preset);
}

TEST(ExternalComponent, TubeIsACoaxialExternalComponent)
{
    // Tube is the abstract base of launch lugs and tube fin sets; a body tube is Coaxial on its
    // own, as in OpenRocket.
    static_assert(std::is_base_of_v<ExternalComponent, QtRocket::Tube>);
    static_assert(std::is_base_of_v<QtRocket::Coaxial, QtRocket::Tube>);
    static_assert(std::is_abstract_v<QtRocket::Tube>);
    static_assert(!std::is_base_of_v<QtRocket::Tube, BodyTube>);
    static_assert(std::is_base_of_v<QtRocket::Coaxial, BodyTube>);
    SUCCEED();
}

}  // namespace
