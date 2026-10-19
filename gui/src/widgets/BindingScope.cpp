#include "QtRocket/gui/widgets/BindingScope.h"

#include <memory>
#include <utility>

#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Commits the pending edit of every @p Field under @p root with @p commit (each guarded, so that
/// the next one commits too): whether a commit took a value.
template <class Field, class Commit>
[[nodiscard]] bool commitEach(const QWidget& root, const Commit& commit)
{
    bool taken = false;
    // QPointers: a commit's listeners may delete widgets with it (they should use deleteLater()).
    QList<QPointer<Field>> fields;
    for (Field* field : root.findChildren<Field*>())
    {
        fields.append(QPointer<Field>(field));
    }
    for (const QPointer<Field>& field : fields)
    {
        if (field.isNull())
        {
            continue;
        }
        Field&        target  = *field;
        const QString context = target.objectName();
        Guard::run([&taken, &target, &commit] { taken = commit(target) || taken; }, context);
    }
    return taken;
}

}  // namespace

BindingScope::BindingScope(QWidget& root, std::shared_ptr<ComponentSelection> selection)
  : m_root(&root), m_selection(std::move(selection))
{
}

BindingScope::~BindingScope()
{
    release();
}

void BindingScope::checkNotReleased() const
{
    if (m_released)
    {
        bug("The binding scope is used after release()");
    }
}

bool BindingScope::isReleased() const noexcept
{
    return m_released || (m_selection != nullptr && !m_selection->isValid());
}

bool BindingScope::commitAll()
{
    if (isReleased() || m_root.isNull())
    {
        return false;
    }
    const QPointer<QWidget> root = m_root;
    bool                    taken =
        commitEach<UnitSpinBox>(*root, [](UnitSpinBox& spin) { return spin.commitEdit(); });
    if (!root.isNull())
    {
        taken = commitEach<IntSpinBox>(*root, [](IntSpinBox& spin) { return spin.commitEdit(); }) ||
                taken;
    }
    if (!root.isNull())
    {
        taken = commitEach<TextCommitter>(
                    *root, [](TextCommitter& committer) { return committer.commit(); }) ||
                taken;
    }
    return taken;
}

void BindingScope::release() noexcept
{
    if (m_released)
    {
        return;
    }
    m_released = true;
    // The dialog's own listeners first: nothing the release does reaches them.
    m_connections.clear();
    if (m_selection != nullptr)
    {
        m_selection->release();
    }
    m_bindings.clear();
}

}  // namespace QtRocket::Gui
