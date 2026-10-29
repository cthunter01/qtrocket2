#include "QtRocket/gui/app/ApplicationContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QApplication>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

/// A context over a preferences file of the test's own and the data of the build tree (in
/// QStandardPaths' test mode without the bundled presets and the user's data:
/// DataPaths::forApplication()) or other data, as the application makes it.
class ApplicationContextTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        m_context.reset();
        GuiTest::TearDown();
    }

    /// The data of the build tree with the user's data directory @p userData, in the test's
    /// directory, without the bundled presets.
    [[nodiscard]] DataPaths withUserData(const std::filesystem::path& userData) const
    {
        return DataPaths::forApplication().withUserDataDirectory(m_directory.resolve(userData));
    }

    /// Stores @p text as the preference @p key in the preferences file, for the next start.
    void storePreference(std::string_view key, std::string_view text) const
    {
        SettingsPreferences stored(preferencesFile());
        stored.put(key, text);
        ASSERT_TRUE(stored.sync().has_value());
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
    EXPECT_EQ(&services.presets(), &context.presets());
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
    // The services' presets (none here: the test's data has no bundled presets).
    EXPECT_EQ(loading.getComponentPresetDatabase(), &context.services().presets());
    EXPECT_TRUE(context.presets().empty());
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
    const DataPaths nowhere =
        DataPaths(directory().resolve("no data")).withComponentDirectory(std::nullopt);
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

/// A thrust curve of a RASP file: an Estes A8 (as MotorDatabaseLoaderTests' kA8), a motor the
/// bundled database does not hold under the manufacturer "QtRocket".
constexpr std::string_view kUserMotor = "A8 18 70 3 0.003 0.016 QtRocket\n0 0\n0.5 5\n1 0\n";

/// The motor sets of the bundled database, which a user's file adds one to.
[[nodiscard]] std::size_t bundledMotorSets()
{
    // OpenRocket 5f164fd0e's database (MotorDatabaseLoaderTests' kBundledSets).
    constexpr std::size_t kBundledSets = 1458;
    return kBundledSets;
}

/// Whether @p context's motors hold a set of the manufacturer "QtRocket" (kUserMotor).
[[nodiscard]] bool hasUserMotor(const ApplicationContext& context)
{
    return std::ranges::any_of(context.motors().getMotorSets(), [](const ThrustCurveMotorSet& set) {
        return set.getDesignation() == "A8" && set.getManufacturer() != nullptr &&
               set.getManufacturer()->getDisplayName() == "QtRocket";
    });
}

/// Whether @p path is a directory.
[[nodiscard]] bool isDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

/// Whether @p path exists.
[[nodiscard]] bool pathExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

TEST_F(ApplicationContextTest, LoadsTheBundledPresets)
{
    // ComponentPresetDatabaseLoader.loadPresetComponents(): data/components, as the command-line
    // tool reads it.
    const DataPaths           tests = DataPaths::forApplication();
    const ApplicationContext& context =
        makeContext(tests.withComponentDirectory(tests.bundledComponentDirectory()));
    EXPECT_EQ(context.presets().size(), 5228U);
    EXPECT_TRUE(context.componentPresetProblems().empty());
    EXPECT_TRUE(context.databaseProblems().empty());
    EXPECT_EQ(&context.services().presets(), &context.presets());

    // An example's presets are found: OpenRocket loads it without a warning (with no presets,
    // two "No matching ComponentPreset" warnings).
    const Result<LoadedDocument> loaded =
        GeneralRocketLoader(context.loadingContext())
            .load(context.dataPaths().examplesDirectory() / "Deployable payload.ork");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->warnings.countCritical() + loaded->warnings.countNormal() +
                  loaded->warnings.countInformational(),
              0U);
}

