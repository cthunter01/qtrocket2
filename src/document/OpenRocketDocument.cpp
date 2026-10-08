#include "QtRocket/document/OpenRocketDocument.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The extensions of an OpenRocket document (Java: file_extensions). The two with a dot in them
/// are Java's too and never match: what follows the last dot of a path has no dot.
constexpr std::array<std::u8string_view, 4> kFileExtensions{u8"ork", u8"ork.gz", u8"rkt",
                                                            u8"rkt.gz"};

/// A description as Java's string concatenation prints it: the text, or "null".
[[nodiscard]] std::string_view textOf(const std::optional<std::string>& description) noexcept
{
    return description.has_value() ? std::string_view(*description) : std::string_view("null");
}

/// Whether @p appearance has a decal of the image named @p name (Java: hasDecal(), which compares
/// the image objects).
[[nodiscard]] bool hasDecal(const std::optional<Appearance>& appearance, std::string_view name)
{
    if (!appearance.has_value())
    {
        return false;
    }
    const std::optional<Decal>& texture = appearance->getTexture();
    return texture.has_value() && texture->getImageName() == name;
}

/// Whether the inside appearance of @p component, when it has an inside, has a decal of the
/// image named @p name (Java: hasDecalInside()).
[[nodiscard]] bool hasDecalInside(const RocketComponent& component, std::string_view name)
{
    const auto* inside = dynamic_cast<const InsideColorComponent*>(&component);
    return inside != nullptr &&
           hasDecal(inside->getInsideColorComponentHandler().getInsideAppearance(), name);
}

/// Java's clearOutsideDecal(): sets the appearance of @p component again without its image when
/// the image is the one named @p name; true when it did.
bool clearOutsideDecal(RocketComponent& component, std::string_view name)
{
    const std::optional<Appearance>& appearance = component.getAppearance();
    if (!hasDecal(appearance, name))
    {
        return false;
    }
    AppearanceBuilder builder(appearance);
    builder.setImage(std::nullopt);
    component.setAppearance(builder.getAppearance());
    return true;
}

/// Java's clearInsideDecal(): the same for the inside appearance of a component that has one.
bool clearInsideDecal(RocketComponent& component, std::string_view name)
{
    auto* inside = dynamic_cast<InsideColorComponent*>(&component);
    if (inside == nullptr)
    {
        return false;
    }
    InsideColorComponentHandler&     handler    = inside->getInsideColorComponentHandler();
    const std::optional<Appearance>& appearance = handler.getInsideAppearance();
    if (!hasDecal(appearance, name))
    {
        return false;
    }
    AppearanceBuilder builder(appearance);
    builder.setImage(std::nullopt);
    handler.setInsideAppearance(builder.getAppearance());
    return true;
}

/// The flight data types of a Java LinkedHashSet<FlightDataType>: in the order they were added,
/// without a type that equals one added before it.
class FlightDataTypeSet
{
public:
    void add(const FlightDataType& type)
    {
        std::vector<const FlightDataType*>& bucket  = m_buckets[type.hashCode()];
        const bool                          present = std::ranges::any_of(
            bucket, [&type](const FlightDataType* other) { return type.equals(*other); });
        if (present)
        {
            return;
        }
        bucket.push_back(&type);
        m_types.push_back(&type);
    }

    [[nodiscard]] std::vector<const FlightDataType*> take() noexcept { return std::move(m_types); }

private:
    std::vector<const FlightDataType*> m_types;
    /// The types by FlightDataType::hashCode(), which equal types share.
    std::unordered_map<int, std::vector<const FlightDataType*>> m_buckets;
};

/// Sets a flag for as long as it lives (Java: `flag = true; try { ... } finally { flag = false;
/// }`).
class FlagScope
{
public:
    explicit FlagScope(bool& flag) noexcept : m_flag(&flag) { *m_flag = true; }
    ~FlagScope() { *m_flag = false; }

