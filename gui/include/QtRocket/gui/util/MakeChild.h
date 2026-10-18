#pragma once

#include <concepts>
#include <utility>

#include <QObject>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

/// Makes a T that its Qt parent owns: QObject's object tree deletes the child with the parent (or
/// earlier, through deleteLater()). This is the GUI's only `new`: a parented QObject (a widget, a
/// layout, an action, ...) is always made here, and a parentless one (a window, a dialog) lives on
/// the stack or in a std::unique_ptr. The parent comes first, and the other arguments go to T's
/// constructor before it, in the order of Qt's own constructors: makeChild<QLabel>(this,
/// tr("Text")) is new QLabel(tr("Text"), this). A null parent would leak the child, so it is a
/// BugError.
template <std::derived_from<QObject> T, std::derived_from<QObject> Parent, class... Args>
    requires std::constructible_from<T, Args..., Parent*>
[[nodiscard]] T* makeChild(Parent* parent, Args&&... args)
{
    QTROCKET_ASSERT(parent != nullptr);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): the Qt parent owns and deletes the child
    return new T(std::forward<Args>(args)..., parent);
}

}  // namespace QtRocket::Gui
