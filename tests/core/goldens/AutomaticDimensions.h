#pragma once

// Forwards to the settling of automatic dimensions, which moved into the core
// (QtRocket/rocket/AutomaticDimensions.h; the command-line tool's 'info' settles too), under the
// names the golden tests have always used. Read the core header for what settling does and why
// a design that is to be saved is never settled.

#include "QtRocket/rocket/AutomaticDimensions.h"  // IWYU pragma: export

namespace QtRocket::Test
{

using QtRocket::automaticDimensions;
using QtRocket::kMaxSettlingPasses;
using QtRocket::samePass;
using QtRocket::settleAutomaticDimensions;
using QtRocket::settleBy;

}  // namespace QtRocket::Test