    FlagScope(const FlagScope&)            = delete;
    FlagScope& operator=(const FlagScope&) = delete;
    FlagScope(FlagScope&&)                 = delete;
    FlagScope& operator=(FlagScope&&)      = delete;

private:
    bool* m_flag;
};

}  // namespace

/// A state of the undo history: Java's entries of undoHistory, undoDescription and
/// undoSimulationHistory at one index.
struct OpenRocketDocument::UndoState
{
    /// The rocket, copied with its ids and its modification ids.
    std::unique_ptr<Rocket> rocket;
    /// The description of the step that leads away from this state; nullopt until a change
    /// labels it.
    std::optional<std::string> description;
    /// The simulations (Simulation::cloneForUndo()), which refer to the document's rocket.
    std::vector<std::unique_ptr<Simulation>> simulations;
};

// Java: "main constructor, enable events in the rocket and initializes the document".
OpenRocketDocument::OpenRocketDocument(std::unique_ptr<Rocket> rocket) : m_rocket(std::move(rocket))
{
    QTROCKET_ASSERT(m_rocket != nullptr);
    // Java: rocket.setDocument(this). From here on a component finds its document and registers
    // its document materials with it, which is the connection to the rocket's signal.
    m_rocket->setDocument(this);
    m_materialConnection = m_rocket->documentMaterialSet().connect(
        [this](const Material& material) { m_documentMaterials.addMaterial(material); });
    // HOOK(obj-export): Java reads the default OBJ export options of the rocket from the
    // application preferences here.
    m_rocket->enableEvents();

    // Java: init(), clearing the undo cache and setting itself as a listener for changes in the
    // rocket.
    clearUndo();
    m_rocketConnection = m_rocket->addComponentChangeListener(
        [this](const ComponentChangeEvent& event) { rocketChanged(event); });
    m_preferencesConnection = m_docPrefs.changed().connect([this] { preferencesChanged(); });
}

OpenRocketDocument::~OpenRocketDocument() = default;

// ------------------------------------------------------------------- custom expressions

void OpenRocketDocument::addCustomExpression(CustomExpression expression)
{
    // Java logs "Could not add custom expression ... as document already has a matching
    // expression." for an expression the list holds, and adds it all the same.
    m_customExpressions.push_back(std::move(expression));
}

void OpenRocketDocument::removeCustomExpression(const CustomExpression& expression)
{
    const auto found = std::ranges::find(m_customExpressions, expression);
    if (found != m_customExpressions.end())
    {
        m_customExpressions.erase(found);
    }
}

std::vector<const FlightDataType*> OpenRocketDocument::getFlightDataTypes() const
{
    FlightDataTypeSet allTypes;

    // built in
    for (const FlightDataType* const type : FlightDataType::allTypes())
    {
        allTypes.add(*type);
    }

    // custom expressions
    for (const CustomExpression& exp : m_customExpressions)
    {
        allTypes.add(exp.getType());
    }

    // simulation listeners
    for (const std::shared_ptr<Simulation>& sim : m_simulations)
    {
        const Simulation& simulation = *sim;
        for (const std::shared_ptr<SimulationExtension>& c : simulation.getSimulationExtensions())
        {
            if (c == nullptr)
            {
                bug("The simulation holds a null extension");
            }
            for (const FlightDataType* const type : c->getFlightDataTypes())
            {
                QTROCKET_ASSERT(type != nullptr);
                allTypes.add(*type);
            }
        }
    }

    // imported data: not implemented yet (as in Java)

    return allTypes.take();
}

// -------------------------------------------------------------------------- the rocket

FlightConfiguration& OpenRocketDocument::getSelectedConfiguration()
{
    return m_rocket->getSelectedConfiguration();
}

const FlightConfiguration& OpenRocketDocument::getSelectedConfiguration() const
{
    const Rocket& rocket = *m_rocket;
    return rocket.getSelectedConfiguration();
}

