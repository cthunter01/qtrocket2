#pragma once

#include <concepts>
#include <utility>

#include <QObject>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

/// Makes a T that its Qt parent owns: QObject's object tree deletes the child with the parent (or
/// earlier, through deleteLater()). This is the GUI's only `new` of a QObject (makeQtOwned()
/// below is the one for objects a Qt API takes over): a parented QObject (a widget, a layout, an
/// action, ...) is always made here, and a parentless one (a window, a dialog) lives on the stack
/// or in a std::unique_ptr. The parent comes first, and the other arguments go to T's
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

/// Makes a T that is not a QObject for a Qt API that takes it over at once and deletes it: an
/// icon engine for QIcon(QIconEngine*), or the copy QIconEngine::clone() returns, which its
/// caller's QIcon owns. The result goes straight into that call; kept anywhere else, it leaks.
/// It is the GUI's other `new`, so that this file holds every one of them (a QObject goes
/// through makeChild()).
template <class T, class... Args>
    requires(!std::derived_from<T, QObject>) && std::constructible_from<T, Args...>
[[nodiscard]] T* makeQtOwned(Args&&... args)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): the Qt API it is handed to deletes it
    return new T(std::forward<Args>(args)...);
}

}  // namespace QtRocket::Gui
