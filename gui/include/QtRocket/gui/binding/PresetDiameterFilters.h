#pragma once

#include <optional>

#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"

namespace QtRocket
{
class RocketComponent;
}

namespace QtRocket::Gui
{

/// The diameters the parts library's "Match fore diameter" and "Match aft diameter" boxes filter
/// a component's presets by (OpenRocket: ComponentPresetChooserDialog's constructor, which finds
/// the columns, and getFilterCheckboxes(), which finds the neighbours, swing
/// gui/dialogs/preset/ComponentPresetChooserDialog.java:99-123 and 311-357), what the caller of
/// UserPrompts::choosePreset() puts into its PresetRequest. Qt-free, so that the configuration
/// dialog can make the request (configdialog/ comes before the dialog's area, dialogs/).
///
/// The boxes exist for a symmetric component only (a nose cone, a transition, a body tube):
/// - "Match fore diameter" when the component has a previous symmetric component and its preset
///   type shows a fore diameter column (foreDiameterKey()): that component's aft diameter, where
///   it meets this one's fore end.
/// - "Match aft diameter" when it has a next symmetric component and its type shows an aft
///   diameter column (aftDiameterKey()): that component's fore diameter, where it meets this
///   one's aft end.
/// - A flipped nose cone's base, its preset's aft diameter, is at its fore end: its "aft" box
///   (named "Match fore diameter" too, flippedNoseCone) compares with the previous component.
///
/// Deviation from OpenRocket (bug 16 of the plan's section 10.3, fixed): for a flipped nose cone
/// OpenRocket takes the previous component's fore radius, the far end of that component, which is
/// the meeting diameter only when the component is a tube. Here it is that component's aft
/// diameter, where it meets the nose cone's base.
struct PresetDiameterFilters
{
    /// The "Match fore diameter" box's diameter in m, nullopt without the box.
    std::optional<double> foreDiameter;
    /// The "Match aft diameter" box's diameter in m (the base's for a flipped nose cone), nullopt
    /// without the box.
    std::optional<double> aftDiameter;
    /// The component is a flipped nose cone (NoseCone::isFlipped()): the aft box takes the fore
    /// box's text and tooltip, as in OpenRocket.
    bool flippedNoseCone = false;

    /// The boxes of @p component (see the struct comment); none for a component that is not
    /// symmetric or takes no presets.
    [[nodiscard]] static PresetDiameterFilters forComponent(const RocketComponent& component);

    /// The column of a preset table of @p type that the fore box filters: the last of
    /// OuterDiameter and ForeOuterDiameter (ComponentPreset::orderedKeyList()'s order) among the
    /// type's displayed columns (ComponentPreset::displayedColumns()), nullopt for none (Java:
    /// foreDiameterColumnIndex, -1).
    [[nodiscard]] static std::optional<AnyTypedKey> foreDiameterKey(ComponentPresetType type);

    /// The aft box's column: the last of OuterDiameter and AftOuterDiameter among the displayed
    /// columns (Java: aftDiameterColumnIndex).
    [[nodiscard]] static std::optional<AnyTypedKey> aftDiameterKey(ComponentPresetType type);
};

}  // namespace QtRocket::Gui
