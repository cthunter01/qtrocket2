#include "QtRocket/gui/dialogs/PreferencesStore.h"

#include <memory>

#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::PreferencesStore
{

namespace
{

[[noreturn]] void unknownStore()
{
    bug("The preferences dialog needs a SettingsPreferences or an InMemoryPreferences store");
}

}  // namespace

std::unique_ptr<InMemoryPreferences> snapshot(const Preferences& store)
{
    if (const auto* const settings = dynamic_cast<const SettingsPreferences*>(&store))
    {
        return settings->snapshot();
    }
    if (const auto* const memory = dynamic_cast<const InMemoryPreferences*>(&store))
    {
        return std::make_unique<InMemoryPreferences>(*memory);
    }
    unknownStore();
}

void restore(Preferences& store, const InMemoryPreferences& snapshot)
{
    if (auto* const settings = dynamic_cast<SettingsPreferences*>(&store))
    {
        settings->restore(snapshot);
        return;
    }
    if (auto* const memory = dynamic_cast<InMemoryPreferences*>(&store))
    {
        *memory = snapshot;
        return;
    }
    unknownStore();
}

void clearPreferences(Preferences& store)
{
    if (auto* const settings = dynamic_cast<SettingsPreferences*>(&store))
    {
        settings->clearPreferences();
        return;
    }
    if (auto* const memory = dynamic_cast<InMemoryPreferences*>(&store))
    {
        // SettingsPreferences::clearPreferences() for the store in memory.
        memory->reset();
        UnitGroup::resetDefaultUnits();
        memory->storeDefaultUnits();
        return;
    }
    unknownStore();
}

}  // namespace QtRocket::Gui::PreferencesStore
