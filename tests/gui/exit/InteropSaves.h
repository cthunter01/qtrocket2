#pragma once

// Where the GUI tests' DISABLED_ writers of the interop check put their saves (plan sections 7,
// 9.5, 10.5 and 11.6 E7; tools/openrocket-goldens/README.md, "Interop check"): the writers
// FromScratchInteropSave (from_scratch_exit_tests.cpp), FlightConfigurationsInteropSave
// (flight_configurations_exit_tests.cpp), SimulationsInteropSave (simulations_exit_tests.cpp) and
// FlightDataInteropSave (flight_data_interop_tests.cpp), which the filter '*InteropSave*' runs,
// after the core's OrkInteropSaves (tests/core/goldens/ork_interop_saves_tests.cpp), which removes
// their saves of an earlier run with its own (guiInteropInputs() of
// tests/core/goldens/GuiInteropInputs.h names their inputs).

#include <filesystem>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket::Gui::Test
{

/// The directory of the interop check's saves: the one the command-line argument
/// --interop-saves=<directory> of the test program names (GoogleTest keeps the arguments it does
/// not know), else interop-saves in the build tree, the directory above the bin/ of the test
/// program (build/<preset>/interop-saves, the default directory of interop.sh), as for the core's
/// saves.
[[nodiscard]] std::filesystem::path interopSavesDirectory();

/// Copies @p file into interopSavesDirectory(), which it makes when it is missing, as @p name
/// ("<input>.<form>.ork", InteropCheck's name of the save), replacing the file of that name an
/// earlier run wrote; no other file is touched. The file written, or what went wrong (an IO
/// error).
[[nodiscard]] Result<std::filesystem::path> writeInteropSave(const std::filesystem::path& file,
                                                             std::string_view             name);

}  // namespace QtRocket::Gui::Test
