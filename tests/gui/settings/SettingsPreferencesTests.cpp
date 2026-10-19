#include "QtRocket/gui/settings/SettingsPreferences.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScopedApplicationPalette.h"
#include "TestTempDir.h"
#include "settings/PreferencesTranscript.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using SettingsPreferencesTest = Test::GuiTest;
using Names                   = std::vector<std::string>;
using ::testing::HasSubstr;
using ::testing::Not;

/// The INI file @p name in @p directory, as SettingsPreferences takes it.
[[nodiscard]] QString iniFile(const QtRocket::Test::TempDir& directory, std::string_view name)
{
    return QtStrings::fromPath(directory.resolve(name));
}

/// The text of the file @p name in @p directory, or "<no file>", with '\n' line ends (QSettings
/// ends the lines of an INI file with "\r\n" on Windows).
[[nodiscard]] std::string fileText(const QtRocket::Test::TempDir& directory, std::string_view name)
{
    std::string text = readTextFile(directory.resolve(name)).value_or("<no file>");
    std::erase(text, '\r');
    return text;
}

/// Writes every change of @p preferences to its file; whether that worked.
[[nodiscard]] bool synced(SettingsPreferences& preferences)
{
    return preferences.sync().has_value();
}

/// A copy of @p tree without its nodes that hold no key anywhere below them: what is left of a
/// tree once it went through an INI file, which has no empty sections.
void copyWithoutEmptyNodes(const InMemoryPreferences& tree, InMemoryPreferences& target)
{
    for (const std::string& key : tree.keys())
    {
        target.put(key, tree.get(key).value_or(""));
    }
    for (const std::string& name : tree.childrenNames())
    {
        InMemoryPreferences child;
        copyWithoutEmptyNodes(*tree.findNode(name), child);
        if (!child.empty())
        {
            target.getNode(name) = child;
        }
    }
}

[[nodiscard]] std::unique_ptr<InMemoryPreferences> withoutEmptyNodes(
    const InMemoryPreferences& tree)
{
    auto copy = std::make_unique<InMemoryPreferences>();
    copyWithoutEmptyNodes(tree, *copy);
    return copy;
}

// ------------------------------------------------------------------ the store, call by call

/// The answer of @p node to a findNode() of @p name, as text.
[[nodiscard]] std::string found(const Preferences& node, std::string_view name)
{
    const Preferences* const child = node.findNode(name);
    if (child == nullptr)
    {
        return std::format("findNode({}) = null", name);
    }
    std::string keys;
    for (const std::string& key : child->keys())
    {
        keys += std::format("{}={} ", key, child->get(key).value_or("?"));
    }
    return std::format("findNode({}) = [{}]", name, keys);
}

/// The keys and the child nodes of @p node, as text.
[[nodiscard]] std::string listed(const Preferences& node)
{
    std::string text = "keys:";
    for (const std::string& key : node.keys())
    {
        text += std::format(" '{}'='{}'", key, node.get(key).value_or("?"));
    }
    text += " children:";
    for (const std::string& name : node.childrenNames())
    {
        text += std::format(" '{}'", name);
    }
    return text;
}

/// The names the store must keep whatever QSettings makes of them, and values of every kind.
void putAwkwardNames(Preferences& p, std::vector<std::string>& answers)
{
    for (const std::string_view key :
         {"", "a/b", "a\\b", "%", "%2F", "%25", "50%", "General", " lead", "trail ", "a=b", "[x]",
          "#k", ";k", "\xC3\xA9t\xC3\xA9", "x.y"})
    {
        p.put(key, std::format("value of '{}'", key));
    }
    p.put("empty", "");
    p.put("commas", "0,0,240");
    p.put("list, spaced", "a, b");
    p.put("quotes", "\"q\" and '");
    p.put("at", "@ByteArray(x)");
    p.put("lines", "one\ntwo\r\nthree\ttab");
    p.put("bytes", std::string_view("\xFF\xFE\x00z\x80", 5));
    p.put("text with nul", std::string_view("a\0b", 3));
    answers.push_back(listed(p));
    answers.push_back(std::format("get(empty key) = {}", p.get("").value_or("<absent>")));
    answers.push_back(std::format("get(a/b) = {}", p.get("a/b").value_or("<absent>")));
}

