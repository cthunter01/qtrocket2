#pragma once

#include <vector>

#include <QMimeData>
#include <QObject>
#include <QString>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

/// What a drag of the component tree carries (OpenRocket's RocketComponentTransferable): the ids
/// of the dragged components and the document they are in. It works within this process only,
/// across its windows, as Java's DataFlavor of the class is local to its JVM: the format
/// mimeType() is set (its data are the ids, one per line, for whoever looks), but only an
/// object of this class, which a drop finds with fromMimeData(), says what to drop; the system
/// clipboard is never involved.
///
/// The ids are the dragged components without those that have a dragged ancestor (they go
/// with it), in tree order (pre-order), as ComponentTreeModel::mimeData() gives them. Java
/// orders its transferable by descending child position within each parent, which only its
/// per-component drop loop needs.
///
/// The source document is a reference that the drag does not own: it is valid while the source
/// window is open, which it is while a drag from it runs (QDrag::exec()). A drop compares it
/// with its own document (a move within the same rocket, else a copy) and reads the components
/// from it by id: a component that has left the source since is not there any more.
class ComponentMimeData final : public QMimeData
{
    Q_OBJECT

public:
    /// The drag of the components @p ids of @p source.
    ComponentMimeData(const OpenRocketDocument& source, std::vector<Uuid> ids);
    ~ComponentMimeData() override;

    ComponentMimeData(const ComponentMimeData&)            = delete;
    ComponentMimeData& operator=(const ComponentMimeData&) = delete;
    ComponentMimeData(ComponentMimeData&&)                 = delete;
    ComponentMimeData& operator=(ComponentMimeData&&)      = delete;

    /// The format of the tree's drags: "application/x-qtrocket-components".
    [[nodiscard]] static QString mimeType();

    /// @p data as a drag of the component tree, or null when it is something else (a drag of
    /// another program, or null).
    [[nodiscard]] static const ComponentMimeData* fromMimeData(const QMimeData* data);

    /// The document the components are dragged from.
    [[nodiscard]] const OpenRocketDocument& sourceDocument() const noexcept { return *m_source; }

    /// Whether the components are dragged from @p document.
    [[nodiscard]] bool isFrom(const OpenRocketDocument& document) const noexcept
    {
        return &document == m_source;
    }

    /// The ids of the dragged components, in tree order.
    [[nodiscard]] const std::vector<Uuid>& componentIds() const noexcept { return m_ids; }

private:
    const OpenRocketDocument* m_source;
    std::vector<Uuid>         m_ids;
};

}  // namespace QtRocket::Gui
