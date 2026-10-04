#include "QtRocket/rocket/preset/ComponentPresetFactory.h"

#include <concepts>
#include <cstddef>
#include <expected>
#include <format>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

using Preset        = ComponentPreset;
using InvalidPreset = ComponentPresetFactory::InvalidPreset;

/// addInvalidParameter(key, message)
void addInvalidParameter(InvalidPreset& problems, const AnyTypedKey& key, std::string message)
{
    problems.invalidParameters.push_back(key);
    problems.errors.push_back(std::move(message));
}

/// The steps of ComponentPresetFactory.java's private methods, applied to the properties of the
/// preset being made (Java's preset.put() and has() calls; tryCreate() hands the result to the
/// preset), collecting the problems found. Each make*() returns false where Java throws the
/// exception at once (checkDiametersAndThickness()) or where its get() would throw a
/// BugException for a missing length or diameter (see computeVolumeOfTube()).
class PresetBuilder
{
public:
    /// Starts from @p properties and the problems found so far; @p materials must outlive the
    /// builder.
    PresetBuilder(TypedPropertyMap properties, InvalidPreset problems,
                  const MaterialStorage& materials) noexcept
      : m_properties(std::move(properties)),
        m_problems(std::move(problems)),
        m_materials(&materials)
    {
    }

    /// The properties as completed.
    [[nodiscard]] const TypedPropertyMap& properties() const noexcept { return m_properties; }

    /// Hands the problems found over.
    [[nodiscard]] InvalidPreset takeProblems() noexcept { return std::move(m_problems); }

    [[nodiscard]] bool hasProblems() const noexcept { return m_problems.hasProblems(); }

    /// makeBodyTube(), makeCenteringRing() and makeEngineBlock(), which differ only in the
    /// default material name.
    bool makeTube(std::string_view defaultMaterialName)
    {
        checkRequiredFields({Preset::kLength});

        if (!checkDiametersAndThickness())
        {
            return false;
        }

        const std::optional<double> volume = computeVolumeOfTube();
        if (!volume)
        {
            return false;
        }

        // Need to translate Mass to Density.
        if (has(Preset::kMass))
        {
            putMaterial(materialName(defaultMaterialName), value(Preset::kMass) / *volume);
        }
        return true;
    }

    /// makeBulkHead()
    bool makeBulkHead()
    {
        checkRequiredFields({Preset::kLength, Preset::kOuterDiameter});

        if (has(Preset::kMass))
        {
            // compute a density for this component
            const double                mass   = value(Preset::kMass);
            const std::optional<double> volume = computeVolumeOfTube();
            if (!volume)
            {
                return false;
            }
            putMaterial(materialName("BulkHeadCustom"), mass / *volume);
        }
        return true;
    }

    /// makeNoseCone(), makeTransition() and makeRailButton(): a mass becomes a material over
    /// @p componentVolume(properties), the volume of a component of the preset's type with the
    /// properties loaded (Java loads the preset being made into a new NoseCone, Transition or
    /// RailButton).
    template <std::invocable<const TypedPropertyMap&> ComponentVolume>
    void makeShaped(std::initializer_list<AnyTypedKey> requiredKeys,
                    std::string_view defaultMaterialName, const ComponentVolume& componentVolume)
    {
        checkRequiredFields(requiredKeys);

        if (has(Preset::kMass))
        {
            // compute a density for this component
            const double mass    = value(Preset::kMass);
            const double density = mass / componentVolume(m_properties);
            putMaterial(materialName(defaultMaterialName), density);
        }
    }

    /// makeStreamer() and makeParachute(), and the material types (no Java counterpart): the
    /// MATERIAL must be a SURFACE material and a parachute's LINE_MATERIAL a LINE one.
    void makeRecoveryDevice(std::initializer_list<AnyTypedKey> requiredKeys, bool hasLines)
    {
        checkRequiredFields(requiredKeys);
        checkMaterialType(Preset::kMaterial, Material::Type::SURFACE);
        if (hasLines)
        {
            checkMaterialType(Preset::kLineMaterial, Material::Type::LINE);
        }
    }

private:
    [[nodiscard]] bool has(const AnyTypedKey& key) const noexcept
    {
        return m_properties.containsKey(key);
    }

    /// The value of a key has() found.
    template <TypedValueType T>
    [[nodiscard]] const T& value(const TypedKey<T>& key) const
    {
        const T* found = m_properties.get(key);
        QTROCKET_ASSERT(found != nullptr);
        return *found;
    }

