#pragma once

namespace QtRocket::Gui
{

/// The wait cursor over the whole application while a file is read or written on the GUI thread
/// (decision O15 of Milestone 2: designs open and save synchronously; from Milestone 4 the
/// simulation data's CSV export does too, plan 11.3), from the construction until the
/// destruction: it pushes Qt's wait cursor onto the application's override cursors
/// (QGuiApplication::setOverrideCursor()) and takes it off again (restoreOverrideCursor()), so
/// that nested ones end in the reverse order they began and the cursor before them comes back.
/// OpenRocket does that work on a worker thread and shows a progress dialog with Cancel when it
/// takes long (SwingWorkerDialog).
class WaitCursor final
{
public:
    WaitCursor();
    ~WaitCursor();
    WaitCursor(const WaitCursor&)            = delete;
    WaitCursor& operator=(const WaitCursor&) = delete;
    WaitCursor(WaitCursor&&)                 = delete;
    WaitCursor& operator=(WaitCursor&&)      = delete;
};

}  // namespace QtRocket::Gui
