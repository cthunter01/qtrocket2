#pragma once

#include <concepts>
#include <memory>
#include <utility>

#include <QDialog>
#include <QPointer>
#include <QWidget>

#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

/// A modal dialog made for one question or message and destroyed after it (JOptionPane's
/// dialogs, a file chooser), which survives the destruction of its parent while it is open: a
/// window or a dialog can be deleted during the question's event loop (another window's event,
/// a timer), and it takes its child dialogs with it. A dialog on the stack would then be deleted
/// twice; this one is made on the heap, as the child of its parent (or, without a parent, owned
/// here), and held by a QPointer, so that whoever asked learns that it is gone (get() is null,
/// exec() gives QDialog::Rejected) and answers as if it had been closed.
///
/// A dialog with a parent is deleted later (deleteLater()) when this object ends, one without a
/// parent at once.
template <std::derived_from<QDialog> Dialog>
class ModalDialog final
{
public:
    /// A new Dialog(@p args..., @p parent); @p parent may be null.
    template <class... Args>
    explicit ModalDialog(QWidget* parent, Args&&... args)
    {
        if (parent != nullptr)
        {
            m_dialog = makeChild<Dialog>(parent, std::forward<Args>(args)...);
        }
        else
        {
            m_owned  = std::make_unique<Dialog>(std::forward<Args>(args)..., nullptr);
            m_dialog = m_owned.get();
        }
    }

    ~ModalDialog()
    {
        if (m_owned == nullptr)
        {
            if (Dialog* const dialog = m_dialog.data(); dialog != nullptr)
            {
                dialog->deleteLater();
            }
        }
    }

    ModalDialog(const ModalDialog&)            = delete;
    ModalDialog& operator=(const ModalDialog&) = delete;
    ModalDialog(ModalDialog&&)                 = delete;
    ModalDialog& operator=(ModalDialog&&)      = delete;

    /// The dialog, or null once it was destroyed (with its parent).
    [[nodiscard]] Dialog* get() const noexcept { return m_dialog.data(); }

    /// Shows the dialog modal and returns when it is closed (QDialog::exec()): its result, or
    /// QDialog::Rejected when it was destroyed before or while it was open.
    int exec()
    {
        Dialog* const dialog = get();
        return dialog == nullptr ? static_cast<int>(QDialog::Rejected) : dialog->exec();
    }

private:
    std::unique_ptr<Dialog> m_owned;
    QPointer<Dialog>        m_dialog;
};

}  // namespace QtRocket::Gui
