#include "QtRocket/gui/dialogs/PreferencesStore.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using PreferencesStoreTest = Gui::Test::GuiTest;

/// A store of neither kind the dialog knows: an InMemoryPreferences behind another class.
class OtherStore final : public Preferences
{
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const override
    {
        return m_store.get(key);
    }
    void put(std::string_view key, std::string_view value) override { m_store.put(key, value); }
    void remove(std::string_view key) override { m_store.remove(key); }
    void clear() override { m_store.clear(); }
    [[nodiscard]] std::vector<std::string> keys() const override { return m_store.keys(); }
    [[nodiscard]] std::vector<std::string> childrenNames() const override
    {
        return m_store.childrenNames();
    }
    [[nodiscard]] Preferences& getNode(std::string_view name) override
    {
        return m_store.getNode(name);
    }
    [[nodiscard]] const Preferences* findNode(std::string_view name) const noexcept override
    {
        return m_store.findNode(name);
    }

private:
    InMemoryPreferences m_store;
};

/// Fills @p store with a few keys and nodes.
void fill(Preferences& store)
{
    store.put("KeptKey", "before");
    store.getNode("windows").put("position.ComponentConfigDialog", "10,20");
}

/// Changes @p store after a snapshot: a key changed, one added, a node added.
void change(Preferences& store)
{
    store.put("KeptKey", "after");
    store.put("AddedKey", "added");
    store.getNode("addedNode").put("key", "value");
}

TEST_F(PreferencesStoreTest, RestoresAStoreInMemoryExactly)
{
    InMemoryPreferences store;
    fill(store);
    const std::unique_ptr<InMemoryPreferences> snapshot = PreferencesStore::snapshot(store);
    EXPECT_EQ(*snapshot, store);
    change(store);
    PreferencesStore::restore(store, *snapshot);
    // PreferencesProbe: java.util.prefs' import keeps AddedKey and addedNode/key; QtRocket's
    // restore removes them (PreferencesStore's deviation).
    EXPECT_EQ(store, *snapshot);
    EXPECT_EQ(store.get("KeptKey"), "before");
    EXPECT_EQ(store.get("AddedKey"), std::nullopt);
    EXPECT_EQ(store.findNode("addedNode"), nullptr);
}

TEST_F(PreferencesStoreTest, RestoresTheSettingsStoreExactly)
{
    const QtRocket::Test::TempDir directory;
    SettingsPreferences           store(QtStrings::fromPath(directory.resolve("prefs.ini")));
    fill(store);
    const std::unique_ptr<InMemoryPreferences> snapshot = PreferencesStore::snapshot(store);
    EXPECT_EQ(*snapshot, *store.snapshot());
    change(store);
    PreferencesStore::restore(store, *snapshot);
    EXPECT_EQ(*store.snapshot(), *snapshot);
    EXPECT_EQ(store.get("AddedKey"), std::nullopt);
}

TEST_F(PreferencesStoreTest, ClearPreferencesLeavesTheFactoryUnitsStored)
{
    const QtRocket::Test::TempDir directory;
    SettingsPreferences           settings(QtStrings::fromPath(directory.resolve("prefs.ini")));
    InMemoryPreferences           memory;
    for (Preferences* store : std::vector<Preferences*>{&settings, &memory})
    {
        fill(*store);
        static_cast<void>(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));
        PreferencesStore::clearPreferences(*store);
        EXPECT_EQ(store->keys(), std::vector<std::string>{});
        EXPECT_EQ(store->childrenNames(), std::vector<std::string>{"units"});
        EXPECT_EQ(store->getNode("units").get("LENGTH"), "cm");
        EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    }
}

TEST_F(PreferencesStoreTest, AnotherStoreIsABug)
{
    OtherStore                store;
    const InMemoryPreferences snapshot;
    EXPECT_THROW(static_cast<void>(PreferencesStore::snapshot(store)), BugError);
    EXPECT_THROW(PreferencesStore::restore(store, snapshot), BugError);
    EXPECT_THROW(PreferencesStore::clearPreferences(store), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