TEST_F(ApplicationContextTest, TheTestsStartWithoutTheBundledPresets)
{
    // The test seam: QStandardPaths' test mode (DataPaths::forApplication()).
    const DataPaths tests = DataPaths::forApplication();
    EXPECT_EQ(tests.componentDirectory(), std::nullopt);
    EXPECT_EQ(tests.bundledComponentDirectory(), tests.dataDirectory() / "components");
    EXPECT_TRUE(isDirectory(tests.bundledComponentDirectory()));
    // The application's data of the same directory has them.
    EXPECT_EQ(DataPaths(tests.dataDirectory()).componentDirectory(),
              std::optional(tests.bundledComponentDirectory()));
}

TEST_F(ApplicationContextTest, LoadsTheUsersFilesFromTheDefaultDirectories)
{
    // The lists unset: the default directories <user data>/ThrustCurves and Components.
    static_cast<void>(
        directory().write(std::filesystem::path("user") / "ThrustCurves" / "mine.eng", kUserMotor));
    static_cast<void>(
        directory().copy(QtRocket::Test::testDataDir() / "presets" / "every-kind.orc",
                         std::filesystem::path("user") / "Components" / "sub" / "every-kind.orc"));
    const ApplicationContext& context = makeContext(withUserData("user"));
    EXPECT_TRUE(hasUserMotor(context));
    EXPECT_EQ(context.motors().getMotorSets().size(), bundledMotorSets() + 1);
    // every-kind.orc: one preset of each of the eleven kinds.
    EXPECT_EQ(context.presets().size(), 11U);
    EXPECT_TRUE(context.motorDatabaseProblems().empty());
    EXPECT_TRUE(context.userThrustCurveProblems().empty());
    EXPECT_TRUE(context.componentPresetProblems().empty());
    EXPECT_TRUE(context.databaseProblems().empty());
}

TEST_F(ApplicationContextTest, LoadsTheUsersFilesTheirPreferencesName)
{
    // The stored lists (absolute paths joined by '|'): the default directories are not read.
    static_cast<void>(
        directory().write(std::filesystem::path("user") / "ThrustCurves" / "mine.eng", kUserMotor));
    const std::filesystem::path motor =
        directory().write(std::filesystem::path("elsewhere") / "a8.eng", kUserMotor);
    const std::filesystem::path preset =
        directory().copy(QtRocket::Test::testDataDir() / "presets" / "legacy.orc",
                         std::filesystem::path("elsewhere") / "presets" / "legacy.orc");
    storePreference(PreferenceKeys::kUserThrustCurvesKey, pathToUtf8(motor));
    storePreference(PreferenceKeys::kUserComponentPresetsKey, pathToUtf8(preset.parent_path()));
    const ApplicationContext& context = makeContext(withUserData("user"));
    EXPECT_TRUE(hasUserMotor(context));
    EXPECT_EQ(context.motors().getMotorSets().size(), bundledMotorSets() + 1);
    // legacy.orc: a streamer and a bulkhead.
    EXPECT_EQ(context.presets().size(), 2U);
    EXPECT_TRUE(context.databaseProblems().empty());
    // Nothing is made for a list that is set.
    EXPECT_FALSE(pathExists(directory().resolve(std::filesystem::path("user") / "Components")));
}

TEST_F(ApplicationContextTest, MakesTheDefaultDirectoriesWhenTheirListIsUnset)
{
    // OpenRocket's getters mkdirs() them; here the start does.
    const std::filesystem::path userData =
        directory().resolve(std::filesystem::path("home") / "data" / "QtRocket");
    const ApplicationContext& context =
        makeContext(withUserData(std::filesystem::path("home") / "data" / "QtRocket"));
    EXPECT_TRUE(isDirectory(userData / "ThrustCurves"));
    EXPECT_TRUE(isDirectory(userData / "Components"));
    EXPECT_EQ(context.services().startupData().defaultUserThrustCurveDirectory,
              std::optional(userData / "ThrustCurves"));
    EXPECT_EQ(context.services().startupData().defaultUserComponentDirectory,
              std::optional(userData / "Components"));
    // An empty directory gives no file and no problem.
    EXPECT_TRUE(context.userThrustCurveProblems().empty());
    EXPECT_TRUE(context.componentPresetProblems().empty());
    // The preferences stay unset.
    EXPECT_FALSE(context.preferences().get(PreferenceKeys::kUserThrustCurvesKey).has_value());
    EXPECT_FALSE(context.preferences().get(PreferenceKeys::kUserComponentPresetsKey).has_value());
}

