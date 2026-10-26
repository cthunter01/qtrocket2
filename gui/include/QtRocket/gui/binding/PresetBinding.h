#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"

namespace QtRocket
{
class ComponentPreset;
class ComponentPresetDatabase;
class OpenRocketDocument;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The model of a component's preset combo box (Java: PresetModel, swing
/// gui/adaptors/PresetModel.java, the ComboBoxModel of RocketComponentConfig's preset combo):
/// the main component's preset and the favourite presets of its type, and the two writes, a
/// preset chosen (choose()) and "Custom" (clear()).
///
/// The favourites, favorites(), are the presets of the main component's preset type that the
/// database lists for the favourite keys of that type in the preferences, in the database's order
/// (ComponentPresetDatabase::listForType(type, ComponentPresetFavorites::getFavorites(
/// preferences, type)); Java: listForType(type, true)). The combo shows them after its "Custom"
/// item (Java's getElementAt(): NONE_SELECTED, then the presets). value() is the main component's
/// preset (Java: getSelectedItem(), NONE_SELECTED for none); it need not be a favourite.
///
/// choose() is Java's setSelectedItem() of a preset: one undo position "Use Preset <name>" (the
/// caller gives the text, see choose()), then RocketComponent::loadPreset() on every selected
/// component, the others first and the main component last (Java: loadPreset() forwards to the
/// config listeners before it loads its own component), through
/// ComponentSelection::applyToEachUnfrozen(): loadPreset() freezes the rocket itself, so it never
/// runs inside apply() (the plan's decision O-M3-6; RocketComponent::loadPreset() stays Java's).
/// Each component's load fires its own events (a merged change at its thaw, then
/// NONFUNCTIONAL_CHANGE), the others' first. The undo position comes first, outside any freeze and
/// outside every rocket slot. clear() is Java's setSelectedItem(NONE_SELECTED):
/// RocketComponent::clearPreset() on every selected component (ComponentSelection::apply(), one
/// merged event), without an undo position of its own (the dialog session's step holds it, as in
/// Java).
///
/// The binding caches the value and the favourites and re-reads both on every event of the
/// rocket (ComponentSelection's Phase::READ), telling its listeners when either changed (Java:
/// componentChanged() fires when the preset object changed). The favourites change without an
/// event when the parts library ticks one: the caller calls reread() after the parts library
/// closes (the plan's decision O-M3-12, no database signal; Java registers the PresetModel as a
/// database listener while its parts library is open, PresetModel.elementAdded()).
///
/// The selection must be of one kind of component that takes presets (Java shows the combo only
/// when checkAllClassesEqual() and getPresetType() != null, RocketComponentConfig.java:160). The
/// database and the preferences outlive the binding, and the database does not change while it
/// lives (the binding lists it again only when the favourite keys change; QtRocket loads its
/// presets once, at start); the document is the selection's rocket's.
///
/// Deviations from OpenRocket:
/// - choose() and clear() are widget writes: dropped while the binding notifies (a combo box
///   refreshing itself) and once the selection is invalid (MaterialBinding does the same). Java's
///   combo box never writes back what it is told.
/// - choose() changes the rocket only; Java's setSelectedItem() also moves the focus to the
///   dialog's first field (RocketComponentConfig.setFocusElement()), which the caller does here.
/// - The other components' events reach the rocket's listeners (ComponentSelection's deviations,
///   U3); Java drops the events of the config listeners and fires the main component's only. So
///   when only the others change (the main component has the preset chosen already), the
///   document sees the change and the session's undo step holds it; OpenRocket's document sees
///   nothing, and that change is in no undo step (probe: PresetModelProbe, "the main component's
///   preset chosen again").
/// - The favourites are read again on every event of the rocket and on reread(), not on the
///   database's events (O-M3-12): while OpenRocket's parts library is open, its combo box behind
///   it follows each tick at once (RocketComponentConfig.selectPreset() registers the PresetModel
///   as a database listener for the time the modal dialog is open); QtRocket's follows when the
///   parts library closes.
class PresetBinding final : public Binding
{
public:
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    /// A binding of the presets of the components of @p selection, offering the favourites of
    /// @p presets as @p preferences names them; @p document adds the undo positions.
    /// @throws BugError when @p selection is null or invalid, its components are not all of one
    ///         kind (ComponentSelection::isAllOfOneKind()), the main component takes no preset
    ///         (RocketComponent::getPresetType()), or @p document is not the document of the
    ///         selection's rocket
    PresetBinding(std::shared_ptr<ComponentSelection> selection, OpenRocketDocument& document,
                  const ComponentPresetDatabase& presets, const Preferences& preferences);

