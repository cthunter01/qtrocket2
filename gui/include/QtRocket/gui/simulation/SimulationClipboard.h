#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/document/SimulationCopy.h"

namespace QtRocket::Gui
{

/// The clipboard of simulations, as the simulations table's editing sees it (SimulationEditing):
/// what Copy and Cut put aside for Paste (OpenRocket's OpenRocketClipboard.
/// getClipboardSimulations() and setClipboard(Simulation[])). The application's clipboard is
/// main/OpenRocketClipboard, which holds the components too and empties either half when the other
/// is set, as Java's does; this interface lets the area simulation reach it without including the
/// area main, which comes after it (a lower area reaches a higher one only through signals or
/// injected interfaces). A test may implement it too.
///
/// What it holds is a document/SimulationCopy: detached copies of simulations, which refer to
/// neither the design they were copied from nor its rocket, so that they can be pasted into any
/// design, also after their own design was closed, and a pasted simulation belongs to the design
/// it is pasted into (OpenRocket bug 22: see SimulationCopy).
///
/// changed() tells of every change of what the clipboard holds (Java's
/// ClipboardListener.clipboardChanged()), its simulations or anything else it holds, after which
/// the actions decide again whether Paste is possible.
class SimulationClipboard : public QObject
{
    Q_OBJECT

public:
    ~SimulationClipboard() override;

    SimulationClipboard(const SimulationClipboard&)            = delete;
    SimulationClipboard& operator=(const SimulationClipboard&) = delete;
    SimulationClipboard(SimulationClipboard&&)                 = delete;
    SimulationClipboard& operator=(SimulationClipboard&&)      = delete;

    /// The simulations on the clipboard (getClipboardSimulations()): never null, empty when it
    /// holds none. Shared, so that a paste keeps them while it goes on whatever happens to the
    /// clipboard meanwhile (a slot of the paste's document events could set it again). They are
    /// never inserted themselves, only made into new simulations (SimulationCopy::insertInto()).
    [[nodiscard]] virtual std::shared_ptr<const SimulationCopy> simulations() const = 0;

    /// Holds @p simulations from now on, in place of what it held (setClipboard(Simulation[]));
    /// emits changed().
    virtual void setSimulations(SimulationCopy simulations) = 0;

Q_SIGNALS:
    /// What the clipboard holds changed (clipboardChanged()).
    void changed();

protected:
    explicit SimulationClipboard(QObject* parent);
};

}  // namespace QtRocket::Gui
