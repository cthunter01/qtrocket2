#pragma once

#include <QString>

/// The application's names and version, as Qt is told them (QCoreApplication's application name,
/// organisation name and version, which QSettings and QCommandLineParser use) and as QtRocket
/// --version prints them. The version is the project's (project(VERSION) in the top-level
/// CMakeLists.txt, through the core's version()).
namespace QtRocket::Gui::BuildInfo
{

/// "QtRocket": the application's name and the window title of an empty main window.
[[nodiscard]] QString applicationName();

/// "QtRocket": the organisation whose settings the application keeps.
[[nodiscard]] QString organizationName();

/// The version, e.g. "0.2.0".
[[nodiscard]] QString applicationVersion();

/// The line QtRocket --version prints: "QtRocket 0.2.0", the core's creatorString(), which saved
/// files carry too.
[[nodiscard]] QString versionText();

}  // namespace QtRocket::Gui::BuildInfo