    ~PresetBinding() override;
    PresetBinding(const PresetBinding&)            = delete;
    PresetBinding& operator=(const PresetBinding&) = delete;
    PresetBinding(PresetBinding&&)                 = delete;
    PresetBinding& operator=(PresetBinding&&)      = delete;

    /// The preset type of the main component (Java: component.getPresetType()).
    [[nodiscard]] ComponentPresetType getType() const noexcept { return m_type; }

    /// The cached favourite presets of the type, in the database's order (see the class
    /// comment); the combo box's items after "Custom".
    [[nodiscard]] const std::vector<PresetPtr>& favorites() const noexcept { return m_favorites; }

    /// The cached preset of the main component, or nullptr for none ("Custom"). Compare it with
    /// favorites() by identity; it points into a preset the main component holds, so dereference
    /// it only while the selection is valid and outside a freeze of the rocket (a clearPreset()
    /// whose event has not been delivered yet may have let go of it): valueText() is the text to
    /// show.
    [[nodiscard]] const ComponentPreset* value() const noexcept { return m_value; }

    /// The text of value() in the combo box (Java: ComponentPreset.toString(), the part number),
    /// read with it; empty for none.
    [[nodiscard]] const std::string& valueText() const noexcept { return m_valueText; }

    /// The index of value() in favorites(), or -1 when the main component has no preset or one
    /// that is not a favourite (Java's combo then shows it closed, with no item of its list
    /// selected).
    [[nodiscard]] int valueIndex() const noexcept;

    /// Java's setSelectedItem(preset): adds the undo position @p undoDescription (Java:
    /// "Use Preset " + getComponentName(), English in every language: "Use Preset Body Tube";
    /// the caller passes its tr() text), then loads @p preset into every selected component (see
    /// the class comment), also when it is the main component's preset already (Java does too:
    /// the others then get it, and the position renames a clean step). False, with nothing done,
    /// while firing and once the selection is invalid.
    /// @throws BugError when @p preset is null (clear() is "Custom") or not of getType()
    bool choose(const PresetPtr& preset, std::string_view undoDescription);

    /// Java's setSelectedItem(NONE_SELECTED), "Custom": RocketComponent::clearPreset() on every
    /// selected component, no undo position. False, with nothing done, while firing and once
    /// the selection is invalid.
    bool clear();

    /// Reads the favourites and the value again and tells the listeners when they changed: after
    /// the parts library, which may have ticked favourites, closes. Nothing happens once the
    /// selection is invalid.
    void reread();

protected:
    bool readState() override;

private:
    /// Reads the favourites of m_type from the preferences and, when their keys changed, from
    /// the database; whether favorites() changed.
    bool readFavorites();

    OpenRocketDocument*            m_document;
    const ComponentPresetDatabase* m_presets;
    const Preferences*             m_preferences;
    ComponentPresetType            m_type;
    /// The favourite keys m_favorites was listed for, once m_favoritesRead.
    ComponentPresetFavorites::FavoriteKeys m_favoriteKeys;
    std::vector<PresetPtr>                 m_favorites;
    bool                                   m_favoritesRead{false};
    const ComponentPreset*                 m_value{nullptr};
    std::string                            m_valueText;
};

}  // namespace QtRocket::Gui