/// The names of nodes the store must keep, and paths through them.
void makeAwkwardNodes(Preferences& p, std::vector<std::string>& answers)
{
    p.getNode("back\\slash").put("k", "1");
    p.getNode("%").put("k", "2");
    p.getNode("General").put("k", "3");
    p.getNode("deep/deeper/deepest").put("k", "4");
    static_cast<void>(p.getNode("empty node"));
    static_cast<void>(p.getNode("empty/inside"));
    answers.push_back(listed(p));
    answers.push_back(found(p, "back\\slash"));
    answers.push_back(found(p, "%"));
    answers.push_back(found(p, "deep/deeper/deepest"));
    answers.push_back(found(p, "deep/deeper"));
    answers.push_back(found(p, "deep/missing"));
    answers.push_back(found(p, "empty node"));
    answers.push_back(found(p, "empty"));
    answers.push_back(found(p, "empty/inside"));
    answers.push_back(found(p, "missing"));
    answers.push_back(found(p, ""));
    answers.push_back(found(p, "deep//deeper"));
    answers.push_back(found(p, "deep/"));
    answers.push_back(listed(p.getNode("deep")));
    answers.push_back(listed(p.getNode("empty")));
}

/// A key and a child node of the same name stay apart.
void useKeyAndNodeOfOneName(Preferences& p, std::vector<std::string>& answers)
{
    p.put("units", "a key");
    p.getNode("units").put("LENGTH", "in");
    p.getNode("units").getNode("deeper").put("k", "v");
    answers.push_back(listed(p));
    p.remove("units");
    answers.push_back(listed(p));
    answers.push_back(listed(p.getNode("units")));
    p.put("units", "again");
    p.getNode("units").clear();
    answers.push_back(listed(p));
    answers.push_back(listed(p.getNode("units")));
    answers.push_back(found(p, "units/deeper"));
}

/// remove() and clear(), the latter keeping the child nodes.
void removeAndClear(Preferences& p, std::vector<std::string>& answers)
{
    p.put("one", "1");
    p.put("two", "2");
    p.remove("one");
    p.remove("never there");
    answers.push_back(listed(p));
    Preferences& node = p.getNode("cleared");
    node.put("a", "1");
    node.getNode("child").put("b", "2");
    node.clear();
    answers.push_back(listed(node));
    answers.push_back(found(node, "child"));
    p.put("one", "1 again");
    answers.push_back(std::format("get(one) = {}", p.get("one").value_or("<absent>")));
}

/// A sequence of store operations and what the store answers to each; two stores that answer
/// alike give the same list.
[[nodiscard]] std::vector<std::string> runStoreScript(Preferences& p)
{
    std::vector<std::string> answers;
    answers.push_back(listed(p));
    answers.push_back(found(p, "nothing"));
    putAwkwardNames(p, answers);
    makeAwkwardNodes(p, answers);
    useKeyAndNodeOfOneName(p, answers);
    removeAndClear(p, answers);
    answers.push_back(listed(p));
    return answers;
}

TEST_F(SettingsPreferencesTest, StoreAnswersAsInMemoryPreferences)
{
    const QtRocket::Test::TempDir directory;
    InMemoryPreferences           memory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));

    EXPECT_EQ(runStoreScript(settings), runStoreScript(memory));
    EXPECT_EQ(*settings.snapshot(), memory);
    // The node references stay those of the same nodes.
    EXPECT_EQ(&settings.getNode("deep/deeper"), &settings.getNode("deep").getNode("deeper"));
    EXPECT_EQ(settings.findNode("deep/deeper"), &settings.getNode("deep/deeper"));
}

TEST_F(SettingsPreferencesTest, StoreSurvivesReopeningTheFile)
{
    const QtRocket::Test::TempDir directory;
    InMemoryPreferences           memory;
    static_cast<void>(runStoreScript(memory));
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        static_cast<void>(runStoreScript(settings));
        ASSERT_TRUE(synced(settings));
    }
    // A copy is read from the disk (QSettings keeps the files a process used in memory).
    const std::filesystem::path copy =
        directory.copy(directory.resolve("prefs.ini"), "copy/prefs.ini");
    const SettingsPreferences reopened(QtStrings::fromPath(copy));

    // The nodes that held nothing are gone (an INI file has no empty sections); everything else
    // is back, the awkward names included.
    EXPECT_EQ(*reopened.snapshot(), *withoutEmptyNodes(memory));
    EXPECT_EQ(reopened.get(""), "value of ''");
    EXPECT_EQ(reopened.get("a/b"), "value of 'a/b'");
    EXPECT_EQ(reopened.get("bytes"), std::string_view("\xFF\xFE\x00z\x80", 5));
    EXPECT_EQ(reopened.get("text with nul"), std::string_view("a\0b", 3));
    EXPECT_EQ(reopened.findNode("empty node"), nullptr);
    ASSERT_NE(reopened.findNode("back\\slash"), nullptr);
    EXPECT_EQ(reopened.findNode("back\\slash")->get("k"), "1");
    EXPECT_TRUE(reopened.status().has_value());
}

