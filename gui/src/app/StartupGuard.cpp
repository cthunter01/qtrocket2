#include "QtRocket/gui/app/StartupGuard.h"

#include <functional>
#include <utility>

#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

/// Installs a hook for as long as it lives, then puts the one before back.
class HookScope
{
public:
    explicit HookScope(Guard::Hook hook) : m_previous(Guard::setHook(std::move(hook))) { }
    ~HookScope() { static_cast<void>(Guard::setHook(std::move(m_previous))); }
    HookScope(const HookScope&)            = delete;
    HookScope& operator=(const HookScope&) = delete;
    HookScope(HookScope&&)                 = delete;
    HookScope& operator=(HookScope&&)      = delete;

private:
    Guard::Hook m_previous;
};

}  // namespace

int StartupGuard::run(UserPrompts& prompts, const std::function<int()>& start)
{
    const HookScope hook([&prompts](const Guard::Failure& failure) {
        static_cast<void>(prompts.showInternalError(nullptr, failure));
    });
    int             exitCode = kStartupFailureExitCode;
    static_cast<void>(Guard::run([&] { exitCode = start(); }, QStringLiteral("Start-up")));
    return exitCode;
}

}  // namespace QtRocket::Gui
