#pragma once

// The simulation listeners take a SimulationStatus&, a class that the listener headers only
// forward-declare: it arrives with the simulation engine (include/QtRocket/simulation/
// SimulationStatus.h). Until then the tests of the listeners need an object to pass, and this
// header defines an empty class in its place. No library code sees it.
//
// Once the real class exists this stand-in would be a second definition of it, so it refuses to
// compile: delete this header, include the real one in the tests that used it and give them a
// real status.
#if __has_include("QtRocket/simulation/SimulationStatus.h")
#error "SimulationStatus exists now: delete SimulationStatusStandIn.h and use a real status"
#endif

namespace QtRocket
{

/// A stand-in for the simulation status; see the comment at the top of this file.
class SimulationStatus
{ };

}  // namespace QtRocket