TEST_F(SettingsPreferencesTest, FileReadsLikeTheJavaPreferencesTree)
{
    const QtRocket::Test::TempDir directory;
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        settings.putDouble(PreferenceKeys::kDefaultMachNumber, 0.5);
        settings.getNode(PreferenceKeys::kUnitsNode).put("LENGTH", "in");
        settings.put("with/slash", "x");
        ASSERT_TRUE(synced(settings));
    }
    const std::string text = fileText(directory, "prefs.ini");
    // The root's keys are in the [General] section, a node is a section of its own.
    EXPECT_THAT(text, HasSubstr("[General]\nDefaultMachNumber=0.5\n"));
    EXPECT_THAT(text, HasSubstr("[units]\nLENGTH=in\n"));
    // A '/' of a key is escaped (and QSettings escapes the '%' once more).
    EXPECT_THAT(text, HasSubstr("with%252Fslash=x\n"));
}

TEST_F(SettingsPreferencesTest, EmptyAndBadNodeNames)
{
    const QtRocket::Test::TempDir directory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));
    EXPECT_THROW(static_cast<void>(settings.getNode("")), BugError);
    EXPECT_THROW(static_cast<void>(settings.getNode("a//b")), BugError);
    EXPECT_THROW(static_cast<void>(settings.getNode("a/")), BugError);
    EXPECT_EQ(settings.findNode(""), nullptr);
    EXPECT_EQ(settings.findNode("/a"), nullptr);
}

TEST_F(SettingsPreferencesTest, HandWrittenFileIsReadAsQSettingsReadsIt)
{
    const QtRocket::Test::TempDir directory;
    static_cast<void>(directory.write("prefs.ini",
                                      "[General]\n"
                                      "list=0, 0, 240\n"
                                      "quoted=\"0, 0, 240\"\n"
                                      "number=12\n"
                                      "50%25=no key of the store is stored so\n"
                                      "\n"
                                      "[%]\n"
                                      "k=no node has an empty name\n"
                                      "\n"
                                      "[sub]\n"
                                      "key=value\n"));
    const SettingsPreferences settings(iniFile(directory, "prefs.ini"));
    EXPECT_EQ(settings.get("list"), "0,0,240");
    EXPECT_EQ(settings.get("quoted"), "0, 0, 240");
    EXPECT_EQ(settings.getInt("number", 0), 12);
    EXPECT_EQ(settings.keys(), (Names{"list", "number", "quoted"}));
    EXPECT_EQ(settings.childrenNames(), Names{"sub"});
    EXPECT_EQ(settings.getInNode("sub", "key"), "value");
    EXPECT_EQ(settings.snapshot()->childrenNames(), Names{"sub"});
}

// -------------------------------------------------------------- every accessor of Preferences

TEST_F(SettingsPreferencesTest, EveryAccessorAnswersAsInMemoryPreferences)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    InMemoryPreferences                     memory;
    SettingsPreferences                     settings(iniFile(directory, "prefs.ini"));

    // OpenRocket's defaults, from an empty store.
    EXPECT_EQ(Gui::Test::readEveryPreference(settings), Gui::Test::readEveryPreference(memory));
    // The same calls store the same and emit changed() alike.
    EXPECT_EQ(Gui::Test::writeEveryPreference(settings), Gui::Test::writeEveryPreference(memory));
    EXPECT_EQ(Gui::Test::readEveryPreference(settings), Gui::Test::readEveryPreference(memory));
    EXPECT_EQ(*settings.snapshot(), memory);
}

