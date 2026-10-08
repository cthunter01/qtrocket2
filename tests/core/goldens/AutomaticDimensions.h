#pragma once

// Settling the automatic dimensions of a design: a port of the golden harness's
// AutomaticDimensions.java (tools/openrocket-goldens). Test-only.
//
// OpenRocket computes automatic dimensions lazily and stores the result in the component, and the
// port keeps that (the getters are const and refresh mutable members): a getter such as
// BodyTube::getOuterRadius() (automatic radius) or MassObject::getRadius() (automatic radius, which
// also rescales the packed length) refreshes the stored member, and other getters read the stored
// member without refreshing it (MassObject::getLength() divides the volume by the stored radius; a
// body tube's automatic radius depends on which neighbour it last took its radius from). Right
// after a design is loaded or built the stored values can therefore be stale, and the first
// calculations mix stale and refreshed values: which state a comparison sees would depend on
// which getters ran before it.
//
// The harness therefore settles every design before it dumps anything, and the goldens (geometry,
// mass, aero, and the simulations that follow) describe that settled state. A test that compares
// a loaded design with its goldens calls settleAutomaticDimensions() right after loading and
// before anything else reads the design. A test that saves a design must not: the harness writes
// its re-save (resave/rocket.ork) before this step, as OpenRocket saves the design as loaded.
//
// Deviations from the Java:
// - AutomaticDimensions.settle() throws IllegalStateException when the dimensions do not settle;
//   here that is an Error (ErrorCode::UNKNOWN) with Java's message.
// - Java's passes are lists of Double and Boolean, compared with List.equals(). Here a pass is a
//   list of doubles, a boolean as 1.0 or 0.0, compared as Double.equals() compares (NaN equals
//   NaN, 0.0 differs from -0.0): the components and their kinds are the same in every pass, so
//   the same place of two passes holds values of the same kind.
// - The harness hands settle() its ComponentIndex for the tree order; here the tree is walked.

#include <functional>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Test
{

/// AutomaticDimensions.MAX_PASSES: the passes after which the dimensions must have settled.
inline constexpr int kMaxSettlingPasses = 20;

/// AutomaticDimensions.snapshot(): one pass. Calls the refreshing getters of every component of
/// @p rocket in tree order (depth first, a component before its children, the rocket first) and
/// returns what they returned, in that order:
/// - a BodyTube: getOuterRadius(), getInnerRadius();
/// - a SymmetricComponent (a body tube again, a transition, a nose cone): getForeRadius(),
///   getAftRadius(), usesPreviousCompAutomatic(), usesNextCompAutomatic();
/// - a RingComponent: getOuterRadius(), getInnerRadius();
/// - a MassObject: getRadius(), getLength().
/// A boolean is 1.0 or 0.0. (@p rocket is const because the getters are: they refresh mutable
/// members, as OpenRocket's refresh its fields.)
[[nodiscard]] std::vector<double> automaticDimensions(const Rocket& rocket);

/// Whether two passes are equal as Java's List.equals() of their Doubles says: the same number
/// of values, and every pair either both NaN or the same bits (0.0 differs from -0.0).
[[nodiscard]] bool samePass(const std::vector<double>& a, const std::vector<double>& b);

/// The loop of AutomaticDimensions.settle() over the passes @p pass makes: a first pass, then
/// passes until one repeats the pass before it. Returns how many passes returned values that
/// differed from those of the pass before; an Error with Java's message, which names the design
/// @p name, when kMaxSettlingPasses passes after the first one have not settled.
[[nodiscard]] Result<int> settleBy(const std::function<std::vector<double>()>& pass,
                                   std::string_view                            name);

/// AutomaticDimensions.settle(): brings the automatic dimensions of @p rocket into their settled
/// state (see the top of the file), which does not depend on the order of later calls. Returns
/// how many passes returned values that differed from those of the pass before (0 for a design
/// whose first pass already left it settled); an Error when they do not settle within
/// kMaxSettlingPasses passes.
[[nodiscard]] Result<int> settleAutomaticDimensions(const Rocket& rocket);

}  // namespace QtRocket::Test
