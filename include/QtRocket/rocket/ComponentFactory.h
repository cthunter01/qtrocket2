#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket
{

class MaterialStorage;
class Preferences;
class RocketComponent;

/// Makes the components the user adds to a design as OpenRocket makes them: what one of the add
/// buttons of its design window creates (swing's ComponentAddButtons; ComponentButton
/// .fireActionPerformed() calls the no-argument constructor of the button's class), with the
/// values Java's constructors give it, its default materials taken from the application's
/// preferences included.
///
/// OpenRocket's constructors ask the global application preferences for the materials of the
/// new component (ApplicationPreferences.getDefaultComponentMaterial()): ExternalComponent's
/// for its material (BULK, the component's class), FinSet's for its fillets (BULK, the class),
/// StructuralComponent's (BULK, the class), ShockCord's (LINE, ShockCord.class),
/// RecoveryDevice's for its canopy and the default that a preset without a usable material
/// leaves (SURFACE, RecoveryDevice.class whatever the device), and Parachute's for its lines
/// and their default (LINE, Parachute.class). rocket/ has no global preferences (decision D2:
/// what Java asks the application for is handed in), so a component of this library is
/// constructed with the built-in defaults those lookups fall back on ("Cardboard", "Ripstop
/// nylon", "Elastic cord (round 2 mm, 1/16 in)"; a rail button "Delrin"), and
/// applyDefaultMaterials() completes the constructor from the preferences and the material
/// database the caller passes. Nothing else of Java's constructors reads the preferences.
///
/// The two steps are separate functions, and create() does both:
/// - createWithBuiltinMaterials(): the constructor of the kind's class, nothing more;
/// - applyDefaultMaterials(): the rest of Java's constructor, for a component that is not in a
///   tree yet. The .ork loader (ComponentHandler) calls only this one, on the component that
///   DocumentConfig::createComponent() made for an element, before it attaches it;
/// - create(): what an add button makes. The GUI's add buttons (Milestone 2, run 2) call it with
///   the application's Preferences and MaterialStorage (GuiServices), then do what
///   ComponentButton does with the component: the undo position "Add <getComponentName()>"
///   (OpenRocketDocument::addUndoPosition()), parent.addChild() at the button's position, the
///   new component selected alone, and its configuration dialog opened.
///
/// The application must set its preferences up as OpenRocket's start does
/// (SwingStartup: loadDefaultComponentMaterials() in material/MaterialPreferences.h: "Balsa"
/// for FinSet, "Polystyrene" for NoseCone): without it a new nose cone and new fins are
/// Cardboard. The material database must hold the built-in materials (addBuiltinMaterials()),
/// as OpenRocket's databases always do. Nothing here is global and nothing is cached: every
/// call reads the preferences it is given.
///
/// OpenRocket's buttons make no typed mass component: the MassComponent button makes one of the
/// default type, MASSCOMPONENT, as every other way to make one does here.
///
/// The defaults of every kind and the materials of three sets of preferences are compared with
/// OpenRocket's in tests/core/rocket/ComponentFactoryTests.cpp, and a design of 25 clicks saves as
/// OpenRocket's in tests/core/file/openrocket/add_buttons_save_tests.cpp (the reference data:
/// tests/data/component-factory/, tools/openrocket-component-factory/).
class ComponentFactory
{
public:
    ComponentFactory() = delete;

    /// Whether the factory makes components of @p kind: every kind but ROCKET. (OpenRocket's
    /// add buttons make the other 21; a rocket is made with its document,
    /// OpenRocketDocumentFactory.)
    [[nodiscard]] static constexpr bool isCreatable(ComponentKind kind) noexcept
    {
        return kind != ComponentKind::ROCKET;
    }

    /// A new component of @p kind as the no-argument constructor of its class makes it, with the
    /// built-in default materials (see the class comment): Java's `new X()` without the
    /// preferences. AXIAL_STAGE is an AxialStage, PARALLEL_STAGE a ParallelStage ("Booster
    /// Set"), POD_SET a PodSet.
    /// @throws BugError for ROCKET (isCreatable()).
    [[nodiscard]] static std::unique_ptr<RocketComponent> createWithBuiltinMaterials(
        ComponentKind kind);

    /// What an add button of OpenRocket makes for @p kind: createWithBuiltinMaterials(), then
    /// applyDefaultMaterials() with @p preferences and @p materials, which are read and not
    /// kept.
    /// @throws BugError for ROCKET, and when @p materials lacks a built-in material the lookup
    ///         falls back on (see applyDefaultMaterials()).
    [[nodiscard]] static std::unique_ptr<RocketComponent> create(ComponentKind          kind,
                                                                 const Preferences&     preferences,
                                                                 const MaterialStorage& materials);

    /// The rest of Java's constructor of @p component: the materials OpenRocket's constructors
    /// ask the application's preferences for (see the class comment), each
    /// getDefaultComponentMaterial(preferences, chain, type, materials) of
    /// material/MaterialPreferences.h, chain being the component's class chain
    /// (componentClassChain()) or the part of it the Java constructor asks for:
    /// - an external component: ExternalComponent::applyDefaultMaterial(), the material (BULK)
    ///   and, for a fin set, its fillets; a rail button is "Delrin" whatever the preferences say;
    /// - a structural component (the ring components): its material (BULK);
    /// - a shock cord: its material (LINE);
    /// - a recovery device: its canopy and its default material (SURFACE), asked for
    ///   RecoveryDevice, so that a default stored for Parachute or Streamer is not seen (the
    ///   class chain without its first class); a parachute also its lines and their default
    ///   (LINE, its own class chain).
    /// The nearest class of the chain that has a stored default decides, and one of another type
    /// than asked for gives the built-in default (no class further up is asked). Every other
    /// kind (the assemblies, a mass component, the rocket) has no material: nothing is done.
    ///
    /// @p component must not be in a tree, as a Java constructor's object is not: the external
    /// component's material is assigned directly, the others through their setters, which then
    /// reach no rocket, so that nothing is fired. A getter may have run on it before: the mass
    /// values a fin set or a body component (SymmetricComponent) cached are cleared with the
    /// assignment, so that its mass and CG follow the new material at once (the other kinds cache
    /// nothing their material decides). @p materials must hold the built-in materials
    /// the lookup falls back on ("Cardboard", "Ripstop nylon", "Elastic cord (round 2 mm, 1/16
    /// in)"; OpenRocket's databases always do).
    /// @throws BugError when @p component has a parent, and when @p materials lacks a fallback
    ///         material that a lookup needs (getDefaultComponentMaterial()).
    static void applyDefaultMaterials(RocketComponent& component, const Preferences& preferences,
                                      const MaterialStorage& materials);
};

}  // namespace QtRocket
