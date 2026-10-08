#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/document/DecalRegistry.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class Attachment;
class DecalImage;
class FlightConfiguration;
class FlightConfigurationId;
class FlightDataType;
class Rocket;
class RocketComponent;
class Simulation;

/// A whole design as the user edits and saves it (OpenRocket's document/OpenRocketDocument): the
/// rocket, its simulations, the custom expressions, the document preferences and materials, the
/// decal images, the file it was read from with its storage options, and the undo history.
///
/// The rocket: the document owns it and keeps the one Rocket object for its whole life (undo and
/// redo load a state into it), so a Rocket* or Rocket& of the document stays valid, while every
/// component and every FlightConfiguration is replaced by an undo or a redo. The constructor
/// does what Java's does, in its order: it sets the rocket's document, enables the rocket's
/// events (so a rocket is filled with its events enabled while a file loads), clears the undo
/// history and then starts to listen to the rocket and to the document preferences.
///
/// The simulations: the list holds them by std::shared_ptr, because an undo or redo that changes
/// their number replaces every object of the list and the callers keep the old ones (Java: the
/// garbage collector). A simulation refers to the document's rocket without owning it, so it
/// must not be used once the document is gone. The document listens to a simulation
/// (Simulation::changed()) from the moment it enters the list until it leaves it.
///
/// Change notification, three signals and one for errors:
/// - undoRedoChanged() (Java: UndoRedoListener.setAllValues()): what undo and redo would do may
///   have changed.
/// - documentChanged() (Java: DocumentChangeListener.documentChanged()): see DocumentChangeEvent
///   for the kinds and the sources.
/// - documentSaving() (Java: DocumentChangeListener.documentSaving()): emitted by whoever saves
///   the document, through fireDocumentSavingEvent(), before it reads it.
/// - undoErrorOccurred(): see "Undo errors" below.
/// What the document emits, in order (U = undoRedoChanged, D = documentChanged of kind DOCUMENT,
/// S = of kind SIMULATION, with the source in parentheses):
/// - a change event of the rocket: U D(component) D(component). Java's rocket calls the
///   document twice for one event, as a ComponentChangeListener and as a StateChangeListener,
///   and so does this. The first call drops the redo states and labels the undo step, unless the
///   event is an undo change; the second draws the document's modification id. So isSaved() is
///   still what it was during the first D and false during the second.
/// - a change of a simulation in the list: D(simulation), preceded by U when the change makes
///   an undo step (see "Undo");
/// - addSimulation() and removeSimulation(): U S(simulation), after the rocket's events when
///   the simulation's flight configuration had to be created;
/// - undo() and redo(): the rocket's U D(rocket) D(rocket), a D(simulation) for a simulation
///   that tells of the other values its options were restored to, S(document), U;
/// - a change of a document preference: D(preferences);
/// - removeDecal(): the events of the components that lose the image, then D(document);
/// - a change of a registered decal image: the events of the components that use it;
/// - clearUndo(): U; addUndoPosition(), startUndo() and stopUndo(): nothing.
/// A slot may change the document, with the care a listener needs in Java too; it must not
/// destroy the document, and it must not make the simulation whose change it is told of leave
/// the list unless somebody else holds that simulation.
///
/// Saved or not: the document has a modification id, drawn anew by every rocket event,
/// simulation change, preference change and change of the simulation list, and remembers the id
/// it was saved with (setSaved()). Both start invalid, so a new document is saved until
/// something changes, and setSaved(false) of a document nothing has changed leaves it saved.
/// Undo never makes a document saved again. Removing a decal image that no component uses and
/// registering a document material do not make it unsaved, as in Java.
///
/// Undo: the history is a list of states (a copy of the rocket, copies of the simulations and
/// the description of the step that leads away from the state) and a position in it. The
/// document is "clean" when the rocket has the modification id of the state at the position and
/// the simulations equal that state's (pairwise Simulation::operator==, which ignores the
/// status and everything simulated), else "dirty".
/// - addUndoPosition(description) is called before an action that is to be undone as one: when
///   the document is dirty the current state is appended and becomes the position; in either
///   case the description is the one the next change gets. Called twice without a change in
///   between, only the second call counts. The history is trimmed to kUndoLevels + 1 states when
///   it has grown beyond kUndoLevels + kUndoMargin.
/// - startUndo(description) and stopUndo() bracket a time-limited action: stopUndo() puts the
///   description back that was in force before startUndo(). They cannot be nested.
/// - A change of the rocket (not one made by undo or redo), of the simulation list or of a
///   simulation that makes the list differ from the state's drops the redo states and labels the
///   state at the position with the description. A change of a simulation that leaves the
///   document clean (a run, a new status) makes no undo step: the state's copy of that
///   simulation is refreshed instead, so that an undo back to this state brings the results
///   along.
/// - undo() and redo() load a state into the rocket (Rocket::loadFrom()) and into the
///   simulations: Simulation::loadFrom() by position when their number is the same, else new
///   simulations made from the state's (the list is replaced: see "The simulations").
/// As in Java, the simulations an undo or redo makes do not emit changed() when their options
/// change (Simulation::cloneForUndo()), so such a change is noticed only with the next event.
///
/// Undo errors: where Java logs an inconsistency of the undo calls, tells the user once per run
/// through the application's error handler and carries on, the document emits
/// undoErrorOccurred() with the text Java gives the handler ("Undo/Redo error: " and the error)
/// and carries on in the same way: undo() or redo() that is not available (only U is emitted),
/// addUndoPosition(), startUndo(), undo() or redo() while a description is stored by
/// startUndo(), and "undo position inconsistency" (the document is dirty although redo states
/// exist, which a change of a simulation's extension list brings about, since that list changes
/// without an event). They are not BugErrors: a caller can run into each of them.
///
/// Document materials: the materials the components of the rocket were given that belong to the
/// document (Material::isDocumentMaterial()). The document adds each one when the rocket
/// announces it (Rocket::documentMaterialSet(), the moment Java registers it), in a
/// MaterialStorage of its own; an equal material is not added twice, and nothing removes one
/// but removeMaterial() of that storage. The order of getDocumentMaterials().allMaterials() is
/// the order a saver writes.
///
/// Decal images: the document's registry keeps them by name, and a rocket/Decal names its image.
/// When a registered image says that it changed (DecalImage::fireChangeEvent()), every component
/// whose outside appearance names it fires TEXTURE_CHANGE, and once more when its inside
/// appearance does.
///
/// Deviations from OpenRocket:
/// - Simulations are heard while they are in the list (see above). Java's Simulation registers
///   its document as a listener when it is constructed and never removes it, so there a
///   simulation that was not added yet, or was removed, still marks the document unsaved, and
///   one made without a document and added later is never heard.
/// - getSimulationIndex() and removeSimulation(Simulation&) find the simulation by identity.
///   Java searches with equals(), which finds the first simulation that equals the argument:
///   asked to remove a copy of a simulation it would remove the original.
/// - Indices are std::size_t and getSimulationIndex() gives nullopt for Java's -1. An index out
///   of range and a null simulation are BugErrors (Java: IndexOutOfBoundsException,
///   NullPointerException).
/// - The undo errors are a signal emitted every time (see above); Java tells the user the first
///   one of a run only. startUndo() while a description is stored therefore reports two errors,
///   its own and that of the addUndoPosition() it calls; Java logs both.
/// - A description is a std::optional<std::string>, nullopt for Java's null.
/// - The document materials are in a MaterialStorage of the document (getDocumentMaterials()),
///   where Java keeps them in the three databases of its DocumentPreferences; the string list
///   of QtRocket's DocumentPreferences is not used.
/// - Texture changes: Java's components subscribe to the image in setAppearance() and
///   setInsideAppearance() and never unsubscribe, so a component fires once for every such call
///   it ever had, also for an image it no longer uses or that the document removed, and a
///   component that undo or redo made does not fire at all. Here the components that use the
///   image when it changes fire, in tree order, for the images registered in the document
///   (getDecalImage(), makeUniqueDecal()) until removeDecal() takes them out.
/// - countDecalUsage(), makeUniqueDecal() and removeDecal() compare the name of the image with
///   the name a decal holds (Java compares the image objects; names are unique in a registry).
///   findDecalImage() is an addition, for whoever has a Decal and needs its image.
/// - makeUniqueDecal(null) is a BugError (Java: null comes back unless a component has a decal,
///   and then a NullPointerException).
/// - getFlightDataTypes() returns a vector without repeats, in Java's order (Java: a
///   LinkedHashSet); "the same type" is FlightDataType::equals(), as there.
/// - getUndoDetail() is an addition (Java's undo fields are private; its log prints them).
/// - getCustomExpressions() and getPhotoSettings() hand out the document's own containers, as
///   Java does; the photo settings are kept sorted by key (Java: a HashMap).
/// - getFileNoExtension() works on the UTF-8 form of the path.
/// - The constructor is public (Java: package-private, for the factory and the tests).
/// - Not ported: copy(), which only the optimization dialog uses and which copies neither the
///   custom expressions, the document preferences nor the decals (it comes with the optimizer);
///   getDefaultOBJOptions() and the OBJ export options (OBJ export is not part of Milestone
///   1); the static logger and the static application preferences, which the constructor reads
///   for those options only.
/// - Not copyable and not movable: the rocket, the simulations and the listeners point at it.
class OpenRocketDocument
{
public:
    /// The minimum number of undo levels that are stored (Java: UNDO_LEVELS).
    static constexpr std::size_t kUndoLevels = 50;
    /// The margin of the undo levels: once the number of undo levels exceeds kUndoLevels by
    /// this amount, the undo history is purged to that length (Java: UNDO_MARGIN).
    static constexpr std::size_t kUndoMargin = 10;