TEST_F(SettingsPreferencesTest, EveryValueSurvivesReopeningTheFile)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    InMemoryPreferences                     memory;
    static_cast<void>(Gui::Test::writeEveryPreference(memory));
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        static_cast<void>(Gui::Test::writeEveryPreference(settings));
        ASSERT_TRUE(synced(settings));
    }
    const std::filesystem::path copy =
        directory.copy(directory.resolve("prefs.ini"), "copy/prefs.ini");
    const SettingsPreferences reopened(QtStrings::fromPath(copy));
    EXPECT_EQ(Gui::Test::readEveryPreference(reopened), Gui::Test::readEveryPreference(memory));
    EXPECT_EQ(*reopened.snapshot(), memory);
}

TEST_F(SettingsPreferencesTest, ReadingWritesNothing)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        static_cast<void>(Gui::Test::writeEveryPreference(settings));
        ASSERT_TRUE(synced(settings));
    }
    const std::string before = fileText(directory, "prefs.ini");
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        static_cast<void>(Gui::Test::readEveryPreference(settings));
        static_cast<void>(settings.snapshot());
        static_cast<void>(settings.findNode("missing/node"));
        ASSERT_TRUE(synced(settings));
        QCoreApplication::processEvents();
    }
    EXPECT_EQ(fileText(directory, "prefs.ini"), before);
    // SwingPreferences would have stored the default of every key read (such as the welcome
    // dialog's key of version 2.0); here the file holds only what was set.
    EXPECT_THAT(before, HasSubstr("IgnoreWelcome_1.0=true"));
    EXPECT_THAT(before, Not(HasSubstr("IgnoreWelcome_2.0")));
}

TEST_F(SettingsPreferencesTest, ReadingAMissingFileDoesNotMakeIt)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        static_cast<void>(Gui::Test::readEveryPreference(settings));
        EXPECT_EQ(settings.keys(), Names{});
        EXPECT_EQ(settings.childrenNames(), Names{});
        ASSERT_TRUE(synced(settings));
    }
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("prefs.ini")));
}

// ---------------------------------------------------------- units and materials, as the core

TEST_F(SettingsPreferencesTest, DefaultUnitsRoundTrip)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit("in"));
    ASSERT_TRUE(unitGroup(UnitGroupId::MASS).setDefaultUnit("oz"));
    ASSERT_TRUE(unitGroup(UnitGroupId::ANGLE).setDefaultUnit("rad"));
    InMemoryPreferences memory;
    memory.storeDefaultUnits();
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        settings.storeDefaultUnits();
        EXPECT_EQ(*settings.snapshot(), memory);
        ASSERT_TRUE(synced(settings));
    }

    UnitGroup::resetDefaultUnits();
    const SettingsPreferences reopened(iniFile(directory, "prefs.ini"));
    reopened.loadDefaultUnits();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_EQ(unitGroup(UnitGroupId::MASS).getDefaultUnit().getUnit(), "oz");
    EXPECT_EQ(unitGroup(UnitGroupId::ANGLE).getDefaultUnit().getUnit(), "rad");
    EXPECT_EQ(*reopened.snapshot(), memory);
}

/// The user materials of the material tests, with the characters a material string carries.
[[nodiscard]] std::vector<Material> someUserMaterials()
{
    return {
        Material::newMaterial(Material::Type::BULK, "Mine, \"best\" \xC2\xB0", 1234.5, true),
        Material::newMaterial(Material::Type::SURFACE, "Cloth", 0.1, 0.0, MaterialGroup::FABRICS,
                              true),
        Material::newMaterial(Material::Type::LINE, "Cord", 0.01, true),
    };
}

/// addUserMaterial() of each of @p materials.
void addUserMaterials(Preferences& preferences, const std::vector<Material>& materials,
                      const MaterialStorage& storage)
{
    for (const Material& material : materials)
    {
        addUserMaterial(preferences, material, storage);
    }
}

