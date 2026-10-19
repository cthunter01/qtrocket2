#pragma once

#include <functional>

namespace QtRocket::Gui
{

class UserPrompts;

/// The error handling of the application's start. OpenRocket registers its exception handler
/// (SwingExceptionHandler) before it makes anything else (SwingStartup.runInEDT()), so a failure
/// while it loads the preferences, the units, the materials or the motors reaches its error
/// dialog. QtRocket's session installs the error hook that shows the internal error dialog
/// (ApplicationSession) only once the services are made (ApplicationContext): main() runs the
/// start through run(), so that what goes wrong before also reaches the dialog, and not only
/// the standard error, which a Windows program has none of.
///
/// Deviation: a start that throws ends the program whatever the user answers (OpenRocket's
/// dialog has no Quit, and its start leaves the application running without a window); an
/// error condition reported while the start runs (Guard::reportErrorCondition()) lets it go on,
/// whatever the answer, as there is no event loop to end yet.
class StartupGuard
{
public:
    /// The exit code of a start that failed.
    static constexpr int kStartupFailureExitCode = 1;

    StartupGuard() = delete;

    /// Calls @p start (main()'s start of the application and its event loop) and returns what
    /// it returns. While it runs, the error hook is @p prompts' internal error dialog
    /// (UserPrompts::showInternalError(), with no parent window) wherever no hook of its own
    /// replaces it (the session's), and the hook before is back when run() returns. What
    /// @p start throws is shown there with the context "Start-up", and run() returns
    /// kStartupFailureExitCode.
    static int run(UserPrompts& prompts, const std::function<int()>& start);
};

}  // namespace QtRocket::Gui