    /// Java: SIMULATION_NAME_PREFIX.
    static constexpr std::string_view kSimulationNamePrefix = "Simulation ";

    /// A document of @p rocket, which it takes over (see the class comment for what the
    /// constructor does to it). OpenRocketDocumentFactory makes documents.
    /// @throws BugError when @p rocket is null
    explicit OpenRocketDocument(std::unique_ptr<Rocket> rocket);

    OpenRocketDocument(const OpenRocketDocument&)            = delete;
    OpenRocketDocument& operator=(const OpenRocketDocument&) = delete;
    OpenRocketDocument(OpenRocketDocument&&)                 = delete;
    OpenRocketDocument& operator=(OpenRocketDocument&&)      = delete;
    ~OpenRocketDocument();

    // --------------------------------------------------------------- custom expressions

    /// Adds @p expression to the list, also when the list already holds an equal one (Java only
    /// logs that it does).
    void addCustomExpression(CustomExpression expression);

    /// Removes the first expression of the list that equals @p expression, if any.
    void removeCustomExpression(const CustomExpression& expression);

    /// The custom expressions: the list itself, which the caller may change (Java hands out its
    /// list).
    [[nodiscard]] std::vector<CustomExpression>& getCustomExpressions() noexcept
    {
        return m_customExpressions;
    }
    [[nodiscard]] const std::vector<CustomExpression>& getCustomExpressions() const noexcept
    {
        return m_customExpressions;
    }