    /// checkRequiredFields(): "No <name> specified" for each absent key.
    void checkRequiredFields(std::initializer_list<AnyTypedKey> keys)
    {
        for (const AnyTypedKey& key : keys)
        {
            if (!has(key))
            {
                addInvalidParameter(m_problems, key,
                                    "No " + std::string(key.getName()) + " specified");
            }
        }
    }

    /// '<key> "<material name>" is not a <type> material' when @p key holds a material of
    /// another type than @p type.
    void checkMaterialType(const TypedKey<Material>& key, Material::Type type)
    {
        if (has(key) && value(key).getType() != type)
        {
            addInvalidParameter(m_problems, key,
                                std::format(R"({} "{}" is not a {} material)", key.getName(),
                                            value(key).getName(), toString(type)));
        }
    }

    /// checkDiametersAndThickness(): two of the outer diameter, inner diameter and thickness give
    /// the third; all three are stored.
    bool checkDiametersAndThickness()
    {
        // Need to verify contains 2 of OD, thickness, ID. Compute the third.
        const bool hasOd        = has(Preset::kOuterDiameter);
        const bool hasId        = has(Preset::kInnerDiameter);
        const bool hasThickness = has(Preset::kThickness);

        double outerRadius = 0;
        double innerRadius = 0;
        double thickness   = 0;

        if (hasOd)
        {
            outerRadius = value(Preset::kOuterDiameter) / 2.0;
            if (hasId)
            {
                innerRadius = value(Preset::kInnerDiameter) / 2.0;
                thickness   = outerRadius - innerRadius;
            }
            else if (hasThickness)
            {
                thickness   = value(Preset::kThickness);
                innerRadius = outerRadius - thickness;
            }
            else
            {
                m_problems.errors.emplace_back("Preset dimensions underspecified");
                return false;
            }
        }
        else
        {
            if (!hasId || !hasThickness)
            {
                m_problems.errors.emplace_back("Preset dimensions underspecified");
                return false;
            }
            innerRadius = value(Preset::kInnerDiameter) / 2.0;
            thickness   = value(Preset::kThickness);
            outerRadius = innerRadius + thickness;
        }

        m_properties.put(Preset::kOuterDiameter, outerRadius * 2.0);
        m_properties.put(Preset::kInnerDiameter, innerRadius * 2.0);
        m_properties.put(Preset::kThickness, thickness);
        return true;
    }

    /// computeVolumeOfTube(): pi (ro^2 - ri^2) l, ri 0 without an inner diameter. Deviation:
    /// nullopt without an outer diameter or a length, where OpenRocket's get() throws a
    /// BugException; the missing keys are then among the problems already.
    [[nodiscard]] std::optional<double> computeVolumeOfTube() const
    {
        if (!has(Preset::kOuterDiameter) || !has(Preset::kLength))
        {
            return std::nullopt;
        }
        const double outerRadius = value(Preset::kOuterDiameter) / 2.0;
        const double innerRadius =
            has(Preset::kInnerDiameter) ? value(Preset::kInnerDiameter) / 2.0 : 0.0;
        const double length = value(Preset::kLength);
        return std::numbers::pi * ((outerRadius * outerRadius) - (innerRadius * innerRadius)) *
               length;
    }

    /// The name of the material a mass turns into: the given material's, else @p defaultName.
    [[nodiscard]] std::string materialName(std::string_view defaultName) const
    {
        if (has(Preset::kMaterial))
        {
            return value(Preset::kMaterial).getName();
        }
        return std::string(defaultName);
    }

    /// Databases.findMaterial(BULK, name, density), stored as the preset's material.
    void putMaterial(const std::string& name, double density)
    {
        m_properties.put(Preset::kMaterial,
                         m_materials->findMaterial(Material::Type::BULK, name, density));
    }

    TypedPropertyMap       m_properties;
    InvalidPreset          m_problems;
    const MaterialStorage* m_materials;
};

}  // namespace

Error ComponentPresetFactory::InvalidPreset::toError() const
{
    std::string message = "Invalid preset specification.";
    for (std::size_t i = 0; i < errors.size(); i++)
    {
        message += (i == 0 ? " " : "; ");
        message += errors[i];
    }
    return Error{.code    = ErrorCode::INVALID_ARGUMENT,
                 .message = std::move(message),
                 .where   = std::source_location::current()};
}