TEST_F(SettingsPreferencesTest, UserMaterialsRoundTrip)
{
    const QtRocket::Test::TempDir directory;
    MaterialStorage               storage;
    addBuiltinMaterials(storage);
    const std::vector<Material> materials = someUserMaterials();
    {
        SettingsPreferences settings(iniFile(directory, "prefs.ini"));
        addUserMaterials(settings, materials, storage);
        loadDefaultComponentMaterials(settings, storage);
        ASSERT_TRUE(synced(settings));
    }

    SettingsPreferences reopened(iniFile(directory, "prefs.ini"));
    EXPECT_EQ(getUserMaterials(reopened, storage), materials);
    EXPECT_EQ(
        getDefaultComponentMaterial(reopened, componentClassChain(ComponentKind::TRAPEZOID_FIN_SET),
                                    Material::Type::BULK, storage)
            .getName(),
        "Balsa");
    EXPECT_EQ(getDefaultComponentMaterial(reopened, componentClassChain(ComponentKind::NOSE_CONE),
                                          Material::Type::BULK, storage)
                  .getName(),
              "Polystyrene");

    // The application's material storage follows the store, as Databases does.
    MaterialStorage application;
    addBuiltinMaterials(application);
    EXPECT_EQ(addUserMaterials(application, reopened), materials.size());
    const auto     connections = storeUserMaterialChanges(application, reopened);
    const Material later       = Material::newMaterial(Material::Type::BULK, "Later", 5.0, true);
    EXPECT_TRUE(application.addMaterial(later));
    EXPECT_TRUE(application.removeMaterial(materials.at(2)));
    const std::vector<Material> expected{materials.at(0), materials.at(1), later};
    EXPECT_EQ(getUserMaterials(reopened, storage), expected);
}

// ---------------------------------------------------------------- snapshot, restore, reset

/// Something in every part of a store: keys, nodes, a nested node and an empty one.
void fillStore(Preferences& p)
{
    p.put("text", "some text");
    p.putDouble("number", 2.5);
    p.getNode("units").put("LENGTH", "cm");
    p.getNode("units").put("MASS", "g");
    p.getNode("OBJExportOptions/CoordTransform").put("xAxis", "Y");
    static_cast<void>(p.getNode("empty"));
}

/// Changes everything fillStore() put there.
void changeStore(Preferences& p)
{
    p.put("text", "changed");
    p.remove("number");
    p.put("new", "key");
    p.getNode("units").put("LENGTH", "ft");
    p.getNode("units").remove("MASS");
    p.getNode("OBJExportOptions").getNode("CoordTransform").clear();
    p.getNode("another/node").put("k", "v");
}

TEST_F(SettingsPreferencesTest, SnapshotIsTheWholeTree)
{
    const QtRocket::Test::TempDir directory;
    InMemoryPreferences           memory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));
    fillStore(memory);
    fillStore(settings);

    const std::unique_ptr<InMemoryPreferences> snapshot = settings.snapshot();
    EXPECT_EQ(*snapshot, memory);
    EXPECT_EQ(snapshot->childrenNames(), (Names{"OBJExportOptions", "empty", "units"}));
    // A snapshot of a node is that node's subtree.
    EXPECT_EQ(*settings.getNode("units").snapshot(), memory.getNode("units"));
    // It is a copy: later changes do not reach it.
    changeStore(settings);
    EXPECT_EQ(*snapshot, memory);
}

TEST_F(SettingsPreferencesTest, RestoreGoesBackToTheSnapshot)
{
    const QtRocket::Test::TempDir directory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));
    fillStore(settings);
    const std::unique_ptr<InMemoryPreferences> snapshot = settings.snapshot();
    const SettingsPreferences&                 units    = settings.getNode("units");

    changeStore(settings);
    EXPECT_NE(*settings.snapshot(), *snapshot);
    settings.restore(*snapshot);
    EXPECT_EQ(*settings.snapshot(), *snapshot);
    EXPECT_EQ(settings.findNode("another"), nullptr);
    EXPECT_NE(settings.findNode("empty"), nullptr);
    // The node objects stay: a reference taken before still is the node of its path.
    EXPECT_EQ(&settings.getNode("units"), &units);
    EXPECT_EQ(units.get("LENGTH"), "cm");
    EXPECT_EQ(units.get("MASS"), "g");

    // Restoring a store from itself changes nothing.
    settings.restore(settings);
    EXPECT_EQ(*settings.snapshot(), *snapshot);

    // And the file follows.
    ASSERT_TRUE(synced(settings));
    const std::filesystem::path copy =
        directory.copy(directory.resolve("prefs.ini"), "copy/prefs.ini");
    const SettingsPreferences reopened(QtStrings::fromPath(copy));
    EXPECT_EQ(*reopened.snapshot(), *withoutEmptyNodes(*snapshot));
}

TEST_F(SettingsPreferencesTest, RestoreOfANodeReplacesOnlyItsSubtree)
{
    const QtRocket::Test::TempDir directory;
    InMemoryPreferences           memory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));
    fillStore(memory);
    fillStore(settings);
    InMemoryPreferences replacement;
    replacement.put("LENGTH", "mm");
    replacement.getNode("nested").put("k", "v");

    memory.getNode("units") = replacement;
    settings.getNode("units").restore(replacement);
    EXPECT_EQ(*settings.snapshot(), memory);
}

