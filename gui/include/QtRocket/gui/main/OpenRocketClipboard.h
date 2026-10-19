#pragma once

#include <QObject>

#include "QtRocket/document/ComponentCopy.h"

namespace QtRocket::Gui
{

/// The components that Copy, Cut and Duplicate put aside for Paste (OpenRocket's
/// OpenRocketClipboard): one application-wide clipboard, instance(), shared by every window and
/// document, as Java's static one. It holds a ComponentCopy, detached copies with the decal
/// images they show, so that a paste into another document, after the source is closed too,
/// brings the images along; a paste inserts a further copy (ComponentCopy::copy()), so that
/// pasting twice gives two sets of components and the clipboard keeps its own.
///
/// changed() tells of every change (Java's ClipboardListener.clipboardChanged()), after which
/// the windows' actions decide again whether Paste is possible.
///
/// As in Java, the system clipboard is never involved: the components are not text, and a
/// paste into another program makes no sense. The simulations Java's clipboard also holds come
/// with Milestone 4.
///
/// The application's clipboard is instance(); a test makes its own (and empties instance() if it
/// used it: it is process-wide state).
class OpenRocketClipboard final : public QObject
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

    /// What the clipboard holds (getClipboardComponents()): never to be inserted itself, only a
    /// copy() of it.
    [[nodiscard]] const ComponentCopy& components() const noexcept { return m_components; }

    /// Whether the clipboard holds no component.
    [[nodiscard]] bool isEmpty() const noexcept { return m_components.empty(); }

    /// Holds @p components from now on, in place of what it held (setClipboard()); emits
    /// changed().
    void setComponents(ComponentCopy components);

    /// Holds nothing from now on; emits changed().
    void clear();

Q_SIGNALS:
    /// What the clipboard holds changed (clipboardChanged()).
    void changed();

private:
    ComponentCopy m_components;
};

}  // namespace QtRocket::Gui
