#pragma once

// Rebuilding a body component (a BodyTube, a NoseCone or a Transition) from its entry in the
// geometry golden data, for the golden tests of the bodies themselves and of what stands on them
// (body_geometry_golden_tests.cpp, fin_geometry_golden_tests.cpp). Test-only. The entries are
// read with GoldenGeometry.h and compared with GoldenMismatches.h.

#include <memory>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/SymmetricComponent.h"

namespace QtRocket::Test
{

/// The body component the golden @p component describes, built on its own (not in a rocket),
/// with its material and with fixed radii (automatic radii as OpenRocket settled them); nullptr
/// for a component that is not a body component (a BodyTube, a NoseCone or a Transition).
[[nodiscard]] std::unique_ptr<SymmetricComponent> rebuildGoldenBody(
    const nlohmann::json& component);

}  // namespace QtRocket::Test
