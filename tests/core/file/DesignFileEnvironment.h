#pragma once

// The environment in which the tests load whole design files as OpenRocket's own tests and the
// golden harness load them: a motor database behind a DatabaseMotorFinder, the component presets
// of the example designs, the preference store of OpenRocket's test set-up, OpenRocket's
// materials and the bundled simulation extensions. Test-only.

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "motor/TestMotorDatabase.h"
#include "rocket/preset/ExamplePresets.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// What a GeneralRocketLoader of a test is made with (context()) to load a design file as
/// OpenRocket loads it with ServicesForTesting, its motor database and its preset database:
/// - the motors of a design are looked up in a motor database, exactly as DatabaseMotorFinder
///   does it: the bundled one (bundledMotorDatabase(), read once per test process) unless the
///   test hands in another, which must outlive the environment;
/// - the component presets are the six the example designs refer to
///   (makeExamplePresetDatabase()), or none: OpenRocket's own database holds 5228, of which a
///   design of tests/data/ork or data/examples can find these six and six more that QtRocket
///   cannot have before it reads .orc files (Milestone 3);
/// - the preference store holds what OpenRocket's tests read from theirs
///   (storeJavaTestPreferences()) and shows a motor by its common name
///   (setMotorNameColumn(false): ServicesForTesting answers false for every flag). It names NO
///   default material for any component class, as OpenRocket's test preferences name none: a
///   component whose element has no <material> keeps the built-in default;
/// - the application's materials are OpenRocket's own (addBuiltinMaterials()), so that a
///   material of a file that is one of them is no document material;
/// - the extension providers are the bundled ones (SimulationExtensionRegistry::bundled()).
///
/// The members are declared so that the preference store outlives everything, the documents a
/// test loads into its own variables excepted: declare those after the environment (the
/// simulations of a document keep the store).
class DesignFileEnvironment
{
public:
    /// Which component presets a load finds.
    enum class Presets
    {
        /// None: every <preset> of a design gives OpenRocket's warning.
        NONE,
        /// The six presets of the example designs.
        EXAMPLES,
    };

    /// An environment with the bundled motor database.
    explicit DesignFileEnvironment(Presets presets = Presets::EXAMPLES)
      : DesignFileEnvironment(presets, bundledMotorDatabase())
    {
    }

    /// An environment whose motors come from @p motors, which must outlive it.
    DesignFileEnvironment(Presets presets, const MotorDatabase& motors)
      : m_presets(presets == Presets::EXAMPLES ? makeExamplePresetDatabase()
                                               : ComponentPresetDatabase()),
        m_motorFinder(motors),
        m_extensions(SimulationExtensionRegistry::bundled())
    {
        storeJavaTestPreferences(m_preferences);
        m_preferences.setMotorNameColumn(false);
        addBuiltinMaterials(m_materials);
        m_context.setMotorFinder(&m_motorFinder);
        m_context.setApplicationMaterials(&m_materials);
        m_context.setPreferences(&m_preferences);
        m_context.setComponentPresetDatabase(&m_presets);
        m_context.setSimulationExtensionRegistry(&m_extensions);
    }
    DesignFileEnvironment(Presets presets, const MotorDatabase&& motors) = delete;
    ~DesignFileEnvironment()                                             = default;

    // The context points at the members.
    DesignFileEnvironment(const DesignFileEnvironment&)            = delete;
    DesignFileEnvironment& operator=(const DesignFileEnvironment&) = delete;
    DesignFileEnvironment(DesignFileEnvironment&&)                 = delete;
    DesignFileEnvironment& operator=(DesignFileEnvironment&&)      = delete;

    /// The context a GeneralRocketLoader is made with.
    [[nodiscard]] const DocumentLoadingContext& context() const noexcept { return m_context; }
    /// The preference store of the loads, which the simulations of the loaded documents keep.
    [[nodiscard]] InMemoryPreferences& preferences() noexcept { return m_preferences; }

private:
    InMemoryPreferences         m_preferences;
    MaterialStorage             m_materials;
    ComponentPresetDatabase     m_presets;
    DatabaseMotorFinder         m_motorFinder;
    SimulationExtensionRegistry m_extensions;
    DocumentLoadingContext      m_context;
};

}  // namespace QtRocket::Test