// ---------------------------------------------------------------------------- the file

std::optional<std::filesystem::path> OpenRocketDocument::getFileNoExtension() const
{
    if (!m_file.has_value())
    {
        return std::nullopt;
    }
    const std::u8string absolute = absolutePath(*m_file).u8string();
    const std::size_t   index    = absolute.rfind(u8'.');
    if (index != std::u8string::npos && index > 0)
    {
        const std::u8string_view extension = std::u8string_view(absolute).substr(index + 1);
        if (std::ranges::find(kFileExtensions, extension) != kFileExtensions.end())
        {
            std::filesystem::path filename(absolute.substr(0, index));
            // Java's File drops the separator that "/a/b/.ork" leaves at the end.
            if (!filename.has_filename() && filename.has_relative_path())
            {
                filename = filename.parent_path();
            }
            return filename;
        }
    }
    return m_file;
}

void OpenRocketDocument::setFile(std::optional<std::filesystem::path> file)
{
    m_file = std::move(file);
}

void OpenRocketDocument::setSaved(bool saved) noexcept
{
    if (!saved)
    {
        m_savedId = ModId::invalid();
    }
    else
    {
        m_savedId = m_modId;
    }
}

// ------------------------------------------------------------------------------ decals

std::vector<std::shared_ptr<DecalImage>> OpenRocketDocument::getDecalList() const
{
    return m_decalRegistry.getDecalList();
}

int OpenRocketDocument::countDecalUsage(const DecalImage& image) const
{
    const std::string& name  = image.getName();
    int                count = 0;

    const Rocket& rocket = *m_rocket;
    for (const RocketComponent& c : rocket.subtree())
    {
        if (hasDecal(c.getAppearance(), name))
        {
            count++;
        }
        if (hasDecalInside(c, name))
        {
            count++;
        }
    }
    return count;
}

bool OpenRocketDocument::removeDecal(const DecalImage* decal)
{
    if (decal == nullptr)
    {
        return false;
    }
    // The registry may hold the only reference to the image, and a slot of the events below may
    // take the image out of it: the registered image is kept until the end. (An image of that
    // name that is not the registered one is the caller's to keep.)
    const std::string&                name       = decal->getName();
    const std::shared_ptr<DecalImage> registered = m_decalRegistry.find(name);

    const bool clearedUsage = clearDecalUsage(name);
    const bool removed      = m_decalRegistry.removeDecal(decal);
    if (m_decalRegistry.find(name) == nullptr)
    {
        m_decalConnections.erase(name);
    }
    if (clearedUsage || removed)
    {
        fireDocumentChangeEvent(
            DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::DOCUMENT, .source = this});
    }
    return clearedUsage || removed;
}

bool OpenRocketDocument::clearDecalUsage(std::string_view name)
{
    bool updated = false;
    for (RocketComponent& component : m_rocket->subtree())
    {
        if (clearOutsideDecal(component, name))
        {
            updated = true;
        }
        if (clearInsideDecal(component, name))
        {
            updated = true;
        }
    }
    return updated;
}

std::shared_ptr<DecalImage> OpenRocketDocument::makeUniqueDecal(
    const std::shared_ptr<DecalImage>& image)
{
    QTROCKET_ASSERT(image != nullptr);
    if (countDecalUsage(*image) <= 1)
    {
        return image;
    }
    std::shared_ptr<DecalImage> unique = m_decalRegistry.makeUniqueImage(image);
    watchDecalImage(unique);
    return unique;
}

std::shared_ptr<DecalImage> OpenRocketDocument::getDecalImage(
    const std::shared_ptr<const Attachment>& attachment)
{
    std::shared_ptr<DecalImage> image = m_decalRegistry.getDecalImage(attachment);
    watchDecalImage(image);
    return image;
}

std::shared_ptr<DecalImage> OpenRocketDocument::findDecalImage(std::string_view name) const
{
    return m_decalRegistry.find(name);
}

