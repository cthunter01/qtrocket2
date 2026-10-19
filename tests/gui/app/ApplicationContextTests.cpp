#include "QtRocket/gui/app/ApplicationContext.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QDir>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

/// A context over a preferences file of the test's own and the data of the build tree (or other
/// data), as the application makes it.
class ApplicationContextTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        m_context.reset();
        GuiTest::TearDown();
    }

    /// The preferences file.
    [[nodiscard]] QString preferencesFile() const
    {
        return QtStrings::fromPath(m_directory.resolve("QtRocket.ini"));
    }

    /// The context, made with @p dataPaths.
    ApplicationContext& makeContext(const DataPaths& dataPaths = DataPaths::forApplication())
    {
        auto prompts = std::make_unique<Gui::Test::ScriptedUserPrompts>();
        m_prompts    = prompts.get();
        m_context    = std::make_unique<ApplicationContext>(
            std::make_unique<SettingsPreferences>(preferencesFile()), dataPaths,
            std::move(prompts));
        return *m_context;
    }

    [[nodiscard]] const QtRocket::Test::TempDir&  directory() const noexcept { return m_directory; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts* prompts() const noexcept { return m_prompts; }

private:
    QtRocket::Test::TempDir             m_directory;
    Gui::Test::ScriptedUserPrompts*     m_prompts{nullptr};
    std::unique_ptr<ApplicationContext> m_context;
};

/// The name of the material a new @p component gets from the application's preferences.
template <class Component>
[[nodiscard]] std::string newComponentMaterial(const ApplicationContext& context)
{
    Component component;
    component.applyDefaultMaterial(context.services().preferences(),
                                   context.services().materials());
    return component.getMaterial().getName();
}

/// Whether @p storage holds the user-defined material @p name of @p type.
[[nodiscard]] bool hasUserMaterial(const MaterialStorage& storage, Material::Type type,
                                   const std::string& name)
{
    const std::optional<Material> material = storage.findMaterial(type, name);
    return material.has_value() && material->isUserDefined();
}

/// The names of the user materials @p preferences stores.
[[nodiscard]] std::vector<std::string> storedUserMaterials(const Preferences&     preferences,
                                                           const MaterialStorage& storage)
{
    std::vector<std::string> names;
    for (const Material& material : getUserMaterials(preferences, storage))
    {
        names.push_back(material.getName());
    }
    return names;
}

TEST_F(ApplicationContextTest, TheServicesAreTheContextsObjects)
{
    ApplicationContext& context  = makeContext();
    const GuiServices&  services = context.services();
    EXPECT_EQ(&services.preferences(), &context.preferences());
    EXPECT_EQ(&services.materials(), &context.materials());
    EXPECT_EQ(&services.motors(), &context.motors());
    EXPECT_EQ(&services.loadingContext(), &context.loadingContext());
    EXPECT_EQ(&services.prompts(), prompts());
    EXPECT_EQ(&context.prompts(), prompts());
    // QSettings gives its file with forward slashes on every platform.
    EXPECT_EQ(QDir::fromNativeSeparators(context.preferences().fileName()),
              QDir::fromNativeSeparators(preferencesFile()));
}

TEST_F(ApplicationContextTest, DesignsLoadWithTheContextsObjects)
{
    // As the command-line tool's environment (CliEnvironment).
    const ApplicationContext&     context = makeContext();
    const DocumentLoadingContext& loading = context.loadingContext();
    EXPECT_NE(loading.getMotorFinder(), nullptr);
    EXPECT_EQ(loading.getApplicationMaterials(), &context.services().materials());
    EXPECT_EQ(loading.getPreferences(), &context.preferences());
    EXPECT_NE(loading.getComponentPresetDatabase(), nullptr);
    EXPECT_NE(loading.getSimulationExtensionRegistry(), nullptr);
}

TEST_F(ApplicationContextTest, LoadsTheBundledMotorDatabase)
{
    const ApplicationContext& context = makeContext();
    EXPECT_FALSE(context.motors().getMotorSets().empty());
    EXPECT_TRUE(context.motorDatabaseProblems().empty());
    EXPECT_EQ(context.dataPaths().dataDirectory(), DataPaths::forApplication().dataDirectory());

    // An example's motors are found: OpenRocket loads it without a warning.
    const Result<LoadedDocument> loaded =
        GeneralRocketLoader(context.loadingContext())
            .load(context.dataPaths().examplesDirectory() / "A simple model rocket.ork");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->document->getRocket().getName(), "A simple model rocket");
    EXPECT_EQ(loaded->warnings.countCritical() + loaded->warnings.countNormal() +
                  loaded->warnings.countInformational(),
              0U);
}

TEST_F(ApplicationContextTest, AMissingMotorDatabaseIsAProblemToShow)
{
    const DataPaths           nowhere(directory().resolve("no data"));
    const ApplicationContext& context = makeContext(nowhere);
    EXPECT_TRUE(context.motors().getMotorSets().empty());
    // The directory that cannot be listed, then that no database was found in it.
    ASSERT_EQ(context.motorDatabaseProblems().size(), 2U);
    EXPECT_EQ(context.motorDatabaseProblems().front().file, nowhere.motorDirectory());
    EXPECT_EQ(context.motorDatabaseProblems().front().error.code, ErrorCode::IO);
    EXPECT_EQ(context.motorDatabaseProblems().back().file, nowhere.motorDirectory());
    EXPECT_EQ(context.motorDatabaseProblems().back().error.code, ErrorCode::NOT_FOUND);
}

TEST_F(ApplicationContextTest, NewComponentsGetTheDefaultComponentMaterials)
{
    // SwingStartup.runInEDT(): loadDefaultComponentMaterials() at every start.
    const ApplicationContext& context = makeContext();
    EXPECT_EQ(newComponentMaterial<NoseCone>(context), "Polystyrene");
    EXPECT_EQ(newComponentMaterial<TrapezoidFinSet>(context), "Balsa");
    EXPECT_EQ(newComponentMaterial<BodyTube>(context), "Cardboard");
}

TEST_F(ApplicationContextTest, LoadsTheDefaultUnitsBeforeAnyWindow)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    {
        SettingsPreferences stored(preferencesFile());
        ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit("in"));
        stored.storeDefaultUnits();
        ASSERT_TRUE(stored.sync().has_value());
    }
    UnitGroup::resetDefaultUnits();
    ASSERT_NE(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");

    static_cast<void>(makeContext());
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_TRUE(QApplication::topLevelWidgets().isEmpty());
}

TEST_F(ApplicationContextTest, LoadsTheUsersMaterialsAndStoresTheirChanges)
{
    {
        SettingsPreferences stored(preferencesFile());
        stored.getNode(PreferenceKeys::kUserMaterialsNode)
            .put("material0", "BULK|Mine|2.0|0.0|Custom");
        ASSERT_TRUE(stored.sync().has_value());
    }
    ApplicationContext& context = makeContext();
    EXPECT_TRUE(hasUserMaterial(context.materials(), Material::Type::BULK, "Mine"));
    // Databases' listener stores what the user adds.
    context.materials().addMaterial(
        Material::newMaterial(Material::Type::LINE, "Cord", 0.01, true));
    EXPECT_EQ(storedUserMaterials(context.preferences(), context.materials()),
              (std::vector<std::string>{"Mine", "Cord"}));
}

}  // namespace

}  // namespace QtRocket::Gui
