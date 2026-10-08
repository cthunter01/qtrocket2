#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class ComponentPreset;
class DocumentLoadingContext;
class RocketComponent;

/// Bases a component on the preset that `<preset type="BODY_TUBE" manufacturer="Estes"
/// partno="BT-50FE, 30359" digest="e9c3e97d32e96762f65e051d74cb9fd7"/>` names, and loads the
/// preset's values into it (OpenRocket's file/openrocket/importt/ComponentPresetSetter). The
/// element's text is not read.
///
/// set() does, in this order (<name> is the component's name at that moment):
/// - No attribute "manufacturer": the warning "Invalid ComponentPreset for component <name>, no
///   manufacturer specified.  Ignored" (two blanks before "Ignored"), and nothing is loaded.
/// - No attribute "partno": "Invalid ComponentPreset for component <name>, no partno specified.
///    Ignored" (two blanks again), and nothing is loaded.
/// - No attribute "digest": the warning "Invalid ComponentPreset for component <name>, no digest
///   specified.", and the setter goes on.
/// - No attribute "type": the warning "Invalid ComponentPreset for component <name>, no type
///   specified.", and the setter goes on.
/// - The preset is the first one of ComponentPresetDatabase::find(manufacturer, partno) whose
///   digest is the file's, compared exactly. The database is the one of the context
///   (DocumentLoadingContext::getComponentPresetDatabase(); Java: the application's
///   ComponentPresetDao); none, or an empty one, finds nothing. No such preset: the warning "No
///   matching ComponentPreset for component <name> found matching <manufacturer> <partno>", and
///   nothing is loaded. So an element without a digest never finds a preset, and its two
///   warnings are followed by this third one.
/// - Else the preset is loaded (RocketComponent::loadPreset(): the preset's values replace the
///   component's, the component co-owns the preset, and a material of the document that the
///   preset brings is announced to the document) and the component gets back the name it had,
///   because loading may change it (a parachute takes its preset's description).
///
/// The attribute "type" is read for its warning only: it is not compared with the preset's type
/// or the component's. A body tube takes a nose cone preset that is found, as far as its values
/// go, as in OpenRocket.
///
/// The elements that follow <preset> in a file set the component's own values, which may differ
/// from the preset's, and most setters of a component drop its preset link when they change a
/// value (RocketComponent::clearPreset()). The handler of a component's parameters therefore
/// brackets the component's elements with RocketComponent::setIgnorePresetClearing(true) and
/// (false), as OpenRocket's ComponentParameterHandler does. Without that bracket the first
/// element that changes a value takes the link away again (in the example designs: the
/// <material> of the body tube, whose group differs from the preset's), with no warning.
///
/// Java's lookup also has a branch that would take the first preset of the file's type when no
/// digest matches, and a warning "ComponentPreset for component <name> has wrong digest". The
/// branch can never be taken (it asks for a preset to have been found already), so the warning
/// never appears; neither is here (decision L11 of the loader).
///
/// Deviations from OpenRocket:
/// - The function the setter is made with loads the preset (Java: the method loadPreset() and
///   the extra parameters of the setter's second constructor). The table's function for a
///   parachute passes PresetLoadOptions{.allowAutoRadius = false}, Java's `false`.
/// - A parachute or streamer is not given a preset whose material it would take and that is
///   not a surface material, nor a parachute one whose line material it would take and that is
///   not a line material: the warning Warning::kFileInvalidParameter, and nothing is loaded.
///   OpenRocket dies there of a ClassCastException, after it has set the length (a body tube
///   preset named in a <parachute> or <streamer> element); RecoveryDevice::loadFromPreset() and
///   Parachute::loadFromPreset() have a BugError there (decisions D9, L4). A device takes a
///   preset's material only when its text ("name (density)", Material::toString()) is longer
///   than 12 characters; a preset with a shorter one is loaded whatever the material's type,
///   and the device has its default material, as in OpenRocket.
/// - A rocket that is frozen (Rocket::freeze()) loads no preset: the same warning. No loader
///   freezes the rocket, and RocketComponent::loadPreset() has a BugError for it where
///   OpenRocket reports an error and goes on.
class ComponentPresetSetter final : public Setter
{
public:
    /// What loads the preset into the component (Java: the setter method, loadPreset(), with
    /// the setter's extra parameters).
    using LoadFunction =
        std::function<void(RocketComponent&, std::shared_ptr<const ComponentPreset>)>;

    /// @throws BugError when @p load is empty.
    explicit ComponentPresetSetter(LoadFunction load);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    LoadFunction m_load;
};

}  // namespace QtRocket