void OpenRocketDocument::watchDecalImage(const std::shared_ptr<DecalImage>& image)
{
    if (image == nullptr)
    {
        return;
    }
    const std::string& name = image->getName();
    // makeUniqueImage() hands an image back that it did not have to rename and so did not
    // register; one image is registered under a name until removeDecal() takes it out.
    if (m_decalRegistry.find(name) != image || m_decalConnections.contains(name))
    {
        return;
    }
    m_decalConnections.emplace(name, Signal<>::ScopedConnection(image->changed().connect(
                                         [this, name] { decalImageChanged(name); })));
}

void OpenRocketDocument::decalImageChanged(std::string_view name)
{
    // Java: the listener that RocketComponent.setAppearance() and
    // InsideColorComponentHandler.setInsideAppearance() add to the image.
    for (RocketComponent& component : m_rocket->subtree())
    {
        if (hasDecal(component.getAppearance(), name))
        {
            component.fireComponentChangeEvent(ComponentChangeEvent::kTextureChange);
        }
        if (hasDecalInside(component, name))
        {
            component.fireComponentChangeEvent(ComponentChangeEvent::kTextureChange);
        }
    }
}

// ------------------------------------------------------------------------- simulations

std::shared_ptr<Simulation> OpenRocketDocument::getSimulation(std::size_t n) const
{
    if (n >= m_simulations.size())
    {
        bug(std::format("Simulation index {} out of range, the document has {}", n,
                        m_simulations.size()));
    }
    return m_simulations[n];
}

std::optional<std::size_t> OpenRocketDocument::getSimulationIndex(
    const Simulation& simulation) const noexcept
{
    for (std::size_t i = 0; i < m_simulations.size(); i++)
    {
        if (m_simulations[i].get() == &simulation)
        {
            return i;
        }
    }
    return std::nullopt;
}

void OpenRocketDocument::addSimulation(std::shared_ptr<Simulation> simulation)
{
    addSimulation(std::move(simulation), m_simulations.size());
}

void OpenRocketDocument::addSimulation(std::shared_ptr<Simulation> simulation, std::size_t n)
{
    QTROCKET_ASSERT(simulation != nullptr);
    if (n > m_simulations.size())
    {
        bug(std::format("Simulation index {} out of range, the document has {}", n,
                        m_simulations.size()));
    }
    // Kept until the events are out, whatever their slots do to the list.
    const std::shared_ptr<Simulation> added = simulation;
    m_simulations.insert(std::next(m_simulations.begin(), static_cast<std::ptrdiff_t>(n)),
                         std::move(simulation));
    // Deviation: Java's simulation has been telling its document of its changes since it was
    // made.
    listenTo(*added);
    const FlightConfigurationId simId = added->getId();
    if (!m_rocket->containsFlightConfigurationId(simId))
    {
        m_rocket->createFlightConfiguration(simId);
    }
    simulationsChanged();
    fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::SIMULATION, .source = added.get()});
}

void OpenRocketDocument::removeSimulation(Simulation& simulation)
{
    // Deviation: Java removes the first simulation that equals() the argument.
    const std::optional<std::size_t> index = getSimulationIndex(simulation);
    // Kept until the event is out: the list may hold the only reference.
    const std::shared_ptr<Simulation> removed = index.has_value() ? removeAt(*index) : nullptr;
    simulationsChanged();
    fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::SIMULATION, .source = &simulation});
}

std::shared_ptr<Simulation> OpenRocketDocument::removeSimulation(std::size_t n)
{
    if (n >= m_simulations.size())
    {
        bug(std::format("Simulation index {} out of range, the document has {}", n,
                        m_simulations.size()));
    }
    std::shared_ptr<Simulation> simulation = removeAt(n);
    simulationsChanged();
    fireDocumentChangeEvent(DocumentChangeEvent{.kind   = DocumentChangeEvent::Kind::SIMULATION,
                                                .source = simulation.get()});
    return simulation;
}