TEST_F(SettingsPreferencesTest, ResetEmptiesTheTree)
{
    const QtRocket::Test::TempDir directory;
    InMemoryPreferences           memory;
    SettingsPreferences           settings(iniFile(directory, "prefs.ini"));
    fillStore(memory);
    fillStore(settings);
    SettingsPreferences& units = settings.getNode("units");

    // A node's reset leaves its parent's key of the same name, and the node itself.
    memory.put("units", "a key");
    settings.put("units", "a key");
    memory.getNode("units").reset();
    units.reset();
    EXPECT_EQ(*settings.snapshot(), memory);
    EXPECT_EQ(settings.get("units"), "a key");

    memory.reset();
    settings.reset();
    EXPECT_EQ(*settings.snapshot(), memory);
    EXPECT_EQ(settings.keys(), Names{});
    EXPECT_EQ(settings.childrenNames(), Names{});
    EXPECT_EQ(settings.findNode("units"), nullptr);

    // The reference still works; what it stores is the node of its path again.
    units.put("LENGTH", "in");
    EXPECT_EQ(settings.childrenNames(), Names{"units"});
    ASSERT_TRUE(synced(settings));
    EXPECT_EQ(fileText(directory, "prefs.ini"), "[units]\nLENGTH=in\n");
}

TEST_F(SettingsPreferencesTest, ClearPreferencesStoresTheDefaultUnits)
{
    // SwingPreferences.clearPreferences(): removes the whole node, resets the default units and
    // stores them.
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TempDir           directory;
    SettingsPreferences                     settings(iniFile(directory, "prefs.ini"));
    fillStore(settings);
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit("in"));

    settings.clearPreferences();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    InMemoryPreferences expected;
    expected.storeDefaultUnits();
    EXPECT_EQ(*settings.snapshot(), expected);
}

// ------------------------------------------------------------------------------- threads

/// What one writer thread does: @p rounds times it writes, reads back and removes keys of its
/// own in the root and in a node every thread uses, writes a key every thread writes and lists
/// the store; counts what did not read back in @p mismatches, and marks itself done.
void writeFromThread(SettingsPreferences& store, int thread, int rounds, int& mismatches,
                     std::atomic<int>& running)
{
    const std::string own = std::format("thread.{}", thread);
    for (int round = 0; round < rounds; ++round)
    {
        const std::string key   = std::format("{}.{}", own, round % 5);
        const std::string value = std::to_string(round);
        store.put(key, value);
        mismatches += store.get(key) == value ? 0 : 1;
        store.putInt("shared", round);
        Preferences& units = store.getNode(PreferenceKeys::kUnitsNode);
        units.put(key, value);
        mismatches += units.get(key) == value ? 0 : 1;
        mismatches += store.findNode(PreferenceKeys::kUnitsNode) != nullptr ? 0 : 1;
        mismatches += store.keys().empty() ? 1 : 0;
        units.remove(key);
        store.remove(key);
    }
    store.put(own, "done");
    --running;
}

/// Runs writeFromThread() on @p threads threads, @p rounds rounds each, while this thread (the
/// application's) changes the store too and runs its events; the mismatches of each thread.
[[nodiscard]] std::vector<int> useFromThreads(SettingsPreferences& store, int threads, int rounds)
{
    std::vector<int> mismatches(static_cast<std::size_t>(threads), 0);
    std::atomic<int> running{threads};
    {
        std::vector<std::jthread> writers;
        writers.reserve(mismatches.size());
        for (int thread = 0; thread < threads; ++thread)
        {
            writers.emplace_back(writeFromThread, std::ref(store), thread, rounds,
                                 std::ref(mismatches.at(static_cast<std::size_t>(thread))),
                                 std::ref(running));
        }
        for (int round = 0; running.load() > 0; ++round)
        {
            store.putInt("gui", round);
            QCoreApplication::processEvents();
            std::this_thread::yield();
        }
    }  // joins
    return mismatches;
}

/// The keys "thread.<i>" of @p text whose line says "done", for i < @p threads.
[[nodiscard]] int threadsDoneIn(const std::string& text, int threads)
{
    int done = 0;
    for (int thread = 0; thread < threads; ++thread)
    {
        done += text.contains(std::format("thread.{}=done\n", thread)) ? 1 : 0;
    }
    return done;
}

