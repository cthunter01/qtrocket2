#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"

namespace QtRocket::Gui
{

/// The components and the simulations that Copy, Cut and Duplicate put aside for Paste
/// (OpenRocket's OpenRocketClipboard): one application-wide clipboard, instance(), shared by every
/// window and document, as Java's static one. It holds either components or simulations, never
/// both: setting the one empties the other, as Java's setClipboard(List<RocketComponent>) sets its
/// simulations to null and setClipboard(Simulation[]) clears its components.
/// - The components are a ComponentCopy, detached copies with the decal images they show, so that
///   a paste into another document, after the source is closed too, brings the images along; a
///   paste inserts a further copy (ComponentCopy::copy()), so that pasting twice gives two sets of
///   components and the clipboard keeps its own.
/// - The simulations are a SimulationCopy (the clipboard is the SimulationClipboard of the
///   simulations table, the area simulation's view of it): detached copies, which refer to
///   neither their design nor its rocket, made into new simulations of the design they are pasted
///   into (OpenRocket bug 22: see SimulationCopy), any number of times.
///
/// changed() tells of every change (Java's ClipboardListener.clipboardChanged()), after which
/// the windows' actions decide again whether Paste is possible.
///
/// As in Java, the system clipboard is not this one: the components and the simulations are not
/// text, and a paste into another program makes no sense. (Copy of simulations puts the table's
/// text on the system clipboard besides, SimulationEditing.)
///
/// The application's clipboard is instance(); a test makes its own (and empties instance() if it
/// used it: it is process-wide state).
class OpenRocketClipboard final : public SimulationClipboard
{
    Q_OBJECT

public:
    /// An empty clipboard.
    explicit OpenRocketClipboard(QObject* parent = nullptr);
    ~OpenRocketClipboard() override;

    OpenRocketClipboard(const OpenRocketClipboard&)            = delete;
    OpenRocketClipboard& operator=(const OpenRocketClipboard&) = delete;
    OpenRocketClipboard(OpenRocketClipboard&&)                 = delete;
    OpenRocketClipboard& operator=(OpenRocketClipboard&&)      = delete;

    /// The application's clipboard, made at the first call and destroyed when the process ends.
    [[nodiscard]] static OpenRocketClipboard& instance();

    /// The components on the clipboard (getClipboardComponents()), empty when it holds none:
    /// never to be inserted itself, only a copy() of it.
    [[nodiscard]] const ComponentCopy& components() const noexcept { return m_components; }

    /// The simulations on the clipboard (getClipboardSimulations()); never null, empty when it
    /// holds none (see SimulationClipboard).
    [[nodiscard]] std::shared_ptr<const SimulationCopy> simulations() const override;

    /// Whether the clipboard holds nothing: neither components nor simulations.
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Holds @p components from now on, in place of what it held, the simulations too
    /// (setClipboard(List<RocketComponent>)); emits changed().
    void setComponents(ComponentCopy components);

    /// Holds @p simulations from now on, in place of what it held, the components too
    /// (setClipboard(Simulation[])); emits changed().
    void setSimulations(SimulationCopy simulations) override;

    /// Holds nothing from now on; emits changed().
    void clear();

private:
    ComponentCopy m_components;
    /// Never null.
    std::shared_ptr<const SimulationCopy> m_simulations;
};

}  // namespace QtRocket::Gui