    /// All the flight data types defined or available in any way in the document: the built-in
    /// ones (FlightDataType::allTypes()), then the type of every custom expression
    /// (CustomExpression::getType(), which registers it), then the types of every extension of
    /// every simulation, in that order; a type equal to an earlier one
    /// (FlightDataType::equals(): the same name, ignoring case) is left out.
    /// @throws BugError when a simulation holds a null extension
    [[nodiscard]] std::vector<const FlightDataType*> getFlightDataTypes() const;

    // ---------------------------------------------------------------------- the rocket

    /// The rocket of the document.
    [[nodiscard]] Rocket&       getRocket() noexcept { return *m_rocket; }
    [[nodiscard]] const Rocket& getRocket() const noexcept { return *m_rocket; }

    /// The selected configuration of the rocket.
    [[nodiscard]] FlightConfiguration&       getSelectedConfiguration();
    [[nodiscard]] const FlightConfiguration& getSelectedConfiguration() const;

    // ------------------------------------------------------------------------ the file

    /// The file of the document, or nullopt (Java: null): where it was loaded from or saved to.
    /// A loader does not set it; its caller does.
    [[nodiscard]] const std::optional<std::filesystem::path>& getFile() const noexcept
    {
        return m_file;
    }

    /// The file without its extension (".ork" removed), or nullopt without a file. As in Java:
    /// the absolute path (absolutePath()) is cut at its last dot when what follows the dot is
    /// "ork" or "rkt", exactly; otherwise the file comes back as it was set, not made absolute.
    /// "x.ork.gz" and "x.rkt.gz" therefore come back unchanged (their extension is "gz"; Java
    /// lists "ork.gz" and "rkt.gz", which what follows a last dot can never be), and so does
    /// "x.ORK". A separator the cut leaves at the end is dropped, as Java's File drops it.
    [[nodiscard]] std::optional<std::filesystem::path> getFileNoExtension() const;