std::expected<ComponentPreset, ComponentPresetFactory::InvalidPreset>
ComponentPresetFactory::tryCreate(const TypedPropertyMap& props, const MaterialStorage& materials)
{
    InvalidPreset problems;

    // First do validation.
    if (!props.containsKey(Preset::kManufacturer))
    {
        addInvalidParameter(problems, Preset::kManufacturer, "No Manufacturer specified");
    }
    if (!props.containsKey(Preset::kPartNo))
    {
        addInvalidParameter(problems, Preset::kPartNo, "No PartNo specified");
    }
    const ComponentPresetType* type = props.get(Preset::kType);
    if (type == nullptr)
    {
        addInvalidParameter(problems, Preset::kType, "No Type specified");
        // We can't do anything else without TYPE so throw immediately.
        return std::unexpected(std::move(problems));
    }

    // The volume of a new NoseCone, Transition or RailButton with the properties loaded as a
    // preset (lambdas here, where the preset's private constructor is accessible).
    const auto noseConeVolume = [](const TypedPropertyMap& properties) {
        ComponentPreset preset;
        preset.putAll(properties);
        NoseCone noseCone;
        noseCone.loadPreset(&preset);
        return noseCone.getComponentVolume();
    };
    const auto transitionVolume = [](const TypedPropertyMap& properties) {
        ComponentPreset preset;
        preset.putAll(properties);
        Transition transition;
        transition.loadPreset(&preset);
        return transition.getComponentVolume();
    };
    const auto railButtonVolume = [](const TypedPropertyMap& properties) {
        ComponentPreset preset;
        preset.putAll(properties);
        RailButton railButton;
        railButton.loadPreset(&preset);
        return railButton.getComponentVolume();
    };

    // Should check for various bits of each of the types.
    PresetBuilder builder(props, std::move(problems), materials);
    bool          complete = true;
    switch (*type)
    {
        case ComponentPresetType::BODY_TUBE:
            complete = builder.makeTube("TubeCustom");
            break;
        case ComponentPresetType::NOSE_CONE:
            builder.makeShaped({Preset::kLength, Preset::kShape, Preset::kAftOuterDiameter},
                               "NoseConeCustom", noseConeVolume);
            break;
        case ComponentPresetType::TRANSITION:
            builder.makeShaped(
                {Preset::kLength, Preset::kAftOuterDiameter, Preset::kForeOuterDiameter},
                "TransitionCustom", transitionVolume);
            break;
        case ComponentPresetType::BULK_HEAD:
            complete = builder.makeBulkHead();
            break;
        case ComponentPresetType::TUBE_COUPLER:
            // For now TUBE_COUPLER is the same as BODY_TUBE
            complete = builder.makeTube("TubeCustom");
            break;
        case ComponentPresetType::CENTERING_RING:
            complete = builder.makeTube("CenteringRingCustom");
            break;
        case ComponentPresetType::ENGINE_BLOCK:
            complete = builder.makeTube("EngineBlockCustom");
            break;
        case ComponentPresetType::LAUNCH_LUG:
            // Same processing as BODY_TUBE
            complete = builder.makeTube("TubeCustom");
            break;
        case ComponentPresetType::RAIL_BUTTON:
            builder.makeShaped({Preset::kHeight, Preset::kOuterDiameter, Preset::kInnerDiameter,
                                Preset::kFlangeHeight, Preset::kBaseHeight},
                               "RailButtonCustom", railButtonVolume);
            break;
        case ComponentPresetType::STREAMER:
            builder.makeRecoveryDevice({Preset::kLength, Preset::kWidth}, false);
            break;
        case ComponentPresetType::PARACHUTE:
            builder.makeRecoveryDevice({Preset::kDiameter, Preset::kLineCount, Preset::kLineLength},
                                       true);
            break;
    }

    if (!complete || builder.hasProblems())
    {
        return std::unexpected(builder.takeProblems());
    }

    ComponentPreset preset;
    preset.putAll(builder.properties());
    preset.computeDigest();

    return preset;
}

Result<ComponentPreset> ComponentPresetFactory::create(const TypedPropertyMap& props,
                                                       const MaterialStorage&  materials)
{
    return tryCreate(props, materials).transform_error(&InvalidPreset::toError);
}

}  // namespace QtRocket
