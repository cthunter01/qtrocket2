#pragma once

#include <chrono>
#include <functional>

#include <QElapsedTimer>
#include <QTimer>

class QWidget;

namespace QtRocket::Gui::Test
{

/// Answers the next modal dialog the code under test shows (QDialog::exec(), a QMessageBox):
/// armed right before the call that shows it, it polls from the event loop of the dialog's exec()
/// for the active modal widget and hands it to the action, which answers it (clicks a button,
/// accepts it), once. A dialog that does not appear within kTimeout fails the test, and so does
/// an object destroyed before it acted. This is for the tests of the widget implementations of
/// UserPrompts and of the dialogs themselves: the code above them asks UserPrompts, answered by
/// ScriptedUserPrompts.
class NextModalDialog
{
public:
    /// What to do with the dialog.
    using Action = std::function<void(QWidget& dialog)>;

    /// How long the dialog may take to appear.
    static constexpr std::chrono::milliseconds kTimeout{10000};

    explicit NextModalDialog(Action action);
    ~NextModalDialog();
    NextModalDialog(const NextModalDialog&)            = delete;
    NextModalDialog& operator=(const NextModalDialog&) = delete;
    NextModalDialog(NextModalDialog&&)                 = delete;
    NextModalDialog& operator=(NextModalDialog&&)      = delete;

    /// Whether the action was done.
    [[nodiscard]] bool acted() const noexcept { return m_acted; }

private:
    /// One look for the dialog.
    void poll();

    Action        m_action;
    QTimer        m_timer;
    QElapsedTimer m_elapsed;
    bool          m_acted = false;
};

}  // namespace QtRocket::Gui::Test