    /// Sets the file; nullopt for none.
    void setFile(std::optional<std::filesystem::path> file);

    /// Whether the document is as it was when it was last saved (see the class comment).
    [[nodiscard]] bool isSaved() const noexcept { return m_modId == m_savedId; }

    /// Marks the current state as the saved one, or, with false, no state.
    void setSaved(bool saved) noexcept;

    /// The default storage options of the document: the object itself, which loaders and the
    /// save dialog change.
    [[nodiscard]] StorageOptions& getDefaultStorageOptions() noexcept { return m_storageOptions; }
    [[nodiscard]] const StorageOptions& getDefaultStorageOptions() const noexcept
    {
        return m_storageOptions;
    }

    // -------------------------------------------------------------------------- decals

    /// The decal images registered in the document, sorted by name.
    [[nodiscard]] std::vector<std::shared_ptr<DecalImage>> getDecalList() const;

    /// The number of uses of @p image in the rocket: one for every component whose appearance
    /// has a decal of the image's name, and one more when its inside appearance does.
    [[nodiscard]] int countDecalUsage(const DecalImage& image) const;

    /// Takes @p decal off every component that uses it (outside, then inside, component by
    /// component in tree order: the appearance is set again without the image, which fires the
    /// component's NONFUNCTIONAL_CHANGE) and out of the registry. When either did something,
    /// emits documentChanged() with the document as the source and returns true. Null gives
    /// false. As in the registry the name of the image decides, not the object.
    bool removeDecal(const DecalImage* decal);

    /// An image for a component that is to change its decal without changing the others':
    /// @p image itself when at most one use of it is counted, else a copy under a new name
    /// (DecalRegistry::makeUniqueImage()).
    /// @throws BugError when @p image is null
    [[nodiscard]] std::shared_ptr<DecalImage> makeUniqueDecal(
        const std::shared_ptr<DecalImage>& image);

    /// The decal image of @p attachment, registered now when it is new
    /// (DecalRegistry::getDecalImage()). A Decal is made with the name of the image, which is
    /// not always the attachment's.
    /// @throws BugError when @p attachment is null
    [[nodiscard]] std::shared_ptr<DecalImage> getDecalImage(
        const std::shared_ptr<const Attachment>& attachment);

    /// The registered image named exactly @p name (Decal::getImageName()), or null (an
    /// addition: Java's Decal holds the image).
    [[nodiscard]] std::shared_ptr<DecalImage> findDecalImage(std::string_view name) const;

    // --------------------------------------------------------------------- simulations

    /// The simulations of the document: a copy of the list.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> getSimulations() const
    {
        return m_simulations;
    }

    /// The number of simulations in the document.
    [[nodiscard]] std::size_t getSimulationCount() const noexcept { return m_simulations.size(); }

    /// The simulation at @p n.
    /// @throws BugError when @p n is not below getSimulationCount()
    [[nodiscard]] std::shared_ptr<Simulation> getSimulation(std::size_t n) const;

    /// The index of @p simulation, this very object, in the list, or nullopt (Java: -1).
    [[nodiscard]] std::optional<std::size_t> getSimulationIndex(
        const Simulation& simulation) const noexcept;

    /// Adds @p simulation at the end of the list.
    /// @throws BugError when @p simulation is null
    void addSimulation(std::shared_ptr<Simulation> simulation);

