#pragma once

#include <variant>

namespace QtRocket
{

class DocumentPreferences;
class OpenRocketDocument;
class RocketComponent;
class Simulation;

/// What a document tells its listeners: something in it changed, or it is about to be saved
/// (OpenRocket's document/events/DocumentChangeEvent and its subclass SimulationChangeEvent, in
/// one struct). The kind says which of the two Java classes the event is, and the source is
/// Java's EventObject.getSource().
///
/// The sources OpenRocket puts into these events:
/// - a RocketComponent: a change event of the rocket, passed on (kind DOCUMENT);
/// - a Simulation: a change of that simulation (kind DOCUMENT), or the simulation that was added
///   to or removed from the document (kind SIMULATION);
/// - the document: its simulations were reloaded by undo or redo (kind SIMULATION), or a decal
///   image was removed (kind DOCUMENT);
/// - the DocumentPreferences: a preference of the document changed (kind DOCUMENT);
/// - nothing (std::monostate): a source outside the core. OpenRocket's windows fire events with
///   themselves as the source (a simulation dialog, the frame that saves).
///
/// The pointers do not own what they point at and are valid during the emission; a listener must
/// not keep them beyond the call.
///
/// Deviations from OpenRocket:
/// - Java's listener interfaces, DocumentChangeListener (documentChanged(), documentSaving()) and
///   UndoRedoListener (setAllValues()), are signals of the document.
/// - Java's source is any Object and never null (EventObject refuses null); here it is one of the
///   alternatives above, each pointer non-null by convention (the helpers below give null only
///   for another alternative).
struct DocumentChangeEvent
{
    /// Which Java class the event is.
    enum class Kind
    {
        DOCUMENT,    ///< Java: DocumentChangeEvent itself
        SIMULATION,  ///< Java: SimulationChangeEvent, the list of simulations or their data changed
    };

    using Source = std::variant<std::monostate, OpenRocketDocument*, RocketComponent*, Simulation*,
                                DocumentPreferences*>;

    Kind   kind{Kind::DOCUMENT};
    Source source;

    /// Java: `event instanceof SimulationChangeEvent`.
    [[nodiscard]] bool isSimulationChange() const noexcept { return kind == Kind::SIMULATION; }

    /// The document when it is the source, else null.
    [[nodiscard]] OpenRocketDocument* getDocument() const noexcept
    {
        return pointer<OpenRocketDocument>();
    }
    /// The component when a component is the source, else null.
    [[nodiscard]] RocketComponent* getComponent() const noexcept
    {
        return pointer<RocketComponent>();
    }
    /// The simulation when a simulation is the source, else null.
    [[nodiscard]] Simulation* getSimulation() const noexcept { return pointer<Simulation>(); }
    /// The document preferences when they are the source, else null.
    [[nodiscard]] DocumentPreferences* getPreferences() const noexcept
    {
        return pointer<DocumentPreferences>();
    }

    /// The same kind and the same source object (an addition: Java compares events by identity).
    [[nodiscard]] bool operator==(const DocumentChangeEvent&) const = default;

private:
    template <class T>
    [[nodiscard]] T* pointer() const noexcept
    {
        T* const* const found = std::get_if<T*>(&source);
        return found != nullptr ? *found : nullptr;
    }
};

}  // namespace QtRocket