std::shared_ptr<Simulation> OpenRocketDocument::removeAt(std::size_t n)
{
    std::shared_ptr<Simulation> simulation = std::move(m_simulations[n]);
    m_simulations.erase(std::next(m_simulations.begin(), static_cast<std::ptrdiff_t>(n)));
    stopListeningTo(*simulation);
    return simulation;
}

void OpenRocketDocument::simulationsChanged()
{
    m_modId = ModId{};
    removeRedoInfo();
    setLatestDescription();
    fireUndoRedoChangeEvent();
}

void OpenRocketDocument::removeFlightConfigurationAndSimulations(
    const FlightConfigurationId& configId)
{
    // Java: removeSimulations(configId), over a copy of the list.
    for (const std::shared_ptr<Simulation>& s : getSimulations())
    {
        if (configId == s->getId())
        {
            removeSimulation(*s);
        }
    }
    m_rocket->removeFlightConfiguration(configId);
}

std::string OpenRocketDocument::getNextSimulationName() const
{
    // Generate unique name for the simulation
    std::int32_t maxValue = 0;
    for (const std::shared_ptr<Simulation>& s : m_simulations)
    {
        const std::string_view name = s->getName();
        if (name.starts_with(kSimulationNamePrefix))
        {
            // Java: Integer.parseInt(), whose NumberFormatException is ignored.
            const std::optional<int> value =
                Strings::parseInt(name.substr(kSimulationNamePrefix.size()));
            if (value.has_value())
            {
                maxValue = std::max(maxValue, std::int32_t{*value});
            }
        }
    }
    // Java's int addition wraps around.
    const auto next = static_cast<std::int32_t>(static_cast<std::uint32_t>(maxValue) + 1U);
    return std::format("{}{}", kSimulationNamePrefix, next);
}

void OpenRocketDocument::listenTo(Simulation& simulation)
{
    if (m_simulationConnections.contains(&simulation))
    {
        return;
    }
    m_simulationConnections.emplace(&simulation,
                                    Signal<>::ScopedConnection(simulation.changed().connect(
                                        [this, &simulation] { simulationChanged(simulation); })));
}

void OpenRocketDocument::stopListeningTo(const Simulation& simulation)
{
    if (!getSimulationIndex(simulation).has_value())
    {
        m_simulationConnections.erase(&simulation);
    }
}

// -------------------------------------------------------------------------------- undo

void OpenRocketDocument::addUndoPosition(std::optional<std::string> description)
{
    checkDescription(description);

    // Check whether modifications have been done since last call
    if (isCheckNoModification(description))
    {
        return;
    }
    checkUndoPositionConsistency();
    addStateToUndoHistory(std::move(description));

    maintainMaximumUndoSize();
}

void OpenRocketDocument::maintainMaximumUndoSize()
{
    if (m_undoHistory.size() > kUndoLevels + kUndoMargin && m_undoPosition > kUndoMargin)
    {
        m_undoHistory.erase(
            m_undoHistory.begin(),
            std::next(m_undoHistory.begin(), static_cast<std::ptrdiff_t>(kUndoMargin)));
        m_undoPosition -= kUndoMargin;
    }
}

void OpenRocketDocument::addStateToUndoHistory(std::optional<std::string> description)
{
    // Add the current state to the undo history
    m_undoHistory.push_back(currentState());
    m_nextDescription = std::move(description);
    m_undoPosition++;
}

bool OpenRocketDocument::isCheckNoModification(const std::optional<std::string>& description)
{
    if (isCleanState())
    {
        // No modifications
        m_nextDescription = description;
        return true;
    }
    return false;
}

void OpenRocketDocument::checkDescription(const std::optional<std::string>& description)
{
    if (m_storedDescription.has_value())
    {
        logUndoError(std::format("addUndoPosition called while storedDescription={} description={}",
                                 *m_storedDescription, textOf(description)));
    }
}