    /// Inserts @p simulation at @p n (0 to getSimulationCount()); creates its flight
    /// configuration in the rocket when the rocket has none of that id; then marks the change
    /// (a new modification id, the redo states dropped, the undo step labelled,
    /// undoRedoChanged()) and emits documentChanged() of kind SIMULATION with the simulation.
    /// @throws BugError when @p simulation is null or @p n is above getSimulationCount()
    void addSimulation(std::shared_ptr<Simulation> simulation, std::size_t n);

    /// Removes @p simulation, this very object, from the list. The change is marked and the
    /// event emitted as by addSimulation(), also when the simulation was not in the list (as in
    /// Java).
    void removeSimulation(Simulation& simulation);

    /// Removes the simulation at @p n and returns it.
    /// @throws BugError when @p n is not below getSimulationCount()
    std::shared_ptr<Simulation> removeSimulation(std::size_t n);

    /// Removes every simulation of the flight configuration @p configId (each as
    /// removeSimulation() does) and then the configuration from the rocket
    /// (Rocket::removeFlightConfiguration()).
    void removeFlightConfigurationAndSimulations(const FlightConfigurationId& configId);

    /// A name for the next simulation: kSimulationNamePrefix and a number one above the largest
    /// that a simulation named "Simulation <number>" has (Integer.parseInt of what follows the
    /// prefix: "Simulation +7" and "Simulation 007" count, "Simulation 12 " does not), 1 when
    /// there is none. As in Java the number wraps around: beside "Simulation 2147483647" the
    /// answer is "Simulation -2147483648".
    [[nodiscard]] std::string getNextSimulationName() const;

    // ---------------------------------------------------------------------------- undo

    /// Adds an undo point at this position. Call it before any action that is to be undoable:
    /// all actions after the call will be undone by a single undo. @p description is a short
    /// text of the actions that follow, as an edit menu shows it ("Modify body tube"); nullopt
    /// when they are not known. See the class comment.
    void addUndoPosition(std::optional<std::string> description);

    /// Starts a time-limited undoable operation: addUndoPosition(@p description), remembering
    /// the description that was in force. stopUndo() must follow before any other undo call.
    void startUndo(std::optional<std::string> description);

    /// Ends the operation startUndo() began: addUndoPosition() with the description that was in
    /// force before it.
    void stopUndo();

    /// Clears the undo history: the current state becomes its only state. Emits
    /// undoRedoChanged(). The description for the next change stays.
    void clearUndo();

    /// Whether undo() can be performed: the position is not the first state, or the document is
    /// dirty.
    [[nodiscard]] bool isUndoAvailable() const;

    /// The description of what undo() would undo; nullopt when undo is not available or the
    /// step has no description.
    [[nodiscard]] std::optional<std::string> getUndoDescription() const;

    /// Whether redo() can be performed: there are states after the position.
    [[nodiscard]] bool isRedoAvailable() const noexcept;

    /// The description of what redo() would redo; nullopt when redo is not available or the
    /// step has no description.
    [[nodiscard]] std::optional<std::string> getRedoDescription() const;

    /// Performs an undo: in the clean state the position moves back one state, in the dirty
    /// state the current state is appended first (so that redo can come back to it) and the
    /// position stays; the state at the position is then loaded. When undo is not available an
    /// undo error is reported, undoRedoChanged() is emitted and nothing else happens. Whatever
    /// the loading does (Java: finally), undoRedoChanged() is emitted at the end.
    void undo();

    /// Performs a redo: the position moves forward one state, which is loaded. Otherwise as
    /// undo().
    void redo();

    /// The state of the undo mechanism, for diagnostics and tests (an addition: Java's fields
    /// are private, and its log prints them with every undo call).
    struct UndoDetail
    {
        /// The position in the history.
        std::size_t position{0};
        /// The description of every state of the history, in order; their number is the size
        /// of the history.
        std::vector<std::optional<std::string>> descriptions;
        /// The description the next change gets.
        std::optional<std::string> nextDescription;
        /// The description stopUndo() puts back.
        std::optional<std::string> storedDescription;
        /// Whether the document is in the clean state.
        bool clean{true};
    };
    [[nodiscard]] UndoDetail getUndoDetail() const;

