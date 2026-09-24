#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class MaterialStorage;

/// Builds ComponentPresets from property maps (OpenRocket's ComponentPresetFactory): checks that
/// the properties a preset of its type needs are there, completes the derived ones and computes
/// the digest.
///
/// Every preset needs a manufacturer, a part number and a type; without a type nothing else is
/// checked. Then, per type:
/// - BODY_TUBE, TUBE_COUPLER, LAUNCH_LUG, CENTERING_RING, ENGINE_BLOCK: a length, and two of the
///   outer diameter, inner diameter and thickness ("Preset dimensions underspecified" otherwise,
///   which ends the checks); the missing one is computed and all three are stored again (from
///   the outer and inner diameters when all three are given, so the thickness is recomputed).
///   A mass becomes a material: the given material's name (or "TubeCustom",
///   "CenteringRingCustom", "EngineBlockCustom") with density mass / (pi (ro^2 - ri^2) length),
///   looked up as a BULK material in @p materials (MaterialStorage::findMaterial, which gives a
///   new user-defined material when none matches).
/// - BULK_HEAD: a length and an outer diameter; a mass becomes a "BulkHeadCustom" material over
///   the solid cylinder (the inner diameter counts when given).
/// - NOSE_CONE: a length, a shape and an aft outer diameter. TRANSITION: a length, an aft and a
///   fore outer diameter. RAIL_BUTTON: a height, outer and inner diameters, flange and base
///   heights. For these three a mass becomes a material over the volume of the component loaded
///   from the preset (NoseCone, Transition, RailButton.getComponentVolume()).
/// - STREAMER: a length and a width. PARACHUTE: a diameter, a line count and a line length.
///
/// The properties are not otherwise checked: a negative or NaN dimension passes, as in
/// OpenRocket. Only the density conversion reads the other properties (a material's name).
///
/// Deviation: where OpenRocket needs the volume of a tube without its length (a tube, or a
/// bulkhead with a mass, whose length or outer diameter is missing), its get() throws a
/// BugException out of create(); here the problems collected so far are returned, and they
/// already name the missing key ("No Length specified").
///
/// Deferred to the rocket components (they need NoseCone, Transition and RailButton, which are
/// not ported yet): the density of a NOSE_CONE, TRANSITION or RAIL_BUTTON preset with a mass.
/// Until then such a preset is refused with the problem "Mass of a <TYPE> preset needs the
/// <TYPE> component, which is not ported yet".
class ComponentPresetFactory
{
public:
    /// The problems create() collects (OpenRocket's InvalidComponentPresetException, whose
    /// message is always "Invalid preset specification.").
    struct InvalidPreset
    {
        /// The messages, in the order found, e.g. "No Length specified".
        std::vector<std::string> errors;
        /// The keys whose values are missing, in the order found.
        std::vector<AnyTypedKey> invalidParameters;

        /// True when there is any problem (hasProblems()).
        [[nodiscard]] bool hasProblems() const noexcept
        {
            return !errors.empty() || !invalidParameters.empty();
        }

        /// The larger of the two counts (problemCount()).
        [[nodiscard]] std::size_t problemCount() const noexcept
        {
            return errors.size() > invalidParameters.size() ? errors.size()
                                                            : invalidParameters.size();
        }

        /// ErrorCode::INVALID_ARGUMENT with the message "Invalid preset specification." followed
        /// by the errors, each after a space and ending with ';' but the last ("Invalid preset
        /// specification. No Manufacturer specified; No PartNo specified").
        [[nodiscard]] Error toError() const;
    };

    ComponentPresetFactory() = delete;

    /// ComponentPresetFactory.create(props): the preset, or the problems found.
    [[nodiscard]] static std::expected<ComponentPreset, InvalidPreset> tryCreate(
        const TypedPropertyMap& props, const MaterialStorage& materials);

    /// tryCreate() with the problems as an Error (InvalidPreset::toError()).
    [[nodiscard]] static Result<ComponentPreset> create(const TypedPropertyMap& props,
                                                        const MaterialStorage&  materials);
};

}  // namespace QtRocket
