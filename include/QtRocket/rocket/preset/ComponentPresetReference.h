#pragma once

#include <optional>
#include <string>

namespace QtRocket
{

/// What a design file says about the preset a component is based on, the four attributes of a
/// <preset> element, as texts:
///
///     <preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50FE, 30359"
///             digest="e9c3e97d32e96762f65e051d74cb9fd7"/>
///
/// OpenRocket has no such class: its loader looks the preset up and, when it finds none, warns
/// and forgets the element, so that a design loses its link to a preset the application does
/// not know as soon as it is saved again. QtRocket keeps the element's attributes on the
/// component instead (RocketComponent::getUnresolvedPreset(), the user's decision of
/// 2026-10-08), for the saver to write back.
///
/// The texts are the file's own, unchanged and unchecked: the type need not name a
/// ComponentPresetType, the manufacturer is as the file spells it (a preset's own manufacturer
/// may be known under another name), and the digest need not be one. A file always gives a
/// manufacturer and a part number where it gives a reference at all (without either, the
/// loader ignores the element as OpenRocket does); the type and the digest are optional, as
/// they are for OpenRocket's loader, which warns of a missing one and goes on.
struct ComponentPresetReference
{
    /// The attribute "type" (the name of a ComponentPresetType constant in a file OpenRocket
    /// wrote: "BODY_TUBE"), or nullopt when the element has none.
    std::optional<std::string> type;
    /// The attribute "manufacturer".
    std::string manufacturer;
    /// The attribute "partno".
    std::string partNo;
    /// The attribute "digest" (ComponentPreset::getDigest() of the preset that was saved), or
    /// nullopt when the element has none.
    std::optional<std::string> digest;

    /// Whether the four texts are the same, each compared exactly.
    [[nodiscard]] friend bool operator==(const ComponentPresetReference&,
                                         const ComponentPresetReference&) = default;
};

}  // namespace QtRocket