// If modifications have been made to the rocket. We should be at the end of the undo history,
// but check for consistency and try to recover.
void OpenRocketDocument::checkUndoPositionConsistency()
{
    if (m_undoPosition + 1 != m_undoHistory.size())
    {
        logUndoError("undo position inconsistency");
    }
    removeRedoInfo();
}

void OpenRocketDocument::startUndo(std::optional<std::string> description)
{
    if (m_storedDescription.has_value())
    {
        logUndoError(std::format("startUndo called while storedDescription={} description={}",
                                 *m_storedDescription, textOf(description)));
    }
    std::optional<std::string> store = m_nextDescription;
    addUndoPosition(std::move(description));
    m_storedDescription = std::move(store);
}

void OpenRocketDocument::stopUndo()
{
    std::optional<std::string> stored = std::exchange(m_storedDescription, std::nullopt);
    addUndoPosition(std::move(stored));
}

void OpenRocketDocument::clearUndo()
{
    // The new state is made before the old ones go (Java clears first, to the same end).
    std::shared_ptr<UndoState> state = currentState();
    m_undoHistory.clear();
    m_undoHistory.push_back(std::move(state));
    m_undoPosition = 0;

    fireUndoRedoChangeEvent();
}

void OpenRocketDocument::rocketChanged(const ComponentChangeEvent& event)
{
    const DocumentChangeEvent change{.kind   = DocumentChangeEvent::Kind::DOCUMENT,
                                     .source = event.getSource()};

    // Java: componentChanged(e)
    if (!event.isUndoChange())
    {
        removeRedoInfo();
        setLatestDescription();
    }

    fireUndoRedoChangeEvent();
    fireDocumentChangeEvent(change);

    // Java: stateChanged(e), which the rocket calls next on the same listener. The source is no
    // simulation.
    m_modId = ModId{};
    fireDocumentChangeEvent(change);
}

void OpenRocketDocument::simulationChanged(Simulation& simulation)
{
    m_modId = ModId{};
    if (!m_inUndoRedo)
    {
        // Shared with the history, which a slot may change while this function runs.
        const std::shared_ptr<UndoState> state           = m_undoHistory[m_undoPosition];
        const bool                       simulationsSame = simulationsEqual(state->simulations);
        if (simulationsSame && m_rocket->getModId() == state->rocket->getModId())
        {
            // Keep the "clean state" simulation snapshot in sync for non-undoable simulation
            // changes (e.g. simulated data/status updates).
            const std::optional<std::size_t> index = getSimulationIndex(simulation);
            if (index.has_value() && *index < state->simulations.size())
            {
                state->simulations[*index] = simulation.cloneForUndo();
            }
            else
            {
                state->simulations = copySimulationsForUndo();
            }
        }
        else if (!simulationsSame)
        {
            removeRedoInfo();
            setLatestDescription();
            fireUndoRedoChangeEvent();
        }
    }
    fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::DOCUMENT, .source = &simulation});
}

void OpenRocketDocument::preferencesChanged()
{
    // Java: stateChanged(e) with the document preferences as the source.
    m_modId = ModId{};
    fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::DOCUMENT, .source = &m_docPrefs});
}

// Sets the latest description
void OpenRocketDocument::setLatestDescription()
{
    m_undoHistory[m_undoPosition]->description = m_nextDescription;
}

// Removes any redo information if available
void OpenRocketDocument::removeRedoInfo()
{
    if (m_undoPosition + 1 < m_undoHistory.size())
    {
        m_undoHistory.erase(
            std::next(m_undoHistory.begin(), static_cast<std::ptrdiff_t>(m_undoPosition + 1)),
            m_undoHistory.end());
    }
}

bool OpenRocketDocument::isUndoAvailable() const
{
    if (m_undoPosition > 0)
    {
        return true;
    }

    return !isCleanState();
}

