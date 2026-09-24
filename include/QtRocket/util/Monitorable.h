#pragma once

#include <concepts>

#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// An object whose changes can be watched through a modification id (OpenRocket:
/// util/Monitorable). modId() returns a ModId unique to the current state of the object and of
/// the objects it contains: when one object has the same id at two moments, its state did not
/// change in between, and the ids of an object are monotonically increasing. A reader (a
/// calculator's cache, a view in the GUI) keeps the id with what it computed and recomputes only
/// when the id differs. Two objects of one type may share an id without being equal, and a copy
/// may or may not carry the original's id; an object with no state may return ModId::zero().
///
/// An object of plain values draws a fresh ModId whenever a value is set; one that contains
/// mutable objects also draws one whenever a contained object changes or is replaced, so that
/// the whole stays monotonic (Java suggests summing the ids; ModId has no arithmetic, a redraw
/// is the equivalent).
///
/// Java's Monitorable is an interface; here it is a concept, since the users are templates and
/// concrete types (MessageSet, the rocket model, the calculators) and nothing needs to hold a
/// Monitorable through a base pointer. modId() is a const member returning a ModId by value.
template <class T>
concept Monitorable = requires(const T& object) {
    { object.modId() } -> std::same_as<ModId>;
};

}  // namespace QtRocket