TEST_F(ApplicationContextTest, MakesNoDefaultDirectoryForAListThatIsSet)
{
    // An empty list is no file at all; a list of a file that is missing names it alone.
    storePreference(PreferenceKeys::kUserThrustCurvesKey, "");
    storePreference(PreferenceKeys::kUserComponentPresetsKey,
                    pathToUtf8(directory().resolve("missing.orc")));
    const ApplicationContext& context =
        makeContext(withUserData(std::filesystem::path("home") / "QtRocket"));
    EXPECT_FALSE(pathExists(directory().resolve("home")));
    EXPECT_TRUE(context.userThrustCurveProblems().empty());
    // OpenRocket logs "... is neither file nor directory": no warning for the user.
    ASSERT_EQ(context.componentPresetProblems().size(), 1U);
    EXPECT_EQ(context.componentPresetProblems().front().error.code, ErrorCode::NOT_FOUND);
    EXPECT_TRUE(context.databaseProblems().empty());
}

TEST_F(ApplicationContextTest, WithoutAUsersDataDirectoryNothingIsMadeOrRead)
{
    // The tests' data: QStandardPaths' test mode has no user's data directory.
    const ApplicationContext& context = makeContext();
    EXPECT_EQ(context.dataPaths().userDataDirectory(), std::nullopt);
    EXPECT_EQ(context.services().startupData().defaultUserThrustCurveDirectory, std::nullopt);
    EXPECT_EQ(context.services().startupData().defaultUserComponentDirectory, std::nullopt);
    EXPECT_TRUE(context.userThrustCurveProblems().empty());
    EXPECT_TRUE(context.componentPresetProblems().empty());
    EXPECT_EQ(context.motors().getMotorSets().size(), bundledMotorSets());
}

TEST_F(ApplicationContextTest, TheUsersFilesThatCannotBeLoadedAreDatabaseProblems)
{
    // MotorDbLoaderDlg for a thrust-curve file its loader refuses, ComponentDbLoaderDlg for a
    // preset file that cannot be read (what OpenRocket only logs is no such problem:
    // MakesNoDefaultDirectoryForAListThatIsSet).
    const std::filesystem::path brokenMotor =
        directory().write(std::filesystem::path("user") / "ThrustCurves" / "broken.eng", "junk\n");
    static_cast<void>(
        directory().write(std::filesystem::path("user") / "ThrustCurves" / "good.eng", kUserMotor));
    const std::filesystem::path brokenPreset =
        directory().write(std::filesystem::path("user") / "Components" / "broken.orc", "<not xml");
    const ApplicationContext& context = makeContext(withUserData("user"));
    EXPECT_TRUE(hasUserMotor(context));
    ASSERT_EQ(context.userThrustCurveProblems().size(), 1U);
    ASSERT_EQ(context.componentPresetProblems().size(), 1U);
    const std::vector<UserPrompts::DatabaseProblem>& problems = context.databaseProblems();
    ASSERT_EQ(problems.size(), 2U);
    EXPECT_EQ(problems[0].database, UserPrompts::DatabaseKind::THRUST_CURVES);
    EXPECT_EQ(problems[0].file, brokenMotor);
    EXPECT_EQ(problems[0].message,
              QtStrings::fromUtf8(context.userThrustCurveProblems().front().error.message));
    EXPECT_FALSE(problems[0].message.isEmpty());
    EXPECT_EQ(problems[1].database, UserPrompts::DatabaseKind::COMPONENT_PRESETS);
    EXPECT_EQ(problems[1].file, brokenPreset);
    EXPECT_TRUE(problems[1].message.startsWith(
        QStringLiteral("Unable to parse component preset file: broken.orc")));
}