std::optional<std::string> OpenRocketDocument::getUndoDescription() const
{
    if (!isUndoAvailable())
    {
        return std::nullopt;
    }

    if (isCleanState())
    {
        // Undo is available in the clean state only behind the first state.
        return m_undoHistory[m_undoPosition - 1]->description;
    }
    return m_undoHistory[m_undoPosition]->description;
}

bool OpenRocketDocument::isRedoAvailable() const noexcept
{
    return m_undoPosition + 1 < m_undoHistory.size();
}

std::optional<std::string> OpenRocketDocument::getRedoDescription() const
{
    if (!isRedoAvailable())
    {
        return std::nullopt;
    }

    return m_undoHistory[m_undoPosition]->description;
}

void OpenRocketDocument::undo()
{
    if (!isUndoAvailable())
    {
        logUndoError("Undo not available");
        fireUndoRedoChangeEvent();
        return;
    }
    if (m_storedDescription.has_value())
    {
        logUndoError(std::format("undo() called with storedDescription={}", *m_storedDescription));
    }

    // Update history position

    if (isCleanState())
    {
        // We are in a clean state, simply move backwards in history
        m_undoPosition--;
    }
    else
    {
        if (m_undoPosition + 1 != m_undoHistory.size())
        {
            logUndoError("undo position inconsistency");
        }
        // Modifications have been made, save the state and restore previous state
        m_undoHistory.push_back(currentState());
    }

    loadStateAtPosition(true);
}

void OpenRocketDocument::redo()
{
    if (!isRedoAvailable())
    {
        logUndoError("Redo not available");
        fireUndoRedoChangeEvent();
        return;
    }
    if (m_storedDescription.has_value())
    {
        logUndoError(std::format("redo() called with storedDescription={}", *m_storedDescription));
    }

    m_undoPosition++;

    // Java loads a copy of the state's rocket; Rocket::loadFrom() leaves its source as it is.
    loadStateAtPosition(false);
}

OpenRocketDocument::UndoDetail OpenRocketDocument::getUndoDetail() const
{
    UndoDetail detail;
    detail.position = m_undoPosition;
    detail.descriptions.reserve(m_undoHistory.size());
    for (const std::shared_ptr<UndoState>& state : m_undoHistory)
    {
        detail.descriptions.push_back(state->description);
    }
    detail.nextDescription   = m_nextDescription;
    detail.storedDescription = m_storedDescription;
    detail.clean             = isCleanState();
    return detail;
}

void OpenRocketDocument::loadStateAtPosition(bool checkStructure)
{
    // Shared with the history, which a slot may change while the state is loaded.
    const std::shared_ptr<UndoState> state = m_undoHistory[m_undoPosition];
    try
    {
        const FlagScope inUndoRedo(m_inUndoRedo);
        if (checkStructure)
        {
            m_rocket->checkComponentStructure();
        }
        m_rocket->loadFrom(*state->rocket);
        if (checkStructure)
        {
            m_rocket->checkComponentStructure();
        }
        loadSimulationsFrom(state->simulations);
    }
    catch (...)
    {
        // Java's finally block, on the way out of an exception (a BugError).
        fireUndoRedoChangeEvent();
        throw;
    }
    fireUndoRedoChangeEvent();
}

bool OpenRocketDocument::isCleanState() const
{
    const UndoState& state = *m_undoHistory[m_undoPosition];
    return m_rocket->getModId() == state.rocket->getModId() && simulationsEqual(state.simulations);
}

bool OpenRocketDocument::simulationsEqual(
    const std::vector<std::unique_ptr<Simulation>>& snapshot) const
{
    return std::ranges::equal(
        m_simulations, snapshot,
        [](const std::shared_ptr<Simulation>& simulation, const std::unique_ptr<Simulation>& copy) {
            return *simulation == *copy;
        });
}