    // ------------------------------------------------------------------------- signals

    /// Emitted when what undo and redo would do may have changed.
    [[nodiscard]] Signal<>& undoRedoChanged() noexcept { return m_undoRedoChanged; }

    /// Emitted when something in the document changed (see the class comment).
    [[nodiscard]] Signal<const DocumentChangeEvent&>& documentChanged() noexcept
    {
        return m_documentChanged;
    }

    /// Emitted by fireDocumentSavingEvent(): the document is about to be saved.
    [[nodiscard]] Signal<const DocumentChangeEvent&>& documentSaving() noexcept
    {
        return m_documentSaving;
    }

    /// Emitted with "Undo/Redo error: <error>" for every undo error (see the class comment).
    [[nodiscard]] Signal<const std::string&>& undoErrorOccurred() noexcept
    {
        return m_undoErrorOccurred;
    }

    /// Emits documentChanged() with @p event. It changes nothing else: neither the saved state
    /// nor the undo history.
    void fireDocumentChangeEvent(const DocumentChangeEvent& event) const;

    /// Emits documentSaving() with @p event.
    void fireDocumentSavingEvent(const DocumentChangeEvent& event) const;

    // -------------------------------------------------------------------------- others

    /// ">> Dumping simulation list:" and one line per simulation, "    [index] name (the short
    /// key of its configuration id) ", each line ended by a newline.
    [[nodiscard]] std::string toSimulationDetail() const;

    /// The settings of the photo studio, as the keys and texts a file holds them: the map
    /// itself. The core does not interpret them; they are kept so that a file is saved with the
    /// settings it was loaded with.
    [[nodiscard]] std::map<std::string, std::string>& getPhotoSettings() noexcept
    {
        return m_photoSettings;
    }
    [[nodiscard]] const std::map<std::string, std::string>& getPhotoSettings() const noexcept
    {
        return m_photoSettings;
    }
    void setPhotoSettings(std::map<std::string, std::string> photoSettings);

    /// The preferences saved with the document. A change of one makes the document unsaved and
    /// emits documentChanged() with the preferences as the source.
    [[nodiscard]] DocumentPreferences& getDocumentPreferences() noexcept { return m_docPrefs; }
    [[nodiscard]] const DocumentPreferences& getDocumentPreferences() const noexcept
    {
        return m_docPrefs;
    }

    /// The materials of the document (see the class comment; Java: the material databases of
    /// getDocumentPreferences()). A loader adds the materials a file lists, a saver writes
    /// allMaterials().
    [[nodiscard]] MaterialStorage& getDocumentMaterials() noexcept { return m_documentMaterials; }
    [[nodiscard]] const MaterialStorage& getDocumentMaterials() const noexcept
    {
        return m_documentMaterials;
    }

    /// Adds to the document materials every material of the rocket's components
    /// (RocketComponent::getAllMaterials()) that is user-defined and a document material. For a
    /// rocket that got such materials before it belonged to the document.
    void reloadDocumentMaterials();

    // HOOK(optimizer): Java's copy() comes with the optimizer, its only user.
    // HOOK(obj-export): Java's getDefaultOBJOptions() comes with the OBJ export.

private:
    /// A state of the undo history (defined with the implementation).
    struct UndoState;

    /// Java's componentChanged() followed by stateChanged(): the rocket's change listener.
    void rocketChanged(const ComponentChangeEvent& event);

    /// Java's stateChanged() for a simulation of the list.
    void simulationChanged(Simulation& simulation);

    /// Java's stateChanged() for the document preferences.
    void preferencesChanged();

    /// Java's simulationsChanged(): the list of simulations changed.
    void simulationsChanged();

    /// Takes the simulation at @p n, which must be in range, out of the list and stops listening
    /// to it.
    std::shared_ptr<Simulation> removeAt(std::size_t n);

    /// Connects to @p simulation's changed(), unless the document listens to it already (a
    /// simulation that is in the list twice).
    void listenTo(Simulation& simulation);

    /// Disconnects from @p simulation's changed() when it is no longer in the list.
    void stopListeningTo(const Simulation& simulation);

