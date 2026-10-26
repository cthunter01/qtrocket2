#include "QtRocket/gui/binding/PresetBinding.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The preset type of the components of @p selection.
/// @throws BugError when @p selection is null or invalid, of several kinds, or its main component
///         takes no preset
[[nodiscard]] ComponentPresetType presetTypeOfSelection(
    const std::shared_ptr<ComponentSelection>& selection)
{
    if (selection == nullptr)
    {
        bug("A preset binding needs a component selection");
    }
    if (!selection->isAllOfOneKind())
    {
        bug("A preset binding needs selected components of one kind (Java: allSameType)");
    }
    const std::optional<ComponentPresetType> type = selection->getMain().getPresetType();
    if (!type.has_value())
    {
        bug("A preset binding needs a component that takes presets");
    }
    return *type;
}

}  // namespace

PresetBinding::PresetBinding(std::shared_ptr<ComponentSelection> selection,
                             OpenRocketDocument& document, const ComponentPresetDatabase& presets,
                             const Preferences& preferences)
  : Binding(std::move(selection)),
    m_document(&document),
    m_presets(&presets),
    m_preferences(&preferences),
    m_type(presetTypeOfSelection(getSelection()))
{
    if (&document.getRocket() != &getSelection()->getRocket())
    {
        bug("The preset binding's document is not the document of the selection's rocket");
    }
    readState();
}

PresetBinding::~PresetBinding() = default;

bool PresetBinding::readFavorites()
{
    // The keys are few and cheap to read; the database is walked again only when they changed.
    ComponentPresetFavorites::FavoriteKeys keys =
        ComponentPresetFavorites::getFavorites(*m_preferences, m_type);
    if (m_favoritesRead && keys == m_favoriteKeys)
    {
        return false;
    }
    std::vector<PresetPtr> favorites = m_presets->listForType(m_type, keys);
    const bool             changed   = !m_favoritesRead || favorites != m_favorites;
    m_favoriteKeys                   = std::move(keys);
    m_favorites                      = std::move(favorites);
    m_favoritesRead                  = true;
    return changed;
}

bool PresetBinding::readState()
{
    const ComponentPreset* value   = getSelection()->getMain().getPresetComponent();
    std::string            text    = value != nullptr ? value->toString() : std::string{};
    bool                   changed = value != m_value || text != m_valueText;
    m_value                        = value;
    m_valueText                    = std::move(text);
    if (readFavorites())
    {
        changed = true;
    }
    return changed;
}

int PresetBinding::valueIndex() const noexcept
{
    if (m_value == nullptr)
    {
        return -1;
    }
    const auto found = std::ranges::find_if(
        m_favorites, [this](const PresetPtr& favorite) { return favorite.get() == m_value; });
    if (found == m_favorites.end())
    {
        return -1;
    }
    return static_cast<int>(std::distance(m_favorites.begin(), found));
}

bool PresetBinding::choose(const PresetPtr& preset, std::string_view undoDescription)
{
    if (preset == nullptr)
    {
        bug("PresetBinding::choose() needs a preset (clear() is \"Custom\")");
    }
    if (preset->getType() != m_type)
    {
        bug("A preset of another type was given to a preset binding");
    }
    if (!isWidgetWriteAllowed())
    {
        return false;
    }
    // The selection's events (or its own refresh) update the cache and tell the listeners, one of
    // which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    // Java: document.addUndoPosition("Use Preset " + getComponentName()), then
    // component.loadPreset(preset), which forwards to the config listeners first.
    m_document->addUndoPosition(std::string{undoDescription});
    getSelection()->applyToEachUnfrozen(
        [&preset](RocketComponent& component) { component.loadPreset(preset); });
    return true;
}

bool PresetBinding::clear()
{
    if (!isWidgetWriteAllowed())
    {
        return false;
    }
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    // Java: component.clearPreset(), which forwards to the config listeners first.
    getSelection()->applyToEach([](RocketComponent& component) { component.clearPreset(); });
    return true;
}

void PresetBinding::reread()
{
    if (!isValid())
    {
        return;
    }
    if (readState())
    {
        fireChanged();
    }
}

}  // namespace QtRocket::Gui