std::shared_ptr<OpenRocketDocument::UndoState> OpenRocketDocument::currentState() const
{
    std::shared_ptr<UndoState> state = std::make_shared<UndoState>();
    state->rocket                    = m_rocket->copyRocketWithOriginalId();
    state->simulations               = copySimulationsForUndo();
    return state;
}

std::vector<std::unique_ptr<Simulation>> OpenRocketDocument::copySimulationsForUndo() const
{
    std::vector<std::unique_ptr<Simulation>> copy;
    copy.reserve(m_simulations.size());
    for (const std::shared_ptr<Simulation>& simulation : m_simulations)
    {
        copy.push_back(simulation->cloneForUndo());
    }
    return copy;
}

void OpenRocketDocument::loadSimulationsFrom(
    const std::vector<std::unique_ptr<Simulation>>& simulationsSnapshot)
{
    if (m_simulations.size() == simulationsSnapshot.size())
    {
        // The size of the list is asked every time round, as in Java: a slot of the events of
        // loadFrom() may change the list.
        for (std::size_t i = 0; i < m_simulations.size() && i < simulationsSnapshot.size(); i++)
        {
            const std::shared_ptr<Simulation> simulation = m_simulations[i];
            simulation->loadFrom(*simulationsSnapshot[i]);
        }
    }
    else
    {
        // The simulations the list held stay with whoever else holds them.
        const std::vector<std::shared_ptr<Simulation>> previous = std::move(m_simulations);
        m_simulations.clear();
        m_simulationConnections.clear();
        for (const std::unique_ptr<Simulation>& snapshot : simulationsSnapshot)
        {
            std::shared_ptr<Simulation> simulation = snapshot->cloneForUndo();
            const FlightConfigurationId simId      = simulation->getId();
            if (!m_rocket->containsFlightConfigurationId(simId))
            {
                m_rocket->createFlightConfiguration(simId);
            }
            simulation->syncModId();
            listenTo(*simulation);
            m_simulations.push_back(std::move(simulation));
        }
    }
    fireDocumentChangeEvent(
        DocumentChangeEvent{.kind = DocumentChangeEvent::Kind::SIMULATION, .source = this});
}

// Log a non-fatal undo/redo error or inconsistency.
void OpenRocketDocument::logUndoError(std::string_view error) const
{
    // Java logs the error with the state of the undo system and tells the user the first one of
    // a run through the application's error handler, with this text.
    m_undoErrorOccurred.emit(std::format("Undo/Redo error: {}", error));
}

// ----------------------------------------------------------------------------- signals

void OpenRocketDocument::fireUndoRedoChangeEvent() const
{
    m_undoRedoChanged.emit();
}

void OpenRocketDocument::fireDocumentChangeEvent(const DocumentChangeEvent& event) const
{
    m_documentChanged.emit(event);
}

void OpenRocketDocument::fireDocumentSavingEvent(const DocumentChangeEvent& event) const
{
    m_documentSaving.emit(event);
}

// ------------------------------------------------------------------------------ others

std::string OpenRocketDocument::toSimulationDetail() const
{
    std::string str    = ">> Dumping simulation list:\n";
    std::size_t simNum = 0;
    for (const std::shared_ptr<Simulation>& s : m_simulations)
    {
        str += std::format("    [{}] {} ({}) \n", simNum, s->getName(), s->getId().toShortKey());
        simNum++;
    }

    return str;
}

void OpenRocketDocument::setPhotoSettings(std::map<std::string, std::string> photoSettings)
{
    m_photoSettings = std::move(photoSettings);
}

void OpenRocketDocument::reloadDocumentMaterials()
{
    const Rocket& rocket = *m_rocket;
    for (const RocketComponent& c : rocket.subtree())
    {
        for (const Material& m : c.getAllMaterials())
        {
            if (m.isUserDefined() && m.isDocumentMaterial())
            {
                m_documentMaterials.addMaterial(m);
            }
        }
    }
}

}  // namespace QtRocket
