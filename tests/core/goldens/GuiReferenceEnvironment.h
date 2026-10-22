#pragma once

// The environment in which tools/openrocket-gui-reference loaded the design its from-scratch
// scenario saved (tests/data/goldens/gui/from-scratch/state.json, "loaded"), for the tests that
// load a save of that scenario and compare with it: from_scratch_reference_golden_tests.cpp
// (OpenRocket's save) and the GUI's exit test (tests/gui/exit/, QtRocket's). Test-only.

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Test
{

/// OpenRocket's GUI after its start-up (SwingStartup), as the tool loads the saved design in it:
/// a DesignFileEnvironment without component presets and motors (QtRocket's start-up of
/// Milestone 2 has none either, and the design names none), with the default component
/// materials the start-up stores (loadDefaultComponentMaterials(): Balsa for fin sets,
/// Polystyrene for nose cones). A new component of the loader starts with them, so a <material>
/// that names the same one changes nothing and fires no event: the events of the load count it.
class GuiReferenceEnvironment
{
public:
    GuiReferenceEnvironment()
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
    ThrustCurveMotorSetDatabase m_motors;
    MaterialStorage             m_materials;
    DesignFileEnvironment       m_environment{DesignFileEnvironment::Presets::NONE, m_motors};
};

}  // namespace QtRocket::Test