    /// Connects to the changed() of @p image when it is registered in the document and not
    /// heard yet. Null does nothing.
    void watchDecalImage(const std::shared_ptr<DecalImage>& image);

    /// The slot of the decal image named @p name: fires TEXTURE_CHANGE on the components that
    /// use it.
    void decalImageChanged(std::string_view name);

    /// Takes the image named @p name off every component; true when a component used it.
    bool clearDecalUsage(std::string_view name);

    // Java's private helpers of the undo mechanism, under their names.
    void               checkDescription(const std::optional<std::string>& description);
    [[nodiscard]] bool isCheckNoModification(const std::optional<std::string>& description);
    void               checkUndoPositionConsistency();
    void               addStateToUndoHistory(std::optional<std::string> description);
    void               maintainMaximumUndoSize();
    void               setLatestDescription();
    void               removeRedoInfo();
    [[nodiscard]] bool isCleanState() const;
    [[nodiscard]] std::vector<std::unique_ptr<Simulation>> copySimulationsForUndo() const;
    void loadSimulationsFrom(const std::vector<std::unique_ptr<Simulation>>& simulationsSnapshot);
    void logUndoError(std::string_view error) const;
    void fireUndoRedoChangeEvent() const;

    /// The current state, as a new entry of the history without a description.
    [[nodiscard]] std::shared_ptr<UndoState> currentState() const;

    /// Whether the simulations equal @p snapshot: the same number, pairwise equal (Java:
    /// simulations.equals(snapshot)).
    [[nodiscard]] bool simulationsEqual(
        const std::vector<std::unique_ptr<Simulation>>& snapshot) const;

    /// What undo() and redo() share: loads the state at the position into the rocket and the
    /// simulations with the in-undo flag set, checking the component structure before and after
    /// the rocket is loaded when @p checkStructure (undo does, redo does not); whatever
    /// happens, the flag is cleared and undoRedoChanged() emitted (Java: the finally block).
    void loadStateAtPosition(bool checkStructure);

    /// The rocket; declared first, so that it is destroyed last: the simulations and the undo
    /// states refer to it.
    std::unique_ptr<Rocket> m_rocket;

    DocumentPreferences m_docPrefs;
    /// The document materials (Java: the three material databases of docPrefs).
    MaterialStorage m_documentMaterials;

    std::vector<std::shared_ptr<Simulation>> m_simulations;
    std::vector<CustomExpression>            m_customExpressions;

    /// The photo settings, as a map of keys with their content.
    std::map<std::string, std::string> m_photoSettings;

    /// The undo history. Whenever a new undo position is created while the document is in the
    /// dirty state, the current state is copied here. The states are shared so that the one
    /// being loaded survives a slot that changes the history.
    std::vector<std::shared_ptr<UndoState>> m_undoHistory;
    bool                                    m_inUndoRedo{false};

    /// The position in the undo history we are currently at. If modifications have been made,
    /// the document is in the dirty state and this points to the previous clean state.
    std::size_t m_undoPosition{0};

    /// The description of the next action that modifies this document.
    std::optional<std::string> m_nextDescription;
    std::optional<std::string> m_storedDescription;

    std::optional<std::filesystem::path> m_file;
    ModId                                m_modId{ModId::invalid()};
    ModId                                m_savedId{ModId::invalid()};

    StorageOptions m_storageOptions;

    DecalRegistry m_decalRegistry;

    Signal<>                           m_undoRedoChanged;
    Signal<const DocumentChangeEvent&> m_documentChanged;
    Signal<const DocumentChangeEvent&> m_documentSaving;
    Signal<const std::string&>         m_undoErrorOccurred;

    // The document's connections; declared last, so that they are cut first.
    ComponentChangeSignal::ScopedConnection   m_rocketConnection;
    Signal<const Material&>::ScopedConnection m_materialConnection;
    Signal<>::ScopedConnection                m_preferencesConnection;
    /// One connection per simulation of the list.
    std::map<const Simulation*, Signal<>::ScopedConnection> m_simulationConnections;
    /// One connection per registered decal image, by its name.
    std::map<std::string, Signal<>::ScopedConnection, std::less<>> m_decalConnections;
};

}  // namespace QtRocket
