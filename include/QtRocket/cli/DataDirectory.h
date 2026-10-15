#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{

/// Where the command-line tool finds its data, and the rest of what it asks the operating system
/// (QtRocket's own). All the calls to the operating system of the tool are in DataDirectory.cpp,
/// behind one #ifdef split with a branch for Windows, macOS and Linux:
/// - the path of the running executable: GetModuleFileNameW on Windows, _NSGetExecutablePath on
///   macOS (with the links resolved), /proc/self/exe on Linux;
/// - the command-line arguments as UTF-8: on Windows main()'s argv is in the ANSI code page,
///   which garbles or drops what lies outside it, so the arguments are read from
///   GetCommandLineW() with CommandLineToArgvW() (shell32) and converted;
/// - the standard output and error in binary mode on Windows, so that a line ends with '\n' on
///   every platform (a text-mode stream writes "\r\n").
///
/// The data directory is the installed layout, <prefix>/share/QtRocket beside
/// <prefix>/bin/QtRocket_cli, so dataDirectory() is <directory of the executable>/../share/
/// QtRocket. The build puts a copy of the motor database at the same place relative to the
/// executable in the build tree (src/cli/CMakeLists.txt), and the archives of the dist-*
/// presets hold that layout. No path of the source tree is built into the program.
class DataDirectory
{
public:
    DataDirectory() = delete;

    /// The path of the running executable, or nullopt when the operating system does not say
    /// (and on a platform other than these three).
    [[nodiscard]] static std::optional<std::filesystem::path> executablePath();

    /// The data directory of the executable @p executable: <its directory>/../share/QtRocket.
    [[nodiscard]] static std::filesystem::path forExecutable(
        const std::filesystem::path& executable);

    /// The data directory of the running program: forExecutable(executablePath()), or, when the
    /// operating system does not say, of @p argv0 (main()'s argv[0], made absolute).
    [[nodiscard]] static std::filesystem::path dataDirectory(std::string_view argv0);

    /// The arguments of the program, its name left out, as UTF-8: from GetCommandLineW() on
    /// Windows (argv is used when that fails), main()'s @p argv elsewhere.
    [[nodiscard]] static std::vector<std::string> commandLineArguments(int argc, char** argv);

    /// Puts the standard output and error into binary mode on Windows; nothing elsewhere.
    static void useBinaryStandardStreams();
};

}  // namespace QtRocket