/// The text that the rich text @p html shows.
[[nodiscard]] QString shownText(const QString& html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText();
}

TEST_F(ApplicationContextTest, ARaspFilesWarningShowsOpenRocketsLineBreak)
{
    // RASPMotorLoader's message for a header line without its 7 fields holds "<br>" and "&nbsp",
    // which MotorDatabaseLoader.loadFile() puts into its HTML as they are: a line break and a
    // space, not text.
    static_cast<void>(
        directory().write(std::filesystem::path("user") / "ThrustCurves" / "broken.eng", "junk\n"));
    const ApplicationContext& context = makeContext(withUserData("user"));
    const QStringList lines = WidgetUserPrompts::databaseProblemLines(context.databaseProblems());
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines.front().contains(QStringLiteral(
        "<i>Illegal file format. Motor header line must contain 7 fields:<br>&nbsp; designation "
        "diameter length delays propellantWeight totalWeight manufacturer</i>.")))
        << lines.front().toStdString();
    const QString shown = shownText(lines.front());
    EXPECT_FALSE(shown.contains(QStringLiteral("<br>"))) << shown.toStdString();
    EXPECT_FALSE(shown.contains(QStringLiteral("&nbsp"))) << shown.toStdString();
    EXPECT_TRUE(shown.contains(QStringLiteral("must contain 7 fields:\n"))) << shown.toStdString();
    EXPECT_TRUE(shown.contains(QStringLiteral(" designation diameter length")))
        << shown.toStdString();
}

TEST_F(ApplicationContextTest, ABrokenPresetFileIsLeftOutAloneAndShown)
{
    // Bug 18: OpenRocket's NullPointerException for a body tube without its <Length> ends the
    // whole preset load with an internal error. Here that file alone is left out, the one after
    // it loads, and the start's message names it, its '<' and '>' as text.
    const std::filesystem::path broken =
        directory().copy(QtRocket::Test::testDataDir() / "presets" / "broken.orc",
                         std::filesystem::path("user") / "Components" / "broken.orc");
    static_cast<void>(directory().copy(QtRocket::Test::testDataDir() / "presets" / "legacy.orc",
                                       std::filesystem::path("user") / "Components" / "later.orc"));
    const ApplicationContext& context = makeContext(withUserData("user"));
    // later.orc: a streamer and a bulkhead.
    EXPECT_EQ(context.presets().size(), 2U);
    const std::vector<UserPrompts::DatabaseProblem>& problems = context.databaseProblems();
    ASSERT_EQ(problems.size(), 1U);
    EXPECT_EQ(problems.front().database, UserPrompts::DatabaseKind::COMPONENT_PRESETS);
    EXPECT_EQ(problems.front().file, broken);
    EXPECT_EQ(problems.front().message,
              QStringLiteral("Unable to parse component preset file: broken.orc: the BodyTube "
                             "'BT-50' (component 2) has no <Length>"));
    const QStringList lines = WidgetUserPrompts::databaseProblemLines(problems);
    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(shownText(lines.front()).contains(QStringLiteral("has no <Length>.")))
        << lines.front().toStdString();
}

TEST_F(ApplicationContextTest, ReadsTheMotorDatabasesVersion)
{
    // data/motors/metadata.json, OpenRocket's: its database_version.
    const ApplicationContext& context = makeContext();
    EXPECT_EQ(context.motorDatabaseVersion(), std::optional<std::int64_t>(20260418111610));
    EXPECT_EQ(context.services().startupData().motorDatabaseVersion,
              context.motorDatabaseVersion());
}

TEST_F(ApplicationContextTest, AMissingMotorMetadataGivesNoVersion)
{
    const DataPaths nowhere =
        DataPaths(directory().resolve("no data")).withComponentDirectory(std::nullopt);
    const ApplicationContext& context = makeContext(nowhere);
    EXPECT_EQ(context.motorDatabaseVersion(), std::nullopt);
    EXPECT_EQ(context.services().startupData().motorDatabaseVersion, std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