TEST_F(SettingsPreferencesTest, IsSafeToUseFromSeveralThreadsWhileTheEventLoopWritesTheFile)
{
    // Four threads use the store while the GUI thread changes it too and runs its events, in
    // which QSettings writes the file after a change (under the store's mutex). The tsan preset
    // is what finds an access that is not locked.
    constexpr int kThreads = 4;
    constexpr int kRounds  = 300;

    const QtRocket::Test::TempDir directory;
    SettingsPreferences           store(iniFile(directory, "prefs.ini"));
    EXPECT_EQ(useFromThreads(store, kThreads, kRounds), std::vector<int>(kThreads, 0));
    EXPECT_EQ(store.childrenNames(), Names{"units"});
    EXPECT_EQ(store.getNode("units").keys(), Names{});

    // No sync(): the event loop writes the file once the last change is in.
    QCoreApplication::processEvents();
    const std::string text = fileText(directory, "prefs.ini");
    EXPECT_EQ(threadsDoneIn(text, kThreads), kThreads);
    EXPECT_THAT(text, HasSubstr(std::format("shared={}\n", kRounds - 1)));
}

// ------------------------------------------------------------------- the file and its place

TEST_F(SettingsPreferencesTest, FileThatCannotBeWrittenIsAnIoError)
{
    const QtRocket::Test::TempDir directory;
    static_cast<void>(directory.write("blocker", "a file where a directory would have to be"));
    SettingsPreferences settings(iniFile(directory, "blocker/prefs.ini"));
    settings.put("key", "value");
    const Result<void> written = settings.sync();
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().code, ErrorCode::IO);
    EXPECT_THAT(written.error().message, HasSubstr("prefs.ini"));
    EXPECT_EQ(settings.status().error().code, ErrorCode::IO);
    // The value is still there for this run.
    EXPECT_EQ(settings.get("key"), "value");
}

TEST_F(SettingsPreferencesTest, FileThatIsNoIniFileIsAParseError)
{
    const QtRocket::Test::TempDir directory;
    static_cast<void>(directory.write("prefs.ini", "[unclosed\nkey=value\n"));
    const SettingsPreferences settings(iniFile(directory, "prefs.ini"));
    const Result<void>        status = settings.status();
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::PARSE);
}

TEST_F(SettingsPreferencesTest, DefaultFileIsTheIniFileOfQtRocket)
{
    // QSettings' INI file of the organisation and the application QtRocket (the tests' main()
    // moves QSettings' places into a temporary directory).
    const QString name = SettingsPreferences::defaultFileName();
    EXPECT_TRUE(name.endsWith(QStringLiteral("/QtRocket/QtRocket.ini"))) << name.toStdString();
    const std::unique_ptr<SettingsPreferences> opened = SettingsPreferences::openDefault();
    EXPECT_EQ(opened->fileName(), name);
}

// ----------------------------------------------------------- the theme's component colours

/// getDefaultColor() of @p chain asked on a thread of its own.
[[nodiscard]] Color defaultColorOnAnotherThread(const Preferences&  preferences,
                                                ComponentClassChain chain)
{
    Color color = Color::black();
    std::jthread([&color, &preferences, chain] {
        color = preferences.getDefaultColor(chain);
    }).join();
    return color;
}

TEST_F(SettingsPreferencesTest, DefaultComponentColorsFollowThePalette)
{
    const QtRocket::Test::TempDir directory;
    const SettingsPreferences     settings(iniFile(directory, "prefs.ini"));
    const InMemoryPreferences     memory;
    const auto                    bodyTube = componentClassChain(ComponentKind::BODY_TUBE);
    {
        const Gui::Test::ScopedApplicationPalette light(
            Gui::Test::ScopedApplicationPalette::light());
        EXPECT_EQ(settings.getDefaultColor(bodyTube), Color(0, 0, 240));
        EXPECT_EQ(settings.getDefaultColor(bodyTube), memory.getDefaultColor(bodyTube));
    }
    const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
    EXPECT_EQ(settings.getDefaultColor(bodyTube), Color(150, 162, 255));
    EXPECT_EQ(memory.getDefaultColor(bodyTube), Color(0, 0, 240));
    // Another thread gets the theme the application's thread saw last.
    EXPECT_EQ(defaultColorOnAnotherThread(settings, bodyTube), Color(150, 162, 255));
}

}  // namespace

}  // namespace QtRocket::Gui
