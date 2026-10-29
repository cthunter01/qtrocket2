#pragma once

// The environment in which tools/openrocket-gui-reference loaded the design a scenario saved
// (tests/data/goldens/gui/<scenario>/state.json, "loaded"), for the tests that load a save of a
// scenario and compare with it: from_scratch_reference_golden_tests.cpp and
// flight_configurations_reference_golden_tests.cpp (OpenRocket's saves) and the GUI's exit tests
// (tests/gui/exit/, QtRocket's). Test-only.

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Test
{

/// OpenRocket's GUI after its start-up (SwingStartup), as the tool loads the saved design in it:
/// a DesignFileEnvironment with the databases the scenario's start-up loaded, with the default
/// component materials the start-up stores (loadDefaultComponentMaterials(): Balsa for fin sets,
/// Polystyrene for nose cones). A new component of the loader starts with them, so a <material>
/// that names the same one changes nothing and fires no event: the events of the load count it.
class GuiReferenceEnvironment
{
public:
    /// The databases of the start-up.
    enum class Databases
    {
        /// No component preset and no motor: the scenario of version 1 (from-scratch), whose
        /// start-up bypasses both (QtRocket's start-up of Milestone 2 had none either, and the
        /// design names none).
        NONE,
        /// The bundled motors (bundledMotorDatabase(): 1458 sets of 1588 motors) and component
        /// presets (Presets::BUNDLED: the 5228 of data/components): the scenarios of version 2,
        /// whose start-up loads OpenRocket's bundled databases (state.json's "startup").
        BUNDLED,
    };

    explicit GuiReferenceEnvironment(Databases databases = Databases::NONE)
      : m_environment(databases == Databases::BUNDLED ? DesignFileEnvironment::Presets::BUNDLED
                                                      : DesignFileEnvironment::Presets::NONE,
                      databases == Databases::BUNDLED ? bundledMotorDatabase() : m_motors)
    {
        addBuiltinMaterials(m_materials);
        loadDefaultComponentMaterials(m_environment.preferences(), m_materials);
    }
    ~GuiReferenceEnvironment() = default;

    // The environment points at the members.
    GuiReferenceEnvironment(const GuiReferenceEnvironment&)            = delete;
    GuiReferenceEnvironment& operator=(const GuiReferenceEnvironment&) = delete;
    GuiReferenceEnvironment(GuiReferenceEnvironment&&)                 = delete;
    GuiReferenceEnvironment& operator=(GuiReferenceEnvironment&&)      = delete;

    [[nodiscard]] DesignFileEnvironment& get() noexcept { return m_environment; }

private:
    /// The empty motor database of Databases::NONE.
    ThrustCurveMotorSetDatabase m_motors;
    MaterialStorage             m_materials;
    DesignFileEnvironment       m_environment;
};

}  // namespace QtRocket::Test
